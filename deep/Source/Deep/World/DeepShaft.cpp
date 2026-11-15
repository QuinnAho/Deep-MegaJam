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
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/Crc.h"
#include "Sim/DeepCaveRules.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepArena.h"
#include "World/DeepCookKey.h"
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
	// Live tuning: a changed number in Project Settings > DEEP Tuning > Cave rebuilds the rock (Deep.Cave.AutoRebuild).
	FingerprintCheckS -= DeltaSeconds;
	if (FingerprintCheckS <= 0.f)
	{
		FingerprintCheckS = 0.5f;
		if (DeepShaft::AutoRebuild() && TuningFingerprint() != BuiltFingerprint)
		{
			DeepShaft::RebuildAll(GetWorld());
		}
	}
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
	// The generated cave's rock (Content/Deep/Cave/M_CaveRock), else the old wall material.
	UMaterialInterface* Material = Cave.RockMaterial.IsNull() ? nullptr : Cast<UMaterialInterface>(Cave.RockMaterial.TryLoad());
	if (!Material)
	{
		Material = Cast<UMaterialInterface>(Cave.WallMaterial.TryLoad());
	}
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
			Centres.Add(C);
			for (int32 I = 0; I < Cols; ++I)
			{
				const float W = float(I) / float(Sides);
				// The last column repeats the first exactly, so the seam meets.
				const float Theta = I == Sides ? 0.f : UE_TWO_PI * W;
				const float Along = W * AroundM;
				// The one shape (Sim/DeepCaveRules::WallRadiusM): what is drawn is what Contact hits and the sonar traces.
				const float Rd = UDeepCaveRules::WallRadiusM(D, Theta, Stops, MinR, Cave);
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
		// In pieces of at most PieceRows rows, each its own component with its own cook key (World/DeepCookKey.h): in
		// editor builds every complex-only procedural mesh shares one cached collision mesh unless its simple geometry
		// differs, so whole stretches answered traces with another mesh's rock (found by Deep.World.Cave.Tube).
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

			UProceduralMeshComponent* Tube = NewObject<UProceduralMeshComponent>(this, MakeUniqueObjectName(this, UProceduralMeshComponent::StaticClass(), FName(*FString::Printf(TEXT("Tube%d_%d"), K, J0 / PieceRows))));
			Tube->SetMobility(EComponentMobility::Movable);
			Tube->SetupAttachment(Root);
			Tube->bUseComplexAsSimpleCollision = true;
			Tube->SetCollisionProfileName(TEXT("BlockAll"));
			Tube->SetCanEverAffectNavigation(false);
			Tube->SetCastShadow(false);
			Tube->RegisterComponent();
			// Its own collision cook key (World/DeepCookKey.h), 40 m out in the rock behind its first row.
			const FVector KeyFrom = PieceVertices[0];
			const FVector KeyOut = (KeyFrom - FVector(Centres[J0].X, Centres[J0].Y, 0.0) * UDeepRules::CmPerM).GetSafeNormal2D();
			DeepCookKey::Apply(Tube, KeyFrom + KeyOut * 40.0 * UDeepRules::CmPerM, DeepCookKey::Hash(PieceVertices, Tris, uint32(K)));
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
	const double MeshSeconds = FPlatformTime::Seconds() - Start;
	BuildScatter(Stops, MinR, Material ? Material : static_cast<UMaterialInterface*>(Fallback));
	BuildSeconds = FPlatformTime::Seconds() - Start;
	BuiltFingerprint = TuningFingerprint();
	UE_LOG(LogDeepShaft, Display, TEXT("DEEPSHAFT built %d rock pieces, %d triangles (%d sides, rows %.2f m), %d scattered rocks, in %.0f ms (mesh %.0f ms; min radius %.1f m, rock floor %.1f m, seed %d)"),
		Tubes.Num(), Triangles, Sides, RowM, GetScatterCount(), BuildSeconds * 1000.0, MeshSeconds * 1000.0, MinR, UDeepCaveRules::WallFloorM(MinR, Cave), Cave.RockSeed);
}

int32 ADeepShaft::GetScatterCount() const
{
	int32 Count = 0;
	for (const UInstancedStaticMeshComponent* Mesh : Scatter)
	{
		Count += Mesh ? Mesh->GetInstanceCount() : 0;
	}
	return Count;
}

void ADeepShaft::BuildScatter(const TArray<float>& Stops, float MinRadiusM, UMaterialInterface* Rock)
{
	for (UInstancedStaticMeshComponent* Mesh : Scatter)
	{
		if (Mesh)
		{
			Mesh->DestroyComponent();
		}
	}
	Scatter.Reset();
	ScatterStretches.Reset();
	const FDeepCaveTuning& Cave = UDeepTuningSettings::Get().Cave;
	if (Cave.ScatterPer100M <= 0.f || Stops.IsEmpty())
	{
		return;
	}
	// The Dragon Cave pack's rocks (read-only pack, referenced by path); missing meshes are skipped.
	static const TCHAR* Paths[] = {
		TEXT("/Game/DragonCave/Meshes/SM_CaveRock01.SM_CaveRock01"), TEXT("/Game/DragonCave/Meshes/SM_CaveRock02.SM_CaveRock02"),
		TEXT("/Game/DragonCave/Meshes/SM_CaveRock03.SM_CaveRock03"), TEXT("/Game/DragonCave/Meshes/SM_CaveRock04.SM_CaveRock04"),
		TEXT("/Game/DragonCave/Meshes/SM_LargeRock01.SM_LargeRock01"), TEXT("/Game/DragonCave/Meshes/SM_LargeRock02.SM_LargeRock02"),
		TEXT("/Game/DragonCave/Meshes/SM_LargeRock03.SM_LargeRock03"), TEXT("/Game/DragonCave/Meshes/SM_LargeRock04.SM_LargeRock04"),
		TEXT("/Game/DragonCave/Meshes/SM_LargeRock05.SM_LargeRock05"), TEXT("/Game/DragonCave/Meshes/SM_LargeRock06.SM_LargeRock06"),
		TEXT("/Game/DragonCave/Meshes/SM_LargeRock07.SM_LargeRock07"),
	};
	TArray<UStaticMesh*> Meshes;
	for (const TCHAR* Path : Paths)
	{
		if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Meshes.Add(Mesh);
		}
	}
	if (Meshes.IsEmpty())
	{
		UE_LOG(LogDeepShaft, Display, TEXT("DEEPSHAFT scatter: no Dragon Cave rock meshes, none placed"));
		return;
	}
	const float Floor = UDeepCaveRules::WallFloorM(MinRadiusM, Cave);

	for (int32 K = 0; K < Stops.Num(); ++K)
	{
		const float Top = Stops[K] + Cave.TubeStartBelowStopM + 3.f;
		const float Bottom = K + 1 < Stops.Num() ? Stops[K + 1] - Cave.TubeEndAboveStopM - 3.f : Stops[K] + Cave.BelowLastStopM - 3.f;
		if (Bottom <= Top)
		{
			continue;
		}
		TArray<UInstancedStaticMeshComponent*> ByMesh;
		ByMesh.SetNumZeroed(Meshes.Num());
		const int32 Count = FMath::RoundToInt((Bottom - Top) * Cave.ScatterPer100M / 100.f);
		for (int32 N = 0; N < Count; ++N)
		{
			// Seeded per stretch and rock by the cave's own hash (UDeepCaveRules::Noise3 lattice hash via cheap draws).
			auto Draw = [&](int32 Channel) { return 0.5f + 0.5f * UDeepCaveRules::Noise3(FVector(N * 0.618034 + 0.31, K * 7.123 + Channel * 3.71 + 0.17, Channel * 0.913 + 0.53), Cave.RockSeed + 4242); };
			const float D = FMath::Lerp(Top, Bottom, (N + Draw(0)) / float(Count));
			const float Theta = UE_TWO_PI * FMath::Frac(Draw(1) * 3.17f + N * 0.381966f);
			const float Size = FMath::Lerp(Cave.ScatterSizeM.X, Cave.ScatterSizeM.Y, FMath::Square(Draw(2)));
			const int32 Pick = FMath::Clamp(int32(FMath::Frac(Draw(3) * 5.3f + N * 0.7548f) * Meshes.Num()), 0, Meshes.Num() - 1);
			UStaticMesh* Mesh = Meshes[Pick];
			const FBoxSphereBounds Bounds = Mesh->GetBounds();
			const float MeshSizeCm = FMath::Max(1.f, float(Bounds.BoxExtent.GetMax()) * 2.f);
			const float ScaleUniform = Size * UDeepRules::CmPerM / MeshSizeCm;
			const FVector Scale3 = FVector(ScaleUniform) * FVector(FMath::Lerp(0.8f, 1.25f, Draw(7)), FMath::Lerp(0.8f, 1.25f, Draw(8)), FMath::Lerp(0.7f, 1.1f, Draw(9)));
			// The rock's reach from its pivot (m), at its largest axis scale, whatever the rotation.
			const float ReachM = float(Bounds.SphereRadius + Bounds.Origin.Size()) * ScaleUniform * 1.25f / UDeepRules::CmPerM;
			// The wall's deepest point over the rock's span (angle and depth): the rock stands at most ScatterProtrudeM in
			// from there, so wherever it shows, the wall's own line (where Contact stops the padded box) is within that.
			const FVector2D C = UDeepCaveRules::CentreM(D, Stops, Cave);
			const float Wall0 = UDeepCaveRules::WallRadiusM(D, Theta, Stops, MinRadiusM, Cave);
			const float SpanRad = ReachM / FMath::Max(1.f, Wall0);
			float WallMax = Wall0;
			for (int32 A = -2; A <= 2; ++A)
			{
				for (int32 B = -1; B <= 1; ++B)
				{
					WallMax = FMath::Max(WallMax, UDeepCaveRules::WallRadiusM(D + B * ReachM, Theta + A * 0.5f * SpanRad, Stops, MinRadiusM, Cave));
				}
			}
			// How far the rock's box reaches toward the passage's centre from its pivot (m), as rotated and scaled.
			const FRotator Rot(Draw(4) * 360.f - 180.f, Draw(5) * 360.f, Draw(6) * 360.f - 180.f);
			const FVector Inward(-FMath::Cos(Theta), -FMath::Sin(Theta), 0.f);
			const FBox LocalBox = Mesh->GetBoundingBox();
			float InM = 0.f;
			for (int32 Corner = 0; Corner < 8; ++Corner)
			{
				const FVector Pt((Corner & 1) ? LocalBox.Max.X : LocalBox.Min.X, (Corner & 2) ? LocalBox.Max.Y : LocalBox.Min.Y, (Corner & 4) ? LocalBox.Max.Z : LocalBox.Min.Z);
				InM = FMath::Max(InM, float(FVector::DotProduct(Rot.RotateVector(Pt * Scale3), Inward)) / UDeepRules::CmPerM);
			}
			const float Centre = FMath::Max(WallMax - FMath::Clamp(Cave.ScatterProtrudeM, 0.f, Cave.HullPadM) + InM, Floor + InM);
			// One that would barely show (its inner face behind the wall here) is left out.
			if (Centre - InM > Wall0 - 0.1f)
			{
				continue;
			}
			const FVector2D P = C + FVector2D(FMath::Cos(Theta), FMath::Sin(Theta)) * Centre;
			const FTransform Xf(Rot, FVector(P.X, P.Y, -D) * UDeepRules::CmPerM, Scale3);
			if (!ByMesh[Pick])
			{
				UInstancedStaticMeshComponent* H = NewObject<UInstancedStaticMeshComponent>(this, MakeUniqueObjectName(this, UInstancedStaticMeshComponent::StaticClass(), FName(*FString::Printf(TEXT("Scatter%d_%d"), K, Pick))));
				H->SetMobility(EComponentMobility::Movable);
				H->SetupAttachment(Root);
				H->SetStaticMesh(Mesh);
				H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				H->SetCanEverAffectNavigation(false);
				H->SetCastShadow(false);
				H->RegisterComponent();
				if (Rock)
				{
					for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
					{
						H->SetMaterial(Slot, Rock);
					}
				}
				ByMesh[Pick] = H;
				Scatter.Add(H);
				ScatterStretches.Add(K);
			}
			ByMesh[Pick]->AddInstance(Xf, /*bWorldSpace*/ false);
		}
	}
}

void ADeepShaft::Rebuild()
{
	Build();
	// Force the show/hide pass to set every new piece.
	for (UProceduralMeshComponent* Tube : Tubes)
	{
		if (Tube)
		{
			Tube->SetVisibility(false);
			Tube->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	Follow();
}

uint32 ADeepShaft::TuningFingerprint()
{
	const FDeepCaveTuning& C = UDeepTuningSettings::Get().Cave;
	const float Numbers[] = {
		C.StopRadiusM, C.StraightBelowStopM, C.StraightAboveStopM, float(C.BendAmplitudeM.X), float(C.BendAmplitudeM.Y),
		float(C.BendWavelengthM.X), float(C.BendWavelengthM.Y), C.CavernRadiusM, C.SqueezeMarginM, C.RadiusWavelengthM, float(C.Seed),
		C.BelowLastStopM, C.TubeStartBelowStopM, C.TubeEndAboveStopM, C.HullPadM, C.WallRowM, float(C.WallSides),
		C.bRock ? 1.f : 0.f, float(C.RockSeed), float(C.RockOctaves), C.RockAmplitudeM, C.RockScaleM, C.RockLacunarity, C.RockGain,
		C.RockRidge, C.RockStretchZ, C.StrataSpacingM, C.StrataDepthM, C.StrataBreakup, C.MouthCalm01, C.RockFloorMarginM,
		C.ScatterPer100M, float(C.ScatterSizeM.X), float(C.ScatterSizeM.Y), C.ScatterProtrudeM, C.ContactStepM,
	};
	uint32 Hash = FCrc::MemCrc32(Numbers, sizeof(Numbers));
	Hash = HashCombine(Hash, GetTypeHash(C.RockMaterial.ToString()));
	return Hash;
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
		// Turned with the arena root about the vertical through the sub by the helm's heading (same as
		// UDeepPilotRules::HangLocationCm; exactly the old placement at yaw 0).
		const FVector Side = Sub->GetActorRotation().RotateVector(FVector(State->SubOffsetM.X, State->SubOffsetM.Y, 0.f));
		const FQuat Turn = State->SubYawDeg == 0.f ? FQuat::Identity : FQuat(FVector::UpVector, FMath::DegreesToRadians(-State->SubYawDeg));
		const FVector At = Sub->GetActorLocation() + Turn.RotateVector((FVector(0.f, 0.f, State->SubDepthM) - Side) * UDeepRules::CmPerM);
		SetActorLocationAndRotation(At, Turn * Sub->GetActorQuat());
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
	// The scattered rocks go with their stretch's rock.
	for (int32 Index = 0; Index < Scatter.Num(); ++Index)
	{
		UInstancedStaticMeshComponent* Mesh = Scatter[Index];
		const int32 K = ScatterStretches.IsValidIndex(Index) ? ScatterStretches[Index] : 0;
		if (!Mesh)
		{
			continue;
		}
		const bool bAtItsStop = State && State->Stop == K + 1 && Stops.IsValidIndex(K) && State->SubDepthM <= Stops[K] + 0.5f;
		const bool bShow = bOn && !bAtItsStop;
		if (Mesh->IsVisible() != bShow)
		{
			Mesh->SetVisibility(bShow);
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
