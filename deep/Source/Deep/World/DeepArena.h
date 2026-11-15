// The arena builds itself: whatever a map lacks (sub, salvage, floor, water look) is spawned from tuning.
// Server only. Idempotent: calling it twice spawns nothing new.

#pragma once

#include "CoreMinimal.h"

class UWorld;
class ADeepSubmarine;
class ADeepSalvage;
struct FDeepSalvageKind;

namespace DeepArena
{
	DEEP_API ADeepSubmarine* FindSubmarine(const UWorld* World);

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
