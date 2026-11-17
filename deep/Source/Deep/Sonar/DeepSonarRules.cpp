#include "Sonar/DeepSonarRules.h"

namespace
{
	float Wrap360(float Deg)
	{
		const float W = FMath::Fmod(Deg, 360.f);
		return W < 0.f ? W + 360.f : W;
	}

	/** Plan forward and right, unit. Unreal is left-handed with Z up: seen from above, +Y is right of +X. */
	void PlanAxes(const FVector& Forward, FVector2D& F, FVector2D& Right)
	{
		F = FVector2D(Forward.X, Forward.Y);
		if (!F.Normalize())
		{
			F = FVector2D(1.0, 0.0);
		}
		Right = FVector2D(-F.Y, F.X);
	}
}

FVector2D UDeepSonarRules::PanelPoint(float BearingDeg, float RangeM, float RangeMaxM, float RadiusPx)
{
	const float R = RangeMaxM > 0.f ? FMath::Clamp(RangeM / RangeMaxM, 0.f, 1.f) * RadiusPx : 0.f;
	const float Rad = FMath::DegreesToRadians(BearingDeg);
	return FVector2D(FMath::Sin(Rad) * R, -FMath::Cos(Rad) * R);
}

float UDeepSonarRules::BearingDeg(FVector Forward, FVector Offset)
{
	FVector2D F, Right;
	PlanAxes(Forward, F, Right);
	const FVector2D O(Offset.X, Offset.Y);
	if (O.IsNearlyZero())
	{
		return 0.f;
	}
	return Wrap360(FMath::RadiansToDegrees(FMath::Atan2(FVector2D::DotProduct(O, Right), FVector2D::DotProduct(O, F))));
}

float UDeepSonarRules::PlanRangeM(FVector OffsetCm)
{
	return static_cast<float>(FVector2D(OffsetCm.X, OffsetCm.Y).Size() / 100.0);
}

FVector UDeepSonarRules::BeamDirection(FVector Forward, int32 Index, int32 Beams)
{
	FVector2D F, Right;
	PlanAxes(Forward, F, Right);
	const double Rad = FMath::DegreesToRadians(360.0 * Index / FMath::Max(1, Beams));
	const FVector2D D = F * FMath::Cos(Rad) + Right * FMath::Sin(Rad);
	return FVector(D.X, D.Y, 0.0);
}

int32 UDeepSonarRules::BeamAt(float BearingDeg, int32 Beams)
{
	const int32 N = FMath::Max(1, Beams);
	return FMath::RoundToInt(Wrap360(BearingDeg) / 360.f * N) % N;
}

float UDeepSonarRules::SweepDeg(float Seconds, float PeriodS)
{
	return PeriodS > 0.f ? Wrap360(360.f * Seconds / PeriodS) : 0.f;
}

bool UDeepSonarRules::PingDue(float PrevSweepDeg, float NewSweepDeg)
{
	return Wrap360(NewSweepDeg) < Wrap360(PrevSweepDeg);
}

bool UDeepSonarRules::PingDueAt(double PrevClockS, double NewClockS, float PeriodS)
{
	const double P = FMath::Max(0.01, static_cast<double>(PeriodS));
	return NewClockS > PrevClockS && FMath::FloorToDouble(NewClockS / P) > FMath::FloorToDouble(PrevClockS / P);
}

float UDeepSonarRules::PingPeriodFor(int32 Style, const FDeepSonarTuning& Tuning)
{
	return Style == 0 ? Tuning.SweepPeriodS : FMath::Max(Tuning.SweepPeriodS, Tuning.PingPeriodS);
}

bool UDeepSonarRules::SweptOver(float BearingDeg, float PrevSweepDeg, float NewSweepDeg)
{
	const float Prev = Wrap360(PrevSweepDeg);
	const float Arc = Wrap360(NewSweepDeg - Prev);
	const float At = Wrap360(BearingDeg - Prev);
	return At > 0.f && At <= Arc;
}

float UDeepSonarRules::ReturnStrength(float AgeS, float RangeM, const FDeepSonarTuning& Tuning)
{
	if (AgeS < 0.f || Tuning.PersistS <= 0.f || AgeS >= Tuning.PersistS)
	{
		return 0.f;
	}
	// Phosphor: bright on the sweep, then a quadratic fade so a stale wall reads as stale.
	const float Fade = FMath::Square(1.f - AgeS / Tuning.PersistS);
	const float Far = Tuning.RangeM > 0.f ? FMath::Clamp(RangeM / Tuning.RangeM, 0.f, 1.f) : 0.f;
	return Fade * (1.f - FMath::Clamp(Tuning.RangeFalloff, 0.f, 1.f) * Far);
}

float UDeepSonarRules::LowerSliceBelowM(float BedBelowM, const FDeepSonarTuning& Tuning)
{
	if (BedBelowM < 0.f)
	{
		return Tuning.SliceBelowM;
	}
	return FMath::Clamp(BedBelowM - Tuning.BedClearanceM, 0.f, Tuning.SliceBelowM);
}

float UDeepSonarRules::ProximityLevel(float ClearanceM, const FDeepSonarTuning& Tuning)
{
	if (ClearanceM < 0.f || ClearanceM >= Tuning.ToneStartM)
	{
		return 0.f;
	}
	const float Span = FMath::Max(0.01f, Tuning.ToneStartM - Tuning.ToneFullM);
	const float T = FMath::Clamp((Tuning.ToneStartM - ClearanceM) / Span, 0.f, 1.f);
	// Any wall inside the start distance is audible at once (a floor of 0.05), rising smoothly to full.
	return FMath::Max(0.05f, T * T * (3.f - 2.f * T));
}

float UDeepSonarRules::ToneIntervalS(float Level, const FDeepSonarTuning& Tuning)
{
	return Level <= 0.f ? 0.f : FMath::Lerp(Tuning.ToneSlowS, Tuning.ToneFastS, FMath::Clamp(Level, 0.f, 1.f));
}

float UDeepSonarRules::TonePitch(float Level, const FDeepSonarTuning& Tuning)
{
	return FMath::Lerp(Tuning.TonePitchLow, Tuning.TonePitchHigh, FMath::Clamp(Level, 0.f, 1.f));
}

float UDeepSonarRules::ToneVolume(float Level, const FDeepSonarTuning& Tuning)
{
	return Level <= 0.f ? 0.f : FMath::Lerp(Tuning.ToneVolumeLow, Tuning.ToneVolumeHigh, FMath::Clamp(Level, 0.f, 1.f));
}

bool UDeepSonarRules::SerpentCallDue(int32 CalledStop, int32 Stop)
{
	return CalledStop != Stop;
}
