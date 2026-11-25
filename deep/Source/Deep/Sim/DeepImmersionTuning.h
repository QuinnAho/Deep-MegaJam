// Immersion I1 (D-072): the hull's voice, the silt the hull lifts and the sonar's echo. Every number for them lives
// here, one property in UDeepTuningSettings (DEEP Tuning > Immersion). Rules: Sim/DeepImmersionRules.

#pragma once

#include "CoreMinimal.h"
#include "DeepImmersionTuning.generated.h"

/** The steel hull under pressure: creaks, a groan at each 100 m band, the water flowing past (Immersion/DeepImmersionAudio). */
USTRUCT(BlueprintType)
struct DEEP_API FDeepHullVoiceTuning
{
	GENERATED_BODY()

	/** Depth (read m) of the shallowest and the deepest creak character: rate, pitch and level blend between them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Creak")
	float ShallowM = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Creak")
	float DeepM = 600.f;

	/** Mean seconds between creaks at ShallowM and at DeepM with the hull still: rarer up top, frequent deep down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Creak", meta = (ClampMin = "1"))
	float ShallowIntervalS = 26.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Creak", meta = (ClampMin = "1"))
	float DeepIntervalS = 7.f;

	/** Extra creaks per second at a full-rate turn (TurnFullDegS) and at a full-rate depth change (DepthChangeFullMps). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Creak", meta = (ClampMin = "0"))
	float TurnCreaksPerS = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Creak", meta = (ClampMin = "1"))
	float TurnFullDegS = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Creak", meta = (ClampMin = "0"))
	float DepthChangeCreaksPerS = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Creak", meta = (ClampMin = "0.1"))
	float DepthChangeFullMps = 2.f;

	/** Two creaks are never closer than this (s), however hard the hull works. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Creak", meta = (ClampMin = "0.1"))
	float MinGapS = 2.5f;

	/** How uneven the spacing is: each wait is the mean x (1 +- this). 0 = metronome. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Creak", meta = (ClampMin = "0", ClampMax = "0.9"))
	float Irregularity = 0.6f;

	/** Parked at or above SurfaceStopM, the room's creaks come at this share of the rate (so they do not wear thin). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Creak", meta = (ClampMin = "0", ClampMax = "1"))
	float SurfaceRestRate = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Creak")
	float SurfaceStopM = 150.f;

	/** Pitch of a creak at ShallowM and at DeepM (the steel tighter and lower deep down), and its +- jitter (fraction). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Creak", meta = (ClampMin = "0.5", ClampMax = "2"))
	float ShallowPitch = 1.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Creak", meta = (ClampMin = "0.5", ClampMax = "2"))
	float DeepPitch = 0.78f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Creak", meta = (ClampMin = "0", ClampMax = "0.25"))
	float PitchJitter = 0.06f;

	/** Level of a creak (x its Sounds.csv row) at ShallowM and DeepM; a creak forced by a turn or a depth change adds StressVolume. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Creak", meta = (ClampMin = "0", ClampMax = "1"))
	float ShallowVolume = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Creak", meta = (ClampMin = "0", ClampMax = "1"))
	float DeepVolume = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Creak", meta = (ClampMin = "0", ClampMax = "1"))
	float StressVolume = 0.2f;

	/** The long groan plays once the first time the read depth passes each multiple of this (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Groan", meta = (ClampMin = "10"))
	float GroanBandM = 100.f;

	/** The groan's pitch at ShallowM and DeepM: each band sounds heavier than the last. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Groan", meta = (ClampMin = "0.5", ClampMax = "2"))
	float GroanShallowPitch = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Groan", meta = (ClampMin = "0.5", ClampMax = "2"))
	float GroanDeepPitch = 0.8f;

	/** After a groan no creak comes for this long (s): the groan is the event; a creak on top of it would cheapen it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Groan", meta = (ClampMin = "0"))
	float CreakQuietAfterGroanS = 6.f;

	/** The flow noise: silent at or under FlowStartMps, full at FlowFullMps (the hull's top speed ahead). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Flow", meta = (ClampMin = "0"))
	float FlowStartMps = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Flow", meta = (ClampMin = "0.1"))
	float FlowFullMps = 4.f;

	/** Pitch and low-pass of the flow at rest-to-slow and at full speed: dark and low when slow, brighter as it builds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Flow", meta = (ClampMin = "0.5", ClampMax = "2"))
	float FlowSlowPitch = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Flow", meta = (ClampMin = "0.5", ClampMax = "2"))
	float FlowFullPitch = 1.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Flow", meta = (ClampMin = "50"))
	float FlowSlowLowPassHz = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Flow", meta = (ClampMin = "50"))
	float FlowFullLowPassHz = 2600.f;

	/** The flow follows the speed with these fades (s): it swells as the hull gets going, dies off as it coasts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Flow", meta = (ClampMin = "0.05"))
	float FlowFadeInS = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hull voice|Flow", meta = (ClampMin = "0.05"))
	float FlowFadeOutS = 1.5f;
};

/** Silt the hull and the divers lift off the floor and rock (Immersion/DeepSilt). Sizes in metres, times in seconds. */
USTRUCT(BlueprintType)
struct DEEP_API FDeepSiltTuning
{
	GENERATED_BODY()

	/** The hull lifts silt when its underside runs within this of the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Lift")
	float LiftHeightM = 1.5f;

	/** Puffs a second at the floor (clearance 0) at LiftFullMps; fewer higher up (by LiftFalloff) and slower. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Lift", meta = (ClampMin = "0"))
	float LiftPuffsPerS = 7.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Lift", meta = (ClampMin = "0.5"))
	float LiftFalloff = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Lift", meta = (ClampMin = "0.1"))
	float LiftFullMps = 2.5f;

	/** Under this speed (m/s) the hull lifts nothing: a sub at rest over the floor does not keep smoking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Lift", meta = (ClampMin = "0"))
	float LiftMinMps = 0.3f;

	/** Rock this close to a side of the hull (m) counts as a scrape: a puff at the contact every ScrapeEveryS while moving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Scrape", meta = (ClampMin = "0"))
	float ScrapeGapM = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Scrape", meta = (ClampMin = "0.05"))
	float ScrapeEveryS = 0.25f;

	/** A knock (hull_hit) bursts this many puffs at 1 m/s into the rock, and PuffsPerImpactMps more per m/s, up to KnockPuffsMax. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Scrape", meta = (ClampMin = "0"))
	int32 KnockPuffs = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Scrape", meta = (ClampMin = "0"))
	float PuffsPerImpactMps = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Scrape", meta = (ClampMin = "0"))
	int32 KnockPuffsMax = 10;

	/** A swimming diver this close to the floor (m) and faster than DiverMinMps kicks up a small puff every DiverEveryS. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Diver", meta = (ClampMin = "0"))
	float DiverLiftHeightM = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Diver", meta = (ClampMin = "0"))
	float DiverMinMps = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Diver", meta = (ClampMin = "0.1"))
	float DiverEveryS = 0.6f;

	/** A diver's puff is this share of a hull puff's size. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Diver", meta = (ClampMin = "0.1", ClampMax = "1"))
	float DiverPuffScale = 0.45f;

	/** How often (Hz) the hull and the divers probe the rock round them (a few line traces each). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Probe", meta = (ClampMin = "1", ClampMax = "60"))
	float ProbeHz = 10.f;

	/** The most puffs alive at once; a new one takes the oldest one's place. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Puff", meta = (ClampMin = "1", ClampMax = "512"))
	int32 MaxPuffs = 72;

	/** A puff's radius at birth and once spread (m), each picked in the range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Puff")
	FVector2D StartRadiusM = FVector2D(0.35f, 0.6f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Puff")
	FVector2D SpreadRadiusM = FVector2D(1.4f, 2.6f);

	/** Seconds to reach most of its spread (ease out). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Puff", meta = (ClampMin = "0.1"))
	float SpreadS = 4.f;

	/** The first push off the floor (m/s along the surface's normal) and outward, both slowed by Drag (per s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Puff")
	FVector2D RiseMps = FVector2D(0.25f, 0.55f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Puff")
	float OutwardMps = 0.35f;

	/** The thrusters' wash: lifted puffs are thrown aft of the hull's travel (this share of their push) at up to WashMps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Lift", meta = (ClampMin = "0", ClampMax = "1"))
	float WashShare = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Lift", meta = (ClampMin = "0"))
	float WashMps = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Puff", meta = (ClampMin = "0"))
	float Drag = 0.55f;

	/** The slow current every puff drifts in (m/s, world), a wander on top, and the settling of fine silt (m/s down). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Puff")
	FVector CurrentMps = FVector(0.04f, 0.025f, 0.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Puff", meta = (ClampMin = "0"))
	float WanderMps = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Puff", meta = (ClampMin = "0"))
	float SettleMps = 0.02f;

	/** How long a puff hangs (s), picked in the range; it fades in over FadeInS and out over the last FadeOutShare of its life. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Puff")
	FVector2D LifeS = FVector2D(10.f, 20.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Puff", meta = (ClampMin = "0.05"))
	float FadeInS = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Puff", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float FadeOutShare = 0.45f;

	/** A puff's peak opacity (the material multiplies its soft edge by it), and how much it thins as it spreads. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Puff", meta = (ClampMin = "0", ClampMax = "1"))
	float PeakOpacity = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Puff", meta = (ClampMin = "0", ClampMax = "1"))
	float SpreadThinning = 0.4f;

	/** Puffs are born this share of their start radius off the surface, on its open side, and never within the hull's box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Puff", meta = (ClampMin = "0"))
	float SpawnLiftShare = 0.6f;

	/** The hull shoves puffs aside until this share of their radius is clear of its box (1 = none of the puff inside). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Puff", meta = (ClampMin = "0", ClampMax = "1.5"))
	float HullKeepShare = 1.f;

	/** The puffs' look (style guide 4: suspended silt, grey-brown, saturation under 35%; light from the sources). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Look")
	FLinearColor Albedo = FLinearColor(0.44f, 0.41f, 0.35f);

	/** The material's Density (x every puff's opacity) and EdgePower (how fast a puff thins toward its rim). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Look", meta = (ClampMin = "0"))
	float Density = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Look", meta = (ClampMin = "0.1"))
	float EdgePower = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Silt|Look")
	FString Material = TEXT("/Game/Deep/Immersion/M_SiltPuff.M_SiltPuff");
};

/** The sonar's ping comes back off the walls it traced: delay and level by distance (Immersion/DeepImmersionAudio). */
USTRUCT(BlueprintType)
struct DEEP_API FDeepEchoTuning
{
	GENERATED_BODY()

	/**
	 * Seconds of echo delay per metre to the wall. Sound in water would come back from 10 m in 13 ms, too soon to hear
	 * as an echo; this stretches it so a tunnel answers at once and a cavern a long beat later.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo", meta = (ClampMin = "0"))
	float DelayPerM = 0.014f;

	/** The shortest delay (s): an echo always sits a little behind the ping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo", meta = (ClampMin = "0"))
	float MinDelayS = 0.06f;

	/** Level of a return from RefM away (x the sonar_echo row); farther walls fall off as (RefM / d)^Falloff. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo", meta = (ClampMin = "0", ClampMax = "1"))
	float NearGain = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo", meta = (ClampMin = "0.5"))
	float RefM = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo", meta = (ClampMin = "0"))
	float Falloff = 0.85f;

	/** Each echo is also as loud as the share of beams that came back (open water answers faintly). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo", meta = (ClampMin = "0", ClampMax = "1"))
	float MinReturnShare = 0.15f;

	/** Returns get darker with distance: low-pass at RefM and at FarM. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo", meta = (ClampMin = "100"))
	float NearLowPassHz = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo", meta = (ClampMin = "100"))
	float FarLowPassHz = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo", meta = (ClampMin = "1"))
	float FarM = 60.f;

	/** The echo's two returns: from the near walls (this quantile of the traced ranges) and the room's far side. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo", meta = (ClampMin = "0", ClampMax = "1"))
	float NearQuantile = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo", meta = (ClampMin = "0", ClampMax = "1"))
	float FarQuantile = 0.8f;

	/** The far return is this share as loud as the near one would be at its distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo", meta = (ClampMin = "0", ClampMax = "1"))
	float FarShare = 0.7f;

	/** A diver's ping (V) has no sweep to read: it traces this many level beams once, at the ping (not every frame). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo", meta = (ClampMin = "4", ClampMax = "32"))
	int32 DiverBeams = 12;

	/** Range of those beams (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echo", meta = (ClampMin = "5"))
	float DiverRangeM = 60.f;
};

/** Immersion I1 (D-072). Deep.Immersion 0 turns all of it off (A/B and the shaft test). */
USTRUCT(BlueprintType)
struct DEEP_API FDeepImmersionTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immersion")
	FDeepHullVoiceTuning Voice;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immersion")
	FDeepSiltTuning Silt;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Immersion")
	FDeepEchoTuning Echo;
};
