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

	/** 1-based descent stop. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	int32 Stop = 1;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	int32 Credits = 0;

	/** Hull rating N lets the sub descend to stop N. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	int32 HullRating = 1;

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
