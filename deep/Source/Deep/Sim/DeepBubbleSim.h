// Air bubbles: pure maths for one bubble's rise and for when the diver breathes out. Visual only; no rule reads it.

#pragma once

#include "CoreMinimal.h"
#include "Sim/DeepTuningTypes.h"

struct FDeepBubble
{
	/** Path centre (cm, world); the drawn position adds the wobble. */
	FVector CentreCm = FVector::ZeroVector;
	/** Spawn speed left over (cm/s); water drag eats it and the rise takes over. */
	FVector KickCmS = FVector::ZeroVector;
	float SizeCm = 1.f;
	float AgeS = 0.f;
	float LifeS = 3.f;
	float Phase = 0.f;
	bool bAlive = false;
};

namespace DeepBubbleSim
{
	/** True while the breath cycle at TimeS (period PeriodS) is in its breath out. */
	DEEP_API bool IsExhaling(float TimeS, float PeriodS, const FDeepBubbleTuning& Tuning);

	/** Rise speed for a bubble of this diameter (cm/s). */
	DEEP_API float RiseCmS(float SizeCm, const FDeepBubbleTuning& Tuning);

	/** Advances one bubble; it dies at the end of its life. */
	DEEP_API void Step(FDeepBubble& Bubble, float DeltaSeconds, const FDeepBubbleTuning& Tuning);

	/** Where it is drawn: the path centre plus a sideways wobble that grows over the first half second. */
	DEEP_API FVector DrawnCm(const FDeepBubble& Bubble, const FDeepBubbleTuning& Tuning);

	/** Drawn diameter: shrinks to nothing over the last 0.3 s instead of popping out. */
	DEEP_API float DrawnSizeCm(const FDeepBubble& Bubble);
}
