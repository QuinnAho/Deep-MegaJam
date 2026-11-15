// Pure rules for the water effects (bubbles and silt), tested without a world. The subsystem gathers the diver's
// state, asks these what to spawn, and spawns it. Numbers are in UDeepFxSettings (Project Settings > Game > DEEP FX).

#pragma once

#include "CoreMinimal.h"

class UDeepFxSettings;

namespace DeepFxRules
{
	/** The breath cycle in seconds: the calm loop or the fast one (the same periods DeepSynth's breath loops use). */
	DEEP_API float BreathPeriodS(bool bFast, const UDeepFxSettings& Settings);

	/** 0..1 through the breath cycle, PlaySeconds into a loop of PeriodS. */
	DEEP_API float BreathPhase(float PlaySeconds, float PeriodS);

	/** How many puff phases lie in (PrevPhase, Phase], wrapping once past 1 (a new cycle). Each one is a burst. */
	DEEP_API int32 PuffsCrossed(float PrevPhase, float Phase, const TArray<float>& PuffPhases);

	/** The puff phases crossed, in order (indices into PuffPhases); the first of a breath out is the biggest. */
	DEEP_API TArray<int32> PuffIndicesCrossed(float PrevPhase, float Phase, const TArray<float>& PuffPhases);

	/** Bubbles in the burst for puff Index of a breath out: the first is the full breath, later ones trail off. */
	DEEP_API int32 PuffCount(int32 Index, const UDeepFxSettings& Settings);

	/** How much of the diver's own light a bubble rim catches: the lamp, else the hatch light by distance, else the
	 *  blue water's ambient floor by depth; 0 in black water with the lamp off. */
	DEEP_API float Spill(bool bLampOn, float HatchDistanceM, float DepthM, const UDeepFxSettings& Settings);

	/** Whether a diver this close above the bed, moving this fast, kicks up silt (and not again within the cooldown). */
	DEEP_API bool KicksSilt(float HeightAboveBedCm, float SpeedCmS, float SecondsSinceLastPuff, const UDeepFxSettings& Settings);
}
