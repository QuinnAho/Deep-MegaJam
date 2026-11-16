// Code-only camera shakes. One pattern (Perlin noise on location and rotation under DeepCameraRules::Envelope) whose
// numbers are an FDeepShakeSpec copied in when the shake starts, so every shake is tuning, not an asset.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "Sim/DeepTuningTypes.h"
#include "DeepCameraShake.generated.h"

UCLASS()
class DEEP_API UDeepShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

public:
	FDeepShakeSpec Spec;

	float GetElapsed() const { return Elapsed; }

private:
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;
	virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override;
	virtual void TeardownShakePatternImpl() override;

	float Elapsed = 0.f;
	float StopAt = -1.f;
	float Seed = 0.f;
	bool bKilled = false;
};

/** An event shake: started with an initializer that sets the pattern's Spec. */
UCLASS()
class DEEP_API UDeepCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UDeepCameraShake(const FObjectInitializer& ObjectInitializer);

	UDeepShakePattern* GetDeepPattern() const { return Cast<UDeepShakePattern>(GetRootShakePattern()); }
};

/** The footage look's endless handheld sway (its own class so it can be stopped alone). */
UCLASS()
class DEEP_API UDeepSwayShake : public UDeepCameraShake
{
	GENERATED_BODY()

public:
	UDeepSwayShake(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
};
