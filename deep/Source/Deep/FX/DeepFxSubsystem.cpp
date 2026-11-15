#include "FX/DeepFxSubsystem.h"

#include "Audio/DeepAudioRules.h"
#include "Audio/DeepAudioSettings.h"
#include "Audio/DeepAudioSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Debug/DeepStateReport.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/World.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "EngineUtils.h"
#include "FX/DeepFxRules.h"
#include "FX/DeepFxSettings.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "World/DeepAbyssFloor.h"
#include "World/DeepSubmarine.h"

namespace
{
	const FName CountParam(TEXT("Count"));
	const FName SizeParam(TEXT("SizeScale"));
}

UDeepFxSubsystem* UDeepFxSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDeepFxSubsystem>() : nullptr;
}

bool UDeepFxSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UDeepFxSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDeepFxSubsystem, STATGROUP_Tickables);
}

bool UDeepFxSubsystem::IsTickable() const
{
	return !IsTemplate() && GetWorld() != nullptr;
}

bool UDeepFxSubsystem::IsEnabled() const
{
	return (RuntimeOverride >= 0 ? RuntimeOverride >= 1 : UDeepFxSettings::Get().bEnabled) && !IsRunningDedicatedServer();
}

UNiagaraSystem* UDeepFxSubsystem::LoadSystem(bool bSilt)
{
	TObjectPtr<UNiagaraSystem>& Slot = bSilt ? SiltSystem : BubbleSystem;
	if (!Slot && !bTriedLoad[bSilt ? 1 : 0])
	{
		bTriedLoad[bSilt ? 1 : 0] = true;
		const UDeepFxSettings& S = UDeepFxSettings::Get();
		Slot = (bSilt ? S.SiltSystem : S.BubbleSystem).LoadSynchronous();
		if (!Slot)
		{
			UE_LOG(LogTemp, Warning, TEXT("DEEPFX could not load %s"), *(bSilt ? S.SiltSystem : S.BubbleSystem).ToString());
		}
	}
	return Slot;
}

static bool SpawnBurst(UWorld* World, UNiagaraSystem* System, const FVector& Location, int32 Count, float SizeScale)
{
	if (!World || !System || Count <= 0)
	{
		return false;
	}
	UNiagaraComponent* Comp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, Location, FRotator::ZeroRotator, FVector(1.f),
		/*bAutoDestroy*/ true, /*bAutoActivate*/ false, ENCPoolMethod::AutoRelease, /*bPreCullCheck*/ true);
	if (!Comp)
	{
		return false;
	}
	Comp->SetVariableInt(CountParam, Count);
	Comp->SetVariableFloat(SizeParam, SizeScale);
	Comp->Activate(true);
	return true;
}

bool UDeepFxSubsystem::SpawnBubbles(const FVector& Location, int32 Count, float SizeScale)
{
	if (!IsEnabled())
	{
		return false;
	}
	++BubbleBursts;
	return SpawnBurst(GetWorld(), LoadSystem(false), Location, Count, SizeScale);
}

bool UDeepFxSubsystem::SpawnSilt(const FVector& Location, int32 Count, float SizeScale)
{
	if (!IsEnabled())
	{
		return false;
	}
	++SiltPuffs;
	return SpawnBurst(GetWorld(), LoadSystem(true), Location, Count, SizeScale);
}

void UDeepFxSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World || DeltaTime <= 0.f)
	{
		return;
	}
	UpdateSpill();
	for (TActorIterator<ADeepDiver> It(World); It; ++It)
	{
		StepDiver(**It, Divers.FindOrAdd(*It), DeltaTime);
	}
	for (auto It = Divers.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void UDeepFxSubsystem::UpdateSpill()
{
	if (!LightCollection && !bTriedCollection)
	{
		bTriedCollection = true;
		LightCollection = UDeepFxSettings::Get().LightCollection.LoadSynchronous();
	}
	UWorld* World = GetWorld();
	const ADeepDiver* Diver = DeepStateReport::FindDiver(World);
	float Spill = 0.f;
	if (Diver)
	{
		const ADeepSubmarine* Sub = TActorIterator<ADeepSubmarine>(World) ? *TActorIterator<ADeepSubmarine>(World) : nullptr;
		const float HatchM = Sub ? FVector::Dist(Sub->GetHatchLocation(), Diver->GetActorLocation()) / 100.f : 1000.f;
		Spill = DeepFxRules::Spill(Diver->IsLampOn(), HatchM, Diver->GetDepthM(), UDeepFxSettings::Get());
	}
	LastSpill = Spill;
	if (LightCollection)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(World, LightCollection, TEXT("Spill"), Spill);
	}
}

bool UDeepFxSubsystem::FindBedBelow(const ADeepDiver& Diver, const FVector& From, FVector& OutBed) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DeepFxBed), false, &Diver);
	const FVector To = From - FVector(0.f, 0.f, UDeepFxSettings::Get().BedProbeCm);
	if (GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params) && Hit.GetActor() && Hit.GetActor()->IsA<ADeepAbyssFloor>())
	{
		OutBed = Hit.ImpactPoint;
		return true;
	}
	return false;
}

void UDeepFxSubsystem::StepDiver(ADeepDiver& Diver, FDiverFx& Fx, float DeltaTime)
{
	const UDeepFxSettings& S = UDeepFxSettings::Get();
	const UDeepRunComponent* Run = Diver.GetRun();
	const bool bSwimming = Run && Run->CanSwim();
	const int32 Cargo = Diver.GetCargo().Num();
	Fx.SinceSiltS += DeltaTime;

	if (!bSwimming)
	{
		Fx.bWasSwimming = false;
		Fx.bWasWinching = false;
		Fx.EntryLeftS = 0.f;
		Fx.CargoCount = Cargo;
		return;
	}

	// Into the water: a stream of the air carried down through the hatch.
	if (!Fx.bWasSwimming)
	{
		Fx.bWasSwimming = true;
		Fx.OwnClockS = 0.f;
		Fx.BreathPhase = 0.f;
		Fx.EntryLeftS = Run->GetState() == EDeepRunState::Tethered ? S.EntryStreamS : 0.f;
		Fx.EntryNextS = 0.f;
	}
	if (Fx.EntryLeftS > 0.f && Wants(S.bEntryStream))
	{
		Fx.EntryLeftS -= DeltaTime;
		Fx.EntryNextS -= DeltaTime;
		if (Fx.EntryNextS <= 0.f)
		{
			Fx.EntryNextS = S.EntryIntervalS;
			const FVector Body = Diver.GetActorLocation() + FMath::VRand() * FVector(30.f, 30.f, 60.f);
			SpawnBubbles(Body, S.EntryCount, S.EntrySizeScale);
		}
	}

	if (Wants(S.bBreath))
	{
		StepBreath(Diver, Fx, DeltaTime);
	}

	// The winch: a churn under the hatch, round the line, while it hauls.
	if (Diver.IsWinching() && Wants(S.bWinchChurn))
	{
		Fx.ChurnNextS -= DeltaTime;
		if (!Fx.bWasWinching || Fx.ChurnNextS <= 0.f)
		{
			Fx.ChurnNextS = S.ChurnIntervalS;
			if (const ADeepSubmarine* Sub = TActorIterator<ADeepSubmarine>(GetWorld()) ? *TActorIterator<ADeepSubmarine>(GetWorld()) : nullptr)
			{
				const FVector2D Ring = FVector2D(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)).GetSafeNormal() * FMath::FRandRange(0.3f, 1.f) * S.ChurnRadiusCm;
				const FVector At = Sub->GetHatchLocation() + FVector(Ring.X, Ring.Y, -S.ChurnBelowHatchCm * FMath::FRandRange(0.7f, 1.f));
				SpawnBubbles(At, S.ChurnCount, S.ChurnSizeScale);
			}
		}
	}
	Fx.bWasWinching = Diver.IsWinching();

	// Salvage off the bed: a puff where it lay. The diver grabs what it looks at, so follow the view to the bed;
	// failing that, the bed under the diver's reach.
	if (Cargo > Fx.CargoCount && Wants(S.bSilt))
	{
		FVector Bed;
		const UCameraComponent* Camera = Diver.FindComponentByClass<UCameraComponent>();
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DeepFxGrab), false, &Diver);
		const FVector Eye = Camera ? Camera->GetComponentLocation() : Diver.GetActorLocation();
		const FVector Look = Camera ? Camera->GetForwardVector() : Diver.GetActorForwardVector();
		if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + Look * 600.f, ECC_Visibility, Params) && Hit.GetActor() && Hit.GetActor()->IsA<ADeepAbyssFloor>())
		{
			Bed = Hit.ImpactPoint;
		}
		else if (!FindBedBelow(Diver, Diver.GetActorLocation() + Diver.GetActorForwardVector() * 60.f, Bed))
		{
			Bed = Diver.GetActorLocation() + Look * 120.f;
		}
		SpawnSilt(Bed + FVector(0.f, 0.f, 8.f), S.GrabSiltCount);
		Fx.SinceSiltS = 0.f;
	}
	Fx.CargoCount = Cargo;

	// Touching the bed: a smaller puff, not more than once per cooldown.
	if (Fx.SinceSiltS >= S.TouchCooldownS && Wants(S.bSilt))
	{
		const UCapsuleComponent* Capsule = Diver.GetCapsuleComponent();
		const float Half = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f;
		const FVector Feet = Diver.GetActorLocation() - FVector(0.f, 0.f, Half);
		FVector Bed;
		if (FindBedBelow(Diver, Feet + FVector(0.f, 0.f, 50.f), Bed)
			&& DeepFxRules::KicksSilt(Feet.Z - Bed.Z, Diver.GetVelocity().Size(), Fx.SinceSiltS, S))
		{
			SpawnSilt(Bed + FVector(0.f, 0.f, 8.f), S.TouchSiltCount, 0.8f);
			Fx.SinceSiltS = 0.f;
		}
	}
}

void UDeepFxSubsystem::StepBreath(ADeepDiver& Diver, FDiverFx& Fx, float DeltaTime)
{
	const UDeepFxSettings& S = UDeepFxSettings::Get();
	// One source of truth with the sound: the breath loop the player hears, and how long it has played.
	float PlayS = -1.f;
	bool bFast = false;
	const UDeepAudioSubsystem* Audio = UDeepAudioSubsystem::Get(this);
	if (Audio && DeepStateReport::FindDiver(GetWorld()) == &Diver)
	{
		const float Fast = Audio->GetLoopPlaySeconds(FName(DeepAudioLoop::BreathFast));
		const float Calm = Audio->GetLoopPlaySeconds(FName(DeepAudioLoop::Breath));
		bFast = Fast >= 0.f && Audio->GetLoopVolume(FName(DeepAudioLoop::BreathFast)) >= Audio->GetLoopVolume(FName(DeepAudioLoop::Breath));
		PlayS = bFast ? Fast : Calm;
	}
	if (PlayS < 0.f)
	{
		// No sound for this diver (another player, or audio off): the same rule on its own clock.
		const UDeepRunComponent* Run = Diver.GetRun();
		bFast = Run && Run->GetState() == EDeepRunState::Cut && Run->GetBailoutAirSeconds() <= UDeepAudioSettings::Get().FastBreathAirS;
		Fx.OwnClockS += DeltaTime;
		PlayS = Fx.OwnClockS;
	}
	const float Phase = DeepFxRules::BreathPhase(PlayS, DeepFxRules::BreathPeriodS(bFast, S));
	const TArray<int32> Puffs = DeepFxRules::PuffIndicesCrossed(Fx.BreathPhase, Phase, S.ExhalePuffPhases);
	Fx.BreathPhase = Phase;
	if (Puffs.Num() == 0)
	{
		return;
	}
	const UCameraComponent* Camera = Diver.FindComponentByClass<UCameraComponent>();
	const FTransform View = Camera ? Camera->GetComponentTransform() : Diver.GetActorTransform();
	for (const int32 Index : Puffs)
	{
		// The exhaust ports either side of the regulator: each breath out leaves 5 cm left or right of the last one.
		if (Index == 0)
		{
			Fx.PuffSide = -Fx.PuffSide;
		}
		const FVector Mouth = View.TransformPosition(FVector(S.MouthOffsetCm.X, S.MouthOffsetCm.Y + 5.f * Fx.PuffSide, S.MouthOffsetCm.Z));
		SpawnBubbles(Mouth, DeepFxRules::PuffCount(Index, S), (bFast ? S.FastSizeScale : 1.f) * S.BreathSizeScale);
		++BreathBursts;
	}
}
