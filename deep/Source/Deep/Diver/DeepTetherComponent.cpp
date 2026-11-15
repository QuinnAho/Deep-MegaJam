#include "Diver/DeepTetherComponent.h"

#include "Components/CapsuleComponent.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Sim/DeepAirLineRules.h"
#include "Engine/World.h"
#include "Game/DeepGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepArena.h"
#include "Monster/DeepMonsterBrain.h"
#include "Monster/DeepSerpent.h"
#include "World/DeepRopeComponent.h"
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
	if (const ADeepDiver* Diver = Cast<ADeepDiver>(GetOwner()); Diver && Diver->IsLocallyControlled())
	{
		BindInput();
	}
	UpdateTether(DeltaTime);
}

void UDeepTetherComponent::UpdateTether(float DeltaSeconds)
{
	ADeepDiver* Diver = Cast<ADeepDiver>(GetOwner());
	if (!Diver || !HasAnchor())
	{
		if (Line)
		{
			Line->HideRope();
		}
		bLineWasTethered = false;
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
		UpdateCutLine(DeltaSeconds, Run->GetState());
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
	UpdateLine(DeltaSeconds);
	// The air line's one enemy: the serpent's head on the rope cuts it (server only, once: the state leaves Tethered).
	CheckSerpentBite();
}

void UDeepTetherComponent::UpdateLine(float DeltaSeconds)
{
	ADeepDiver* Diver = Cast<ADeepDiver>(GetOwner());
	if (!Line || !Diver)
	{
		return;
	}
	const FVector Start = GetAnchorLocation();
	const FVector End = Diver->GetTetherAttachLocation();
	const float Straight = static_cast<float>(FVector::Dist(Start, End));
	// A whole line again (a new dive, a re-tether): lay a fresh rope from the hatch.
	if (!bLineWasTethered || Line->IsCut())
	{
		Line->Restart();
		ShownLengthCm = Straight;
	}
	bLineWasTethered = true;
	const FDeepRopeTuning& Rope = UDeepTuningSettings::Get().Rope;
	const float TargetCm = UDeepRules::RopeShownLengthCm(Straight, Solve.LengthM * UDeepRules::CmPerM,
		Diver->IsWinching() || Solve.bTaut, Rope);
	// Pays out and takes up smoothly, but never shorter than the straight line now.
	ShownLengthCm = FMath::Max(Straight, FMath::FInterpTo(ShownLengthCm, TargetCm, DeltaSeconds, Rope.LengthFollowPerS));
	Line->Drive(DeltaSeconds, &Start, &End, ShownLengthCm, Rope.SinkCmS2);
}

void UDeepTetherComponent::UpdateCutLine(float DeltaSeconds, EDeepRunState RunState)
{
	ADeepDiver* Diver = Cast<ADeepDiver>(GetOwner());
	if (!Line || !Diver)
	{
		return;
	}
	const FDeepRopeTuning& Rope = UDeepTuningSettings::Get().Rope;
	const bool bWasWhole = bLineWasTethered;
	bLineWasTethered = false;
	if (const UDeepRunComponent* Run = Diver->GetRun(); RunState == EDeepRunState::Cut && Run && Run->IsUnclipped())
	{
		// Unclipped by choice: the line stays whole and hangs from the hatch, its end loose, waiting to be clipped on.
		const FVector Hatch = GetAnchorLocation();
		Line->Drive(DeltaSeconds, &Hatch, nullptr, ShownLengthCm, Rope.LooseSinkCmS2);
		return;
	}
	if (bWasWhole && RunState == EDeepRunState::Cut && Line->IsShown())
	{
		// Bitten through: the line parts at the serpent's jaws. Otherwise the Captain cut it at the hatch.
		int32 Link = INDEX_NONE;
		const UWorld* World = GetWorld();
		const UDeepMonsterBrain* Brain = World ? World->GetSubsystem<UDeepMonsterBrain>() : nullptr;
		if (const ADeepSerpent* Serpent = Brain ? Brain->GetSerpent() : nullptr)
		{
			const FVector Head = Serpent->GetHeadLocation();
			const int32 Near = Line->GetSim().NearestLink(Head);
			if (Near != INDEX_NONE)
			{
				const FDeepRopeSim& Sim = Line->GetSim();
				const FVector OnLine = FMath::ClosestPointOnSegment(Head, Sim.Pos[Near], Sim.Pos[Near + 1]);
				if (FVector::Dist(OnLine, Head) <= Rope.BiteNearM * UDeepRules::CmPerM)
				{
					Link = Line->CutNear(Head);
				}
			}
		}
		if (Link == INDEX_NONE)
		{
			Line->CutAtStart();
		}
	}
	if (!Line->IsCut() || (RunState != EDeepRunState::Cut && RunState != EDeepRunState::Dead))
	{
		Line->HideRope();
		return;
	}
	// The hatch end keeps its stub; the diver's end stays clipped to the harness while the diver swims, and lets go
	// when the diver is lost.
	const FVector Start = GetAnchorLocation();
	const FVector End = Diver->GetTetherAttachLocation();
	Line->Drive(DeltaSeconds, &Start, RunState == EDeepRunState::Cut ? &End : nullptr, ShownLengthCm, Rope.LooseSinkCmS2);
}

// ---------------------------------------------------------------- the air line: the serpent's bite, the unclip key

bool UDeepTetherComponent::CheckSerpentBite()
{
	ADeepDiver* Diver = Cast<ADeepDiver>(GetOwner());
	UDeepRunComponent* Run = Diver ? Diver->GetRun() : nullptr;
	if (!Diver || !Diver->HasAuthority() || !Run || !Run->HasTether() || !HasAnchor())
	{
		return false;
	}
	const UWorld* World = GetWorld();
	const UDeepMonsterBrain* Brain = World ? World->GetSubsystem<UDeepMonsterBrain>() : nullptr;
	const ADeepSerpent* Serpent = Brain ? Brain->GetSerpent() : nullptr;
	const FDeepAirLineTuning& AirLine = UDeepTuningSettings::Get().AirLine;
	if (!Serpent || !DeepAirLine::StateBitesLine(Brain->GetState(), AirLine))
	{
		return false;
	}
	// The rope as simulated (what the player sees); the straight line until it has been laid.
	TArray<FVector> Straight;
	TConstArrayView<FVector> Points;
	TConstArrayView<uint8> Linked;
	if (Line && Line->IsShown() && Line->GetSim().IsValid())
	{
		Points = Line->GetSim().Pos;
		Linked = Line->GetSim().Linked;
	}
	else
	{
		Straight = { GetAnchorLocation(), Diver->GetTetherAttachLocation() };
		Points = Straight;
	}
	if (DeepAirLine::TouchedLink(Points, Linked, Serpent->GetHeadLocation(), AirLine.BiteTouchM) == INDEX_NONE)
	{
		return false;
	}
	Run->CutTetherFor(DeepAirLine::TetherBitten);
	return Run->GetState() == EDeepRunState::Cut;
}

FKey UDeepTetherComponent::UnclipKey()
{
	// Free while swimming: WASD, Space/C/Ctrl, Shift, E, G, R, F, V, Q and the left button are taken.
	return EKeys::T;
}

void UDeepTetherComponent::BindInput()
{
	const ADeepDiver* Diver = Cast<ADeepDiver>(GetOwner());
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
	if (!UnclipAction)
	{
		UnclipAction = NewObject<UInputAction>(this, TEXT("IA_Unclip"));
		UnclipAction->ValueType = EInputActionValueType::Boolean;
		UnclipAction->bConsumeInput = false;
		UnclipMapping = NewObject<UInputMappingContext>(this, TEXT("IMC_AirLine"));
		UnclipMapping->MapKey(UnclipAction, UnclipKey());
	}
	Input->BindAction(UnclipAction, ETriggerEvent::Started, this, &UDeepTetherComponent::PressUnclip);
	const ULocalPlayer* Local = Player->GetLocalPlayer();
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = Local ? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
	{
		if (!Subsystem->HasMappingContext(UnclipMapping))
		{
			Subsystem->AddMappingContext(UnclipMapping, 1);
		}
	}
	BoundInputComponent = Input;
}

void UDeepTetherComponent::PressUnclip()
{
	ADeepDiver* Diver = Cast<ADeepDiver>(GetOwner());
	UDeepRunComponent* Run = Diver ? Diver->GetRun() : nullptr;
	if (Run && Run->HasTether() && Run->CanSwim())
	{
		Run->RequestUnclip();
	}
}

void UDeepTetherComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UnclipMapping)
	{
		const ADeepDiver* Diver = Cast<ADeepDiver>(GetOwner());
		const APlayerController* Player = Diver ? Cast<APlayerController>(Diver->GetController()) : nullptr;
		const ULocalPlayer* Local = Player ? Player->GetLocalPlayer() : nullptr;
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = Local ? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
		{
			Subsystem->RemoveMappingContext(UnclipMapping);
		}
	}
	Super::EndPlay(EndPlayReason);
}
