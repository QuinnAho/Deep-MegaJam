// Light for a stop. One per world, spawned by DeepArena::EnsureArena. SetStop switches between the stops and replicates.
// Water look (the default, style guide 5): every machine blends the look presets (FDeepLookTuning) by the replicated
// SubDepthM each tick, so driving fades continuously: downlight with volumetric shafts, height fog with volumetric fog,
// locked exposure, grade, bloom, vignette, grain; and marine snow (instanced specks) wrapped round the view.
// Grey box (Arena.bGreyBox): even key + fill light, fixed exposure, no fog shafts, a mild tint per stop.
// Deep.RayTracing 0|1 switches hardware ray tracing for Lumen reflections and shadows (the look holds either way).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sim/DeepTuningTypes.h"
#include "DeepEnvironment.generated.h"

class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class UInstancedStaticMeshComponent;

UCLASS()
class DEEP_API ADeepEnvironment : public AActor
{
	GENERATED_BODY()

public:
	ADeepEnvironment();

	/** Server: switch to a stop's look. Replicates, so clients get the same water. */
	void SetStop(int32 InStop);

	int32 GetStop() const { return Stop; }

	/** The light colour for a stop in grey-box mode: white leaning a little toward the stop's water colour. */
	static FLinearColor GreyBoxTint(const FDeepStopTuning& StopTuning, const FDeepArenaTuning& Arena);

	/** The look at a depth: the presets blended linearly between their depths, clamped at both ends. */
	static FDeepLookPreset BlendLook(const FDeepLookTuning& Look, float DepthM);

	/** The look last applied (for tests and the state report). */
	const FDeepLookPreset& GetAppliedLook() const { return Applied; }

	/** Hardware ray tracing for Lumen reflections (hit lighting) and ray-traced shadows. Console: Deep.RayTracing. */
	static void SetRayTracing(bool bOn);

	/** Camera lane: adds a post-process material to the water look's blendables, or sets its weight (0 = off). Local view only. */
	void SetCameraBlendable(class UMaterialInterface* Material, float Weight);

	/** True while the footage look (Deep.Look.Footage) is applied on top of the depth look. */
	bool IsFootageApplied() const { return bAppliedFootage; }

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Stop();

	void ApplyStop(const FDeepStopTuning& StopTuning, const FDeepArenaTuning& Arena);
	void ApplyLook(const FDeepLookPreset& Preset, const FDeepLookTuning& Look);
	void BuildSnow(const FDeepLookTuning& Look);
	/** The volumetric fog's quality CVars from DEEP Tuning > Look (history weight, grid). */
	static void ApplyFogQuality(const FDeepLookTuning& Look);
	void UpdateSnow(float DeltaSeconds, const FDeepLookTuning& Look);

	UPROPERTY(ReplicatedUsing = OnRep_Stop, VisibleAnywhere, Category = "Deep")
	int32 Stop = 1;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UDirectionalLightComponent> Downlight;

	/** Grey box only: soft light from below and the far side. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UDirectionalLightComponent> Fill;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UExponentialHeightFogComponent> Fog;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UPostProcessComponent> Exposure;

	/** Marine snow: tiny lit specks, revealed only by local lights (the lamp, the sub's flood). */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UInstancedStaticMeshComponent> Snow;

	FDeepLookPreset Applied;
	float AppliedDepthM = -1.f;
	bool bAppliedFootage = false;

	/** Per speck: home offset in the box (cm), drift velocity (cm/s), scale. */
	TArray<FVector> SnowHome;
	TArray<FVector> SnowDrift;
	TArray<float> SnowScale;
	TArray<FQuat> SnowSpin;
	TArray<FTransform> SnowXforms;
	float SnowTime = 0.f;
};
