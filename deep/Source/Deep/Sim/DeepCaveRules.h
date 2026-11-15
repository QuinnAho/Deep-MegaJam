// The cave shaft's rules as pure static functions (same style as UDeepRules): no world, no actors.
// The passage between stops is a function of depth: a centre (m, X ahead, Y starboard, relative to the stops' column)
// and a radius. The run component steers the sub's offset against it; ADeepShaft draws it. Tests: DeepCaveTests.cpp.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Sim/DeepTuningTypes.h"
#include "DeepCaveRules.generated.h"

/** The sub's box against the wall at one moment. */
USTRUCT(BlueprintType)
struct FDeepCaveContact
{
	GENERATED_BODY()

	/** Some corner of the box is outside the passage. */
	UPROPERTY(BlueprintReadOnly, Category = "Deep|Cave")
	bool bTouching = false;

	/** How far (m) the worst corner is outside; <= 0 = clear by that much. */
	UPROPERTY(BlueprintReadOnly, Category = "Deep|Cave")
	float PenetrationM = 0.f;

	/** Outward wall normal at the worst corner (horizontal, unit): push the sub by -Normal * Penetration to clear it. */
	UPROPERTY(BlueprintReadOnly, Category = "Deep|Cave")
	FVector2D Normal = FVector2D::ZeroVector;
};

UCLASS()
class DEEP_API UDeepCaveRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 0-based segment of a depth: k when StopsM[k] <= d < StopsM[k+1]; -1 above the first stop; last index below the last. */
	UFUNCTION(BlueprintPure, Category = "Deep|Cave")
	static int32 Segment(float DepthM, const TArray<float>& StopsM);

	/** 0 near the stops and outside the shaft, 1 mid-shaft, smooth between: how much the passage bends and breathes. */
	UFUNCTION(BlueprintPure, Category = "Deep|Cave")
	static float Envelope(float DepthM, const TArray<float>& StopsM, const FDeepCaveTuning& Cave);

	/** The passage's centre at a depth (m, relative to the stops' column). Zero at every stop. */
	UFUNCTION(BlueprintPure, Category = "Deep|Cave")
	static FVector2D CentreM(float DepthM, const TArray<float>& StopsM, const FDeepCaveTuning& Cave);

	/** The passage's radius at a depth: StopRadiusM at the stops, caverns and squeezes between, never below MinRadiusM. */
	UFUNCTION(BlueprintPure, Category = "Deep|Cave")
	static float RadiusM(float DepthM, const TArray<float>& StopsM, float MinRadiusM, const FDeepCaveTuning& Cave);

	/** The tightest squeeze for a sub of this box (sub-local metres): its widest corner, padded, plus SqueezeMarginM. */
	UFUNCTION(BlueprintPure, Category = "Deep|Cave")
	static float MinRadiusM(const FVector& BoxMinM, const FVector& BoxMaxM, const FDeepCaveTuning& Cave);

	/**
	 * The sub's box (sub-local metres, padded by HullPadM) at this offset and depth against the wall: its four corners at
	 * the box's top, middle and bottom, each against the passage at that corner's depth.
	 */
	UFUNCTION(BlueprintPure, Category = "Deep|Cave")
	static FDeepCaveContact Contact(const FVector2D& OffsetM, float SubDepthM, const FVector& BoxMinM, const FVector& BoxMaxM, const TArray<float>& StopsM, float MinRadiusM, const FDeepCaveTuning& Cave);

	/** The steering velocity after one step: toward Input * SteerSpeedMps at SteerAccelMps2. Input is clamped to the unit circle. */
	UFUNCTION(BlueprintPure, Category = "Deep|Cave")
	static FVector2D SteerVelocity(const FVector2D& VelocityMps, const FVector2D& Input, float DeltaSeconds, const FDeepCaveTuning& Cave);

	/** Hull damage for an impact at this closing speed: 0 below HitMinSpeedMps, else round(speed * DamagePerMps) in [1, MaxHitDamage]. */
	UFUNCTION(BlueprintPure, Category = "Deep|Cave")
	static int32 HitDamage(float ImpactSpeedMps, const FDeepCaveTuning& Cave);

	/** 1 = whole hull, 0 = breach: 1 - damage / HullBreachDamage, clamped. */
	UFUNCTION(BlueprintPure, Category = "Deep|Cave")
	static float HullIntegrity01(int32 HullDamage, const FDeepCaveTuning& Cave);

	/** The hull breaches at this damage. */
	UFUNCTION(BlueprintPure, Category = "Deep|Cave")
	static bool IsBreached(int32 HullDamage, const FDeepCaveTuning& Cave);

	/** A dent from DamageBefore to DamageAfter takes the integrity under Cave.HullCriticalIntegrity01 (it was not under before). */
	UFUNCTION(BlueprintPure, Category = "Deep|Cave")
	static bool CrossesCritical(int32 DamageBefore, int32 DamageAfter, const FDeepCaveTuning& Cave);
};
