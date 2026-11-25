#include "Sim/DeepImmersionRules.h"

namespace
{
	constexpr float ImmCmPerM = 100.f;

	float ImmSmooth01(float U)
	{
		U = FMath::Clamp(U, 0.f, 1.f);
		return U * U * (3.f - 2.f * U);
	}

	/** Metres a puff travels in T seconds from V0 m/s slowed by Drag per second. */
	float ImmDraggedM(float V0, float TimeS, float Drag)
	{
		return Drag > 1e-4f ? V0 * (1.f - FMath::Exp(-Drag * TimeS)) / Drag : V0 * TimeS;
	}

	float ImmRangePick(const FVector2D& Range, float Rand01)
	{
		return FMath::Lerp(FMath::Min(Range.X, Range.Y), FMath::Max(Range.X, Range.Y), FMath::Clamp(Rand01, 0.f, 1.f));
	}

	float ImmQuantile(const TArray<float>& Sorted, float Q)
	{
		if (Sorted.IsEmpty())
		{
			return 0.f;
		}
		const float At = FMath::Clamp(Q, 0.f, 1.f) * (Sorted.Num() - 1);
		const int32 Low = FMath::FloorToInt(At);
		const int32 High = FMath::Min(Sorted.Num() - 1, Low + 1);
		return FMath::Lerp(Sorted[Low], Sorted[High], At - Low);
	}
}

int32 FDeepSiltPool::Add(const FDeepSiltPuff& Puff, int32 MaxPuffs)
{
	const int32 Max = FMath::Max(1, MaxPuffs);
	while (Puffs.Num() > Max)
	{
		Puffs.RemoveAtSwap(0);
	}
	if (Puffs.Num() < Max)
	{
		return Puffs.Add(Puff);
	}
	// Full: the oldest share of its life gives way (it was nearly gone anyway).
	int32 Oldest = 0;
	float Most = -1.f;
	for (int32 I = 0; I < Puffs.Num(); ++I)
	{
		const float Share = Puffs[I].AgeS / FMath::Max(0.01f, Puffs[I].LifeS);
		if (Share > Most)
		{
			Most = Share;
			Oldest = I;
		}
	}
	Puffs[Oldest] = Puff;
	return Oldest;
}

void FDeepSiltPool::Step(float DeltaSeconds)
{
	const float Dt = FMath::Max(0.f, DeltaSeconds);
	for (int32 I = Puffs.Num() - 1; I >= 0; --I)
	{
		Puffs[I].AgeS += Dt;
		if (Puffs[I].AgeS >= Puffs[I].LifeS)
		{
			Puffs.RemoveAtSwap(I);
		}
	}
}

namespace DeepImmersionRules
{
	float Depth01(float DepthM, const FDeepHullVoiceTuning& T)
	{
		const float Span = FMath::Max(1.f, T.DeepM - T.ShallowM);
		return FMath::Clamp((DepthM - T.ShallowM) / Span, 0.f, 1.f);
	}

	float CreakRatePerS(const FDeepCreakInputs& In, const FDeepHullVoiceTuning& T)
	{
		const float D = Depth01(In.DepthM, T);
		const float Interval = FMath::Lerp(FMath::Max(1.f, T.ShallowIntervalS), FMath::Max(1.f, T.DeepIntervalS), D);
		float Base = 1.f / Interval;
		if (In.bParked && In.DepthM <= T.SurfaceStopM)
		{
			Base *= FMath::Clamp(T.SurfaceRestRate, 0.f, 1.f);
		}
		const float Turn = T.TurnCreaksPerS * FMath::Clamp(FMath::Abs(In.YawRateDegS) / FMath::Max(1.f, T.TurnFullDegS), 0.f, 1.f);
		const float Sink = T.DepthChangeCreaksPerS * FMath::Clamp(FMath::Abs(In.DepthRateMps) / FMath::Max(0.1f, T.DepthChangeFullMps), 0.f, 1.f);
		return Base + Turn + Sink;
	}

	FDeepCreak StepCreak(FDeepCreakClock& Clock, const FDeepCreakInputs& In, float DeltaS, float RandA, float RandB, const FDeepHullVoiceTuning& T)
	{
		FDeepCreak Out;
		const float Dt = FMath::Max(0.f, DeltaS);
		Clock.SinceLastS += Dt;
		Clock.QuietS = FMath::Max(0.f, Clock.QuietS - Dt);
		const float Rate = CreakRatePerS(In, T);
		// The charge is capped at the threshold: a wait (the gap, a groan) never banks a burst of creaks.
		Clock.Charge = FMath::Min(Clock.Charge + Rate * Dt, FMath::Max(Clock.Threshold, 0.05f));
		if (Clock.Charge < Clock.Threshold || Clock.SinceLastS < T.MinGapS || Clock.QuietS > 0.f)
		{
			return Out;
		}
		Clock.Charge = 0.f;
		Clock.SinceLastS = 0.f;
		const float Irregular = FMath::Clamp(T.Irregularity, 0.f, 0.9f);
		Clock.Threshold = 1.f + Irregular * (2.f * FMath::Clamp(RandA, 0.f, 1.f) - 1.f);

		const float D = Depth01(In.DepthM, T);
		const float BaseRate = CreakRatePerS(FDeepCreakInputs{ In.DepthM, 0.f, 0.f, In.bParked }, T);
		const float StressRate = FMath::Max(0.f, Rate - BaseRate);
		Out.bFire = true;
		Out.bStress = StressRate > BaseRate;
		Out.Pitch = FMath::Lerp(T.ShallowPitch, T.DeepPitch, D) * (1.f + T.PitchJitter * (2.f * FMath::Clamp(RandB, 0.f, 1.f) - 1.f));
		const float StressShare = Rate > 0.f ? StressRate / Rate : 0.f;
		Out.Volume = FMath::Clamp(FMath::Lerp(T.ShallowVolume, T.DeepVolume, D) + T.StressVolume * StressShare, 0.f, 1.f);
		return Out;
	}

	int32 GroanBand(float DepthM, const FDeepHullVoiceTuning& T)
	{
		return FMath::FloorToInt(FMath::Max(0.f, DepthM) / FMath::Max(10.f, T.GroanBandM) + 1e-4f);
	}

	bool StepGroan(int32& DeepestBand, float DepthM, const FDeepHullVoiceTuning& T)
	{
		const int32 Band = GroanBand(DepthM, T);
		if (DeepestBand < 0)
		{
			DeepestBand = Band;
			return false;
		}
		if (Band <= DeepestBand)
		{
			return false;
		}
		DeepestBand = Band;
		return true;
	}

	float GroanPitch(float DepthM, const FDeepHullVoiceTuning& T)
	{
		return FMath::Lerp(T.GroanShallowPitch, T.GroanDeepPitch, Depth01(DepthM, T));
	}

	FDeepFlowLevel FlowLevel(float SpeedMps, const FDeepHullVoiceTuning& T)
	{
		FDeepFlowLevel Out;
		const float Span = FMath::Max(0.05f, T.FlowFullMps - T.FlowStartMps);
		const float U = FMath::Clamp((FMath::Abs(SpeedMps) - T.FlowStartMps) / Span, 0.f, 1.f);
		Out.Volume01 = ImmSmooth01(U);
		Out.Pitch = FMath::Lerp(T.FlowSlowPitch, T.FlowFullPitch, U);
		const float Slow = FMath::Max(50.f, T.FlowSlowLowPassHz);
		const float Full = FMath::Max(50.f, T.FlowFullLowPassHz);
		// Even steps of brightness to the ear: the cutoff moves on a log scale.
		Out.LowPassHz = Slow * FMath::Pow(Full / Slow, U);
		return Out;
	}

	float LiftPuffsPerS(float ClearanceM, float SpeedMps, const FDeepSiltTuning& T)
	{
		if (ClearanceM < 0.f || ClearanceM > T.LiftHeightM || SpeedMps <= T.LiftMinMps || T.LiftHeightM <= 0.f)
		{
			return 0.f;
		}
		const float Near = FMath::Pow(1.f - ClearanceM / T.LiftHeightM, FMath::Max(0.5f, T.LiftFalloff));
		const float Fast = FMath::Clamp((SpeedMps - T.LiftMinMps) / FMath::Max(0.05f, T.LiftFullMps - T.LiftMinMps), 0.f, 1.f);
		return FMath::Max(0.f, T.LiftPuffsPerS) * Near * Fast;
	}

	float WashSpeedMps(float MeasuredMps, float Effort01, const FDeepSiltTuning& T)
	{
		return FMath::Max(FMath::Max(0.f, MeasuredMps), FMath::Clamp(Effort01, 0.f, 1.f) * T.LiftFullMps);
	}

	bool IsScrape(float GapM, const FDeepSiltTuning& T)
	{
		return GapM >= -0.5f && GapM <= T.ScrapeGapM;
	}

	int32 KnockPuffs(float ImpactMps, const FDeepSiltTuning& T)
	{
		if (ImpactMps <= 0.f)
		{
			return 0;
		}
		const int32 Extra = FMath::RoundToInt(FMath::Max(0.f, ImpactMps - 1.f) * T.PuffsPerImpactMps);
		return FMath::Clamp(T.KnockPuffs + Extra, 0, FMath::Max(0, T.KnockPuffsMax));
	}

	bool DiverLifts(float ClearanceM, float SpeedMps, const FDeepSiltTuning& T)
	{
		return ClearanceM >= 0.f && ClearanceM <= T.DiverLiftHeightM && SpeedMps >= T.DiverMinMps;
	}

	FDeepSiltPuff MakePuff(const FVector& HitCm, const FVector& Normal, float Scale, FRandomStream& Rand, const FDeepSiltTuning& T)
	{
		FDeepSiltPuff Puff;
		Puff.Normal = Normal.GetSafeNormal(1e-4, FVector::UpVector);
		Puff.Scale = FMath::Clamp(Scale, 0.05f, 4.f);
		Puff.StartRadiusM = ImmRangePick(T.StartRadiusM, Rand.FRand());
		Puff.SpreadRadiusM = FMath::Max(Puff.StartRadiusM, ImmRangePick(T.SpreadRadiusM, Rand.FRand()));
		Puff.RiseMps = ImmRangePick(T.RiseMps, Rand.FRand());
		Puff.LifeS = FMath::Max(0.5f, ImmRangePick(T.LifeS, Rand.FRand()));
		// Outward: a random direction along the surface.
		const FVector Any = FVector(Rand.VRand());
		const FVector Along = (Any - Puff.Normal * FVector::DotProduct(Any, Puff.Normal)).GetSafeNormal();
		Puff.Out = Along;
		Puff.OutMps = T.OutwardMps;
		Puff.Wander = FVector(Rand.VRand());
		// Born on the open side of the surface, the start radius share off it: never in the rock.
		Puff.SurfaceCm = HitCm;
		Puff.BirthCm = HitCm + Puff.Normal * (Puff.StartRadiusM * Puff.Scale * ImmCmPerM * FMath::Max(0.f, T.SpawnLiftShare) + 1.f);
		return Puff;
	}

	FDeepSiltPuffNow PuffAt(const FDeepSiltPuff& Puff, const FDeepSiltTuning& T)
	{
		FDeepSiltPuffNow Now;
		const float Age = FMath::Clamp(Puff.AgeS, 0.f, Puff.LifeS);
		const float Rise = ImmDraggedM(Puff.RiseMps, Age, T.Drag);
		const float Outward = ImmDraggedM(Puff.OutMps, Age, T.Drag);
		const float Settle = T.SettleMps * FMath::Max(0.f, Age - 0.5f * Puff.LifeS);
		const FVector DriftM = (T.CurrentMps + Puff.Wander * T.WanderMps) * Age;
		Now.LocationCm = Puff.BirthCm + Puff.PushCm
			+ (Puff.Normal * Rise + Puff.Out * Outward + DriftM - FVector(0.0, 0.0, Settle)) * ImmCmPerM;
		// The current and the settling never carry it back into the rock it rose from: it stays at least as far off
		// that surface as it was born.
		const double BornAbove = FVector::DotProduct(Puff.BirthCm - Puff.SurfaceCm, Puff.Normal);
		const double Above = FVector::DotProduct(Now.LocationCm - Puff.SurfaceCm, Puff.Normal);
		if (Above < BornAbove)
		{
			Now.LocationCm += Puff.Normal * (BornAbove - Above);
		}
		const float Spread01 = 1.f - FMath::Exp(-3.f * Age / FMath::Max(0.1f, T.SpreadS));
		Now.RadiusCm = FMath::Lerp(Puff.StartRadiusM, Puff.SpreadRadiusM, Spread01) * Puff.Scale * ImmCmPerM;
		if (Puff.FitRadiusCm >= 0.f)
		{
			Now.RadiusCm = FMath::Min(Now.RadiusCm, Puff.FitRadiusCm);
		}
		const float In = ImmSmooth01(Age / FMath::Max(0.05f, T.FadeInS));
		const float Out = ImmSmooth01((Puff.LifeS - Age) / FMath::Max(0.05f, Puff.LifeS * T.FadeOutShare));
		Now.Opacity01 = FMath::Clamp(T.PeakOpacity * In * Out * (1.f - T.SpreadThinning * Spread01), 0.f, 1.f);
		return Now;
	}

	void Wash(FDeepSiltPuff& Puff, const FVector& HullVelocityMps, const FDeepSiltTuning& T)
	{
		// Aft of the travel, flattened onto the surface it rose from.
		const FVector Back = -HullVelocityMps;
		const FVector Along = Back - Puff.Normal * FVector::DotProduct(Back, Puff.Normal);
		const double Speed = Along.Size();
		if (Speed < 1e-3)
		{
			return;
		}
		const float Share = FMath::Clamp(T.WashShare, 0.f, 1.f);
		const FVector Out = (Along / Speed) * Share + Puff.Out * (1.f - Share);
		Puff.Out = Out.GetSafeNormal(1e-4, Along / Speed);
		Puff.OutMps = T.OutwardMps + T.WashMps * FMath::Clamp(static_cast<float>(Speed) / FMath::Max(0.1f, T.LiftFullMps), 0.f, 1.f);
	}

	bool TouchesBox(const FVector& PointCm, float RadiusCm, const FBox& Box)
	{
		return Box.IsValid && Box.ComputeSquaredDistanceToPoint(PointCm) < FMath::Square(FMath::Max(0.f, RadiusCm));
	}

	FVector KeepOffBox(const FVector& PointCm, float RadiusCm, float CoreShare, const FBox& Box, const FVector& SurfaceCm, const FVector& SurfaceNormal, float MinAboveCm)
	{
		const double Core = FMath::Max(0.f, RadiusCm) * FMath::Max(0.f, CoreShare);
		if (!Box.IsValid || !TouchesBox(PointCm, static_cast<float>(Core), Box))
		{
			return PointCm;
		}
		// Out through the nearest face, to Core off it; never back into the rock it rose from.
		const FVector Normal = SurfaceNormal.GetSafeNormal(1e-4, FVector::UpVector);
		const FVector Min = Box.Min - FVector(Core);
		const FVector Max = Box.Max + FVector(Core);
		FVector Best = PointCm;
		double BestMove = TNumericLimits<double>::Max();
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			for (int32 Side = 0; Side < 2; ++Side)
			{
				FVector Candidate = PointCm;
				Candidate[Axis] = Side == 0 ? Min[Axis] : Max[Axis];
				const double Move = FMath::Abs(Candidate[Axis] - PointCm[Axis]);
				if (Move < BestMove && FVector::DotProduct(Candidate - SurfaceCm, Normal) >= MinAboveCm - 0.01)
				{
					BestMove = Move;
					Best = Candidate;
				}
			}
		}
		return Best;
	}

	float FitRadiusCm(const FVector& PointCm, float CoreShare, const FBox& Box)
	{
		if (!Box.IsValid)
		{
			return TNumericLimits<float>::Max();
		}
		return static_cast<float>(FMath::Sqrt(Box.ComputeSquaredDistanceToPoint(PointCm)) / FMath::Max(0.01f, CoreShare));
	}

	FDeepEchoTap EchoTap(float DistanceM, float GainScale, const FDeepEchoTuning& T)
	{
		FDeepEchoTap Tap;
		const float D = FMath::Max(0.1f, DistanceM);
		Tap.DistanceM = D;
		Tap.DelayS = FMath::Max(0.f, T.MinDelayS) + D * FMath::Max(0.f, T.DelayPerM);
		const float Ref = FMath::Max(0.5f, T.RefM);
		Tap.Gain = FMath::Clamp(T.NearGain * FMath::Min(1.f, FMath::Pow(Ref / D, FMath::Max(0.f, T.Falloff))) * GainScale, 0.f, 1.f);
		const float Far01 = FMath::Clamp((D - Ref) / FMath::Max(1.f, T.FarM - Ref), 0.f, 1.f);
		const float Near = FMath::Max(100.f, T.NearLowPassHz);
		const float Far = FMath::Max(100.f, T.FarLowPassHz);
		Tap.LowPassHz = Near * FMath::Pow(Far / Near, Far01);
		return Tap;
	}

	TArray<FDeepEchoTap> EchoTaps(const TArray<float>& RangesM, const FDeepEchoTuning& T)
	{
		TArray<FDeepEchoTap> Out;
		TArray<float> Returns;
		for (const float R : RangesM)
		{
			if (R >= 0.f)
			{
				Returns.Add(R);
			}
		}
		if (Returns.IsEmpty() || RangesM.IsEmpty())
		{
			return Out;
		}
		Returns.Sort();
		const float Share = float(Returns.Num()) / float(RangesM.Num());
		const float ShareGain = FMath::Lerp(FMath::Clamp(T.MinReturnShare, 0.f, 1.f), 1.f, Share);
		const float NearM = ImmQuantile(Returns, T.NearQuantile);
		const float FarM = ImmQuantile(Returns, FMath::Max(T.NearQuantile, T.FarQuantile));
		Out.Add(EchoTap(NearM, ShareGain, T));
		// A far side clearly beyond the near walls answers a second time; in a tight tunnel the two are one slap.
		if (FarM - NearM >= 2.f)
		{
			Out.Add(EchoTap(FarM, ShareGain * FMath::Clamp(T.FarShare, 0.f, 1.f), T));
		}
		return Out;
	}
}
