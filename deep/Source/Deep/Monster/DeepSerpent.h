// The sea serpent's body: engine basic shapes, no animation assets. A head steers (speed + max turn rate);
// about 20 instanced spheres follow the path the head actually travelled, tapering to the tail, with a
// travelling sideways weave. Two small glowing eyes show as a pair of points in the dark.
// A Pawn possessed by ADeepSerpentController, which decides where it goes (server) and calls SwimToward.
// No movement component and no navmesh: open water, own steering. The body lays itself out every tick on every machine.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DeepSerpent.generated.h"

class UInstancedStaticMeshComponent;
class UPointLightComponent;
class UStaticMeshComponent;

UCLASS()
class DEEP_API ADeepSerpent : public APawn
{
	GENERATED_BODY()

public:
	ADeepSerpent();

	/** Server: turn toward Aim by at most TurnRate * dt, then swim forward. Never overshoots Aim. */
	void SwimToward(const FVector& AimCm, float SpeedMps, float TurnRateDegPerS, float DeltaSeconds);

	/** Places the head and lays the body out straight behind it. */
	void PlaceHead(const FVector& HeadCm, const FVector& Facing);

	FVector GetHeadLocation() const { return GetActorLocation(); }
	FVector GetHeading() const { return Heading; }

	/** World positions of the body segments as last laid out (head excluded). */
	const TArray<FTransform>& GetSegmentTransforms() const { return SegmentTransforms; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	/** Adds the head position to the trail when it has moved far enough; trims the trail to the body length. */
	void RecordTrail();

	/** Positions every body segment along the trail, with taper and weave. */
	void LayBody();

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> Jaw;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> EyeLeft;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> EyeRight;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UPointLightComponent> EyeLightLeft;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UPointLightComponent> EyeLightRight;

	/** Body segments, in world space (the component sits at the origin with absolute transform). */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UInstancedStaticMeshComponent> Body;

	/** Head positions, newest first. */
	TArray<FVector> Trail;
	TArray<FTransform> SegmentTransforms;
	FVector Heading = FVector::ForwardVector;
};
