// The serpent's mind: an AAIController (Unreal game AI) that possesses ADeepSerpent on the server.
// It hears divers through a UAIPerceptionComponent with the engine hearing sense, tests the lamp beams against
// the head, feeds the pure UDeepMonsterRules::NextState, and steers the pawn itself (open water: no navmesh,
// no MoveTo, no Behavior Tree asset). Events go out through the delegates on UDeepMonsterBrain.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Monster/DeepMonsterTypes.h"
#include "Perception/AIPerceptionTypes.h"
#include "DeepSerpentController.generated.h"

class ADeepDiver;
class ADeepSerpent;
class UAISenseConfig_Hearing;
class UDeepMonsterBrain;

UCLASS()
class DEEP_API ADeepSerpentController : public AAIController
{
	GENERATED_BODY()

public:
	ADeepSerpentController();

	/** One decision step: lamp, distances, NextState, events, steering. Tick calls this on the server; tests call it directly. */
	void StepMonster(float DeltaSeconds);

	/** Noise straight into its ears, bypassing perception (cheats and tests). */
	void AddNoise(float Amount) { PendingNoise += FMath::Max(0.f, Amount); }
	/** Pretends the beam is on its head for this many seconds. */
	void SetDebugLit(float Seconds) { DebugLitS = FMath::Max(0.f, Seconds); }
	/** Jumps to a state with fresh timers (no events fire). */
	void ForceState(EDeepMonsterState NewState);

	/**
	 * The first sight now (cheat, tests): fires the first sound, lays out the pass across the focus diver's lamp and
	 * enters Pass. Counts as this run's first sight. False if there is no diver.
	 */
	bool StartFirstSight();
	bool IsFirstSightDone() const { return bFirstSightDone; }
	/** Where on this diver's tether it means to bite (shots and tests). */
	FVector GetBitePoint(const ADeepDiver* Diver) const;
	/** Closest the head came to the focus diver during the last pass (m); 0 before any pass. */
	float GetPassClosestM() const { return PassClosestM; }

	EDeepMonsterState GetState() const { return State; }
	float GetNoise() const { return Noise; }
	float GetLitSeconds() const { return LitSeconds; }
	float GetCooldownSeconds() const { return CooldownS; }
	float GetTetherDistanceM() const { return TetherDistanceM; }
	float GetDiverDistanceM() const { return DiverDistanceM; }
	float GetThreat01() const { return Threat01; }
	/** Noise units heard through the perception system so far (for tests and debugging). */
	float GetHeardTotal() const { return HeardTotal; }
	ADeepDiver* GetFocusDiver() const { return Focus.Get(); }
	ADeepSerpent* GetSerpent() const;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;

	/** Perception callback: a hearing stimulus adds its strength (noise units) to the noise level. */
	UFUNCTION()
	void HandlePerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

private:
	UDeepMonsterBrain* GetBrain() const;
	void GatherDivers(TArray<ADeepDiver*>& Out) const;
	ADeepDiver* PickFocus(const TArray<ADeepDiver*>& Divers) const;
	/** Lurk/stalk centre: the middle of the diver's tether, or the diver when untethered. */
	FVector TetherMiddle(const ADeepDiver* Diver) const;
	float LurkRadiusCm(const ADeepDiver* Diver, const FDeepMonsterTuning& Tuning) const;
	bool IsHeadLit(const TArray<ADeepDiver*>& Divers, const FDeepMonsterTuning& Tuning) const;
	void EnterState(EDeepMonsterState NewState, const FDeepMonsterTuning& Tuning);
	/** Lays out the first-sight pass across this diver's lamp and puts the head at its start (out of view). */
	bool BeginPass(const ADeepDiver* Diver, const FDeepMonsterTuning& Tuning);
	/** Where on the tether it means to bite: BiteFromDiverM up the line from the diver. */
	FVector BitePoint(const ADeepDiver* Diver, const FDeepMonsterTuning& Tuning) const;
	/** The first sight has not happened, the settings allow it, the brain spawned it itself, and a player is there to see it. */
	bool IsFirstSightPending(const ADeepDiver* Diver, const FDeepMonsterTuning& Tuning) const;
	void Steer(const FDeepMonsterTuning& Tuning, float DeltaSeconds);

	TWeakObjectPtr<ADeepDiver> Focus;

	EDeepMonsterState State = EDeepMonsterState::Lurk;
	float Noise = 0.f;
	float PendingNoise = 0.f;
	float HeardTotal = 0.f;
	float LitSeconds = 0.f;
	float StateSeconds = 0.f;
	float CooldownS = 0.f;
	float DebugLitS = 0.f;
	float TetherDistanceM = 0.f;
	float DiverDistanceM = 0.f;
	float Threat01 = 0.f;
	/** Where on the tether (0 anchor .. 1 diver) the bite holds. */
	float BiteAlpha = 0.5f;

	// ---- The first sight ----
	bool bFirstSightDone = false;
	TArray<FVector> PassPoints;
	int32 PassLeg = 0;
	float PassClosestM = 0.f;

	// ---- The staged bite: the side it strikes from, fixed once it is close ----
	FVector BiteSide = FVector::ZeroVector;
	bool bBiteCommit = false;
};
