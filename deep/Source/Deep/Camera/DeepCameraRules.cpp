#include "Camera/DeepCameraRules.h"

#include "Sim/DeepRules.h"

namespace
{
	FDeepShakeSpec Spec(float LocationCm, float RotationDeg, float FrequencyHz, float DurationS, float BlendInS, float BlendOutS)
	{
		FDeepShakeSpec S;
		S.LocationCm = LocationCm;
		S.RotationDeg = RotationDeg;
		S.FrequencyHz = FrequencyHz;
		S.DurationS = DurationS;
		S.BlendInS = BlendInS;
		S.BlendOutS = BlendOutS;
		return S;
	}

	void Add(TArray<FDeepEventShake>& Out, const TCHAR* Event, const FDeepShakeSpec& Shake)
	{
		FDeepEventShake Row;
		Row.Event = FName(Event);
		Row.Shake = Shake;
		Out.Add(Row);
	}
}

FDeepCameraTuning::FDeepCameraTuning()
{
	//                                     loc cm, rot deg, Hz,  dur s, in s,  out s
	// The hull: a knock, a groan, a breach (shaft lane events, matched by name).
	Add(EventShakes, TEXT("hull_hit"),       Spec(4.f,  1.4f, 13.f, 0.45f, 0.02f, 0.30f));
	Add(EventShakes, TEXT("hull_critical"),  Spec(6.f,  2.2f, 10.f, 0.80f, 0.03f, 0.50f));
	Add(EventShakes, TEXT("hull_breach"),    Spec(10.f, 3.5f,  9.f, 1.40f, 0.03f, 0.90f));
	// The serpent: the alarm jolts, the bite lunges, death throws the view.
	Add(EventShakes, TEXT("bite_alarm"),     Spec(2.f,  0.9f, 11.f, 0.50f, 0.03f, 0.35f));
	Add(EventShakes, TEXT("cut_bite"),       Spec(12.f, 4.0f, 14.f, 0.90f, 0.02f, 0.60f));
	Add(EventShakes, TEXT("death_monster"),  Spec(16.f, 6.0f, 12.f, 1.50f, 0.02f, 1.00f));
	Add(EventShakes, TEXT("monster_near"),   Spec(0.6f, 0.25f, 2.5f, 2.50f, 0.80f, 1.20f));
	// The harpoon: a small kick; a bolt into the hull rings it.
	Add(EventShakes, TEXT("harpoon_fired"),  Spec(1.f,  0.6f, 16.f, 0.18f, 0.01f, 0.14f));
	Add(EventShakes, TEXT("harpoon_hit_sub"),Spec(2.f,  1.0f, 15.f, 0.40f, 0.02f, 0.30f));
	// The sub: the hatch clunks shut behind you; a long low rumble on reaching a new depth.
	Add(EventShakes, TEXT("enter_sub"),      Spec(2.f,  0.8f, 10.f, 0.30f, 0.02f, 0.22f));
	Add(EventShakes, TEXT("descend_stop2"),  Spec(2.f,  0.5f,  3.f, 3.50f, 0.60f, 1.50f));
	Add(EventShakes, TEXT("descend_stop3"),  Spec(2.5f, 0.6f,  2.6f, 4.00f, 0.60f, 1.80f));

	// Handheld: well under a degree, about one drift every three seconds. Endless; eases in over a second.
	Sway = Spec(0.6f, 0.35f, 0.35f, 0.f, 1.0f, 0.6f);
	Sway.RollScale = 0.6f;
}

FDeepShakeSpec DeepCameraRules::ShakeForEvent(FName Event, const FDeepCameraTuning& Tuning)
{
	for (const FDeepEventShake& Row : Tuning.EventShakes)
	{
		if (Row.Event == Event)
		{
			FDeepShakeSpec Out = Row.Shake;
			const float Scale = FMath::Max(0.f, Tuning.ShakeScale);
			Out.LocationCm *= Scale;
			Out.RotationDeg *= Scale;
			return Out;
		}
	}
	return FDeepShakeSpec();
}

float DeepCameraRules::Ease01(float X)
{
	const float T = FMath::Clamp(X, 0.f, 1.f);
	return T * T * (3.f - 2.f * T);
}

namespace
{
	float EndOf(const FDeepShakeSpec& Spec, float StopAtS)
	{
		float End = Spec.DurationS > 0.f ? Spec.DurationS : TNumericLimits<float>::Max();
		if (StopAtS >= 0.f)
		{
			End = FMath::Min(End, StopAtS + FMath::Max(0.f, Spec.BlendOutS));
		}
		return End;
	}
}

float DeepCameraRules::Envelope(float ElapsedS, const FDeepShakeSpec& Spec, float StopAtS)
{
	if (ElapsedS < 0.f)
	{
		return 0.f;
	}
	const float End = EndOf(Spec, StopAtS);
	if (ElapsedS >= End)
	{
		return 0.f;
	}
	const float In = Spec.BlendInS > 0.f ? Ease01(ElapsedS / Spec.BlendInS) : 1.f;
	const float Out = Spec.BlendOutS > 0.f ? Ease01((End - ElapsedS) / Spec.BlendOutS) : 1.f;
	return FMath::Min(In, Out);
}

bool DeepCameraRules::IsShakeDone(float ElapsedS, const FDeepShakeSpec& Spec, float StopAtS)
{
	return ElapsedS >= EndOf(Spec, StopAtS);
}

FVector2D DeepCameraRules::FootageGrain(float DepthM, const FDeepCameraTuning& Tuning)
{
	const float Span = FMath::Max(Tuning.FootageDeepM - Tuning.FootageShallowM, 1.f);
	const float T = FMath::Clamp((DepthM - Tuning.FootageShallowM) / Span, 0.f, 1.f);
	return FVector2D(
		FMath::Lerp(Tuning.FootageGrainAdd.X, Tuning.FootageGrainAdd.Y, T),
		FMath::Lerp(Tuning.FootageGrainShadows.X, Tuning.FootageGrainShadows.Y, T));
}

float DeepCameraRules::PulseRadiusCm(float AgeS, float RangeM, const FDeepCameraTuning& Tuning)
{
	const float T = Tuning.PulseSeconds > 0.f ? FMath::Clamp(AgeS / Tuning.PulseSeconds, 0.f, 1.f) : 1.f;
	return T * FMath::Max(0.f, RangeM) * UDeepRules::CmPerM;
}

bool DeepCameraRules::IsPulseLive(float AgeS, const FDeepCameraTuning& Tuning)
{
	return AgeS >= 0.f && AgeS < FMath::Max(0.f, Tuning.PulseSeconds) + FMath::Max(0.f, Tuning.PulseDotFadeS);
}

float DeepCameraRules::InterferenceTarget(float SerpentDistanceM, bool bBiting, float BurstLeftS, const FDeepCameraTuning& Tuning)
{
	float Target = 0.f;
	if (bBiting)
	{
		Target = 1.f;
	}
	else if (SerpentDistanceM >= 0.f)
	{
		const float Far = Tuning.InterferenceFarNearM.X;
		const float Near = Tuning.InterferenceFarNearM.Y;
		const float T = Far > Near ? FMath::Clamp((Far - SerpentDistanceM) / (Far - Near), 0.f, 1.f) : 0.f;
		Target = Ease01(T) * FMath::Clamp(Tuning.InterferenceNearMax, 0.f, 1.f);
	}
	if (BurstLeftS > 0.f)
	{
		Target = FMath::Max(Target, FMath::Clamp(Tuning.InterferenceBurst, 0.f, 1.f));
	}
	return Target;
}

float DeepCameraRules::InterferenceStep(float Current, float Target, float DeltaSeconds, const FDeepCameraTuning& Tuning)
{
	const float Rate = FMath::Max(0.f, Tuning.InterferenceEasePerS) * (Target > Current ? 4.f : 1.f);
	return FMath::Clamp(FMath::FInterpConstantTo(Current, FMath::Clamp(Target, 0.f, 1.f), FMath::Max(0.f, DeltaSeconds), Rate), 0.f, 1.f);
}
