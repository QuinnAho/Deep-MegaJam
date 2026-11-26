// D2 (Nov 25): BT-01's cabin as pure rules. A bow port's pane from traces against the model's Glass slot (FitPane), its
// mirror on the other side, the box that cuts both out of the model's own glass, and the transform that lays the engine
// cylinder into a port as its pane. Units: cm, the sub's frame (x ahead, y starboard, z up).
#pragma once

#include "CoreMinimal.h"
#include "Sim/DeepCabinTuning.h"

/** One port's pane: its centre, its outward normal (unit) and its radius. */
struct FDeepPortPane
{
	FVector CentreCm = FVector::ZeroVector;
	FVector Normal = FVector::ForwardVector;
	float RadiusCm = 0.f;
	bool bValid = false;
};

namespace DeepCabin
{
	/**
	 * A pane from the points where traces met the glass and the glass's normals there: the centre is the points' middle,
	 * the normal their mean (turned to point along Outward), the radius the farthest point's distance in the pane's plane.
	 * Invalid with fewer than 8 points.
	 */
	DEEP_API FDeepPortPane FitPane(const TArray<FVector>& PointsCm, const TArray<FVector>& Normals, const FVector& Outward);

	/** The tuned starboard pane and its mirror (y negated) to port: index 0 port, 1 starboard. */
	DEEP_API TArray<FDeepPortPane> TunedPanes(const FDeepCabinTuning& T);

	/** The box round every pane (each disc PortThicknessCm thick), grown by PortCutMarginCm. */
	DEEP_API FBox CutBox(const TArray<FDeepPortPane>& Panes, const FDeepCabinTuning& T);

	/** The engine cylinder (100 cm across, 100 cm tall, its axis on local Z) laid into a pane: its axis on the normal. */
	DEEP_API FTransform PaneTransform(const FDeepPortPane& Pane, const FDeepCabinTuning& T);

	/** A box in one frame as the axis-aligned box round it in another (Into: from the first frame to the second). */
	DEEP_API FBox BoxInto(const FBox& Box, const FTransform& Into);
}
