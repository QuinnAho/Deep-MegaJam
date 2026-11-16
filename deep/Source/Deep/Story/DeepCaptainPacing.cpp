#include "Story/DeepCaptainPacing.h"

#include "Sim/DeepTuningTypes.h"

namespace DeepCaptainPacing
{
	EDeepCaptainDanger Danger(const FDeepCaptainDangerInputs& In, const FDeepCaptainTuning& Tuning)
	{
		if (!In.bInWater)
		{
			// Aboard the only danger is the hull (at the helm, in the cave shaft).
			if (In.Hull01 < Tuning.ScaredHull01)
			{
				return EDeepCaptainDanger::Scared;
			}
			return In.Hull01 < Tuning.TenseHull01 ? EDeepCaptainDanger::Tense : EDeepCaptainDanger::Calm;
		}
		const bool bSerpentClose = In.bSerpentClosing && In.SerpentDistanceM >= 0.f && In.SerpentDistanceM <= Tuning.ScaredSerpentM;
		if (bSerpentClose || In.bBiteAlarm)
		{
			return EDeepCaptainDanger::Scared;
		}
		if (In.bBailout || (In.bTethered && In.PatienceFraction < Tuning.TensePatienceFraction))
		{
			return EDeepCaptainDanger::Tense;
		}
		return EDeepCaptainDanger::Calm;
	}

	FName ToneName(EDeepCaptainDanger Danger)
	{
		switch (Danger)
		{
		case EDeepCaptainDanger::Tense: return TEXT("tense");
		case EDeepCaptainDanger::Scared: return TEXT("scared");
		default: return TEXT("calm");
		}
	}

	bool Allows(EDeepCaptainScene Scene, EDeepCaptainDanger Danger, int32 Urgency, double SinceLastShownS, const FDeepCaptainTuning& Tuning)
	{
		if (Scene != EDeepCaptainScene::Water)
		{
			return true;
		}
		if (Danger == EDeepCaptainDanger::Scared)
		{
			return Urgency >= Tuning.ScaredMinUrgency;
		}
		if (Urgency <= 0)
		{
			return false;
		}
		if (Urgency == 1)
		{
			return SinceLastShownS >= Tuning.WaterNotableGapSeconds;
		}
		return true;
	}

	const TCHAR* SceneName(EDeepCaptainScene Scene)
	{
		switch (Scene)
		{
		case EDeepCaptainScene::Title: return TEXT("title");
		case EDeepCaptainScene::Water: return TEXT("water");
		case EDeepCaptainScene::Settlement: return TEXT("settlement");
		default: return TEXT("aboard");
		}
	}
}
