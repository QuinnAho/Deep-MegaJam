// Server rules for a dive. Builds the arena if the map has none and spawns divers under the sub's airlock.
// Owns the run's frame: the title screen before the first dive, starting the run, and restarting it.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DeepGameMode.generated.h"

class ADeepSubmarine;

UCLASS()
class DEEP_API ADeepGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADeepGameMode();

	/** Moves the sub to a stop (clamped to the stop table): new light and fog, longer tether, the relic at stop 3. */
	UFUNCTION(BlueprintCallable, Category = "Deep")
	void GotoStop(int32 Stop);

	/** Builds whatever the map is missing. Safe to call repeatedly. */
	ADeepSubmarine* EnsureArena();

	/** Resets the run and shows the title screen: nobody dives until StartRun. Fires the Captain's `title`. */
	void EnterTitle();

	/** Leaves the title screen: the first dive starts (dive_start). Does nothing if the title is not up. */
	void StartRun();

	/** Everything back to the start (credits, hull, stop, diver number, cargo, Patience, the serpent) and a fresh dive. */
	void RestartRun();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void SetPlayerDefaults(APawn* PlayerPawn) override;

protected:
	/** Set by InitGame (a real map load) unless -DeepNoTitle is on the command line. Test worlds start diving at once. */
	bool bStartOnTitle = false;
};
