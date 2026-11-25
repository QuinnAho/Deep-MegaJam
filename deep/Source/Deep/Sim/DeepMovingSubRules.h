// The moving sub as pure rules: depth from a world height, the hull against a wall, parking onto a stop point, the
// fixed room faking the hull's motion, and the airlock's timeline. World units are metres unless a name says Cm.
// Frames: WallNormal points out of the wall toward the hull; yaw is degrees about +Z; depth grows down from the datum.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Sim/DeepTuningTypes.h"
#include "DeepMovingSubRules.generated.h"

/** Where the airlock's cycle is. */
UENUM(BlueprintType)
enum class EDeepAirlockPhase : uint8
{
	Idle,
	/** Going out only: the hiss with the view still clear. */
	Hiss,
	FadeOut,
	/** Black: the diver is moved at its start. */
	Black,
	FadeIn,
	Done
};

/** The airlock at one moment: the phase, how black the view is (0 clear .. 1 black) and whether the move is due. */
USTRUCT(BlueprintType)
struct DEEP_API FDeepAirlockState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	EDeepAirlockPhase Phase = EDeepAirlockPhase::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	float Fade01 = 0.f;

	/** True from the start of Black on: the diver belongs on the other side. */
	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	bool bMoveDue = false;
};

/** The hull against a wall for one step. */
USTRUCT(BlueprintType)
struct DEEP_API FDeepWallHit
{
	GENERATED_BODY()

	/** The hull's velocity after the wall (m/s). */
	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	FVector VelocityMps = FVector::ZeroVector;

	/** Closing speed into the wall (m/s, 0 when moving away). */
	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	float ImpactMps = 0.f;

	/** At least HitMinSpeedMps: a knock (dent, bounce, keep only KnockKeepAlong of the speed along the wall). */
	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	bool bKnock = false;

	/** Touching while closing slower than a knock: grinding along the rock. */
	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	bool bScrape = false;

	/** Hull damage for this contact (UDeepCaveRules::HitDamage; 0 for a scrape). */
	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	int32 Damage = 0;
};

/** One step of the hull parking onto a stop point. */
USTRUCT(BlueprintType)
struct DEEP_API FDeepParkStep
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	FVector LocationM = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	float YawDeg = 0.f;

	/** Near enough, slow enough and the controls left alone: the glide runs. */
	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	bool bInRange = false;

	/** On the point and its heading: parked. */
	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	bool bArrived = false;
};

/** The fixed room's faked motion at one moment. */
USTRUCT(BlueprintType)
struct DEEP_API FDeepRoomSway
{
	GENERATED_BODY()

	/** Added to the view (deg). Pitch + = up. */
	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	float PitchDeg = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	float RollDeg = 0.f;

	/** The caged lights' swing about their hook (deg). */
	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	float LightSwingDeg = 0.f;

	/** Engine hum and hull creak loudness, 0..1. */
	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	float Hum01 = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	float Creak01 = 0.f;

	/** A knock's shake (deg), already in PitchDeg and RollDeg. */
	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	float ShakeDeg = 0.f;
};

UCLASS()
class DEEP_API UDeepMovingSubRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Depth (m, + down) of a world height (cm) under a datum height (cm). */
	UFUNCTION(BlueprintPure, Category = "Deep|MovingSub")
	static float DepthFromZ(double ZCm, double DatumZCm);

	/** The world height (cm) of a depth (m) under the datum: DepthFromZ's inverse. */
	UFUNCTION(BlueprintPure, Category = "Deep|MovingSub")
	static double ZFromDepth(float DepthM, double DatumZCm);

	/**
	 * The hull meets a wall: WallNormal points from the wall to the hull. Closing at HitMinSpeedMps or more is a knock:
	 * the speed into the wall is thrown back by Cave.Bounce and only Pilot.KnockKeepAlong of the speed along it stays.
	 * Slower is a scrape: the speed into the wall stops, the slide along it goes on. Moving away changes nothing.
	 */
	UFUNCTION(BlueprintPure, Category = "Deep|MovingSub")
	static FDeepWallHit ResolveWallHit(const FVector& VelocityMps, const FVector& WallNormal, const FDeepCaveTuning& Cave, const FDeepPilotTuning& Pilot);

	/** The same with the knock's keep-along share and bounce given (D-070's direct drive: Drive.KnockKeepAlong, Drive.KnockBounce). */
	static FDeepWallHit ResolveWallHitKeeping(const FVector& VelocityMps, const FVector& WallNormal, const FDeepCaveTuning& Cave, float KnockKeepAlong, float Bounce);

	/**
	 * Parking onto a stop point: within Pilot.ParkNearM, slower than ParkSpeedMps and with the controls idle, the hull
	 * glides straight onto the point and turns the short way to its heading (UDeepPilotRules::GlideStep's pace). Out of
	 * range it stays where it is.
	 */
	UFUNCTION(BlueprintPure, Category = "Deep|MovingSub")
	static FDeepParkStep ParkNear(const FVector& HullM, float HullYawDeg, const FVector& StopM, float StopYawDeg, float SpeedMps, bool bControlsIdle, float DeltaSeconds, const FDeepPilotTuning& Pilot);

	/**
	 * The fixed room fakes the hull's motion: a slow roll that grows with speed, a lean with the acceleration ahead (AheadAccelMps2,
	 * + = speeding up), lights that swing, hum, creak and a shake that dies away after a knock (KnockAgeS < 0 = none).
	 */
	UFUNCTION(BlueprintPure, Category = "Deep|MovingSub")
	static FDeepRoomSway RoomSway(float SpeedMps, float AheadAccelMps2, float TimeS, float KnockAgeS, float KnockMps, const FDeepMovingSubTuning& T);

	/** Seconds the whole cycle takes: out (hiss, fade out, black, fade in) or back (fade out, black, fade in). */
	UFUNCTION(BlueprintPure, Category = "Deep|MovingSub")
	static float AirlockSeconds(bool bOut, const FDeepAirlockTuning& T);

	/** The airlock ElapsedS into a cycle (< 0 = not started). */
	UFUNCTION(BlueprintPure, Category = "Deep|MovingSub")
	static FDeepAirlockState AirlockPhase(float ElapsedS, bool bOut, const FDeepAirlockTuning& T);

	/**
	 * Deep.World.Still: the map from the world as D-022 places it (the hull fixed at Home, the arena root moved by the
	 * game state's depth, offset and yaw) to the still world (the root at rest, every stop's content at its own depth
	 * under the datum, the hull moved). Content of the loaded stop placed the D-022 way lands where this map puts it,
	 * so the hull keeps exactly its place among the walls, the bed and the salvage. Rows: Moving * Map = Still.
	 */
	UFUNCTION(BlueprintPure, Category = "Deep|MovingSub")
	static FTransform StillMap(const FTransform& Home, const FVector2D& OffsetM, float DepthM, float YawDeg, double DatumZCm);

	/** Deep.World.Still: the hull's transform for the game state's depth, offset and yaw (Home * StillMap). */
	UFUNCTION(BlueprintPure, Category = "Deep|MovingSub")
	static FTransform StillHull(const FTransform& Home, const FVector2D& OffsetM, float DepthM, float YawDeg, double DatumZCm);

	/** Deep.World.Still: the frame a stop's content hangs in (where the arena root stood for it under D-022): under Home at the stop's depth. */
	UFUNCTION(BlueprintPure, Category = "Deep|MovingSub")
	static FTransform StillStopFrame(const FTransform& Home, float StopDepthM, double DatumZCm);

	/** Deep.World.Still: the cave shaft, placed once: its point at depth d is d below the datum, under Home. */
	UFUNCTION(BlueprintPure, Category = "Deep|MovingSub")
	static FTransform StillShaft(const FTransform& Home, double DatumZCm);

	/** Deep.World.Still: a stop's bed shows (and collides) while the hull is at or above it (BedShowsBelowStopM of slack). */
	UFUNCTION(BlueprintPure, Category = "Deep|MovingSub")
	static bool StillBedShows(float SubDepthM, float StopDepthM, const FDeepMovingSubTuning& T);

	/** How far the valve has turned, 0..1, after holding E for HeldS. At 1 the cycle starts. */
	UFUNCTION(BlueprintPure, Category = "Deep|MovingSub")
	static float ValveTurn01(float HeldS, const FDeepAirlockTuning& T);
};
