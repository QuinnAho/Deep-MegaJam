#include "Harpoon/DeepHarpoonRules.h"

namespace
{
	constexpr float HarpoonRulesCmPerM = 100.f;
}

bool UDeepHarpoonRules::InRange(const FVector& FromCm, const FVector& ToCm, float RangeM)
{
	const double RangeCm = FMath::Max(0.f, RangeM) * HarpoonRulesCmPerM;
	return FVector::DistSquared(FromCm, ToCm) <= RangeCm * RangeCm;
}

bool UDeepHarpoonRules::CanFire(int32 ShotsLeft, bool bCanSwim, EDeepHarpoonPhase Phase)
{
	return ShotsLeft > 0 && bCanSwim && Phase == EDeepHarpoonPhase::Idle;
}

FVector UDeepHarpoonRules::ReelStep(const FVector& PositionCm, const FVector& TargetCm, float SpeedMps, float DeltaSeconds)
{
	const FVector ToTarget = TargetCm - PositionCm;
	const double Distance = ToTarget.Size();
	const double Step = FMath::Max(0.f, SpeedMps) * HarpoonRulesCmPerM * FMath::Max(0.f, DeltaSeconds);
	if (Distance <= Step || Distance <= UE_KINDA_SMALL_NUMBER)
	{
		return TargetCm;
	}
	return PositionCm + ToTarget / Distance * Step;
}

float UDeepHarpoonRules::BoltStepCm(float TravelledCm, float RangeM, float SpeedMps, float DeltaSeconds)
{
	const float Left = FMath::Max(0.f, FMath::Max(0.f, RangeM) * HarpoonRulesCmPerM - FMath::Max(0.f, TravelledCm));
	return FMath::Min(Left, FMath::Max(0.f, SpeedMps) * HarpoonRulesCmPerM * FMath::Max(0.f, DeltaSeconds));
}

float UDeepHarpoonRules::SegmentSphereHit(const FVector& StartCm, const FVector& EndCm, const FVector& CentreCm, float RadiusCm)
{
	const double Radius = FMath::Max(0.f, RadiusCm);
	const FVector D = EndCm - StartCm;
	const FVector F = StartCm - CentreCm;
	const double C = F.SizeSquared() - Radius * Radius;
	if (C <= 0.0)
	{
		return 0.f;
	}
	const double A = D.SizeSquared();
	if (A <= UE_SMALL_NUMBER)
	{
		return -1.f;
	}
	const double B = 2.0 * FVector::DotProduct(F, D);
	const double Disc = B * B - 4.0 * A * C;
	if (Disc < 0.0)
	{
		return -1.f;
	}
	const double T = (-B - FMath::Sqrt(Disc)) / (2.0 * A);
	return (T >= 0.0 && T <= 1.0) ? static_cast<float>(T) : -1.f;
}

int32 UDeepHarpoonRules::RepairFee(int32 Hits, int32 FeePerHit)
{
	const int64 Total = static_cast<int64>(FMath::Max(0, Hits)) * FMath::Max(0, FeePerHit);
	return static_cast<int32>(FMath::Min<int64>(Total, MAX_int32));
}

int32 UDeepHarpoonRules::RepairFromCredits(int32 Gross, int32 DirectorCut, int32 Fees, int32 Repair)
{
	const int64 Shortfall = static_cast<int64>(FMath::Max(0, DirectorCut)) + FMath::Max(0, Fees) - FMath::Max(0, Gross);
	return static_cast<int32>(FMath::Clamp<int64>(Shortfall, 0, FMath::Max(0, Repair)));
}

FVector UDeepHarpoonRules::RecoilVelocityMps(const FVector& Aim, float SecondsSinceShot, float RecoilMps, float RecoilSeconds)
{
	const FVector Back = -Aim.GetSafeNormal();
	if (Back.IsNearlyZero() || RecoilSeconds <= 0.f || SecondsSinceShot < 0.f || SecondsSinceShot >= RecoilSeconds)
	{
		return FVector::ZeroVector;
	}
	return Back * (FMath::Max(0.f, RecoilMps) * (1.f - SecondsSinceShot / RecoilSeconds));
}

float UDeepHarpoonRules::RecoilTravelM(float FromSeconds, float ToSeconds, float RecoilMps, float RecoilSeconds)
{
	if (RecoilSeconds <= 0.f || RecoilMps <= 0.f)
	{
		return 0.f;
	}
	// v(t) = RecoilMps * (1 - t / T) on [0, T]; its area from 0 to t is RecoilMps * (t - t^2 / 2T).
	auto Area = [RecoilMps, RecoilSeconds](float T)
	{
		const float C = FMath::Clamp(T, 0.f, RecoilSeconds);
		return RecoilMps * (C - C * C / (2.f * RecoilSeconds));
	};
	return FMath::Max(0.f, Area(ToSeconds) - Area(FromSeconds));
}
