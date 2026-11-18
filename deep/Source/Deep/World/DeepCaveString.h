// Deep.World.Caves: the baked cave chunks strung into the dive (D-055, D-064). Every machine spawns its own string, not
// replicated: one static mesh per chunk, composed from Content/Data/CaveChunks.csv (Sim/DeepCaveStringRules) with S's
// Park socket at stop 1's still frame, turned by the hull's home heading. The actor's transform is the string's frame,
// so the game state's offset, depth and yaw are the hull's pose in it (UDeepCaveStringRules::HullCm). The helm sweeps
// the hull's real box against these chunks only (UDeepRunComponent::StepPilotCave).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sim/DeepCaveStringRules.h"
#include "DeepCaveString.generated.h"

class UStaticMeshComponent;
class UWorld;
class ADeepCaveString;
class ADeepGameState;

/** One sweep of the hull's box through the string (string frame, cm). */
struct FDeepCaveSweep
{
	bool bHit = false;
	/** The box overlapped rock where it started; Normal and PenetrationCm are the push out (may be zero). */
	bool bStartPenetrating = false;
	/** 0..1 along the move to the first touch. */
	float Time = 1.f;
	/** Out of the rock, toward the hull (string frame). */
	FVector Normal = FVector::ZeroVector;
	float PenetrationCm = 0.f;
	FName Chunk;
};

namespace DeepCaveString
{
	/** The world's string, or null (caves off, or not built yet on this machine). */
	DEEP_API ADeepCaveString* Find(const UWorld* World);

	/**
	 * Caves on: finds or builds the string (once the hull has its home) and removes the shaft's rock. Every machine; the
	 * arena calls it when built and each frame (DeepArena::UpdateStillWorld). Null with caves off or on a bad table.
	 */
	DEEP_API ADeepCaveString* Ensure(UWorld* World);

	/** The hull's pose in the string's frame from the game state (cm, degrees); its depth is read on the depth map (D-065). */
	DEEP_API FDeepCavePose HullPose(const ADeepGameState& State);

	/** The true depth where the gauge reads ReadM: the world's string's depth map, else ReadM (caves off, or not built yet). */
	DEEP_API float PhysicalDepthM(const UWorld* World, float ReadM);

	/** What the gauge reads at a true depth (the inverse; PhysicalM with no string). */
	DEEP_API float ReadDepthM(const UWorld* World, float PhysicalM);
}

UCLASS(Transient, NotPlaceable)
class DEEP_API ADeepCaveString : public AActor
{
	GENERATED_BODY()

public:
	ADeepCaveString();

	/** Reads the tables, composes the string and adds the chunks. False, with the reason logged, on a bad table or mesh. */
	bool Build(FString& OutError);

	const TArray<FDeepCavePlace>& GetPlaces() const { return Places; }
	const TArray<FDeepCaveSocketRow>& GetSockets() const { return Sockets; }
	const TArray<FDeepCavePathPoint>& GetPath() const { return Path; }
	const TArray<FDeepCavePose>& GetPathPoses() const { return PathPoses; }
	const TArray<float>& GetPathMetres() const { return PathMetres; }
	const TArray<FDeepCaveDepthKnot>& GetDepthMap() const { return DepthMap; }

	/** What the gauge reads at a world height (D-065): the string's origin is the first stop's depth. */
	float ReadDepthAtZ(double WorldZCm) const;

	/** A chunk's socket in the world (cm); false if there is none. */
	bool SocketWorld(FName Chunk, FName Socket, FVector& OutWorldCm) const;

	/** The first rock straight down from a world point within MaxDownCm (complex collision, the chunks only). */
	bool RockBelow(const FVector& WorldCm, float MaxDownCm, FHitResult& OutHit) const;

	/**
	 * The rock under a world box's footprint: straight down at its centre and its four corners (pulled in 10%), from
	 * UpCm above its bottom to DownCm below it. The highest and lowest hits' heights; the number of points that hit.
	 */
	int32 FootprintRock(const FBox& WorldBox, float UpCm, float DownCm, double& OutHighestZ, double& OutLowestZ) const;

	/** A box (world centre, half size, rotation) overlaps a chunk's rock. */
	bool OverlapsBoxWorld(const FVector& CentreCm, const FVector& HalfCm, const FQuat& Rotation) const;
	const TArray<TObjectPtr<UStaticMeshComponent>>& GetChunks() const { return Chunks; }

	/** A chunk's socket as a hull pose in the string's frame; false if there is none. */
	bool SocketPose(FName Chunk, FName Socket, FDeepCavePose& OutPose) const;

	/**
	 * Sweeps a box (Box: the hull's solid bounds in its own frame, cm) from one hull origin to another at a heading,
	 * against the chunks only (complex collision, as cavestring.sweep traces). The nearest touch wins.
	 */
	FDeepCaveSweep SweepHull(const FBox& Box, const FVector& FromCm, const FVector& ToCm, float YawDeg) const;

	/** The box (grown by InflateCm on every side) overlaps a chunk with the hull origin at AtCm and that heading. */
	bool OverlapsHull(const FBox& Box, const FVector& AtCm, float YawDeg, float InflateCm = 0.f) const;

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> Frame;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Chunks;

	TArray<FDeepCavePlace> Places;
	TArray<FDeepCaveSocketRow> Sockets;
	TArray<FDeepCavePathPoint> Path;
	TArray<FDeepCavePose> PathPoses;
	TArray<float> PathMetres;
	TArray<FDeepCaveDepthKnot> DepthMap;

	/** World shape transform for the box at a hull pose (string frame). */
	FTransform BoxWorld(const FBox& Box, const FVector& AtCm, float YawDeg) const;
};
