// DEEP's rules as pure static functions. No world, no actors, no globals: every input is a parameter.
// Actors gather inputs, call one of these, and apply the result. Each rule has an automation test in
// Private/Tests/DeepRulesTests.cpp.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Sim/DeepTuningTypes.h"
#include "DeepRules.generated.h"

UCLASS()
class DEEP_API UDeepRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static constexpr float CmPerM = 100.f;

	// ---- Depth ----

	/** Depth in metres of a world Z, given the sub anchor's world Z and its depth at this stop. Never negative. */
	UFUNCTION(BlueprintPure, Category = "Deep|Depth")
	static float DepthM(float WorldZCm, float AnchorZCm, float AnchorDepthM);

	/** Index of the zone with the greatest MinDepthM not deeper than InDepthM. Shallowest zone if none; -1 if no zones. */
	UFUNCTION(BlueprintPure, Category = "Deep|Depth")
	static int32 ZoneIndex(float InDepthM, const TArray<FDeepZone>& Zones);

	UFUNCTION(BlueprintPure, Category = "Deep|Depth")
	static FString ZoneName(float InDepthM, const TArray<FDeepZone>& Zones);

	// ---- Tether and winch ----

	/** Tether length at a 1-based stop. Stops below 1 count as 1. */
	UFUNCTION(BlueprintPure, Category = "Deep|Tether")
	static float TetherLengthM(int32 Stop, const FDeepTetherTuning& Tuning);

	/** Lamp intensity scale for a facing surface DistM ahead on the beam's axis (negative = nothing ahead): (d / NearFullM)^2 clamped to [NearMinScale, 1]. */
	static float LampNearScale(float DistM, const FDeepLampTuning& Tuning);

	/** The engine's spot cone falloff at CosAngle off the axis: 1 inside the inner cone, 0 outside the outer, a squared ramp in cosine between. */
	static float LampConeFalloff(float CosAngle, const FDeepLampTuning& Tuning);

	/**
	 * How bright the lamp lights a point, relative to a facing surface at NearFullM on the axis: cone falloff times facing
	 * (cosine of incidence, clamped to [0, 1]) times (NearFullM / d)^2. Negative distance = nothing there (0).
	 */
	static float LampSampleBrightness(float DistM, float CosOffAxis, float CosIncidence, const FDeepLampTuning& Tuning);

	/** The intensity scale that brings the brightest sample in the cone back to 1: 1 / Brightest, clamped to [NearMinScale, 1]. */
	static float LampScaleForBrightest(float Brightest, const FDeepLampTuning& Tuning);

	/** Where the diver may be if it wants to be at WantedCm on a line of LengthM from AnchorCm. */
	UFUNCTION(BlueprintPure, Category = "Deep|Tether")
	static FDeepTetherSolve SolveTether(const FVector& AnchorCm, const FVector& WantedCm, float LengthM, const FDeepTetherTuning& Tuning);

	/** The winch's speed: RelicWinchSpeedMps while the relic is aboard, else WinchSpeedMps. */
	UFUNCTION(BlueprintPure, Category = "Deep|Tether")
	static float WinchSpeedMps(bool bCarryingRelic, const FDeepTetherTuning& Tuning);

	/** One winch step: moves toward the anchor at WinchSpeedMps, stopping WinchArriveM from it. Never moves away. */
	UFUNCTION(BlueprintPure, Category = "Deep|Tether")
	static FVector WinchStep(const FVector& DiverCm, const FVector& AnchorCm, float DeltaSeconds, const FDeepTetherTuning& Tuning);

	/**
	 * The winch the game uses: reels toward a point WinchApproachBelowM under the airlock (inside the home radius,
	 * below the hull) and stops there. Does nothing once home.
	 */
	UFUNCTION(BlueprintPure, Category = "Deep|Tether")
	static FVector WinchStepToAirlock(const FVector& DiverCm, const FVector& AnchorCm, float DeltaSeconds, const FDeepTetherTuning& Tuning);

	/**
	 * Where the winch heads next, all in the sub's local cm. BlockedCm is the hull's collision box grown by the diver's
	 * capsule (so a centre outside it does not touch the hull). Straight to Via when that line is clear; else, over the
	 * hull: sideways out past its edge; beside it (or caught in it): straight down below it. Each leg stays outside
	 * BlockedCm, so the winch goes round and under the hull and never through it.
	 */
	UFUNCTION(BlueprintPure, Category = "Deep|Tether")
	static FVector WinchRouteTarget(const FVector& FromCm, const FVector& ViaCm, const FBox& BlockedCm, float ClearCm);

	/** WinchStepToAirlock routed by WinchRouteTarget round the hull (sub-local cm). Does nothing once home. */
	UFUNCTION(BlueprintPure, Category = "Deep|Tether")
	static FVector WinchStepAroundHull(const FVector& DiverCm, const FVector& AnchorCm, const FBox& BlockedCm, float DeltaSeconds, const FDeepTetherTuning& Tuning);

	/**
	 * A swimmer touching the side of the hull (sub-local cm; HullCm = the hull's collision box): the part of the
	 * velocity that pushes into the side turns downward, so the diver slides down the side and under the hull instead
	 * of pinning on it. Unchanged when not beside the hull or not pushing into it. Never faster than the input.
	 */
	UFUNCTION(BlueprintPure, Category = "Deep|Swim")
	static FVector HullSlideVelocity(const FVector& LocalCm, const FVector& Velocity, const FBox& HullCm, float RadiusCm, float HalfHeightCm, float ContactCm);

	/** True when the diver is within WinchArriveM of the anchor (home). */
	UFUNCTION(BlueprintPure, Category = "Deep|Tether")
	static bool IsHome(const FVector& DiverCm, const FVector& AnchorCm, const FDeepTetherTuning& Tuning);

	// ---- Weight and swimming ----

	UFUNCTION(BlueprintPure, Category = "Deep|Weight")
	static float CargoKg(const TArray<FDeepCargoItem>& Cargo);

	UFUNCTION(BlueprintPure, Category = "Deep|Weight")
	static int32 CargoValue(const TArray<FDeepCargoItem>& Cargo);

	UFUNCTION(BlueprintPure, Category = "Deep|Weight")
	static FDeepWeightResult SolveWeight(float CarriedKg, const FDeepWeightTuning& Tuning);

	/** Swim velocity in m/s from a wish direction (length clamped to 1). Overloaded divers cannot rise; excess weight sinks. */
	UFUNCTION(BlueprintPure, Category = "Deep|Weight")
	static FVector SwimVelocityMps(const FVector& WishDirection, bool bSprint, const FDeepWeightResult& Weight, const FDeepSwimTuning& Tuning);

	// ---- Patience ----

	UFUNCTION(BlueprintPure, Category = "Deep|Patience")
	static float PatienceStep(float PatienceSeconds, float DeltaSeconds, bool bHunted, const FDeepPatienceTuning& Tuning);

	/** Seconds of Patience a piece of this size refills. */
	UFUNCTION(BlueprintPure, Category = "Deep|Patience")
	static float PatienceRefillSeconds(EDeepSalvageSize Size, const FDeepPatienceTuning& Tuning);

	UFUNCTION(BlueprintPure, Category = "Deep|Patience")
	static float PatienceAfterSalvage(float PatienceSeconds, EDeepSalvageSize Size, const FDeepPatienceTuning& Tuning);

	/** Index into WarningFractions of the most severe warning crossed going from Before to After; -1 if none. */
	UFUNCTION(BlueprintPure, Category = "Deep|Patience")
	static int32 PatienceWarningCrossed(float BeforeSeconds, float AfterSeconds, const FDeepPatienceTuning& Tuning);

	// ---- Settlement and economy ----

	/** The Director's cut: Percent of Gross, rounded up, integer maths. Zero for non-positive gross. */
	UFUNCTION(BlueprintPure, Category = "Deep|Economy")
	static int32 DirectorCut(int32 Gross, int32 Percent);

	/** Gross minus the cut minus fees, never below zero. Negative fees count as zero. */
	UFUNCTION(BlueprintPure, Category = "Deep|Economy")
	static FDeepSettlement Settle(int32 Gross, const TArray<int32>& Fees, const FDeepEconomyTuning& Tuning);

	/** Credits after paying a fee. Never below zero. */
	UFUNCTION(BlueprintPure, Category = "Deep|Economy")
	static int32 ApplyFee(int32 Credits, int32 Fee);

	/** Price of the next hull rating, or -1 when maxed. Hull rating N lets the sub reach stop N. */
	UFUNCTION(BlueprintPure, Category = "Deep|Economy")
	static int32 NextHullPrice(int32 HullRating, const FDeepEconomyTuning& Tuning);

	UFUNCTION(BlueprintPure, Category = "Deep|Economy")
	static int32 MaxHullRating(const FDeepEconomyTuning& Tuning);

	/** Buys at most one hull rating if affordable (one per settlement, so descent is one stop at a time). */
	UFUNCTION(BlueprintPure, Category = "Deep|Economy")
	static FDeepHullPurchase TryBuyHull(int32 Credits, int32 HullRating, const FDeepEconomyTuning& Tuning);

	/** Total settlement fees for a haul: FeePercent of gross rounded up, plus FeeFlat (charged even on nothing). */
	UFUNCTION(BlueprintPure, Category = "Deep|Economy")
	static int32 SettlementFeeTotal(int32 Gross, const FDeepEconomyTuning& Tuning);

	/** Splits Total into Lines non-negative amounts that sum to Total (each at least 1 when Total allows). Seeded. */
	UFUNCTION(BlueprintPure, Category = "Deep|Economy")
	static TArray<int32> SplitFee(int32 Total, int32 Lines, int32 Seed);

	/** Empty when nothing was banked, Good when net reaches GoodBankNet, else Poor. */
	UFUNCTION(BlueprintPure, Category = "Deep|Economy")
	static EDeepBankMood BankMood(int32 Gross, int32 Net, const FDeepEconomyTuning& Tuning);

	// ---- Bailout ----

	/** Bailout air left after RealDeltaSeconds. Inside the slow window air drains at SlowDrainScale. Never negative. */
	UFUNCTION(BlueprintPure, Category = "Deep|Bailout")
	static float BailoutStep(float AirSeconds, float RealDeltaSeconds, const FDeepBailoutTuning& Tuning);

	// ---- Grabbing ----

	/**
	 * Index of the candidate to grab: the nearest (by distance to its surface, RadiiM) that is within ReachM
	 * and within GrabConeDeg of ViewDirection. -1 if none. RadiiM may be shorter than CandidatesCm (missing = 0).
	 */
	UFUNCTION(BlueprintPure, Category = "Deep|Salvage")
	static int32 PickGrabTarget(const FVector& EyeCm, const FVector& ViewDirection, const TArray<FVector>& CandidatesCm, const TArray<float>& RadiiM, const FDeepSwimTuning& Tuning);

	// ---- Arena ----

	/** World offset (cm) of a salvage spot from the sub's tether anchor. */
	UFUNCTION(BlueprintPure, Category = "Deep|Arena")
	static FVector SalvageSpotOffsetCm(const FDeepSalvageSpot& Spot);

	/**
	 * World offset (cm) of the relic from the anchor at a stop whose anchor sits at StopDepthM: RelicDepthM deep if the
	 * tether allows, else raised until it is RelicReachMarginM inside TetherLengthM.
	 */
	UFUNCTION(BlueprintPure, Category = "Deep|Arena")
	static FVector RelicOffsetCm(const FDeepArenaTuning& Arena, float StopDepthM, float TetherLengthM);

	/** relic_seen: within RelicSeenNearM, or within RelicSeenViewM and RelicSeenConeDeg of the view direction. */
	UFUNCTION(BlueprintPure, Category = "Deep|Arena")
	static bool IsRelicSeen(const FVector& EyeCm, const FVector& ViewDirection, const FVector& RelicCm, const FDeepArenaTuning& Arena);

	// ---- The submarine: the helm and the layer gates (D-023) ----

	/** Deepest the helm may go: DepthLimitsM[GatesBought] (0 none, 1 flashlight, 2 hull upgrade), clamped to the table. */
	UFUNCTION(BlueprintPure, Category = "Deep|Sub")
	static float HelmDepthLimitM(int32 GatesBought, const FDeepSubTuning& Tuning);

	/**
	 * Drives the sub: Input > 0 descends, < 0 ascends, at DriveSpeedMps. Never below LimitM, never above TopM.
	 * bAtLimit / bAtTop when the control pushed against either end.
	 */
	UFUNCTION(BlueprintPure, Category = "Deep|Sub")
	static FDeepHelmStep HelmStep(float DepthM, float Input, float DeltaSeconds, float TopM, float LimitM, const FDeepSubTuning& Tuning);

	/** 1-based stop within SnapM of DepthM (the nearest such), or 0 when none is that close. */
	UFUNCTION(BlueprintPure, Category = "Deep|Sub")
	static int32 HelmParkStop(float DepthM, const TArray<float>& StopDepthsM, float SnapM);

	/** 1-based stop nearest DepthM (ties go to the shallower). The arena of that stop is the one shown while driving. */
	UFUNCTION(BlueprintPure, Category = "Deep|Sub")
	static int32 NearestStop(float DepthM, const TArray<float>& StopDepthsM);

	/** A point (sub-relative cm) is inside the hatch's square opening (any height). */
	UFUNCTION(BlueprintPure, Category = "Deep|Sub")
	static bool InHatchColumn(const FVector& LocalCm, const FDeepSubTuning& Tuning);

	// ---- The win ----

	/** The Captain's one word for a run (see FDeepReviewTuning). Empty if no words are tuned. */
	UFUNCTION(BlueprintPure, Category = "Deep|Review")
	static FString ReviewRating(int32 DiversExpended, float Seconds, const FDeepReviewTuning& Tuning);

	/** The performance review: totals as given, Kept = Gross - Cut - Fees (never below zero), and the rating. */
	UFUNCTION(BlueprintPure, Category = "Deep|Review")
	static FDeepRunReview BuildReview(int32 DiversExpended, int32 Gross, int32 DirectorCut, int32 Fees, float Seconds, const FDeepReviewTuning& Tuning);
};
