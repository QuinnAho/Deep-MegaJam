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
	// A scrape along the rock: a short grinding judder, well under a knock. Hits and scrapes scale by the impact.
	Add(EventShakes, TEXT("hull_damage"),    Spec(1.5f, 0.5f, 18.f, 0.50f, 0.05f, 0.35f));
	Add(EventShakes, TEXT("hull_hit"),       Spec(4.f,  1.4f, 13.f, 0.45f, 0.02f, 0.30f));
	Add(EventShakes, TEXT("hull_critical"),  Spec(6.f,  2.2f, 10.f, 0.80f, 0.03f, 0.50f));
	Add(EventShakes, TEXT("hull_breach"),    Spec(10.f, 3.5f,  9.f, 1.40f, 0.03f, 0.90f));
	// The serpent: the alarm jolts, the bite lunges, death throws the view.
	Add(EventShakes, TEXT("bite_alarm"),     Spec(2.f,  0.9f, 11.f, 0.50f, 0.03f, 0.35f));
	Add(EventShakes, TEXT("cut_bite"),       Spec(12.f, 4.0f, 14.f, 0.90f, 0.02f, 0.60f));
	// The serpent bit the air line: a hard tug back along the line, harder and shorter than a timed bite.
	Add(EventShakes, TEXT("tether_bitten"),  Spec(18.f, 6.0f, 9.f,  0.70f, 0.01f, 0.50f));
	Add(EventShakes, TEXT("death_monster"),  Spec(16.f, 6.0f, 12.f, 1.50f, 0.02f, 1.00f));
	Add(EventShakes, TEXT("monster_near"),   Spec(0.6f, 0.25f, 2.5f, 2.50f, 0.80f, 1.20f));
	// The harpoon: a firm kick on the shot (the diver is shoved back too); a bolt into the hull rings it; a bolt
	// into the serpent lands as a heavy thud, between a hull hit and a critical.
	Add(EventShakes, TEXT("harpoon_fired"),  Spec(3.f,  1.5f, 12.f, 0.30f, 0.01f, 0.22f));
	Add(EventShakes, TEXT("harpoon_hit_sub"),Spec(2.f,  1.0f, 15.f, 0.40f, 0.02f, 0.30f));
	Add(EventShakes, TEXT("harpoon_hit_monster"), Spec(5.f, 1.8f, 11.f, 0.60f, 0.02f, 0.40f));
	// The sub: the hatch clunks shut behind you; a long low rumble on reaching a new depth.
	Add(EventShakes, TEXT("enter_sub"),      Spec(2.f,  0.8f, 10.f, 0.30f, 0.02f, 0.22f));
	Add(EventShakes, TEXT("descend_stop2"),  Spec(2.f,  0.5f,  3.f, 3.50f, 0.60f, 1.50f));
	Add(EventShakes, TEXT("descend_stop3"),  Spec(2.5f, 0.6f,  2.6f, 4.00f, 0.60f, 1.80f));

	// Handheld: well under a degree, about one drift every three seconds. Endless; eases in over a second.
	Sway = Spec(0.6f, 0.35f, 0.35f, 0.f, 1.0f, 0.6f);
	Sway.RollScale = 0.6f;

	// The sonar pulse. The jam's look (D-041): amber, a hard bright band, a dense grid lingering 2 s, every sweep.
	PulseJam.Color = FLinearColor(FColor(0xFF, 0xB4, 0x5A));
	PulseJam.Seconds = 1.5f;
	PulseJam.BandM = 0.6f;
	PulseJam.BandLead = 0.35f;
	PulseJam.BandIntensity = 0.9f;
	PulseJam.DotSpacingM = 0.35f;
	PulseJam.DotIntensity = 0.35f;
	PulseJam.DotFadeS = 2.f;
	PulseJam.HelmRangeM = 0.f;
	PulseJam.DiverGain = 1.f;
	PulseJam.SerpentBandGain = 1.f;
	PulseJam.RangeFade = 0.f;

	// Soft (Nov 17): the sub's ambient strip colour (#BFE3EA, cool and low), a thin dim band with a soft front that
	// loses strength as it travels, sparse faint dots gone in 0.8 s. The serpent's outline still catches the band.
	PulseSoft.Color = FLinearColor(FColor(0xBF, 0xE3, 0xEA));
	PulseSoft.Seconds = 1.5f;
	PulseSoft.BandM = 0.35f;
	PulseSoft.BandLead = 1.f;
	PulseSoft.BandIntensity = 0.45f;
	PulseSoft.DotSpacingM = 0.8f;
	PulseSoft.DotIntensity = 0.16f;
	PulseSoft.DotFadeS = 0.8f;
	PulseSoft.HelmRangeM = 50.f;
	PulseSoft.DiverGain = 1.6f;
	PulseSoft.SerpentBandGain = 2.5f;
	PulseSoft.RangeFade = 0.45f;
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

FDeepShakeSpec DeepCameraRules::ShakeForHit(FName Event, float ImpactMps, const FDeepCameraTuning& Tuning)
{
	FDeepShakeSpec Out = ShakeForEvent(Event, Tuning);
	if (Out.IsNone() || ImpactMps <= 0.f || Tuning.HitShakeRefMps <= 0.f)
	{
		return Out;
	}
	const float Low = FMath::Max(0.f, Tuning.HitShakeScaleRange.X);
	const float High = FMath::Max(Low, Tuning.HitShakeScaleRange.Y);
	const float Scale = FMath::Clamp(ImpactMps / Tuning.HitShakeRefMps, Low, High);
	Out.LocationCm *= Scale;
	Out.RotationDeg *= Scale;
	return Out;
}

void DeepCameraRules::PulseHullBox(const FBox& PartsLocal, const FTransform& SubToWorld, float MarginM,
	FVector& OutCentre, FVector& OutAxisX, FVector& OutAxisY, FVector& OutAxisZ)
{
	if (!PartsLocal.IsValid)
	{
		// No sub: a 1 cm box at the transform's origin, so nothing in view is skipped.
		OutCentre = SubToWorld.GetLocation();
		OutAxisX = FVector::ForwardVector;
		OutAxisY = FVector::RightVector;
		OutAxisZ = FVector::UpVector;
		return;
	}
	const FBox Grown = PartsLocal.ExpandBy(FMath::Max(0.f, MarginM) * UDeepRules::CmPerM);
	const FVector Half = Grown.GetExtent().ComponentMax(FVector(1.0));
	const FVector Scale = SubToWorld.GetScale3D().GetAbs().ComponentMax(FVector(KINDA_SMALL_NUMBER));
	OutCentre = SubToWorld.TransformPosition(Grown.GetCenter());
	OutAxisX = SubToWorld.GetUnitAxis(EAxis::X) / (Half.X * Scale.X);
	OutAxisY = SubToWorld.GetUnitAxis(EAxis::Y) / (Half.Y * Scale.Y);
	OutAxisZ = SubToWorld.GetUnitAxis(EAxis::Z) / (Half.Z * Scale.Z);
}

bool DeepCameraRules::IsInPulseHull(const FVector& WorldCm, const FVector& Centre, const FVector& AxisX, const FVector& AxisY, const FVector& AxisZ)
{
	const FVector Rel = WorldCm - Centre;
	return FMath::Abs(FVector::DotProduct(Rel, AxisX)) < 1.0 && FMath::Abs(FVector::DotProduct(Rel, AxisY)) < 1.0
		&& FMath::Abs(FVector::DotProduct(Rel, AxisZ)) < 1.0;
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

const FDeepPulseLook& DeepCameraRules::PulseLook(const FDeepCameraTuning& Tuning, int32 Style)
{
	return Style == 0 ? Tuning.PulseJam : Tuning.PulseSoft;
}

float DeepCameraRules::HelmPulseRangeM(const FDeepPulseLook& Look, float SonarRangeM, const FDeepCameraTuning& Tuning)
{
	if (Look.HelmRangeM > 0.f)
	{
		return Look.HelmRangeM;
	}
	return SonarRangeM > 0.f ? SonarRangeM : Tuning.PulseRangeM;
}

float DeepCameraRules::PulseRadiusCm(float AgeS, float RangeM, const FDeepPulseLook& Look)
{
	const float T = Look.Seconds > 0.f ? FMath::Clamp(AgeS / Look.Seconds, 0.f, 1.f) : 1.f;
	return T * FMath::Max(0.f, RangeM) * UDeepRules::CmPerM;
}

bool DeepCameraRules::IsPulseLive(float AgeS, const FDeepPulseLook& Look)
{
	return AgeS >= 0.f && AgeS < FMath::Max(0.f, Look.Seconds) + FMath::Max(0.f, Look.DotFadeS);
}

float DeepCameraRules::PulseRangeFall(float DistanceM, float RangeM, const FDeepPulseLook& Look)
{
	const float T = RangeM > 0.f ? FMath::Clamp(DistanceM / RangeM, 0.f, 1.f) : 1.f;
	return 1.f - FMath::Clamp(Look.RangeFade, 0.f, 1.f) * T * T;
}

bool DeepCameraRules::CanDiverPing(float NowS, float LastPingS, bool bInWater, const FDeepCameraTuning& Tuning)
{
	return bInWater && DiverPingWaitS(NowS, LastPingS, Tuning) <= 0.f;
}

float DeepCameraRules::DiverPingWaitS(float NowS, float LastPingS, const FDeepCameraTuning& Tuning)
{
	return LastPingS < 0.f ? 0.f : FMath::Max(0.f, LastPingS + FMath::Max(0.f, Tuning.DiverPingCooldownS) - NowS);
}

float DeepCameraRules::PulseBandCm(float EyeCm, const FDeepPulseLook& Look, const FDeepCameraTuning& Tuning)
{
	const float FarCm = FMath::Max(1.f, Tuning.PulseBandFarM * UDeepRules::CmPerM);
	return Look.BandM * UDeepRules::CmPerM * FMath::Max(1.f, EyeCm / FarCm);
}

float DeepCameraRules::PulseDotSpacingCm(float EyeCm, const FDeepPulseLook& Look, const FDeepCameraTuning& Tuning)
{
	const float NearCm = FMath::Max(1.f, Tuning.PulseDotNearM * UDeepRules::CmPerM);
	const float Tier = FMath::Exp2(FMath::Max(0.f, FMath::FloorToFloat(FMath::Log2(FMath::Max(EyeCm, 1.f) / NearCm))));
	return Look.DotSpacingM * UDeepRules::CmPerM * Tier;
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
