// The tether as the diver's air line (rescope, Nov 15). Clipped on, air is unlimited; off it (bitten, cut, unclipped)
// the bailout air runs. The serpent bites the line where its head touches the rope. Pure: no world, tests drive it.

#pragma once

#include "CoreMinimal.h"
#include "Monster/DeepMonsterTypes.h"
#include "Sim/DeepTuningTypes.h"

namespace DeepAirLine
{
	/** The serpent's head touched the rope: the line is cut (camera tug, snap). */
	inline const TCHAR* TetherBitten = TEXT("tether_bitten");
	/** The diver unclipped the line by choice: bailout air, re-clip at the hatch for free. */
	inline const TCHAR* Unclip = TEXT("tether_unclip");

	/** The Captain's Patience after a step: drains only when the tuning says so (the old rule), else it holds. */
	DEEP_API float PatienceAfterStep(float Before, float DeltaSeconds, bool bHunted, const FDeepPatienceTuning& Patience, const FDeepAirLineTuning& AirLine);

	/** The serpent in this state bites a line its head touches. */
	DEEP_API bool StateBitesLine(EDeepMonsterState State, const FDeepAirLineTuning& AirLine);

	/**
	 * The first joined link of a rope (points in cm; Linked[i] joins point i and i + 1, empty = all joined) whose segment
	 * passes within TouchM of the head. INDEX_NONE if none does.
	 */
	DEEP_API int32 TouchedLink(TConstArrayView<FVector> Points, TConstArrayView<uint8> Linked, const FVector& HeadCm, float TouchM);

	/** Credits to clip back on at the hatch: the re-tether fee after a cut, the unclip fee after a voluntary unclip. */
	DEEP_API int32 ReclipFee(bool bUnclipped, const FDeepEconomyTuning& Economy, const FDeepAirLineTuning& AirLine);
}
