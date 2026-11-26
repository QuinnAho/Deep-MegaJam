// D-070: what the pilot sees, as pure rules (World/DeepSubmarine applies them). The chase view sits on the hull's own
// trail, behind and above the stern, in water the hull has just swept clear; while the trail does not lie astern (a
// turn in place, going astern, a fresh start) it sits straight astern instead and the probe keeps it out of rock. It
// looks ahead along the travel. The mouse looks round within limits and the view eases back after a moment.
// Units: cm and degrees, world or one fixed frame throughout; Z is up.
#pragma once

#include "CoreMinimal.h"
#include "Sim/DeepTuningTypes.h"

/** D2: the pilot's view on its servo: where it points (deg off ahead, x yaw + starboard, y pitch + up) and how fast it turns. */
struct FDeepServo
{
	FVector2D AngleDeg = FVector2D::ZeroVector;
	FVector2D RateDegS = FVector2D::ZeroVector;
};

namespace DeepHelmView
{
	/**
	 * The point BackCm of path behind FromCm, walking the trail from its newest point (last) to its oldest. False (and
	 * Out = the oldest point) when the trail is shorter than that.
	 */
	DEEP_API bool PointBack(const TArray<FVector>& Trail, const FVector& FromCm, double BackCm, FVector& OutCm);

	/** Adds the hull's centre to the trail when it has moved TrailStepM from the newest point; keeps TrailMaxPoints. */
	DEEP_API void Record(TArray<FVector>& Trail, const FVector& HullCm, const FDeepHelmViewTuning& T);

	/**
	 * Where the chase view wants to be: on the trail ChaseBackM behind the hull and UpM over it (World: ChaseUpM, lower where the roof comes down), while that point
	 * lies within TrailMaxOffHeadingDeg of straight astern (Ahead: the hull's heading, level); else straight astern.
	 */
	DEEP_API FVector ChaseAim(const TArray<FVector>& Trail, const FVector& HullCm, const FVector& Ahead, float UpM, const FDeepHelmViewTuning& T);

	/** The travel to look along: the velocity when faster than TravelMinMps, else the heading (level). Unit length. */
	DEEP_API FVector Travel(const FVector& VelocityMps, const FVector& Ahead, const FDeepHelmViewTuning& T);

	/** The chase view's rotation: from the camera at a point LookAheadM along the travel from the hull, pitch clamped. */
	DEEP_API FRotator ChaseLook(const FVector& CameraCm, const FVector& HullCm, const FVector& TravelDir, const FDeepHelmViewTuning& T);

	/** The mouse's look after a move (degrees, + right, + up), inside the view's limits. */
	DEEP_API FVector2D AddLook(const FVector2D& LookDeg, float YawDeg, float PitchDeg, bool bCockpit, const FDeepHelmViewTuning& T);

	/**
	 * D2: the servo that turns the view toward the aim (deg, x yaw, y pitch), each axis on its own: a spring of
	 * ServoStiffnessPerS damped at ServoDamping, its push capped at ServoAccelDegS2 and its speed at ServoRateDegS, so a
	 * big move speeds up, cruises and slows like a motor and overshoots a little; once within ServoHoldDeg of the aim and
	 * slower than ServoHoldRateDegS it stops on the aim and holds (no creep). Steps in slices of at most 1/120 s.
	 */
	DEEP_API FDeepServo StepServo(const FDeepServo& In, const FVector2D& AimDeg, float DeltaSeconds, const FDeepHelmViewTuning& T);

	/** The servo's speed as a share of its cap, 0..1 (the faster axis): what the whine follows. */
	DEEP_API float ServoSpeed01(const FDeepServo& Servo, const FDeepHelmViewTuning& T);

	/** The vertical field of view (deg) for a horizontal one and the frame's aspect (width over height). */
	DEEP_API float VerticalFovDeg(float HorizontalFovDeg, float Aspect);

	/**
	 * Where a point shows on the frame's vertical: 0 the centre, +1 the bottom edge, -1 the top (past +-1 it is off the
	 * frame). A point behind the camera is 10 (off the frame).
	 */
	DEEP_API float FrameY(const FVector& CameraCm, const FRotator& View, const FVector& PointCm, float HorizontalFovDeg, float Aspect);

	/**
	 * D1b: the chase view's pitch. LookPitchDeg (the look ahead), raised until HullTopCm shows HullTopFrameFrac of the half
	 * frame under the centre, so the passage shows over and past the hull; never so high that the hull's top falls under
	 * HullTopMaxFrameFrac (D1c); inside the look's pitch limits. Pass Heaved(T, ...) while the hull heaves.
	 */
	DEEP_API float FramePitch(const FVector& CameraCm, const FVector& HullTopCm, float LookPitchDeg, float HorizontalFovDeg, const FDeepHelmViewTuning& T);

	/** D1c: how much the hull heaves, from its travel's pitch (deg, + up): -1 sinking at HeaveFullDeg or steeper, +1 rising, 0 level. */
	DEEP_API float HeaveShare(float TravelPitchDeg, const FDeepHelmViewTuning& T);

	/**
	 * D1c: the chase tuning for a heave share (HeaveShare): sinking, the place moves toward SinkBackM astern and SinkUpM
	 * over the hull, the look may pitch down to SinkMinPitchDeg and the hull's top may rise to SinkHullTopFrac; rising,
	 * toward RiseBackM and RiseUpM, and the look may pitch up to RiseMaxPitchDeg. 0 returns T unchanged.
	 */
	DEEP_API FDeepHelmViewTuning Heaved(const FDeepHelmViewTuning& T, float Heave);
}
