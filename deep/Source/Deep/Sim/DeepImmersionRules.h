// Immersion I1 (D-072) as pure rules, tested without a world: when the hull creaks and groans, how loud the water
// flows past, how silt lifts, spreads, hangs and keeps off the hull, and how the sonar's echo follows the walls.
// Numbers: FDeepImmersionTuning (Sim/DeepImmersionTuning.h). Actors: Immersion/DeepImmersionAudio, Immersion/DeepSilt.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Sim/DeepImmersionTuning.h"

/** What the creak clock reads each step. */
struct FDeepCreakInputs
{
	/** Read depth (m). */
	float DepthM = 0.f;
	/** How fast the hull turns (deg/s) and changes depth (m/s); either sign. */
	float YawRateDegS = 0.f;
	float DepthRateMps = 0.f;
	/** Parked: the room at rest. */
	bool bParked = false;
};

/** The creak clock's state, kept by the caller between steps. */
struct FDeepCreakClock
{
	/** Builds by the creak rate; a creak comes when it reaches Threshold. */
	float Charge = 0.f;
	float Threshold = 1.f;
	float SinceLastS = 1000.f;
	/** No creak until this runs out (after a groan). */
	float QuietS = 0.f;
};

/** One step's answer: a creak or not, and how it sounds. */
struct FDeepCreak
{
	bool bFire = false;
	/** Pitch and volume scales for the hull_creak row. */
	float Pitch = 1.f;
	float Volume = 1.f;
	/** The hull's work (a turn or a depth change) brought this one, not the depth alone. */
	bool bStress = false;
};

/** The flow loop's target from the speed. */
struct FDeepFlowLevel
{
	float Volume01 = 0.f;
	float Pitch = 1.f;
	float LowPassHz = 0.f;
};

/** One silt puff. Positions are in the frame the silt lives in (the arena root's), cm. */
struct FDeepSiltPuff
{
	FVector BirthCm = FVector::ZeroVector;
	/** The rock it was lifted off (a point on it): it never goes back into that surface. */
	FVector SurfaceCm = FVector::ZeroVector;
	/** The surface's normal at birth (unit): the puff rises along it. */
	FVector Normal = FVector::UpVector;
	/** Outward along the surface (unit, may be zero). */
	FVector Out = FVector::ZeroVector;
	/** The wander's direction (unit). */
	FVector Wander = FVector::ZeroVector;
	/** Where the hull has shoved it (cm, added to its path). */
	FVector PushCm = FVector::ZeroVector;
	float RiseMps = 0.4f;
	float StartRadiusM = 0.5f;
	float SpreadRadiusM = 2.f;
	float LifeS = 15.f;
	float AgeS = 0.f;
	/** Size share (a diver's puff is smaller). */
	float Scale = 1.f;
	/** Squeezed between the hull and the rock: drawn no bigger than this (cm); < 0 = free. Set each step. */
	float FitRadiusCm = -1.f;
};

/** Where a puff is and how it looks at its age. */
struct FDeepSiltPuffNow
{
	FVector LocationCm = FVector::ZeroVector;
	float RadiusCm = 0.f;
	float Opacity01 = 0.f;
};

/** A bounded set of puffs: never more than Max alive; a new one replaces the oldest. */
struct DEEP_API FDeepSiltPool
{
	TArray<FDeepSiltPuff> Puffs;

	/** Adds a puff (replacing the oldest when MaxPuffs are alive). Returns its index. */
	int32 Add(const FDeepSiltPuff& Puff, int32 MaxPuffs);
	/** Ages every puff and drops the ones past their life. */
	void Step(float DeltaSeconds);
	int32 Num() const { return Puffs.Num(); }
};

/** One return of the sonar's echo. */
struct FDeepEchoTap
{
	float DistanceM = 0.f;
	float DelayS = 0.f;
	/** Volume scale of the sonar_echo row. */
	float Gain = 0.f;
	float LowPassHz = 0.f;
};

namespace DeepImmersionRules
{
	// ---------------------------------------------------------------- the hull's voice

	/** 0 at the voice's ShallowM, 1 at DeepM. */
	DEEP_API float Depth01(float DepthM, const FDeepHullVoiceTuning& T);

	/** Mean creaks a second now: the depth's own rate (rarer at rest near the surface) plus the turn's and the depth change's. */
	DEEP_API float CreakRatePerS(const FDeepCreakInputs& In, const FDeepHullVoiceTuning& T);

	/**
	 * Advances the clock by DeltaS; fires a creak when the charge reaches its threshold and the last creak is at least
	 * MinGapS back (and no groan is ringing). RandA picks the next wait, RandB the pitch jitter (both 0..1).
	 */
	DEEP_API FDeepCreak StepCreak(FDeepCreakClock& Clock, const FDeepCreakInputs& In, float DeltaS, float RandA, float RandB, const FDeepHullVoiceTuning& T);

	/** The 100 m band a depth is in (0 above the first). */
	DEEP_API int32 GroanBand(float DepthM, const FDeepHullVoiceTuning& T);

	/**
	 * True once, the first time the depth reaches a band deeper than DeepestBand (which it then records). DeepestBand < 0 =
	 * not started: it is set to the depth's band with no groan (a run starting at 100 m does not groan at once).
	 */
	DEEP_API bool StepGroan(int32& DeepestBand, float DepthM, const FDeepHullVoiceTuning& T);

	/** The groan's pitch at a depth. */
	DEEP_API float GroanPitch(float DepthM, const FDeepHullVoiceTuning& T);

	/** The flow noise for the hull's speed: nothing at rest, rising and brightening with speed. */
	DEEP_API FDeepFlowLevel FlowLevel(float SpeedMps, const FDeepHullVoiceTuning& T);

	// ---------------------------------------------------------------- silt

	/** Puffs a second the hull lifts with its underside ClearanceM over the floor (< 0 = no floor found) at SpeedMps. */
	DEEP_API float LiftPuffsPerS(float ClearanceM, float SpeedMps, const FDeepSiltTuning& T);

	/** True when rock this far from a side of the hull counts as a scrape. */
	DEEP_API bool IsScrape(float GapM, const FDeepSiltTuning& T);

	/** Puffs a knock into the rock at ImpactMps bursts. */
	DEEP_API int32 KnockPuffs(float ImpactMps, const FDeepSiltTuning& T);

	/** True when a swimming diver this far over the floor, this fast, kicks silt up. */
	DEEP_API bool DiverLifts(float ClearanceM, float SpeedMps, const FDeepSiltTuning& T);

	/** A new puff at a surface point (cm) with its normal, on the surface's open side. Rand drives its spread. */
	DEEP_API FDeepSiltPuff MakePuff(const FVector& HitCm, const FVector& Normal, float Scale, FRandomStream& Rand, const FDeepSiltTuning& T);

	/** The puff at its age: drifted, spread, faded. */
	DEEP_API FDeepSiltPuffNow PuffAt(const FDeepSiltPuff& Puff, const FDeepSiltTuning& T);

	/** True when a sphere (local cm) of RadiusCm reaches into Box. */
	DEEP_API bool TouchesBox(const FVector& PointCm, float RadiusCm, const FBox& Box);

	/**
	 * Where a sphere's centre must move (local cm) so that only its outer (1 - CoreShare) reaches into Box: out through
	 * the nearest face, but never to within MinAboveCm of the surface it rose from (the rock under or beside it). The
	 * input when it is already clear or has nowhere to go (then FitRadiusCm squeezes it). The hull shoves silt aside.
	 */
	DEEP_API FVector KeepOffBox(const FVector& PointCm, float RadiusCm, float CoreShare, const FBox& Box, const FVector& SurfaceCm, const FVector& SurfaceNormal, float MinAboveCm);

	/** The largest radius a sphere at PointCm can have with CoreShare of it clear of Box (0 with its centre inside). */
	DEEP_API float FitRadiusCm(const FVector& PointCm, float CoreShare, const FBox& Box);

	// ---------------------------------------------------------------- the sonar's echo

	/**
	 * The ping's returns from the ranges its beams traced (m; < 0 = nothing within range): a near return (the close walls)
	 * and a far one (the room's far side), each delayed by its distance, fainter and darker the farther it is, and
	 * quieter when few beams came back. Empty when nothing returned.
	 */
	DEEP_API TArray<FDeepEchoTap> EchoTaps(const TArray<float>& RangesM, const FDeepEchoTuning& T);

	/** One tap at a distance, with GainScale applied (the return share, the far share). */
	DEEP_API FDeepEchoTap EchoTap(float DistanceM, float GainScale, const FDeepEchoTuning& T);
}
