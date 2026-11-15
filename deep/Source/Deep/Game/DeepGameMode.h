// Server rules for a dive. Builds the arena if the map has none and spawns divers under the sub's airlock.

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

	/** Moves the sub to a stop (clamped to the stop table): new light and fog, longer tether. */
	UFUNCTION(BlueprintCallable, Category = "Deep")
	void GotoStop(int32 Stop);

	/** Builds whatever the map is missing. Safe to call repeatedly. */
	ADeepSubmarine* EnsureArena();

	virtual void InitGameState() override;
	virtual void StartPlay() override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void SetPlayerDefaults(APawn* PlayerPawn) override;
};
