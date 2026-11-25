// Real submarine controls at the helm, as pure rules: the throttle's notches, the ballast lever, the sub's motion
// (surge along the heading, sway across it, heave, yaw rate), parking, the glide home and where the world hangs.
// Frames: offsets and velocities are in metres in the stop's frame as the sub sees it parked (X ahead, Y starboard);
// YawDeg is the sub's heading in that frame (0 = as parked, + = turned to starboard). Depth grows down.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Sim/DeepTuningTypes.h"
#include "DeepPilotRules.generated.h"

/** The sub's motion: horizontal velocity (stop frame, m/s), vertical speed (m/s, + = down), heading and turn rate. */
USTRUCT(BlueprintType)
struct DEEP_API FDeepPilotMotion
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Deep")
	FVector2D VelocityMps = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Deep")
	float HeaveMps = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Deep")
	float YawDeg = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Deep")
	float YawRateDegS = 0.f;
};

/**
 * D-070 direct drive: the pilot's held keys, each -1..1. Nothing latches: let go, and the axis is 0. Surge + ahead (W),
 * Turn + starboard (D), Heave + down (C; Space is up), Strafe + starboard (E; Q is port).
 */
USTRUCT(BlueprintType)
struct DEEP_API FDeepDriveInput
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Deep")
	float Surge = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Deep")
	float Turn = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Deep")
	float Heave = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Deep")
	float Strafe = 0.f;

	/** Every axis clamped to -1..1. */
	FDeepDriveInput Clamped() const;
	/** No key held. */
	bool IsIdle() const;
	bool Equals(const FDeepDriveInput& Other, float Tolerance = 0.01f) const;
};

UCLASS()
class DEEP_API UDeepPilotRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** One throttle press: Dir > 0 one notch ahead, < 0 one notch astern; held at full either way. */
	UFUNCTION(BlueprintPure, Category = "Deep|Pilot")
	static int32 StepNotch(int32 Notch, int32 Dir, const FDeepPilotTuning& T);

	/** The speed the notch asks for along the heading (m/s, astern negative). */
	UFUNCTION(BlueprintPure, Category = "Deep|Pilot")
	static float NotchSpeedMps(int32 Notch, const FDeepPilotTuning& T);

	/** The notch as a lever fraction, -1 full astern .. 0 stop .. 1 full ahead. */
	UFUNCTION(BlueprintPure, Category = "Deep|Pilot")
	static float NotchFraction(int32 Notch, const FDeepPilotTuning& T);

	/**
	 * The ballast lever (-1 blown .. 0 neutral .. 1 flooded). Input > 0 floods (C), < 0 blows (Space), 0 = let go: it
	 * stays, or drops into the neutral detent within BallastDetentSnap. Passing neutral it holds in the detent for
	 * BallastDetentS (DetentLeftS counts it down) before going on.
	 */
	static float StepBallast(float Lever, float Input, float DeltaSeconds, float& DetentLeftS, const FDeepPilotTuning& T);

	/** One step of the sub's motion from the controls (Rudder -1 port .. 1 starboard). No walls, no limits. */
	UFUNCTION(BlueprintPure, Category = "Deep|Pilot")
	static FDeepPilotMotion StepMotion(const FDeepPilotMotion& In, int32 Notch, float Ballast, float Rudder, float DeltaSeconds, const FDeepPilotTuning& T);

	/**
	 * D-070 direct drive, one step (no walls, no limits): each axis moves toward its held speed at AccelMps2 (or
	 * HeaveAccelMps2) and back toward rest at BrakeMps2 when let go or held against the way; way across the heading that
	 * Strafe does not ask for dies at SideGripMps2 (a little drift in a turn); the turn rate builds and dies over TurnBuildS.
	 */
	UFUNCTION(BlueprintPure, Category = "Deep|Pilot")
	static FDeepPilotMotion StepDrive(const FDeepPilotMotion& In, const FDeepDriveInput& Input, float DeltaSeconds, const FDeepDriveTuning& T);

	/**
	 * Deep.Pilot.MouseSteer: while thrusting (Surge not 0), the turn and heave that bring the heading toward where the
	 * view looks, LookYawDeg off the heading (+ starboard) and LookPitchDeg off level (+ up). X = Turn, Y = Heave (+ down).
	 */
	UFUNCTION(BlueprintPure, Category = "Deep|Pilot")
	static FVector2D MouseSteer(float LookYawDeg, float LookPitchDeg, float Surge, const FDeepDriveTuning& T);

	/** Value moved toward Target by at most Rate * DeltaSeconds (never past it). */
	static float MoveToward(float Value, float Target, float Rate, float DeltaSeconds);

	/** Unit vectors of the heading and of starboard in the stop frame. */
	static FVector2D Ahead(float YawDeg);
	static FVector2D Starboard(float YawDeg);

	/** Speed over all axes (m/s). */
	UFUNCTION(BlueprintPure, Category = "Deep|Pilot")
	static float SpeedMps(const FDeepPilotMotion& Motion);

	/** Parks: near the stop's depth, slow, the throttle at stop and the ballast lever in neutral (so casting off never re-parks). */
	UFUNCTION(BlueprintPure, Category = "Deep|Pilot")
	static bool ShouldPark(float DepthM, float StopDepthM, float SpeedMps, int32 Notch, float Ballast, const FDeepPilotTuning& T);

	/** One glide step toward Target: a share of the gap each second (ParkGlidePerS), never slower than MinSpeed. */
	UFUNCTION(BlueprintPure, Category = "Deep|Pilot")
	static float GlideStep(float Value, float Target, float DeltaSeconds, float MinSpeed, const FDeepPilotTuning& T);

	/** The same for a heading: the short way round to 0, the result in (-180, 180]. */
	UFUNCTION(BlueprintPure, Category = "Deep|Pilot")
	static float GlideYawStep(float YawDeg, float DeltaSeconds, const FDeepPilotTuning& T);

	/**
	 * A square box (sub-local metres) whose corners lie on the hull box's circumcircle, so it is the same at any
	 * heading: a turn never pushes a corner into rock. Z is kept.
	 */
	static void TurnFreeBoxM(const FVector& BoxMinM, const FVector& BoxMaxM, FVector& OutMinM, FVector& OutMaxM);

	/**
	 * Where something hung in the stop's frame goes when the sub sits at OffsetM (stop frame), UpM above it and turned
	 * YawDeg: the world turns about the vertical through the sub, the sub stays put. OutTurn is the world's turn
	 * (pure yaw of -YawDeg); compose it in front of the thing's own rotation. At yaw 0 this is the old placement.
	 */
	static FVector HangLocationCm(const FVector& SubCm, const FQuat& SubRotation, const FVector2D& OffsetM, float UpM, float YawDeg, FQuat& OutTurn);
};
