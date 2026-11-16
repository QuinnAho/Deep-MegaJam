// The cavern: rock walls and a roof round each stop's arena, the sea bed as its floor (Quinn, 11:55 AM Nov 15: "if
// you're swimming in an open ocean it really lessens it"). Generated in code, one shell per stop, hung from the arena
// root like ADeepAbyssFloor, so it slides past the fixed sub with the stop's bed and hides with it.
//
// Shape (metres, anchor frame): a surface round the anchor, star-shaped, so the straight line from the hatch to any point
// inside is clear. Below the anchor's level a sphere of radius BaseRadiusM; above it a flat-topped vault (a superellipse)
// up to FDeepCavernTuning::RoofAboveAnchorM. BaseRadiusM is chosen so the vault stays past NeedRadiusM (tether + margin,
// farthest salvage spot + margin) up to ClearAboveAnchorM over the hatch. Relief (noise, alcoves, strata, an uneven roof)
// only ever pushes rock outward. The roof has a round opening of HoleRadiusM centred over the sub, with a short collar up
// into the descent shaft. The wall runs down into the silt; there is no cave floor, so the bed and its sinkhole are
// untouched. Spurs hang from the roof, tips above the clear space and away from the shaft.
//
// Collision blocks everything (Visibility = the sonar and the lamp, Pawn = the diver), only on the shell that shows.
// `Deep.Cave.Cavern 0` hides it all (and the dressing's cave_ rows): today's open arena.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeepCavern.generated.h"

class UProceduralMeshComponent;
class UDeepTuningSettings;
struct FDeepCavernTuning;

/** One mesh section, in metres from the anchor. */
struct DEEP_API FDeepCaveSection
{
	TArray<FVector> VerticesM;
	TArray<FVector> Normals;
	TArray<FVector> TangentsX;
	TArray<FVector2D> UV0;
	TArray<int32> Triangles;
};

/** A stop's cave: the shell (rows of Columns + 1 vertices, the last column repeating the first) and the hanging spurs. */
struct DEEP_API FDeepCaveMesh
{
	FDeepCaveSection Shell;
	FDeepCaveSection Hangs;
	int32 Columns = 0;
	int32 Rows = 0;
	/** The row of the roof opening's rim (rows above it are the collar). */
	int32 RimRow = 0;
	float BaseRadiusM = 0.f;
	float NeedRadiusM = 0.f;
};

UCLASS()
class DEEP_API ADeepCavern : public AActor
{
	GENERATED_BODY()

public:
	ADeepCavern();

	/** True unless the tuning or the cheat (Deep.Cave.Cavern 0) turns the cave off. */
	static bool IsOn();

	/** How far rock must stand from the anchor at a stop (m): max(tether + margin, farthest salvage spot + margin). */
	static float NeedRadiusM(int32 Stop, const UDeepTuningSettings& Tuning);

	/** The sphere's radius that keeps the vault past NeedM up to ClearAboveAnchorM (m). */
	static float BaseRadiusM(float NeedM, const FDeepCavernTuning& Cave);

	/** Distance from the anchor to the rock along a unit direction (m): the base shape plus outward-only relief. Pure. */
	static float SurfaceDistanceM(const FVector& Dir, float BaseM, const FDeepCavernTuning& Cave);

	/**
	 * How far from the anchor along Dir a ball of ClearM stays clear of the rock (m). Pure. Relief only moves rock outward,
	 * so the smooth base shape (the sphere and the vault) bounds the rock from inside; the roof spurs end above
	 * ClearAboveAnchorM + HangTipClearM. The serpent keeps to this, so the Monster code never reads the cave tuning.
	 */
	static float RoomM(const FVector& Dir, float ClearM, float BaseM, const FDeepCavernTuning& Cave);

	/** PointM (anchor frame, m) pulled straight toward the anchor until a ball of ClearM round it is clear of the rock; unchanged if it is. Pure. */
	static FVector KeepClearM(const FVector& PointM, float ClearM, float BaseM, const FDeepCavernTuning& Cave);

	/** A stop's base radius (m), as Build makes it. */
	static float StopBaseRadiusM(int32 Stop, const UDeepTuningSettings& Tuning);

	/** The first cavern in a world, or null. */
	static ADeepCavern* FindIn(const UWorld* World);

	/** The stop (1-based) whose rock shows and collides now; 0 when none does (Deep.Cave.Cavern 0, or the bed hidden between stops). */
	int32 ShownStop() const;

	/** World point (cm) kept a ball of ClearM clear of the shown stop's rock (KeepClearM round this actor); unchanged when no rock shows. */
	FVector KeepClearCm(const FVector& PointCm, float ClearM) const;

	/** The whole mesh of a stop's cave. RestM: the bed's resting points (ADeepAbyssFloor), to find where the wall meets the silt. Pure. */
	static FDeepCaveMesh MakeMesh(int32 Stop, const TArray<FVector4>& RestM, const UDeepTuningSettings& Tuning);

	/** Builds every stop's shell. Synchronous cooking for tests (traces work at once); async in play. */
	void Build(bool bAsyncCook);

	/** Shows the shell of the loaded stop when the cave is on and the bed shows; the rest hide and stop colliding. */
	void UpdateShells(int32 Stop, bool bBedHidden);

	/**
	 * Deep.World.Still: every stop's shell at its own depth under this actor (which stands at stop 1's anchor), shown and
	 * colliding while the hull is at or above that stop (UDeepMovingSubRules::StillBedShows).
	 */
	void UpdateShellsStill(float SubDepthM);

	/** The shell component of a stop (1-based), or null. */
	UProceduralMeshComponent* GetShell(int32 Stop) const;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	static void ShowShell(UProceduralMeshComponent* Shell, bool bShow);

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UProceduralMeshComponent>> Shells;
};
