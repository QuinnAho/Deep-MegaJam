// Every sea serpent number. Project Settings > Game > DEEP Monster, saved to Config/DefaultGame.ini.
// Defaults are in code (the struct initialisers and the constructor). Stop 3 is meaner through Stops only.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Monster/DeepMonsterTypes.h"
#include "DeepMonsterSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "DEEP Monster"))
class DEEP_API UDeepMonsterSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDeepMonsterSettings();

	static const UDeepMonsterSettings& Get() { return *GetDefault<UDeepMonsterSettings>(); }

	/** Behaviour with the overrides for this stop (1-based) applied. */
	FDeepMonsterTuning GetTuning(int32 Stop) const;

	/** The brain spawns the serpent automatically at this stop and deeper. */
	UPROPERTY(Config, EditAnywhere, Category = "Spawn")
	int32 FirstStop = 2;

	UPROPERTY(Config, EditAnywhere, Category = "Behaviour")
	FDeepMonsterTuning Behaviour;

	/** Per-stop overrides (cooldown, approach speed). */
	UPROPERTY(Config, EditAnywhere, Category = "Behaviour")
	TArray<FDeepMonsterStopTuning> Stops;

	UPROPERTY(Config, EditAnywhere, Category = "Body")
	FDeepSerpentBodyTuning Body;
};
