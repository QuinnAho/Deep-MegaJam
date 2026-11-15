// The sea serpent's rules as pure static functions (same style as UDeepRules): no world, no actors.
// UDeepMonsterBrain gathers inputs, calls these and applies the result. Tests: Private/Tests/DeepMonsterTests.cpp.
// Lengths in metres, world positions in cm.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Monster/DeepMonsterTypes.h"
#include "DeepMonsterRules.generated.h"

UCLASS()
class DEEP_API UDeepMonsterRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ---- Noise ----

	/** Speed above which a swimming diver counts as sprinting: halfway between swim and sprint speed. */
	UFUNCTION(BlueprintPure, Category = "Deep|Monster")
	static float SprintThresholdMps(float SwimSpeedMps, float SprintMultiplier);

	/** Noise one diver made this step. Winching is loud, sprint speed is loud (not while winching), a cargo rise is a grab. */
	UFUNCTION(BlueprintPure, Category = "Deep|Monster")
	static float NoiseFromDiver(bool bWinching, float SpeedMps, float SprintSpeedMps, float CargoGainKg, float DeltaSeconds, const FDeepMonsterTuning& Tuning);

	/**
	 * Not pure: the one door into Unreal's hearing sense. Reports a noise event (UAISense_Hearing) that the serpent's
	 * perception hears; Loudness is in noise units. Any system can call it (winch motor, alarms, dropped salvage).
	 */
	UFUNCTION(BlueprintCallable, Category = "Deep|Monster", meta = (WorldContext = "WorldContextObject"))
	static void ReportDiverNoise(UObject* WorldContextObject, FVector LocationCm, float Loudness, AActor* Instigator);

	/** Adds this step's noise, then decays it. Clamped to [0, NoiseMax]. */
	UFUNCTION(BlueprintPure, Category = "Deep|Monster")
	static float NoiseStep(float Noise, float Added, float DeltaSeconds, const FDeepMonsterTuning& Tuning);

	// ---- Lamp ----

	/**
	 * True if a point is lit by a spot lamp: within RangeM of the lamp and within HalfAngleDeg of its axis.
	 * PointRadiusM widens the cone for a target of that size (aiming leniency). Points behind the lamp are never lit.
	 */
	UFUNCTION(BlueprintPure, Category = "Deep|Monster")
	static bool IsInBeam(const FVector& LampCm, const FVector& LampDirection, float HalfAngleDeg, float RangeM, const FVector& PointCm, float PointRadiusM);

	/** Lit time: grows 1:1 while lit, falls at LitDecayPerS when not. Never below zero. */
	UFUNCTION(BlueprintPure, Category = "Deep|Monster")
	static float LitStep(float LitSeconds, bool bLitNow, float DeltaSeconds, const FDeepMonsterTuning& Tuning);

	// ---- State ----

	/** The one state machine. Returns the next state and the events this step caused. */
	UFUNCTION(BlueprintPure, Category = "Deep|Monster")
	static FDeepMonsterStep NextState(const FDeepMonsterInputs& In, const FDeepMonsterTuning& Tuning);

	/** 0 far away, 1 on top of you (or biting). For sound and HUD. */
	UFUNCTION(BlueprintPure, Category = "Deep|Monster")
	static float Threat01(EDeepMonsterState State, float DiverDistanceM, const FDeepMonsterTuning& Tuning);

	// ---- Geometry and steering ----

	/** Metres from a point to the segment A-B (all cm). */
	UFUNCTION(BlueprintPure, Category = "Deep|Monster")
	static float DistanceToSegmentM(const FVector& PointCm, const FVector& ACm, const FVector& BCm);

	/** 0..1 along A-B of the point on the segment nearest PointCm. */
	UFUNCTION(BlueprintPure, Category = "Deep|Monster")
	static float SegmentAlpha(const FVector& PointCm, const FVector& ACm, const FVector& BCm);

	/** Lurk: a point on a horizontal circle of RadiusM around Centre, LeadDeg ahead of where Pos is (so it circles). */
	UFUNCTION(BlueprintPure, Category = "Deep|Monster")
	static FVector OrbitAimPoint(const FVector& CentreCm, const FVector& PosCm, float RadiusM, float LeadDeg);

	/** Stalk: aim ArcDeg around the target and InwardFraction of the distance in, so the approach spirals. Straight in within DirectWithinM. */
	UFUNCTION(BlueprintPure, Category = "Deep|Monster")
	static FVector SpiralAimPoint(const FVector& TargetCm, const FVector& PosCm, float ArcDeg, float InwardFraction, float DirectWithinM);

	/** Rotates a unit heading toward Wanted by at most MaxDegrees. */
	UFUNCTION(BlueprintPure, Category = "Deep|Monster")
	static FVector TurnToward(const FVector& Heading, const FVector& Wanted, float MaxDegrees);

	/** The point DistanceCm back along a polyline (Trail[0] is the head). Extends straight past the end. */
	UFUNCTION(BlueprintPure, Category = "Deep|Monster")
	static FVector PointAlongTrail(const TArray<FVector>& TrailCm, float DistanceCm);

	/** Share of the weave's amplitude at T along the body (0 = neck, 1 = tail): NeckShare at the neck, all of it at the tail. */
	UFUNCTION(BlueprintPure, Category = "Deep|Monster")
	static float WeaveShare(float T, float NeckShare);

	UFUNCTION(BlueprintPure, Category = "Deep|Monster")
	static FString StateName(EDeepMonsterState State);

	// ---- The well (pure; world cm; not Blueprint: FDeepWell is a plain struct) ----

	/** Inside the shaft: within its radius less MarginCm of the axis, and between its floor and top. */
	static bool IsInWell(const FDeepWell& Well, const FVector& PointCm, double MarginCm = 0.0);
	/** Horizontal distance from the shaft's axis (m). */
	static float WellAxisDistanceM(const FDeepWell& Well, const FVector& PointCm);
	/** The point moved inside the shaft: at most radius - WellWallClearM off the axis, at least WellFloorClearM above its floor. */
	static FVector KeepInWell(const FDeepWell& Well, const FVector& AimCm, const FDeepMonsterTuning& Tuning, float WallClearM = -1.f);
	/**
	 * How far below a diver who rises at RiseMps it lurks in the shaft (m): far enough that a stalk from there takes as
	 * long as one from the lurk radius in open water ((LurkRadiusM - BiteReachM) / StalkSpeedMps) at the closing speed it
	 * has from below (StalkSpeedMps - RiseMps). Never more than LurkRadiusM, never less than twice the bite reach.
	 */
	static float WellHoldBelowM(float RiseMps, const FDeepMonsterTuning& Tuning);
	/**
	 * Where it steers for in the shaft, by state, kept inside it. Lurk: a small circle round the axis at the hold depth
	 * below the diver. Stalk: up from below to the diver's end of the line, WellStalkSideM to SideDir's side. Recoil:
	 * the head turns down, then straight down the shaft. Bite, hunt, pass: the open-water aim, kept inside.
	 */
	static FVector WellAimPoint(EDeepMonsterState State, const FDeepWell& Well, const FVector& HeadCm, const FVector& DiverCm,
		const FVector& OpenWaterAimCm, const FVector2D& SideDir, float RiseMps, const FDeepMonsterTuning& Tuning);
};
