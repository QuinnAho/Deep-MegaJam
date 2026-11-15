#include "Sim/DeepCaveRules.h"

namespace
{
	/** A fixed phase in [0, 2 pi) per segment and channel: the same shaft on every machine and every run. */
	float Phase(int32 Seed, int32 Segment, int32 Channel)
	{
		const double X = FMath::Sin(double(Segment) * 12.9898 + double(Seed) * 78.233 + double(Channel) * 37.719) * 43758.5453;
		return float((X - FMath::FloorToDouble(X)) * UE_DOUBLE_TWO_PI);
	}

	float Smooth01(float T)
	{
		const float C = FMath::Clamp(T, 0.f, 1.f);
		return C * C * (3.f - 2.f * C);
	}

	/** Two sines (the second shorter and weaker), in [-1, 1]. */
	float Wave(float DepthM, float WavelengthM, float PhaseA, float PhaseB)
	{
		const float W = UE_TWO_PI / FMath::Max(1.f, WavelengthM);
		return 0.8f * FMath::Sin(W * DepthM + PhaseA) + 0.2f * FMath::Sin(W * 2.3f * DepthM + PhaseB);
	}
}

int32 UDeepCaveRules::Segment(float DepthM, const TArray<float>& StopsM)
{
	if (StopsM.IsEmpty() || DepthM < StopsM[0])
	{
		return -1;
	}
	for (int32 K = 0; K + 1 < StopsM.Num(); ++K)
	{
		if (DepthM < StopsM[K + 1])
		{
			return K;
		}
	}
	return StopsM.Num() - 1;
}

float UDeepCaveRules::Envelope(float DepthM, const TArray<float>& StopsM, const FDeepCaveTuning& Cave)
{
	const int32 K = Segment(DepthM, StopsM);
	if (K < 0 || K + 1 >= StopsM.Num())
	{
		return 0.f;
	}
	const float A = StopsM[K] + FMath::Max(0.f, Cave.StraightBelowStopM);
	const float B = StopsM[K + 1] - FMath::Max(0.f, Cave.StraightAboveStopM);
	if (B <= A)
	{
		return 0.f;
	}
	const float Half = (B - A) * 0.5f;
	const float RampIn = FMath::Clamp(Cave.StraightBelowStopM, 1.f, Half);
	const float RampOut = FMath::Clamp(Cave.StraightAboveStopM, 1.f, Half);
	return Smooth01((DepthM - A) / RampIn) * Smooth01((B - DepthM) / RampOut);
}

FVector2D UDeepCaveRules::CentreM(float DepthM, const TArray<float>& StopsM, const FDeepCaveTuning& Cave)
{
	const float E = Envelope(DepthM, StopsM, Cave);
	if (E <= 0.f)
	{
		return FVector2D::ZeroVector;
	}
	const int32 K = Segment(DepthM, StopsM);
	const float X = Cave.BendAmplitudeM.X * Wave(DepthM, Cave.BendWavelengthM.X, Phase(Cave.Seed, K, 0), Phase(Cave.Seed, K, 1));
	const float Y = Cave.BendAmplitudeM.Y * Wave(DepthM, Cave.BendWavelengthM.Y, Phase(Cave.Seed, K, 2), Phase(Cave.Seed, K, 3));
	return FVector2D(X, Y) * E;
}

float UDeepCaveRules::RadiusM(float DepthM, const TArray<float>& StopsM, float MinRadiusM, const FDeepCaveTuning& Cave)
{
	const float Stop = Cave.StopRadiusM;
	const float E = Envelope(DepthM, StopsM, Cave);
	float R = Stop;
	if (E > 0.f)
	{
		const int32 K = Segment(DepthM, StopsM);
		// Shaped so the extremes are reached and held a little: real caverns and real squeezes, not a gentle wobble.
		const float N = FMath::Clamp(1.4f * Wave(DepthM, Cave.RadiusWavelengthM, Phase(Cave.Seed, K, 4), Phase(Cave.Seed, K, 5)), -1.f, 1.f);
		const float Raw = N >= 0.f ? Stop + N * (Cave.CavernRadiusM - Stop) : Stop + N * (Stop - MinRadiusM);
		R = Stop + E * (Raw - Stop);
	}
	return FMath::Max(R, MinRadiusM);
}

float UDeepCaveRules::MinRadiusM(const FVector& BoxMinM, const FVector& BoxMaxM, const FDeepCaveTuning& Cave)
{
	const float Pad = FMath::Max(0.f, Cave.HullPadM);
	const float HX = FMath::Max(FMath::Abs(BoxMinM.X), FMath::Abs(BoxMaxM.X)) + Pad;
	const float HY = FMath::Max(FMath::Abs(BoxMinM.Y), FMath::Abs(BoxMaxM.Y)) + Pad;
	return FMath::Sqrt(HX * HX + HY * HY) + FMath::Max(0.f, Cave.SqueezeMarginM);
}

FDeepCaveContact UDeepCaveRules::Contact(const FVector2D& OffsetM, float SubDepthM, const FVector& BoxMinM, const FVector& BoxMaxM, const TArray<float>& StopsM, float MinRadiusM, const FDeepCaveTuning& Cave)
{
	FDeepCaveContact Out;
	Out.PenetrationM = -UE_BIG_NUMBER;
	const float Pad = FMath::Max(0.f, Cave.HullPadM);
	const float Xs[2] = { float(BoxMinM.X) - Pad, float(BoxMaxM.X) + Pad };
	const float Ys[2] = { float(BoxMinM.Y) - Pad, float(BoxMaxM.Y) + Pad };
	// Box Z is up (sub-local); depth grows down.
	const float Zs[3] = { float(BoxMinM.Z) - Pad, float(BoxMinM.Z + BoxMaxM.Z) * 0.5f, float(BoxMaxM.Z) + Pad };
	for (const float Z : Zs)
	{
		const float D = SubDepthM - Z;
		const FVector2D C = CentreM(D, StopsM, Cave);
		const float R = RadiusM(D, StopsM, MinRadiusM, Cave);
		for (const float X : Xs)
		{
			for (const float Y : Ys)
			{
				const FVector2D Rel = OffsetM + FVector2D(X, Y) - C;
				const float Dist = Rel.Size();
				const float Pen = Dist - R;
				if (Pen > Out.PenetrationM)
				{
					Out.PenetrationM = Pen;
					Out.Normal = Dist > UE_KINDA_SMALL_NUMBER ? Rel / Dist : FVector2D(1.f, 0.f);
				}
			}
		}
	}
	Out.bTouching = Out.PenetrationM > 0.f;
	return Out;
}

FVector2D UDeepCaveRules::SteerVelocity(const FVector2D& VelocityMps, const FVector2D& Input, float DeltaSeconds, const FDeepCaveTuning& Cave)
{
	FVector2D In = Input;
	if (In.SizeSquared() > 1.f)
	{
		In.Normalize();
	}
	const FVector2D Want = In * FMath::Max(0.f, Cave.SteerSpeedMps);
	const FVector2D Gap = Want - VelocityMps;
	const float Step = FMath::Max(0.f, Cave.SteerAccelMps2) * FMath::Max(0.f, DeltaSeconds);
	const float GapSize = Gap.Size();
	return GapSize <= Step ? Want : VelocityMps + Gap / GapSize * Step;
}

int32 UDeepCaveRules::HitDamage(float ImpactSpeedMps, const FDeepCaveTuning& Cave)
{
	if (ImpactSpeedMps < Cave.HitMinSpeedMps || ImpactSpeedMps <= 0.f)
	{
		return 0;
	}
	return FMath::Clamp(FMath::RoundToInt(ImpactSpeedMps * Cave.DamagePerMps), 1, FMath::Max(1, Cave.MaxHitDamage));
}

float UDeepCaveRules::HullIntegrity01(int32 HullDamage, const FDeepCaveTuning& Cave)
{
	if (Cave.HullBreachDamage <= 0)
	{
		return 1.f;
	}
	return FMath::Clamp(1.f - float(HullDamage) / float(Cave.HullBreachDamage), 0.f, 1.f);
}

bool UDeepCaveRules::IsBreached(int32 HullDamage, const FDeepCaveTuning& Cave)
{
	return Cave.HullBreachDamage > 0 && HullDamage >= Cave.HullBreachDamage;
}

bool UDeepCaveRules::CrossesCritical(int32 DamageBefore, int32 DamageAfter, const FDeepCaveTuning& Cave)
{
	return HullIntegrity01(DamageBefore, Cave) >= Cave.HullCriticalIntegrity01 && HullIntegrity01(DamageAfter, Cave) < Cave.HullCriticalIntegrity01;
}
