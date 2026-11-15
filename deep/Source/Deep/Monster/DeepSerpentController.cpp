#include "Monster/DeepSerpentController.h"

#include "Components/SpotLightComponent.h"
#include "Diver/DeepDiver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "EngineUtils.h"
#include "Game/DeepGameState.h"
#include "Monster/DeepMonsterBrain.h"
#include "Monster/DeepMonsterRules.h"
#include "Monster/DeepMonsterSettings.h"
#include "Monster/DeepSerpent.h"
#include "World/DeepCavern.h"
#include "World/DeepArena.h"
#include "World/DeepArenaRoot.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISense_Hearing.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepSerpent, Log, All);

namespace
{
	using DeepMonster::CmPerM;

	TAutoConsoleVariable<int32> CVarWell(TEXT("Deep.Monster.Well"), -1,
		TEXT("The serpent's steering in the relic's shaft: -1 as the settings say (bWellSteering), 0 the old open-water steering (before frames), 1 on."));
	TAutoConsoleVariable<int32> CVarWellLog(TEXT("Deep.Monster.WellLog"), 0,
		TEXT("1: every 0.5 s the serpent logs DEEPWELL {json}: state, head and body distance from the shaft's axis, depth below the diver."));
	TAutoConsoleVariable<int32> CVarWatch(TEXT("Deep.Monster.Watch"), 0,
		TEXT("Shots: 1 turns the player's view (and lamp) onto the serpent's head every step; 2 looks straight down; 3 looks down, and at the head while it stalks or bites (the scripted player); 0 off."));
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
	TauntsLeft = UDeepMonsterSettings::Get().Behaviour.TauntCount;
	TauntsDone = 0;
	TauntLeg = 0;
}

void ADeepSerpentController::HandlePerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>() && Stimulus.WasSuccessfullySensed())
	{
		PendingNoise += FMath::Max(0.f, Stimulus.Strength);
		HeardTotal += FMath::Max(0.f, Stimulus.Strength);
		if (Stimulus.Strength > HeardLoudest)
		{
			HeardLoudest = Stimulus.Strength;
			HeardAt = Stimulus.StimulusLocation;
		}
	}
}

FQuat ADeepSerpentController::RootRotation() const
{
	const ADeepArenaRoot* Root = DeepArena::FindArenaRoot(GetWorld());
	return Root ? Root->GetActorQuat() : FQuat::Identity;
}

bool ADeepSerpentController::BeginTaunt(const ADeepDiver* Diver, const FDeepMonsterTuning& Tuning)
{
	const ADeepSerpent* Body = GetSerpent();
	if (!Body || !Diver)
	{
		return false;
	}
	FVector Eye;
	FRotator View;
	Diver->GetActorEyesViewPoint(Eye, View);
	// It comes in on the side it is already on, so the first beat is a swing out to the edge of the light, not a teleport.
	const FVector Diver0 = Diver->GetActorLocation();
	const FVector F = View.Vector();
	const FVector R = FVector::CrossProduct(FVector::UpVector, FVector(F.X, F.Y, 0.0)).GetSafeNormal();
	const float Side = FVector::DotProduct(Body->GetHeadLocation() - Diver0, R) >= 0.0 ? 1.f : -1.f;
	TArray<FVector> Points;
	UDeepMonsterRules::TauntPath(Diver0, BitePoint(Diver, Tuning), F, Side, Tuning, Points);
	const FQuat Inv = RootRotation().Inverse();
	TauntOffsets.Reset();
	for (const FVector& P : Points)
	{
		TauntOffsets.Add(Inv.RotateVector(P - Diver0));
	}
	TauntDiver = Diver;
	TauntLeg = 0;
	return true;
}

FVector ADeepSerpentController::TauntPoint(int32 Index) const
{
	const ADeepDiver* Diver = TauntDiver.Get();
	if (!Diver || !TauntOffsets.IsValidIndex(Index))
	{
		return GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;
	}
	return Diver->GetActorLocation() + RootRotation().RotateVector(TauntOffsets[Index]);
}

void ADeepSerpentController::Call(uint8 Kind, const TCHAR* Why, const FDeepMonsterTuning& Tuning)
{
	ADeepSerpent* Body = GetSerpent();
	if (!Body)
	{
		return;
	}
	Body->Call(Kind);
	SinceCallS = 0.f;
	const ADeepDiver* Diver = Focus.Get();
	const float DistM = Diver ? static_cast<float>(FVector::Dist(Body->GetHeadLocation(), Diver->GetActorLocation())) / CmPerM : -1.f;
	const float Gain = DistM >= 0.f ? UDeepMonsterRules::VoiceGain(DistM, Tuning.VoiceInnerM, Tuning.CallFalloffM) * Tuning.CallVolume : 0.f;
	UE_LOG(LogDeepSerpent, Log, TEXT("DEEPMONSTER_CALL {\"why\":\"%s\",\"kind\":%d,\"state\":\"%s\",\"distance_m\":%.1f,\"volume\":%.2f}"),
		Why, Kind, *UDeepMonsterRules::StateName(State), DistM, Gain);
}

void ADeepSerpentController::LogTauntBeat(const TCHAR* Beat, const FDeepMonsterTuning& Tuning) const
{
	const ADeepSerpent* Body = GetSerpent();
	const ADeepDiver* Diver = Focus.Get();
	const float DistM = Body && Diver ? static_cast<float>(FVector::Dist(Body->GetHeadLocation(), Diver->GetActorLocation())) / CmPerM : -1.f;
	const float Drone = DistM >= 0.f ? UDeepMonsterRules::VoiceGain(DistM, Tuning.VoiceInnerM, Tuning.DroneFalloffM)
		* FMath::Lerp(Tuning.DroneVolumeCalm, Tuning.DroneVolumeNear, Threat01) : 0.f;
	UE_LOG(LogDeepSerpent, Log, TEXT("DEEPMONSTER_TAUNT {\"beat\":\"%s\",\"taunt\":%d,\"left\":%d,\"distance_m\":%.1f,\"tether_m\":%.1f,\"drone_volume\":%.2f}"),
		Beat, TauntsDone, TauntsLeft, DistM, TetherDistanceM, Drone);
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
	CaveStopKept = 0;
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
		if (Lamp && UDeepMonsterRules::IsInBeam(Lamp->GetComponentLocation(), Lamp->GetForwardVector(), Tuning.BeamHalfAngleDeg,
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
		// Driven off, it comes back to watch before it bites again.
		TauntsLeft = FMath::Max(TauntsLeft, Tuning.TauntsAfterRecoil);
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
	SinceCallS += Dt;
	HeardTurnLeftS = FMath::Max(0.f, HeardTurnLeftS - Dt);
	const float Heard = HeardLoudest;
	HeardLoudest = 0.f;
	if (Heard >= Tuning.AnswerNoise)
	{
		// It turns toward what it heard (while it circles), and answers if it has not called for a while.
		TurnToward = HeardAt;
		HeardTurnLeftS = Tuning.HeardTurnS;
		if (UDeepMonsterRules::ShouldAnswer(Heard, SinceCallS, Tuning))
		{
			Call(ADeepSerpent::CallAnswer, TEXT("answer"), Tuning);
		}
	}

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
	// Taunts are staged for a player's eyes, like the first sight: test divers and cheat spawns keep the old rules.
	const bool bCanTaunt = Target && Target->IsPlayerControlled() && !Brain->IsForcedSpawn();
	In.TauntsLeft = bCanTaunt ? TauntsLeft : 0;
	if (State == EDeepMonsterState::Taunt)
	{
		In.bTauntComplete = TauntLeg >= TauntOffsets.Num();
	}
	FDeepMonsterStep Result = UDeepMonsterRules::NextState(In, Tuning);
	if (Result.bTauntStarted)
	{
		if (BeginTaunt(Target, Tuning))
		{
			TauntsLeft = FMath::Max(0, TauntsLeft - 1);
			++TauntsDone;
		}
		else
		{
			TauntsLeft = 0;
			Result = FDeepMonsterStep();
			Result.Next = State;
		}
	}
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
	if (In.State == EDeepMonsterState::Taunt && Result.Next == EDeepMonsterState::Stalk)
	{
		// The taunt does not calm it: after the last one it strikes.
		Noise = FMath::Max(Noise, Tuning.StalkNoise);
		LogTauntBeat(TEXT("gone"), Tuning);
	}
	if (Result.bTauntStarted)
	{
		LogTauntBeat(TEXT("start"), Tuning);
	}
	if (Result.bBiteStarted)
	{
		Call(ADeepSerpent::CallStrike, TEXT("strike"), Tuning);
	}

	// Events last, after the state is consistent, so listeners that read it see the new state.
	// The first sight starts with a sound: the run's first-heard line (the run component says it once per run), so
	// by the first real stalk the Captain has already named it and says it is near instead.
	if (Result.bPassStarted)
	{
		Brain->OnStalkStarted.Broadcast(Target);
	}
	if (Result.Changed(In.State) && Result.Next == EDeepMonsterState::Stalk && In.State != EDeepMonsterState::Taunt)
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

	UpdateWell(Target, Tuning, Dt);
	Steer(Tuning, Dt);
	DebugWell(Target, Dt);
	Threat01 =Target ? UDeepMonsterRules::Threat01(State, DiverDistanceM, Tuning) : 0.f;
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

	// Each state picks where it steers for, how fast and how sharply it turns; in the well the place is moved into the
	// shaft (UDeepMonsterRules::WellAimPoint) and the speed and turn rate stay the state's own.
	FVector Aim = Head + Body->GetHeading() * (10.f * CmPerM);
	float Speed = 0.f;
	float Turn = Tuning.TurnRateDegPerS;
	switch (State)
	{
	case EDeepMonsterState::Lurk:
		Aim = UDeepMonsterRules::OrbitAimPoint(Middle, Head, LurkRadiusCm(Target, Tuning) / CmPerM, Tuning.LurkLeadDeg);
		Speed = Tuning.LurkSpeedMps;
		if (HeardTurnLeftS > 0.f && !TurnToward.IsZero())
		{
			// It heard something: it turns and swims toward it for a moment.
			Aim = TurnToward;
			Turn = Tuning.TurnRateDegPerS * 1.5f;
		}
		break;

	case EDeepMonsterState::Taunt:
	{
		// Edge of the light, brush past the line, back into the dark. Each beat logs once; the brush calls.
		if (TauntLeg < TauntOffsets.Num())
		{
			Aim = TauntPoint(TauntLeg);
			if (FVector::Dist(Head, Aim) < 250.f)
			{
				static const TCHAR* Beats[] = { TEXT("edge"), TEXT("brush"), TEXT("withdraw") };
				LogTauntBeat(Beats[FMath::Clamp(TauntLeg, 0, 2)], Tuning);
				if (TauntLeg == 1)
				{
					Call(ADeepSerpent::CallTaunt, TEXT("taunt"), Tuning);
				}
				++TauntLeg;
			}
		}
		Speed = Tuning.TauntSpeedMps;
		Turn = Tuning.TurnRateDegPerS * 1.5f;
		break;
	}

	case EDeepMonsterState::Stalk:
	{
		// Spiral in on the bite point, a few metres up the line from the diver. Close in, swing out to one side of the
		// line and strike across it, so the diver who turns sees it in profile clamping the line.
		const FVector Point = BitePoint(Target, Tuning);
		Aim = UDeepMonsterRules::SpiralAimPoint(Point, Head, Tuning.StalkArcDeg, Tuning.StalkInwardFraction, Tuning.StalkDirectWithinM);
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
		Speed = Tuning.StalkSpeedMps;
		break;
	}

	case EDeepMonsterState::Pass:
		// The first sight: slow, along the laid-out line, then out into the dark.
		if (PassLeg < PassPoints.Num())
		{
			Aim = PassPoints[PassLeg];
			if (FVector::Dist(Head, Aim) < 150.f)
			{
				++PassLeg;
			}
		}
		Speed = Tuning.PassSpeedMps;
		break;

	case EDeepMonsterState::Recoil:
	{
		if (StateSeconds < Tuning.RecoilTurnS)
		{
			// First it turns its head away from the lamp almost where it is, so the flinch happens in frame.
			FVector FromLamp = Head - Target->GetActorLocation();
			FromLamp.Z *= 0.3f;
			FromLamp = FromLamp.GetSafeNormal(UE_SMALL_NUMBER, -Body->GetHeading());
			Aim = Head + FromLamp * (Tuning.LurkRadiusM * CmPerM);
			Speed = Tuning.RecoilSpeedMps * Tuning.RecoilTurnSpeedFraction;
		}
		else
		{
			// Then straight away from the line, flinching round faster than it normally turns.
			FVector Away = Head - Middle;
			Away.Z *= 0.3f;
			Away = Away.GetSafeNormal(UE_SMALL_NUMBER, -Body->GetHeading());
			Aim = Head + Away * (Tuning.LurkRadiusM * CmPerM);
			Speed = Tuning.RecoilSpeedMps;
		}
		Turn = Tuning.TurnRateDegPerS * 2.f;
		break;
	}

	case EDeepMonsterState::Bite:
	{
		FVector A, B;
		if (!Brain->TetherEnds(Target, A, B))
		{
			return;
		}
		Aim = FMath::Lerp(A, B, BiteAlpha);
		Speed = Tuning.StalkSpeedMps;
		Turn = Tuning.TurnRateDegPerS * 3.f;
		break;
	}

	case EDeepMonsterState::Hunt:
		Aim = Target->GetActorLocation();
		Speed = Tuning.HuntSpeedMps;
		break;
	}

	if (bInWell)
	{
		FVector DiverEnd = Target->GetActorLocation();
		FVector A, B;
		if (Brain->TetherEnds(Target, A, B))
		{
			DiverEnd = B;
		}
		Aim = UDeepMonsterRules::WellAimPoint(State, Well, Head, DiverEnd, Aim, WellSide, RiseMps, Tuning);
	}

	// The cavern: whatever the state, the aim and the path stay in the cave's clear space, the drawn body CaveWallClearM
	// off the rock (a lurk circle wider than the cave follows the wall round). Not in the well: it is below the bed and
	// holds the path itself. The shape is the cavern's (ADeepCavern::KeepClearCm); no rock shows, nothing changes.
	if (!Cavern.IsValid())
	{
		Cavern = ADeepCavern::FindIn(GetWorld());
	}
	const ADeepCavern* Cave = bInWell ? nullptr : Cavern.Get();
	const int32 CaveStop = Cave ? Cave->ShownStop() : 0;
	const float CaveClearM = Tuning.CaveWallClearM + Tuning.CaveBodyReachM;
	if (CaveStop > 0)
	{
		Aim = Cave->KeepClearCm(Aim, CaveClearM);
	}
	Body->SwimToward(Aim, Speed, Turn, DeltaSeconds);
	if (CaveStop > 0)
	{
		// The body keeps this rule too (its chain particles, every substep), so it is held weakly: the body stores it.
		const TWeakObjectPtr<const ADeepCavern> WeakCave(Cave);
		Body->KeepPath([WeakCave, CaveClearM](const FVector& Point)
		{
			const ADeepCavern* Kept = WeakCave.Get();
			return Kept ? Kept->KeepClearCm(Point, CaveClearM) : Point;
		}, CaveStop != CaveStopKept);
	}
	else
	{
		Body->ClearBodyKeep();
	}
	CaveStopKept = CaveStop;
	if (bInWell)
	{
		// The path itself stays in the shaft: a fast turn (10 m/s recoil, a hunt) swings wider than any aim, and the
		// DEEPWELL log showed the head 6 m off the axis with a third of the body in the stone. The head slides along the
		// wall instead; the body follows its path.
		const FVector Path = Body->GetActorLocation();
		const FVector Kept = UDeepMonsterRules::KeepInWell(Well, Path, Tuning, Tuning.WellPathWallClearM);
		if (!Kept.Equals(Path, 1.0))
		{
			Body->SetActorLocation(Kept);
		}
	}
}

void ADeepSerpentController::UpdateWell(const ADeepDiver* Target, const FDeepMonsterTuning& Tuning, float DeltaSeconds)
{
	const UDeepMonsterBrain* Brain = GetBrain();
	ADeepSerpent* Body = GetSerpent();
	const int32 Switch = CVarWell.GetValueOnGameThread();
	const bool bOn = Switch < 0 ? Tuning.bWellSteering : Switch > 0;
	Well = Brain ? Brain->GetWell() : FDeepWell();
	if (!Target || !Body || !Well.bValid)
	{
		bInWell = false;
		bHaveLastDiverZ = false;
		return;
	}
	// The rise from the diver's own movement: the winch moves the diver without a velocity.
	const FVector Diver = Target->GetActorLocation();
	if (bHaveLastDiverZ && DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		const float Measured = static_cast<float>((Diver.Z - LastDiverZ) / CmPerM) / DeltaSeconds;
		const float Alpha = FMath::Clamp(DeltaSeconds * 2.f, 0.f, 1.f);
		RiseMps = FMath::Lerp(RiseMps, FMath::Clamp(Measured, -20.f, 20.f), Alpha);
	}
	LastDiverZ = Diver.Z;
	bHaveLastDiverZ = true;

	if (!bOn || !UDeepMonsterRules::IsInWell(Well, Diver))
	{
		bInWell = false;
		EnterWaitS = 0.f;
		return;
	}
	if (bInWell)
	{
		return;
	}
	// Entry: only once there is a below to come up from.
	if (Diver.Z - Well.FloorZCm < Tuning.WellEnterAboveFloorM * CmPerM)
	{
		return;
	}
	const FVector Head = Body->GetHeadLocation();
	if (UDeepMonsterRules::IsInWell(Well, Head, Tuning.WellWallClearM * CmPerM * 0.5))
	{
		bInWell = true;
		return;
	}
	// It is out in the rock round the shaft, where nobody can see it: it comes into the shaft below the diver, head up,
	// body hanging down the shaft, out of view like the first sight's start. While the diver's beam is on that spot it
	// waits (WellEnterWaitS at most), so it is not seen to appear.
	FVector2D Side(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f));
	WellSide = Side.IsNearlyZero() ? FVector2D(1.0, 0.0) : Side.GetSafeNormal();
	const FVector Below(Well.AxisCm.X - WellSide.X * 100.0, Well.AxisCm.Y - WellSide.Y * 100.0,
		Diver.Z - UDeepMonsterRules::WellHoldBelowM(RiseMps, Tuning) * CmPerM);
	const FVector Hold = UDeepMonsterRules::KeepInWell(Well, Below, Tuning);
	const USpotLightComponent* Lamp = Target->IsLampOn() ? Target->FindComponentByClass<USpotLightComponent>() : nullptr;
	const bool bSeen = Lamp && UDeepMonsterRules::IsInBeam(Lamp->GetComponentLocation(), Lamp->GetForwardVector(), Tuning.BeamHalfAngleDeg,
		Lamp->AttenuationRadius / CmPerM, Hold, UDeepMonsterSettings::Get().Body.HeadRadiusM + Tuning.BeamLeniencyM);
	EnterWaitS += DeltaSeconds;
	if (bSeen && EnterWaitS < Tuning.WellEnterWaitS)
	{
		return;
	}
	bInWell = true;
	Body->PlaceHead(Hold, FVector::UpVector);
	UE_LOG(LogDeepSerpent, Log, TEXT("Serpent enters the well %.1f m below the diver (%s, rising %.1f m/s, %s)"), (Diver.Z - Hold.Z) / CmPerM,
		*UDeepMonsterRules::StateName(State), RiseMps, bSeen ? TEXT("in the beam after waiting") : TEXT("out of the beam"));
}

void ADeepSerpentController::DebugWell(const ADeepDiver* Target, float DeltaSeconds)
{
	ADeepSerpent* Body = GetSerpent();
	if (!Body || !Target)
	{
		return;
	}
	const FVector Head = Body->GetHeadLocation();
	const int32 Watch = CVarWatch.GetValueOnGameThread();
	if (Watch > 0)
	{
		if (APlayerController* Player = Cast<APlayerController>(Target->GetController()))
		{
			const FVector Eye = Target->GetPawnViewLocation();
			const bool bAtHead = Watch == 1 || (Watch == 3 && (State == EDeepMonsterState::Stalk || State == EDeepMonsterState::Bite || State == EDeepMonsterState::Hunt));
			const FRotator Current = Player->GetControlRotation();
			const FRotator Look = bAtHead ? (Head - Eye).Rotation() : FRotator(-89.f, Current.Yaw, 0.f);
			Player->SetControlRotation(Look);
		}
	}
	WellClockS += DeltaSeconds;
	if (CVarWellLog.GetValueOnGameThread() <= 0)
	{
		return;
	}
	WellLogAccumS += DeltaSeconds;
	if (WellLogAccumS < 0.5f)
	{
		return;
	}
	WellLogAccumS = 0.f;
	const FDeepWell Shaft = GetBrain() ? GetBrain()->GetWell() : FDeepWell();
	if (!Shaft.bValid)
	{
		return;
	}
	// How far the drawn body reaches from the axis: past the radius (and below the rim), it is in the stone.
	TArray<FSphere> Spheres;
	Body->GetDrawnSpheres(Spheres);
	float BodyOutM = 0.f;
	int32 InStone = 0;
	for (const FSphere& S : Spheres)
	{
		const float Out = UDeepMonsterRules::WellAxisDistanceM(Shaft, S.Center) + static_cast<float>(S.W) / CmPerM;
		BodyOutM = FMath::Max(BodyOutM, Out);
		InStone += (Out > Shaft.RadiusCm / CmPerM && S.Center.Z < Shaft.TopZCm) ? 1 : 0;
	}
	const FVector Diver = Target->GetActorLocation();
	const FVector Eye = Target->GetPawnViewLocation();
	const FVector ViewDir = Target->GetControlRotation().Vector();
	const FVector ToHead = Head - Eye;
	const float OffViewDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(ToHead.GetSafeNormal(), ViewDir)), -1.f, 1.f)));
	UE_LOG(LogDeepSerpent, Display,
		TEXT("DEEPWELL {\"t\":%.1f,\"well_mode\":%d,\"state\":\"%s\",\"head_axis_m\":%.1f,\"head_below_diver_m\":%.1f,\"head_dist_m\":%.1f,\"head_off_view_deg\":%.0f,\"body_out_m\":%.1f,\"spheres_in_stone\":%d,\"spheres\":%d,\"diver_above_floor_m\":%.1f,\"diver_axis_m\":%.1f,\"rise_mps\":%.1f,\"tether_m\":%.1f,\"lit_s\":%.2f,\"radius_m\":%.1f,\"top_above_floor_m\":%.1f}"),
		WellClockS, bInWell ? 1 : 0, *UDeepMonsterRules::StateName(State), UDeepMonsterRules::WellAxisDistanceM(Shaft, Head), (Diver.Z - Head.Z) / CmPerM,
		ToHead.Size() / CmPerM, OffViewDeg, BodyOutM, InStone, Spheres.Num(), (Diver.Z - Shaft.FloorZCm) / CmPerM,
		UDeepMonsterRules::WellAxisDistanceM(Shaft, Diver), RiseMps, TetherDistanceM, LitSeconds, Shaft.RadiusCm / CmPerM, (Shaft.TopZCm - Shaft.FloorZCm) / CmPerM);
}
