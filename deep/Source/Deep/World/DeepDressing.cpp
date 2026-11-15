#include "World/DeepDressing.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/DeepGameState.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "ProceduralMeshComponent.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepCaptainLines.h"
#include "World/DeepAbyssFloor.h"
#include "World/DeepArena.h"
#include "World/DeepArenaRoot.h"
#include "World/DeepSubmarine.h"

#if WITH_EDITOR
#include "GameDelegates.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogDeepDressing, Log, All);

namespace
{
	float Num(const TMap<FString, FString>& Row, const TCHAR* Key, float Default)
	{
		const FString* Cell = Row.Find(Key);
		if (!Cell)
		{
			return Default;
		}
		const FString T = Cell->TrimStartAndEnd();
		return T.IsEmpty() ? Default : FCString::Atof(*T);
	}

	FString Str(const TMap<FString, FString>& Row, const TCHAR* Key)
	{
		const FString* Cell = Row.Find(Key);
		return Cell ? Cell->TrimStartAndEnd() : FString();
	}

	/** The lowest and highest Z of a box after rotation and scale (cm, relative to the pivot). */
	void RotatedZRange(const FBox& Box, const FQuat& Rot, float Scale, double& OutMin, double& OutMax)
	{
		OutMin = TNumericLimits<double>::Max();
		OutMax = -TNumericLimits<double>::Max();
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector P((Corner & 1) ? Box.Max.X : Box.Min.X, (Corner & 2) ? Box.Max.Y : Box.Min.Y, (Corner & 4) ? Box.Max.Z : Box.Min.Z);
			const double Z = Rot.RotateVector(P * Scale).Z;
			OutMin = FMath::Min(OutMin, Z);
			OutMax = FMath::Max(OutMax, Z);
		}
	}

	FString NormalisePackage(const FString& Path)
	{
		FString P = Path.TrimStartAndEnd().TrimQuotes();
		if (!P.StartsWith(TEXT("/")) || P.StartsWith(TEXT("/Engine/")) || P.StartsWith(TEXT("/Script/")))
		{
			return FString();
		}
		if (P.Contains(TEXT(".")))
		{
			P = FPackageName::ObjectPathToPackageName(P);
		}
		return P;
	}
}

TArray<FDeepDressRow> DeepDressing::ParseRows(const FString& Csv, TArray<FString>* OutErrors)
{
	TArray<FDeepDressRow> Out;
	int32 Line = 1;
	for (const TMap<FString, FString>& Cells : DeepData::ParseCsv(Csv))
	{
		++Line;
		FDeepDressRow Row;
		Row.Id = Str(Cells, TEXT("id"));
		Row.Mesh = Str(Cells, TEXT("mesh"));
		Row.Material = Str(Cells, TEXT("material"));
		auto Fail = [&](const FString& Why)
		{
			if (OutErrors)
			{
				OutErrors->Add(FString::Printf(TEXT("row %d (%s): %s"), Line, *Row.Id, *Why));
			}
		};
		if (Row.Id.IsEmpty())
		{
			Row.Id = FString::Printf(TEXT("row%d"), Line);
		}
		if (Row.Mesh.IsEmpty())
		{
			Fail(TEXT("no mesh"));
			continue;
		}
		const FString Kind = Str(Cells, TEXT("kind")).ToLower();
		if (Kind.IsEmpty() || Kind == TEXT("place"))
		{
			Row.Kind = EDeepDressKind::Place;
		}
		else if (Kind == TEXT("scatter"))
		{
			Row.Kind = EDeepDressKind::Scatter;
		}
		else
		{
			Fail(FString::Printf(TEXT("unknown kind '%s'"), *Kind));
			continue;
		}
		const FString Parent = Str(Cells, TEXT("parent")).ToLower();
		if (Parent.IsEmpty() || Parent == TEXT("root"))
		{
			Row.Parent = EDeepDressParent::Root;
		}
		else if (Parent == TEXT("trench"))
		{
			Row.Parent = EDeepDressParent::Trench;
		}
		else
		{
			Fail(FString::Printf(TEXT("unknown parent '%s'"), *Parent));
			continue;
		}
		const FString Ref = Str(Cells, TEXT("ref")).ToLower();
		if (Row.Parent == EDeepDressParent::Trench)
		{
			// The trench frame has one reference: the rock face.
			if (!Ref.IsEmpty() && Ref != TEXT("wall"))
			{
				Fail(FString::Printf(TEXT("parent trench takes ref wall, not '%s'"), *Ref));
				continue;
			}
			Row.Ref = EDeepDressRef::Wall;
		}
		else if (Ref.IsEmpty() || Ref == TEXT("bed"))
		{
			Row.Ref = EDeepDressRef::Bed;
		}
		else if (Ref == TEXT("anchor"))
		{
			Row.Ref = EDeepDressRef::Anchor;
		}
		else if (Ref == TEXT("depth"))
		{
			Row.Ref = EDeepDressRef::Depth;
		}
		else
		{
			Fail(FString::Printf(TEXT("unknown ref '%s' for parent root"), *Ref));
			continue;
		}
		Row.Stop = FMath::RoundToInt(Num(Cells, TEXT("stop"), 0.f));
		Row.XM = Num(Cells, TEXT("x_m"), 0.f);
		Row.YM = Num(Cells, TEXT("y_m"), 0.f);
		Row.ZM = Num(Cells, TEXT("z_m"), 0.f);
		Row.ZToM = Num(Cells, TEXT("z_to_m"), Row.ZM);
		Row.Yaw = Num(Cells, TEXT("yaw"), 0.f);
		Row.Pitch = Num(Cells, TEXT("pitch"), 0.f);
		Row.Roll = Num(Cells, TEXT("roll"), 0.f);
		Row.Scale = Num(Cells, TEXT("scale"), 1.f);
		Row.Sink = FMath::Clamp(Num(Cells, TEXT("sink"), 0.f), 0.f, 1.f);
		Row.Count = FMath::Clamp(FMath::RoundToInt(Num(Cells, TEXT("count"), 1.f)), 0, 2000);
		Row.RadiusMinM = FMath::Max(0.f, Num(Cells, TEXT("radius_min_m"), 0.f));
		Row.RadiusMaxM = FMath::Max(Row.RadiusMinM, Num(Cells, TEXT("radius_max_m"), Row.RadiusMinM));
		Row.ScaleMin = Num(Cells, TEXT("scale_min"), 1.f);
		Row.ScaleMax = FMath::Max(Row.ScaleMin, Num(Cells, TEXT("scale_max"), Row.ScaleMin));
		Row.Seed = FMath::RoundToInt(Num(Cells, TEXT("seed"), 0.f));
		Out.Add(MoveTemp(Row));
	}
	return Out;
}

double DeepDressing::DepthToRootZCm(float DepthM, float StopDepthM)
{
	return (double(StopDepthM) - double(DepthM)) * UDeepRules::CmPerM;
}

bool DeepDressing::Blocks(const FTransform& Piece, const FBox& MeshBox, const TArray<FDeepDressClear>& Clear)
{
	const FVector Centre = Piece.TransformPosition(MeshBox.GetCenter());
	const double Radius = MeshBox.GetExtent().Size() * Piece.GetMaximumAxisScale();
	for (const FDeepDressClear& C : Clear)
	{
		if (FMath::PointDistToSegment(Centre, C.StartCm, C.EndCm) < C.RadiusCm + Radius)
		{
			return true;
		}
	}
	return false;
}

TArray<FTransform> DeepDressing::Expand(const FDeepDressRow& Row, const FDeepDressContext& Context, int32* OutRejected)
{
	TArray<FTransform> Out;
	int32 Rejected = 0;
	const double Cm = UDeepRules::CmPerM;

	auto MakeRoot = [&](float X, float Y, float Z, const FRotator& Rot, float Scale) -> FTransform
	{
		const FQuat Q = Rot.Quaternion();
		FVector Loc = Context.AnchorCm + FVector(X * Cm, Y * Cm, 0.0);
		switch (Row.Ref)
		{
		case EDeepDressRef::Bed:
		{
			const double Surface = Context.AnchorCm.Z - (Context.BedBelowM ? Context.BedBelowM(FVector2D(X, Y)) : 0.f) * Cm;
			double MinZ = 0.0;
			double MaxZ = 0.0;
			RotatedZRange(Context.MeshBox, Q, Scale, MinZ, MaxZ);
			Loc.Z = Surface + Z * Cm - MinZ - Row.Sink * (MaxZ - MinZ);
			break;
		}
		case EDeepDressRef::Depth:
			Loc.Z = DepthToRootZCm(Z, Context.StopDepthM);
			break;
		default:
			Loc.Z = Context.AnchorCm.Z + Z * Cm;
			break;
		}
		return FTransform(Q, Loc, FVector(Scale));
	};

	auto MakeWall = [&](float Out_, float Along, float Depth, const FRotator& Rot, float Scale) -> FTransform
	{
		const float Relief = Context.WallReliefM ? Context.WallReliefM(Along, Depth) : 0.f;
		return FTransform(Rot.Quaternion(), FVector(-(Relief + Out_) * Cm, Along * Cm, -Depth * Cm), FVector(Scale));
	};

	const bool bWall = Row.Parent == EDeepDressParent::Trench;
	if (Row.Kind == EDeepDressKind::Place)
	{
		const FRotator Rot(Row.Pitch, Row.Yaw, Row.Roll);
		const FTransform T = bWall ? MakeWall(Row.XM, Row.YM, Row.ZM, Rot, Row.Scale) : MakeRoot(Row.XM, Row.YM, Row.ZM, Rot, Row.Scale);
		if (!bWall && Blocks(T, Context.MeshBox, Context.Clear))
		{
			++Rejected;
		}
		else
		{
			Out.Add(T);
		}
	}
	else
	{
		FRandomStream Rng(Row.Seed != 0 ? Row.Seed : int32(GetTypeHash(Row.Id)));
		for (int32 Index = 0; Index < Row.Count; ++Index)
		{
			// A few tries per piece, so a path through the ring thins it rather than emptying it.
			bool bPlaced = false;
			for (int32 Try = 0; Try < 6 && !bPlaced; ++Try)
			{
				const float Scale = Row.Scale * Rng.FRandRange(Row.ScaleMin, Row.ScaleMax);
				const FRotator Rot(Row.Pitch * Rng.FRandRange(-1.f, 1.f), Row.Yaw + Rng.FRandRange(0.f, 360.f), Row.Roll * Rng.FRandRange(-1.f, 1.f));
				const float Z = FMath::Lerp(Row.ZM, Row.ZToM, Rng.FRand());
				FTransform T;
				if (bWall)
				{
					const float Along = Row.YM + Rng.FRandRange(-Row.RadiusMaxM, Row.RadiusMaxM);
					T = MakeWall(Row.XM, Along, Z, Rot, Scale);
					bPlaced = true;
				}
				else
				{
					// Uniform over the ring's area.
					const float Angle = Rng.FRandRange(0.f, 2.f * PI);
					const float R = FMath::Sqrt(FMath::Lerp(FMath::Square(Row.RadiusMinM), FMath::Square(Row.RadiusMaxM), Rng.FRand()));
					T = MakeRoot(Row.XM + R * FMath::Cos(Angle), Row.YM + R * FMath::Sin(Angle), Z, Rot, Scale);
					bPlaced = !Blocks(T, Context.MeshBox, Context.Clear);
				}
				if (bPlaced)
				{
					Out.Add(T);
				}
			}
			Rejected += bPlaced ? 0 : 1;
		}
	}
	if (OutRejected)
	{
		*OutRejected = Rejected;
	}
	return Out;
}

TArray<FName> DeepDressing::PackagesToCook(const FString& DressingCsv, const FString& PackAssetsCsv)
{
	TSet<FString> Set;
	for (const FDeepDressRow& Row : ParseRows(DressingCsv))
	{
		for (const FString& Path : { Row.Mesh, Row.Material })
		{
			const FString P = NormalisePackage(Path);
			if (!P.IsEmpty())
			{
				Set.Add(P);
			}
		}
	}
	for (const TMap<FString, FString>& Row : DeepData::ParseCsv(PackAssetsCsv))
	{
		const FString P = NormalisePackage(Str(Row, TEXT("package")));
		if (!P.IsEmpty())
		{
			Set.Add(P);
		}
	}
	TArray<FString> Sorted = Set.Array();
	Sorted.Sort();
	TArray<FName> Out;
	for (const FString& P : Sorted)
	{
		Out.Add(FName(*P));
	}
	return Out;
}

#if WITH_EDITOR
namespace
{
	// The pack is loaded by path, which the cook cannot follow, and it is 4.8 GB: add exactly what the game names.
	void AddDressingToCook(TConstArrayView<const ITargetPlatform*> /*Platforms*/, TArray<FName>& PackagesToCook, TArray<FName>& /*NeverCook*/)
	{
		FString Dressing;
		FString PackAssets;
		FFileHelper::LoadFileToString(Dressing, *DeepData::DataPath(UDeepTuningSettings::Get().Look.DressingFile));
		FFileHelper::LoadFileToString(PackAssets, *DeepData::DataPath(TEXT("PackAssets.csv")));
		const TArray<FName> Packages = DeepDressing::PackagesToCook(Dressing, PackAssets);
		for (const FName& Package : Packages)
		{
			PackagesToCook.AddUnique(Package);
			UE_LOG(LogDeepDressing, Display, TEXT("DEEPCOOK add %s"), *Package.ToString());
		}
		UE_LOG(LogDeepDressing, Display, TEXT("DEEPCOOK added %d packages named by Dressing.csv and PackAssets.csv"), Packages.Num());
	}

	struct FRegisterDressingCook
	{
		FRegisterDressingCook()
		{
			FGameDelegates::Get().GetModifyCookDelegate().AddStatic(&AddDressingToCook);
		}
	} GRegisterDressingCook;
}
#endif

ADeepDressing::ADeepDressing()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);
}

void ADeepDressing::BeginPlay()
{
	Super::BeginPlay();
	AttachToRoot();
	if (bLoaded)
	{
		return;
	}
	const FString Path = DeepData::DataPath(UDeepTuningSettings::Get().Look.DressingFile);
	FString Csv;
	if (FFileHelper::LoadFileToString(Csv, *Path))
	{
		LoadFromString(Csv);
	}
	else
	{
		UE_LOG(LogDeepDressing, Display, TEXT("DEEPDRESS no dressing file at %s: the sea stays bare"), *Path);
	}
}

void ADeepDressing::AttachToRoot()
{
	if (GetAttachParentActor())
	{
		return;
	}
	if (ADeepArenaRoot* ArenaRoot = DeepArena::FindArenaRoot(GetWorld()))
	{
		// The dressing frame is the root's frame: every row is placed root-local.
		AttachToActor(ArenaRoot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
}

FDeepDressContext ADeepDressing::MakeContext(int32 Stop) const
{
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const FDeepLookTuning& Look = Tuning.Look;
	FDeepDressContext Context;
	Context.StopDepthM = Tuning.GetStop(Stop).SubDepthM;
	const ADeepSubmarine* Sub = DeepArena::FindSubmarine(GetWorld());
	Context.AnchorCm = Sub ? Sub->GetAnchorLocation() - Sub->GetActorLocation() : FVector(0.0, 0.0, -Tuning.Arena.AnchorBelowSubM * UDeepRules::CmPerM);
	const TArray<FVector4> Rest = ADeepAbyssFloor::RestPointsFromTuning();
	Context.BedBelowM = [Rest, Look](const FVector2D& XyM) { return ADeepAbyssFloor::SeabedBelowM(XyM, Rest, Look); };
	const float Every = Look.TrenchLedgeEveryM;
	const float Out = Look.TrenchLedgeOutM;
	Context.WallReliefM = [Every, Out](float Along, float Depth) { return ADeepSubmarine::TrenchReliefM(Along, Depth, Every, Out); };

	// Open water a diver uses: hatch to every salvage spot, and the shaft under the hatch down to the relic.
	const float ClearCm = FMath::Max(0.f, Look.DressingClearM) * UDeepRules::CmPerM;
	for (const FDeepSalvageSpot& Spot : Tuning.Arena.SalvageSpots)
	{
		Context.Clear.Add({ Context.AnchorCm, Context.AnchorCm + UDeepRules::SalvageSpotOffsetCm(Spot), ClearCm });
	}
	const int32 RelicStop = Tuning.Arena.RelicStop;
	const FVector Relic = UDeepRules::RelicOffsetCm(Tuning.Arena, Tuning.GetStop(RelicStop).SubDepthM, UDeepRules::TetherLengthM(RelicStop, Tuning.Tether));
	Context.Clear.Add({ Context.AnchorCm, Context.AnchorCm + Relic, FMath::Max(ClearCm, Look.SeabedPitRadiusM * float(UDeepRules::CmPerM)) });
	return Context;
}

int32 ADeepDressing::LoadFromString(const FString& Csv)
{
	for (UHierarchicalInstancedStaticMeshComponent* C : Components)
	{
		if (C)
		{
			C->DestroyComponent();
		}
	}
	Components.Reset();
	Groups.Reset();
	RowGroup.Reset();
	TArray<FString> Errors;
	Rows = DeepDressing::ParseRows(Csv, &Errors);
	for (const FString& Error : Errors)
	{
		UE_LOG(LogDeepDressing, Warning, TEXT("DEEPDRESS skipped %s"), *Error);
	}

	ADeepSubmarine* Sub = DeepArena::FindSubmarine(GetWorld());
	USceneComponent* Trench = Sub ? Sub->GetTrench() : nullptr;
	TMap<FString, int32> GroupByKey;
	for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
	{
		const FDeepDressRow& Row = Rows[RowIndex];
		const FString Key = FString::Printf(TEXT("%s|%s|%d|%d|%d"), *Row.Mesh, *Row.Material, int32(Row.Parent), Row.Stop, Row.HidesBelowStop() ? 1 : 0);
		if (const int32* Existing = GroupByKey.Find(Key))
		{
			Groups[*Existing].RowIndices.Add(RowIndex);
			RowGroup.Add(Row.Id, *Existing);
			continue;
		}
		if (MissingMeshes.Contains(Row.Mesh))
		{
			continue;
		}
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Row.Mesh, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Mesh)
		{
			// One warning per mesh: a checkout without the pack plays with a bare sea.
			MissingMeshes.Add(Row.Mesh);
			UE_LOG(LogDeepDressing, Warning, TEXT("DEEPDRESS mesh missing (%s): its rows draw nothing"), *Row.Mesh);
			continue;
		}
		UMaterialInterface* Material = nullptr;
		if (!Row.Material.IsEmpty() && !MissingMaterials.Contains(Row.Material))
		{
			Material = LoadObject<UMaterialInterface>(nullptr, *Row.Material, nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!Material)
			{
				MissingMaterials.Add(Row.Material);
				UE_LOG(LogDeepDressing, Warning, TEXT("DEEPDRESS material missing (%s): the mesh keeps its own"), *Row.Material);
			}
		}
		UHierarchicalInstancedStaticMeshComponent* C = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, FName(*FString::Printf(TEXT("Dress%02d_%s"), Groups.Num(), *Mesh->GetName())));
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Mesh);
		if (Material)
		{
			for (int32 Slot = 0; Slot < C->GetNumMaterials(); ++Slot)
			{
				C->SetMaterial(Slot, Material);
			}
		}
		C->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(true);
		const bool bOnTrench = Row.Parent == EDeepDressParent::Trench && Trench;
		C->SetupAttachment(bOnTrench ? Trench : Root.Get());
		C->RegisterComponent();
		AddInstanceComponent(C);
		Components.Add(C);

		FGroup Group;
		Group.Component = C;
		Group.Stop = Row.Stop;
		Group.bHideBelow = Row.HidesBelowStop();
		Group.Parent = Row.Parent;
		Group.RowIndices.Add(RowIndex);
		GroupByKey.Add(Key, Groups.Add(Group));
		RowGroup.Add(Row.Id, Groups.Num() - 1);
	}
	bLoaded = true;
	BuiltStop = -1;
	Refresh();
	UE_LOG(LogDeepDressing, Display, TEXT("DEEPDRESS %d rows, %d components, %d pieces, %d meshes missing"), Rows.Num(), Groups.Num(), GetInstanceCount(), MissingMeshes.Num());
	return Rows.Num();
}

void ADeepDressing::Build(int32 Stop)
{
	BuiltStop = Stop;
	FDeepDressContext Context = MakeContext(Stop);
	for (FGroup& Group : Groups)
	{
		UHierarchicalInstancedStaticMeshComponent* C = Group.Component;
		if (!C)
		{
			continue;
		}
		C->ClearInstances();
		Context.MeshBox = C->GetStaticMesh() ? C->GetStaticMesh()->GetBoundingBox() : FBox(FVector(-50.f), FVector(50.f));
		TArray<FTransform> All;
		for (const int32 RowIndex : Group.RowIndices)
		{
			int32 Rejected = 0;
			const TArray<FTransform> Pieces = DeepDressing::Expand(Rows[RowIndex], Context, &Rejected);
			if (Rejected > 0)
			{
				UE_LOG(LogDeepDressing, Log, TEXT("DEEPDRESS %s: %d pieces kept out of a diver's path"), *Rows[RowIndex].Id, Rejected);
			}
			All.Append(Pieces);
		}
		if (All.Num() > 0)
		{
			C->AddInstances(All, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ false);
		}
		UE_LOG(LogDeepDressing, Verbose, TEXT("DEEPDRESS %s: %d pieces, bounds %s"), *GetNameSafe(C->GetStaticMesh()), All.Num(), *Context.MeshBox.ToString());
	}
}

void ADeepDressing::Refresh()
{
	UWorld* World = GetWorld();
	if (!bLoaded || !World)
	{
		return;
	}
	AttachToRoot();
	const ADeepGameState* State = World->GetGameState<ADeepGameState>();
	const int32 Stop = State ? State->Stop : 1;
	if (Stop != BuiltStop)
	{
		// The root re-bases when the loaded stop changes: re-place every piece for the new stop.
		Build(Stop);
	}
	const bool bBelowStop = State && State->SubDepthM > UDeepTuningSettings::Get().GetStop(Stop).SubDepthM + 0.5f;
	const ADeepSubmarine* Sub = DeepArena::FindSubmarine(World);
	const USceneComponent* Trench = Sub ? Sub->GetTrench() : nullptr;
	const bool bHelmView = Trench && Trench->IsVisible();
	for (FGroup& Group : Groups)
	{
		if (!Group.Component)
		{
			continue;
		}
		bool bVisible = Group.Stop == 0 || Group.Stop == Stop;
		bVisible &= !(Group.bHideBelow && bBelowStop);
		bVisible &= Group.Parent != EDeepDressParent::Trench || bHelmView;
		if (Group.Component->IsVisible() != bVisible)
		{
			Group.Component->SetVisibility(bVisible);
		}
	}
}

void ADeepDressing::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Refresh();
}

int32 ADeepDressing::GetInstanceCount() const
{
	int32 N = 0;
	for (const FGroup& Group : Groups)
	{
		N += Group.Component ? Group.Component->GetInstanceCount() : 0;
	}
	return N;
}

UHierarchicalInstancedStaticMeshComponent* ADeepDressing::FindComponentForRow(const FString& Id) const
{
	const int32* Index = RowGroup.Find(Id);
	return Index && Groups.IsValidIndex(*Index) ? Groups[*Index].Component.Get() : nullptr;
}
