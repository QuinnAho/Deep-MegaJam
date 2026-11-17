// Everything outside the submarine hangs from this one actor: salvage, the relic, the abyss floor (D-022).
// The sub never moves; driving down moves this root up by the same distance (DeepArena::ApplySubDepth).
// Deep.World.Still turns that round: this root stays at identity and the hull moves (DeepArena::IsWorldStill).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeepArenaRoot.generated.h"

UCLASS()
class DEEP_API ADeepArenaRoot : public AActor
{
	GENERATED_BODY()

public:
	ADeepArenaRoot();

	/**
	 * Deep.World.Still (server): the stop whose frame the salvage hung from this root was placed in (0 = none yet). When
	 * the loaded stop changes without a respawn (Deep.GotoStop), the salvage moves to the new stop's frame, as it rides
	 * the re-based root under D-022.
	 */
	int32 StillContentStop = 0;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<USceneComponent> Root;
};
