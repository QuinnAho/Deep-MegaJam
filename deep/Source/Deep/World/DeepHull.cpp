#include "World/DeepHull.h"

#include "World/DeepShapes.h"

namespace DeepHull
{
	TArray<FDeepHullPart> Parts(const FVector& OuterCm, double HatchHalfCm, double AnchorZCm)
	{
		const double OL = OuterCm.X;  // 470: front and back faces
		const double OW = OuterCm.Y;  // 245: side faces
		const double OH = OuterCm.Z;  // 170: ceiling top, belly bottom at -OH
		const double Hh = HatchHalfCm;
		TArray<FDeepHullPart> Out;
		Out.Reserve(128);

		auto Add = [&Out](const FString& Name, const TCHAR* Mesh, const FVector& Centre, const FVector& Size, const FRotator& Rot, EDeepHullPaint Paint, bool bShadow)
		{
			FDeepHullPart& P = Out.AddDefaulted_GetRef();
			P.Name = FName(*(TEXT("Hull_") + Name));
			P.Mesh = Mesh;
			P.Centre = Centre;
			P.Size = Size;
			P.Rotation = Rot;
			P.Paint = Paint;
			P.bShadow = bShadow;
			return Out.Num() - 1;
		};
		auto Box = [&Add](const FString& Name, const FVector& Centre, const FVector& Size, EDeepHullPaint Paint, bool bShadow = true)
		{
			return Add(Name, DeepShapes::Cube, Centre, Size, FRotator::ZeroRotator, Paint, bShadow);
		};
		// A cylinder (or cone, apex at B) from A to B.
		auto Rod = [&Add](const FString& Name, const FVector& A, const FVector& B, double D, EDeepHullPaint Paint, bool bShadow = true, const TCHAR* Mesh = DeepShapes::Cylinder)
		{
			const FVector Axis = B - A;
			return Add(Name, Mesh, (A + B) * 0.5, FVector(D, D, Axis.Size()), FRotationMatrix::MakeFromZ(Axis.GetSafeNormal()).Rotator(), Paint, bShadow);
		};
		auto Ball = [&Add](const FString& Name, const FVector& Centre, const FVector& Size, EDeepHullPaint Paint, bool bShadow = true)
		{
			return Add(Name, DeepShapes::Sphere, Centre, Size, FRotator::ZeroRotator, Paint, bShadow);
		};

		const TCHAR* Side[2] = { TEXT("Port"), TEXT("Stbd") };
		const double Sgn[2] = { -1.0, 1.0 };

		// Buoyancy foam high: three blocks a side, one fore and one aft of the tower, on cross-members standing off the
		// ceiling, so a dark gap shows between foam and hull. Top 54 cm over the ceiling.
		const double FoamZ0 = OH + 14.0;
		const double FoamZ1 = OH + 54.0;
		const double FoamH = FoamZ1 - FoamZ0;
		const double FoamX0 = -OL + 20.0;
		const double FoamSpan = 2.0 * (OL - 20.0);
		const double Gap = 8.0;
		const double BlockL = (FoamSpan - 2.0 * Gap) / 3.0;
		for (int32 S = 0; S < 2; ++S)
		{
			for (int32 I = 0; I < 3; ++I)
			{
				const double X = FoamX0 + BlockL * 0.5 + I * (BlockL + Gap);
				Box(FString::Printf(TEXT("Foam%s%d"), Side[S], I), FVector(X, Sgn[S] * (78.0 + OW - 5.0) * 0.5, (FoamZ0 + FoamZ1) * 0.5),
					FVector(BlockL, OW - 5.0 - 78.0, FoamH), EDeepHullPaint::Foam);
			}
		}
		Box(TEXT("FoamFore"), FVector((258.0 + OL - 20.0) * 0.5, 0.0, (FoamZ0 + FoamZ1) * 0.5), FVector(OL - 20.0 - 258.0, 148.0, FoamH), EDeepHullPaint::Foam);
		Box(TEXT("FoamAft"), FVector((-OL + 20.0 - 58.0) * 0.5, 0.0, (FoamZ0 + FoamZ1) * 0.5), FVector(OL - 20.0 - 58.0, 148.0, FoamH), EDeepHullPaint::Foam);
		const double CrossX[4] = { -OL + 30.0, -152.0, 152.0, OL - 30.0 };
		for (int32 I = 0; I < 4; ++I)
		{
			Rod(FString::Printf(TEXT("Cross%d"), I), FVector(CrossX[I], -OW - 8.0, OH + 7.0), FVector(CrossX[I], OW + 8.0, OH + 7.0), 10.0, EDeepHullPaint::Frame);
		}

		// The tube frame: a top rail each side a little past both ends, posts down the sides into the ballast tanks, a
		// grab rail at mid height.
		const double PostY = OW + 7.0;
		const double TankY = OW + 5.0;
		const double TankZ = -OH + 10.0;
		const double TankD = 60.0;
		const double PostX[5] = { -380.0, -190.0, 0.0, 190.0, 380.0 };
		for (int32 S = 0; S < 2; ++S)
		{
			const double Y = Sgn[S];
			Rod(FString::Printf(TEXT("TopRail%s"), Side[S]), FVector(-OL - 20.0, Y * PostY, OH + 7.0), FVector(OL + 20.0, Y * PostY, OH + 7.0), 10.0, EDeepHullPaint::Frame);
			for (int32 I = 0; I < 5; ++I)
			{
				Rod(FString::Printf(TEXT("Post%s%d"), Side[S], I), FVector(PostX[I], Y * PostY, TankZ + TankD * 0.5 - 4.0), FVector(PostX[I], Y * PostY, OH + 7.0), 9.0, EDeepHullPaint::Frame);
			}
			Rod(FString::Printf(TEXT("GrabRail%s"), Side[S]), FVector(-400.0, Y * (OW + 16.0), 40.0), FVector(400.0, Y * (OW + 16.0), 40.0), 6.0, EDeepHullPaint::Frame, false);

			// Soft ballast tanks along the lower corners: domed ends, three straps each.
			Rod(FString::Printf(TEXT("Tank%s"), Side[S]), FVector(-400.0, Y * TankY, TankZ), FVector(400.0, Y * TankY, TankZ), TankD, EDeepHullPaint::Tank);
			Ball(FString::Printf(TEXT("TankCapAft%s"), Side[S]), FVector(-400.0, Y * TankY, TankZ), FVector(70.0, TankD, TankD), EDeepHullPaint::Tank);
			Ball(FString::Printf(TEXT("TankCapFore%s"), Side[S]), FVector(400.0, Y * TankY, TankZ), FVector(70.0, TankD, TankD), EDeepHullPaint::Tank);
			for (int32 I = 0; I < 3; ++I)
			{
				const double X = -260.0 + 260.0 * I;
				Rod(FString::Printf(TEXT("TankStrap%s%d"), Side[S], I), FVector(X - 4.0, Y * TankY, TankZ), FVector(X + 4.0, Y * TankY, TankZ), TankD + 5.0, EDeepHullPaint::Frame, false);
			}

			// Skids under the belly, turned up at the bow, on three struts.
			const double SkidY = Y * 165.0;
			const double SkidZ = -OH - 45.0;
			Rod(FString::Printf(TEXT("Skid%s"), Side[S]), FVector(-380.0, SkidY, SkidZ), FVector(330.0, SkidY, SkidZ), 10.0, EDeepHullPaint::Frame);
			Rod(FString::Printf(TEXT("SkidNose%s"), Side[S]), FVector(328.0, SkidY, SkidZ), FVector(420.0, SkidY, -OH - 3.0), 10.0, EDeepHullPaint::Frame);
			const double StrutX[3] = { -300.0, 0.0, 260.0 };
			for (int32 I = 0; I < 3; ++I)
			{
				Rod(FString::Printf(TEXT("SkidStrut%s%d"), Side[S], I), FVector(StrutX[I], SkidY, -OH), FVector(StrutX[I], SkidY, SkidZ), 8.0, EDeepHullPaint::Frame, false);
			}

			// Battery pods low and aft (heavy things low), domed ends.
			const double PodY = Y * 205.0;
			const double PodZ = -OH - 22.0;
			Rod(FString::Printf(TEXT("Pod%s"), Side[S]), FVector(-400.0, PodY, PodZ), FVector(-140.0, PodY, PodZ), 36.0, EDeepHullPaint::Tank);
			Ball(FString::Printf(TEXT("PodCapAft%s"), Side[S]), FVector(-400.0, PodY, PodZ), FVector(30.0, 36.0, 36.0), EDeepHullPaint::Tank);
			Ball(FString::Printf(TEXT("PodCapFore%s"), Side[S]), FVector(-140.0, PodY, PodZ), FVector(30.0, 36.0, 36.0), EDeepHullPaint::Tank);

			// Stern thrusters in ducts: the duct, its dark mouth and the hub.
			const double DuctY = Y * 150.0;
			Rod(FString::Printf(TEXT("Duct%s"), Side[S]), FVector(-OL - 2.0, DuctY, 10.0), FVector(-OL - 47.0, DuctY, 10.0), 66.0, EDeepHullPaint::Frame);
			Rod(FString::Printf(TEXT("DuctMouth%s"), Side[S]), FVector(-OL - 46.0, DuctY, 10.0), FVector(-OL - 49.0, DuctY, 10.0), 54.0, EDeepHullPaint::Dark, false);
			Rod(FString::Printf(TEXT("DuctHub%s"), Side[S]), FVector(-OL - 30.0, DuctY, 10.0), FVector(-OL - 58.0, DuctY, 10.0), 18.0, EDeepHullPaint::Frame, false, DeepShapes::Cone);

			// Hull markers: a short housing out of the side, the light at its tip.
			Rod(FString::Printf(TEXT("Marker%s"), Side[S]), FVector(400.0, Y * OW, 130.0), FVector(400.0, Y * (OW + 10.0), 130.0), 14.0, EDeepHullPaint::Frame, false);
		}

		// Work lamps on the top rails amidships, aimed down the sides (the lights are ADeepSubmarine::HullLights).
		for (int32 S = 0; S < 2; ++S)
		{
			Rod(FString::Printf(TEXT("WorkLamp%s"), Side[S]), FVector(95.0, Sgn[S] * PostY, OH + 12.0), FVector(95.0, Sgn[S] * (PostY + 6.0), OH + 1.0), 16.0, EDeepHullPaint::Frame, false);
		}

		// A manipulator stowed along the starboard side of the bow: base, upper arm, elbow, forearm, wrist, two jaws.
		{
			const FVector Shoulder(484.0, 140.0, -118.0);
			const FVector Elbow(600.0, 172.0, -150.0);
			const FVector Wrist(700.0, 122.0, -172.0);
			Box(TEXT("ArmBase"), Shoulder + FVector(1.0, 0.0, 0.0), FVector(30.0, 26.0, 26.0), EDeepHullPaint::Frame);
			Rod(TEXT("ArmUpper"), Shoulder, Elbow, 13.0, EDeepHullPaint::Tank);
			Ball(TEXT("ArmElbow"), Elbow, FVector(17.0), EDeepHullPaint::Frame);
			Rod(TEXT("ArmFore"), Elbow, Wrist, 10.0, EDeepHullPaint::Tank);
			Ball(TEXT("ArmWrist"), Wrist, FVector(13.0), EDeepHullPaint::Frame, false);
			Rod(TEXT("ArmJawA"), Wrist, Wrist + FVector(38.0, -12.0, -10.0), 7.0, EDeepHullPaint::Frame, false, DeepShapes::Cone);
			Rod(TEXT("ArmJawB"), Wrist, Wrist + FVector(38.0, 10.0, -16.0), 7.0, EDeepHullPaint::Frame, false, DeepShapes::Cone);
		}

		// Gas cylinders racked on the stern between the ducts, domed, two straps.
		const double GasY[4] = { -75.0, -27.0, 27.0, 75.0 };
		for (int32 I = 0; I < 4; ++I)
		{
			Rod(FString::Printf(TEXT("Gas%d"), I), FVector(-OL - 16.0, GasY[I], -120.0), FVector(-OL - 16.0, GasY[I], 60.0), 24.0, EDeepHullPaint::Tank);
			Ball(FString::Printf(TEXT("GasTop%d"), I), FVector(-OL - 16.0, GasY[I], 60.0), FVector(24.0, 24.0, 22.0), EDeepHullPaint::Tank, false);
		}
		Box(TEXT("GasStrapHigh"), FVector(-OL - 16.0, 0.0, 30.0), FVector(28.0, 180.0, 6.0), EDeepHullPaint::Frame, false);
		Box(TEXT("GasStrapLow"), FVector(-OL - 16.0, 0.0, -80.0), FVector(28.0, 180.0, 6.0), EDeepHullPaint::Frame, false);

		// The belly flood's housing (the light sits 10 cm under the belly at x 150) and its dark bezel.
		Rod(TEXT("FloodHousing"), FVector(150.0, 0.0, -OH), FVector(150.0, 0.0, -OH - 7.0), 36.0, EDeepHullPaint::Frame, false);
		Rod(TEXT("FloodBezel"), FVector(150.0, 0.0, -OH - 6.0), FVector(150.0, 0.0, -OH - 9.0), 40.0, EDeepHullPaint::Dark, false);

		// The hatch well: a collar under the opening (outside the column), a worn ochre lip, grab rails round it.
		const double In = Hh + 4.0;
		const double CollarT = 10.0;
		const double CollarZ0 = -OH;
		const double CollarZ1 = -OH - 42.0;
		const double CZ = (CollarZ0 + CollarZ1) * 0.5;
		const double CH = CollarZ0 - CollarZ1;
		Box(TEXT("WellPortWall"), FVector(0.0, -(In + CollarT * 0.5), CZ), FVector(2.0 * (In + CollarT), CollarT, CH), EDeepHullPaint::Hull);
		Box(TEXT("WellStbdWall"), FVector(0.0, In + CollarT * 0.5, CZ), FVector(2.0 * (In + CollarT), CollarT, CH), EDeepHullPaint::Hull);
		Box(TEXT("WellForeWall"), FVector(In + CollarT * 0.5, 0.0, CZ), FVector(CollarT, 2.0 * In, CH), EDeepHullPaint::Hull);
		Box(TEXT("WellAftWall"), FVector(-(In + CollarT * 0.5), 0.0, CZ), FVector(CollarT, 2.0 * In, CH), EDeepHullPaint::Hull);
		const double LipW = 26.0;
		const double LipZ = CollarZ1 - 3.0;
		Box(TEXT("WellPortLip"), FVector(0.0, -(In + LipW * 0.5), LipZ), FVector(2.0 * (In + LipW), LipW, 6.0), EDeepHullPaint::Rim);
		Box(TEXT("WellStbdLip"), FVector(0.0, In + LipW * 0.5, LipZ), FVector(2.0 * (In + LipW), LipW, 6.0), EDeepHullPaint::Rim);
		Box(TEXT("WellForeLip"), FVector(In + LipW * 0.5, 0.0, LipZ), FVector(LipW, 2.0 * In, 6.0), EDeepHullPaint::Rim);
		Box(TEXT("WellAftLip"), FVector(-(In + LipW * 0.5), 0.0, LipZ), FVector(LipW, 2.0 * In, 6.0), EDeepHullPaint::Rim);
		const double RailZ = -OH - 30.0;
		const double RailOff = In + LipW + 8.0;  // 118: clear of the lip and of the flood housing at x 132
		for (int32 S = 0; S < 2; ++S)
		{
			const double Y = Sgn[S];
			Rod(FString::Printf(TEXT("WellRailY%s"), Side[S]), FVector(-90.0, Y * RailOff, RailZ), FVector(90.0, Y * RailOff, RailZ), 5.0, EDeepHullPaint::Frame, false);
			Rod(FString::Printf(TEXT("WellRailX%d"), S), FVector(Y * RailOff, -90.0, RailZ), FVector(Y * RailOff, 90.0, RailZ), 5.0, EDeepHullPaint::Frame, false);
			for (int32 E = 0; E < 2; ++E)
			{
				const double A = Sgn[E] * 75.0;
				Rod(FString::Printf(TEXT("WellPostY%s%d"), Side[S], E), FVector(A, Y * RailOff, -OH), FVector(A, Y * RailOff, RailZ), 4.0, EDeepHullPaint::Frame, false);
				Rod(FString::Printf(TEXT("WellPostX%d%d"), S, E), FVector(Y * RailOff, A, -OH), FVector(Y * RailOff, A, RailZ), 4.0, EDeepHullPaint::Frame, false);
			}
		}

		// The tether's guide: a two-leg bridle from the well's fore and aft lips to a shackle at the anchor, where the line
		// is tied. Thin, so the column stays open.
		const FVector Shackle(0.0, 0.0, AnchorZCm + 3.0);
		Out[Rod(TEXT("BridleFore"), FVector(In + 10.0, 0.0, LipZ - 3.0), Shackle, 4.0, EDeepHullPaint::Frame, false)].bInHatchColumn = true;
		Out[Rod(TEXT("BridleAft"), FVector(-In - 10.0, 0.0, LipZ - 3.0), Shackle, 4.0, EDeepHullPaint::Frame, false)].bInHatchColumn = true;
		Out[Ball(TEXT("Shackle"), Shackle, FVector(10.0, 10.0, 12.0), EDeepHullPaint::Rim, false)].bInHatchColumn = true;

		// Doubler plates: the cheapest submarine on the market has been patched.
		Box(TEXT("PatchStbd"), FVector(-270.0, OW + 1.5, -50.0), FVector(120.0, 3.0, 80.0), EDeepHullPaint::Patch, false);
		Box(TEXT("PatchPort"), FVector(110.0, -OW - 1.5, 80.0), FVector(70.0, 3.0, 55.0), EDeepHullPaint::Patch, false);
		Box(TEXT("PatchBelly"), FVector(-290.0, 70.0, -OH - 1.5), FVector(100.0, 70.0, 3.0), EDeepHullPaint::Patch, false);
		Box(TEXT("PatchStern"), FVector(-OL - 1.5, 10.0, 115.0), FVector(3.0, 110.0, 50.0), EDeepHullPaint::Patch, false);

		// The helm window outside: a retaining frame round the glass and a guard visor over it.
		const double WinZ = 40.0;
		const double WinHalfW = 85.0;
		const double WinHalfH = 35.0;
		const double FrameW = 16.0;
		Box(TEXT("WindowFrameTop"), FVector(OL + 4.0, 0.0, WinZ + WinHalfH + FrameW * 0.5), FVector(8.0, 2.0 * (WinHalfW + FrameW), FrameW), EDeepHullPaint::Frame);
		Box(TEXT("WindowFrameBottom"), FVector(OL + 4.0, 0.0, WinZ - WinHalfH - FrameW * 0.5), FVector(8.0, 2.0 * (WinHalfW + FrameW), FrameW), EDeepHullPaint::Frame);
		Box(TEXT("WindowFramePort"), FVector(OL + 4.0, -(WinHalfW + FrameW * 0.5), WinZ), FVector(8.0, FrameW, 2.0 * WinHalfH), EDeepHullPaint::Frame);
		Box(TEXT("WindowFrameStbd"), FVector(OL + 4.0, WinHalfW + FrameW * 0.5, WinZ), FVector(8.0, FrameW, 2.0 * WinHalfH), EDeepHullPaint::Frame);
		Box(TEXT("WindowVisor"), FVector(OL + 18.0, 0.0, WinZ + WinHalfH + FrameW + 8.0), FVector(36.0, 240.0, 6.0), EDeepHullPaint::Frame);

		// Tower top: a hatch, a grab rail each side, a strobe mast aft.
		const double TowerTop = OH + 220.0;
		Rod(TEXT("TowerHatch"), FVector(130.0, 0.0, TowerTop), FVector(130.0, 0.0, TowerTop + 6.0), 70.0, EDeepHullPaint::Hull, false);
		for (int32 S = 0; S < 2; ++S)
		{
			const double Y = Sgn[S] * 62.0;
			Rod(FString::Printf(TEXT("TowerRail%s"), Side[S]), FVector(-40.0, Y, TowerTop + 14.0), FVector(240.0, Y, TowerTop + 14.0), 5.0, EDeepHullPaint::Frame, false);
			Rod(FString::Printf(TEXT("TowerPost%sA"), Side[S]), FVector(-30.0, Y, TowerTop), FVector(-30.0, Y, TowerTop + 14.0), 4.0, EDeepHullPaint::Frame, false);
			Rod(FString::Printf(TEXT("TowerPost%sB"), Side[S]), FVector(230.0, Y, TowerTop), FVector(230.0, Y, TowerTop + 14.0), 4.0, EDeepHullPaint::Frame, false);
		}
		Rod(TEXT("Mast"), FVector(-20.0, 0.0, TowerTop), FVector(-20.0, 0.0, TowerTop + 50.0), 6.0, EDeepHullPaint::Frame, false);
		Ball(TEXT("MastCap"), FVector(-20.0, 0.0, TowerTop + 52.0), FVector(12.0, 12.0, 12.0), EDeepHullPaint::Dark, false);
		return Out;
	}

	FBox PartBox(const FDeepHullPart& Part)
	{
		const FTransform T(Part.Rotation, Part.Centre, Part.Size / DeepShapes::ShapeSizeCm);
		return FBox(FVector(-50.0), FVector(50.0)).TransformBy(T);
	}
}
