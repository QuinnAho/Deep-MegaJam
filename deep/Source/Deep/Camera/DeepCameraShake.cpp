#include "Camera/DeepCameraShake.h"

#include "Camera/DeepCameraRules.h"

void UDeepShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = Spec.DurationS > 0.f ? FCameraShakeDuration(Spec.DurationS) : FCameraShakeDuration::Infinite();
	OutInfo.BlendIn = Spec.BlendInS;
	OutInfo.BlendOut = Spec.BlendOutS;
}

void UDeepShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	Elapsed = 0.f;
	StopAt = -1.f;
	bKilled = false;
	// A different stretch of noise each time, so repeated hits do not move the same way.
	Seed = FMath::FRandRange(0.f, 1000.f);
}

void UDeepShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	Elapsed += Params.DeltaTime;
	const float Weight = bKilled ? 0.f : DeepCameraRules::Envelope(Elapsed, Spec, StopAt);
	const float T = Elapsed * Spec.FrequencyHz + Seed;
	// Perlin noise is 0 on whole numbers: each channel reads its own offset stretch.
	auto Noise = [T](float Offset) { return FMath::PerlinNoise1D(T + Offset); };
	const float Loc = Spec.LocationCm * Weight;
	const float Rot = Spec.RotationDeg * Weight;
	OutResult.Location = FVector(Noise(0.37f), Noise(31.71f), Noise(63.13f)) * Loc;
	OutResult.Rotation = FRotator(Noise(97.29f) * Rot, Noise(131.93f) * Rot, Noise(171.57f) * Rot * Spec.RollScale);
}

bool UDeepShakePattern::IsFinishedImpl() const
{
	return bKilled || DeepCameraRules::IsShakeDone(Elapsed, Spec, StopAt);
}

void UDeepShakePattern::StopShakePatternImpl(const FCameraShakePatternStopParams& Params)
{
	// Stopping eases out over BlendOutS; immediately ends now.
	if (StopAt < 0.f)
	{
		StopAt = Elapsed;
	}
	bKilled = bKilled || Params.bImmediately;
}

void UDeepShakePattern::TeardownShakePatternImpl()
{
	Elapsed = 0.f;
	StopAt = -1.f;
	bKilled = false;
}

UDeepCameraShake::UDeepCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UDeepShakePattern>(TEXT("RootShakePattern")))
{
}
