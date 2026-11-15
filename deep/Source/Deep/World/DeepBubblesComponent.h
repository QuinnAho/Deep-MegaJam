// Air bubbles for one diver: a burst on each breath out (calm and fast, like the breath loops), a stream where the
// diver drops in through the hatch, and a churn at the hatch while the winch runs. Visual only.
// Each bubble is a tiny sphere with a mirror-like surface (the room's steel material, unpainted metal, low roughness),
// so it shows only where the lamp or the hatch light lands on it and is dark elsewhere. Each is its own movable mesh,
// so it writes motion vectors like the rope. No new asset, so no usage flag can be missing in a cooked build.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Sim/DeepBubbleSim.h"
#include "DeepBubblesComponent.generated.h"

class UStaticMeshComponent;

UCLASS(ClassGroup = (Deep), meta = (BlueprintSpawnableComponent))
class DEEP_API UDeepBubblesComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UDeepBubblesComponent();

	/** Releases Count bubbles at AtCm, scattered over SpreadCm, pushed by KickCmS (plus a random part). */
	void Emit(const FVector& AtCm, int32 Count, float SpreadCm, const FVector& KickCmS, float SizeScale = 1.f);

	int32 CountAlive() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	void EnsurePool();
	void UpdateSources(float DeltaSeconds);
	void Lay();

	TArray<FDeepBubble> Bubbles;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Meshes;

	FRandomStream Rng;
	float BreathClockS = 0.f;
	float BreathOwedPerS = 0.f;
	float BreathCarry = 0.f;
	float WinchCarry = 0.f;
	bool bWasSwimming = false;
	float HatchStreamLeftS = 0.f;
	int32 Next = 0;
};
