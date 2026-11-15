// Water look for a stop: downwelling light, height fog (with volumetric shafts) and a locked exposure.
// One per world, spawned by DeepArena::EnsureArena. SetStop switches between the stops' settings and replicates.
// Per-stop numbers (fog density, far-fog luminance, sun lux) are in UDeepTuningSettings::Stops; the water's
// scattering character below is the same at every stop. Tune live with `Deep.Tune Env.<Property> <Value>`.

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

	/** Re-reads the tuning and applies the current stop's look (Deep.Env.Apply). */
	void ApplyCurrentStop() { OnRep_Stop(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Sun direction: pitch is negative downward. Slanted a little so the shafts read. */
	UPROPERTY(EditAnywhere, Category = "Deep|Water")
	float SunPitchDeg = -68.f;

	UPROPERTY(EditAnywhere, Category = "Deep|Water")
	float SunYawDeg = 35.f;

	UPROPERTY(EditAnywhere, Category = "Deep|Water")
	FLinearColor SunColor = FLinearColor(0.55f, 0.85f, 1.f);

	/** How strongly the sun lights the water itself (the shafts). */
	UPROPERTY(EditAnywhere, Category = "Deep|Water")
	float SunScattering = 1.f;

	/** Colour of the light the water scatters (volumetric fog albedo). Red is absorbed first. */
	UPROPERTY(EditAnywhere, Category = "Deep|Water")
	FLinearColor WaterAlbedo = FLinearColor(0.25f, 0.65f, 0.75f);

	/** Phase g: above 0 scatters forward, so looking up toward the sun is brighter than looking down. */
	UPROPERTY(EditAnywhere, Category = "Deep|Water")
	float ScatteringDistribution = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Deep|Water")
	float ExtinctionScale = 1.f;

	/** Volumetric fog covers this far from the eye; the plain height fog takes over beyond it. */
	UPROPERTY(EditAnywhere, Category = "Deep|Water")
	float VolumetricDistanceM = 80.f;

	/** Volumetric fog starts this far from the eye, so the helmet lamp does not light a blob on the visor. */
	UPROPERTY(EditAnywhere, Category = "Deep|Water")
	float VolumetricStartM = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Deep|Water")
	float FogMaxOpacity = 1.f;

	UPROPERTY(EditAnywhere, Category = "Deep|Water")
	float BloomIntensity = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Deep|Water")
	float VignetteIntensity = 0.6f;

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
