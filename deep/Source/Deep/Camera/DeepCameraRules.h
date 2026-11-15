// Pure camera rules: which shake an event plays, the eased envelope every shake follows, the footage look's grain by
// depth, the sonar pulse's shell and the interference strength. Numbers come from FDeepCameraTuning.

#pragma once

#include "CoreMinimal.h"
#include "Sim/DeepTuningTypes.h"

namespace DeepCameraRules
{
	/** The shake an event plays (scaled by ShakeScale), or a none spec (IsNone) for events that do not shake. */
	DEEP_API FDeepShakeSpec ShakeForEvent(FName Event, const FDeepCameraTuning& Tuning);

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

	/** The pulse shell's radius (cm) at AgeS: grows linearly from 0 to RangeM over PulseSeconds, then holds. */
	DEEP_API float PulseRadiusCm(float AgeS, float RangeM, const FDeepCameraTuning& Tuning);

	/** The pulse is drawn until the band has reached its range and the last dots have faded. */
	DEEP_API bool IsPulseLive(float AgeS, const FDeepCameraTuning& Tuning);

	/** Interference target 0..1: 0 beyond the far distance, NearMax at the near one, 1 in a bite, at least the burst. */
	DEEP_API float InterferenceTarget(float SerpentDistanceM, bool bBiting, float BurstLeftS, const FDeepCameraTuning& Tuning);

	/** One step of the shown strength toward Target: rises four times faster than it falls, so a hit reads at once. */
	DEEP_API float InterferenceStep(float Current, float Target, float DeltaSeconds, const FDeepCameraTuning& Tuning);
}
