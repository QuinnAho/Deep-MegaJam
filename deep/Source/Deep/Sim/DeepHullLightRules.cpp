#include "Sim/DeepHullLightRules.h"

namespace DeepHullLights
{
	namespace
	{
		/** Probes start this far outside the box (cm). */
		constexpr double ProbeOutCm = 60.0;
	}

	TArray<FProbe> TowerTopProbes(const FBox& Solid, int32 Count)
	{
		TArray<FProbe> Probes;
		const int32 N = FMath::Max(2, Count);
		const FVector Centre = Solid.GetCenter();
		for (int32 Index = 0; Index < N; ++Index)
		{
			// Inside the ends a little: the very tips are rounded and never the top.
			const double Along = FMath::Lerp(0.1, 0.9, double(Index) / double(N - 1));
			const double X = FMath::Lerp(Solid.Min.X, Solid.Max.X, Along);
			FProbe Probe;
			Probe.FromCm = FVector(X, Centre.Y, Solid.Max.Z + ProbeOutCm);
			Probe.ToCm = FVector(X, Centre.Y, Solid.Min.Z);
			Probes.Add(Probe);
		}
		return Probes;
	}

	FProbe TowerBackProbe(const FBox& Solid, const FVector& TowerTopCm, const FDeepHullLightTuning& T)
	{
		const double Z = FMath::Clamp(TowerTopCm.Z - FMath::Max(0.f, T.TowerLampBelowTopCm), Solid.Min.Z, Solid.Max.Z);
		FProbe Probe;
		Probe.FromCm = FVector(Solid.Min.X - ProbeOutCm, TowerTopCm.Y, Z);
		Probe.ToCm = FVector(TowerTopCm.X, TowerTopCm.Y, Z);
		return Probe;
	}

	FProbe DeckProbe(const FBox& Solid, const FDeepHullLightTuning& T)
	{
		const double X = FMath::Lerp(Solid.Min.X, Solid.Max.X, double(FMath::Clamp(T.TowerLampAimAlongFrac, 0.f, 1.f)));
		const FVector Centre = Solid.GetCenter();
		FProbe Probe;
		Probe.FromCm = FVector(X, Centre.Y, Solid.Max.Z + ProbeOutCm);
		Probe.ToCm = FVector(X, Centre.Y, Solid.Min.Z);
		return Probe;
	}

	FProbe SternProbe(const FBox& Solid, int32 Side, const FDeepHullLightTuning& T)
	{
		const FVector Centre = Solid.GetCenter();
		const double X = FMath::Lerp(Solid.Min.X, Solid.Max.X, double(FMath::Clamp(T.SternLampAlongFrac, 0.f, 1.f)));
		const double Y = Centre.Y + (Side < 0 ? -1.0 : 1.0) * Solid.GetExtent().Y * FMath::Clamp(T.SternLampSideFrac, 0.f, 1.f);
		FProbe Probe;
		Probe.FromCm = FVector(X, Y, Solid.Max.Z + ProbeOutCm);
		Probe.ToCm = FVector(X, Y, Solid.Min.Z);
		return Probe;
	}

	FTransform Mount(const FVector& SurfaceCm, const FVector& NormalCm, const FVector& AimCm, const FDeepHullLightTuning& T)
	{
		const FVector Normal = NormalCm.GetSafeNormal();
		const FVector At = SurfaceCm + Normal * FMath::Max(0.f, T.MountOutCm);
		FVector Aim = AimCm - At;
		if (Aim.IsNearlyZero())
		{
			Aim = Normal.IsNearlyZero() ? FVector::ForwardVector : Normal;
		}
		return FTransform(Aim.Rotation(), At);
	}

	bool InCone(const FTransform& Lamp, const FVector& Cm, float OuterConeDeg, float RangeCm)
	{
		const FVector To = Cm - Lamp.GetLocation();
		const double Dist = To.Size();
		if (Dist > RangeCm)
		{
			return false;
		}
		if (Dist < UE_KINDA_SMALL_NUMBER)
		{
			return true;
		}
		const double Cos = FVector::DotProduct(To / Dist, Lamp.GetRotation().GetForwardVector());
		return Cos >= FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(OuterConeDeg, 0.f, 89.f)));
	}
}
