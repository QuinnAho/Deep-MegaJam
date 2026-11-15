#include "World/DeepCavern.h"

#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameState.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepAbyssFloor.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepCave, Log, All);

// 0 = today's open arena (no walls, no roof, no cave_ dressing rows): a look and frame-time A/B in one run.
static TAutoConsoleVariable<int32> CVarDeepCaveCavern(TEXT("Deep.Cave.Cavern"), 1,
	TEXT("1 = rock walls and a roof round each stop (ADeepCavern); 0 = the open arena, as before the cave."));

namespace
{
	FVector Direction(double Theta, double Phi)
	{
		const double C = FMath::Cos(Phi);
		return FVector(C * FMath::Cos(Theta), C * FMath::Sin(Theta), FMath::Sin(Phi));
	}

	/** The base shape along elevation Phi (rad): the sphere below the anchor's level, the flat-topped vault above. */
	double BaseDistanceM(double Phi, double BaseM, const FDeepCaveTuning& Cave)
	{
		if (Phi <= 0.0)
		{
			return BaseM;
		}
		const double N = FMath::Max(2.0, double(Cave.RoofPower));
		const double Roof = FMath::Max(1.0, double(Cave.RoofAboveAnchorM));
		const double C = FMath::Cos(Phi) / BaseM;
		const double S = FMath::Sin(Phi) / Roof;
		return 1.0 / FMath::Pow(FMath::Pow(C, N) + FMath::Pow(S, N), 1.0 / N);
	}

	/** A fixed offset into the noise field per seed and layer, so each layer is its own pattern. */
	FVector NoiseOffset(int32 Seed, int32 Layer)
	{
		FRandomStream Rng(Seed * 31 + Layer * 7919);
		return FVector(Rng.FRandRange(-400.f, 400.f), Rng.FRandRange(-400.f, 400.f), Rng.FRandRange(-400.f, 400.f));
	}

	/** Elevation where the rock along azimuth Theta is OutM from the axis (rad), searched over [0, 89.5 deg]. */
	double ElevationAtOutM(double Theta, double OutM, double BaseM, const FDeepCaveTuning& Cave)
	{
		double Lo = 0.0;
		double Hi = FMath::DegreesToRadians(89.5);
		for (int32 Step = 0; Step < 40; ++Step)
		{
			const double Mid = 0.5 * (Lo + Hi);
			const double Out = ADeepCavern::SurfaceDistanceM(Direction(Theta, Mid), BaseM, Cave) * FMath::Cos(Mid);
			(Out > OutM ? Lo : Hi) = Mid;
		}
		return 0.5 * (Lo + Hi);
	}

	void AddSectionTo(UProceduralMeshComponent* Mesh, int32 Index, const FDeepCaveSection& Section)
	{
		if (Section.Triangles.IsEmpty())
		{
			return;
		}
		TArray<FVector> Vertices;
		Vertices.Reserve(Section.VerticesM.Num());
		for (const FVector& V : Section.VerticesM)
		{
			Vertices.Add(V * UDeepRules::CmPerM);
		}
		TArray<FProcMeshTangent> Tangents;
		Tangents.Reserve(Section.TangentsX.Num());
		for (const FVector& T : Section.TangentsX)
		{
			Tangents.Add(FProcMeshTangent(T, false));
		}
		Mesh->CreateMeshSection(Index, Vertices, Section.Triangles, Section.Normals, Section.UV0, TArray<FColor>(), Tangents, /*bCreateCollision*/ true);
	}

	/**
	 * A 1 cm tetrahedron off in the rock behind the wall, placed by a hash of the mesh. UE 5.9 Preview's
	 * UBodySetup::InvalidatePhysicsData derives the collision cook's key from the simple geometry only, and a procedural
	 * mesh has no mesh id, so in an editor build every large complex-only procedural mesh (this cave, the sea bed) shares
	 * one derived-data entry: stop 2's shell came back with stop 1's rock. The convex element is part of that key, so
	 * each shell gets its own entry that changes with its shape; with complex-as-simple collision it makes no shape
	 * (ChaosInterfaceUtils: simple geometry is skipped when a triangle mesh exists).
	 */
	TArray<FVector> CookKeyTetraCm(const FDeepCaveMesh& Mesh)
	{
		uint32 Hash = 0;
		for (const FDeepCaveSection* Section : { &Mesh.Shell, &Mesh.Hangs })
		{
			Hash = FCrc::MemCrc32(Section->VerticesM.GetData(), Section->VerticesM.Num() * sizeof(FVector), Hash);
			Hash = FCrc::MemCrc32(Section->Triangles.GetData(), Section->Triangles.Num() * sizeof(int32), Hash);
		}
		const FVector Corner = FVector((Mesh.BaseRadiusM + 40.0) * UDeepRules::CmPerM, 0.0, 0.0)
			+ FVector(double(Hash & 0xFFFF), double(Hash >> 16), 0.0) * 1e-4;
		return { Corner, Corner + FVector(1.0, 0.0, 0.0), Corner + FVector(0.0, 1.0, 0.0), Corner + FVector(0.0, 0.0, 1.0) };
	}

	UMaterialInterface* RockMaterial(UObject* Outer)
	{
		const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
		const FSoftObjectPath& Path = Tuning.Cave.Material.IsNull() ? Tuning.Look.SeabedMaterial : Tuning.Cave.Material;
		if (UMaterialInterface* Found = Path.IsNull() ? nullptr : Cast<UMaterialInterface>(Path.TryLoad()))
		{
			return Found;
		}
		UE_LOG(LogDeepCave, Warning, TEXT("DEEPCAVE material missing (%s): tinted fallback"), *Path.ToString());
		if (UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			UMaterialInstanceDynamic* Tinted = UMaterialInstanceDynamic::Create(Basic, Outer);
			Tinted->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.035f, 0.032f, 0.028f));
			return Tinted;
		}
		return nullptr;
	}
}

ADeepCavern::ADeepCavern()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
}

bool ADeepCavern::IsOn()
{
	return UDeepTuningSettings::Get().Cave.bEnabled && CVarDeepCaveCavern.GetValueOnGameThread() != 0;
}

float ADeepCavern::NeedRadiusM(int32 Stop, const UDeepTuningSettings& Tuning)
{
	float FarthestSpotM = 0.f;
	for (const FDeepSalvageSpot& Spot : Tuning.Arena.SalvageSpots)
	{
		FarthestSpotM = FMath::Max(FarthestSpotM, float(UDeepRules::SalvageSpotOffsetCm(Spot).Size() / UDeepRules::CmPerM));
	}
	const float TetherM = UDeepRules::TetherLengthM(Stop, Tuning.Tether);
	return FMath::Max(TetherM + Tuning.Cave.TetherMarginM, FarthestSpotM + Tuning.Cave.SpotMarginM);
}

float ADeepCavern::BaseRadiusM(float NeedM, const FDeepCaveTuning& Cave)
{
	// On the vault (x/B)^n + (z/Z)^n = 1, the rock at height h stands B * (1 - (h/Z)^n)^(1/n) out: pick B so that at the
	// clear height it is still NeedM.
	const double N = FMath::Max(2.0, double(Cave.RoofPower));
	const double Ratio = FMath::Clamp(double(Cave.ClearAboveAnchorM) / FMath::Max(1.0, double(Cave.RoofAboveAnchorM)), 0.0, 0.95);
	return float(NeedM / FMath::Pow(1.0 - FMath::Pow(Ratio, N), 1.0 / N));
}

float ADeepCavern::SurfaceDistanceM(const FVector& Dir, float BaseM, const FDeepCaveTuning& Cave)
{
	const double Phi = FMath::Asin(FMath::Clamp(Dir.Z, -1.0, 1.0));
	const double Theta = FMath::Atan2(Dir.Y, Dir.X);
	const double Base = BaseDistanceM(Phi, BaseM, Cave);
	// Noise is sampled at the base point itself, so its scale is true metres on the roof and the walls alike.
	const FVector P = Dir * Base;
	const double Scale = FMath::Max(1.0, double(Cave.ReliefScaleM));
	const double N1 = FMath::PerlinNoise3D(P / Scale + NoiseOffset(Cave.Seed, 1));
	const double N2 = FMath::PerlinNoise3D(P / (Scale * 0.37) + NoiseOffset(Cave.Seed, 2));
	const double N3 = FMath::PerlinNoise3D(P / (Scale * 0.6) + NoiseOffset(Cave.Seed, 3));
	// Weights: the wall band (strongest at the anchor's level, gone by about 40 deg up or down) and the roof.
	const double Wall = FMath::Clamp(1.0 - FMath::Abs(Dir.Z) * 1.6, 0.0, 1.0);
	const double Up = FMath::Clamp(Dir.Z * 4.0, 0.0, 1.0);

	const double Lump = Cave.ReliefM * FMath::Clamp(0.5 + 0.65 * (N1 + 0.45 * N2), 0.0, 1.0) * (0.35 + 0.65 * Wall);
	// Bays: an integer count round the cave, so the pattern closes on itself; warped so they are not evenly spaced.
	const int32 K = FMath::Max(0, Cave.AlcoveCount);
	const double Phase = 0.37 * Cave.Seed;
	const double Bay = K > 0 ? FMath::Pow(FMath::Max(0.0, FMath::Sin(K * Theta + 0.9 * FMath::Sin(2.0 * Theta + Phase) + Phase)), 3.0) : 0.0;
	const double Alcove = Cave.AlcoveDepthM * Bay * Wall * (0.6 + 0.4 * FMath::Clamp(0.5 + N3, 0.0, 1.0));
	const double Strata = Cave.StrataEveryM > 0.f
		? Cave.StrataM * (0.5 + 0.5 * FMath::Sin(2.0 * PI * P.Z / Cave.StrataEveryM + 1.5 * N1)) * Wall
		: 0.0;
	const double Roof = Cave.RoofReliefM * FMath::Clamp(0.5 + 0.7 * N3, 0.0, 1.0) * Up;
	// Every term is >= 0: rock only ever moves away from the anchor, so the clear space is never entered.
	return float(Base + Lump + Alcove + Strata + Roof);
}

FDeepCaveMesh ADeepCavern::MakeMesh(int32 Stop, const TArray<FVector4>& RestM, const UDeepTuningSettings& Tuning)
{
	const FDeepCaveTuning& Cave = Tuning.Cave;
	FDeepCaveMesh Mesh;
	Mesh.NeedRadiusM = NeedRadiusM(Stop, Tuning);
	Mesh.BaseRadiusM = BaseRadiusM(Mesh.NeedRadiusM, Cave);
	const double BaseM = Mesh.BaseRadiusM;
	const int32 C = FMath::Clamp(Cave.Columns, 16, 2048);
	const int32 Body = FMath::Clamp(Cave.Rows, 8, 2048);
	const int32 Neck = Cave.NeckHeightM > 0.f ? 4 : 0;
	const int32 Rows = Neck + 1 + Body;
	const int32 Stride = C + 1;
	Mesh.Columns = C;
	Mesh.Rows = Rows;
	Mesh.RimRow = Neck;
	const double HoleM = FMath::Max(1.0, double(Cave.HoleRadiusM));

	FDeepCaveSection& S = Mesh.Shell;
	S.VerticesM.SetNumZeroed(Rows * Stride);

	for (int32 i = 0; i < C; ++i)
	{
		const double Theta = 2.0 * PI * i / C;
		const FVector2D Out(FMath::Cos(Theta), FMath::Sin(Theta));

		// The rim: where the vault is HoleM from the axis.
		const double PhiRim = ElevationAtOutM(Theta, HoleM, BaseM, Cave);
		const double RimZ = SurfaceDistanceM(Direction(Theta, PhiRim), BaseM, Cave) * FMath::Sin(PhiRim);

		// The foot: march down from the anchor's level until the rock is SkirtBelowBedM under the silt, then refine.
		auto Below = [&](double Phi)
		{
			const FVector P = Direction(Theta, Phi) * SurfaceDistanceM(Direction(Theta, Phi), BaseM, Cave);
			return -ADeepAbyssFloor::SeabedBelowM(FVector2D(P.X, P.Y), RestM, Tuning.Look) - Cave.SkirtBelowBedM - P.Z;
		};
		const double StepRad = FMath::DegreesToRadians(0.5);
		const double MinPhi = FMath::DegreesToRadians(-88.0);
		double Hi = 0.0;
		double Lo = MinPhi;
		for (double Phi = 0.0; Phi > MinPhi; Phi -= StepRad)
		{
			if (Below(Phi) > 0.0)
			{
				Lo = Phi;
				break;
			}
			Hi = Phi;
		}
		for (int32 Step = 0; Step < 24 && Lo > MinPhi; ++Step)
		{
			const double Mid = 0.5 * (Lo + Hi);
			(Below(Mid) > 0.0 ? Lo : Hi) = Mid;
		}
		const double PhiFoot = Lo;

		for (int32 n = 0; n < Neck; ++n)
		{
			const double T = double(Neck - n) / Neck;
			const double R = HoleM + Cave.NeckFlareM * T;
			S.VerticesM[n * Stride + i] = FVector(Out.X * R, Out.Y * R, RimZ + Cave.NeckHeightM * T);
		}
		// Exactly HoleM from the axis: the opening is round and centred, whatever the relief does.
		S.VerticesM[Neck * Stride + i] = FVector(Out.X * HoleM, Out.Y * HoleM, RimZ);
		for (int32 k = 1; k <= Body; ++k)
		{
			const double Phi = FMath::Lerp(PhiRim, PhiFoot, double(k) / Body);
			const FVector Dir = Direction(Theta, Phi);
			S.VerticesM[(Neck + k) * Stride + i] = Dir * SurfaceDistanceM(Dir, BaseM, Cave);
		}
	}
	for (int32 j = 0; j < Rows; ++j)
	{
		S.VerticesM[j * Stride + C] = S.VerticesM[j * Stride];
	}

	// Normals and tangents from the grid; UVs: U round the cave (a whole number of tiles, so the seam matches), V down it.
	const double Tile = FMath::Max(0.1f, Cave.UvTileM);
	const double UTiles = FMath::Max(1.0, FMath::RoundToDouble(2.0 * PI * BaseM / Tile));
	S.Normals.SetNumZeroed(Rows * Stride);
	S.TangentsX.SetNumZeroed(Rows * Stride);
	S.UV0.SetNumZeroed(Rows * Stride);
	auto At = [&](int32 j, int32 i) -> const FVector& { return S.VerticesM[j * Stride + i]; };
	for (int32 i = 0; i <= C; ++i)
	{
		double V = 0.0;
		for (int32 j = 0; j < Rows; ++j)
		{
			if (j > 0)
			{
				V += FVector::Dist(At(j, i), At(j - 1, i)) / Tile;
			}
			const int32 Prev = i == 0 ? C - 1 : i - 1;
			const int32 Next = i == C ? 1 : i + 1;
			const FVector Round = At(j, Next) - At(j, Prev);
			const FVector Down = At(FMath::Min(j + 1, Rows - 1), i) - At(FMath::Max(j - 1, 0), i);
			// Faces the inside: (next column) x (next row), the winding below.
			S.Normals[j * Stride + i] = (Round ^ Down).GetSafeNormal();
			S.TangentsX[j * Stride + i] = Round.GetSafeNormal();
			S.UV0[j * Stride + i] = FVector2D(UTiles * i / C, V);
		}
	}
	S.Triangles.Reserve((Rows - 1) * C * 6);
	for (int32 j = 0; j + 1 < Rows; ++j)
	{
		for (int32 i = 0; i < C; ++i)
		{
			const int32 V00 = j * Stride + i;
			const int32 V10 = V00 + 1;
			const int32 V01 = V00 + Stride;
			const int32 V11 = V01 + 1;
			// Unreal's face normal is (P1 - P2) x (P0 - P2): this winding faces (next column) x (next row), into the cave.
			S.Triangles.Append({ V00, V01, V10, V10, V01, V11 });
		}
	}

	// Spurs hanging from the roof: tapered, leaning, rough. Base a metre into the rock, tip above the clear space.
	FDeepCaveSection& H = Mesh.Hangs;
	FRandomStream Rng(Cave.Seed + Stop * 101);
	const double ClearTopM = Cave.ClearAboveAnchorM + Cave.HangTipClearM;
	const int32 Sides = 10;
	const double RingT[] = { 0.0, 0.45, 0.8 };
	const double RingR[] = { 1.0, 0.6, 0.28 };
	for (int32 Index = 0; Index < FMath::Max(0, Cave.HangCount); ++Index)
	{
		const double Theta = Rng.FRandRange(0.f, 2.f * PI);
		const double MaxOut = FMath::Max(double(Cave.HangMinOutM) + 1.0, 0.85 * Mesh.NeedRadiusM);
		const double OutM = FMath::Sqrt(FMath::Lerp(FMath::Square(double(Cave.HangMinOutM)), FMath::Square(MaxOut), double(Rng.FRand())));
		const double Radius = Rng.FRandRange(Cave.HangRadiusMinM, Cave.HangRadiusMaxM);
		const double TipFrac = Rng.FRandRange(0.f, 0.6f);
		const FVector2D Lean(Rng.FRandRange(-0.3f, 0.3f) * Radius, Rng.FRandRange(-0.3f, 0.3f) * Radius);
		const int32 JitterSeed = Rng.RandHelper(1 << 20);
		if (OutM - 1.6 * Radius < HoleM + 1.0)
		{
			continue;
		}
		const double Phi = ElevationAtOutM(Theta, OutM, BaseM, Cave);
		const double RoofZ = SurfaceDistanceM(Direction(Theta, Phi), BaseM, Cave) * FMath::Sin(Phi);
		const double TipZ = FMath::Max(ClearTopM, FMath::Lerp(ClearTopM, RoofZ - 1.5, TipFrac));
		const double TopZ = RoofZ + 1.0;
		if (TopZ - TipZ < 2.5)
		{
			continue;
		}
		const FVector2D Centre(FMath::Cos(Theta) * OutM, FMath::Sin(Theta) * OutM);
		FRandomStream Jitter(JitterSeed);
		const int32 First = H.VerticesM.Num();
		const double Slope = Radius / (TopZ - TipZ);
		for (int32 Ring = 0; Ring < 3; ++Ring)
		{
			const double Z = FMath::Lerp(TopZ, TipZ, RingT[Ring]);
			const FVector2D Mid = Centre + Lean * RingT[Ring];
			for (int32 Side = 0; Side <= Sides; ++Side)
			{
				const double A = 2.0 * PI * (Side % Sides) / Sides;
				// The seam vertex repeats the first, jitter included.
				FRandomStream SideJitter(JitterSeed + Ring * 37 + (Side % Sides));
				const double R = Radius * RingR[Ring] * SideJitter.FRandRange(0.78f, 1.22f);
				const FVector Radial(FMath::Cos(A), FMath::Sin(A), 0.0);
				H.VerticesM.Add(FVector(Mid.X + Radial.X * R, Mid.Y + Radial.Y * R, Z));
				H.Normals.Add((Radial - FVector::UpVector * Slope).GetSafeNormal());
				H.TangentsX.Add(FVector(-Radial.Y, Radial.X, 0.0));
				H.UV0.Add(FVector2D(double(Side) / Sides * 2.0, Z / Tile));
			}
		}
		const int32 Tip = H.VerticesM.Num();
		H.VerticesM.Add(FVector(Centre.X + Lean.X, Centre.Y + Lean.Y, TipZ));
		H.Normals.Add(-FVector::UpVector);
		H.TangentsX.Add(FVector::ForwardVector);
		H.UV0.Add(FVector2D(1.0, TipZ / Tile));
		const int32 RingStride = Sides + 1;
		for (int32 Ring = 0; Ring + 1 < 3; ++Ring)
		{
			for (int32 Side = 0; Side < Sides; ++Side)
			{
				const int32 V00 = First + Ring * RingStride + Side;
				const int32 V10 = V00 + 1;
				const int32 V01 = V00 + RingStride;
				const int32 V11 = V01 + 1;
				// Faces out: (next ring down) x (next side round).
				H.Triangles.Append({ V00, V10, V01, V01, V10, V11 });
			}
		}
		for (int32 Side = 0; Side < Sides; ++Side)
		{
			const int32 V0 = First + 2 * RingStride + Side;
			H.Triangles.Append({ V0, V0 + 1, Tip });
		}
	}
	return Mesh;
}

void ADeepCavern::BeginPlay()
{
	Super::BeginPlay();
	Build(/*bAsyncCook*/ true);
}

void ADeepCavern::Build(bool bAsyncCook)
{
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	for (UProceduralMeshComponent* Shell : Shells)
	{
		if (Shell)
		{
			Shell->DestroyComponent();
		}
	}
	Shells.Reset();

	const TArray<FVector4> RestM = ADeepAbyssFloor::RestPointsFromTuning();
	UMaterialInterface* Rock = RockMaterial(this);
	const int32 StopCount = FMath::Max(1, Tuning.NumStops());
	for (int32 Stop = 1; Stop <= StopCount; ++Stop)
	{
		const double Start = FPlatformTime::Seconds();
		const FDeepCaveMesh Mesh = MakeMesh(Stop, RestM, Tuning);
		UProceduralMeshComponent* Shell = NewObject<UProceduralMeshComponent>(this, FName(*FString::Printf(TEXT("CaveShell%d"), Stop)));
		Shell->SetMobility(EComponentMobility::Movable);
		Shell->bUseAsyncCooking = bAsyncCook;
		Shell->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Shell->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Shell->SetCanEverAffectNavigation(false);
		Shell->SetupAttachment(GetRootComponent());
		Shell->RegisterComponent();
		AddInstanceComponent(Shell);
		Shell->SetCollisionConvexMeshes({ CookKeyTetraCm(Mesh) });
		AddSectionTo(Shell, 0, Mesh.Shell);
		AddSectionTo(Shell, 1, Mesh.Hangs);
		for (int32 Section = 0; Section < Shell->GetNumSections(); ++Section)
		{
			Shell->SetMaterial(Section, Rock);
		}
		Shell->SetCastShadow(true);
		Shells.Add(Shell);

		float RimLo = TNumericLimits<float>::Max();
		float RimHi = TNumericLimits<float>::Lowest();
		float FootLo = TNumericLimits<float>::Max();
		float FootHi = TNumericLimits<float>::Lowest();
		const int32 Stride = Mesh.Columns + 1;
		for (int32 i = 0; i < Mesh.Columns; ++i)
		{
			const FVector& Rim = Mesh.Shell.VerticesM[Mesh.RimRow * Stride + i];
			const FVector& Foot = Mesh.Shell.VerticesM[(Mesh.Rows - 1) * Stride + i];
			RimLo = FMath::Min(RimLo, float(Rim.Z));
			RimHi = FMath::Max(RimHi, float(Rim.Z));
			FootLo = FMath::Min(FootLo, float(FVector2D(Foot.X, Foot.Y).Size()));
			FootHi = FMath::Max(FootHi, float(FVector2D(Foot.X, Foot.Y).Size()));
		}
		UE_LOG(LogDeepCave, Display, TEXT("DEEPCAVE stop %d: rock past %.1f m (base %.1f m), opening %.1f m with rim %.1f..%.1f m over the anchor, wall foot %.1f..%.1f m out, %d + %d vertices, %d spur triangles, %.1f ms"),
			Stop, Mesh.NeedRadiusM, Mesh.BaseRadiusM, Tuning.Cave.HoleRadiusM, RimLo, RimHi, FootLo, FootHi,
			Mesh.Shell.VerticesM.Num(), Mesh.Hangs.VerticesM.Num(), Mesh.Hangs.Triangles.Num() / 3, (FPlatformTime::Seconds() - Start) * 1000.0);
	}

	const UWorld* World = GetWorld();
	const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
	UpdateShells(State ? State->Stop : 1, false);
}

UProceduralMeshComponent* ADeepCavern::GetShell(int32 Stop) const
{
	return Shells.IsValidIndex(Stop - 1) ? Shells[Stop - 1].Get() : nullptr;
}

void ADeepCavern::UpdateShells(int32 Stop, bool bBedHidden)
{
	const bool bOn = IsOn();
	const int32 Shown = FMath::Clamp(Stop, 1, FMath::Max(1, Shells.Num())) - 1;
	for (int32 Index = 0; Index < Shells.Num(); ++Index)
	{
		UProceduralMeshComponent* Shell = Shells[Index];
		if (!Shell)
		{
			continue;
		}
		// Hidden rock must not block: the sonar and the lamp trace Visibility, and a diver must never meet a wall they cannot see.
		const bool bShow = bOn && !bBedHidden && Index == Shown;
		if (Shell->IsVisible() != bShow)
		{
			Shell->SetVisibility(bShow);
		}
		const ECollisionEnabled::Type Want = bShow ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision;
		if (Shell->GetCollisionEnabled() != Want)
		{
			Shell->SetCollisionEnabled(Want);
		}
	}
}

void ADeepCavern::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
	// The cave goes with the stop's bed: the sub hides the bed while it is below the loaded stop (TickHelmLook).
	bool bBedHidden = false;
	for (TActorIterator<ADeepAbyssFloor> It(World); It; ++It)
	{
		bBedHidden = It->IsHidden();
		break;
	}
	UpdateShells(State ? State->Stop : 1, bBedHidden);
}
