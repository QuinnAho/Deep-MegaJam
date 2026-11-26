#include "Sim/DeepJudderRules.h"

namespace
{
	FDeepJudderStat Summarise(TArray<double>& Values)
	{
		FDeepJudderStat Out;
		if (Values.IsEmpty())
		{
			return Out;
		}
		double Sum = 0.0;
		for (const double V : Values)
		{
			Sum += V * V;
			Out.Max = FMath::Max(Out.Max, V);
		}
		Out.Rms = FMath::Sqrt(Sum / Values.Num());
		Values.Sort();
		Out.P95 = Values[FMath::Clamp(FMath::FloorToInt(0.95 * (Values.Num() - 1) + 0.5), 0, Values.Num() - 1)];
		return Out;
	}

	FString StatJson(const TCHAR* Name, const FDeepJudderStat& S)
	{
		return FString::Printf(TEXT("\"%s\":{\"rms\":%.3f,\"p95\":%.3f,\"max\":%.3f}"), Name, S.Rms, S.P95, S.Max);
	}
}

namespace DeepJudder
{
	double Residual(const FVector& A, double T0, const FVector& P, double T, const FVector& B, double T1)
	{
		const double Span = T1 - T0;
		const double Alpha = Span > UE_SMALL_NUMBER ? FMath::Clamp((T - T0) / Span, 0.0, 1.0) : 0.5;
		return FVector::Dist(P, FMath::Lerp(A, B, Alpha));
	}

	double ResidualDeg(const FQuat& A, double T0, const FQuat& Q, double T, const FQuat& B, double T1)
	{
		const double Span = T1 - T0;
		const double Alpha = Span > UE_SMALL_NUMBER ? FMath::Clamp((T - T0) / Span, 0.0, 1.0) : 0.5;
		const FQuat Mid = FQuat::Slerp(A, B, Alpha);
		return FMath::RadiansToDegrees(Mid.AngularDistance(Q));
	}

	FDeepJudderStats Measure(const TArray<FDeepJudderSample>& Samples)
	{
		FDeepJudderStats Out;
		Out.Frames = Samples.Num();
		if (Samples.Num() < 3)
		{
			return Out;
		}
		TArray<double> Cam, CamDeg, Hull, HullDeg, InView, Dts;
		double Travel = 0.0;
		for (int32 I = 1; I < Samples.Num(); ++I)
		{
			Dts.Add(1000.0 * (Samples[I].TimeS - Samples[I - 1].TimeS));
			Travel += FVector::Dist(Samples[I].HullCm, Samples[I - 1].HullCm);
		}
		for (int32 I = 1; I + 1 < Samples.Num(); ++I)
		{
			const FDeepJudderSample& A = Samples[I - 1];
			const FDeepJudderSample& P = Samples[I];
			const FDeepJudderSample& B = Samples[I + 1];
			Cam.Add(Residual(A.CamCm, A.TimeS, P.CamCm, P.TimeS, B.CamCm, B.TimeS));
			CamDeg.Add(ResidualDeg(A.CamRot, A.TimeS, P.CamRot, P.TimeS, B.CamRot, B.TimeS));
			Hull.Add(Residual(A.HullCm, A.TimeS, P.HullCm, P.TimeS, B.HullCm, B.TimeS));
			HullDeg.Add(ResidualDeg(A.HullRot, A.TimeS, P.HullRot, P.TimeS, B.HullRot, B.TimeS));
			const auto InCam = [](const FDeepJudderSample& S) { return S.CamRot.UnrotateVector(S.HullCm - S.CamCm); };
			InView.Add(Residual(InCam(A), A.TimeS, InCam(P), P.TimeS, InCam(B), B.TimeS));
			const double Before = FVector::Dist(P.HullCm, A.HullCm);
			const double After = FVector::Dist(B.HullCm, P.HullCm);
			// Stalled: the hull stood still for a frame between two frames in which it moved.
			if (Before > 0.5 && After < 0.01 && I + 2 < Samples.Num() && FVector::Dist(Samples[I + 2].HullCm, B.HullCm) > 0.5)
			{
				++Out.StallFrames;
			}
		}
		double Sum = 0.0;
		for (const double D : Dts)
		{
			Sum += D;
		}
		Out.MeanDtMs = Sum / Dts.Num();
		double Var = 0.0;
		for (const double D : Dts)
		{
			Var += FMath::Square(D - Out.MeanDtMs);
		}
		Out.DtSpreadMs = FMath::Sqrt(Var / Dts.Num());
		const double Span = Samples.Last().TimeS - Samples[0].TimeS;
		Out.HullMeanMps = Span > UE_SMALL_NUMBER ? Travel / 100.0 / Span : 0.0;
		Out.CamCm = Summarise(Cam);
		Out.CamDeg = Summarise(CamDeg);
		Out.HullCm = Summarise(Hull);
		Out.HullDeg = Summarise(HullDeg);
		Out.HullInViewCm = Summarise(InView);
		return Out;
	}

	FString ToJson(const FDeepJudderStats& S, const FString& Label)
	{
		return FString::Printf(TEXT("{\"label\":\"%s\",\"frames\":%d,\"dt_ms\":%.2f,\"dt_spread_ms\":%.2f,\"hull_mps\":%.2f,\"stalls\":%d,%s,%s,%s,%s,%s}"),
			*Label, S.Frames, S.MeanDtMs, S.DtSpreadMs, S.HullMeanMps, S.StallFrames,
			*StatJson(TEXT("cam_cm"), S.CamCm), *StatJson(TEXT("cam_deg"), S.CamDeg), *StatJson(TEXT("hull_cm"), S.HullCm),
			*StatJson(TEXT("hull_deg"), S.HullDeg), *StatJson(TEXT("hull_in_view_cm"), S.HullInViewCm));
	}
}
