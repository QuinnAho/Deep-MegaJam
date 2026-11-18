// The cave shaft's rock: one tube of procedural mesh per stretch between stops, built once from Sim/DeepCaveRules
// (centre and radius by depth), with collision (it blocks Visibility traces for the sonar, and pawns). Not replicated:
// every machine spawns its own (ADeepSubmarine::BeginPlay) and places it from the game state's depth and offset, the
// same way the arena root moves (D-022: the sub never moves).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeepShaft.generated.h"

class UProceduralMeshComponent;
class ADeepSubmarine;
class UMaterialInterface;
class UWorld;

namespace DeepShaft
{
	/** The shaft is on: Deep.Cave.Shaft (1 on, 0 off, -1 = the tuning's Cave.bShaft). Off = the old straight drive. */
	DEEP_API bool IsOn();

	/** The shaft is on in this world: IsOn(), and not a cave world (Deep.World.Caves: the chunks are the rock there). */
	DEEP_API bool IsOn(const UWorld* World);

	/** There is rock to hit in this world (the shaft, or the cave string): the hull bar shows, the helm steers off it. */
	DEEP_API bool HasRock(const UWorld* World);

	/** The sub's solid box in its own frame, m. False (and the room's tuned size) when there is no sub. */
	DEEP_API bool SubBoxM(const ADeepSubmarine* Sub, FVector& OutMinM, FVector& OutMaxM);

	/** Deep.Cave.AutoRebuild (1 = rebuild the rock when the Cave tuning changes; World/DeepCaveCheats.cpp). */
	DEEP_API bool AutoRebuild();

	/** Rebuilds every shaft and cavern in the world from the current tuning (Deep.Cave.Rebuild). Milliseconds taken. */
	DEEP_API double RebuildAll(UWorld* World, bool bSyncCook = false);
}

UCLASS()
class DEEP_API ADeepShaft : public AActor
{
	GENERATED_BODY()

public:
	ADeepShaft();

	/**
	 * To the game state's depth and offset: the tube point at absolute depth d sits (SubDepthM - d) above the sub, moved
	 * by -SubOffsetM sideways. A stretch is hidden (and without collision) while the sub is at or above the stop it
	 * starts under, where that stop's sea bed and salvage are. All hidden while the shaft is off.
	 */
	void Follow();

	/** The rock in pieces (a few dozen rows each); GetTubeStretch says which stretch each is in: 0 under the first stop, ..., the last under the deepest stop. */
	const TArray<TObjectPtr<UProceduralMeshComponent>>& GetTubes() const { return Tubes; }
	int32 GetTubeStretch(int32 Index) const { return TubeStretches.IsValidIndex(Index) ? TubeStretches[Index] : INDEX_NONE; }

	/** Seconds the build took (mesh and collision), and its size. */
	double GetBuildSeconds() const { return BuildSeconds; }
	int32 GetTriangleCount() const { return Triangles; }

	/** Where the drawn rock stands (m from the passage's centre) for a rule radius and the rock's relief there. */
	static float DrawRadiusM(float RuleRadiusM, float ReliefM, float ReliefOutM, float ReliefScale);

	/** Builds the rock again from the current tuning (Deep.Cave.Rebuild, Deep.Cave.AutoRebuild), then places it. */
	void Rebuild();

	/** Pack rocks scattered half-buried in the walls (look only), over every instanced mesh. */
	int32 GetScatterCount() const;

	/** A fingerprint of the tuning the build reads: when it changes, Deep.Cave.AutoRebuild rebuilds. */
	static uint32 TuningFingerprint();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void Build();

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UProceduralMeshComponent>> Tubes;

	TArray<int32> TubeStretches;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<class UInstancedStaticMeshComponent>> Scatter;

	/** Which stretch each scatter component's rocks are in (hidden with that stretch). */
	TArray<int32> ScatterStretches;

	void BuildScatter(const TArray<float>& Stops, float MinRadiusM, UMaterialInterface* Rock);

	uint32 BuiltFingerprint = 0;
	float FingerprintCheckS = 0.f;

	double BuildSeconds = 0.0;
	int32 Triangles = 0;
};
