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
	/** Banked at the console: the settlement screen is up. */
	Settling,
	/** Inside the submarine: walking on the deck, at the console or seated at the helm. No Patience, no serpent. */
	Aboard,
	/** The relic is banked: the run is over and the performance review is up. */
	Won
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

	/**
	 * Winch speed while the diver carries the relic (60 kg). Slower than WinchSpeedMps so the last ascent is long
	 * enough for the serpent to reach the line: at 7 m/s the winch outran it and a player who never used the lamp won.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether")
	float RelicWinchSpeedMps = 4.f;

	/** The winch stops reeling at this distance from the anchor (the airlock). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether")
	float WinchArriveM = 3.f;

	/** The winch pulls toward this far below the airlock (kept inside WinchArriveM), so it passes under the hull. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether")
	float WinchApproachBelowM = 2.f;

	/** The winch keeps the diver's capsule this far clear of the hull's collision while it routes round and under it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether")
	float WinchHullClearM = 0.3f;

	/** A diver's umbilical: thick enough to read in the lamp at 20 m, still a rope, not a rod. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tether")
	float LineThicknessCm = 3.f;
};

/** How the tether and the harpoon line look as ropes (visual only: no rule reads these). */
USTRUCT(BlueprintType)
struct FDeepRopeTuning
{
	GENERATED_BODY()

	/** Points along the tether. More = smoother sag; one mesh segment per link. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	int32 TetherPoints = 36;

	/** Points along the harpoon line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	int32 HarpoonPoints = 16;

	/** Slack shown when the diver is inside the tether length: this share of the straight distance... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	float SlackShare = 0.06f;

	/** ...but never more than this (m), and never more than the line left on the reel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	float SlackMaxM = 3.f;

	/** How fast the shown length follows (1/s): the line pays out and takes up slack smoothly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	float LengthFollowPerS = 2.5f;

	/** A wet line is a little heavier than water (cm/s2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	float SinkCmS2 = 120.f;

	/** A cut length falls faster: nothing holds it up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	float LooseSinkCmS2 = 260.f;

	/** Water drag (1/s): high, so the line lags behind the diver and settles without swinging. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	float DragPerS = 2.2f;

	/** Recoil speed of a cut end (cm/s), fading over WhipPoints points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	float WhipCmS = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	int32 WhipPoints = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	int32 SolverIterations = 24;

	/** A pin that jumps further than this in one frame (a teleport) restarts the rope straight (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	float SnapResetM = 8.f;

	/** A serpent head within this distance of the line when it parts means a bite: it parts there (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	float BiteNearM = 6.f;

	/** Bending stiffness per solver pass (0..1): folds round over several links instead of kinking at one joint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	float BendStiffness = 0.08f;

	/**
	 * The tether: old, dirty tan-yellow line (linear). Saturation 32%, under the style guide's 35% cap; dark, because
	 * the lamp is 40,000 cd at arm's length and the first, brighter yellow (0.20, 0.16, 0.07) washed out cream-white.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	FLinearColor TetherColor = FLinearColor(0.085f, 0.075f, 0.058f);

	/** The harpoon line: a dull grey cord. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope")
	FLinearColor HarpoonColor = FLinearColor(0.07f, 0.072f, 0.066f);
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

	/** Scale on the mouse look action. One mouse count turns 0.07 (axis config) x 2.5 (legacy scale) x this degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim")
	float LookSensitivity = 1.f;

	/**
	 * A swimmer within this far of the hull's side who pushes into it slides down it and under the hull (toward the
	 * hatch) instead of being stopped by it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swim")
	float HullContactM = 0.15f;
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

	/** The first dive of a run shows a large controls card this long (or until the first grab). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	float ControlsCardSeconds = 15.f;

	/** The win screen ignores keys this long, so a held winch or grab key cannot skip the review. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Run")
	float ReviewMinSeconds = 1.5f;
};

/** The Captain's one-word performance review at the win. */
USTRUCT(BlueprintType)
struct FDeepReviewTuning
{
	GENERATED_BODY()

	FDeepReviewTuning()
	{
		MaxDiversExpended = { 0, 2, 5 };
		Words = { TEXT("ADEQUATE"), TEXT("TOLERABLE"), TEXT("WASTEFUL"), TEXT("CATASTROPHIC") };
	}

	/** Words[i] is earned with at most MaxDiversExpended[i] divers expended; past the last, the last word. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Review")
	TArray<int32> MaxDiversExpended;

	/** Best first. The Captain never says better than adequate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Review")
	TArray<FString> Words;

	/** A run longer than this drops one word. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Review")
	float SlowRunSeconds = 900.f;
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

	/**
	 * Look only: dropping through the hatch, the view is pitched at least this far down (0 = left alone). Level with the
	 * eye after the drop at 500 m there is only black water; at -45 the far lip of the well (about 7 m out, 6 m down)
	 * and the relic's light in it sit in the middle of the frame. Ascent pass, Nov 15.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float HatchDropPitchDeg = -45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float FloorBelowSubM = 120.f;

	/** Wanted displayed depth of the relic. It is raised as far as needed to stay inside the tether at RelicStop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Relic")
	float RelicDepthM = 600.f;

	/** The relic lies at this stop and deeper. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Relic")
	int32 RelicStop = 3;

	/**
	 * Compass angle of the relic around the anchor (0 = +X, ahead of the diver's start). 330, not 0: the relic lies in a
	 * sinkhole in the sea bed, and at 0 it was 2 m from the 0-degree scrap 66 m above it (no bed can hold both).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Relic")
	float RelicAngleDeg = 330.f;

	/**
	 * Horizontal distance of the relic from the anchor. 0: the sinkhole is a shaft straight under the hatch, so the
	 * winch line runs up the open shaft. At 8 m the straight line from the relic to the anchor ran through the shaft's
	 * wall: the winch dragged the diver into the rock, and a diver cut there could never swim out.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Relic")
	float RelicOutM = 0.f;

	/** The relic stays this much inside the tether's reach, so a tethered diver can grab and winch it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Relic")
	float RelicReachMarginM = 3.f;

	/**
	 * The relic's own light, a warm glow over it. Small and contained by the shaft (D-037): at 12000 over 25 m it lit the
	 * whole pit yellow-green and burned the frame; the beacon below does the job of being seen from the hatch.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Relic")
	float RelicLightIntensity = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Relic")
	float RelicLightRadiusM = 7.f;

	/** Amber (style guide 3: salvage glints may be amber). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Relic")
	FLinearColor RelicLightColor = FLinearColor(1.f, 0.62f, 0.27f);

	/**
	 * Once taken, the relic's glow goes with the diver (the relic actor is gone): a smaller light of the same colour at the
	 * harness, so the winch up the black shaft keeps the warm light the climax was built around. Fraction of
	 * RelicLightIntensity, and its reach. Look only (DeepLamp::UpdateCarriedGlow).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Relic")
	float RelicCarriedLightFraction = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Relic")
	float RelicCarriedLightRadiusM = 5.f;

	/** How strongly the carried glow lights the fog round the diver (a warm halo in the water). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Relic")
	float RelicCarriedLightScattering = 1.5f;

	/**
	 * The beacon: a narrow spot from the relic straight up the shaft, scattering in the volumetric fog, so a column of
	 * light rises out of the well's mouth and the player finds it with the lamp off. Shadowed, so the shaft holds it.
	 * Recipe: Epic, "Volumetric Fog": a light adds to the fog by its Volumetric Scattering Intensity; shadowed spots cost
	 * about three times unshadowed ones; fog is only computed within the fog's view distance (40 m at 500 m).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Relic")
	float RelicBeaconIntensity = 200000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Relic")
	float RelicBeaconConeDeg = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Relic")
	float RelicBeaconRangeM = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Relic")
	float RelicBeaconScattering = 12.f;

	/** The relic's shape: a pack mesh (cooked through PackAssets.csv), worn with the dressing skin. Falls back to basic shapes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Relic")
	FSoftObjectPath RelicMesh = FSoftObjectPath(TEXT("/Game/DragonCave/Meshes/SM_StoneworksSphere01.SM_StoneworksSphere01"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Relic")
	FSoftObjectPath RelicMaterial = FSoftObjectPath(TEXT("/Game/Deep/Dressing/M_DressSilt.M_DressSilt"));

	/** The instance the relic's skin takes its textures from (the Stoneworks mesh carries the bare master). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Relic")
	FSoftObjectPath RelicTextures = FSoftObjectPath(TEXT("/Game/DragonCave/Materials/MI_Sphere01.MI_Sphere01"));

	/** relic_seen: the diver is this close, or ... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Relic")
	float RelicSeenNearM = 20.f;

	/** ... looking within RelicSeenConeDeg of it from at most this far. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Relic")
	float RelicSeenViewM = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Relic")
	float RelicSeenConeDeg = 25.f;

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
	float SubLightRadiusM = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float ExposureMinEV = -3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float ExposureMaxEV = 2.f;

	/**
	 * Grey box (Quinn: "no environment yet, just raw gameplay"): no fog shafts, fixed exposure, even key + fill light.
	 * The stop's fog and sun settings are ignored; the stop's fog colour only tints the light a little.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Grey Box")
	bool bGreyBox = false;

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
	float InnerConeDeg = 7.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float OuterConeDeg = 21.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float RangeM = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	bool bStartsOn = true;

	/** Style guide 3: cool neutral white #F2F4EE, never yellow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	FLinearColor Color = FLinearColor(FColor(0xF2, 0xF4, 0xEE));

	/** How strongly the beam lights the volumetric fog (the visible cone in the murk). 0 = no beam. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float VolumetricScattering = 1.2f;

	/** The lamp's shadows cut the fog too (the beam is blocked by what it hits). About 3x a plain light's fog cost. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	bool bVolumetricShadow = true;

	/**
	 * Offset from the eye: a torch held low and to the right, so the beam's upper edge crosses the view as a directed cone
	 * and the shadows it throws fall visibly away from the eye (at the eye, the beam is a disc).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float OffsetRightCm = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float OffsetUpCm = -30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float OffsetForwardCm = 20.f;

	/** A slow, small intensity wobble (fraction) and its rate. No flicker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float WobbleFraction = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float WobbleHz = 0.3f;

	/**
	 * Close-range dimming: inverse-square makes a surface 3 m away 136x brighter than at 35 m, so the lamp dims when
	 * anything in its cone is nearer than NearFullM: intensity times 1 / (the brightest sample in the cone), a sample
	 * being the spot falloff at its angle times its facing times (NearFullM / d)^2 (UDeepRules::LampSampleBrightness).
	 * The brightest lit point stays at about its NearFullM brightness. Reach is unchanged: in open water it runs at full.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float NearFullM = 7.f;

	/**
	 * The floor of the dimming. (d / NearFullM)^2 reaches it at d = NearFullM * sqrt(NearMinScale): 0.002 holds the
	 * matched brightness to 0.31 m from the lamp (0.06 stopped at 1.7 m, so the pit floor at 1 m burnt three times over).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float NearMinScale = 0.002f;

	/** How fast the lamp comes back up (1/s) when the near surface leaves the cone, so a sweep past an edge does not pop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float NearFollowRate = 8.f;

	/** How fast it dims (1/s) when a surface comes into the cone: fast, or a turn toward a wall flashes white first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float NearDimRate = 40.f;

	/**
	 * How much light the serpent's near-black skin sends back, relative to the silt the dimming is matched on. Its spheres
	 * hold the body loosely (a little nearer than the skin), and at 1 (first frames) the head at 4 m went nearly black;
	 * at full lamp the head at 4 m read well and only the body at 2 m burnt (Deep.Lamp.Near 1 frames).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float NearSerpentReflectance = 0.4f;
};

/** The sea at one depth (style guide 5). ADeepEnvironment blends between presets by the sub's depth. */
USTRUCT(BlueprintType)
struct FDeepLookPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float DepthM = 100.f;

	/** Light from above (lux). 0 hides it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float DownlightLux = 2.f;

	/** How much the downlight lights the fog: the shafts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float DownlightVolumetric = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float FogDensity = 0.02f;

	/** Far fog colour (style guide 3, the water at this depth). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	FLinearColor FogColor = FLinearColor(FColor(0x19, 0x3C, 0x45));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float ExtinctionScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float VolumetricDistanceM = 80.f;

	/** Manual exposure compensation (EV). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float ExposureBias = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float Bloom = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float Vignette = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float Grain = 0.1f;

	/** Saturation R, G, B, All: red goes first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	FVector4 Saturation = FVector4(0.75, 0.95, 1.0, 0.9);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float Contrast = 1.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float Fringe = 0.f;

	/** Salvage glint as a fraction of its full intensity: where something is, not what it is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float GlintScale = 1.f;

	/** The sub's outside lights (airlock light, belly flood) as a fraction of full: the sub goes dark as it goes deep. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float SubLightScale = 1.f;

	/** How much the sub's outside lights light the fog. 0 = they light only what they touch (black water at 500 m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float SubLightFog = 1.f;
};

/** The underwater look (style guide 5): presets by depth plus what every depth shares. */
USTRUCT(BlueprintType)
struct FDeepLookTuning
{
	GENERATED_BODY()

	/** Shallowest first. Blended continuously by the sub's depth as it drives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	TArray<FDeepLookPreset> Presets;

	/** Downwelling light colour (#A1D1ED) and direction: nearly straight down, a little slanted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	FLinearColor DownlightColor = FLinearColor(FColor(0xA1, 0xD1, 0xED));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	FRotator DownlightRotation = FRotator(-70.f, 35.f, 0.f);

	/** Volumetric fog albedo, all depths (#40A6BF). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	FColor FogAlbedo = FColor(0x40, 0xA6, 0xBF);

	/**
	 * Volumetric fog phase (Henyey-Greenstein g). Sea water scatters strongly forward: the fog glows when looking toward a
	 * light (shafts from above) and barely glows back at a viewer holding the lamp, so lit surfaces and specks carry the beam.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float FogScatteringDistribution = 0.5f;

	/** Light function on the downlight: the moving pattern of the surface far above, cut into the fog as shafts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	FSoftObjectPath ShaftLightFunction = FSoftObjectPath(TEXT("/Game/Deep/Look/M_SurfaceShafts.M_SurfaceShafts"));

	/** Light-function scale (cm per pattern unit): 100 = the material works in metres across the light. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float ShaftScaleCm = 100.f;

	/** Volumetric fog starts this far from the eye, or the helmet lamp lights a blob on the visor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float VolumetricStartM = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float FogHeightFalloff = 0.001f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float MotionBlur = 0.2f;

	/** Local exposure off: both contrast scales 1.0 so the dark stays dark. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float LocalExposureContrast = 1.f;

	/** Marine snow: specks hung around the view that only local lights reveal. 0 turns it off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Snow")
	int32 SnowCount = 9000;

	/** Edge of the box of specks kept around the view (wrapped). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Snow")
	float SnowBoxM = 12.f;

	/** Speck size range (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Snow")
	float SnowMinSizeM = 0.002f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Snow")
	float SnowMaxSizeM = 0.0045f;

	/** Slow drift (m/s): a slight sink plus a random wander per speck. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Snow")
	float SnowSinkMps = 0.03f;

	/** No specks closer than this to the eye: up close a speck reads as the cube it is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Snow")
	float SnowClearM = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Snow")
	float SnowWanderMps = 0.04f;

	/** Pale, dull grey: lit only where a light reaches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Snow")
	FLinearColor SnowColor = FLinearColor(0.12f, 0.13f, 0.12f);

	/**
	 * The sub's belly flood: what shows near the sub at 300 and 500 m when the lamp looks away. The guide's 100,000 lit a
	 * 60 m column brighter than the lamp and broke the lamp gate (300 m unreadable without it), so it is a low pool here,
	 * lighting objects only: its fog glow turned 500 m green under the +2 EV exposure.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Sub")
	float BellyFloodIntensity = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Sub")
	float BellyFloodConeDeg = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Sub")
	float BellyFloodRangeM = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Sub")
	float BellyFloodVolumetric = 0.f;

	/**
	 * The sea bed: one surface of silt generated in code under the sub, through the resting point of every salvage spot
	 * (and the relic's), so salvage lies on it. Material by path: swap in a scanned sand or silt material instance here
	 * (it gets UV0 in tiles of SeabedUvTileM metres). Falls back to a dark tint.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Seabed")
	FSoftObjectPath SeabedMaterial = FSoftObjectPath(TEXT("/Game/Deep/Look/M_Silt.M_Silt"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Seabed")
	float SeabedUvTileM = 2.f;

	/** The bed's base shape below the anchor: BaseBelowM under the sub, falling SlopeNear per metre out to SlopeBreakM, then SlopeFar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Seabed")
	float SeabedBaseBelowM = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Seabed")
	float SeabedSlopeNear = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Seabed")
	float SeabedSlopeBreakM = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Seabed")
	float SeabedSlopeFar = 0.25f;

	/** How far (m) a resting point pulls the bed to its own height before the base shape takes over. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Seabed")
	float SeabedRestRadiusM = 6.f;

	/**
	 * The relic's sinkhole: radius (m) of the level floor at its bottom; the shaft is about this wide all the way up and
	 * gone by 1.4x. 4.5, not 2.5: a 3.5 m shaft was too narrow to swim up (a cut diver drifting a few metres off its
	 * axis ended up under the bed). Not more: the cut falls off as exp(-(d/R)^4), and at 6 it lowered the bed under the
	 * salvage spot 8 m out by 2.7 m (Deep.World.Seabed failed); at 4.5 by about 3 mm.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Seabed")
	float SeabedPitRadiusM = 4.5f;

	/**
	 * The sinkhole's cut is exp(-(d / R)^SeabedPitPower). At 4 (until Nov 15) it was a funnel: 2 m from the relic the bed
	 * was already 2.6 m above the relic's floor, so a diver at the relic had the slope in his face (the white disc in the
	 * scripted relic grab). At 8 the bottom is flat to about 2.5 m out (10 cm up at 2 m), the wall stands at about the
	 * same radius (the cut is the same at d = R) and is steeper, and it is gone sooner past R (nothing at the salvage
	 * spot 8 m out). The shaft's dressing finds the wall on this function, so it follows.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Seabed")
	float SeabedPitPower = 8.f;

	/** Gentle rolling (m of height), kept off the resting points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Seabed")
	float SeabedRollM = 0.8f;

	/** Half the bed's width (m): far past where fog ends at any depth, so its edge is never seen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Seabed")
	float SeabedHalfWidthM = 260.f;

	/** Grid cells across; cells are about 0.5 m near the sub and grow toward the edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Seabed")
	int32 SeabedCells = 240;

	/** Salvage sinks into the silt by this fraction of its size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Seabed")
	float SeabedSinkFraction = 0.12f;

	/** The one water surface: the near-mirror pool in the hatch (style guide 4). Falls back to a black tint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Sub")
	FSoftObjectPath HatchWaterMaterial = FSoftObjectPath(TEXT("/Game/Deep/Look/M_HatchWater.M_HatchWater"));

	/** How far below the deck's top the pool lies (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Sub")
	float HatchWaterBelowDeckCm = 6.f;

	/**
	 * Room and hull surfaces (style guide 4). Each works on any mesh: worked out from position, no UVs, so a pack mesh can
	 * take the same material or replace it by path. Missing assets fall back to the old tints.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Room")
	FSoftObjectPath SteelMaterial = FSoftObjectPath(TEXT("/Game/Deep/Room/M_WornSteel.M_WornSteel"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Room")
	FSoftObjectPath ScreenMaterial = FSoftObjectPath(TEXT("/Game/Deep/Room/M_Screen.M_Screen"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Room")
	FSoftObjectPath GlassMaterial = FSoftObjectPath(TEXT("/Game/Deep/Room/M_Glass.M_Glass"));

	/** Exposure inside the room at every depth: the room keeps its own dim look while the sea outside darkens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Room")
	float RoomExposureBias = 0.f;

	/** The old ceiling light, kept as a soft fill at this fraction of Sub.RoomLightIntensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Room")
	float RoomFillScale = 0.2f;

	/** Cool ceiling strips (#BFE3EA), kept low (rect lights along the ceiling, aimed down). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Room")
	FLinearColor StripColor = FLinearColor(FColor(0xBF, 0xE3, 0xEA));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Room")
	float StripIntensity = 6000.f; // 5000 over 5 m until the strips were lengthened to 6 m (opening pass): the same per metre

	/** A third strip across the nose, this far aft of the window wall, so the helm end and the window frame read (opening pass, Nov 15). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Room")
	float NoseStripBackCm = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Room")
	float NoseStripIntensity = 3500.f;

	/** The strips' barn doors (88 = none). 45 left the walls black; wider lets the strips wash down the walls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Room")
	float StripBarnDoorDeg = 75.f;

	/** The console screen's glow, as a fraction, while the settlement card is open over it (its graph ran through the numbers). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Room")
	float ScreenGlowUnderCard = 0.2f;

	/** Amber practicals at the console and the helm (#FFB45A): the only warm room light, from the screens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Room")
	FLinearColor PracticalColor = FLinearColor(FColor(0xFF, 0xB4, 0x5A));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Room")
	float PracticalIntensity = 1600.f;

	/** Emergency red at the hatch rim (#D8271C). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Room")
	FLinearColor EmergencyColor = FLinearColor(FColor(0xD8, 0x27, 0x1C));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Room")
	float EmergencyIntensity = 700.f;

	/** Screen brightness (material Glow). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Room")
	float ScreenGlow = 1.6f;

	/** Two bow lights under the nose, aimed ahead and down: what the helm camera sees the sea move in. Scaled by depth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Sub")
	float BowLightIntensity = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Sub")
	float BowLightConeDeg = 28.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Sub")
	float BowLightRangeM = 18.f;

	/** Work lights on the hull's top rails, aimed down each side so the frame and tanks read from the water (faded with depth like the other outside lights). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Sub")
	float HullLightIntensity = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Sub")
	float HullLightConeDeg = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Sub")
	float HullLightRangeM = 5.f;

	/** Hull marker lights, red port and green starboard (style guide 3), blinking at this rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Sub")
	float MarkerIntensity = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Sub")
	float MarkerHz = 1.f;

	/** Helm sway at full drive speed (degrees of roll and pitch, Hz), scaled by speed; none when parked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Helm")
	float HelmSwayDeg = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Helm")
	float HelmSwayHz = 0.2f;

	/**
	 * The helm floods: while someone is at the helm, the two bow lights become working lights aimed into the helm
	 * camera's view at this intensity, full at every depth (the diver is aboard, so the 300 m lamp rule holds).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Helm")
	float HelmFloodIntensity = 40000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Helm")
	float HelmFloodConeDeg = 44.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Helm")
	float HelmFloodRangeM = 30.f;

	/** How much the floods light the water: a visible beam that the specks stream through. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Helm")
	float HelmFloodFog = 0.35f;

	/** The floods' pitch (down is negative); the helm camera looks down at Sub.HelmCameraPitchDeg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Helm")
	float HelmFloodPitchDeg = -30.f;

	/** The second flood aims lower, at the bed under the view, so an arriving sea floor rises into light. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Helm")
	float HelmFloodLowPitchDeg = -60.f;

	/** Seen from the helm, specks are this much larger (the hull camera sits farther from them than a diver's eye). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Helm")
	float HelmSnowSizeScale = 1.7f;

	/** Streak length = speed x this (s): a camera's shutter. 0.025 s at 8 m/s is a 20 cm streak; parked, none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Helm")
	float HelmStreakSeconds = 0.025f;

	/**
	 * Dressing (ADeepDressing): rock and ruin from owned packs, placed by rows of Content/Data/<DressingFile>. It has no
	 * collision, so it is kept out of every path a diver uses: no piece comes within DressingClearM of the straight line
	 * from the hatch to a salvage spot, or within the sinkhole's radius of the shaft under the hatch.
	 * 1.5 m, not 2: the salvage lies on the bed, so the last metres of every path run about 2 m over the silt, and at 2 m
	 * no slab or column near the well survived (500 m frames). 1.5 m still leaves a metre round a diver.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Dressing")
	FString DressingFile = TEXT("Dressing.csv");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Dressing")
	float DressingClearM = 1.5f;

	/** A piece on the bed whose top is at most this high above the silt lies against the floor (which collides) and may lie under a path: bones, coins, rubble. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Dressing")
	float DressingLowM = 0.6f;

	/** The trench wall ahead of the helm: a cliff of rock from the surface to past the last stop, seen only at the helm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Helm")
	float TrenchAheadM = 8.f;

	/** The wall runs at this angle across the view (deg), so one side is nearer: parallax across the frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Helm")
	float TrenchYawDeg = 22.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Helm")
	float TrenchWidthM = 44.f;

	/** A ledge every this many metres of depth, so the eye can count the descent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Helm")
	float TrenchLedgeEveryM = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Helm")
	float TrenchLedgeOutM = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Helm")
	float TrenchBottomM = 620.f;

	/** Rock: dark, dull, never saturated (style guide 4). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Helm")
	FLinearColor TrenchColor = FLinearColor(0.1f, 0.095f, 0.085f);

	/** The settle when the sub parks: a damped dip of the helm camera (deg of pitch, cm of drop, seconds to fade). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Helm")
	float HelmSettleDeg = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Helm")
	float HelmSettleCm = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Helm")
	float HelmSettleSeconds = 1.4f;
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

/** Everything the performance review (win screen) shows. Built by UDeepRules::BuildReview. */
USTRUCT(BlueprintType)
struct FDeepRunReview
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Review")
	int32 DiversExpended = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Review")
	int32 Gross = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Review")
	int32 DirectorCut = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Review")
	int32 Fees = 0;

	/** Gross minus the cut minus every fee, never below zero. */
	UPROPERTY(BlueprintReadOnly, Category = "Review")
	int32 Kept = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Review")
	float Seconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Review")
	FString Rating;
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

/**
 * The submarine's one room, the hatch, the console and the helm (D-022, D-023). Positions are relative to the
 * submarine actor (metres, +X = the nose). The sub never moves; driving moves the arena root the other way.
 */
USTRUCT(BlueprintType)
struct FDeepSubTuning
{
	GENERATED_BODY()

	FDeepSubTuning()
	{
		DepthLimitsM = { 200.f, 400.f, 600.f };
	}

	/** Inside of the room: half length (X), half width (Y), height. The deck's top is at -RoomHeightM / 2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	float RoomHalfLengthM = 4.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	float RoomHalfWidthM = 2.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	float RoomHeightM = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	float WallThicknessM = 0.2f;

	/** Half the side of the square hatch in the deck, centred under the sub (above the tether anchor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	float HatchHalfSizeM = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	float RoomLightIntensity = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	float RoomLightRadiusM = 9.f;

	/**
	 * Where a diver stands at the start of a run and after a respawn (X, Y on the deck), and the view pitch. The start,
	 * the enter stand, the console and the helm all sit along the +Y side, so no straight walk between them crosses the
	 * hatch (Deep.World.SubRoomLayout checks it).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	FVector2D RoomStartM = FVector2D(-3.4f, 1.5f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	float RoomStartPitchDeg = -22.f;

	/** Where a diver who comes up through the hatch is put on the deck. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	FVector2D EnterStandM = FVector2D(-2.8f, 1.3f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	FVector2D ConsoleM = FVector2D(-1.6f, 1.75f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	FVector2D HelmM = FVector2D(3.7f, 1.5f);

	/** The console or the helm can be used from this far (horizontal). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	float UseRangeM = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	float WalkSpeedMps = 3.f;

	/** A swimmer whose centre rises to this far below the deck's top, inside the hatch, comes aboard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	float EnterBelowDeckM = 0.4f;

	/** A walker whose centre falls this far below the deck's top (into the hatch) drops into the water. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	float DropBelowDeckM = 1.2f;

	/** The Captain mutters (idle_inside) after this long aboard without using anything. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	float IdleInsideSeconds = 25.f;

	/** Displayed metres of depth per second while the helm control is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Helm")
	float DriveSpeedMps = 8.f;

	/** Releasing the control within this many metres of a stop parks there. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Helm")
	float ParkSnapM = 5.f;

	/** The helm will not go below DepthLimitsM[n]: n = 0 no flashlight, 1 flashlight, 2 hull upgrade. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Helm")
	TArray<float> DepthLimitsM;

	/** The helm camera, outside under the hull (metres from the sub's centre), looking down and ahead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Helm")
	FVector HelmCameraM = FVector(5.2f, 0.f, -2.6f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Helm")
	float HelmCameraPitchDeg = -32.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Helm")
	float HelmCameraFovDeg = 95.f;
};

/** One helm step: the new depth, and whether the control pushed against the limit or the top. */
USTRUCT(BlueprintType)
struct FDeepHelmStep
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sub")
	float DepthM = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Sub")
	bool bAtLimit = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sub")
	bool bAtTop = false;
};

/** The helm's sonar (Sonar lane, Nov 15): a sweeping plan view round the sub, two slices, contacts and a proximity tone. */
USTRUCT(BlueprintType)
struct FDeepSonarTuning
{
	GENERATED_BODY()

	/** The scope's edge: returns and contacts beyond this (horizontal metres from the sub's centre) are not shown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float RangeM = 60.f;

	/** A range ring every this many metres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float RingStepM = 20.f;

	/** Beams per turn (each beam = one trace per slice). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	int32 Beams = 90;

	/** One turn of the sweep, and one ping, every this many seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float SweepPeriodS = 2.5f;

	/** The dim slice runs this far under the sub's centre, so a bend shows before the sub reaches it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float SliceBelowM = 20.f;

	/** When the bottom is nearer than the dim slice, the slice rides this far above the bottom instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float BedClearanceM = 2.f;

	/** How far down the bottom probe reaches (the BED readout). Longer than RangeM: over a shaft the bottom is far. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float BedProbeM = 150.f;

	/** A return glows this long after its beam passes, fading (a little longer than a turn, so the picture never blanks). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float PersistS = 3.2f;

	/** The farthest returns are this much dimmer than the nearest (0 = no range falloff). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float RangeFalloff = 0.35f;

	/** Two neighbouring returns closer than this in range are joined into one wall line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float JoinM = 6.f;

	/** At most this many traces in one frame; beams the sweep passed beyond it are traced on the next frames. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	int32 MaxTracesPerFrame = 24;

	/** Salvage shows when it is within this height of the sub's centre, up or down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float ContactHeightM = 40.f;

	/** The serpent shows within this height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float SerpentHeightM = 60.f;

	/** The proximity tone starts when the nearest wall at the sub's depth is this close (metres from the hull's side). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float ToneStartM = 12.f;

	/** ...and is at its fastest and highest this close. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float ToneFullM = 2.f;

	/** Seconds between tone beeps at the start and at full. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float ToneSlowS = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float ToneFastS = 0.12f;

	/** Pitch of the tone at the start and at full. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float TonePitchLow = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float TonePitchHigh = 1.6f;

	/** Volume scale of the tone at the start and at full. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float ToneVolumeLow = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float ToneVolumeHigh = 1.f;
};
