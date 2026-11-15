#include "Diver/DeepTetherComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/World.h"
#include "Game/DeepGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepArena.h"
#include "World/DeepShapes.h"
#include "World/DeepSubmarine.h"

UDeepTetherComponent::UDeepTetherComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

ADeepSubmarine* UDeepTetherComponent::GetSubmarine() const
{
	if (!Submarine.IsValid())
	{
		Submarine = DeepArena::FindSubmarine(GetWorld());
	}
	return Submarine.Get();
}

bool UDeepTetherComponent::HasAnchor() const
{
	return GetSubmarine() != nullptr;
}

FVector UDeepTetherComponent::GetAnchorLocation() const
{
	const ADeepSubmarine* Sub = GetSubmarine();
	return Sub ? Sub->GetAnchorLocation() : FVector::ZeroVector;
}

int32 UDeepTetherComponent::CurrentStop() const
{
	const UWorld* World = GetWorld();
	const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
	return State ? State->Stop : 1;
}

float UDeepTetherComponent::GetAnchorDepthM() const
{
	const UWorld* World = GetWorld();
	const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
	return State ? State->SubDepthM : UDeepTuningSettings::Get().GetStop(CurrentStop()).SubDepthM;
}

void UDeepTetherComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateTether(DeltaTime);
}

void UDeepTetherComponent::UpdateTether(float DeltaSeconds)
{
	ADeepDiver* Diver = Cast<ADeepDiver>(GetOwner());
	if (!Diver || !HasAnchor())
	{
		if (Line)
		{
			Line->SetVisibility(false);
		}
		return;
	}

	const FDeepTetherTuning& Tuning = UDeepTuningSettings::Get().Tether;
	const FVector Anchor = GetAnchorLocation();
	UCharacterMovementComponent* Movement = Diver->GetCharacterMovement();

	// Cut (or dead): no limit, no line. Home still counts, so a cut diver can re-tether at the airlock.
	if (const UDeepRunComponent* Run = Diver->GetRun(); Run && !Run->HasTether())
	{
		Solve = FDeepTetherSolve();
		bHome = UDeepRules::IsHome(Diver->GetActorLocation(), Anchor, Tuning);
		if (Line)
		{
			Line->SetVisibility(false);
		}
		return;
	}

	// Only the machine that moves the diver corrects it: the server, or the owning client for its own prediction.
	const bool bMovesDiver = Diver->HasAuthority() || Diver->IsLocallyControlled();

	if (bMovesDiver && Diver->IsWinching())
	{
		// The winch routes round and under the hull (in the sub's frame) toward a point below the hatch, never
		// through it: the hull's collision box grown by the diver's capsule is the space the route stays out of.
		const ADeepSubmarine* Sub = GetSubmarine();
		const FTransform SubFrame = Sub->GetActorTransform();
		const UCapsuleComponent* Capsule = Diver->GetCapsuleComponent();
		const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 0.f;
		const FBox Blocked = Sub->GetSolidBoundsLocal(false).ExpandBy(FVector(Radius, Radius, Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.f));
		// The relic is heavy: the winch hauls it slower, which gives the serpent its chance at the last ascent.
		FDeepTetherTuning WinchTuning = Tuning;
		const UDeepRunComponent* WinchRun = Diver->GetRun();
		WinchTuning.WinchSpeedMps = UDeepRules::WinchSpeedMps(WinchRun && WinchRun->CarriesRelic(), Tuning);
		const FVector ReeledLocal = UDeepRules::WinchStepAroundHull(SubFrame.InverseTransformPosition(Diver->GetActorLocation()),
			SubFrame.InverseTransformPosition(Anchor), Blocked, DeltaSeconds, WinchTuning);
		const FVector Reeled = SubFrame.TransformPosition(ReeledLocal);
		Diver->SetActorLocation(Reeled, true);
		// The winch always wins: if something else blocked the sweep, the cable drags the diver through.
		if (FVector::DistSquared(Diver->GetActorLocation(), Reeled) > 1.0)
		{
			Diver->SetActorLocation(Reeled, false, nullptr, ETeleportType::TeleportPhysics);
		}
		if (Movement)
		{
			Movement->Velocity = FVector::ZeroVector;
		}
	}

	Solve = UDeepRules::SolveTether(Anchor, Diver->GetActorLocation(), UDeepRules::TetherLengthM(CurrentStop(), Tuning), Tuning);
	if (bMovesDiver && Solve.bClamped)
	{
		Diver->SetActorLocation(Solve.ClampedPositionCm, false);
		if (Movement)
		{
			// Kill only the outward part of the velocity so the diver can still slide along the sphere.
			const FVector Out = (Solve.ClampedPositionCm - Anchor).GetSafeNormal();
			const double Outward = FVector::DotProduct(Movement->Velocity, Out);
			if (Outward > 0.0)
			{
				Movement->Velocity -= Out * Outward;
			}
		}
	}
	bHome = UDeepRules::IsHome(Diver->GetActorLocation(), Anchor, Tuning);
	UpdateLine();
}

void UDeepTetherComponent::UpdateLine()
{
	ADeepDiver* Diver = Cast<ADeepDiver>(GetOwner());
	if (!Line || !Diver)
	{
		return;
	}
	const FVector Start = GetAnchorLocation();
	const FVector End = Diver->GetTetherAttachLocation();
	const FVector Span = End - Start;
	const float Length = static_cast<float>(Span.Size());
	if (Length < 1.f)
	{
		Line->SetVisibility(false);
		return;
	}
	const float Thickness = UDeepTuningSettings::Get().Tether.LineThicknessCm / DeepShapes::ShapeSizeCm;
	Line->SetVisibility(true);
	Line->SetWorldLocationAndRotation((Start + End) * 0.5, FRotationMatrix::MakeFromZ(Span / Length).Rotator());
	Line->SetWorldScale3D(FVector(Thickness, Thickness, Length / DeepShapes::ShapeSizeCm));
}
