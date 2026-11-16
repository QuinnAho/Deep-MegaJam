// The sea bed: one rolling surface of silt generated in code, centred under the sub's anchor and hung from the arena
// root. It passes through the resting point of every salvage spot and the relic, so salvage lies on it, and reaches far
// past where the fog ends so its edge is never seen. Material by path (FDeepLookTuning::SeabedMaterial), so a scanned
// sand or silt surface can replace the made one without code. A sinking diver lands on it (it blocks).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sim/DeepTuningTypes.h"
#include "DeepAbyssFloor.generated.h"

class UProceduralMeshComponent;

UCLASS()
class DEEP_API ADeepAbyssFloor : public AActor
{
	GENERATED_BODY()

public:
	ADeepAbyssFloor();

	/**
	 * Metres below the anchor of the bed at XyM (metres from the anchor). RestM: points the bed must pass through,
	 * (x, y, metres below the anchor, radius in m of the level patch round it; negative = a sinkhole cut into the bed). Pure: the base slope, pulled to each resting point near it, plus a gentle roll kept
	 * off the resting points.
	 */
	static float SeabedBelowM(const FVector2D& XyM, const TArray<FVector4>& RestM, const FDeepLookTuning& Look);

	/** Where salvage at OffsetCm from the anchor rests on the bed: (x, y, metres below the anchor, RadiusM). */
	static FVector4 RestPointM(const FVector& OffsetCm, float HalfM, float RadiusM, const FDeepLookTuning& Look);

	/**
	 * The resting points of every salvage spot and the relic, from the tuning. The relic lies far deeper than the ring
	 * (77 m below the anchor at 500 m), so its point is narrow: a sinkhole in the bed with the relic at the bottom.
	 */
	static TArray<FVector4> RestPointsFromTuning();

	/** Builds the surface around the actor's location (the anchor). */
	void Build(const TArray<FVector4>& RestM);

	/** Kept for callers: the bed's size comes from FDeepLookTuning::SeabedHalfWidthM. */
	void SetSizeM(float SizeM) {}

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UProceduralMeshComponent> Bed;
};
