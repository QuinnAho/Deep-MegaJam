// Shared run state every player sees: which stop the sub is at, the company account, hull rating, diver number,
// whether the title screen is up, and the run totals the performance review shows at the win.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Sim/DeepTuningTypes.h"
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

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
