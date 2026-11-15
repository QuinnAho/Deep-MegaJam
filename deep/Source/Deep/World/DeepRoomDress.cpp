#include "World/DeepRoomDress.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LocalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepShapes.h"
#include "World/DeepSubmarine.h"

namespace
{
	const FRotator RdAlongX(90.f, 0.f, 0.f);  // a cylinder's axis turned to X
	const FRotator RdAlongY(0.f, 0.f, 90.f);  // ... to Y
	const FRotator RdAlongZ = FRotator::ZeroRotator;

	struct FRdRoom
	{
		double L, W, Deck, Ceil, Hh;
		FVector2D Console, Helm;
	};

	FRdRoom RdRoomOf(const FDeepSubTuning& T)
	{
		const double M = UDeepRules::CmPerM;
		FRdRoom R;
		R.L = T.RoomHalfLengthM * M;
		R.W = T.RoomHalfWidthM * M;
		R.Deck = -T.RoomHeightM * M * 0.5;
		R.Ceil = T.RoomHeightM * M * 0.5;
		R.Hh = T.HatchHalfSizeM * M;
		R.Console = T.ConsoleM * M;
		R.Helm = T.HelmM * M;
		return R;
	}

	struct FRdBuilder
	{
		TArray<FDeepRoomPiece> Out;
		void Cube(const FVector& C, const FVector& S, EDeepRoomMat Mat, const FRotator& R = FRotator::ZeroRotator)
		{
			Out.Add({ EDeepRoomMesh::Cube, C, S, R, Mat });
		}
		void Cyl(const FVector& C, double Diameter, double Length, const FRotator& Axis, EDeepRoomMat Mat)
		{
			Out.Add({ EDeepRoomMesh::Cylinder, C, FVector(Diameter, Diameter, Length), Axis, Mat });
		}
		void Ball(const FVector& C, double Diameter, EDeepRoomMat Mat)
		{
			Out.Add({ EDeepRoomMesh::Sphere, C, FVector(Diameter), FRotator::ZeroRotator, Mat });
		}
		/** A valve wheel facing along Axis (unit X or Y) at C: rim, two spokes, hub. */
		void Wheel(const FVector& C, double Diameter, const FVector& Axis)
		{
			const bool bY = FMath::Abs(Axis.Y) > 0.5;
			Cyl(C, Diameter, 3.0, bY ? RdAlongY : RdAlongX, EDeepRoomMat::Brass);
			Cube(C - Axis * 1.0, bY ? FVector(Diameter - 4.0, 2.5, 2.5) : FVector(2.5, Diameter - 4.0, 2.5), EDeepRoomMat::Brass);
			Cube(C - Axis * 1.0, FVector(2.5, 2.5, Diameter - 4.0), EDeepRoomMat::Brass);
			Cyl(C + Axis * 1.5, 7.0, 5.0, bY ? RdAlongY : RdAlongX, EDeepRoomMat::Brass);
		}
		/** A gauge on a wall whose inward normal is N: brass bezel, grimy glass face. */
		void Gauge(const FVector& OnWall, const FVector& N, double Diameter)
		{
			const FRotator Axis = FMath::Abs(N.Y) > 0.5 ? RdAlongY : RdAlongX;
			Cyl(OnWall + N * 2.0, Diameter, 4.0, Axis, EDeepRoomMat::Brass);
			Cyl(OnWall + N * 4.3, Diameter - 3.0, 1.0, Axis, EDeepRoomMat::Glass);
		}
		/** A caged bulb on a surface whose inward normal is N (a unit axis): base plate, bulb, four bars, end ring. */
		void Cage(const FVector& OnSurface, const FVector& N)
		{
			const FRotator Axis = FMath::Abs(N.Z) > 0.5 ? RdAlongZ : (FMath::Abs(N.Y) > 0.5 ? RdAlongY : RdAlongX);
			Cyl(OnSurface + N * 1.5, 15.0, 3.0, Axis, EDeepRoomMat::Frame);
			Ball(OnSurface + N * 9.0, 8.0, EDeepRoomMat::Glass);
			// Two axes across the normal for the bars.
			const FVector U = FMath::Abs(N.Z) > 0.5 ? FVector(1, 0, 0) : FVector(0, 0, 1);
			const FVector V = FVector::CrossProduct(N, U).GetSafeNormal();
			const FVector BarSize = FVector(1.2) + N.GetAbs() * 13.8;
			for (const FVector& Off : { U * 6.0, U * -6.0, V * 6.0, V * -6.0 })
			{
				Cube(OnSurface + N * 9.5 + Off, BarSize, EDeepRoomMat::Frame);
			}
			Cyl(OnSurface + N * 16.0, 14.0, 1.5, Axis, EDeepRoomMat::Frame);
		}
	};

	/** Light point for a cage on a surface: just in front of the bulb, so the bulb's face catches it. */
	FVector RdCageLight(const FVector& OnSurface, const FVector& N)
	{
		return OnSurface + N * 20.0;
	}

	/** The cool strip low on the port wall, under the pipes. */
	struct FRdStrip
	{
		FVector Centre;
		double Length;
	};
	FRdStrip RdPortStrip(const FRdRoom& R)
	{
		const double AftX = -100.0;
		const double ForeX = 230.0;
		return { FVector((AftX + ForeX) * 0.5, -R.W + 3.0, R.Deck + 228.0), ForeX - AftX };
	}

	FVector RdConsoleCage(const FRdRoom& R) { return FVector(R.Console.X, R.W, R.Deck + 218.0); }
	FVector RdHelmCage(const FRdRoom& R) { return FVector(R.Helm.X + 30.0, R.Helm.Y, R.Ceil); }
	FVector RdAftCage(const FRdRoom& R) { return FVector(-R.L, -R.W + 125.0, R.Deck + 180.0); }
	FVector RdForeCage(const FRdRoom& R) { return FVector(R.L, -R.W + 45.0, R.Deck + 150.0); }

	/** 2D distance from a point to a box's XY rectangle. */
	double RdDistToRect(const FVector2D& P, const FBox& B)
	{
		const double Dx = FMath::Max3(B.Min.X - P.X, 0.0, P.X - B.Max.X);
		const double Dy = FMath::Max3(B.Min.Y - P.Y, 0.0, P.Y - B.Max.Y);
		return FMath::Sqrt(Dx * Dx + Dy * Dy);
	}

	bool RdBelowHead(const FBox& B, const FDeepSubTuning& Sub, const FDeepRoomDressTuning& Dress)
	{
		return B.Min.Z < RdRoomOf(Sub).Deck + Dress.HeadClearCm;
	}

	int32 RdMeshIndex(EDeepRoomMesh Mesh, EDeepRoomMat Mat)
	{
		return int32(Mesh) * int32(EDeepRoomMat::Count) + int32(Mat);
	}

	/** `Deep.Room.Dress 0|1`: hides the dressing and its lights for before-and-after frames and frame time. */
	FAutoConsoleCommandWithWorldAndArgs GRoomDressCmd(
		TEXT("Deep.Room.Dress"),
		TEXT("Deep.Room.Dress <0|1>: 1 shows the room's dressing (ribs, pipes, lockers, caged lights), 0 hides it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const bool bOn = Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0;
			for (TActorIterator<ADeepSubmarine> It(World); It; ++It)
			{
				It->SetRoomDressVisible(bOn);
			}
			UE_LOG(LogTemp, Log, TEXT("DEEPROOMDRESS visible=%d pieces=%d"), bOn ? 1 : 0, DeepRoomDress::Pieces(UDeepTuningSettings::Get().Sub).Num());
		}));
}

TArray<FDeepRoomPiece> DeepRoomDress::Pieces(const FDeepSubTuning& Sub)
{
	const FRdRoom R = RdRoomOf(Sub);
	FRdBuilder B;
	const double H = R.Ceil - R.Deck;

	// Hull ribs: a frame every 90 cm, a flat bar across the ceiling between the strips and a leg down each wall, except
	// where the lockers (port aft) and the console (starboard) stand against the wall.
	const double ConsoleMin = R.Console.X - 75.0;
	const double ConsoleMax = R.Console.X + 70.0;
	for (int32 I = 0; I < 9; ++I)
	{
		const double X = -R.L + 45.0 + 90.0 * I;
		B.Cube(FVector(X, 0.0, R.Ceil - 6.0), FVector(8.0, 2.0 * (R.W - 58.0), 12.0), EDeepRoomMat::Frame);
		if (X > -R.L + 200.0)
		{
			B.Cube(FVector(X, -R.W + 4.0, 0.0), FVector(8.0, 8.0, H), EDeepRoomMat::Frame);
		}
		if ((X < ConsoleMin || X > ConsoleMax) && FMath::Abs(X - (R.Helm.X - 60.0)) > 60.0)
		{
			B.Cube(FVector(X, R.W - 4.0, 0.0), FVector(8.0, 8.0, H), EDeepRoomMat::Frame);
		}
	}

	// Port pipe run, high on the wall: three pipes in segments between flanges.
	struct FPipe { double Y, Z, D; };
	const FPipe Pipes[] = { { -R.W + 12.0, R.Deck + 268.0, 12.0 }, { -R.W + 22.0, R.Deck + 252.0, 8.0 }, { -R.W + 11.0, R.Deck + 238.0, 6.0 } };
	const double Breaks[] = { -R.L + 4.0, -270.0, -10.0, 190.0, R.L - 4.0 };
	for (const FPipe& P : Pipes)
	{
		for (int32 S = 0; S + 1 < UE_ARRAY_COUNT(Breaks); ++S)
		{
			B.Cyl(FVector((Breaks[S] + Breaks[S + 1]) * 0.5, P.Y, P.Z), P.D, Breaks[S + 1] - Breaks[S] - 2.0, RdAlongX, EDeepRoomMat::Pipe);
		}
		for (int32 S = 1; S + 1 < UE_ARRAY_COUNT(Breaks); ++S)
		{
			B.Cyl(FVector(Breaks[S], P.Y, P.Z), P.D + 7.0, 4.0, RdAlongX, EDeepRoomMat::Pipe);
		}
	}
	// A drop from the middle pipe to a valve at chest height, on into the deck.
	{
		const FPipe& P = Pipes[1];
		const double X = -60.0;
		const double ValveZ = R.Deck + 130.0;
		B.Cyl(FVector(X, P.Y, (P.Z + ValveZ) * 0.5), P.D, P.Z - ValveZ, RdAlongZ, EDeepRoomMat::Pipe);
		B.Cyl(FVector(X, P.Y, (ValveZ + R.Deck) * 0.5), P.D, ValveZ - R.Deck, RdAlongZ, EDeepRoomMat::Pipe);
		B.Cube(FVector(X, P.Y, ValveZ), FVector(15.0, 15.0, 18.0), EDeepRoomMat::Pipe);
		B.Cyl(FVector(X, P.Y + 13.0, ValveZ), 3.0, 12.0, RdAlongY, EDeepRoomMat::Brass);
		B.Wheel(FVector(X, P.Y + 20.0, ValveZ), 30.0, FVector(0, 1, 0));
		B.Gauge(FVector(X - 34.0, -R.W, ValveZ + 20.0), FVector(0, 1, 0), 15.0);
		B.Gauge(FVector(X + 34.0, -R.W, ValveZ + 20.0), FVector(0, 1, 0), 12.0);
	}
	// A gate valve on the top pipe forward.
	{
		const FPipe& P = Pipes[0];
		const double X = 330.0;
		B.Cube(FVector(X, P.Y, P.Z), FVector(18.0, 18.0, 18.0), EDeepRoomMat::Pipe);
		B.Cyl(FVector(X, P.Y + 13.0, P.Z), 3.0, 10.0, RdAlongY, EDeepRoomMat::Brass);
		B.Wheel(FVector(X, P.Y + 19.0, P.Z), 26.0, FVector(0, 1, 0));
	}

	// Starboard cable tray under the ceiling, three cables in it, drops beside the console and down to the helm.
	{
		const double TrayZ = R.Deck + 274.0;
		const double TrayY = R.W - 18.0;
		B.Cube(FVector(0.0, TrayY, TrayZ), FVector(2.0 * R.L - 8.0, 26.0, 3.0), EDeepRoomMat::Frame);
		B.Cube(FVector(0.0, TrayY - 12.0, TrayZ + 4.0), FVector(2.0 * R.L - 8.0, 2.0, 8.0), EDeepRoomMat::Frame);
		const double CableY[] = { TrayY - 6.0, TrayY, TrayY + 6.0 };
		const double CableD[] = { 4.0, 5.0, 3.5 };
		for (int32 C = 0; C < 3; ++C)
		{
			B.Cyl(FVector(0.0, CableY[C], TrayZ + 1.5 + CableD[C] * 0.5), CableD[C], 2.0 * R.L - 10.0, RdAlongX, EDeepRoomMat::Rubber);
		}
		const double DropTop = TrayZ;
		const double ConsoleTop = R.Deck + 110.0;
		for (const double X : { ConsoleMin - 2.0, ConsoleMax - 5.0 })
		{
			B.Cyl(FVector(X, R.W - 4.0, (DropTop + ConsoleTop) * 0.5), 4.0, DropTop - ConsoleTop, RdAlongZ, EDeepRoomMat::Rubber);
		}
		const double HelmTop = R.Deck + 90.0;
		B.Cyl(FVector(R.Helm.X + 30.0, R.W - 4.0, (DropTop + HelmTop) * 0.5), 4.5, DropTop - HelmTop, RdAlongZ, EDeepRoomMat::Rubber);
	}

	// Gauges over the console, and two on the forward wall beside the window for the helm.
	for (const double Dx : { -32.0, 0.0, 32.0 })
	{
		B.Gauge(FVector(R.Console.X + Dx, R.W, R.Deck + 160.0), FVector(0, -1, 0), Dx == 0.0 ? 18.0 : 13.0);
	}
	B.Gauge(FVector(R.L, R.W - 95.0, R.Deck + 195.0), FVector(-1, 0, 0), 16.0);
	B.Gauge(FVector(R.L, R.W - 60.0, R.Deck + 165.0), FVector(-1, 0, 0), 13.0);

	// Lockers along the port wall aft: body, brass handle, a vent plate.
	for (int32 I = 0; I < 3; ++I)
	{
		const double X = -R.L + 35.0 + 62.0 * I;
		const double Y = -R.W + 22.5;
		B.Cube(FVector(X, Y, R.Deck + 95.0), FVector(60.0, 45.0, 190.0), EDeepRoomMat::Frame);
		B.Cube(FVector(X + 20.0, Y + 23.5, R.Deck + 105.0), FVector(3.0, 3.0, 16.0), EDeepRoomMat::Brass);
		B.Cube(FVector(X, Y + 23.0, R.Deck + 165.0), FVector(32.0, 1.5, 14.0), EDeepRoomMat::Rubber);
	}

	// A fold-down bench on the port wall between two ribs: plank, cushion, wall bar, two struts.
	{
		const double X = -180.0;
		const double Y = -R.W + 20.0;
		B.Cube(FVector(X, Y, R.Deck + 45.0), FVector(80.0, 40.0, 5.0), EDeepRoomMat::Frame);
		B.Cube(FVector(X, Y, R.Deck + 49.5), FVector(76.0, 36.0, 4.0), EDeepRoomMat::Rubber);
		B.Cube(FVector(X, -R.W + 2.0, R.Deck + 42.0), FVector(80.0, 4.0, 6.0), EDeepRoomMat::Frame);
		for (const double Dx : { -35.0, 35.0 })
		{
			B.Cube(FVector(X + Dx, -R.W + 19.0, R.Deck + 26.0), FVector(3.0, 3.0, 46.0), EDeepRoomMat::Frame, FRotator(0.f, 0.f, -48.f));
		}
	}

	// A ladder up the port wall to the ceiling scuttle: two stiles, rungs, wall brackets, the scuttle and its wheel.
	{
		const double X = 270.0;
		const double Y = -R.W + 33.0;
		for (const double Dx : { -18.0, 18.0 })
		{
			B.Cube(FVector(X + Dx, Y, 0.0), FVector(5.0, 5.0, H - 12.0), EDeepRoomMat::Frame);
			for (const double Z : { R.Deck + 50.0, R.Deck + 190.0 })
			{
				B.Cube(FVector(X + Dx, (-R.W + Y) * 0.5, Z), FVector(4.0, Y + R.W - 2.0, 4.0), EDeepRoomMat::Frame);
			}
		}
		for (int32 Rung = 0; Rung < 9; ++Rung)
		{
			B.Cyl(FVector(X, Y, R.Deck + 30.0 + 30.0 * Rung), 3.0, 36.0, RdAlongX, EDeepRoomMat::Pipe);
		}
		B.Cyl(FVector(X, -R.W + 65.0, R.Ceil - 3.0), 70.0, 6.0, RdAlongZ, EDeepRoomMat::Frame);
		B.Cyl(FVector(X, -R.W + 65.0, R.Ceil - 8.0), 30.0, 2.5, RdAlongZ, EDeepRoomMat::Brass);
		B.Cyl(FVector(X, -R.W + 65.0, R.Ceil - 10.0), 7.0, 4.0, RdAlongZ, EDeepRoomMat::Brass);
	}

	// Caged lights: amber over the console and the helm, red aft and forward port, and a cage on a short deck post for
	// the hatch's existing red light (ADeepSubmarine::HatchRed, Hh + 60 cm off the hatch's port edge, 35 cm up).
	B.Cage(RdConsoleCage(R), FVector(0, -1, 0));
	B.Cage(RdHelmCage(R), FVector(0, 0, -1));
	B.Cage(RdAftCage(R), FVector(1, 0, 0));
	B.Cage(RdForeCage(R), FVector(-1, 0, 0));
	{
		const FVector Post(0.0, -R.Hh - 60.0, R.Deck);
		B.Cyl(Post + FVector(0, 0, 12.5), 5.0, 25.0, RdAlongZ, EDeepRoomMat::Frame);
		B.Cage(Post + FVector(0, 0, 25.0), FVector(0, 0, 1));
	}

	// The low cool strip: a housing on the wall and its glass cover.
	{
		const FRdStrip S = RdPortStrip(R);
		B.Cube(S.Centre + FVector(0, -0.5, 0), FVector(S.Length, 5.0, 6.0), EDeepRoomMat::Frame);
		B.Cube(S.Centre + FVector(0, 3.0, -1.0), FVector(S.Length - 4.0, 2.0, 3.0), EDeepRoomMat::Glass);
	}
	return B.Out;
}

TArray<FDeepRoomLight> DeepRoomDress::Lights(const FDeepSubTuning& Sub)
{
	const FRdRoom R = RdRoomOf(Sub);
	TArray<FDeepRoomLight> Out;
	Out.Add({ EDeepRoomLightKind::Practical, RdCageLight(RdConsoleCage(R), FVector(0, -1, 0)) });
	Out.Add({ EDeepRoomLightKind::Practical, RdCageLight(RdHelmCage(R), FVector(0, 0, -1)) });
	Out.Add({ EDeepRoomLightKind::Emergency, RdCageLight(RdAftCage(R), FVector(1, 0, 0)) });
	Out.Add({ EDeepRoomLightKind::Emergency, RdCageLight(RdForeCage(R), FVector(-1, 0, 0)) });
	const FRdStrip S = RdPortStrip(R);
	// Facing into the room and down: yawed to +Y, pitched down; its width axis then runs along the room.
	Out.Add({ EDeepRoomLightKind::Strip, S.Centre + FVector(0, 6.0, -2.0), FRotator(-55.f, 90.f, 0.f), FVector2D(S.Length - 10.0, 3.0) });
	return Out;
}

FBox DeepRoomDress::PieceBox(const FDeepRoomPiece& Piece)
{
	const FVector Half = Piece.Size * 0.5;
	return FBox(-Half, Half).TransformBy(FTransform(Piece.Rotation, Piece.Centre));
}

FBox DeepRoomDress::RoomInside(const FDeepSubTuning& Sub)
{
	const FRdRoom R = RdRoomOf(Sub);
	return FBox(FVector(-R.L, -R.W, R.Deck), FVector(R.L, R.W, R.Ceil));
}

FVector2D DeepRoomDress::HelmSeatCm(const FDeepSubTuning& Sub)
{
	// ADeepSubmarine::LayOut puts the seat 60 cm aft of the helm station.
	const FRdRoom R = RdRoomOf(Sub);
	return FVector2D(R.Helm.X - 60.0, R.Helm.Y);
}

TArray<TPair<FVector2D, FVector2D>> DeepRoomDress::Walks(const FDeepSubTuning& Sub)
{
	const double M = UDeepRules::CmPerM;
	const FVector2D Stations[] = { Sub.RoomStartM * M, Sub.EnterStandM * M, Sub.ConsoleM * M, Sub.HelmM * M };
	TArray<TPair<FVector2D, FVector2D>> Out;
	for (int32 A = 0; A < UE_ARRAY_COUNT(Stations); ++A)
	{
		for (int32 B = A + 1; B < UE_ARRAY_COUNT(Stations); ++B)
		{
			Out.Emplace(Stations[A], Stations[B]);
		}
		Out.Emplace(Stations[A], FVector2D::ZeroVector);
	}
	return Out;
}

bool DeepRoomDress::BlocksWalk(const FBox& Box, const FDeepSubTuning& Sub, const FDeepRoomDressTuning& Dress, double WalkerRadiusCm)
{
	if (!RdBelowHead(Box, Sub, Dress))
	{
		return false;
	}
	const double Clear = WalkerRadiusCm + Dress.WalkMarginCm;
	for (const TPair<FVector2D, FVector2D>& Walk : Walks(Sub))
	{
		const double Len = FVector2D::Distance(Walk.Key, Walk.Value);
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(Len / 5.0));
		for (int32 S = 0; S <= Steps; ++S)
		{
			if (RdDistToRect(FMath::Lerp(Walk.Key, Walk.Value, double(S) / Steps), Box) < Clear)
			{
				return true;
			}
		}
	}
	return false;
}

bool DeepRoomDress::InHatchZone(const FBox& Box, const FDeepSubTuning& Sub, const FDeepRoomDressTuning& Dress)
{
	const double Half = RdRoomOf(Sub).Hh + Dress.HatchClearCm;
	return RdBelowHead(Box, Sub, Dress) && Box.Min.X < Half && Box.Max.X > -Half && Box.Min.Y < Half && Box.Max.Y > -Half;
}

bool DeepRoomDress::InHelmZone(const FBox& Box, const FDeepSubTuning& Sub, const FDeepRoomDressTuning& Dress)
{
	return RdBelowHead(Box, Sub, Dress) && RdDistToRect(HelmSeatCm(Sub), Box) < Dress.HelmClearCm;
}

void DeepRoomDress::Create(AActor* Owner, USceneComponent* Parent, TArray<TObjectPtr<UInstancedStaticMeshComponent>>& OutMeshes,
	TArray<TObjectPtr<ULocalLightComponent>>& OutLights)
{
	static const TCHAR* MeshPaths[] = { DeepShapes::Cube, DeepShapes::Cylinder, DeepShapes::Sphere };
	static const TCHAR* MeshNames[] = { TEXT("Cube"), TEXT("Cyl"), TEXT("Ball") };
	static const TCHAR* MatNames[] = { TEXT("Frame"), TEXT("Pipe"), TEXT("Brass"), TEXT("Rubber"), TEXT("Glass") };
	OutMeshes.Reset();
	for (int32 Mesh = 0; Mesh < int32(EDeepRoomMesh::Count); ++Mesh)
	{
		for (int32 Mat = 0; Mat < int32(EDeepRoomMat::Count); ++Mat)
		{
			UInstancedStaticMeshComponent* Ism = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(
				*FString::Printf(TEXT("RoomDress_%s_%s"), MeshNames[Mesh], MatNames[Mat]));
			Ism->SetupAttachment(Parent);
			Ism->SetStaticMesh(DeepShapes::LoadMesh(MeshPaths[Mesh]));
			Ism->SetMobility(EComponentMobility::Movable);
			Ism->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Ism->SetGenerateOverlapEvents(false);
			Ism->SetCanEverAffectNavigation(false);
			// Small pieces stay out of distance fields; the big flat ones still cast shadows from the practicals.
			Ism->bAffectDistanceFieldLighting = false;
			Ism->SetCastShadow(Mat != int32(EDeepRoomMat::Glass));
			OutMeshes.Add(Ism);
		}
	}
	for (const FDeepRoomPiece& Piece : Pieces(UDeepTuningSettings::Get().Sub))
	{
		UInstancedStaticMeshComponent* Ism = OutMeshes[RdMeshIndex(Piece.Mesh, Piece.Mat)];
		Ism->AddInstance(FTransform(Piece.Rotation, Piece.Centre, Piece.Size / DeepShapes::ShapeSizeCm));
	}

	OutLights.Reset();
	int32 Index = 0;
	for (const FDeepRoomLight& Spec : Lights(UDeepTuningSettings::Get().Sub))
	{
		const FName Name(*FString::Printf(TEXT("RoomDressLight%d"), Index++));
		ULocalLightComponent* Light = nullptr;
		if (Spec.Kind == EDeepRoomLightKind::Strip)
		{
			URectLightComponent* Rect = Owner->CreateDefaultSubobject<URectLightComponent>(Name);
			Rect->SetSourceWidth(static_cast<float>(Spec.SourceCm.X));
			Rect->SetSourceHeight(static_cast<float>(Spec.SourceCm.Y));
			Rect->SetBarnDoorAngle(60.f);
			Rect->SetBarnDoorLength(6.f);
			Light = Rect;
		}
		else
		{
			UPointLightComponent* Point = Owner->CreateDefaultSubobject<UPointLightComponent>(Name);
			Point->SetSourceRadius(3.f);
			Light = Point;
		}
		Light->SetupAttachment(Parent);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetRelativeLocation(Spec.Centre);
		Light->SetRelativeRotation(Spec.Rotation);
		Light->SetCastShadows(false);
		OutLights.Add(Light);
	}
}

void DeepRoomDress::Skin(const TArray<TObjectPtr<UInstancedStaticMeshComponent>>& Meshes, const TArray<TObjectPtr<ULocalLightComponent>>& Lights,
	bool bGreyBox)
{
	const FDeepRoomDressTuning& D = UDeepTuningSettings::Get().RoomDress;
	const FSoftObjectPath* Paths[] = { &D.FrameMaterial, &D.PipeMaterial, &D.BrassMaterial, &D.RubberMaterial, &D.GlassMaterial };
	// Fallback tints if an instance is missing: the same paints, flat.
	const FLinearColor Fallback[] = { FLinearColor(0.03f, 0.033f, 0.032f), FLinearColor(0.045f, 0.045f, 0.04f), FLinearColor(0.14f, 0.10f, 0.055f),
		FLinearColor(0.012f, 0.012f, 0.012f), FLinearColor(0.008f, 0.01f, 0.01f) };
	for (int32 Index = 0; Index < Meshes.Num(); ++Index)
	{
		UInstancedStaticMeshComponent* Ism = Meshes[Index];
		if (!Ism)
		{
			continue;
		}
		const int32 Mat = Index % int32(EDeepRoomMat::Count);
		if (UMaterialInterface* M = Cast<UMaterialInterface>(Paths[Mat]->TryLoad()))
		{
			Ism->SetMaterial(0, M);
		}
		else
		{
			DeepShapes::Tint(Ism, Fallback[Mat]);
		}
	}
	const TArray<FDeepRoomLight> Specs = Lights.Num() ? DeepRoomDress::Lights(UDeepTuningSettings::Get().Sub) : TArray<FDeepRoomLight>();
	for (int32 Index = 0; Index < Lights.Num() && Index < Specs.Num(); ++Index)
	{
		ULocalLightComponent* Light = Lights[Index];
		if (!Light)
		{
			continue;
		}
		switch (Specs[Index].Kind)
		{
		case EDeepRoomLightKind::Practical:
			Light->SetLightColor(D.PracticalColor);
			Light->SetIntensity(D.PracticalIntensity);
			Light->SetAttenuationRadius(D.PracticalRadiusCm);
			break;
		case EDeepRoomLightKind::Emergency:
			Light->SetLightColor(D.EmergencyColor);
			Light->SetIntensity(D.EmergencyIntensity);
			Light->SetAttenuationRadius(D.EmergencyRadiusCm);
			break;
		default:
			Light->SetLightColor(D.StripColor);
			Light->SetIntensity(D.StripIntensity);
			Light->SetAttenuationRadius(D.StripRadiusCm);
			break;
		}
	}
	SetVisible(Meshes, Lights, D.bEnabled && !bGreyBox);
}

void DeepRoomDress::SetVisible(const TArray<TObjectPtr<UInstancedStaticMeshComponent>>& Meshes, const TArray<TObjectPtr<ULocalLightComponent>>& Lights,
	bool bVisible)
{
	for (UInstancedStaticMeshComponent* Ism : Meshes)
	{
		if (Ism)
		{
			Ism->SetVisibility(bVisible);
		}
	}
	for (ULocalLightComponent* Light : Lights)
	{
		if (Light)
		{
			Light->SetVisibility(bVisible);
		}
	}
}
