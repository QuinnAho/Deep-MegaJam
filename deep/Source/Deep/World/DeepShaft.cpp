#include "World/DeepShaft.h"

#include "Debug/DeepStateReport.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/DeepGameState.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Sim/DeepCaveRules.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepArena.h"
#include "World/DeepSubmarine.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepShaft, Log, All);

namespace
{
	TAutoConsoleVariable<int32> CVarDeepCaveShaft(
		TEXT("Deep.Cave.Shaft"), -1,
		TEXT("The cave shaft between stops: 1 on, 0 off (the old straight drive: no offset, no wall, no hits), -1 = tuning Cave.bShaft."));
}

bool DeepShaft::IsOn()
{
	const int32 Value = CVarDeepCaveShaft.GetValueOnAnyThread();
	return Value < 0 ? UDeepTuningSettings::Get().Cave.bShaft : Value != 0;
}

bool DeepShaft::SubBoxM(const ADeepSubmarine* Sub, FVector& OutMinM, FVector& OutMaxM)
{
	const FBox Box = Sub ? Sub->GetSolidBoundsLocal(false) : FBox(ForceInit);
	if (!Box.IsValid)
	{
		// No sub (a bare test): the room's tuned size.
		const FDeepSubTuning& S = UDeepTuningSettings::Get().Sub;
		const float T = S.WallThicknessM;
		OutMinM = FVector(-S.RoomHalfLengthM - T, -S.RoomHalfWidthM - T, -S.RoomHeightM * 0.5f - T);
		OutMaxM = FVector(S.RoomHalfLengthM + T, S.RoomHalfWidthM + T, S.RoomHeightM * 0.5f + T);
		return false;
	}
	OutMinM = Box.Min / UDeepRules::CmPerM;
	OutMaxM = Box.Max / UDeepRules::CmPerM;
	return true;
}

ADeepShaft::ADeepShaft()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
}

float ADeepShaft::DrawRadiusM(float RuleRadiusM, float ReliefM, float ReliefOutM, float ReliefScale)
{
	// The trench relief is "out toward the viewer": on the inside of a tube that is toward the centre.
	return RuleRadiusM + ReliefOutM - ReliefScale * ReliefM;
}

void ADeepShaft::BeginPlay()
{
	Super::BeginPlay();
	Build();
	Follow();
}

void ADeepShaft::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Follow();
}

void ADeepShaft::Build()
{
	const double Start = FPlatformTime::Seconds();
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const FDeepCaveTuning& Cave = Tuning.Cave;
	const FDeepLookTuning& Look = Tuning.Look;
	const TArray<float> Stops = Tuning.StopDepthsM();
	for (UProceduralMeshComponent* Tube : Tubes)
	{
		if (Tube)
		{
			Tube->DestroyComponent();
		}
	}
	Tubes.Reset();
	TubeStretches.Reset();
	Triangles = 0;
	if (Stops.IsEmpty())
	{
		return;
	}

	FVector BoxMin, BoxMax;
	DeepShaft::SubBoxM(DeepArena::FindSubmarine(GetWorld()), BoxMin, BoxMax);
	const float MinR = UDeepCaveRules::MinRadiusM(BoxMin, BoxMax, Cave);
	const int32 Sides = FMath::Clamp(Cave.WallSides, 12, 256);
	const float RowM = FMath::Max(0.25f, Cave.WallRowM);
	const float Tile = FMath::Max(0.25f, Look.SeabedUvTileM);
	const float AroundM = UE_TWO_PI * Cave.StopRadiusM;
	const float Every = Look.TrenchLedgeEveryM;
	const float LedgeOut = Look.TrenchLedgeOutM;

	UMaterialInterface* Material = Cast<UMaterialInterface>(Cave.WallMaterial.TryLoad());
	UMaterialInstanceDynamic* Fallback = nullptr;
	if (!Material)
	{
		if (UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			Fallback = UMaterialInstanceDynamic::Create(Basic, this);
			Fallback->SetVectorParameterValue(TEXT("Color"), Cave.WallColor);
		}
	}

	for (int32 K = 0; K < Stops.Num(); ++K)
	{
		const float Top = Stops[K] + Cave.TubeStartBelowStopM;
		const float Bottom = K + 1 < Stops.Num() ? Stops[K + 1] - Cave.TubeEndAboveStopM : Stops[K] + Cave.BelowLastStopM;
		if (Bottom <= Top + RowM)
		{
			continue;
		}
		const int32 Rows = FMath::CeilToInt((Bottom - Top) / RowM) + 1;
		const int32 Cols = Sides + 1;
		TArray<FVector> Vertices;
		TArray<FVector> Normals;
		TArray<FVector2D> UV0;
		TArray<FProcMeshTangent> Tangents;
		Vertices.Reserve(Rows * Cols);
		UV0.Reserve(Rows * Cols);
		TArray<FVector2D> Centres;
		Centres.Reserve(Rows);
		for (int32 J = 0; J < Rows; ++J)
		{
			const float D = FMath::Min(Bottom, Top + J * RowM);
			const FVector2D C = UDeepCaveRules::CentreM(D, Stops, Cave);
			const float R = UDeepCaveRules::RadiusM(D, Stops, MinR, Cave);
			Centres.Add(C);
			for (int32 I = 0; I < Cols; ++I)
			{
				const float W = float(I) / float(Sides);
				const float Theta = UE_TWO_PI * W;
				const float Along = W * AroundM;
				// The relief wraps round: blended with itself one turn back, so the seam meets.
				const float Relief = FMath::Lerp(ADeepSubmarine::TrenchReliefM(Along, D, Every, LedgeOut), ADeepSubmarine::TrenchReliefM(Along - AroundM, D, Every, LedgeOut), W);
				const float Rd = DrawRadiusM(R, Relief, Cave.WallReliefOutM, Cave.WallReliefScale);
				const FVector2D P = C + FVector2D(FMath::Cos(Theta), FMath::Sin(Theta)) * Rd;
				Vertices.Add(FVector(P.X, P.Y, -D) * UDeepRules::CmPerM);
				UV0.Add(FVector2D(Along, D) / Tile);
			}
		}
		// Normals and tangents from the grid (inward: the rock is seen from inside the tube).
		Normals.SetNum(Vertices.Num());
		Tangents.SetNum(Vertices.Num());
		for (int32 J = 0; J < Rows; ++J)
		{
			const int32 Jm = FMath::Max(0, J - 1);
			const int32 Jp = FMath::Min(Rows - 1, J + 1);
			for (int32 I = 0; I < Cols; ++I)
			{
				const int32 Im = I == 0 ? Sides - 1 : I - 1;
				const int32 Ip = I == Sides ? 1 : I + 1;
				const FVector Around = Vertices[J * Cols + Ip] - Vertices[J * Cols + Im];
				const FVector Down = Vertices[Jp * Cols + I] - Vertices[Jm * Cols + I];
				FVector N = FVector::CrossProduct(Around, Down).GetSafeNormal();
				const FVector P = Vertices[J * Cols + I];
				const FVector Inward = FVector(Centres[J].X * UDeepRules::CmPerM - P.X, Centres[J].Y * UDeepRules::CmPerM - P.Y, 0.0);
				if (FVector::DotProduct(N, Inward) < 0.0)
				{
					N = -N;
				}
				Normals[J * Cols + I] = N;
				Tangents[J * Cols + I] = FProcMeshTangent(Around.GetSafeNormal(), false);
			}
		}
		// Winding: Unreal's face normal is (P1 - P2) x (P0 - P2); pick the order whose face looks inward.
		const FVector P0 = Vertices[0], P1 = Vertices[Cols], P2 = Vertices[1];
		const FVector Face = FVector::CrossProduct(P1 - P2, P0 - P2);
		const bool bFlip = FVector::DotProduct(Face, Normals[0]) < 0.0;
		// In pieces of at most PieceRows rows, each its own component: a whole stretch as one body (36k triangles) did
		// not answer a single trace in this engine (Chaos, 5.9 preview; found by Deep.World.Cave.Tube), the pieces do.
		// Neighbouring pieces share their boundary row, so the rock has no seam.
		constexpr int32 PieceRows = 48;
		for (int32 J0 = 0; J0 < Rows - 1; J0 += PieceRows)
		{
			const int32 J1 = FMath::Min(Rows - 1, J0 + PieceRows);
			const int32 First = J0 * Cols;
			const int32 Count = (J1 - J0 + 1) * Cols;
			const TArray<FVector> PieceVertices(Vertices.GetData() + First, Count);
			const TArray<FVector> PieceNormals(Normals.GetData() + First, Count);
			const TArray<FVector2D> PieceUV0(UV0.GetData() + First, Count);
			const TArray<FProcMeshTangent> PieceTangents(Tangents.GetData() + First, Count);
			TArray<int32> Tris;
			Tris.Reserve((J1 - J0) * Sides * 6);
			for (int32 J = 0; J < J1 - J0; ++J)
			{
				for (int32 I = 0; I < Sides; ++I)
				{
					const int32 V00 = J * Cols + I;
					const int32 V10 = V00 + 1;
					const int32 V01 = V00 + Cols;
					const int32 V11 = V01 + 1;
					if (bFlip)
					{
						Tris.Append({ V00, V10, V01, V10, V11, V01 });
					}
					else
					{
						Tris.Append({ V00, V01, V10, V10, V01, V11 });
					}
				}
			}

			UProceduralMeshComponent* Tube = NewObject<UProceduralMeshComponent>(this, FName(*FString::Printf(TEXT("Tube%d_%d"), K, J0 / PieceRows)));
			Tube->SetMobility(EComponentMobility::Movable);
			Tube->SetupAttachment(Root);
			Tube->bUseComplexAsSimpleCollision = true;
			Tube->SetCollisionProfileName(TEXT("BlockAll"));
			Tube->SetCanEverAffectNavigation(false);
			Tube->SetCastShadow(false);
			Tube->RegisterComponent();
			Tube->CreateMeshSection(0, PieceVertices, Tris, PieceNormals, PieceUV0, TArray<FColor>(), PieceTangents, /*bCreateCollision*/ true);
			if (Material)
			{
				Tube->SetMaterial(0, Material);
			}
			else if (Fallback)
			{
				Tube->SetMaterial(0, Fallback);
			}
			Tubes.Add(Tube);
			TubeStretches.Add(K);
			Triangles += Tris.Num() / 3;
		}
	}
	BuildSeconds = FPlatformTime::Seconds() - Start;
	UE_LOG(LogDeepShaft, Log, TEXT("DEEPSHAFT built %d rock pieces, %d triangles, in %.0f ms (min radius %.1f m)"), Tubes.Num(), Triangles, BuildSeconds * 1000.0, MinR);
}

void ADeepShaft::Follow()
{
	UWorld* World = GetWorld();
	const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
	const ADeepSubmarine* Sub = DeepArena::FindSubmarine(World);
	const bool bOn = DeepShaft::IsOn() && State && Sub;
	const TArray<float> Stops = UDeepTuningSettings::Get().StopDepthsM();
	if (bOn)
	{
		const FVector Side = Sub->GetActorRotation().RotateVector(FVector(State->SubOffsetM.X, State->SubOffsetM.Y, 0.f));
		SetActorLocationAndRotation(Sub->GetActorLocation() + (FVector(0.f, 0.f, State->SubDepthM) - Side) * UDeepRules::CmPerM, Sub->GetActorRotation());
	}
	for (int32 Index = 0; Index < Tubes.Num(); ++Index)
	{
		UProceduralMeshComponent* Tube = Tubes[Index];
		const int32 K = TubeStretches.IsValidIndex(Index) ? TubeStretches[Index] : 0;
		if (!Tube)
		{
			continue;
		}
		// At or above the stop this stretch starts under (that stop loaded): its bed and salvage are where the tube is.
		const bool bAtItsStop = State && State->Stop == K + 1 && Stops.IsValidIndex(K) && State->SubDepthM <= Stops[K] + 0.5f;
		const bool bShow = bOn && !bAtItsStop;
		if (Tube->IsVisible() != bShow)
		{
			Tube->SetVisibility(bShow);
			Tube->SetCollisionEnabled(bShow ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		}
	}
}

// Cheats for frames and checks of the shaft (Deep.Cave.*). Server side, like the other Deep.* cheats.
extern ENGINE_API float GAverageMS;

namespace
{
	/** The narrowest (or widest) point of the first stretch, away from the stops' straight ends. */
	float FindStretchExtremeM(UWorld* World, bool bWidest)
	{
		const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
		const TArray<float> Stops = Tuning.StopDepthsM();
		if (Stops.Num() < 2)
		{
			return Tuning.GetStop(1).SubDepthM;
		}
		FVector BoxMin, BoxMax;
		DeepShaft::SubBoxM(DeepArena::FindSubmarine(World), BoxMin, BoxMax);
		const float MinR = UDeepCaveRules::MinRadiusM(BoxMin, BoxMax, Tuning.Cave);
		const float Top = Stops[0] + Tuning.Cave.StraightBelowStopM + 5.f;
		const float Bottom = Stops[1] - Tuning.Cave.StraightAboveStopM - 5.f;
		float Best = 0.5f * (Stops[0] + Stops[1]);
		float BestR = UDeepCaveRules::RadiusM(Best, Stops, MinR, Tuning.Cave);
		for (float D = Top; D <= Bottom; D += 0.5f)
		{
			const float R = UDeepCaveRules::RadiusM(D, Stops, MinR, Tuning.Cave);
			if (bWidest ? R > BestR : R < BestR)
			{
				Best = D;
				BestR = R;
			}
		}
		UE_LOG(LogDeepShaft, Display, TEXT("DEEPCAVE %s at %.1f m: %.1f m wide"), bWidest ? TEXT("cavern") : TEXT("squeeze"), Best, BestR);
		return Best;
	}

	FAutoConsoleCommandWithWorldAndArgs CaveGotoCommand(
		TEXT("Deep.Cave.Goto"),
		TEXT("Deep.Cave.Goto <squeeze|cavern|metres>: drives at once to the first stretch's narrowest or widest point (or a depth), on the passage's centre."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || !GEngine)
			{
				return;
			}
			const FString Where = Args.Num() > 0 ? Args[0].ToLower() : FString(TEXT("squeeze"));
			const float DepthM = Where == TEXT("squeeze") || Where == TEXT("cavern") ? FindStretchExtremeM(World, Where == TEXT("cavern")) : FCString::Atof(*Where);
			GEngine->Exec(World, *FString::Printf(TEXT("Deep.Drive %.2f now"), DepthM));
		}));

	FAutoConsoleCommandWithWorldAndArgs CaveSteerCommand(
		TEXT("Deep.Cave.Steer"),
		TEXT("Deep.Cave.Steer <forward> <starboard>: holds the helm's sideways stick (-1..1 each) until a key changes it; 0 0 lets go."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ADeepDiver* Diver = World ? DeepStateReport::FindDiver(World) : nullptr;
			UDeepRunComponent* Run = Diver ? Diver->GetRun() : nullptr;
			if (!Run || !Run->IsSeated())
			{
				UE_LOG(LogDeepShaft, Warning, TEXT("Deep.Cave.Steer: nobody at the helm (Deep.Helm 1 or Deep.Cave.Goto first)"));
				return;
			}
			const FVector2D Steer(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.f, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.f);
			Run->SetHelmSteer(Steer.GetClampedToMaxSize(1.f));
		}));

	FAutoConsoleCommandWithWorldAndArgs CaveFrameCommand(
		TEXT("Deep.Cave.Frame"),
		TEXT("Deep.Cave.Frame: logs DEEPFRAME (the average frame time) and the DEEPSTATE line."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UE_LOG(LogDeepShaft, Display, TEXT("DEEPFRAME avg %.2f ms (%.0f fps)"), GAverageMS, GAverageMS > 0.f ? 1000.f / GAverageMS : 0.f);
			if (World)
			{
				DeepStateReport::LogState(World);
			}
		}));
}
