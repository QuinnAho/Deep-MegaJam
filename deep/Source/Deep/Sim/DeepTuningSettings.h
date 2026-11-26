// Every DEEP design number. Edit in Project Settings > Game > DEEP Tuning, saved to Config/DefaultGame.ini.
// Gameplay code reads numbers only from here and passes the relevant struct to a rule in UDeepRules.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Sim/DeepTuningTypes.h"
#include "Sim/DeepImmersionTuning.h"
#include "Sim/DeepCabinTuning.h"
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

	/** Rock walls and a roof round each stop's arena (ADeepCavern). */
	UPROPERTY(Config, EditAnywhere, Category = "Look")
	FDeepCavernTuning Cavern;

	UPROPERTY(Config, EditAnywhere, Category = "Director")
	FDeepPatienceTuning Patience;

	UPROPERTY(Config, EditAnywhere, Category = "Economy")
	FDeepEconomyTuning Economy;

	UPROPERTY(Config, EditAnywhere, Category = "Bailout")
	FDeepBailoutTuning Bailout;

	UPROPERTY(Config, EditAnywhere, Category = "Air line")
	FDeepAirLineTuning AirLine;

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

	/** Deep.World.Caves: the baked chunks strung into the dive, their tables and the hull's contact (World/DeepCaveString). */
	UPROPERTY(Config, EditAnywhere, Category = "Cave")
	FDeepCaveStringTuning CaveString;

	/** Real sub controls at the helm: throttle notches, rudder, ballast lever, parking (Sim/DeepPilotRules). */
	UPROPERTY(Config, EditAnywhere, Category = "Pilot")
	FDeepPilotTuning Pilot;

	/** D-070: the helm drives like a game sub, held keys, nothing latches (Sim/DeepPilotRules::StepDrive). */
	UPROPERTY(Config, EditAnywhere, Category = "Drive")
	FDeepDriveTuning Drive;

	/** D-070: the sub at rest holds station and its visual hull breathes (UDeepMovingSubRules::IdleBob). */
	UPROPERTY(Config, EditAnywhere, Category = "Idle")
	FDeepIdleTuning Idle;

	/** D-070: the pilot's chase and cockpit views (World/DeepSubmarine, Sim/DeepHelmViewRules). */
	UPROPERTY(Config, EditAnywhere, Category = "Helm view")
	FDeepHelmViewTuning HelmView;

	/** D1b: the hull's own work lamps, the floods' spill and the markers while someone drives (World/DeepSubmarine). */
	UPROPERTY(Config, EditAnywhere, Category = "Hull lights")
	FDeepHullLightTuning HullLights;

	/** The airlock between the room and the water: the valve hold, the hiss and the fades (Sim/DeepMovingSubRules). */
	UPROPERTY(Config, EditAnywhere, Category = "Submarine")
	FDeepAirlockTuning Airlock;

	/** The moving hull and its fixed room: depth datum, the room's faked motion, the porthole (Sim/DeepMovingSubRules). */
	UPROPERTY(Config, EditAnywhere, Category = "Submarine")
	FDeepMovingSubTuning MovingSub;

	/** Displayed depth of each stop (Stops[i].SubDepthM), shallowest first. */
	TArray<float> StopDepthsM() const;

	UPROPERTY(Config, EditAnywhere, Category = "Director")
	FDeepCaptainTuning Captain;

	UPROPERTY(Config, EditAnywhere, Category = "Director")
	FDeepReviewTuning Review;

	/** Shakes, the footage look, the sonar pulse made visible, interference (Camera/). */
	UPROPERTY(Config, EditAnywhere, Category = "Camera")
	FDeepCameraTuning Camera;
	/** The helm's sonar (Sonar lane). */
	UPROPERTY(Config, EditAnywhere, Category = "Sonar")
	FDeepSonarTuning Sonar;
	/** The room's dressing: materials, clear zones, caged lights (World/DeepRoomDress). */
	UPROPERTY(Config, EditAnywhere, Category = "RoomDress")
	FDeepRoomDressTuning RoomDress;
	/** D-072: the hull's creaks, groans and flow, the silt it lifts, the sonar's echo (Sim/DeepImmersionRules). */
	UPROPERTY(Config, EditAnywhere, Category = "Immersion")
	FDeepImmersionTuning Immersion;
	/** D2: BT-01's own cabin as the pilot's view: its lights and the bow ports' glass (Sim/DeepCabinRules). */
	UPROPERTY(Config, EditAnywhere, Category = "Cabin")
	FDeepCabinTuning Cabin;
};
