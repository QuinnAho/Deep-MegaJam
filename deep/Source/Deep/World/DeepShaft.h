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

namespace DeepShaft
{
	/** The shaft is on: Deep.Cave.Shaft (1 on, 0 off, -1 = the tuning's Cave.bShaft). Off = the old straight drive. */
	DEEP_API bool IsOn();

	/** The sub's solid box in its own frame, m. False (and the room's tuned size) when there is no sub. */
	DEEP_API bool SubBoxM(const ADeepSubmarine* Sub, FVector& OutMinM, FVector& OutMaxM);
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

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void Build();

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UProceduralMeshComponent>> Tubes;

	TArray<int32> TubeStretches;

	double BuildSeconds = 0.0;
	int32 Triangles = 0;
};
