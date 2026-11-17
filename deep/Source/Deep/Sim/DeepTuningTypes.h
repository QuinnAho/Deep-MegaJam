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

	// The radio (Deep.Captain.Radio 1, D-040): in the water he speaks only when the diver needs it.

	/** In the water, an urgency-1 line shows only this long after the last line shown. Urgency 0 never shows there. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Captain|Radio", meta = (ClampMin = "0"))
	float WaterNotableGapSeconds = 30.f;

	/** Scared while the serpent closes on this diver (stalk, hunt or bite) within this many metres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Captain|Radio", meta = (ClampMin = "0"))
	float ScaredSerpentM = 25.f;

	/** Tense while Patience is under this fraction (tethered). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Captain|Radio", meta = (ClampMin = "0", ClampMax = "1"))
	float TensePatienceFraction = 0.25f;

	/** Seated at the helm: tense while the hull is under this integrity (0..1)... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Captain|Radio", meta = (ClampMin = "0", ClampMax = "1"))
	float TenseHull01 = 0.5f;

	/** ... and scared under this one (the hull_critical line is at Cave.HullCriticalIntegrity01). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Captain|Radio", meta = (ClampMin = "0", ClampMax = "1"))
	float ScaredHull01 = 0.25f;

	/** Scared and in the water: only lines at or above this urgency show (the planned near-silence). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Captain|Radio", meta = (ClampMin = "0", ClampMax = "3"))
	int32 ScaredMinUrgency = 3;
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

	/** Look only: coming back aboard (hatch, winch, harpoon) the view is set to this pitch, yaw kept, so the room reads, not the ceiling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena")
	float AboardPitchDeg = 0.f;

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

	/**
	 * A wide, soft dive light (rescope/light): full strength to InnerConeDeg, then the engine's squared ramp to
	 * OuterConeDeg, so the pool is about 15 m across at 10 m with a soft edge. 7/21 was a pencil beam ("you can't see
	 * anything really", Quinn). The intensity is unitless, so widening the cone does not dim its centre.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float InnerConeDeg = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float OuterConeDeg = 38.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float RangeM = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	bool bStartsOn = true;

	/** Style guide 3: cool neutral white #F2F4EE, never yellow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	FLinearColor Color = FLinearColor(FColor(0xF2, 0xF4, 0xEE));

	/**
	 * How strongly the beam lights the volumetric fog (the visible cone in the murk). 0 = no beam. Below the narrow cone's
	 * 1.2: the wide cone lights about three times the water, and the beam should read as murk, not as a solid.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float VolumetricScattering = 0.8f;

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
	 * 0.7 (was 0.5; measured sea water is near 0.9) halves the backscatter at the eye, which milks the view in a wide cone.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float FogScatteringDistribution = 0.7f;

	/** Light function on the downlight: the moving pattern of the surface far above, cut into the fog as shafts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	FSoftObjectPath ShaftLightFunction = FSoftObjectPath(TEXT("/Game/Deep/Look/M_SurfaceShafts.M_SurfaceShafts"));

	/** Light-function scale (cm per pattern unit): 100 = the material works in metres across the light. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float ShaftScaleCm = 100.f;

	/** Volumetric fog starts this far from the eye, or the lamp lights a blob of milk round the eye (was 0.5 m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float VolumetricStartM = 1.0f;

	/**
	 * Volumetric fog quality, set once as console variables by ADeepEnvironment (r.VolumetricFog.*). Epic, "Volumetric
	 * Fog": the heavy temporal filter leaves "lighting trails" behind a moving flashlight; a lower history weight lets the
	 * beam follow the head, and more history-miss samples hide the noise that adds. 0 keeps the engine's value. The grid
	 * stays at scalability (16 px, 64 slices): 8 px / 128 slices measured GPU 58 ms against 12 ms at stop 1 (Lane F).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	float FogHistoryWeight = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	int32 FogGridPixelSize = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	int32 FogGridSizeZ = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look")
	int32 FogHistoryMissSamples = 8;

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

	/**
	 * The most paint wear (M_WornSteel's Wear) any surface in the room gets: its shell, the hatch and the dressing. The
	 * chips are about 55 cm across, so above this they read as big torn blotches (the deck, a locker door, the porthole
	 * frame, Nov 17) instead of wear. The finish itself waits for the look pass.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Room")
	float RoomMaxWear = 0.15f;

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

	/**
	 * How much the floods light the water: a visible beam that the specks stream through. 0.15 (was 0.35): at 0.35 two
	 * 40,000 floods in front of the camera milked the whole helm view grey and hid the wall 9 m ahead (rescope/light).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look|Helm")
	float HelmFloodFog = 0.15f;

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

	/**
	 * The hull's box (ADeepSubmarine, the shaft, the hull tests): half length (X), half width (Y), height, the deck's top
	 * at -RoomHeightM / 2. Since Nov 17 the room set (ADeepSubRoom) has its own size, the Inside* fields below.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	float RoomHalfLengthM = 4.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	float RoomHalfWidthM = 2.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	float RoomHeightM = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	float WallThicknessM = 0.2f;

	/** Half the side of the hull's square hatch, centred under the sub (above the tether anchor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	float HatchHalfSizeM = 0.8f;

	/**
	 * The room set's inside (World/DeepRoomLayout), fitted to BT-01's cabin (Nov 17): about 1.8 m wide and 2.1 m high
	 * like the model's cabin, its length the part of the hull you could live in. Room metres, X ahead, Y starboard, the
	 * hatch's centre at X 0 Y 0 (lined up with the hull's hatch), so the room runs from -InsideAftM to +InsideForeM.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Inside")
	float InsideAftM = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Inside")
	float InsideForeM = 3.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Inside")
	float InsideHalfWidthM = 0.9f;

	/** Deck to ceiling. Low: a walker's head clears it by about half a metre. The deck's top is at -InsideHeightM / 2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Inside")
	float InsideHeightM = 2.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Inside")
	float InsideWallThicknessM = 0.15f;

	/** The room's round hatch in the deck, shut for good (the airlock is the way out). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Inside")
	float InsideHatchRadiusM = 0.55f;

	/** The porthole in the front wall over the helm: half width and half height, and its centre's height over the deck (eye height). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Inside")
	FVector2D InsidePortholeHalfM = FVector2D(0.45f, 0.3f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Inside")
	float InsidePortholeZM = 1.35f;

	/**
	 * Where a diver is put aboard (the start, a respawn, back in through the airlock): the capsule centre this high over
	 * the deck. The diver's half height (0.8 m) plus the walker's floor gap (the engine keeps 1.9-2.4 cm), so the diver
	 * stands on the deck at once instead of dropping onto it (Deep.World.SubRoomLayout).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Inside")
	float InsideStandHeightM = 0.82f;

	/**
	 * With the roof off (`Deep.Room.Roof 0`), the room's look (its fixed exposure, no water) reaches this high over the deck, so a plan
	 * view from a diver stood on the ceiling (eye about 3.6 m over the deck) reads at the room's exposure.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Inside")
	float InsidePlanViewHeightM = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	float RoomLightIntensity = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	float RoomLightRadiusM = 9.f;

	/**
	 * Where a diver stands at the start of a run and after a respawn (room metres on the deck), and the view pitch. The
	 * start, the enter stand and the console sit just forward of the hatch, all within UseRangeM of each other and of the
	 * valve (E acts on the nearest station in reach); the helm is at the bow, so no straight walk between them crosses
	 * the hatch (Deep.World.SubRoomLayout).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	FVector2D RoomStartM = FVector2D(1.7f, -0.15f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	float RoomStartPitchDeg = -22.f;

	/** Where a diver who comes up through the hatch is put on the deck. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	FVector2D EnterStandM = FVector2D(0.95f, -0.3f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	FVector2D ConsoleM = FVector2D(1.2f, 0.45f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	FVector2D HelmM = FVector2D(3.4f, 0.f);

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

	/**
	 * Where the room's fixed set (ADeepSubRoom) stands, world metres. Far above everything a dive can reach, so no rock,
	 * bed or cave light comes near it whichever way the world is placed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	FVector RoomSetLocationM = FVector(0.f, 0.f, 700.f);

	/** The airlock's valve wheel on the closed hatch in the room's deck (room metres, X ahead, Y starboard). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sub|Room")
	FVector2D ValveM = FVector2D(0.f, 0.f);
};

/**
 * The airlock between the fixed room and the water (Sim/DeepMovingSubRules::AirlockPhase). Out: hold E on the valve,
 * a hiss, the view fades to black, the diver is moved under the hull's hatch and the view fades in. Back: the diver is
 * moved into the room at once and the view starts black (the hiss plays), then fades in.
 */
USTRUCT(BlueprintType)
struct FDeepAirlockTuning
{
	GENERATED_BODY()

	/** Hold E on the valve this long to start the cycle (a tap does nothing: leaving is a choice). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Airlock")
	float ValveHoldSeconds = 0.6f;

	/** Going out: the hiss plays this long with the view still clear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Airlock")
	float HissSeconds = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Airlock")
	float FadeOutSeconds = 0.35f;

	/** Held black; the diver is moved at its start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Airlock")
	float BlackSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Airlock")
	float FadeInSeconds = 0.5f;
};

/**
 * The moving hull and the fixed room it carries (Sim/DeepMovingSubRules): the depth datum and how the still room fakes
 * the hull's motion (RoomSway). The room never tilts; only the view, the caged lights and the sound move.
 */
USTRUCT(BlueprintType)
struct FDeepMovingSubTuning
{
	GENERATED_BODY()

	/** World Z (m) of depth 0. Depth grows down from it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub")
	float DepthDatumZM = 0.f;

	/**
	 * Deep.World.Still: a stop's sea bed and cave show (and collide) while the hull is no more than this far below the
	 * stop's depth, so the hull and the helm camera never pass through a bed that shows.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub")
	float BedShowsBelowStopM = 0.5f;

	/** The view's slow roll in the room: this much at rest, more per m/s of helm speed, never more than the max. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub|Sway")
	float SwayIdleDeg = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub|Sway")
	float SwayDegPerMps = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub|Sway")
	float SwayMaxDeg = 1.5f;

	/** Cycles per second of that roll. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub|Sway")
	float SwayHz = 0.18f;

	/** Speeding up ahead tips the view back (up) this much per m/s/s (slowing tips it forward), up to SwayMaxDeg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub|Sway")
	float LeanDegPerMps2 = 0.6f;

	/** The caged lights swing this much per m/s of speed and per m/s/s of acceleration, up to the max, at LightSwingHz. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub|Sway")
	float LightSwingDegPerMps = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub|Sway")
	float LightSwingDegPerMps2 = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub|Sway")
	float LightSwingMaxDeg = 9.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub|Sway")
	float LightSwingHz = 0.45f;

	/** The engine hum is full at this helm speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub|Sound")
	float HumFullSpeedMps = 4.f;

	/** The hull creaks fully at this acceleration (m/s/s), or right after a knock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub|Sound")
	float CreakFullAccelMps2 = 1.5f;

	/** A knock shakes the view: degrees per m/s of impact (up to the max), dying away per second, at KnockShakeHz. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub|Knock")
	float KnockShakeDegPerMps = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub|Knock")
	float KnockShakeMaxDeg = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub|Knock")
	float KnockShakeDecayPerS = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub|Knock")
	float KnockShakeHz = 9.f;

	/** The blank porthole: now and then a faint light drifts past outside, this often (s), taking this long. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub|Porthole")
	float PortholeLightEveryS = 9.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub|Porthole")
	float PortholeLightPassS = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub|Porthole")
	float PortholeLightIntensity = 40.f;

	/**
	 * The Room look (ADeepEnvironment, each machine for its own view): while the local view is in the room set the water
	 * look eases to this, thin dry-air haze with no volumetric fog and no snow, and back to the cave look outside. The
	 * ease is no longer than the airlock's black (Airlock.BlackSeconds) or the seat's view blend, so a fade hides it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub|RoomLook")
	float RoomLookEaseSeconds = 0.2f;

	/** Height-fog density in the room (engine units: 0.05 = 0.005 per m, about 95% clear across the 10 m room). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub|RoomLook")
	float RoomFogDensity = 0.05f;

	/** The room haze's colour: a dark, nearly neutral steel grey, no sea in it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MovingSub|RoomLook")
	FLinearColor RoomFogColor = FLinearColor(FColor(0x10, 0x13, 0x15));
};

/**
 * The cave shaft between stops (Nov 15, Quinn: driving the sub is the fun, open water kills the dread). The passage is a
 * function of depth: a centre (metres, X = ahead, Y = starboard, relative to the stops' column) and a radius. At every
 * stop it is centred on the parked sub with StopRadiusM (the Cavern lane cuts its roof opening to match); in between it
 * bends, opens into caverns and pinches into squeezes. The sub's box against that wall is the hit. Sim/DeepCaveRules.
 */
USTRUCT(BlueprintType)
struct FDeepCaveTuning
{
	GENERATED_BODY()

	/** The shaft on. Off (or Deep.Cave.Shaft 0): the old straight drive, no offset, no wall, no hits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	bool bShaft = true;

	/** Radius of the passage at every stop (and the Cavern lane's roof opening). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Shape")
	float StopRadiusM = 14.f;

	/** Below a stop the passage stays straight, at StopRadiusM, this far; then it eases into bends over the same distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Shape")
	float StraightBelowStopM = 20.f;

	/** Above the next stop the passage is straight, at StopRadiusM, this far (it drops through the cavern roof). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Shape")
	float StraightAboveStopM = 15.f;

	/** How far the centre swings ahead/astern (X) and to the sides (Y) at most, mid-shaft. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Shape")
	FVector2D BendAmplitudeM = FVector2D(8.f, 12.f);

	/** Depth for one full swing of the bend, X and Y. Lower = sharper bends (more steering per metre of depth). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Shape")
	FVector2D BendWavelengthM = FVector2D(150.f, 130.f);

	/**
	 * The passage centre drifts sideways at most this far (m) per metre of depth: each stretch's bend is scaled down until
	 * it holds. 0.5 at a 2.6 m/s full-flood sink sweeps 1.3 m/s, which slow ahead on the inertial helm can follow. 0 = off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Shape")
	float BendMaxDriftPerM = 0.5f;

	/** Extra radius (m) where the centre drifts at that cap, less where it drifts less: room to fly the bend. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Shape")
	float BendRoomM = 1.5f;

	/** Radius of the open caverns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Shape")
	float CavernRadiusM = 30.f;

	/** Free room (m) left round the sub's widest corner in the tightest squeeze. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Shape")
	float SqueezeMarginM = 2.5f;

	/** Depth for one cavern-to-squeeze cycle of the radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Shape")
	float RadiusWavelengthM = 90.f;

	/** Each segment between two stops gets its own phase from this, so no two look alike. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Shape")
	int32 Seed = 7;

	/** The shaft goes on this far below the deepest stop (straight, StopRadiusM). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Shape")
	float BelowLastStopM = 110.f;

	/**
	 * The rock tube starts this far below each stop (it shows once the sub leaves the stop downward and the stop's bed
	 * hides) and ends this far above the next one, where the cavern roof takes over.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Shape")
	float TubeStartBelowStopM = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Shape")
	float TubeEndAboveStopM = 10.f;

	/** Added round the sub's collision box for the wall test: the outside dress, the tanks and the helm camera stick out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Helm")
	float HullPadM = 0.6f;

	/** Full speed at the helm, sideways (A/D) and ahead or astern (W/S). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Helm")
	float SteerSpeedMps = 6.f;

	/** How fast that speed follows the control (m/s per second): a little inertia, so a hit can be misjudged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Helm")
	float SteerAccelMps2 = 7.f;

	/** Parked, the sub glides back to the stop's centre at this speed (the hatch opens once it is there). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Helm")
	float ParkGlideMps = 5.f;

	/** Fraction of the impact speed the wall throws the sub back with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Hits")
	float Bounce = 0.35f;

	/** Closing on the wall at least this fast is a hit; slower is a scrape. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Hits")
	float HitMinSpeedMps = 1.5f;

	/** Hull damage per m/s of impact speed (rounded, at least 1 for a hit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Hits")
	float DamagePerMps = 0.5f;

	/** One hit does no more than this: a squeeze closing on an off-centre sub at full descent would otherwise breach at once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Hits")
	int32 MaxHitDamage = 3;

	/** No second hit within this long of the last. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Hits")
	float HitCooldownSeconds = 1.f;

	/** Grinding along the wall: one point of damage per this many seconds of contact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Hits")
	float ScrapeSecondsPerDamage = 2.5f;

	/** The scrape sound and line come again no sooner than this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Hits")
	float ScrapeCooldownSeconds = 4.f;

	/** At this much hull damage (cave hits and harpoon hits together) the hull breaches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Hits")
	int32 HullBreachDamage = 10;

	/** hull_critical fires when a dent takes the hull's integrity (0..1) under this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Hits")
	float HullCriticalIntegrity01 = 0.25f;

	/** A breach kills the diver aboard (death fee, a new diver, the hull patched and billed). Off: the hull holds at the limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Hits")
	bool bHullBreachKills = true;

	/** The helm camera's jolt on a hit: degrees per m/s of impact, and how fast it dies away (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Look")
	float JoltDegPerMps = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Look")
	float JoltDecayPerSecond = 5.f;

	/** The rock wall's mesh: rows this tall and this many points round. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Look")
	float WallRowM = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Look")
	int32 WallSides = 128;

	/**
	 * The drawn wall stands this much outside the rule's radius on average, its relief (Look.TrenchLedge*) scaled by
	 * WallReliefScale round that: the rock never stands inside the line the hits are measured on by more than a hand.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Look")
	float WallReliefOutM = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Look")
	float WallReliefScale = 0.5f;

	/** The wall's surface: the sea bed's silt by default (UV0 in tiles of Look.SeabedUvTileM). Falls back to WallColor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Look")
	FSoftObjectPath WallMaterial = FSoftObjectPath(TEXT("/Game/Deep/Look/M_Silt.M_Silt"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Look")
	FLinearColor WallColor = FLinearColor(0.035f, 0.036f, 0.034f);

	// --- The generated rock (UDeepCaveRules::WallRadiusM): one shape for the mesh, Contact and the sonar. ---
	// Tweak while playing: change a number here, then `Deep.Cave.Rebuild` (or leave Deep.Cave.AutoRebuild 1 on).

	/** Off = the smooth round tube (RadiusM only), as before the generated cave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Rock")
	bool bRock = true;

	/** The rock's own seed (its own integer hash, no engine random): same seed, same cave, on every machine. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Rock")
	int32 RockSeed = 1337;

	/** Layers of noise, each Lacunarity times finer and Gain times weaker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Rock", meta = (ClampMin = "1", ClampMax = "8"))
	int32 RockOctaves = 5;

	/** How far (m) the rock bulges in or recedes, at most, mid-shaft. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Rock")
	float RockAmplitudeM = 3.2f;

	/** Size (m) of the biggest lumps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Rock")
	float RockScaleM = 11.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Rock")
	float RockLacunarity = 2.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Rock")
	float RockGain = 0.5f;

	/** 0 = soft round lumps, 1 = sharp ridges (ribs that stand into the passage). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Rock", meta = (ClampMin = "0", ClampMax = "1"))
	float RockRidge = 0.6f;

	/** Below 1 the lumps stretch up and down into ribs and flutes (vertical scale of the noise). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Rock")
	float RockStretchZ = 0.55f;

	/** A ledge (bedding plane) every this many metres of depth; 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Rock")
	float StrataSpacingM = 4.5f;

	/** How far (m) a ledge stands into the passage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Rock")
	float StrataDepthM = 1.8f;

	/** 0 = every ledge runs all the way round; 1 = ledges come and go round the wall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Rock", meta = (ClampMin = "0", ClampMax = "1"))
	float StrataBreakup = 0.7f;

	/** The share of the relief left at a cavern mouth (where the stops' straight ends are): calmer, so the mouth reads. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Rock", meta = (ClampMin = "0", ClampMax = "1"))
	float MouthCalm01 = 0.3f;

	/** The rock never comes nearer the passage's centre than the sub's minimum radius plus this (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Rock")
	float RockFloorMarginM = 0.6f;

	/** Contact samples the sub's box outline and height at least this often (m), so a rib between two corners still hits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Rock")
	float ContactStepM = 1.f;

	/** The rock's surface for the shaft and the caverns; falls back to WallMaterial (shaft) or the cavern's own when missing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Rock")
	FSoftObjectPath RockMaterial = FSoftObjectPath(TEXT("/Game/Deep/Cave/M_CaveRock.M_CaveRock"));

	/** Pack rocks half-buried in the shaft walls (look only, no collision): this many per 100 m of shaft. 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Rock")
	float ScatterPer100M = 70.f;

	/** Scatter rock size (m across) range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Rock")
	FVector2D ScatterSizeM = FVector2D(2.f, 6.f);

	/**
	 * How far (m) a scattered rock may stand out of the wall, measured from the wall's deepest point over the rock's
	 * span. Keep it under HullPadM: Contact keeps the sub's box that far off the rock, so the hull never meets one.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave|Rock")
	float ScatterProtrudeM = 0.45f;
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

/** One camera shake: Perlin noise on location and rotation under an eased envelope (Camera/DeepCameraRules). */
USTRUCT(BlueprintType)
struct FDeepShakeSpec
{
	GENERATED_BODY()

	/** Location noise amplitude on each axis (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float LocationCm = 0.f;

	/** Pitch and yaw noise amplitude (degrees); roll gets RollScale of it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float RotationDeg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float RollScale = 0.5f;

	/** Noise speed: about this many wobbles a second. Low = a heave, high = a rattle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float FrequencyHz = 8.f;

	/** Seconds from start to end, blends included. 0 or less = until stopped (the handheld sway). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float DurationS = 0.4f;

	/** Ease-in and ease-out times (smoothstep). Every shake eases both ways. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float BlendInS = 0.04f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float BlendOutS = 0.25f;

	bool IsNone() const { return LocationCm <= 0.f && RotationDeg <= 0.f; }
};

/** A bus event (matched by name) and the shake it plays. */
USTRUCT(BlueprintType)
struct FDeepEventShake
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FName Event;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FDeepShakeSpec Shake;
};

/**
 * One look for the sonar pulse made visible (M_SonarPulse). FDeepCameraTuning holds two, PulseJam and PulseSoft, and
 * Deep.Sonar.Style picks one. Values are set in the FDeepCameraTuning constructor.
 */
USTRUCT(BlueprintType)
struct FDeepPulseLook
{
	GENERATED_BODY()

	/** The overlay colour (display sRGB, added after the tonemapper). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	FLinearColor Color = FLinearColor::White;

	/** The shell's growth from the ping's origin to its range (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	float Seconds = 1.5f;

	/** The band's trail behind the shell where it crosses a surface (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	float BandM = 0.6f;

	/** The band's leading edge as a fraction of BandM: small = a hard front, 1 = as soft as the trail. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	float BandLead = 0.35f;

	/** Band brightness (overlay units after the tonemapper: 1 = the colour itself). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	float BandIntensity = 0.9f;

	/** Spacing of the lidar dot grid near the eye (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	float DotSpacingM = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	float DotIntensity = 0.35f;

	/** Dots left behind the band fade over this long (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	float DotFadeS = 2.f;

	/** The helm's ping reaches this far (m); 0 = the sonar's own range (Sonar.RangeM). The diver's keeps DiverPingRangeM. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	float HelmRangeM = 0.f;

	/** The diver's ping is this much brighter than the helm's (band and dots). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	float DiverGain = 1.f;

	/** The band is this much brighter on the serpent (custom depth), so its outline reads as the shell passes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	float SerpentBandGain = 1.f;

	/** The shell dims as it travels: at its range, band and dots are (1 - RangeFade) as bright (0 = no loss). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	float RangeFade = 0.f;
};

/**
 * The camera: shakes on events, the helmet-camera footage look, the sonar pulse made visible, interference.
 * Each feature has a console switch (Deep.Camera.Shake, Deep.Look.Footage, Deep.Sonar.Pulse, Deep.Look.Interference);
 * the switch's default is the b...On flag here.
 */
USTRUCT(BlueprintType)
struct FDeepCameraTuning
{
	GENERATED_BODY()

	FDeepCameraTuning();

	// ---- Shakes

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shake")
	bool bShakeOn = true;

	/** Event name -> shake. Names from other lanes (hull_*) are matched by name, so they work when they arrive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shake")
	TArray<FDeepEventShake> EventShakes;

	/** Every shake's size is multiplied by this (one knob for "too much"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shake")
	float ShakeScale = 1.f;

	/** A wall hit at this speed (m/s, payload ImpactMps) plays its row as written; slower is smaller, faster bigger. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shake")
	float HitShakeRefMps = 3.f;

	/** The hit's size factor (impact / HitShakeRefMps) is clamped to this: a graze still reads, a ram is not a blur. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Shake")
	FVector2D HitShakeScaleRange = FVector2D(0.6f, 1.8f);

	// ---- Footage look (the diver's helmet camera, watched from the sub)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Footage")
	bool bFootageOn = false;

	/** Film grain added on top of the depth look, at FootageShallowM and FootageDeepM (the camera pushes its gain in the dark). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Footage")
	FVector2D FootageGrainAdd = FVector2D(0.25f, 0.55f);

	/** Extra grain in the shadows at the same two depths (gain noise lives in the dark). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Footage")
	FVector2D FootageGrainShadows = FVector2D(1.2f, 1.8f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Footage")
	float FootageShallowM = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Footage")
	float FootageDeepM = 500.f;

	/** Vignette added to the depth look. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Footage")
	float FootageVignetteAdd = 0.2f;

	/** Chromatic aberration added to the depth look, kept to the edges by FootageFringeStart (0 centre, 1 corner). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Footage")
	float FootageFringeAdd = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Footage")
	float FootageFringeStart = 0.45f;

	/** Motion blur while the footage look is on (Look.MotionBlur otherwise). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Footage")
	float FootageMotionBlur = 0.2f;

	/** Barrel lens distortion strength (0 = off): UV pushed outward by k * r^2 in the lens material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Footage")
	float FootageBarrel = 0.06f;

	/** The handheld sway: a low, slow Perlin shake that never ends while the look is on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Footage")
	FDeepShakeSpec Sway;

	/** The footage lens post-process material (barrel distortion, interference). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Footage")
	FSoftObjectPath LensMaterial = FSoftObjectPath(TEXT("/Game/Deep/Camera/M_FootageLens.M_FootageLens"));

	// ---- The sonar made visible

	/** On by default since Nov 15 (D-041): the pulse is how the player sees the caverns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	bool bPulseOn = true;

	/** The diver's own ping (V in the water, no flashlight needed): this long between pings (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	float DiverPingCooldownS = 3.f;

	/** The diver's ping reaches this far (m): past the cavern walls at stops 2 and 3 (60-94 m from the hatch). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	float DiverPingRangeM = 100.f;

	/** Past this eye distance (m) the band thickens with distance, so a far wall's band keeps its width on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	float PulseBandFarM = 20.f;

	/** Past this eye distance (m) the lidar grid doubles its spacing each time the distance doubles: far dots stay
	 * a few pixels apart instead of shimmering, and each coarser grid is every other dot of the finer one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	float PulseDotNearM = 12.f;

	/** Which look a ping draws and how often the helm pings (Deep.Sonar.Style overrides): 0 = the jam's (PulseJam, a
	 * ping every sweep), 1 = soft (PulseSoft, a ping every Sonar.PingPeriodS). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	int32 SonarStyle = 1;

	/** The jam's look: amber, a hard bright band, a dense grid that lingers 2 s. Set in the constructor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	FDeepPulseLook PulseJam;

	/** The soft look (Quinn, Nov 17: softer, minimal, low-key, still horror): pale cyan, a thin dim band, sparse faint
	 * dots gone within a second, so the screen is clean between pings. Set in the constructor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	FDeepPulseLook PulseSoft;

	/** Shell range when neither the look nor the ping says (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	float PulseRangeM = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	FSoftObjectPath PulseMaterial = FSoftObjectPath(TEXT("/Game/Deep/Camera/M_SonarPulse.M_SonarPulse"));

	/** No pulse inside this radius from the sub (metres). The hull box below keeps the sub itself dark, so this is
	 * small: rock a metre off the hull in the shaft still lights. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	float PulseStartM = 0.5f;

	/** The pulse never lights the sub: pixels inside the box round the sub's drawn parts, grown by this (metres), are skipped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	float PulseHullMarginM = 0.3f;

	/** Lidar dots fade out between 80% of this distance from the eye and this (metres). Past the cavern walls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Pulse")
	float PulseDotFarM = 120.f;

	// ---- Interference (the feed degrades as the serpent closes)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Interference")
	bool bInterferenceOn = false;

	/** Serpent head distance (m) where interference starts, and where it is full. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Interference")
	FVector2D InterferenceFarNearM = FVector2D(30.f, 4.f);

	/** Strength while the serpent's distance alone drives it (a bite is 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Interference")
	float InterferenceNearMax = 0.6f;

	/** A burst on a hit: strength and seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Interference")
	float InterferenceBurst = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Interference")
	float InterferenceBurstS = 0.5f;

	/** Strength eases toward its target at this rate (per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Interference")
	float InterferenceEasePerS = 4.f;

	/** The interference post-process material (M_FeedInterference: static, line tearing, colour split). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Interference")
	FSoftObjectPath InterferenceMaterial = FSoftObjectPath(TEXT("/Game/Deep/Camera/M_FeedInterference.M_FeedInterference"));

	/** At full strength: how far a torn band jumps sideways (fraction of the screen width). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Interference")
	float InterferenceTear = 0.03f;

	/** At full strength: red and blue pulled apart sideways (fraction of the screen width). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Interference")
	float InterferenceSplit = 0.006f;

	/** At full strength: how much of the picture is static snow (0..1). Kept low so the dark stays dark. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Interference")
	float InterferenceStatic = 0.3f;

	/** Bands down the screen that tear as one (fewer = thicker tears). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Interference")
	float InterferenceBands = 70.f;
};

/**
 * The cavern (ADeepCavern): rock walls and a roof round each stop's arena, the sea bed as its floor. Quinn, 11:55 AM
 * Nov 15: "if you're swimming in an open ocean it really lessens it". Sized from the rules, not by eye: the walls stand
 * past both the tether's reach and the farthest salvage spot, plus a margin, at every height a diver uses (up to
 * ClearAboveAnchorM over the hatch); every bump of relief only pushes rock outward, so the clearance holds by
 * construction. The roof has an opening of HoleRadiusM centred over the sub, where the descent shaft comes down.
 */
USTRUCT(BlueprintType)
struct FDeepCavernTuning
{
	GENERATED_BODY()

	/** Off: today's open arena (the cheat Deep.Cave.Cavern 0 does the same at run time). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	bool bEnabled = true;

	/** Rock stands at least this far past the tether's full length (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float TetherMarginM = 3.f;

	/** ... and this far past the farthest salvage spot (m from the anchor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float SpotMarginM = 3.f;

	/** The clear space reaches this high over the anchor everywhere (m). The sub's roof and dress top out near 5 m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float ClearAboveAnchorM = 6.f;

	/** The roof's height over the anchor before relief (m): low, so the lamp finds it from the hatch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float RoofAboveAnchorM = 10.f;

	/** How square the vault is where the roof meets the wall (superellipse power; 2 = an ellipse, higher = flatter roof, sharper corner). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float RoofPower = 4.f;

	/** The roof's opening over the sub (m): the descent shaft's radius (the Shaft lane's contract). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float HoleRadiusM = 14.f;

	/** A short collar of rock up from the opening's rim, so the shaft's tube never shows a gap at the roof (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float NeckHeightM = 8.f;

	/** The collar widens by this much at its top (m), so it sits just behind the shaft's tube, never in front. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float NeckFlareM = 0.6f;

	/** The walls run this far down into the silt, so no gap shows where they meet the bed (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float SkirtBelowBedM = 4.f;

	/** Rock relief, outward only (m): a large lumpy noise on every face. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float ReliefM = 5.f;

	/** The relief's wavelength (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float ReliefScaleM = 11.f;

	/** Alcoves: this many bays round the wall, each up to AlcoveDepthM deep. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	int32 AlcoveCount = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float AlcoveDepthM = 7.f;

	/** Horizontal strata on the walls: ledges StrataM deep, one every StrataEveryM of height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float StrataM = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float StrataEveryM = 3.f;

	/** An uneven roof: extra relief, upward only (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float RoofReliefM = 3.f;

	/** Crags: sharp ridged relief at lamp scale, CragM deep and CragScaleM across, so a wall 10 m off in the lamp shows facets, not a flat grey. Outward only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float CragM = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float CragScaleM = 3.5f;

	/** Mesh resolution: columns round the cave, rows from the collar's top to the wall's foot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	int32 Columns = 384;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	int32 Rows = 180;

	/** Overhangs: rock spurs hanging from the roof. Their tips stay at least TipClearM above the clear space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	int32 HangCount = 22;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float HangRadiusMinM = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float HangRadiusMaxM = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float HangTipClearM = 1.f;

	/** Spurs hang no nearer the shaft's axis than this (m), clear of the shaft and the sub. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float HangMinOutM = 19.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	int32 Seed = 1931;

	/** The rock's surface. Empty = the sea bed's (FDeepLookTuning::SeabedMaterial), worked out from world position. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	FSoftObjectPath Material;

	/** UV tile (m), for a material that reads UV0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cave")
	float UvTileM = 4.f;
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

	/** One turn of the sweep (traces and scope contacts) every this many seconds. The jam look (Camera.SonarStyle 0)
	 * also pings once a turn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float SweepPeriodS = 2.5f;

	/** The soft look (Camera.SonarStyle 1) pings, sounds and shows the pulse once every this many seconds. The serpent
	 * hears each ping as noise: at 6 s the helm alone peaks at Monster.PingNoise and does not reach StalkNoise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sonar")
	float PingPeriodS = 6.f;

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

/** The tether as the diver's air line (rescope, Nov 15): clipped = unlimited air, off it = bailout air. */
USTRUCT(BlueprintType)
struct FDeepAirLineTuning
{
	GENERATED_BODY()

	/** The old rule: the Captain's Patience drains in the water and cuts the line at zero. Off: it holds (a cheat can
	 * still set it to zero, which still cuts). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air line")
	bool bPatienceDrains = false;

	/** The serpent's head within this of the rope's simulated segments bites it through (head radius + rope + margin). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air line")
	float BiteTouchM = 0.6f;

	/** Only an attacking serpent bites the line (Stalk, Bite); a pass, a lurk or a recoil brushing it does not. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air line")
	bool bOnlyAttackingBites = true;

	/** Credits charged to clip back on at the hatch after a voluntary unclip (the re-tether fee is for cuts). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Air line")
	int32 UnclipReclipFee = 0;
};

/** The room's dressing (World/DeepRoomDress): materials, the clear zones it keeps and its light fixtures (Lane E, rescope). */
USTRUCT(BlueprintType)
struct FDeepRoomDressTuning
{
	GENERATED_BODY()

	/** 0 hides the dressing and its lights (before-and-after frames). `Deep.Room.Dress 0|1` sets it live. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomDress")
	bool bEnabled = true;

	/** Material instances of M_WornSteel, built by Content/Deep/Room/build_room_materials.py. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomDress")
	FSoftObjectPath FrameMaterial = FSoftObjectPath(TEXT("/Game/Deep/Room/MI_FrameSteel.MI_FrameSteel"));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomDress")
	FSoftObjectPath PipeMaterial = FSoftObjectPath(TEXT("/Game/Deep/Room/MI_PipeSteel.MI_PipeSteel"));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomDress")
	FSoftObjectPath BrassMaterial = FSoftObjectPath(TEXT("/Game/Deep/Room/MI_DullBrass.MI_DullBrass"));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomDress")
	FSoftObjectPath RubberMaterial = FSoftObjectPath(TEXT("/Game/Deep/Room/MI_Rubber.MI_Rubber"));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomDress")
	FSoftObjectPath GlassMaterial = FSoftObjectPath(TEXT("/Game/Deep/Room/MI_GrimyGlass.MI_GrimyGlass"));

	/** Pieces below this height over the deck (cm) must keep out of the walks, the hatch and the helm zone: a walker's head top is about 162. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomDress")
	float HeadClearCm = 170.f;

	/** A walk's half width beyond the walker's capsule radius (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomDress")
	float WalkMarginCm = 3.f;

	/** Kept clear round the round hatch's edge (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomDress")
	float HatchClearCm = 20.f;

	/** Kept clear round the helm seat and the throttle and ballast levers beside it (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomDress")
	float HelmClearCm = 25.f;

	/** Caged practicals (#FFB45A) over the console and the helm. Candela-ish units of a point light. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomDress")
	FLinearColor PracticalColor = FLinearColor(FColor(0xFF, 0xB4, 0x5A));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomDress")
	float PracticalIntensity = 900.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomDress")
	float PracticalRadiusCm = 260.f;

	/** Caged emergency lights (#D8271C): port bulkhead by the bench and the aft bulkhead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomDress")
	FLinearColor EmergencyColor = FLinearColor(FColor(0xD8, 0x27, 0x1C));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomDress")
	float EmergencyIntensity = 700.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomDress")
	float EmergencyRadiusCm = 300.f;

	/** Low cool strip (#BFE3EA) under the port pipe run, raking the pipes and the lockers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomDress")
	FLinearColor StripColor = FLinearColor(FColor(0xBF, 0xE3, 0xEA));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomDress")
	float StripIntensity = 350.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoomDress")
	float StripRadiusCm = 280.f;
};

/**
 * Real submarine controls at the helm (Sim/DeepPilotRules): a throttle lever in notches, a rudder with inertia and a
 * ballast lever that stays where it is left. Speeds in m/s, rates per second. The sub never moves; the world does.
 */
USTRUCT(BlueprintType)
struct DEEP_API FDeepPilotTuning
{
	GENERATED_BODY()

	/** Throttle notches ahead of stop (slow, half, full) and astern (half, full). W/S step one notch per press. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Throttle")
	int32 AheadNotches = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Throttle")
	int32 AsternNotches = 2;

	/** Speed along the heading at full ahead and full astern; the notches in between are even fractions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Throttle")
	float AheadSpeedMps = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Throttle")
	float AsternSpeedMps = 2.f;

	/** Drag along the heading (per second): low, so the sub carries way. 1/this = seconds to most of a new speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Throttle")
	float SurgeDragPerS = 0.45f;

	/** Drag across the heading (per second): high, so a turn skids only a little. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Throttle")
	float SwayDragPerS = 2.5f;

	/** Turn rate at full rudder (deg/s) and how fast the rate builds and decays (per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Rudder")
	float YawRateDegS = 28.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Rudder")
	float YawResponsePerS = 1.6f;

	/** Ballast lever travel per second while Space (blow) or C (flood) is held; it stays where it is left. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Ballast")
	float BallastLeverPerS = 1.2f;

	/** Passing neutral the lever holds in the detent this long before it goes on to the other side. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Ballast")
	float BallastDetentS = 0.35f;

	/** Let go this close to neutral, the lever drops into the detent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Ballast")
	float BallastDetentSnap = 0.12f;

	/** Vertical speed at a full lever (flooded: down, blown: up); drag caps it there. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Ballast")
	float HeaveSpeedMps = 2.6f;

	/** Vertical drag (per second): how fast buoyancy reaches that speed and how long the sub coasts after. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Ballast")
	float HeaveDragPerS = 0.55f;

	/** Parks when this close to a stop's depth, slower than ParkSpeedMps (all axes), with the throttle at stop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Park")
	float ParkNearM = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Park")
	float ParkSpeedMps = 1.2f;

	/** Parked, offset, depth and yaw glide to the stop's frame: this fraction-rate per second, at least the minimum speeds (about 2 s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Park")
	float ParkGlidePerS = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Park")
	float ParkGlideMinMps = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Park")
	float ParkGlideMinDegS = 12.f;

	/** A knock on the rock trips the throttle back to stop (clear feedback, no grinding along the wall at full ahead). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Walls")
	bool bKnockTripsThrottle = true;

	/** Of the speed along the wall, this much is left after a knock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Walls")
	float KnockKeepAlong = 0.5f;

	/** The levers' throw either side of upright (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Levers")
	float LeverThrowDeg = 38.f;

	/** Where the throttle and ballast levers stand, room metres (X ahead, Y starboard), either side of the helm seat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Levers")
	FVector2D ThrottleLeverM = FVector2D(3.1f, -0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Levers")
	FVector2D BallastLeverM = FVector2D(3.1f, 0.5f);

	/** How fast a lever swings to where it is set (per second, a share of the gap): it moves, it does not jump. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pilot|Levers")
	float LeverFollowPerS = 14.f;
};
