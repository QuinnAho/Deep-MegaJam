#include "World/DeepDressing.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/FileManager.h"
#include "Components/StaticMeshComponent.h"
#include "Game/DeepGameState.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture.h"
#include "RenderUtils.h"
#include "RHIGlobals.h"
#include "RHI.h"
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

// Hides every piece without unloading it: an A/B of the frame time in one run (Deep.Dressing.Hide 1, then 0).
static TAutoConsoleVariable<int32> CVarDeepDressingHide(TEXT("Deep.Dressing.Hide"), 0, TEXT("1 hides all dressing (for a frame-time A/B); 0 shows it."));

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
		Row.Tex = Str(Cells, TEXT("tex"));
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
		else if (Ref == TEXT("shaft"))
		{
			Row.Ref = EDeepDressRef::Shaft;
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
		Row.Silt = Num(Cells, TEXT("silt"), -1.f);
		Row.Sat = Num(Cells, TEXT("sat"), -1.f);
		Out.Add(MoveTemp(Row));
	}
	return Out;
}

double DeepDressing::DepthToRootZCm(float DepthM, float StopDepthM)
{
	return (double(StopDepthM) - double(DepthM)) * UDeepRules::CmPerM;
}

// The last refusal, for the log line that reports it.
static FString GDeepDressLastBlock;

bool DeepDressing::Blocks(const FTransform& Piece, const FBox& MeshBox, const TArray<FDeepDressClear>& Clear)
{
	// Distance from the path to the piece's own (rotated, scaled) box, not to a sphere round it: a bounding sphere refused
	// every flat slab and column near the well (first 500 m frames). The path is sampled every 25 cm.
	const FVector Centre = Piece.TransformPosition(MeshBox.GetCenter());
	const double Scale = FMath::Max(UE_KINDA_SMALL_NUMBER, Piece.GetMaximumAxisScale());
	const double Reach = MeshBox.GetExtent().Size() * Scale;
	for (int32 Index = 0; Index < Clear.Num(); ++Index)
	{
		const FDeepDressClear& C = Clear[Index];
		// Cheap reject: the bounding sphere does not reach the capsule.
		if (FMath::PointDistToSegment(Centre, C.StartCm, C.EndCm) >= C.RadiusCm + Reach)
		{
			continue;
		}
		const double Length = FVector::Dist(C.StartCm, C.EndCm);
		const int32 Steps = FMath::Clamp(FMath::CeilToInt(Length / 25.0), 1, 2000);
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			const FVector P = FMath::Lerp(C.StartCm, C.EndCm, double(Step) / Steps);
			const FVector Local = Piece.InverseTransformPosition(P);
			const double Dist = FMath::Sqrt(MeshBox.ComputeSquaredDistanceToPoint(Local)) * Scale;
			if (Dist < C.RadiusCm)
			{
				GDeepDressLastBlock = FString::Printf(TEXT("centre %s m, path %d to %s m, box %.1f m off it, needs %.1f"),
					*(Centre / 100.0).ToCompactString(), Index, *(C.EndCm / 100.0).ToCompactString(), Dist / 100.0, C.RadiusCm / 100.0);
				return true;
			}
		}
	}
	return false;
}

TArray<FTransform> DeepDressing::Expand(const FDeepDressRow& Row, const FDeepDressContext& Context, int32* OutRejected)
{
	TArray<FTransform> Out;
	int32 Rejected = 0;
	const double Cm = UDeepRules::CmPerM;
	// Low pieces lie against the bed, which collides: a diver passing over them cannot pass through them.
	bool bLow = false;

	auto MakeRoot = [&](float X, float Y, float Z, const FRotator& Rot, float Scale) -> FTransform
	{
		bLow = false;
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
			bLow = Z * Cm + (MaxZ - MinZ) * (1.0 - Row.Sink) <= Context.LowCm;
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

	// The relic's shaft: the piece stands against the shaft's wall at (angle, depth below the anchor), its +X outward, its
	// inner face Out metres in front of the rock. The wall's radius at that depth is found on the bed's own function.
	if (Row.Ref == EDeepDressRef::Shaft && Row.Parent == EDeepDressParent::Root)
	{
		auto MakeShaft = [&](float AngleDeg, float DepthM, float OutM, const FRotator& Jitter, float Scale) -> FTransform
		{
			const double A = FMath::DegreesToRadians(AngleDeg);
			const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
			double Lo = 0.0;
			double Hi = 14.0;
			for (int32 Step = 0; Step < 32 && Context.BedBelowM; ++Step)
			{
				const double Mid = 0.5 * (Lo + Hi);
				(Context.BedBelowM(Dir * Mid) > DepthM ? Lo : Hi) = Mid;
			}
			const FQuat Q = FRotator(Jitter.Pitch, AngleDeg + Jitter.Yaw, Jitter.Roll).Quaternion();
			const double PivotR = (Lo - OutM) * Cm - Context.MeshBox.Min.X * Scale;
			FVector Loc = Context.AnchorCm + FVector(Dir.X * PivotR, Dir.Y * PivotR, 0.0);
			Loc.Z = Context.AnchorCm.Z - DepthM * Cm - Q.RotateVector(Context.MeshBox.GetCenter() * Scale).Z;
			return FTransform(Q, Loc, FVector(Scale));
		};
		// Standing out of the wall by at most DressingLowM, a piece lies against the rock, which collides.
		const bool bAgainstWall = Row.XM * Cm <= Context.LowCm;
		FRandomStream Rng(Row.Seed != 0 ? Row.Seed : int32(GetTypeHash(Row.Id)));
		const int32 Count = Row.Kind == EDeepDressKind::Place ? 1 : Row.Count;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FTransform T;
			if (Row.Kind == EDeepDressKind::Place)
			{
				T = MakeShaft(Row.YM, Row.ZM, Row.XM, FRotator(Row.Pitch, Row.Yaw, Row.Roll), Row.Scale);
			}
			else
			{
				const float Scale = Row.Scale * Rng.FRandRange(Row.ScaleMin, Row.ScaleMax);
				const FRotator Jitter(Row.Pitch * Rng.FRandRange(-1.f, 1.f), Row.Yaw * Rng.FRandRange(-1.f, 1.f), Row.Roll * Rng.FRandRange(-1.f, 1.f));
				T = MakeShaft(Row.YM + Rng.FRandRange(0.f, 360.f), FMath::Lerp(Row.ZM, Row.ZToM, Rng.FRand()), Row.XM, Jitter, Scale);
			}
			if (!bAgainstWall && Blocks(T, Context.MeshBox, Context.Clear))
			{
				++Rejected;
				continue;
			}
			Out.Add(T);
		}
		if (OutRejected)
		{
			*OutRejected = Rejected;
		}
		return Out;
	}

	const bool bWall = Row.Parent == EDeepDressParent::Trench;
	if (Row.Kind == EDeepDressKind::Place)
	{
		const FRotator Rot(Row.Pitch, Row.Yaw, Row.Roll);
		const FTransform T = bWall ? MakeWall(Row.XM, Row.YM, Row.ZM, Rot, Row.Scale) : MakeRoot(Row.XM, Row.YM, Row.ZM, Rot, Row.Scale);
		if (!bWall && !bLow && Blocks(T, Context.MeshBox, Context.Clear))
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
					bPlaced = bLow || !Blocks(T, Context.MeshBox, Context.Clear);
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

namespace
{
	/**
	 * Streams virtually: the asset asks for it and the platform has it. Not IsCurrentlyVirtualTextured(): in an uncooked game
	 * that reads false until the texture's data is built, so the same set picked the plain skin on one load and the VT
	 * one on the next, and a VT texture in a plain sampler came out black (the well's lining, pass two).
	 */
	bool IsVirtual(const UTexture* Tex)
	{
		return Tex && Tex->VirtualTextureStreaming && UseVirtualTexturing(GMaxRHIShaderPlatform);
	}

	/** Which sampler a texture needs: 0 colour (sRGB), 1 linear colour, 2 masks, 3 normal; +4 when virtual. */
	int32 SamplerClass(const UTexture* Tex)
	{
		int32 Class = 0;
		if (Tex->CompressionSettings == TC_Normalmap)
		{
			Class = 3;
		}
		else if (Tex->CompressionSettings == TC_Masks)
		{
			Class = 2;
		}
		else if (!Tex->SRGB)
		{
			Class = 1;
		}
		return Class + (IsVirtual(Tex) ? 4 : 0);
	}
}

UMaterialInterface* DeepDressing::Skin(UMaterialInterface* Master, UMaterialInterface* Original, float Silt, UObject* Outer, float Saturation)
{
	UTexture* Probe = nullptr;
	if (!Master || !Master->GetTextureParameterValue(FHashedMaterialParameterInfo(FName(TEXT("BaseColorMap"))), Probe))
	{
		// A plain override: the mesh wears it as it is.
		return Master;
	}
	// The pack's own textures, matched by the pack's names (_BC, _N, _ORM). A pack master has several texture parameters
	// and only some are set by an instance: the instance's own values win over the master's defaults (MI_Sphere01's
	// T_Stoneworks_BC was lost behind a default _BC of the master, and the Stoneworks stone came out black).
	TMap<FName, UTexture*> Found;
	TSet<FName> FromInstance;
	if (Original)
	{
		TArray<FMaterialParameterInfo> Infos;
		TArray<FGuid> Ids;
		Original->GetAllTextureParameterInfo(Infos, Ids);
		for (const FMaterialParameterInfo& Info : Infos)
		{
			UTexture* Tex = nullptr;
			if (!Original->GetTextureParameterValue(Info, Tex) || !Tex)
			{
				continue;
			}
			const FString Name = Tex->GetName();
			const FName Slot = Name.EndsWith(TEXT("_BC")) ? FName(TEXT("BaseColorMap"))
				: Name.EndsWith(TEXT("_N")) ? FName(TEXT("NormalMap"))
				: Name.EndsWith(TEXT("_ORM")) ? FName(TEXT("ORMMap"))
				: NAME_None;
			if (Slot.IsNone())
			{
				continue;
			}
			UTexture* Overridden = nullptr;
			const bool bOwn = Original->GetTextureParameterValue(Info, Overridden, /*bOveriddenOnly*/ true) && Overridden == Tex;
			if (!Found.Contains(Slot) || (bOwn && !FromInstance.Contains(Slot)))
			{
				Found.Add(Slot, Tex);
				if (bOwn)
				{
					FromInstance.Add(Slot);
				}
			}
		}
	}
	// Some pack textures stream virtually and need the master's VT twin (<master>VT): pick by the base colour.
	if (UTexture* const* Base = Found.Find(FName(TEXT("BaseColorMap"))); Base && Probe && IsVirtual(*Base) != IsVirtual(Probe))
	{
		const FString Package = FPackageName::ObjectPathToPackageName(Master->GetPathName());
		const FString Name = FPackageName::GetShortName(Package);
		const FString TwinName = Name.EndsWith(TEXT("VT")) ? Name.LeftChop(2) : Name + TEXT("VT");
		const FString Twin = FPackageName::GetLongPackagePath(Package) / TwinName + TEXT(".") + TwinName;
		if (UMaterialInterface* Other = LoadObject<UMaterialInterface>(nullptr, *Twin, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Master = Other;
		}
	}
	UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Master, Outer);
	if (Silt >= 0.f)
	{
		Dynamic->SetScalarParameterValue(TEXT("SiltAmount"), Silt);
	}
	if (Saturation >= 0.f)
	{
		Dynamic->SetScalarParameterValue(TEXT("Saturation"), Saturation);
	}
	for (const TPair<FName, UTexture*>& Pair : Found)
	{
		UTexture* Default = nullptr;
		Master->GetTextureParameterValue(FHashedMaterialParameterInfo(Pair.Key), Default);
		const bool bMasterVT = Master->GetName().EndsWith(TEXT("VT"));
		if ((Default && SamplerClass(Default) % 4 != SamplerClass(Pair.Value) % 4) || IsVirtual(Pair.Value) != bMasterVT)
		{
			// A texture of another kind would sample wrong: keep the default and say so once.
			static TSet<FString> Warned;
			const FString Name = Pair.Value->GetName();
			if (!Warned.Contains(Name))
			{
				Warned.Add(Name);
				UE_LOG(LogDeepDressing, Warning, TEXT("DEEPDRESS %s does not fit %s's sampler (%d, wants %d): left out"), *Name, *Pair.Key.ToString(), SamplerClass(Pair.Value), Default ? SamplerClass(Default) : -1);
			}
			continue;
		}
		Dynamic->SetTextureParameterValue(Pair.Key, Pair.Value);
	}
	return Dynamic;
}

TArray<FName> DeepDressing::PackagesToCook(const FString& DressingCsv, const FString& PackAssetsCsv)
{
	TSet<FString> Set;
	for (const FDeepDressRow& Row : ParseRows(DressingCsv))
	{
		for (const FString& Path : { Row.Mesh, Row.Material, Row.Tex })
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
	Context.LowCm = FMath::Max(0.f, Look.DressingLowM) * UDeepRules::CmPerM;
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
		const FString Key = FString::Printf(TEXT("%s|%s|%s|%.3f|%.3f|%d|%d|%d"), *Row.Mesh, *Row.Material, *Row.Tex, Row.Silt, Row.Sat, int32(Row.Parent), Row.Stop, Row.HidesBelowStop() ? 1 : 0);
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
			// Some pack meshes (the Stoneworks set) carry the bare VT master in their slot, with none of the set's
			// textures: the skin then took its defaults and the stone rendered black (unlit 500 m frame). `tex` names
			// an instance that has them.
			UMaterialInterface* TexSource = Row.Tex.IsEmpty() ? nullptr : LoadObject<UMaterialInterface>(nullptr, *Row.Tex, nullptr, LOAD_NoWarn | LOAD_Quiet);
			for (int32 Slot = 0; Slot < C->GetNumMaterials(); ++Slot)
			{
				C->SetMaterial(Slot, DeepDressing::Skin(Material, TexSource ? TexSource : Mesh->GetMaterial(Slot), Row.Silt, C, Row.Sat));
			}
		}
		C->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(true);
		// Trench rock streams past at 8 m/s: new pieces showed a low mip, magnified into a blocky checker, before the
		// streamer caught up (pass-one drive frames). Their few textures stay at full mip.
		C->bForceMipStreaming = Row.Parent == EDeepDressParent::Trench;
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
				UE_LOG(LogDeepDressing, Log, TEXT("DEEPDRESS %s: %d pieces kept out of a diver's path (last: %s)"), *Rows[RowIndex].Id, Rejected, *GDeepDressLastBlock);
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
		bVisible &= CVarDeepDressingHide.GetValueOnGameThread() == 0;
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

// Deep.Dressing.Sheet <first> <count> [cols]: a contact sheet of the pack's meshes (Content/DragonCave/Meshes, by name),
// each scaled to fit a 1.1 m cell, in a grid 7 m in front of the view. Logs each mesh's real size. For choosing meshes.
static FAutoConsoleCommandWithWorldAndArgs GDeepDressingSheet(
	TEXT("Deep.Dressing.Sheet"),
	TEXT("Deep.Dressing.Sheet <first> <count> [cols]: lays pack meshes out in a grid in front of the view and logs their sizes."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		static TWeakObjectPtr<AActor> Sheet;
		if (Sheet.IsValid())
		{
			Sheet->Destroy();
		}
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (!PC || !PC->PlayerCameraManager)
		{
			return;
		}
		const int32 First = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
		const int32 Count = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 30;
		const int32 Cols = Args.Num() > 2 ? FMath::Max(1, FCString::Atoi(*Args[2])) : 6;
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(FPaths::ProjectContentDir() / TEXT("DragonCave/Meshes/*.uasset")), true, false);
		Files.Sort();
		const FVector Cam = PC->PlayerCameraManager->GetCameraLocation();
		const FRotator Rot = PC->PlayerCameraManager->GetCameraRotation();
		const FVector Fwd = Rot.Vector();
		const FVector Right = FRotationMatrix(Rot).GetUnitAxis(EAxis::Y);
		const FVector Up = FRotationMatrix(Rot).GetUnitAxis(EAxis::Z);
		AActor* Actor = World->SpawnActor<AActor>(Cam, FRotator::ZeroRotator);
		USceneComponent* SheetRoot = NewObject<USceneComponent>(Actor, TEXT("SheetRoot"));
		Actor->SetRootComponent(SheetRoot);
		SheetRoot->RegisterComponent();
		Sheet = Actor;
		const int32 Last = FMath::Min(Files.Num(), First + Count);
		const int32 Rows = FMath::DivideAndRoundUp(Last - First, Cols);
		const double Cell = 130.0;
		for (int32 Index = First; Index < Last; ++Index)
		{
			const FString Name = FPaths::GetBaseFilename(Files[Index]);
			const FString Path = FString::Printf(TEXT("/Game/DragonCave/Meshes/%s.%s"), *Name, *Name);
			UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
			if (!Mesh)
			{
				continue;
			}
			const FBox Box = Mesh->GetBoundingBox();
			const double Fit = 110.0 / FMath::Max(1.0, Box.GetSize().GetMax());
			const int32 N = Index - First;
			const double Col = (N % Cols) - (Cols - 1) * 0.5;
			const double Row = (Rows - 1) * 0.5 - (N / Cols);
			const FVector At = Cam + Fwd * 700.0 + Right * (Col * Cell) + Up * (Row * Cell);
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Actor);
			C->SetStaticMesh(Mesh);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetupAttachment(SheetRoot);
			C->SetWorldScale3D(FVector(Fit));
			// Face the camera: the mesh's +X toward it, centred in its cell.
			const FRotator Face = (-Fwd).Rotation() + FRotator(0.0, 30.0, 0.0);
			C->SetWorldRotation(Face);
			C->SetWorldLocation(At - Face.RotateVector(Box.GetCenter() * Fit));
			C->RegisterComponent();
			UE_LOG(LogDeepDressing, Display, TEXT("DEEPSHEET %d %s size_m %.2f x %.2f x %.2f pivot_to_bottom_m %.2f mats %d"), Index, *Name,
				Box.GetSize().X / 100.0, Box.GetSize().Y / 100.0, Box.GetSize().Z / 100.0, -Box.Min.Z / 100.0, Mesh->GetStaticMaterials().Num());
		}
	}));
