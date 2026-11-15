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
	 * Returns the DEEP game state of the running Play In Editor world as a JSON string. Fails when PIE is not running.
	 * @return JSON from DeepStateReport::BuildJson: diver_present, depth_m, zone, cargo_kg, cargo_value, cargo_items, overloaded, tether_*, winching, home, lamp, credits, hull_rating, stop, diver_number, salvage_in_world, pos_cm.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"), Category = "Deep")
	static FString GetState();

	/**
	 * Runs one console command in the Play In Editor world (Deep.GotoStop 2, Deep.GiveSalvage 6 70, Deep.Teleport x y z, any cvar) and returns the game state JSON afterwards. Fails when PIE is not running.
	 * @param Command One console command line.
	 * @return The same JSON as GetState, read after the command ran.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Execute"), Category = "Deep")
	static FString Exec(const FString& Command);

	/**
	 * Returns where things are in the PIE world as JSON: {submarine:[x,y,z] or null, anchor_cm:[x,y,z] or null, salvage:[{pos_cm:[x,y,z], kg, name}]}. Positions in cm. Fails when PIE is not running.
	 * @return JSON object with submarine, anchor_cm and salvage.
	 */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"), Category = "Deep")
	static FString GetArena();
};
