#include "Sim/DeepCabinRules.h"

FDeepCabinTuning::FDeepCabinTuning()
{
	// The cabin's own light, by its fittings (found in frames from the chair, Deep.Sub.Slot Interior_lights): a cool strip
	// over the pilot on the starboard side of the ceiling, a dimmer one aft over the bunk, and the amber glow of the
	// controls and screens in front of the chair. Only the strip over the pilot casts shadows.
	auto Add = [this](const FVector& AtCm, const FColor& Color, float Intensity, float RadiusCm, float SourceRadiusCm, bool bShadows)
	{
		FDeepCabinLight Light;
		Light.AtCm = AtCm;
		Light.Color = FLinearColor(Color);
		Light.Intensity = Intensity;
		Light.RadiusCm = RadiusCm;
		Light.SourceRadiusCm = SourceRadiusCm;
		Light.bShadows = bShadows;
		Lights.Add(Light);
	};
	Add(FVector(385.0, 35.0, 205.0), FColor(0xBF, 0xE3, 0xEA), 220.f, 320.f, 10.f, true);
	Add(FVector(220.0, 0.0, 210.0), FColor(0xBF, 0xE3, 0xEA), 90.f, 300.f, 8.f, false);
	Add(FVector(425.0, 0.0, 112.0), FColor(0xFF, 0xB4, 0x5A), 30.f, 140.f, 6.f, false);
}

namespace DeepCabin
{
	FDeepPortPane FitPane(const TArray<FVector>& PointsCm, const TArray<FVector>& Normals, const FVector& Outward)
	{
		FDeepPortPane Pane;
		if (PointsCm.Num() < 8 || Normals.Num() != PointsCm.Num())
		{
			return Pane;
		}
		FVector Sum = FVector::ZeroVector;
		FVector NormalSum = FVector::ZeroVector;
		for (int32 Index = 0; Index < PointsCm.Num(); ++Index)
		{
			Sum += PointsCm[Index];
			NormalSum += FVector::DotProduct(Normals[Index], Outward) < 0.0 ? -Normals[Index] : Normals[Index];
		}
		Pane.CentreCm = Sum / PointsCm.Num();
		Pane.Normal = NormalSum.GetSafeNormal();
		if (Pane.Normal.IsNearlyZero())
		{
			return Pane;
		}
		double Far = 0.0;
		for (const FVector& P : PointsCm)
		{
			const FVector Off = P - Pane.CentreCm;
			Far = FMath::Max(Far, (Off - Pane.Normal * FVector::DotProduct(Off, Pane.Normal)).Size());
		}
		Pane.RadiusCm = float(Far);
		Pane.bValid = true;
		return Pane;
	}

	TArray<FDeepPortPane> TunedPanes(const FDeepCabinTuning& T)
	{
		FDeepPortPane Starboard;
		Starboard.CentreCm = T.PortCentreCm;
		Starboard.Normal = T.PortNormal.GetSafeNormal();
		Starboard.RadiusCm = T.PortRadiusCm;
		Starboard.bValid = !Starboard.Normal.IsNearlyZero() && T.PortRadiusCm > 0.f;
		FDeepPortPane Port = Starboard;
		Port.CentreCm.Y = -Port.CentreCm.Y;
		Port.Normal.Y = -Port.Normal.Y;
		return { Port, Starboard };
	}

	FBox CutBox(const TArray<FDeepPortPane>& Panes, const FDeepCabinTuning& T)
	{
		FBox Box(ForceInit);
		for (const FDeepPortPane& Pane : Panes)
		{
			if (!Pane.bValid)
			{
				continue;
			}
			// A disc's extent along each axis: the radius times how far the axis leans off the normal, plus half its thickness.
			FVector Extent;
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				const double N = Pane.Normal[Axis];
				Extent[Axis] = Pane.RadiusCm * FMath::Sqrt(FMath::Max(0.0, 1.0 - N * N)) + 0.5 * T.PortThicknessCm * FMath::Abs(N);
			}
			Box += FBox(Pane.CentreCm - Extent, Pane.CentreCm + Extent);
		}
		return Box.IsValid ? Box.ExpandBy(FMath::Max(0.f, T.PortCutMarginCm)) : Box;
	}

	FTransform PaneTransform(const FDeepPortPane& Pane, const FDeepCabinTuning& T)
	{
		const FQuat Rotation = FRotationMatrix::MakeFromZ(Pane.Normal.GetSafeNormal()).ToQuat();
		const double Across = 2.0 * Pane.RadiusCm / 100.0;
		return FTransform(Rotation, Pane.CentreCm, FVector(Across, Across, FMath::Max(0.1f, T.PortThicknessCm) / 100.0));
	}

	FBox BoxInto(const FBox& Box, const FTransform& Into)
	{
		FBox Out(ForceInit);
		if (!Box.IsValid)
		{
			return Out;
		}
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector P((Corner & 1) ? Box.Max.X : Box.Min.X, (Corner & 2) ? Box.Max.Y : Box.Min.Y, (Corner & 4) ? Box.Max.Z : Box.Min.Z);
			Out += Into.TransformPosition(P);
		}
		return Out;
	}
}
