// Everything outside the submarine hangs from this one actor: salvage, the relic, the abyss floor (D-022).
// The sub never moves; driving down moves this root up by the same distance (DeepArena::ApplySubDepth).

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

protected:
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<USceneComponent> Root;
};
