// The sea serpent's world-level hub (server). Spawns the serpent (a Pawn possessed by ADeepSerpentController,
// which makes the decisions), watches the divers and reports their noise into Unreal's hearing sense, keeps
// track of whose tether is cut, and owns the delegates other systems bind to. It does not cut tethers or kill
// divers itself (see Monster/README.md for the wires).

#pragma once

#include "CoreMinimal.h"
#include "Monster/DeepMonsterTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeepMonsterBrain.generated.h"

class ADeepDiver;
class ADeepSerpent;
class ADeepSerpentController;

DECLARE_MULTICAST_DELEGATE_TwoParams(FDeepMonsterBiteStarted, ADeepDiver* /*Diver*/, float /*AlarmSeconds*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FDeepMonsterDiverEvent, ADeepDiver* /*Diver*/);

UCLASS()
class DEEP_API UDeepMonsterBrain : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// ---- Events (server) ----

	/** It has hold of the diver's tether. The line goes in AlarmSeconds unless the beam hits it. */
	FDeepMonsterBiteStarted OnBiteStarted;
	/** The beam hit it during a bite: it let go. */
	FDeepMonsterDiverEvent OnBiteBroken;
	/** The alarm ran out: cut this diver's tether. Fires once per bite. */
	FDeepMonsterDiverEvent OnTetherSevered;
	/** It touched an untethered diver: kill them. Fires once, then it recoils. */
	FDeepMonsterDiverEvent OnDiverCaught;

	// ---- Reads (forwarded from the controller; safe with no serpent) ----

	/** 0 far away (or no serpent), 1 on top of the nearest diver. For sound and HUD. */
	float GetThreat01() const;
	EDeepMonsterState GetState() const;
	float GetNoise() const;
	float GetTetherDistanceM() const;
	float GetDiverDistanceM() const;
	ADeepSerpent* GetSerpent() const { return Serpent.Get(); }
	ADeepSerpentController* GetSerpentController() const;

	/** `{json}` with present, state, noise, heard, lit_s, cooldown_s, tether_distance_m, diver_distance_m, threat, stop. */
	FString BuildStateJson() const;

	// ---- Tethers ----

	/** The diver's tether is gone (cut, bailout). Marked automatically when the serpent severs one. */
	void NotifyTetherCut(ADeepDiver* Diver);
	/** The diver has a tether again (re-tether or respawn). */
	void NotifyRetethered(ADeepDiver* Diver);
	bool IsTethered(const ADeepDiver* Diver) const;
	/** Anchor and diver ends of a live tether; false if the diver is untethered. */
	bool TetherEnds(const ADeepDiver* Diver, FVector& OutAnchor, FVector& OutDiverEnd) const;
	/** Lurk circle radius around the middle of this diver's tether: no point of the line is closer than LurkRadiusM. */
	float LurkRadiusCm(const ADeepDiver* Diver, const FDeepMonsterTuning& Tuning) const;

	// ---- Cheats and tests ----

	/**
	 * Spawns the serpent in the dark at the lurk radius (or returns the one there is). Forced spawns (cheats,
	 * tests) stay at any stop; the brain's own spawn at FirstStop and deeper is removed again above it.
	 */
	ADeepSerpent* SpawnSerpent(bool bForced = true);
	/** Noise straight into the serpent's ears (bypasses perception). */
	void AddNoise(float Amount);
	/** Pretends the beam is on its head for this many seconds. */
	void SetDebugLit(float Seconds);
	/** Jumps to a state with fresh timers (no events fire). */
	void ForceState(EDeepMonsterState NewState);
	/** Spawn bookkeeping and diver noise reporting. Tick calls this; tests call it directly. */
	void Step(float DeltaSeconds);

	// ---- Subsystem ----

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	int32 CurrentStop() const;
	void GatherDivers(TArray<ADeepDiver*>& Out) const;
	/** Watches each diver (winch, sprint speed, cargo rise) and reports what it hears as engine noise events. */
	void ReportDiverNoise(const TArray<ADeepDiver*>& Divers, float DeltaSeconds);

	TWeakObjectPtr<ADeepSerpent> Serpent;
	TSet<TWeakObjectPtr<const ADeepDiver>> CutDivers;
	TMap<TWeakObjectPtr<const ADeepDiver>, float> LastCargoKg;
	/** Noise made but not yet reported (events go out in whole units so the engine hears them at full range). */
	TMap<TWeakObjectPtr<const ADeepDiver>, float> NoiseOwed;
	/** Spawned by cheat or test: stays even below FirstStop. */
	bool bForcedSpawn = false;
};
