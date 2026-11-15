#include "Monster/DeepSerpentController.h"

#include "Components/SpotLightComponent.h"
#include "Diver/DeepDiver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameState.h"
#include "Monster/DeepMonsterBrain.h"
#include "Monster/DeepMonsterRules.h"
#include "Monster/DeepMonsterSettings.h"
#include "Monster/DeepSerpent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISense_Hearing.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepSerpent, Log, All);

namespace
{
	using DeepMonster::CmPerM;
}

ADeepSerpentController::ADeepSerpentController()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = false;

	// It hears you: the engine hearing sense. Divers have no team, so hear everyone.
	UAIPerceptionComponent* Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = UDeepMonsterSettings::Get().Behaviour.HearingRangeM * CmPerM;
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	HearingConfig->SetMaxAge(1.f);
	Perception->ConfigureSense(*HearingConfig);
	Perception->SetDominantSense(UAISense_Hearing::StaticClass());
	SetPerceptionComponent(*Perception);
}

void ADeepSerpentController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (UAIPerceptionComponent* Perception = GetAIPerceptionComponent())
	{
		Perception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &ADeepSerpentController::HandlePerceptionUpdated);
	}
	State = EDeepMonsterState::Lurk;
	StateSeconds = 0.f;
	LitSeconds = 0.f;
	CooldownS = 0.f;
	bFirstSightDone = false;
	PassPoints.Reset();
	PassLeg = 0;
}

void ADeepSerpentController::HandlePerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>() && Stimulus.WasSuccessfullySensed())
	{
		PendingNoise += FMath::Max(0.f, Stimulus.Strength);
		HeardTotal += FMath::Max(0.f, Stimulus.Strength);
	}
}

void ADeepSerpentController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		StepMonster(DeltaSeconds);
	}
}

// ---------------------------------------------------------------- queries

ADeepSerpent* ADeepSerpentController::GetSerpent() const
{
	return Cast<ADeepSerpent>(GetPawn());
}

UDeepMonsterBrain* ADeepSerpentController::GetBrain() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UDeepMonsterBrain>() : nullptr;
}

void ADeepSerpentController::GatherDivers(TArray<ADeepDiver*>& Out) const
{
	Out.Reset();
	if (const UDeepMonsterBrain* Brain = GetBrain())
	{
		Brain->GatherDivers(Out);
	}
}

ADeepDiver* ADeepSerpentController::PickFocus(const TArray<ADeepDiver*>& Divers) const
{
	const UDeepMonsterBrain* Brain = GetBrain();
	const FVector From = GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;
	ADeepDiver* Best = nullptr;
	double BestScore = TNumericLimits<double>::Max();
	for (ADeepDiver* Diver : Divers)
	{
		// Nearest diver, but an untethered one always wins: that is who it hunts.
		const bool bTethered = Brain && Brain->IsTethered(Diver);
		const double Score = FVector::DistSquared(From, Diver->GetActorLocation()) + (bTethered ? 1e12 : 0.0);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Diver;
		}
	}
	return Best;
}

FVector ADeepSerpentController::TetherMiddle(const ADeepDiver* Diver) const
{
	FVector A, B;
	const UDeepMonsterBrain* Brain = GetBrain();
	if (Brain && Brain->TetherEnds(Diver, A, B))
	{
		return (A + B) * 0.5f;
	}
	return Diver ? Diver->GetActorLocation() : FVector::ZeroVector;
}

float ADeepSerpentController::LurkRadiusCm(const ADeepDiver* Diver, const FDeepMonsterTuning& Tuning) const
{
	const UDeepMonsterBrain* Brain = GetBrain();
	return Brain ? Brain->LurkRadiusCm(Diver, Tuning) : Tuning.LurkRadiusM * CmPerM;
}

FVector ADeepSerpentController::BitePoint(const ADeepDiver* Diver, const FDeepMonsterTuning& Tuning) const
{
	FVector A, B;
	const UDeepMonsterBrain* Brain = GetBrain();
	if (!Brain || !Brain->TetherEnds(Diver, A, B))
	{
		return TetherMiddle(Diver);
	}
	const float LengthCm = static_cast<float>(FVector::Dist(A, B));
	const float Alpha = LengthCm > 1.f ? FMath::Clamp(1.f - Tuning.BiteFromDiverM * CmPerM / LengthCm, 0.1f, 1.f) : 0.5f;
	return FMath::Lerp(A, B, Alpha);
}

FVector ADeepSerpentController::GetBitePoint(const ADeepDiver* Diver) const
{
	const UWorld* World = GetWorld();
	const ADeepGameState* GameState = World ? World->GetGameState<ADeepGameState>() : nullptr;
	return BitePoint(Diver, UDeepMonsterSettings::Get().GetTuning(GameState ? GameState->Stop : 1));
}

bool ADeepSerpentController::IsFirstSightPending(const ADeepDiver* Diver, const FDeepMonsterTuning& Tuning) const
{
	const UDeepMonsterBrain* Brain = GetBrain();
	// Staged for a player's eyes: test divers and cheat spawns keep the old rules exactly.
	return Tuning.bFirstSight && !bFirstSightDone && Diver && Diver->IsPlayerControlled() && Brain && !Brain->IsForcedSpawn();
}

bool ADeepSerpentController::BeginPass(const ADeepDiver* Diver, const FDeepMonsterTuning& Tuning)
{
	ADeepSerpent* Body = GetSerpent();
	if (!Body || !Diver)
	{
		return false;
	}
	// The view is the lamp (it rides the camera). Pitch is limited so a diver looking at the floor still gets a pass in front.
	FVector Eye;
	FRotator View;
	if (const USpotLightComponent* Lamp = Diver->FindComponentByClass<USpotLightComponent>())
	{
		Eye = Lamp->GetComponentLocation();
		View = Lamp->GetComponentRotation();
	}
	else
	{
		Diver->GetActorEyesViewPoint(Eye, View);
	}
	View.Pitch = FMath::Clamp(View.Pitch, -30.f, 30.f);
	View.Roll = 0.f;
	const FRotationMatrix M(View);
	const FVector F = M.GetUnitAxis(EAxis::X);
	const FVector R = M.GetUnitAxis(EAxis::Y);
	const FVector U = M.GetUnitAxis(EAxis::Z);
	const float Side = FMath::RandBool() ? 1.f : -1.f;
	const FVector Base = Eye + U * (Tuning.PassUpM * CmPerM);

	const FVector Start = Base + F * (Tuning.PassDistanceM * CmPerM) - R * (Side * Tuning.PassSideStartM * CmPerM);
	PassPoints.Reset();
	PassPoints.Add(Base + F * (Tuning.PassDistanceM * CmPerM) + R * (Side * Tuning.PassTurnAtM * CmPerM));
	PassPoints.Add(Base + F * ((Tuning.PassDistanceM + Tuning.PassLeaveM) * CmPerM) + R * (Side * (Tuning.PassTurnAtM + Tuning.PassLeaveSideM) * CmPerM));
	PassLeg = 0;
	PassClosestM = 1000.f;
	// Out of view and far outside the beam, so the move is never seen; the body lies straight behind it.
	Body->PlaceHead(Start, R * Side);
	bFirstSightDone = true;
	UE_LOG(LogDeepSerpent, Log, TEXT("Serpent first sight: pass at %.0f m, from the %s"), Tuning.PassDistanceM, Side > 0.f ? TEXT("left") : TEXT("right"));
	return true;
}

bool ADeepSerpentController::StartFirstSight()
{
	UDeepMonsterBrain* Brain = GetBrain();
	UWorld* World = GetWorld();
	if (!Brain || !World)
	{
		return false;
	}
	TArray<ADeepDiver*> Divers;
	GatherDivers(Divers);
	if (!Focus.IsValid() || !Divers.Contains(Focus.Get()))
	{
		Focus = PickFocus(Divers);
	}
	const ADeepGameState* GameState = World->GetGameState<ADeepGameState>();
	const FDeepMonsterTuning Tuning = UDeepMonsterSettings::Get().GetTuning(GameState ? GameState->Stop : 1);
	if (!BeginPass(Focus.Get(), Tuning))
	{
		return false;
	}
	EnterState(EDeepMonsterState::Pass, Tuning);
	LitSeconds = 0.f;
	// The sound comes first: the run's first-heard line and ping.
	Brain->OnStalkStarted.Broadcast(Focus.Get());
	return true;
}

bool ADeepSerpentController::IsHeadLit(const TArray<ADeepDiver*>& Divers, const FDeepMonsterTuning& Tuning) const
{
	const APawn* Body = GetPawn();
	if (!Body)
	{
		return false;
	}
	const FVector Head = Body->GetActorLocation();
	const float TargetRadiusM = Tuning.BeamLeniencyM + UDeepMonsterSettings::Get().Body.HeadRadiusM;
	for (const ADeepDiver* Diver : Divers)
	{
		if (!Diver->IsLampOn())
		{
			continue;
		}
		const USpotLightComponent* Lamp = Diver->FindComponentByClass<USpotLightComponent>();
		if (Lamp && UDeepMonsterRules::IsInBeam(Lamp->GetComponentLocation(), Lamp->GetForwardVector(), Lamp->OuterConeAngle,
			Lamp->AttenuationRadius / CmPerM, Head, TargetRadiusM))
		{
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------- control

void ADeepSerpentController::ForceState(EDeepMonsterState NewState)
{
	State = NewState;
	StateSeconds = 0.f;
	LitSeconds = 0.f;
	CooldownS = 0.f;
	FVector A, B;
	const UDeepMonsterBrain* Brain = GetBrain();
	if (NewState == EDeepMonsterState::Bite && GetPawn() && Brain && Brain->TetherEnds(Focus.Get(), A, B))
	{
		BiteAlpha = UDeepMonsterRules::SegmentAlpha(GetPawn()->GetActorLocation(), A, B);
	}
}

void ADeepSerpentController::EnterState(EDeepMonsterState NewState, const FDeepMonsterTuning& Tuning)
{
	UE_LOG(LogDeepSerpent, Log, TEXT("Serpent %s -> %s (noise %.0f, tether %.1f m)"),
		*UDeepMonsterRules::StateName(State), *UDeepMonsterRules::StateName(NewState), Noise, TetherDistanceM);
	State = NewState;
	StateSeconds = 0.f;
	if (NewState == EDeepMonsterState::Stalk)
	{
		BiteSide = FVector::ZeroVector;
		bBiteCommit = false;
	}
	if (NewState == EDeepMonsterState::Recoil)
	{
		LitSeconds = 0.f;
		DebugLitS = 0.f;
		Noise *= Tuning.NoiseKeptOnRecoil;
	}
}

void ADeepSerpentController::StepMonster(float DeltaSeconds)
{
	ADeepSerpent* Body = GetSerpent();
	UDeepMonsterBrain* Brain = GetBrain();
	UWorld* World = GetWorld();
	if (!Body || !Brain || !World)
	{
		return;
	}
	const float Dt = FMath::Max(0.f, DeltaSeconds);
	const ADeepGameState* GameState = World->GetGameState<ADeepGameState>();
	const FDeepMonsterTuning Tuning = UDeepMonsterSettings::Get().GetTuning(GameState ? GameState->Stop : 1);

	// ---- What it heard (perception callbacks and cheats since the last step), then decay.
	Noise = UDeepMonsterRules::NoiseStep(Noise, PendingNoise, Dt, Tuning);
	PendingNoise = 0.f;

	// ---- Who it is after. Locked in while it bites, hunts or flees.
	TArray<ADeepDiver*> Divers;
	GatherDivers(Divers);
	if (!Focus.IsValid() || !Divers.Contains(Focus.Get()) || State == EDeepMonsterState::Lurk || State == EDeepMonsterState::Stalk)
	{
		Focus = PickFocus(Divers);
	}
	ADeepDiver* Target = Focus.Get();

	// ---- Lamp.
	const bool bLitNow = DebugLitS > 0.f || IsHeadLit(Divers, Tuning);
	DebugLitS = FMath::Max(0.f, DebugLitS - Dt);
	LitSeconds = UDeepMonsterRules::LitStep(LitSeconds, bLitNow, Dt, Tuning);

	// ---- Distances.
	const FVector Head = Body->GetHeadLocation();
	const bool bTethered = Brain->IsTethered(Target);
	DiverDistanceM = Target ? static_cast<float>(FVector::Dist(Head, Target->GetActorLocation())) / CmPerM : 1000.f;
	FVector A = FVector::ZeroVector;
	FVector B = FVector::ZeroVector;
	TetherDistanceM = Brain->TetherEnds(Target, A, B) ? UDeepMonsterRules::DistanceToSegmentM(Head, A, B) : DiverDistanceM;

	CooldownS = FMath::Max(0.f, CooldownS - Dt);
	StateSeconds += Dt;

	// ---- The decision: one pure function.
	FDeepMonsterInputs In;
	In.State = State;
	In.Noise = Noise;
	In.LitSeconds = LitSeconds;
	In.bLitNow = bLitNow;
	In.StateSeconds = StateSeconds;
	In.CooldownS = CooldownS;
	In.TetherDistanceM = TetherDistanceM;
	In.DiverDistanceM = DiverDistanceM;
	In.bDiverTethered = bTethered;
	In.bHasDiver = Target != nullptr;
	In.bFirstSightPending = IsFirstSightPending(Target, Tuning);
	if (State == EDeepMonsterState::Pass)
	{
		PassClosestM = FMath::Min(PassClosestM, DiverDistanceM);
		In.bPassComplete = PassLeg >= PassPoints.Num() && DiverDistanceM >= Tuning.PassEndM;
	}
	FDeepMonsterStep Result = UDeepMonsterRules::NextState(In, Tuning);
	if (Result.bPassStarted && !BeginPass(Target, Tuning))
	{
		// Nowhere to stage it: skip the first sight and stay put this step.
		bFirstSightDone = true;
		Result = FDeepMonsterStep();
		Result.Next = State;
	}

	if (Result.bBiteStarted)
	{
		BiteAlpha = UDeepMonsterRules::SegmentAlpha(Head, A, B);
	}
	if (Result.bStartCooldown)
	{
		CooldownS = Tuning.RecoilCooldownS;
	}
	if (Result.Changed(State))
	{
		EnterState(Result.Next, Tuning);
	}

	// Events last, after the state is consistent, so listeners that read it see the new state.
	// The first sight starts with a sound: the run's first-heard line (the run component says it once per run), so
	// by the first real stalk the Captain has already named it and says it is near instead.
	if (Result.bPassStarted)
	{
		Brain->OnStalkStarted.Broadcast(Target);
	}
	if (Result.Changed(In.State) && Result.Next == EDeepMonsterState::Stalk)
	{
		Brain->OnStalkStarted.Broadcast(Target);
	}
	if (Result.bRepelled)
	{
		Brain->OnRepelled.Broadcast(Target);
	}
	if (Result.bBiteStarted)
	{
		Brain->OnBiteStarted.Broadcast(Target, Tuning.BiteAlarmS);
	}
	if (Result.bBiteBroken)
	{
		Brain->OnBiteBroken.Broadcast(Target);
	}
	if (Result.bSevered)
	{
		Brain->NotifyTetherCut(Target);
		UE_LOG(LogDeepSerpent, Log, TEXT("Serpent severed the tether"));
		Brain->OnTetherSevered.Broadcast(Target);
	}
	if (Result.bCaught)
	{
		UE_LOG(LogDeepSerpent, Log, TEXT("Serpent caught an untethered diver"));
		Brain->OnDiverCaught.Broadcast(Target);
	}

	Steer(Tuning, Dt);
	Threat01 = Target ? UDeepMonsterRules::Threat01(State, DiverDistanceM, Tuning) : 0.f;
	Body->SetMood(State, Threat01);
}

void ADeepSerpentController::Steer(const FDeepMonsterTuning& Tuning, float DeltaSeconds)
{
	ADeepSerpent* Body = GetSerpent();
	const ADeepDiver* Target = Focus.Get();
	const UDeepMonsterBrain* Brain = GetBrain();
	if (!Body || !Target || !Brain)
	{
		return;
	}
	const FVector Head = Body->GetHeadLocation();
	const FVector Middle = TetherMiddle(Target);

	switch (State)
	{
	case EDeepMonsterState::Lurk:
		Body->SwimToward(UDeepMonsterRules::OrbitAimPoint(Middle, Head, LurkRadiusCm(Target, Tuning) / CmPerM, Tuning.LurkLeadDeg),
			Tuning.LurkSpeedMps, Tuning.TurnRateDegPerS, DeltaSeconds);
		break;

	case EDeepMonsterState::Stalk:
	{
		// Spiral in on the bite point, a few metres up the line from the diver. Close in, swing out to one side of the
		// line and strike across it, so the diver who turns sees it in profile clamping the line.
		const FVector Point = BitePoint(Target, Tuning);
		FVector Aim = UDeepMonsterRules::SpiralAimPoint(Point, Head, Tuning.StalkArcDeg, Tuning.StalkInwardFraction, Tuning.StalkDirectWithinM);
		FVector A, B;
		if (FVector::Dist(Head, Point) <= Tuning.StalkDirectWithinM * CmPerM && Brain->TetherEnds(Target, A, B))
		{
			if (BiteSide.IsNearlyZero())
			{
				const FVector Line = (B - A).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
				FVector Out = Head - Point;
				Out -= Line * FVector::DotProduct(Out, Line);
				BiteSide = Out.GetSafeNormal(UE_SMALL_NUMBER, FVector::CrossProduct(Line, FVector::UpVector).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector));
			}
			const FVector Lead = Point + BiteSide * (Tuning.BiteSideLeadM * CmPerM);
			if (!bBiteCommit && FVector::Dist(Head, Lead) > 150.f)
			{
				Aim = Lead;
			}
			else
			{
				bBiteCommit = true;
				Aim = Point;
			}
		}
		Body->SwimToward(Aim, Tuning.StalkSpeedMps, Tuning.TurnRateDegPerS, DeltaSeconds);
		break;
	}

	case EDeepMonsterState::Pass:
	{
		// The first sight: slow, along the laid-out line, then out into the dark.
		FVector Aim = Head + Body->GetHeading() * (10.f * CmPerM);
		if (PassLeg < PassPoints.Num())
		{
			Aim = PassPoints[PassLeg];
			if (FVector::Dist(Head, Aim) < 150.f)
			{
				++PassLeg;
			}
		}
		Body->SwimToward(Aim, Tuning.PassSpeedMps, Tuning.TurnRateDegPerS, DeltaSeconds);
		break;
	}

	case EDeepMonsterState::Recoil:
	{
		if (StateSeconds < Tuning.RecoilTurnS)
		{
			// First it turns its head away from the lamp almost where it is, so the flinch happens in frame.
			FVector FromLamp = Head - Target->GetActorLocation();
			FromLamp.Z *= 0.3f;
			FromLamp = FromLamp.GetSafeNormal(UE_SMALL_NUMBER, -Body->GetHeading());
			Body->SwimToward(Head + FromLamp * (Tuning.LurkRadiusM * CmPerM), Tuning.RecoilSpeedMps * Tuning.RecoilTurnSpeedFraction,
				Tuning.TurnRateDegPerS * 2.f, DeltaSeconds);
			break;
		}
		// Then straight away from the line, flinching round faster than it normally turns.
		FVector Away = Head - Middle;
		Away.Z *= 0.3f;
		Away = Away.GetSafeNormal(UE_SMALL_NUMBER, -Body->GetHeading());
		Body->SwimToward(Head + Away * (Tuning.LurkRadiusM * CmPerM), Tuning.RecoilSpeedMps, Tuning.TurnRateDegPerS * 2.f, DeltaSeconds);
		break;
	}

	case EDeepMonsterState::Bite:
	{
		FVector A, B;
		if (Brain->TetherEnds(Target, A, B))
		{
			Body->SwimToward(FMath::Lerp(A, B, BiteAlpha), Tuning.StalkSpeedMps, Tuning.TurnRateDegPerS * 3.f, DeltaSeconds);
		}
		break;
	}

	case EDeepMonsterState::Hunt:
		Body->SwimToward(Target->GetActorLocation(), Tuning.HuntSpeedMps, Tuning.TurnRateDegPerS, DeltaSeconds);
		break;
	}
}
