// D-070: what the pilot sees, as pure rules (World/DeepSubmarine applies them). The chase view sits on the hull's own
// trail, behind and above the stern, in water the hull has just swept clear; while the trail does not lie astern (a
// turn in place, going astern, a fresh start) it sits straight astern instead and the probe keeps it out of rock. It
// looks ahead along the travel. The mouse looks round within limits and the view eases back after a moment.
// Units: cm and degrees, world or one fixed frame throughout; Z is up.
#pragma once

#include "CoreMinimal.h"
#include "Sim/DeepTuningTypes.h"

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

	/** The look easing back toward ahead once the mouse has been still LookReturnDelayS. */
	DEEP_API FVector2D EaseLook(const FVector2D& LookDeg, float IdleS, float DeltaSeconds, const FDeepHelmViewTuning& T);

	/** The vertical field of view (deg) for a horizontal one and the frame's aspect (width over height). */
	DEEP_API float VerticalFovDeg(float HorizontalFovDeg, float Aspect);

	/**
	 * Where a point shows on the frame's vertical: 0 the centre, +1 the bottom edge, -1 the top (past +-1 it is off the
	 * frame). A point behind the camera is 10 (off the frame).
	 */
	DEEP_API float FrameY(const FVector& CameraCm, const FRotator& View, const FVector& PointCm, float HorizontalFovDeg, float Aspect);

	/**
	 * D1b: the chase view's pitch. LookPitchDeg (the look ahead), raised until HullTopCm shows HullTopFrameFrac of the half
	 * frame under the centre, so the passage shows over and past the hull; inside the look's pitch limits.
	 */
	DEEP_API float FramePitch(const FVector& CameraCm, const FVector& HullTopCm, float LookPitchDeg, float HorizontalFovDeg, const FDeepHelmViewTuning& T);
}
