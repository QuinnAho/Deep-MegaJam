// A rope drawn as a chain of short cylinder segments, one per link of an FDeepRopeSim. Visual only.
// Each segment is its own movable static mesh component, so every frame's move writes motion vectors (a procedural
// mesh rebuilt in place would not, and the temporal upscaler smears it). Opaque, lit by the lamp, casts shadows.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Sim/DeepRopeSim.h"
#include "DeepRopeComponent.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

UCLASS(ClassGroup = (Deep), meta = (BlueprintSpawnableComponent))
class DEEP_API UDeepRopeComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UDeepRopeComponent();

	/** Points, thickness and colour. Call before the first Drive (BeginPlay); a change of points restarts the rope. */
	void Configure(int32 InPoints, float InThicknessCm, const FLinearColor& InColor, bool bInCastShadow);

	/**
	 * Steps the rope and lays the segments. A null pin leaves that end free. LengthCm is the whole rope's rest length.
	 * The first call, a Restart, or a pin that jumps further than SnapResetM lays the rope straight between the pins.
	 */
	void Drive(float DeltaSeconds, const FVector* StartPin, const FVector* EndPin, float LengthCm, float SinkCmS2);

	/** Parts the rope at the link nearest Point (the serpent's bite). Returns the link, or INDEX_NONE. */
	int32 CutNear(const FVector& Point);
	/** Parts the rope at its first link (the hatch end). */
	void CutAtStart();
	/** The next Drive lays a fresh, whole rope. */
	void Restart() { bRestart = true; }
	/** Hides every segment until the next Drive. */
	void HideRope();

	bool IsCut() const { return Sim.IsValid() && Sim.CountLinked() < Sim.NumLinks(); }
	bool IsShown() const { return bShown; }
	const FDeepRopeSim& GetSim() const { return Sim; }

protected:
	void EnsureSegments();
	void LaySegments();

	FDeepRopeSim Sim;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Segments;

	int32 Points = 24;
	float ThicknessCm = 1.5f;
	FLinearColor Color = FLinearColor(0.2f, 0.16f, 0.07f);
	bool bCastShadow = true;
	bool bRestart = true;
	bool bShown = false;
	FVector LastStartPin = FVector::ZeroVector;
	FVector LastEndPin = FVector::ZeroVector;
};
