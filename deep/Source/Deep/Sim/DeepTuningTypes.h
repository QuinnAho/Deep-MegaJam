// Tuning and result types for DEEP's rules. Defaults are the GDD start values.
// Units: lengths named ...M are metres, ...Cm are Unreal units (centimetres). Time in seconds.

#pragma once

#include "CoreMinimal.h"
#include "DeepTuningTypes.generated.h"

UENUM(BlueprintType)
enum class EDeepSalvageSize : uint8
{
	Small,
	Medium,
	Large,
	Pearl,
	Relic
};

/** Where a diver is in the run loop. Only Tethered and Cut can swim. */
UENUM(BlueprintType)
enum class EDeepRunState : uint8
{
	Tethered,
	/** Tether cut: bailout air counts down; reach the airlock to re-tether. */
	Cut,
	/** Bailout air ran out: a short written-off screen, then the next diver. */
	Dead,
	/** Banked at the airlock: the settlement screen is up. */
	Settling,
	/** Fade to the next dive (and the next stop, if the hull allows). */
	Descending
};

UENUM(BlueprintType)
enum class EDeepBankMood : uint8
{
	Empty,
	Poor,
	Good
};

/** A named depth band. The zone at a depth is the one with the greatest MinDepthM not deeper than it. */
USTRUCT(BlueprintType)
struct FDeepZone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Depth")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Depth")
	float MinDepthM = 0.f;
};

/** One descent stop: where the sub hangs and how dark it is. */
USTRUCT(BlueprintType)
struct FDeepStopTuning
{
	GENERATED_BODY()

	/** Depth of the submarine's tether anchor at this stop, shown on the depth counter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stop")
	float SubDepthM = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stop")
	float FogDensity = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stop")
	FLinearColor FogColor = FLinearColor(0.01f, 0.05f, 0.07f);

	/** Downwelling light from above (lux). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stop")
	float SunIntensity = 2.f;
};

USTRUCT(BlueprintType)
struct FDeepTetherTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether")
	float BaseLengthM = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether")
	float LengthPerStopM = 20.f;

	/** Strain (distance / length) at or above which the tether counts as taut. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether")
	float TautStrain = 0.97f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether")
	float WinchSpeedMps = 7.f;

	/** The winch stops reeling at this distance from the anchor (the airlock). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether")
	float WinchArriveM = 3.f;

	/** The winch pulls toward this far below the airlock (kept inside WinchArriveM), so it passes under the hull. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether")
	float WinchApproachBelowM = 2.f;

	/** A cable, not a rod. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether")
	float LineThicknessCm = 1.5f;
};

USTRUCT(BlueprintType)
struct FDeepWeightTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weight")
	float FreeKg = 10.f;

	/** Sink speed per kg carried over FreeKg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weight")
	float SinkMpsPerKg = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weight")
	float MaxSinkMps = 3.f;

	/** Swim speed lost per kg over FreeKg (0.01 = 1%). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weight")
	float SwimSlowdownPerKg = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weight")
	float MinSwimMultiplier = 0.25f;

	/** At or above this total carried mass the diver cannot swim up and needs the winch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weight")
	float OverloadKg = 15.f;
};

USTRUCT(BlueprintType)
struct FDeepSwimTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim")
	float SwimSpeedMps = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim")
	float SprintMultiplier = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim")
	float AccelerationMps2 = 8.f;

	/** Water drag when not swimming (deceleration). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim")
	float DragMps2 = 5.f;

	/** How far the diver can grab salvage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim")
	float ReachM = 3.f;

	/** Salvage must be within this angle of the view direction to be grabbed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim")
	float GrabConeDeg = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim")
	float LookSensitivity = 1.f;
};

USTRUCT(BlueprintType)
struct FDeepPatienceTuning
{
	GENERATED_BODY()

	FDeepPatienceTuning()
	{
		WarningFractions = { 0.5f, 0.25f, 0.1f };
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patience")
	float MaxSeconds = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patience")
	float DrainPerSecond = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patience")
	float HuntedMultiplier = 2.f;

	/** Refill as a fraction of MaxSeconds, by salvage size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patience")
	float RefillSmall = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patience")
	float RefillMedium = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patience")
	float RefillLarge = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patience")
	float RefillPearl = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patience")
	float RefillRelic = 0.2f;

	/** The Director warns when Patience falls to each of these fractions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patience")
	TArray<float> WarningFractions;
};

USTRUCT(BlueprintType)
struct FDeepEconomyTuning
{
	GENERATED_BODY()

	FDeepEconomyTuning()
	{
		HullPrices = { 50, 200 };
	}

	/** The Director's cut of gross, rounded up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	int32 DirectorCutPercent = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	int32 RetetherFee = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	int32 DeathFee = 30;

	/** HullPrices[i] buys hull rating i+2, which unlocks stop i+2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	TArray<int32> HullPrices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	int32 StartingCredits = 0;

	/** Settlement fees: FeePercent of gross (rounded up) plus FeeFlat, itemised over FeeLinesMin..FeeLinesMax joke lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	int32 FeePercent = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	int32 FeeFlat = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	int32 FeeLinesMin = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	int32 FeeLinesMax = 3;

	/** A bank with at least this net is a good one (the Captain's tone). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	int32 GoodBankNet = 50;
};

/** Timings of the run loop around the rules. */
USTRUCT(BlueprintType)
struct FDeepRunTuning
{
	GENERATED_BODY()

	/** Hold the cut key this long to be cut loose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	float CutHoldSeconds = 1.5f;

	/** The written-off screen before the replacement diver appears. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	float DeathScreenSeconds = 3.f;

	/** The settlement screen ignores the continue key this long, so a held key cannot skip it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	float SettlementMinSeconds = 1.f;

	/** Fade out, swap stop, fade in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	float DescendSeconds = 2.5f;

	/** Minimum gap between repeats of the same chatter event (taut, overweight, winch). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	float ChatterCooldownSeconds = 12.f;
};

/** How long the Captain's subtitles stay up. */
USTRUCT(BlueprintType)
struct FDeepCaptainTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Captain")
	float LineMinSeconds = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Captain")
	float SecondsPerWord = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Captain")
	float LineMaxSeconds = 7.f;

	/** A line waiting behind a more urgent one is dropped if it waited longer than this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Captain")
	float PendingMaxAgeSeconds = 6.f;
};

USTRUCT(BlueprintType)
struct FDeepBailoutTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bailout")
	float AirSeconds = 30.f;

	/** The last seconds of air drain slowly so near misses happen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bailout")
	float SlowWindowSeconds = 5.f;

	/** Air drain rate inside the slow window, relative to real time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bailout")
	float SlowDrainScale = 0.5f;

	/** The Captain's "ten seconds" warning fires when air falls to this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bailout")
	float WarnSeconds = 10.f;
};

USTRUCT(BlueprintType)
struct FDeepSalvageKind
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salvage")
	EDeepSalvageSize Size = EDeepSalvageSize::Small;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salvage")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salvage")
	float Kg = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salvage")
	int32 Value = 20;

	/** Stand-in mesh size (edge length). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salvage")
	float SizeM = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Salvage")
	FLinearColor Color = FLinearColor(0.6f, 0.4f, 0.2f);
};

/** Where one salvage piece sits, relative to the sub's tether anchor. */
USTRUCT(BlueprintType)
struct FDeepSalvageSpot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	EDeepSalvageSize Size = EDeepSalvageSize::Small;

	/** Compass angle around the anchor, 0 = +X. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float AngleDeg = 0.f;

	/** Horizontal distance out from the anchor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float OutM = 10.f;

	/** Vertical distance below the anchor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float BelowM = 10.f;
};

USTRUCT(BlueprintType)
struct FDeepArenaTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	FVector SubLocationM = FVector::ZeroVector;

	/** Tether anchor (airlock) below the sub's centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float AnchorBelowSubM = 2.5f;

	/** The diver starts this far below the anchor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float DiverStartBelowAnchorM = 4.f;

	/** The diver starts this far behind the anchor (along the sub's -X), so the sub and salvage are both in view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float DiverStartBackM = 8.f;

	/** Starting look pitch (negative looks down toward the salvage). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float DiverStartPitchDeg = -15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float FloorBelowSubM = 120.f;

	/** Displayed depth of the relic (stop 3). Not spawned yet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float RelicDepthM = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float FloorSizeM = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	TArray<FDeepSalvageSpot> SalvageSpots;

	/** Small glint light on each salvage piece. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float SalvageGlintIntensity = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float SalvageGlintRadiusM = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float SubLightIntensity = 20000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float SubLightRadiusM = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float ExposureMinEV = -3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float ExposureMaxEV = 2.f;

	/**
	 * Grey box (Quinn: "no environment yet, just raw gameplay"): no fog shafts, fixed exposure, even key + fill light.
	 * The stop's fog and sun settings are ignored; the stop's fog colour only tints the light a little.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Grey Box")
	bool bGreyBox = true;

	/** Key light from above (lux). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Grey Box")
	float GreyBoxKeyLux = 3.f;

	/** Fill light from below and the other side (lux), so no face is black. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Grey Box")
	float GreyBoxFillLux = 1.2f;

	/** Fixed manual exposure compensation (EV). 0 = EV100 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Grey Box")
	float GreyBoxExposureEV = 0.f;

	/** 0..1: how far the light leans toward the stop's water colour. Keep it mild. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Grey Box")
	float GreyBoxTintStrength = 0.15f;

	/** A near-zero fog that only colours the empty background, so dark shapes stand out against it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Grey Box")
	float GreyBoxBackdropFogDensity = 0.0004f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Grey Box")
	FLinearColor GreyBoxBackdropColor = FLinearColor(0.05f, 0.09f, 0.12f);
};

USTRUCT(BlueprintType)
struct FDeepLampTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float Intensity = 40000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float InnerConeDeg = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float OuterConeDeg = 24.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float RangeM = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	bool bStartsOn = true;
};

// ---- Rule inputs and results ----

USTRUCT(BlueprintType)
struct FDeepCargoItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo")
	EDeepSalvageSize Size = EDeepSalvageSize::Small;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo")
	float Kg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo")
	int32 Value = 0;
};

USTRUCT(BlueprintType)
struct FDeepTetherSolve
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tether")
	float LengthM = 0.f;

	/** Distance from anchor to the wanted position (before clamping). */
	UPROPERTY(BlueprintReadOnly, Category = "Tether")
	float DistanceM = 0.f;

	/** Distance / length, clamped to [0, 1]. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether")
	float Strain = 0.f;

	/** Metres of line left before the end. Never negative. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether")
	float LeftM = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Tether")
	bool bTaut = false;

	/** True when the wanted position was past the end and got pulled back. */
	UPROPERTY(BlueprintReadOnly, Category = "Tether")
	bool bClamped = false;

	UPROPERTY(BlueprintReadOnly, Category = "Tether")
	FVector ClampedPositionCm = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FDeepWeightResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Weight")
	float CarriedKg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Weight")
	float ExcessKg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Weight")
	float SinkSpeedMps = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Weight")
	float SwimMultiplier = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Weight")
	bool bOverloaded = false;
};

USTRUCT(BlueprintType)
struct FDeepSettlement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 Gross = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 DirectorCut = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 Fees = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 Net = 0;
};

USTRUCT(BlueprintType)
struct FDeepHullPurchase
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 Credits = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 HullRating = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 Spent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	bool bBought = false;
};

/** Everything the settlement screen shows. Built by UDeepRunComponent::Bank from UDeepRules results. */
USTRUCT(BlueprintType)
struct FDeepSettlementReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 Gross = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 DirectorCut = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	TArray<FString> FeeLabels;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	TArray<int32> FeeAmounts;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 Fees = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 Net = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	EDeepBankMood Mood = EDeepBankMood::Empty;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	bool bHullBought = false;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 HullSpent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 HullRating = 1;

	/** Price of the hull rating after this one, -1 when maxed. */
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 NextHullPrice = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 CreditsAfter = 0;

	/** The stop the continue key leads to. */
	UPROPERTY(BlueprintReadOnly, Category = "Economy")
	int32 NextStop = 1;
};
