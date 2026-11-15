#include "World/DeepBubblesComponent.h"

#include "Audio/DeepAudioSettings.h"
#include "Components/StaticMeshComponent.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Diver/DeepTetherComponent.h"
#include "Engine/World.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepShapes.h"

UDeepBubblesComponent::UDeepBubblesComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
	Rng.Initialize(7919);
}

void UDeepBubblesComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsurePool();
}

void UDeepBubblesComponent::EnsurePool()
{
	AActor* Owner = GetOwner();
	const FDeepBubbleTuning& Tuning = UDeepTuningSettings::Get().Bubbles;
	const int32 Want = FMath::Clamp(Tuning.PoolSize, 0, 256);
	if (!Owner || !GetWorld() || Meshes.Num() == Want)
	{
		return;
	}
	Bubbles.SetNum(Want);
	UStaticMesh* Mesh = DeepShapes::LoadMesh(DeepShapes::Sphere);
	UMaterialInterface* Shared = nullptr;
	for (int32 i = Meshes.Num(); i < Want; ++i)
	{
		UStaticMeshComponent* Bubble = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
		Bubble->SetStaticMesh(Mesh);
		Bubble->SetMobility(EComponentMobility::Movable);
		Bubble->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Bubble->SetGenerateOverlapEvents(false);
		Bubble->SetCanEverAffectNavigation(false);
		Bubble->SetUsingAbsoluteLocation(true);
		Bubble->SetUsingAbsoluteRotation(true);
		Bubble->SetUsingAbsoluteScale(true);
		Bubble->SetCastShadow(false);
		Bubble->SetupAttachment(this);
		Bubble->SetVisibility(false);
		Bubble->RegisterComponent();
		// Air seen through water mirrors the light: bare metal, low roughness, no paint, no rust. One instance for all.
		if (!Shared)
		{
			DeepShapes::Steel(Bubble, Tuning.Tint, 0.f, 0.f, Tuning.Roughness, 1.f);
			Shared = Bubble->GetMaterial(0);
		}
		else
		{
			Bubble->SetMaterial(0, Shared);
		}
		Meshes.Add(Bubble);
	}
}

int32 UDeepBubblesComponent::CountAlive() const
{
	int32 Count = 0;
	for (const FDeepBubble& B : Bubbles)
	{
		Count += B.bAlive ? 1 : 0;
	}
	return Count;
}

void UDeepBubblesComponent::Emit(const FVector& AtCm, int32 Count, float SpreadCm, const FVector& KickCmS, float SizeScale)
{
	EnsurePool();
	if (Bubbles.Num() == 0)
	{
		return;
	}
	const FDeepBubbleTuning& Tuning = UDeepTuningSettings::Get().Bubbles;
	for (int32 k = 0; k < Count; ++k)
	{
		// Oldest first: the pool wraps, so a long stream recycles the bubbles highest above.
		FDeepBubble& B = Bubbles[Next];
		Next = (Next + 1) % Bubbles.Num();
		B.bAlive = true;
		B.AgeS = 0.f;
		// Many small, a few big: square the random number.
		B.SizeCm = FMath::Lerp(Tuning.MinSizeCm, Tuning.MaxSizeCm, FMath::Square(Rng.FRand())) * SizeScale;
		B.LifeS = Tuning.LifeS * Rng.FRandRange(0.75f, 1.15f);
		B.Phase = Rng.FRandRange(0.f, 2.f * PI);
		B.CentreCm = AtCm + Rng.VRand() * (SpreadCm * Rng.FRand());
		B.KickCmS = KickCmS + Rng.VRand() * (KickCmS.Size() * 0.35f + 8.f);
	}
}

void UDeepBubblesComponent::UpdateSources(float DeltaSeconds)
{
	const ADeepDiver* Diver = Cast<ADeepDiver>(GetOwner());
	const UDeepRunComponent* Run = Diver ? Diver->GetRun() : nullptr;
	if (!Diver || !Run || DeltaSeconds <= 0.f)
	{
		return;
	}
	const FDeepBubbleTuning& Tuning = UDeepTuningSettings::Get().Bubbles;
	const bool bSwimming = Run->CanSwim();
	const UDeepTetherComponent* Tether = Diver->GetTether();
	const FVector Hatch = Tether && Tether->HasAnchor() ? Tether->GetAnchorLocation() : Diver->GetActorLocation();

	// Into the water through the hatch: air shed off the suit as the diver drops in.
	if (bSwimming && !bWasSwimming && FVector::Dist(Diver->GetActorLocation(), Hatch) < 600.f)
	{
		HatchStreamLeftS = 0.8f;
		BreathClockS = 0.f;
	}
	bWasSwimming = bSwimming;
	if (HatchStreamLeftS > 0.f)
	{
		HatchStreamLeftS -= DeltaSeconds;
		const int32 Count = FMath::Max(1, FMath::RoundToInt(Tuning.HatchBurst * DeltaSeconds / 0.8f));
		Emit(Diver->GetActorLocation() + FVector(0.f, 0.f, 40.f), Count, 45.f, FVector(0.f, 0.f, -30.f), 1.4f);
	}

	// Breath: bubbles over each breath out, on the breath loop's rhythm (fast on low bailout air, as the sound).
	if (bSwimming)
	{
		const bool bFast = Run->GetState() == EDeepRunState::Cut && Run->GetBailoutAirSeconds() <= UDeepAudioSettings::Get().FastBreathAirS;
		const float Period = bFast ? Tuning.FastBreathPeriodS : Tuning.BreathPeriodS;
		BreathClockS += DeltaSeconds;
		if (DeepBubbleSim::IsExhaling(BreathClockS, Period, Tuning))
		{
			const float ExhaleS = FMath::Max(0.1f, Period * (Tuning.ExhaleEnd01 - Tuning.ExhaleStart01));
			BreathCarry += Tuning.PerBreath * DeltaSeconds / ExhaleS;
			const int32 Count = FMath::FloorToInt(BreathCarry);
			if (Count > 0)
			{
				BreathCarry -= Count;
				const FRotator View = Diver->GetViewRotation();
				const FVector Mouth = Diver->GetPawnViewLocation() + View.RotateVector(Tuning.MouthOffsetCm);
				Emit(Mouth, Count, 3.f, View.Vector() * 25.f);
			}
		}
	}

	// The winch: the drum and the line churn the water at the hatch.
	if (Diver->IsWinching())
	{
		WinchCarry += Tuning.WinchPerS * DeltaSeconds;
		const int32 Count = FMath::FloorToInt(WinchCarry);
		if (Count > 0)
		{
			WinchCarry -= Count;
			Emit(Hatch - FVector(0.f, 0.f, 80.f), Count, 35.f, FVector(0.f, 0.f, -20.f), 1.2f);
		}
	}
}

void UDeepBubblesComponent::Lay()
{
	const FDeepBubbleTuning& Tuning = UDeepTuningSettings::Get().Bubbles;
	for (int32 i = 0; i < Meshes.Num() && i < Bubbles.Num(); ++i)
	{
		UStaticMeshComponent* Mesh = Meshes[i];
		const FDeepBubble& B = Bubbles[i];
		if (!Mesh)
		{
			continue;
		}
		if (!B.bAlive)
		{
			if (Mesh->IsVisible())
			{
				Mesh->SetVisibility(false);
			}
			continue;
		}
		const float Scale = FMath::Max(0.01f, DeepBubbleSim::DrawnSizeCm(B)) / DeepShapes::ShapeSizeCm;
		// A little flattened, as air in water is.
		Mesh->SetWorldTransform(FTransform(FQuat::Identity, DeepBubbleSim::DrawnCm(B, Tuning), FVector(Scale, Scale, Scale * 0.8f)));
		if (!Mesh->IsVisible())
		{
			Mesh->SetVisibility(true);
		}
	}
}

void UDeepBubblesComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	EnsurePool();
	UpdateSources(DeltaTime);
	const FDeepBubbleTuning& Tuning = UDeepTuningSettings::Get().Bubbles;
	for (FDeepBubble& B : Bubbles)
	{
		DeepBubbleSim::Step(B, DeltaTime, Tuning);
	}
	Lay();
}
