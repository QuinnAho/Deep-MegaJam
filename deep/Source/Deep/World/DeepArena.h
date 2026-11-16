// The arena builds itself: whatever a map lacks (sub, salvage, floor, water look) is spawned from tuning.
// Server only. Idempotent: calling it twice spawns nothing new.

#pragma once

#include "CoreMinimal.h"

class UWorld;
class ADeepSubmarine;
class ADeepSubRoom;
class ADeepArenaRoot;
class ADeepSalvage;
class ADeepAbyssFloor;
struct FDeepSalvageKind;

namespace DeepArena
{
	DEEP_API ADeepSubmarine* FindSubmarine(const UWorld* World);

	/** The sub's room set (World/DeepSubRoom), or null. */
	DEEP_API ADeepSubRoom* FindRoom(const UWorld* World);

	/** Finds or spawns the room set at Sub.RoomSetLocationM. Every machine, server and client: it is not replicated. */
	DEEP_API ADeepSubRoom* EnsureRoom(UWorld* World);

	/** The one actor everything outside the sub hangs from. */
	DEEP_API ADeepArenaRoot* FindArenaRoot(const UWorld* World);

	/**
	 * Deep.World.Still (CVar, default 0), latched on the game state when the arena is first built: the arena root stays
	 * at identity and the hull moves through a still world instead (moving-sub step 3). False with the flag off.
	 */
	DEEP_API bool IsWorldStill(const UWorld* World);

	/** Deep.World.Still: where the root sits relative to a stop's content, as if parked there (the hull's home over that stop's depth). */
	DEEP_API FTransform StillStopFrame(const UWorld* World, int32 Stop);

	/** The bed of the loaded stop (still: the floor whose BedStop is the game state's Stop), else the only one. */
	DEEP_API ADeepAbyssFloor* FindLoadedFloor(const UWorld* World);

	/** Deep.World.Still, every machine: shows each stop's bed, cave shell and shaft stretch for the hull's depth. No-op with the flag off. */
	DEEP_API void UpdateStillWorld(UWorld* World);

	/** The tether anchor as the arena sees it: the sub's anchor shifted by the arena root's offset (equal when parked). */
	DEEP_API FVector ArenaAnchor(const UWorld* World);

	/**
	 * After the game state's SubDepthM, SubOffsetM or bParked changed: the arena root moves by (SubDepthM - stop depth)
	 * up and by SubOffsetM sideways, the nearest stop's arena is loaded when it changes (light, relic, salvage), and the
	 * hatch opens only while parked on the stop's centre. Server only.
	 */
	DEEP_API void ApplySubDepth(UWorld* World);

	/** Only the root (and the cave shaft) to the game state's depth and offset: for a sideways step. Server only. */
	DEEP_API void MoveRoot(UWorld* World);

	/** Ensures sub, salvage ring, abyss floor and environment exist. Returns the sub (null on clients). */
	DEEP_API ADeepSubmarine* EnsureArena(UWorld* World, int32 Stop);

	/** Spawns one salvage actor of the given kind. Server only. */
	DEEP_API ADeepSalvage* SpawnSalvage(UWorld* World, const FDeepSalvageKind& Kind, const FVector& LocationCm);

	/**
	 * Destroys every salvage piece and spawns the tuned ring again (start of every dive), plus the relic at
	 * Arena.RelicStop and deeper (the stop is read from the game state). Returns pieces spawned.
	 */
	DEEP_API int32 RespawnSalvage(UWorld* World);

	/** Applies the stop's light and fog to the world's environment actor, and adds or removes the relic for the stop. */
	DEEP_API void ApplyStop(UWorld* World, int32 Stop);

	/** The relic in the world, or null. */
	DEEP_API ADeepSalvage* FindRelic(const UWorld* World);

	/** Spawns the relic if the stop has one and none exists; removes it above Arena.RelicStop. Server only. */
	DEEP_API ADeepSalvage* EnsureRelic(UWorld* World, int32 Stop);
}
