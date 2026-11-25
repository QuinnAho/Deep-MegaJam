#include "Sim/DeepHelmLightRules.h"

#include "Sim/DeepRules.h"

namespace DeepHelmLight
{
	float NearShare(float NearestM, float FullM, float Power, float MinShare, float MaxShare)
	{
		const float Min = FMath::Clamp(MinShare, 0.f, 1.f);
		const float Max = FMath::Max(1.f, MaxShare);
		const float Ratio = FMath::Max(0.f, NearestM) / FMath::Max(0.01f, FullM);
		if (Ratio >= 1.f)
		{
			return FMath::Min(Max, FMath::Pow(Ratio, FMath::Max(0.f, Power)));
		}
		return FMath::Clamp(FMath::Pow(Ratio, FMath::Max(0.f, Power)), Min, 1.f);
	}

	float HeaveFromSpeed(float HeaveMps, float FullMps)
	{
		return FMath::Clamp(-HeaveMps / FMath::Max(0.01f, FullMps), -1.f, 1.f);
	}

	float HeavePitchDeg(float LevelDeg, float Heave, float SinkDeg, float RiseDeg)
	{
		const float H = FMath::Clamp(Heave, -1.f, 1.f);
		return H < 0.f ? FMath::Lerp(LevelDeg, SinkDeg, -H) : FMath::Lerp(LevelDeg, RiseDeg, H);
	}

	float SnowReachCm(float Intensity, float AttenuationCm, float LitMin)
	{
		const float ReachM = FMath::Sqrt(FMath::Max(0.f, Intensity) / FMath::Max(1e-3f, LitMin));
		return FMath::Min(FMath::Max(0.f, AttenuationCm), ReachM * float(UDeepRules::CmPerM));
	}

	bool SnowLit(const FVector& Cm, TConstArrayView<FSnowLight> Lights)
	{
		for (const FSnowLight& Light : Lights)
		{
			const FVector To = Cm - Light.AtCm;
			const double DistSq = To.SizeSquared();
			if (DistSq > FMath::Square(double(Light.ReachCm)))
			{
				continue;
			}
			if (Light.CosOuter <= -1.f)
			{
				return true;
			}
			// Inside the cone: the cosine of the angle off the axis (Along / distance) is at least CosOuter; squared, no root.
			const double Along = FVector::DotProduct(To, Light.Dir);
			const double CosSq = double(Light.CosOuter) * double(Light.CosOuter);
			const bool bInside = Light.CosOuter >= 0.f
				? Along > 0.0 && Along * Along >= CosSq * DistSq
				: Along >= 0.0 || Along * Along <= CosSq * DistSq;
			if (bInside)
			{
				return true;
			}
		}
		return false;
	}

	float SnowStreakCm(float SpeedMps, bool bCockpit, const FDeepLookTuning& Look)
	{
		if (!bCockpit && !Look.bHelmStreakInChase)
		{
			return 0.f;
		}
		return FMath::Abs(SpeedMps) * FMath::Max(0.f, Look.HelmStreakSeconds) * float(UDeepRules::CmPerM);
	}

	float SnowSizeCm(float OwnSizeCm, float DistanceCm, const FDeepLookTuning& Look)
	{
		return FMath::Max(OwnSizeCm * FMath::Max(0.1f, Look.HelmSnowSizeScale), FMath::Max(0.f, DistanceCm) * FMath::Max(0.f, Look.HelmSnowMinAngle));
	}
}
