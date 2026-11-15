// Water look for a stop: downwelling light, height fog (with volumetric shafts) and an exposure clamp.
// One per world, spawned by DeepArena::EnsureArena. SetStop switches between the stops' settings and replicates.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sim/DeepTuningTypes.h"
#include "DeepEnvironment.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;

UCLASS()
class DEEP_API ADeepEnvironment : public AActor
{
	GENERATED_BODY()

public:
	ADeepEnvironment();

	/** Server: switch to a stop's look. Replicates, so clients get the same water. */
	void SetStop(int32 InStop);

	int32 GetStop() const { return Stop; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Stop();

	void ApplyStop(const FDeepStopTuning& StopTuning, const FDeepArenaTuning& Arena);

	UPROPERTY(ReplicatedUsing = OnRep_Stop, VisibleAnywhere, Category = "Deep")
	int32 Stop = 1;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UDirectionalLightComponent> Downlight;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UExponentialHeightFogComponent> Fog;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UPostProcessComponent> Exposure;
};
