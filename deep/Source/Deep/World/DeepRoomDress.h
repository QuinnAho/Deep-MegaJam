// The room's dressing (Lane E, rescope Nov 15): what makes the box a cramped, worn, working submarine. Hull ribs, pipe
// runs with flanges and valve wheels, a cable tray with cable drops, gauges, lockers, a fold-down bench, a ladder to the
// ceiling scuttle and caged lights, all engine basic shapes, instanced (one ISM per shape and material), no collision.
// The layout is pure (Pieces, Lights) and tested: every piece inside the room, none in a walk, the hatch or the helm zone.
// Light only from sources: amber practicals at the console and helm, red emergency, one low cool strip.

#pragma once

#include "CoreMinimal.h"

class AActor;
class USceneComponent;
class UInstancedStaticMeshComponent;
class ULocalLightComponent;
struct FDeepSubTuning;
struct FDeepRoomDressTuning;

enum class EDeepRoomMesh : uint8
{
	Cube,
	Cylinder,  // the engine cylinder: axis along its own Z
	Sphere,
	Count
};

enum class EDeepRoomMat : uint8
{
	Frame,   // worn dark painted steel: ribs, lockers, tray, cages
	Pipe,    // pipe paint, rustier
	Brass,   // dull tarnished brass: valve wheels, gauge bezels, handles
	Rubber,  // cables, the bench cushion, locker vents
	Glass,   // grimy glass: gauge faces, bulbs, the strip's cover
	Count
};

enum class EDeepRoomLightKind : uint8
{
	Practical,  // warm amber point light
	Emergency,  // red point light
	Strip       // cool low rect light
};

struct FDeepRoomPiece
{
	EDeepRoomMesh Mesh = EDeepRoomMesh::Cube;
	FVector Centre = FVector::ZeroVector;  // sub-local cm
	FVector Size = FVector(100.0);          // cm in the shape's own frame (cylinders: diameter, diameter, length)
	FRotator Rotation = FRotator::ZeroRotator;
	EDeepRoomMat Mat = EDeepRoomMat::Frame;
};

struct FDeepRoomLight
{
	EDeepRoomLightKind Kind = EDeepRoomLightKind::Practical;
	FVector Centre = FVector::ZeroVector;  // sub-local cm
	FRotator Rotation = FRotator::ZeroRotator;  // strips: the way the light faces
	FVector2D SourceCm = FVector2D::ZeroVector;  // strips: width, height
};

namespace DeepRoomDress
{
	/** Every dressing piece for this room. The order and count do not depend on the sizes. */
	DEEP_API TArray<FDeepRoomPiece> Pieces(const FDeepSubTuning& Sub);

	/** The dressing's own lights (the existing HatchRed gets a caged fixture among the pieces, no new light). */
	DEEP_API TArray<FDeepRoomLight> Lights(const FDeepSubTuning& Sub);

	/** The piece's box in sub-local cm. */
	DEEP_API FBox PieceBox(const FDeepRoomPiece& Piece);

	/** The room's inside (cm): deck to ceiling, wall to wall. */
	DEEP_API FBox RoomInside(const FDeepSubTuning& Sub);

	/** The helm seat's centre on the deck plane (cm). */
	DEEP_API FVector2D HelmSeatCm(const FDeepSubTuning& Sub);

	/** The straight walks (cm): every pair of stations (start, enter stand, console, helm), and each station to the hatch. */
	DEEP_API TArray<TPair<FVector2D, FVector2D>> Walks(const FDeepSubTuning& Sub);

	/** True when the box reaches below head height into a walk (walker radius plus the margin). */
	DEEP_API bool BlocksWalk(const FBox& Box, const FDeepSubTuning& Sub, const FDeepRoomDressTuning& Dress, double WalkerRadiusCm);

	/** True when the box reaches below head height into the hatch opening plus its clear margin. */
	DEEP_API bool InHatchZone(const FBox& Box, const FDeepSubTuning& Sub, const FDeepRoomDressTuning& Dress);

	/** True when the box reaches below head height within the helm clear radius of the seat. */
	DEEP_API bool InHelmZone(const FBox& Box, const FDeepSubTuning& Sub, const FDeepRoomDressTuning& Dress);

	/** Constructor side: one ISM per shape and material, one light per Lights entry, instances placed. */
	DEEP_API void Create(AActor* Owner, USceneComponent* Parent, TArray<TObjectPtr<UInstancedStaticMeshComponent>>& OutMeshes,
		TArray<TObjectPtr<ULocalLightComponent>>& OutLights);

	/** Runtime side (BeginPlay): materials from the tuning and the lights' colours; hidden in the grey box or when disabled. */
	DEEP_API void Skin(const TArray<TObjectPtr<UInstancedStaticMeshComponent>>& Meshes, const TArray<TObjectPtr<ULocalLightComponent>>& Lights,
		bool bGreyBox);

	/** Shows or hides the dressing and its lights (`Deep.Room.Dress 0|1`). */
	DEEP_API void SetVisible(const TArray<TObjectPtr<UInstancedStaticMeshComponent>>& Meshes, const TArray<TObjectPtr<ULocalLightComponent>>& Lights,
		bool bVisible);
}
