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
	const FDeepMonsterStep Result = UDeepMonsterRules::NextState(In, Tuning);

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
		Body->SwimToward(UDeepMonsterRules::SpiralAimPoint(Middle, Head, Tuning.StalkArcDeg, Tuning.StalkInwardFraction, Tuning.StalkDirectWithinM),
			Tuning.StalkSpeedMps, Tuning.TurnRateDegPerS, DeltaSeconds);
		break;

	case EDeepMonsterState::Recoil:
	{
		// Straight away from the line, flinching round faster than it normally turns.
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
