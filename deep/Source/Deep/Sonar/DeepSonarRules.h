// The sonar's pure rules (Sonar lane, Nov 15): bearing and range to the scope, the sweep and its ping, how a return
// fades, the lower slice's height, and the proximity tone. No world access; tested by Deep.UI.Sonar.*.
// Bearings are degrees clockwise from the sub's bow seen from above (0 = up on the scope), 0..360.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Sim/DeepTuningTypes.h"
#include "DeepSonarRules.generated.h"

/** Sound rows the sonar plays itself (Ping, Near: local, not on the bus), and the one bus event it raises (Contact). */
namespace DeepSonarEvent
{
	inline const TCHAR* Ping = TEXT("sonar_ping");
	inline const TCHAR* Near = TEXT("sonar_near");
	/** On the event bus: the serpent first shows on the sonar at this stop. Payload DepthM = its range in metres. */
	inline const TCHAR* Contact = TEXT("sonar_contact");
}

UCLASS()
class DEEP_API UDeepSonarRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Scope point of a return, in pixels from the scope's centre (x right, y down). Ranges past RangeMaxM sit on the rim. */
	UFUNCTION(BlueprintPure, Category = "Deep|Sonar")
	static FVector2D PanelPoint(float BearingDeg, float RangeM, float RangeMaxM, float RadiusPx);

	/** Bearing of a horizontal offset from the bow direction, clockwise seen from above, 0..360. */
	UFUNCTION(BlueprintPure, Category = "Deep|Sonar")
	static float BearingDeg(FVector Forward, FVector Offset);

	/** Horizontal (plan) distance of an offset, metres from centimetres. */
	UFUNCTION(BlueprintPure, Category = "Deep|Sonar")
	static float PlanRangeM(FVector OffsetCm);

	/** Unit horizontal direction of beam Index of Beams, beam 0 on the bow, clockwise seen from above. */
	UFUNCTION(BlueprintPure, Category = "Deep|Sonar")
	static FVector BeamDirection(FVector Forward, int32 Index, int32 Beams);

	/** The beam a bearing falls in. */
	UFUNCTION(BlueprintPure, Category = "Deep|Sonar")
	static int32 BeamAt(float BearingDeg, int32 Beams);

	/** The sweep's bearing after Seconds of sweeping, 0..360. */
	UFUNCTION(BlueprintPure, Category = "Deep|Sonar")
	static float SweepDeg(float Seconds, float PeriodS);

	/** True when the sweep passed the bow between two frames (one ping per turn). */
	UFUNCTION(BlueprintPure, Category = "Deep|Sonar")
	static bool PingDue(float PrevSweepDeg, float NewSweepDeg);

	/** True when Bearing lies on the arc the sweep covered between two frames (clockwise, Prev excluded, New included). */
	UFUNCTION(BlueprintPure, Category = "Deep|Sonar")
	static bool SweptOver(float BearingDeg, float PrevSweepDeg, float NewSweepDeg);

	/** Brightness 0..1 of a return AgeS after its beam passed, at RangeM: fades out over PersistS, far ones dimmer. */
	UFUNCTION(BlueprintPure, Category = "Deep|Sonar")
	static float ReturnStrength(float AgeS, float RangeM, const FDeepSonarTuning& Tuning);

	/** How far under the sub's centre the dim slice runs: SliceBelowM, or just above a nearer bottom (BedBelowM < 0 = none). */
	UFUNCTION(BlueprintPure, Category = "Deep|Sonar")
	static float LowerSliceBelowM(float BedBelowM, const FDeepSonarTuning& Tuning);

	/** The proximity tone's level 0..1 from the clearance to the nearest wall (metres; negative = no wall): 0 at ToneStartM, 1 at ToneFullM. */
	UFUNCTION(BlueprintPure, Category = "Deep|Sonar")
	static float ProximityLevel(float ClearanceM, const FDeepSonarTuning& Tuning);

	/** Seconds between beeps at a level (0 = no tone). */
	UFUNCTION(BlueprintPure, Category = "Deep|Sonar")
	static float ToneIntervalS(float Level, const FDeepSonarTuning& Tuning);

	UFUNCTION(BlueprintPure, Category = "Deep|Sonar")
	static float TonePitch(float Level, const FDeepSonarTuning& Tuning);

	UFUNCTION(BlueprintPure, Category = "Deep|Sonar")
	static float ToneVolume(float Level, const FDeepSonarTuning& Tuning);

	/**
	 * The Captain's call on the serpent's first sonar contact: once per stop the sub is at (a new stop, or a new run
	 * that comes back to it, calls again). CalledStop is the stop of the last call (-1 = none yet).
	 */
	UFUNCTION(BlueprintPure, Category = "Deep|Sonar")
	static bool SerpentCallDue(int32 CalledStop, int32 Stop);
};
