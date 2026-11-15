#include "FX/DeepFxRules.h"

#include "FX/DeepFxSettings.h"
#include "NiagaraSystem.h"

UDeepFxSettings::UDeepFxSettings()
{
	BubbleSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Deep/FX/NS_Bubbles.NS_Bubbles")));
	SiltSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Deep/FX/NS_SiltPuff.NS_SiltPuff")));
	ExhalePuffPhases = { 0.47f, 0.56f, 0.66f, 0.77f };
}

namespace DeepFxRules
{
	float BreathPeriodS(bool bFast, const UDeepFxSettings& Settings)
	{
		return FMath::Max(0.5f, bFast ? Settings.FastBreathPeriodS : Settings.CalmBreathPeriodS);
	}

	float BreathPhase(float PlaySeconds, float PeriodS)
	{
		if (PeriodS <= 0.f || PlaySeconds < 0.f)
		{
			return 0.f;
		}
		return FMath::Fmod(PlaySeconds, PeriodS) / PeriodS;
	}

	TArray<int32> PuffIndicesCrossed(float PrevPhase, float Phase, const TArray<float>& PuffPhases)
	{
		TArray<int32> Out;
		const bool bWrapped = Phase < PrevPhase;
		// Before the wrap: (Prev, 1); after it: [0, Phase]. Without a wrap: (Prev, Phase].
		for (int32 I = 0; I < PuffPhases.Num(); ++I)
		{
			const float P = PuffPhases[I];
			if (!bWrapped ? (P > PrevPhase && P <= Phase) : (P > PrevPhase))
			{
				Out.Add(I);
			}
		}
		if (bWrapped)
		{
			for (int32 I = 0; I < PuffPhases.Num(); ++I)
			{
				if (PuffPhases[I] <= Phase)
				{
					Out.Add(I);
				}
			}
		}
		return Out;
	}

	int32 PuffsCrossed(float PrevPhase, float Phase, const TArray<float>& PuffPhases)
	{
		return PuffIndicesCrossed(PrevPhase, Phase, PuffPhases).Num();
	}

	int32 PuffCount(int32 Index, const UDeepFxSettings& Settings)
	{
		return FMath::Max(Settings.MinPuffCount, Settings.FirstPuffCount - Settings.PuffFalloff * FMath::Max(0, Index));
	}

	bool KicksSilt(float HeightAboveBedCm, float SpeedCmS, float SecondsSinceLastPuff, const UDeepFxSettings& Settings)
	{
		return HeightAboveBedCm >= -50.f && HeightAboveBedCm <= Settings.TouchHeightCm && SpeedCmS >= Settings.TouchMinSpeedCmS
			&& SecondsSinceLastPuff >= Settings.TouchCooldownS;
	}
}
