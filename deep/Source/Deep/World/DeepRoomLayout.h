// The room set's inside as pure boxes (room-local cm: X ahead, Y starboard, Z up, the hatch's centre at X 0 Y 0, the
// deck's top at -InsideHeightM / 2). ADeepSubRoom builds its shell and fixtures from these, DeepRoomDress keeps clear of
// them, UDeepHelmLevers stands on the deck, and Deep.World.SubRoomLayout checks walks, reach, passing and headroom.
// Fitted to BT-01's cabin (Content/Deep/Room/measure_bt01_inside.py, Nov 17).

#pragma once

#include "CoreMinimal.h"

struct FDeepSubTuning;
struct FDeepPilotTuning;

namespace DeepRoomLayout
{
	/** The walls' inner faces, deck top to ceiling. */
	DEEP_API FBox Inside(const FDeepSubTuning& T);
	DEEP_API double DeckTopCm(const FDeepSubTuning& T);
	DEEP_API double HatchRadiusCm(const FDeepSubTuning& T);

	/** Solid fixtures a walker cannot enter. */
	DEEP_API FBox Console(const FDeepSubTuning& T);
	DEEP_API FBox HelmPanel(const FDeepSubTuning& T);
	/** The helm seat (not solid: the pilot sits in it). */
	DEEP_API FBox HelmSeat(const FDeepSubTuning& T);
	/** A helm lever's reach on the deck: its base and the arm's full throw either way. */
	DEEP_API FBox Lever(const FVector2D& AtM, const FDeepSubTuning& T);
	DEEP_API TArray<FBox> Levers(const FDeepSubTuning& T, const FDeepPilotTuning& P);

	/** The porthole's opening in the front wall (X is the wall's inner face). */
	DEEP_API FBox Porthole(const FDeepSubTuning& T);

	/** The foot of the post that carries the hatch's red lamp, on the deck by the port wall beside the hatch. */
	DEEP_API FVector HatchLampPost(const FDeepSubTuning& T);
}
