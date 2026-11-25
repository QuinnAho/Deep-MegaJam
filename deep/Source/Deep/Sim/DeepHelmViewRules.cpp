#include "Sim/DeepHelmViewRules.h"

#include "Sim/DeepRules.h"

namespace DeepHelmView
{
	bool PointBack(const TArray<FVector>& Trail, const FVector& FromCm, double BackCm, FVector& OutCm)
	{
		OutCm = FromCm;
		double Left = FMath::Max(0.0, BackCm);
		FVector At = FromCm;
		for (int32 Index = Trail.Num() - 1; Index >= 0; --Index)
		{
			const FVector& Next = Trail[Index];
			const double Leg = FVector::Dist(At, Next);
			if (Leg >= Left && Leg > UE_KINDA_SMALL_NUMBER)
			{
				OutCm = At + (Next - At) * (Left / Leg);
				return true;
			}
			Left -= Leg;
			At = Next;
		}
		OutCm = At;
		return Left <= UE_KINDA_SMALL_NUMBER;
	}

	void Record(TArray<FVector>& Trail, const FVector& HullCm, const FDeepHelmViewTuning& T)
	{
		const double Step = FMath::Max(0.05f, T.TrailStepM) * UDeepRules::CmPerM;
		if (Trail.IsEmpty() || FVector::Dist(Trail.Last(), HullCm) >= Step)
		{
			Trail.Add(HullCm);
		}
		const int32 Max = FMath::Max(2, T.TrailMaxPoints);
		if (Trail.Num() > Max)
		{
			Trail.RemoveAt(0, Trail.Num() - Max);
		}
	}

	FVector ChaseAim(const TArray<FVector>& Trail, const FVector& HullCm, const FVector& Ahead, float UpM, const FDeepHelmViewTuning& T)
	{
		const double Back = FMath::Max(0.f, T.ChaseBackM) * UDeepRules::CmPerM;
		const FVector Up(0.0, 0.0, FMath::Max(0.f, UpM) * UDeepRules::CmPerM);
		const FVector Flat = FVector(Ahead.X, Ahead.Y, 0.0).GetSafeNormal();
		FVector OnTrail;
		if (PointBack(Trail, HullCm, Back, OnTrail))
		{
			// The trail is water the hull has just swept clear; use it while it lies astern (the hull drives forward).
			const FVector Astern = FVector(HullCm.X - OnTrail.X, HullCm.Y - OnTrail.Y, 0.0);
			const double Flatness = Astern.Size() / FMath::Max(1.0, Back);
			const double CosOff = Flat.IsNearlyZero() ? 1.0 : FVector::DotProduct(Astern.GetSafeNormal(), Flat);
			if (Flatness >= 0.5 && CosOff >= FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(T.TrailMaxOffHeadingDeg, 0.f, 180.f))))
			{
				return OnTrail + Up;
			}
		}
		return HullCm - Flat * Back + Up;
	}

	FVector Travel(const FVector& VelocityMps, const FVector& Ahead, const FDeepHelmViewTuning& T)
	{
		if (VelocityMps.Size() > FMath::Max(0.01f, T.TravelMinMps))
		{
			return VelocityMps.GetSafeNormal();
		}
		const FVector Flat = FVector(Ahead.X, Ahead.Y, 0.0).GetSafeNormal();
		return Flat.IsNearlyZero() ? FVector::ForwardVector : Flat;
	}

	FRotator ChaseLook(const FVector& CameraCm, const FVector& HullCm, const FVector& TravelDir, const FDeepHelmViewTuning& T)
	{
		const FVector Target = HullCm + TravelDir.GetSafeNormal() * (FMath::Max(0.f, T.LookAheadM) * UDeepRules::CmPerM) + FVector(0.0, 0.0, T.LookUpM * UDeepRules::CmPerM);
		FRotator Look = (Target - CameraCm).Rotation();
		Look.Pitch = FMath::Clamp(Look.Pitch, FMath::Min(T.LookMinPitchDeg, T.LookMaxPitchDeg), FMath::Max(T.LookMinPitchDeg, T.LookMaxPitchDeg));
		Look.Roll = 0.f;
		return Look;
	}

	FVector2D AddLook(const FVector2D& LookDeg, float YawDeg, float PitchDeg, bool bCockpit, const FDeepHelmViewTuning& T)
	{
		const float YawMax = FMath::Max(0.f, bCockpit ? T.CockpitLookYawDeg : T.ChaseLookYawDeg);
		const float PitchMax = FMath::Max(0.f, bCockpit ? T.CockpitLookPitchDeg : T.ChaseLookPitchDeg);
		return FVector2D(FMath::Clamp(LookDeg.X + YawDeg, -YawMax, YawMax), FMath::Clamp(LookDeg.Y + PitchDeg, -PitchMax, PitchMax));
	}

	FVector2D EaseLook(const FVector2D& LookDeg, float IdleS, float DeltaSeconds, const FDeepHelmViewTuning& T)
	{
		if (IdleS < T.LookReturnDelayS)
		{
			return LookDeg;
		}
		const FVector2D Eased = LookDeg * FMath::Exp(-FMath::Max(0.f, T.LookReturnPerS) * FMath::Max(0.f, DeltaSeconds));
		return Eased.Size() < 0.05f ? FVector2D::ZeroVector : Eased;
	}

	float VerticalFovDeg(float HorizontalFovDeg, float Aspect)
	{
		const double HalfH = FMath::DegreesToRadians(FMath::Clamp(HorizontalFovDeg, 1.f, 170.f) * 0.5);
		return float(FMath::RadiansToDegrees(2.0 * FMath::Atan(FMath::Tan(HalfH) / FMath::Max(0.1f, Aspect))));
	}

	float FrameY(const FVector& CameraCm, const FRotator& View, const FVector& PointCm, float HorizontalFovDeg, float Aspect)
	{
		const FVector Local = View.UnrotateVector(PointCm - CameraCm);
		if (Local.X <= UE_KINDA_SMALL_NUMBER)
		{
			return 10.f;
		}
		const double TanHalfV = FMath::Tan(FMath::DegreesToRadians(VerticalFovDeg(HorizontalFovDeg, Aspect) * 0.5));
		return float(-(Local.Z / Local.X) / FMath::Max(1e-4, TanHalfV));
	}

	float FramePitch(const FVector& CameraCm, const FVector& HullTopCm, float LookPitchDeg, float HorizontalFovDeg, const FDeepHelmViewTuning& T)
	{
		const FVector To = HullTopCm - CameraCm;
		const double Flat = FVector(To.X, To.Y, 0.0).Size();
		const double TopDeg = FMath::RadiansToDegrees(FMath::Atan2(To.Z, FMath::Max(1.0, Flat)));
		const double TanHalfV = FMath::Tan(FMath::DegreesToRadians(VerticalFovDeg(HorizontalFovDeg, T.FrameAspect) * 0.5));
		// The pitch that puts the hull's top this share of the half frame under the centre (negative: over it).
		const auto PitchFor = [&](float Frac) { return float(TopDeg + FMath::RadiansToDegrees(FMath::Atan(FMath::Clamp(Frac, -0.95f, 0.95f) * TanHalfV))); };
		const float Want = FMath::Min(FMath::Max(LookPitchDeg, PitchFor(T.HullTopFrameFrac)), PitchFor(FMath::Max(T.HullTopFrameFrac, T.HullTopMaxFrameFrac)));
		return FMath::Clamp(Want, FMath::Min(T.LookMinPitchDeg, T.LookMaxPitchDeg), FMath::Max(T.LookMinPitchDeg, T.LookMaxPitchDeg));
	}

	float HeaveShare(float TravelPitchDeg, const FDeepHelmViewTuning& T)
	{
		return FMath::Clamp(TravelPitchDeg / FMath::Max(1.f, T.HeaveFullDeg), -1.f, 1.f);
	}

	FDeepHelmViewTuning Heaved(const FDeepHelmViewTuning& T, float Heave)
	{
		FDeepHelmViewTuning Out = T;
		const float Sink = FMath::Clamp(-Heave, 0.f, 1.f);
		const float Rise = FMath::Clamp(Heave, 0.f, 1.f);
		if (Sink > 0.f)
		{
			Out.ChaseBackM = FMath::Lerp(T.ChaseBackM, T.SinkBackM, Sink);
			Out.ChaseUpM = FMath::Lerp(T.ChaseUpM, T.SinkUpM, Sink);
			Out.LookMinPitchDeg = FMath::Lerp(T.LookMinPitchDeg, FMath::Min(T.LookMinPitchDeg, T.SinkMinPitchDeg), Sink);
			Out.HullTopFrameFrac = FMath::Lerp(T.HullTopFrameFrac, T.SinkHullTopFrac, Sink);
		}
		else if (Rise > 0.f)
		{
			Out.ChaseBackM = FMath::Lerp(T.ChaseBackM, T.RiseBackM, Rise);
			Out.ChaseUpM = FMath::Lerp(T.ChaseUpM, T.RiseUpM, Rise);
			Out.ChaseUpMinM = FMath::Min(T.ChaseUpMinM, Out.ChaseUpM);
			Out.LookMaxPitchDeg = FMath::Lerp(T.LookMaxPitchDeg, FMath::Max(T.LookMaxPitchDeg, T.RiseMaxPitchDeg), Rise);
		}
		return Out;
	}
}
