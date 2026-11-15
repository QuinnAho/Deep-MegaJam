#include "Harpoon/DeepHarpoonComponent.h"

#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Diver/DeepTetherComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Harpoon/DeepHarpoonBolt.h"
#include "Harpoon/DeepHarpoonSettings.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Monster/DeepMonsterBrain.h"
#include "Monster/DeepMonsterRules.h"
#include "Monster/DeepSerpent.h"
#include "Net/UnrealNetwork.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepArena.h"
#include "World/DeepSubmarine.h"

namespace
{
	constexpr float HarpoonComponentCmPerM = 100.f;
	/** The serpent's body spheres are basic-shape spheres: 100 cm across at scale 1. */
	constexpr float SphereRadiusCmAtScale1 = 50.f;
}

UDeepHarpoonComponent::UDeepHarpoonComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(true);
	ShotsLeft = UDeepHarpoonSettings::Get().ShotsPerDive;
}

ADeepDiver* UDeepHarpoonComponent::GetDiver() const
{
	return Cast<ADeepDiver>(GetOwner());
}

ADeepGameState* UDeepHarpoonComponent::GetGameState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<ADeepGameState>() : nullptr;
}

double UDeepHarpoonComponent::ServerNow() const
{
	if (const ADeepGameState* State = GetGameState())
	{
		return State->GetServerWorldTimeSeconds();
	}
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

float UDeepHarpoonComponent::GetSecondsSinceResult() const
{
	return static_cast<float>(ServerNow() - LastResultTime);
}

FVector UDeepHarpoonComponent::GunLocation(const ADeepDiver* Diver)
{
	if (!Diver)
	{
		return FVector::ZeroVector;
	}
	const FRotationMatrix View(Diver->GetBaseAimRotation());
	const FVector Eye = Diver->GetActorLocation() + FVector(0.f, 0.f, 55.f);
	return Eye + View.GetScaledAxis(EAxis::X) * 70.f + View.GetScaledAxis(EAxis::Y) * 22.f - View.GetScaledAxis(EAxis::Z) * 28.f;
}

void UDeepHarpoonComponent::BeginPlay()
{
	Super::BeginPlay();
	ADeepDiver* Diver = GetDiver();
	if (!Diver)
	{
		return;
	}
	if (Diver->HasAuthority())
	{
		ShotsLeft = FMath::Max(0, UDeepHarpoonSettings::Get().ShotsPerDive);
		if (UDeepEventBus* Bus = UDeepEventBus::Get(this))
		{
			BusHandle = Bus->OnEvent.AddUObject(this, &UDeepHarpoonComponent::HandleBusEvent);
		}
	}
	// After the character has moved for this frame, so the reel has the last word before the tether clamps.
	if (UCharacterMovementComponent* Movement = Diver->GetCharacterMovement())
	{
		PrimaryComponentTick.AddPrerequisite(Movement, Movement->PrimaryComponentTick);
	}
	BindInput();
}

void UDeepHarpoonComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UDeepEventBus* Bus = UDeepEventBus::Get(this))
	{
		Bus->OnEvent.Remove(BusHandle);
	}
	if (FireMapping)
	{
		const ADeepDiver* Diver = GetDiver();
		const APlayerController* Player = Diver ? Cast<APlayerController>(Diver->GetController()) : nullptr;
		const ULocalPlayer* Local = Player ? Player->GetLocalPlayer() : nullptr;
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = Local ? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
		{
			Subsystem->RemoveMappingContext(FireMapping);
		}
	}
	DestroyBolt();
	Super::EndPlay(EndPlayReason);
}

void UDeepHarpoonComponent::BindInput()
{
	const ADeepDiver* Diver = GetDiver();
	APlayerController* Player = Diver ? Cast<APlayerController>(Diver->GetController()) : nullptr;
	if (!Player || !Player->IsLocalController())
	{
		return;
	}
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(Player->InputComponent);
	if (!Input || BoundInputComponent.Get() == Input)
	{
		return;
	}
	if (!FireAction)
	{
		FireAction = NewObject<UInputAction>(this, TEXT("IA_Harpoon"));
		FireAction->ValueType = EInputActionValueType::Boolean;
		// The settlement screen's continue also listens to the left button: let it through.
		FireAction->bConsumeInput = false;
		FireMapping = NewObject<UInputMappingContext>(this, TEXT("IMC_Harpoon"));
		FireMapping->MapKey(FireAction, EKeys::LeftMouseButton);
	}
	Input->BindAction(FireAction, ETriggerEvent::Started, this, &UDeepHarpoonComponent::PressFire);
	Input->BindAction(FireAction, ETriggerEvent::Completed, this, &UDeepHarpoonComponent::ReleaseFire);
	const ULocalPlayer* Local = Player->GetLocalPlayer();
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = Local ? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
	{
		if (!Subsystem->HasMappingContext(FireMapping))
		{
			Subsystem->AddMappingContext(FireMapping, 1);
		}
	}
	BoundInputComponent = Input;
}

void UDeepHarpoonComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (const ADeepDiver* Diver = GetDiver(); Diver && Diver->IsLocallyControlled())
	{
		BindInput();
	}
	StepHarpoon(DeltaTime);
}

// ---------------------------------------------------------------- input

bool UDeepHarpoonComponent::GetView(FVector& OutOriginCm, FVector& OutDirection) const
{
	const ADeepDiver* Diver = GetDiver();
	if (!Diver)
	{
		return false;
	}
	FRotator Rotation;
	if (const APlayerController* Player = Cast<APlayerController>(Diver->GetController()))
	{
		Player->GetPlayerViewPoint(OutOriginCm, Rotation);
	}
	else
	{
		Diver->GetActorEyesViewPoint(OutOriginCm, Rotation);
	}
	OutDirection = Rotation.Vector();
	return true;
}

void UDeepHarpoonComponent::PressFire()
{
	ADeepDiver* Diver = GetDiver();
	if (!Diver)
	{
		return;
	}
	if (Phase != EDeepHarpoonPhase::Idle)
	{
		// Pressing again lets go.
		if (Diver->HasAuthority())
		{
			Release();
		}
		else
		{
			ServerRelease(false);
		}
		return;
	}
	FVector Origin, Direction;
	if (!GetView(Origin, Direction))
	{
		return;
	}
	if (Diver->HasAuthority())
	{
		FireAlong(Origin, Direction);
	}
	else
	{
		ServerFire(Origin, Direction);
	}
}

void UDeepHarpoonComponent::ReleaseFire()
{
	const ADeepDiver* Diver = GetDiver();
	if (!Diver || Phase != EDeepHarpoonPhase::Latched || LatchTarget != EDeepHarpoonTarget::Terrain)
	{
		return;
	}
	if (Diver->HasAuthority())
	{
		Release();
	}
	else
	{
		ServerRelease(true);
	}
}

void UDeepHarpoonComponent::ServerFire_Implementation(FVector_NetQuantize OriginCm, FVector_NetQuantizeNormal Direction)
{
	const ADeepDiver* Diver = GetDiver();
	// The shot starts at the diver's eyes; a far-off origin is a bad packet, so use the eyes instead.
	FVector Origin = OriginCm;
	if (Diver && FVector::Dist(Origin, Diver->GetActorLocation()) > 300.f)
	{
		FRotator Ignored;
		Diver->GetActorEyesViewPoint(Origin, Ignored);
	}
	FireAlong(Origin, Direction);
}

void UDeepHarpoonComponent::ServerRelease_Implementation(bool bOnlyTerrain)
{
	if (!bOnlyTerrain || (Phase == EDeepHarpoonPhase::Latched && LatchTarget == EDeepHarpoonTarget::Terrain))
	{
		Release();
	}
}

// ---------------------------------------------------------------- server

void UDeepHarpoonComponent::FireAlong(const FVector& OriginCm, const FVector& Direction)
{
	ADeepDiver* Diver = GetDiver();
	UWorld* World = GetWorld();
	if (!Diver || !Diver->HasAuthority() || !World)
	{
		return;
	}
	const UDeepRunComponent* Run = Diver->GetRun();
	const bool bCanSwim = Run && Run->CanSwim();
	if (!bCanSwim)
	{
		return;
	}
	if (Phase != EDeepHarpoonPhase::Idle)
	{
		Release();
		return;
	}
	if (!UDeepHarpoonRules::CanFire(ShotsLeft, bCanSwim, Phase))
	{
		SetResult(EDeepHarpoonResult::Empty);
		Fire(DeepHarpoonEvent::Empty);
		return;
	}

	const UDeepHarpoonSettings& Settings = UDeepHarpoonSettings::Get();
	ShotsLeft -= 1;
	Phase = EDeepHarpoonPhase::Flying;
	LatchTarget = EDeepHarpoonTarget::None;
	BoltDir = Direction.GetSafeNormal(UE_SMALL_NUMBER, Diver->GetActorForwardVector());
	BoltPosCm = OriginCm;
	TravelledCm = 0.f;
	LatchSeconds = 0.f;

	// Is there something to hit beyond the range? Then a miss reads "out of range", not just "missed".
	const float RangeCm = Settings.RangeM * HarpoonComponentCmPerM;
	const FDeepHarpoonTrace Far = TraceAim(OriginCm, BoltDir, RangeCm * Settings.AimLookAheadFactor);
	BeyondRangeCm = (Far.Target != EDeepHarpoonTarget::None && Far.DistanceCm > RangeCm) ? Far.DistanceCm : 0.f;

	DestroyBolt();
	FActorSpawnParameters Params;
	Params.Owner = Diver;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Bolt = World->SpawnActor<ADeepHarpoonBolt>(OriginCm, BoltDir.Rotation(), Params);
	if (Bolt)
	{
		Bolt->PlaceBolt(OriginCm, BoltDir);
	}
	SetResult(EDeepHarpoonResult::Fired);
	Fire(DeepHarpoonEvent::Fired);
}

void UDeepHarpoonComponent::Release()
{
	if (Phase == EDeepHarpoonPhase::Idle)
	{
		return;
	}
	Phase = EDeepHarpoonPhase::Idle;
	LatchTarget = EDeepHarpoonTarget::None;
	LatchSeconds = 0.f;
	DestroyBolt();
	SetResult(EDeepHarpoonResult::Released);
}

void UDeepHarpoonComponent::Reload()
{
	if (Phase != EDeepHarpoonPhase::Idle)
	{
		Phase = EDeepHarpoonPhase::Idle;
		LatchTarget = EDeepHarpoonTarget::None;
		DestroyBolt();
	}
	ShotsLeft = FMath::Max(0, UDeepHarpoonSettings::Get().ShotsPerDive);
}

void UDeepHarpoonComponent::HandleBusEvent(FName Event, const FDeepEventPayload& Payload)
{
	if (Payload.Source != GetOwner())
	{
		return;
	}
	// Every way a dive starts (first, same stop, new stop, respawn, reset). Not a re-tether: that is the same dive.
	if (Event == FName(DeepEvent::DiveStart) || Event == FName(DeepEvent::Respawn)
		|| Event == FName(DeepEvent::DescendStop2) || Event == FName(DeepEvent::DescendStop3))
	{
		Reload();
	}
}

void UDeepHarpoonComponent::StepHarpoon(float DeltaSeconds)
{
	ADeepDiver* Diver = GetDiver();
	if (!Diver)
	{
		return;
	}
	const float Dt = FMath::Max(0.f, DeltaSeconds);
	if (Diver->HasAuthority())
	{
		const UDeepRunComponent* Run = Diver->GetRun();
		if (Phase != EDeepHarpoonPhase::Idle && (!Run || !Run->CanSwim()))
		{
			// Dead, banked or descending: the line goes slack.
			Phase = EDeepHarpoonPhase::Idle;
			LatchTarget = EDeepHarpoonTarget::None;
			DestroyBolt();
			return;
		}
		if (Phase == EDeepHarpoonPhase::Flying)
		{
			StepFlight(Dt);
		}
	}
	if (Phase == EDeepHarpoonPhase::Latched)
	{
		StepReel(Dt);
	}
}

void UDeepHarpoonComponent::StepFlight(float DeltaSeconds)
{
	const UDeepHarpoonSettings& Settings = UDeepHarpoonSettings::Get();
	const float Step = UDeepHarpoonRules::BoltStepCm(TravelledCm, Settings.RangeM, Settings.BoltSpeedMps, DeltaSeconds);
	if (Step > 0.f)
	{
		const FDeepHarpoonTrace Hit = TraceAim(BoltPosCm, BoltDir, Step);
		if (Hit.Target != EDeepHarpoonTarget::None)
		{
			BoltPosCm = Hit.PointCm;
			TravelledCm += Hit.DistanceCm;
			if (Bolt)
			{
				Bolt->PlaceBolt(BoltPosCm, BoltDir);
			}
			HandleHit(Hit);
			return;
		}
		BoltPosCm += BoltDir * Step;
		TravelledCm += Step;
		if (Bolt)
		{
			Bolt->PlaceBolt(BoltPosCm, BoltDir);
		}
	}
	if (TravelledCm >= Settings.RangeM * HarpoonComponentCmPerM - 0.5f)
	{
		EndFlight(BeyondRangeCm > 0.f ? EDeepHarpoonResult::OutOfRange : EDeepHarpoonResult::Missed);
	}
}

void UDeepHarpoonComponent::HandleHit(const FDeepHarpoonTrace& Hit)
{
	ADeepDiver* Diver = GetDiver();
	const UDeepHarpoonSettings& Settings = UDeepHarpoonSettings::Get();
	LatchSeconds = 0.f;
	LatchPointCm = Hit.PointCm;
	switch (Hit.Target)
	{
	case EDeepHarpoonTarget::Submarine:
	{
		if (ADeepGameState* State = GetGameState())
		{
			State->HullDamage += 1;
			State->HullRepairsOwed += 1;
		}
		const ADeepSubmarine* Sub = Cast<ADeepSubmarine>(Hit.Actor.Get());
		if (!Sub)
		{
			Sub = DeepArena::FindSubmarine(GetWorld());
		}
		Phase = EDeepHarpoonPhase::Latched;
		LatchTarget = EDeepHarpoonTarget::Submarine;
		// Just under the airlock, inside the home radius (the winch's approach point), so the run re-tethers.
		const FDeepTetherTuning& Tether = UDeepTuningSettings::Get().Tether;
		const float BelowM = FMath::Clamp(Tether.WinchApproachBelowM, 0.f, FMath::Max(0.f, Tether.WinchArriveM - 0.5f));
		ReelTargetCm = Sub ? Sub->GetAnchorLocation() - FVector(0.f, 0.f, BelowM * HarpoonComponentCmPerM) : Hit.PointCm;
		SetResult(EDeepHarpoonResult::LatchedSub);
		Fire(DeepHarpoonEvent::HitSub);
		break;
	}
	case EDeepHarpoonTarget::Monster:
	{
		// As if the lamp had driven it off, and loud. The serpent's own rules carry on from Recoil.
		if (UDeepMonsterBrain* Brain = GetWorld()->GetSubsystem<UDeepMonsterBrain>())
		{
			Brain->ForceState(EDeepMonsterState::Recoil);
		}
		UDeepMonsterRules::ReportDiverNoise(Diver, Hit.PointCm, Settings.MonsterHitNoise, Diver);
		Phase = EDeepHarpoonPhase::Idle;
		LatchTarget = EDeepHarpoonTarget::None;
		DestroyBolt(0.4f);
		SetResult(EDeepHarpoonResult::HitMonster);
		Fire(DeepHarpoonEvent::HitMonster);
		break;
	}
	default:
		Phase = EDeepHarpoonPhase::Latched;
		LatchTarget = EDeepHarpoonTarget::Terrain;
		ReelTargetCm = Hit.PointCm + Hit.NormalCm.GetSafeNormal(UE_SMALL_NUMBER, -BoltDir) * Settings.TerrainStandOffM * HarpoonComponentCmPerM;
		SetResult(EDeepHarpoonResult::LatchedTerrain);
		Fire(DeepHarpoonEvent::HitTerrain);
		break;
	}
}

void UDeepHarpoonComponent::EndFlight(EDeepHarpoonResult Result)
{
	Phase = EDeepHarpoonPhase::Idle;
	LatchTarget = EDeepHarpoonTarget::None;
	LastOutOfRangeM = Result == EDeepHarpoonResult::OutOfRange ? BeyondRangeCm / HarpoonComponentCmPerM : 0.f;
	// Leave the spent bolt hanging at the end of the line for a moment so the miss is seen.
	DestroyBolt(0.5f);
	SetResult(Result);
	Fire(DeepHarpoonEvent::Missed);
}

void UDeepHarpoonComponent::StepReel(float DeltaSeconds)
{
	ADeepDiver* Diver = GetDiver();
	const UDeepHarpoonSettings& Settings = UDeepHarpoonSettings::Get();
	const bool bSub = LatchTarget == EDeepHarpoonTarget::Submarine;
	const float Speed = bSub ? Settings.SubReelSpeedMps : Settings.TerrainReelSpeedMps;

	// Only the machine that moves the diver reels it: the server, or the owning client for its own prediction.
	if (Diver->HasAuthority() || Diver->IsLocallyControlled())
	{
		const FVector Next = UDeepHarpoonRules::ReelStep(Diver->GetActorLocation(), ReelTargetCm, Speed, DeltaSeconds);
		// The line always wins, like the winch: straight through whatever is in the way.
		Diver->SetActorLocation(Next, false, nullptr, ETeleportType::TeleportPhysics);
		if (UCharacterMovementComponent* Movement = Diver->GetCharacterMovement())
		{
			Movement->Velocity = FVector::ZeroVector;
		}
	}
	if (!Diver->HasAuthority())
	{
		return;
	}
	LatchSeconds += DeltaSeconds;
	const FVector At = Diver->GetActorLocation();
	bool bArrived = FVector::Dist(At, ReelTargetCm) <= Settings.ArriveM * HarpoonComponentCmPerM;
	if (bSub && !bArrived)
	{
		const UDeepTetherComponent* Tether = Diver->GetTether();
		bArrived = Tether && Tether->HasAnchor() && UDeepRules::IsHome(At, Tether->GetAnchorLocation(), UDeepTuningSettings::Get().Tether);
	}
	if (bArrived || LatchSeconds >= Settings.MaxLatchSeconds)
	{
		Phase = EDeepHarpoonPhase::Idle;
		LatchTarget = EDeepHarpoonTarget::None;
		LatchSeconds = 0.f;
		DestroyBolt();
		SetResult(bArrived ? EDeepHarpoonResult::Arrived : EDeepHarpoonResult::Released);
	}
}

// ---------------------------------------------------------------- aim

FDeepHarpoonTrace UDeepHarpoonComponent::TraceAim(const FVector& OriginCm, const FVector& Direction, float MaxCm) const
{
	FDeepHarpoonTrace Out;
	UWorld* World = GetWorld();
	const FVector Dir = Direction.GetSafeNormal();
	if (!World || MaxCm <= 0.f || Dir.IsNearlyZero())
	{
		return Out;
	}
	const FVector End = OriginCm + Dir * MaxCm;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DeepHarpoonAim), false);
	for (TActorIterator<ADeepDiver> It(World); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	if (Bolt)
	{
		Params.AddIgnoredActor(Bolt);
	}
	FHitResult Hit;
	float BestCm = MaxCm + 1.f;
	if (World->LineTraceSingleByChannel(Hit, OriginCm, End, ECC_Visibility, Params))
	{
		BestCm = static_cast<float>(Hit.Distance);
		Out.PointCm = Hit.ImpactPoint;
		Out.NormalCm = Hit.ImpactNormal;
		Out.DistanceCm = BestCm;
		Out.Actor = Hit.GetActor();
		Out.Target = Cast<ADeepSubmarine>(Hit.GetActor()) ? EDeepHarpoonTarget::Submarine : EDeepHarpoonTarget::Terrain;
	}

	// The serpent has no collision (it swims through everything): test its head and body as spheres.
	const UDeepMonsterBrain* Brain = World->GetSubsystem<UDeepMonsterBrain>();
	ADeepSerpent* Serpent = Brain ? Brain->GetSerpent() : nullptr;
	if (Serpent)
	{
		auto TrySphere = [&](const FVector& Centre, float RadiusCm)
		{
			const float T = UDeepHarpoonRules::SegmentSphereHit(OriginCm, End, Centre, RadiusCm);
			if (T >= 0.f && T * MaxCm < BestCm)
			{
				BestCm = T * MaxCm;
				Out.Target = EDeepHarpoonTarget::Monster;
				Out.PointCm = OriginCm + Dir * BestCm;
				Out.NormalCm = (Out.PointCm - Centre).GetSafeNormal(UE_SMALL_NUMBER, -Dir);
				Out.DistanceCm = BestCm;
				Out.Actor = Serpent;
			}
		};
		TrySphere(Serpent->GetHeadLocation(), UDeepHarpoonSettings::Get().MonsterHeadRadiusM * HarpoonComponentCmPerM);
		for (const FTransform& Segment : Serpent->GetSegmentTransforms())
		{
			TrySphere(Segment.GetLocation(), static_cast<float>(Segment.GetScale3D().GetMax()) * SphereRadiusCmAtScale1);
		}
	}
	return Out;
}

// ---------------------------------------------------------------- feedback

void UDeepHarpoonComponent::SetResult(EDeepHarpoonResult Result)
{
	LastResult = Result;
	LastResultTime = ServerNow();
}

void UDeepHarpoonComponent::DestroyBolt(float AfterSeconds)
{
	if (Bolt)
	{
		if (AfterSeconds > 0.f)
		{
			Bolt->SetLifeSpan(AfterSeconds);
		}
		else
		{
			Bolt->Destroy();
		}
	}
	Bolt = nullptr;
}

void UDeepHarpoonComponent::Fire(const TCHAR* Event)
{
	ADeepDiver* Diver = GetDiver();
	FDeepEventPayload Payload;
	const ADeepGameState* State = GetGameState();
	Payload.DiverNumber = State ? State->DiverNumber : 1;
	Payload.DepthM = Diver ? Diver->GetDepthM() : 0.f;
	Payload.Source = Diver;
	const FName Name(Event);
	if (UDeepEventBus* Bus = UDeepEventBus::Get(this))
	{
		Bus->Broadcast(Name, Payload);
	}
	// A remote player's diver: their machine has its own bus and Captain.
	if (Diver && !Diver->IsLocallyControlled() && Cast<APlayerController>(Diver->GetController()))
	{
		ClientEvent(Name, Payload);
	}
}

void UDeepHarpoonComponent::ClientEvent_Implementation(FName Event, const FDeepEventPayload& Payload)
{
	if (GetOwner()->HasAuthority())
	{
		return;
	}
	if (UDeepEventBus* Bus = UDeepEventBus::Get(this))
	{
		Bus->Broadcast(Event, Payload);
	}
}

void UDeepHarpoonComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDeepHarpoonComponent, ShotsLeft);
	DOREPLIFETIME(UDeepHarpoonComponent, Phase);
	DOREPLIFETIME(UDeepHarpoonComponent, LatchTarget);
	DOREPLIFETIME(UDeepHarpoonComponent, LatchPointCm);
	DOREPLIFETIME(UDeepHarpoonComponent, ReelTargetCm);
	DOREPLIFETIME(UDeepHarpoonComponent, LastResult);
	DOREPLIFETIME(UDeepHarpoonComponent, LastResultTime);
	DOREPLIFETIME(UDeepHarpoonComponent, LastOutOfRangeM);
	DOREPLIFETIME(UDeepHarpoonComponent, Bolt);
}

// ---------------------------------------------------------------- settlement

int32 DeepHarpoon::BillHullRepairs(ADeepGameState* GameState, TArray<FString>& Labels, TArray<int32>& Amounts)
{
	if (!GameState || GameState->HullRepairsOwed <= 0)
	{
		return 0;
	}
	const UDeepHarpoonSettings& Settings = UDeepHarpoonSettings::Get();
	const int32 Hits = GameState->HullRepairsOwed;
	const int32 PerHit = UDeepHarpoonRules::RepairFee(1, Settings.RepairFeePerHit);
	for (int32 Index = 0; Index < Hits; ++Index)
	{
		Labels.Add(Settings.RepairFeeLabel);
		Amounts.Add(PerHit);
	}
	GameState->HullRepairsOwed = 0;
	return UDeepHarpoonRules::RepairFee(Hits, Settings.RepairFeePerHit);
}

FString DeepHarpoon::PhaseName(EDeepHarpoonPhase Phase)
{
	switch (Phase)
	{
	case EDeepHarpoonPhase::Flying: return TEXT("flying");
	case EDeepHarpoonPhase::Latched: return TEXT("latched");
	default: return TEXT("idle");
	}
}

FString DeepHarpoon::TargetName(EDeepHarpoonTarget Target)
{
	switch (Target)
	{
	case EDeepHarpoonTarget::Submarine: return TEXT("sub");
	case EDeepHarpoonTarget::Terrain: return TEXT("terrain");
	case EDeepHarpoonTarget::Monster: return TEXT("monster");
	default: return TEXT("none");
	}
}

FString DeepHarpoon::ResultName(EDeepHarpoonResult Result)
{
	switch (Result)
	{
	case EDeepHarpoonResult::Fired: return TEXT("fired");
	case EDeepHarpoonResult::LatchedSub: return TEXT("latched_sub");
	case EDeepHarpoonResult::LatchedTerrain: return TEXT("latched_terrain");
	case EDeepHarpoonResult::HitMonster: return TEXT("hit_monster");
	case EDeepHarpoonResult::Missed: return TEXT("missed");
	case EDeepHarpoonResult::OutOfRange: return TEXT("out_of_range");
	case EDeepHarpoonResult::Empty: return TEXT("empty");
	case EDeepHarpoonResult::Released: return TEXT("released");
	case EDeepHarpoonResult::Arrived: return TEXT("arrived");
	default: return TEXT("none");
	}
}

TSharedRef<FJsonObject> DeepHarpoon::BuildStateJson(const UDeepHarpoonComponent& Harpoon)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("shots"), Harpoon.GetShotsLeft());
	Json->SetStringField(TEXT("phase"), PhaseName(Harpoon.GetPhase()));
	Json->SetStringField(TEXT("latch"), TargetName(Harpoon.GetLatchTarget()));
	Json->SetStringField(TEXT("last"), ResultName(Harpoon.GetLastResult()));
	const FVector Latch = Harpoon.GetLatchPoint();
	TArray<TSharedPtr<FJsonValue>> LatchArray = {
		MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Latch.X)),
		MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Latch.Y)),
		MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Latch.Z)),
	};
	Json->SetArrayField(TEXT("latch_cm"), LatchArray);
	return Json;
}
