// Shared run state every player sees: which stop the sub is at, the company account, hull rating, diver number,
// whether the title screen is up, and the run totals the performance review shows at the win.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Sim/DeepTuningTypes.h"
#include "Sim/DeepPilotRules.h"
#include "DeepGameState.generated.h"

UCLASS()
class DEEP_API ADeepGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ADeepGameState();

	/** 1-based stop whose arena is loaded: the one parked at, or the nearest while driving. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	int32 Stop = 1;

	/** Displayed depth of the submarine (its tether anchor). The sub never moves; the arena root moves instead. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	float SubDepthM = 100.f;

	/**
	 * The sub's sideways place in the cave shaft (m, X = ahead, Y = starboard, relative to the stops' column). Zero at
	 * every stop once parked (it glides home). The sub never moves; the arena root moves by -SubOffsetM. Sim/DeepCaveRules.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	FVector2D SubOffsetM = FVector2D::ZeroVector;

	/**
	 * The sub's heading in the stop's frame (degrees, + = turned to starboard; 0 as parked). The sub never turns; the
	 * arena root turns by -SubYawDeg about the vertical through the sub (DeepArena::PositionRoot). Glides to 0 when parked.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	float SubYawDeg = 0.f;

	/**
	 * Deep.World.Still, read once when the arena is built and fixed for the world's life (ResetRun keeps it): the arena
	 * root stays still and the hull moves instead (DeepArena::IsWorldStill). Server sets it; every machine reads it.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	bool bWorldStill = false;

	/** The arena was built and bWorldStill latched (server only). */
	bool bWorldStillLatched = false;

	/**
	 * Deep.World.Caves, latched with bWorldStill (it needs it): the baked chunk string (World/DeepCaveString) instead of
	 * the shaft, the code-built caverns and the beds (DeepArena::IsWorldCaves). Server sets it; every machine reads it.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	bool bWorldCaves = false;

	/** The helm's levers, for every machine (World/DeepHelmLevers): throttle notch (- astern, 0 stop, + ahead), ballast -1 blown .. 1 flooded. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	int32 HelmThrottleNotch = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	float HelmBallast = 0.f;

	/** The sub's speed over all axes (m/s) and its speed along the heading (m/s, astern negative), for the HUD. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	float HelmSpeedMps = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	float HelmSurgeMps = 0.f;

	/** The sub's vertical speed (m/s, + = down), for the HUD. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	float HelmHeaveMps = 0.f;

	/**
	 * D-070: the helm drives direct (held keys, Sim/DeepPilotRules::StepDrive) or by the levers (D-048). The server keeps
	 * it on WantsDirectDrive each tick (Deep.Pilot.Levers); every machine reads it (the pilot's keys, the HUD's words).
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	bool bHelmDirect = true;

	/** D-070: what the pilot holds now (the HUD's readout, the levers' prop). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	FDeepDriveInput HelmHold;

	/** Server: the mode the helm should be in (Deep.Pilot.Levers 0|1, else Drive.bDirect). */
	static bool WantsDirectDrive();

	/** The helm's mode on any machine: the server's live wish (a switch counts at once), a client's replicated copy. */
	bool IsHelmDirect() const { return HasAuthority() ? WantsDirectDrive() : bHelmDirect; }

	/** Parked, and the glide to the stop's frame is done (offset 0, yaw 0, the stop's depth): the hatch may open. Caves: at S's berth only. */
	bool IsSettledAtStop() const;

	/** The hull for the HUD: 1 whole, 0 breached (HullDamage against Cave.HullBreachDamage). */
	UFUNCTION(BlueprintPure, Category = "Deep")
	float GetHullIntegrity01() const;

	/** Parked at Stop's depth: the hatch is open. False while driving or hanging between stops. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	bool bParked = true;

	/** The deepest stop parked at this run (descend_stopN fires the first time). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	int32 DeepestStopReached = 1;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	int32 Credits = 0;

	/**
	 * Layer gates bought + 1 (D-023): 1 nothing, 2 the flashlight, 3 the hull upgrade. Prices are Economy.HullPrices.
	 * Kept as one number so the purchase rule (TryBuyHull) stays one rule.
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	int32 HullRating = 1;

	/** Gates in effect: bought, or implied by the stop the sub is at (a cheat that jumps stops implies them). */
	int32 GatesBought() const { return FMath::Max(HullRating, Stop) - 1; }
	bool HasFlashlight() const { return GatesBought() >= 1; }
	bool IsHullUpgraded() const { return GatesBought() >= 2; }
	/** The helm's limit now (Sub.DepthLimitsM by gates). */
	float DepthLimitM() const;
	/** 1-based stop parked at, or 0 while driving or between stops. */
	int32 ParkedStop() const { return bParked ? Stop : 0; }

	/** Replacement diver number; goes up each time a diver is written off. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	int32 DiverNumber = 1;

	/** Harpoon hits on the hull not yet repaired (repairs by hand come later). See Harpoon/. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	int32 HullDamage = 0;

	/** Hull hits not yet on a settlement; each becomes one repair fee line. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	int32 HullRepairsOwed = 0;

	/** The title screen is up: nobody dives, Patience does not drain, the serpent stays away. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	bool bOnTitle = false;

	/** The relic was banked: the run is over. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	bool bWon = false;

	/** Seconds played this run (not on the title, not after the win, not while paused). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	float RunSeconds = 0.f;

	/** Run totals for the performance review: gross banked, the Director's cut, every fee (settlement, re-tether, death). */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	int32 TotalGross = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	int32 TotalDirectorCut = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	int32 TotalFees = 0;

	/** Server: every run value back to the start (credits, hull, stop, diver number, totals, time, win). */
	void ResetRun();

	/** The performance review from the run totals. Divers expended = written-off divers (DiverNumber - 1). */
	FDeepRunReview BuildReview() const;

	/** Server: parked off-centre, the sub glides home to the stop's centre (Cave.ParkGlideMps); with the shaft off the offset is cleared. */
	void StepParkGlide(float DeltaSeconds);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
