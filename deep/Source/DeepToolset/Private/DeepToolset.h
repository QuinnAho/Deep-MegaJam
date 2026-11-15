#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "DeepToolset.generated.h"

/** DEEP's own MCP toolset. Shows up in list_toolsets as DeepToolset.DeepToolset. */
UCLASS(BlueprintType)
class UDeepToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Returns a JSON string describing the running Play In Editor game.
	 * @return JSON object with pieRunning, world, actorCount and timeSeconds.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"), Category = "Deep")
	static FString GetState();
};
