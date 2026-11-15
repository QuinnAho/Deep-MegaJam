// Dressing: rock and ruin from owned packs (Dragon Cave), placed from text rows in Content/Data/Dressing.csv (plan 7).
// One ADeepDressing per world, spawned by DeepArena::EnsureArena, hung from the arena root so driving moves it. Every
// machine reads the CSV itself; nothing is replicated but the actor. One instanced component per mesh (and material,
// parent, stop), no collision, shadows on. A mesh that is not installed logs one warning and draws nothing, so a checkout
// without the pack builds, tests and plays.
//
// Columns (by header name; empty cells take the defaults in FDeepDressRow):
//   id, stop (0 = every stop), kind (place|scatter), parent (root|trench), ref (bed|anchor|depth|wall), mesh, material,
//   x_m, y_m, z_m, z_to_m, yaw, pitch, roll, scale, sink, count, radius_min_m, radius_max_m, scale_min, scale_max, seed,
//   silt (SiltAmount for a skin material such as /Game/Deep/Dressing/M_DressSilt: see DeepDressing::Skin)
// Position by ref:
//   bed    x, y from the hatch anchor; the mesh's lowest point sits z_m above the silt there, then sinks by `sink` of its
//          height (0.3 = a third buried). Hidden while the sub is below the loaded stop, like the bed itself.
//   anchor x, y, z from the hatch anchor (pivot).
//   depth  x, y from the anchor; z_m is an ABSOLUTE depth in metres (positive down). Placed at (stop depth - depth) under
//          the root and re-placed whenever the loaded stop changes: the root re-bases then (the trap in plan 2).
//   wall   parent trench only: y along the wall, z_m absolute depth, x metres out from the rock face toward the sub
//          (negative = sunk into it). Shown only in the helm view, with the trench wall.
// Scatter: `count` pieces, seeded. Root: in the ring radius_min_m..radius_max_m round (x, y), z (or depth) between z_m and
// z_to_m, full random yaw, pitch and roll jittered by up to the row's pitch and roll. Trench: along y +- radius_max_m.
// Every root piece is kept clear of the diver's paths (FDeepLookTuning::DressingClearM), except low pieces lying in the
// silt (DressingLowM); a placed piece in a path is dropped (logged).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeepDressing.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

enum class EDeepDressKind : uint8 { Place, Scatter };
enum class EDeepDressParent : uint8 { Root, Trench };
enum class EDeepDressRef : uint8 { Bed, Anchor, Depth, Wall };

struct DEEP_API FDeepDressRow
{
	FString Id;
	int32 Stop = 0;
	EDeepDressKind Kind = EDeepDressKind::Place;
	EDeepDressParent Parent = EDeepDressParent::Root;
	EDeepDressRef Ref = EDeepDressRef::Bed;
	FString Mesh;
	FString Material;
	float XM = 0.f;
	float YM = 0.f;
	float ZM = 0.f;
	float ZToM = 0.f;
	float Yaw = 0.f;
	float Pitch = 0.f;
	float Roll = 0.f;
	float Scale = 1.f;
	float Sink = 0.f;
	int32 Count = 1;
	float RadiusMinM = 0.f;
	float RadiusMaxM = 0.f;
	float ScaleMin = 1.f;
	float ScaleMax = 1.f;
	int32 Seed = 0;
	/** SiltAmount for a skin material (0 none .. 1 thick); negative keeps the material's own. */
	float Silt = -1.f;

	/** Hidden with the bed while the sub is below the loaded stop. */
	bool HidesBelowStop() const { return Parent == EDeepDressParent::Root && Ref != EDeepDressRef::Depth; }
};

/** A capsule of open water a diver uses (dressing-frame cm): nothing may stand in it. */
struct DEEP_API FDeepDressClear
{
	FVector StartCm = FVector::ZeroVector;
	FVector EndCm = FVector::ZeroVector;
	float RadiusCm = 0.f;
};

/** What expanding a row needs to know about the world. Positions are in the dressing frame (root-local cm). */
struct DEEP_API FDeepDressContext
{
	/** Depth of the loaded stop (m). */
	float StopDepthM = 100.f;
	/** The hatch anchor in the dressing frame (cm). */
	FVector AnchorCm = FVector::ZeroVector;
	/** Metres of silt below the anchor at (x, y) metres from it. */
	TFunction<float(const FVector2D&)> BedBelowM;
	/** The trench face's distance out from its base plane (m) at (along, depth). */
	TFunction<float(float, float)> WallReliefM;
	/** The mesh's local bounds (cm). */
	FBox MeshBox = FBox(FVector(-50.f), FVector(50.f));
	TArray<FDeepDressClear> Clear;
	/** A bed piece whose top is at most this high above the silt lies against the floor and may sit under a path (cm). */
	float LowCm = 0.f;
};

namespace DeepDressing
{
	/** Parses Dressing.csv text. Rows with no mesh or an unknown kind, parent or ref are skipped and described in OutErrors. */
	DEEP_API TArray<FDeepDressRow> ParseRows(const FString& Csv, TArray<FString>* OutErrors = nullptr);

	/**
	 * Root-local Z (cm) of a point at an absolute depth while StopDepthM is loaded. The root stands at the sub moved up by
	 * (SubDepthM - StopDepthM), so the point lands at (SubDepthM - DepthM) above the sub whatever stop is loaded.
	 */
	DEEP_API double DepthToRootZCm(float DepthM, float StopDepthM);

	/** The pieces of one row (dressing-frame transforms; for trench rows, trench-local). Pure and seeded. */
	DEEP_API TArray<FTransform> Expand(const FDeepDressRow& Row, const FDeepDressContext& Context, int32* OutRejected = nullptr);

	/** Does a piece with these bounds and transform come into any clear capsule? */
	DEEP_API bool Blocks(const FTransform& Piece, const FBox& MeshBox, const TArray<FDeepDressClear>& Clear);

	/**
	 * Packages the cook must add: every non-engine asset named in Dressing.csv (mesh, material) and every `package` of
	 * PackAssets.csv. Object paths become package names; sorted, no duplicates. The pack is loaded by path, which the
	 * cook cannot see, and it is 4.8 GB, so only these go in.
	 */
	DEEP_API TArray<FName> PackagesToCook(const FString& DressingCsv, const FString& PackAssetsCsv);

	/**
	 * The material a mesh slot wears for a row. A skin master (a material with a BaseColorMap texture parameter, such as
	 * /Game/Deep/Dressing/M_DressSilt) becomes a dynamic instance carrying the slot's own pack textures (_BC, _N, _ORM)
	 * and SiltAmount (if Silt >= 0); any other material is used as it is.
	 */
	DEEP_API UMaterialInterface* Skin(UMaterialInterface* Master, UMaterialInterface* Original, float Silt, UObject* Outer);
}

UCLASS()
class DEEP_API ADeepDressing : public AActor
{
	GENERATED_BODY()

public:
	ADeepDressing();

	/** Replaces every row from CSV text and builds the pieces for the current stop. Returns rows parsed. */
	int32 LoadFromString(const FString& Csv);

	/** Re-reads the stop, re-places depth rows if it changed and sets what is visible. Called each tick. */
	void Refresh();

	/** Pieces drawn now (all components). */
	int32 GetInstanceCount() const;

	/** Instanced components made (one per mesh, material, parent and stop). */
	int32 GetComponentCount() const { return Groups.Num(); }

	/** The component that draws a row's pieces, or null (missing mesh, unknown id). */
	UHierarchicalInstancedStaticMeshComponent* FindComponentForRow(const FString& Id) const;

	/** Meshes that were not found (each warned once). */
	const TSet<FString>& GetMissingMeshes() const { return MissingMeshes; }

	int32 GetBuiltStop() const { return BuiltStop; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	struct FGroup
	{
		TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Component;
		int32 Stop = 0;
		bool bHideBelow = false;
		EDeepDressParent Parent = EDeepDressParent::Root;
		TArray<int32> RowIndices;
	};

	void AttachToRoot();
	void Build(int32 Stop);
	FDeepDressContext MakeContext(int32 Stop) const;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<USceneComponent> Root;

	/** Keeps the made components alive (FGroup is not reflected). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Components;

	TArray<FDeepDressRow> Rows;
	TArray<FGroup> Groups;
	TMap<FString, int32> RowGroup;
	TSet<FString> MissingMeshes;
	TSet<FString> MissingMaterials;
	int32 BuiltStop = -1;
	bool bLoaded = false;
};
