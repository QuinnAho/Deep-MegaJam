#include "Sim/DeepBubbleSim.h"

namespace DeepBubbleSim
{
	bool IsExhaling(float TimeS, float PeriodS, const FDeepBubbleTuning& Tuning)
	{
		if (PeriodS <= 0.f)
		{
			return false;
		}
		const float Phase = FMath::Fmod(FMath::Max(0.f, TimeS), PeriodS) / PeriodS;
		return Phase >= Tuning.ExhaleStart01 && Phase < Tuning.ExhaleEnd01;
	}

	float RiseCmS(float SizeCm, const FDeepBubbleTuning& Tuning)
	{
		return Tuning.RiseBaseCmS + Tuning.RisePerCmS * FMath::Max(0.f, SizeCm);
	}

	void Step(FDeepBubble& Bubble, float DeltaSeconds, const FDeepBubbleTuning& Tuning)
	{
		if (!Bubble.bAlive || DeltaSeconds <= 0.f)
		{
			return;
		}
		Bubble.AgeS += DeltaSeconds;
		if (Bubble.AgeS >= Bubble.LifeS)
		{
			Bubble.bAlive = false;
			return;
		}
		Bubble.KickCmS *= FMath::Exp(-Tuning.DragPerS * DeltaSeconds);
		Bubble.CentreCm += (Bubble.KickCmS + FVector(0.f, 0.f, RiseCmS(Bubble.SizeCm, Tuning))) * DeltaSeconds;
	}

	FVector DrawnCm(const FDeepBubble& Bubble, const FDeepBubbleTuning& Tuning)
	{
		const float Ramp = FMath::Clamp(Bubble.AgeS / 0.5f, 0.f, 1.f);
		const float W = 2.f * PI * Tuning.WobbleHz * Bubble.AgeS + Bubble.Phase;
		// A flattened spiral: bubbles of a few millimetres rock side to side as they rise.
		return Bubble.CentreCm + FVector(FMath::Sin(W), FMath::Cos(W * 0.7f + Bubble.Phase), 0.f) * (Tuning.WobbleCm * Ramp);
	}

	float DrawnSizeCm(const FDeepBubble& Bubble)
	{
		const float Left = Bubble.LifeS - Bubble.AgeS;
		return Bubble.SizeCm * FMath::Clamp(Left / 0.3f, 0.f, 1.f);
	}
}
