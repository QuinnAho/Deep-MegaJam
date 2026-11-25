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
		const double BelowDeg = FMath::RadiansToDegrees(FMath::Atan(FMath::Clamp(T.HullTopFrameFrac, 0.f, 0.95f) * TanHalfV));
		const float Want = FMath::Max(LookPitchDeg, float(TopDeg + BelowDeg));
		return FMath::Clamp(Want, FMath::Min(T.LookMinPitchDeg, T.LookMaxPitchDeg), FMath::Max(T.LookMinPitchDeg, T.LookMaxPitchDeg));
	}
}
