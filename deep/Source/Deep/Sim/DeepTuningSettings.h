// Every DEEP design number. Edit in Project Settings > Game > DEEP Tuning, saved to Config/DefaultGame.ini.
// Gameplay code reads numbers only from here and passes the relevant struct to a rule in UDeepRules.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Sim/DeepTuningTypes.h"
#include "DeepTuningSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "DEEP Tuning"))
class DEEP_API UDeepTuningSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDeepTuningSettings();

	static const UDeepTuningSettings& Get() { return *GetDefault<UDeepTuningSettings>(); }

	/** Stop is 1-based; clamped into the table. Returns defaults if the table is empty. */
	FDeepStopTuning GetStop(int32 Stop) const;
	int32 NumStops() const { return Stops.Num(); }

	/** Returns false (and leaves Out untouched) if no kind of that size exists. */
	bool FindSalvageKind(EDeepSalvageSize Size, FDeepSalvageKind& Out) const;

	UPROPERTY(Config, EditAnywhere, Category = "Depth")
	TArray<FDeepZone> Zones;

	UPROPERTY(Config, EditAnywhere, Category = "Depth")
	TArray<FDeepStopTuning> Stops;

	UPROPERTY(Config, EditAnywhere, Category = "Tether")
	FDeepTetherTuning Tether;

	UPROPERTY(Config, EditAnywhere, Category = "Tether")
	FDeepRopeTuning Rope;

	UPROPERTY(Config, EditAnywhere, Category = "Diver")
	FDeepWeightTuning Weight;

	UPROPERTY(Config, EditAnywhere, Category = "Diver")
	FDeepSwimTuning Swim;

	UPROPERTY(Config, EditAnywhere, Category = "Diver")
	FDeepLampTuning Lamp;

	/** The sea at each depth, the marine snow, the sub's flood and the hatch water (style guide 5). */
	UPROPERTY(Config, EditAnywhere, Category = "Look")
	FDeepLookTuning Look;

	UPROPERTY(Config, EditAnywhere, Category = "Director")
	FDeepPatienceTuning Patience;

	UPROPERTY(Config, EditAnywhere, Category = "Economy")
	FDeepEconomyTuning Economy;

	UPROPERTY(Config, EditAnywhere, Category = "Bailout")
	FDeepBailoutTuning Bailout;

	UPROPERTY(Config, EditAnywhere, Category = "Salvage")
	TArray<FDeepSalvageKind> SalvageKinds;

	UPROPERTY(Config, EditAnywhere, Category = "Arena")
	FDeepArenaTuning Arena;

	UPROPERTY(Config, EditAnywhere, Category = "Run")
	FDeepRunTuning Run;

	UPROPERTY(Config, EditAnywhere, Category = "Submarine")
	FDeepSubTuning Sub;

	/** The cave shaft between stops: its shape, the helm's steering and the wall's hits (Sim/DeepCaveRules). */
	UPROPERTY(Config, EditAnywhere, Category = "Cave")
	FDeepCaveTuning Cave;

	/** Displayed depth of each stop (Stops[i].SubDepthM), shallowest first. */
	TArray<float> StopDepthsM() const;

	UPROPERTY(Config, EditAnywhere, Category = "Director")
	FDeepCaptainTuning Captain;

	UPROPERTY(Config, EditAnywhere, Category = "Director")
	FDeepReviewTuning Review;
};
