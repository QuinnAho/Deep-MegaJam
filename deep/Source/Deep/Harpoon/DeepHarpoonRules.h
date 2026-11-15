// The harpoon's pure rules and types: range, bolt flight against spheres, reel step, firing permission, repair fees.
// No world access; tested in Private/Tests/DeepHarpoonTests.cpp. Event names live here, not in DeepEvent::,
// because the Captain's lines for them are written separately (an event with no line is harmless).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DeepHarpoonRules.generated.h"

namespace DeepHarpoonEvent
{
	inline const TCHAR* Fired = TEXT("harpoon_fired");
	inline const TCHAR* HitSub = TEXT("harpoon_hit_sub");
	inline const TCHAR* HitTerrain = TEXT("harpoon_hit_terrain");
	inline const TCHAR* HitMonster = TEXT("harpoon_hit_monster");
	inline const TCHAR* Missed = TEXT("harpoon_missed");
	inline const TCHAR* Empty = TEXT("harpoon_empty");
}

UENUM(BlueprintType)
enum class EDeepHarpoonPhase : uint8
{
	/** Nothing out. Ready when shots are left. */
	Idle,
	/** The bolt is in the water. */
	Flying,
	/** The bolt holds; the line pulls the diver. */
	Latched
};

UENUM(BlueprintType)
enum class EDeepHarpoonTarget : uint8
{
	None,
	Submarine,
	Terrain,
	Monster
};

/** What the HUD says after the last thing the harpoon did. */
UENUM(BlueprintType)
enum class EDeepHarpoonResult : uint8
{
	None,
	Fired,
	LatchedSub,
	LatchedTerrain,
	HitMonster,
	Missed,
	OutOfRange,
	Empty,
	Released,
	Arrived
};

UCLASS()
class DEEP_API UDeepHarpoonRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** True when To is within RangeM of From (cm positions). */
	UFUNCTION(BlueprintPure, Category = "Deep|Harpoon")
	static bool InRange(const FVector& FromCm, const FVector& ToCm, float RangeM);

	/** May the diver fire now: shots left, in the water, nothing already out. */
	UFUNCTION(BlueprintPure, Category = "Deep|Harpoon")
	static bool CanFire(int32 ShotsLeft, bool bCanSwim, EDeepHarpoonPhase Phase);

	/** One reel step toward Target at SpeedMps; never overshoots. */
	UFUNCTION(BlueprintPure, Category = "Deep|Harpoon")
	static FVector ReelStep(const FVector& PositionCm, const FVector& TargetCm, float SpeedMps, float DeltaSeconds);

	/** How far the bolt goes this step: speed * dt, but never past the range. */
	UFUNCTION(BlueprintPure, Category = "Deep|Harpoon")
	static float BoltStepCm(float TravelledCm, float RangeM, float SpeedMps, float DeltaSeconds);

	/** Where along Start->End (0..1) the segment first touches the sphere; -1 if it misses. Starting inside is 0. */
	UFUNCTION(BlueprintPure, Category = "Deep|Harpoon")
	static float SegmentSphereHit(const FVector& StartCm, const FVector& EndCm, const FVector& CentreCm, float RadiusCm);

	/** Total repair fee for this many submarine hits (each FeePerHit; never negative, no overflow). */
	UFUNCTION(BlueprintPure, Category = "Deep|Harpoon")
	static int32 RepairFee(int32 Hits, int32 FeePerHit);

	/**
	 * The part of the repair bill the haul could not cover, which comes out of the account instead.
	 * Settle() never lets a settlement's net go below zero; repairs are real money, so the rest is charged
	 * against credits (ApplyFee keeps those at zero or more). Fees includes the repair lines.
	 */
	UFUNCTION(BlueprintPure, Category = "Deep|Harpoon")
	static int32 RepairFromCredits(int32 Gross, int32 DirectorCut, int32 Fees, int32 Repair);
};
