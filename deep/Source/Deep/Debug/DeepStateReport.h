// The one-line machine-readable state used by tests and playtest agents: `DEEPSTATE {json}`.

#pragma once

#include "CoreMinimal.h"

class UWorld;
class ADeepDiver;

namespace DeepStateReport
{
	/** The diver to report on: the first local player's pawn, else the first diver in the world. */
	DEEP_API ADeepDiver* FindDiver(UWorld* World);

	/**
	 * Condensed JSON. Keys: diver_present, depth_m, zone, cargo_kg, cargo_value, cargo_items, overloaded,
	 * tether_length_m, tether_distance_m, tether_left_m, tether_strain, tether_taut, winching, home, lamp,
	 * run_state (tethered/cut/dead/settling/aboard/won), inside, seated, sub_depth_m, depth_limit_m, has_flashlight, hull_upgraded, parked_stop (0 driving), near_console, near_helm, patience (0..1), patience_s, bailout_air_s, last_bank {gross,cut,fees,net,hull_bought} (after a bank),
	 * last_event, captain_line, credits, hull_rating, stop, diver_number, on_title, won, run_s, run_totals {divers_expended,gross,cut,fees,kept,rating}, relic_in_world, relic_distance_m, carrying_relic,
	 * hull_damage, hull_repairs_owed, harpoon {shots, phase (idle/flying/latched), latch (none/sub/terrain), last (result), latch_cm [x,y,z]},
	 * salvage_in_world, pos_cm [x,y,z]. Positions are Unreal units (cm).
	 */
	DEEP_API FString BuildJson(UWorld* World);

	/** Logs `DEEPSTATE {json}` and returns the line. */
	DEEP_API FString LogState(UWorld* World);
}
