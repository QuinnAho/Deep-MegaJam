// Pure camera rules: which shake an event plays, the eased envelope every shake follows, the footage look's grain by
// depth, the sonar pulse's shell and the interference strength. Numbers come from FDeepCameraTuning.

#pragma once

#include "CoreMinimal.h"
#include "Sim/DeepTuningTypes.h"

namespace DeepCameraRules
{
	/** The shake an event plays (scaled by ShakeScale), or a none spec (IsNone) for events that do not shake. */
	DEEP_API FDeepShakeSpec ShakeForEvent(FName Event, const FDeepCameraTuning& Tuning);

	/**
	 * The shake for a hull event sized by the hit: ShakeForEvent scaled by clamp(ImpactMps / HitShakeRefMps,
	 * HitShakeScaleRange). ImpactMps <= 0 (a breach, an event with no impact) plays the row as written.
	 */
	DEEP_API FDeepShakeSpec ShakeForHit(FName Event, float ImpactMps, const FDeepCameraTuning& Tuning);

	/**
	 * The sonar pulse's hull mask. PartsLocal is the box round the sub's drawn parts in the sub's space (cm). Out: the
	 * box centre in world space and its three axes divided by their half extents (grown by PulseHullMarginM), so a world
	 * point p is inside when every |dot(p - Centre, Axis)| < 1. This is the test M_SonarPulse runs per pixel.
	 */
	DEEP_API void PulseHullBox(const FBox& PartsLocal, const FTransform& SubToWorld, float MarginM,
		FVector& OutCentre, FVector& OutAxisX, FVector& OutAxisY, FVector& OutAxisZ);

	/** The same inside test the material runs (for tests and debugging). */
	DEEP_API bool IsInPulseHull(const FVector& WorldCm, const FVector& Centre, const FVector& AxisX, const FVector& AxisY, const FVector& AxisZ);

	/** Smoothstep 0..1: eases in and out, no overshoot. */
	DEEP_API float Ease01(float X);

	/**
	 * Envelope weight 0..1 at ElapsedS. Eases in over BlendInS, eases out over BlendOutS before the end. The end is
	 * DurationS (if > 0), or StopAtS + BlendOutS once stopped (StopAtS >= 0), whichever comes first.
	 */
	DEEP_API float Envelope(float ElapsedS, const FDeepShakeSpec& Spec, float StopAtS = -1.f);

	/** True once the envelope has reached its end (never for an unstopped endless shake). */
	DEEP_API bool IsShakeDone(float ElapsedS, const FDeepShakeSpec& Spec, float StopAtS = -1.f);

	/** Footage grain on top of the depth look: X = intensity add, Y = shadows intensity, blended by depth. */
	DEEP_API FVector2D FootageGrain(float DepthM, const FDeepCameraTuning& Tuning);

	/** The pulse look for a sonar style: 0 = PulseJam, anything else = PulseSoft. */
	DEEP_API const FDeepPulseLook& PulseLook(const FDeepCameraTuning& Tuning, int32 Style);

	/** How far the helm's pulse reaches (m): the look's HelmRangeM, else the sonar's range, else PulseRangeM. */
	DEEP_API float HelmPulseRangeM(const FDeepPulseLook& Look, float SonarRangeM, const FDeepCameraTuning& Tuning);

	/** The pulse shell's radius (cm) at AgeS: grows linearly from 0 to RangeM over Look.Seconds, then holds. */
	DEEP_API float PulseRadiusCm(float AgeS, float RangeM, const FDeepPulseLook& Look);

	/** The pulse is drawn until the band has reached its range and the last dots have faded. */
	DEEP_API bool IsPulseLive(float AgeS, const FDeepPulseLook& Look);

	/** Brightness left at DistanceM from the origin: 1 - RangeFade * (d / range)^2 (mirrors M_SonarPulse). */
	DEEP_API float PulseRangeFall(float DistanceM, float RangeM, const FDeepPulseLook& Look);

	/** The diver may ping now: in the water (bInWater), and the first ping (LastPingS < 0) or DiverPingCooldownS since. */
	DEEP_API bool CanDiverPing(float NowS, float LastPingS, bool bInWater, const FDeepCameraTuning& Tuning);

	/** Seconds until the diver may ping again (0 = ready). */
	DEEP_API float DiverPingWaitS(float NowS, float LastPingS, const FDeepCameraTuning& Tuning);

	/** The band's trail (cm) on a surface EyeCm from the eye: Look.BandM, growing with distance past PulseBandFarM so
	 * it keeps its width on screen (mirrors M_SonarPulse). */
	DEEP_API float PulseBandCm(float EyeCm, const FDeepPulseLook& Look, const FDeepCameraTuning& Tuning);

	/** The fine lidar grid's spacing (cm) on a surface EyeCm from the eye: Look.DotSpacingM, doubled each time the
	 * distance doubles past PulseDotNearM (mirrors M_SonarPulse). */
	DEEP_API float PulseDotSpacingCm(float EyeCm, const FDeepPulseLook& Look, const FDeepCameraTuning& Tuning);

	/** Interference target 0..1: 0 beyond the far distance, NearMax at the near one, 1 in a bite, at least the burst. */
	DEEP_API float InterferenceTarget(float SerpentDistanceM, bool bBiting, float BurstLeftS, const FDeepCameraTuning& Tuning);

	/** One step of the shown strength toward Target: rises four times faster than it falls, so a hit reads at once. */
	DEEP_API float InterferenceStep(float Current, float Target, float DeltaSeconds, const FDeepCameraTuning& Tuning);
}
