// A per-mesh collision cook key for procedural meshes (the cave shells, the shaft's rock, the sea bed).
#pragma once

#include "CoreMinimal.h"

class UProceduralMeshComponent;

/**
 * UE 5.9 Preview keys a body's collision cook (UBodySetup::GetGeometryDDCKey) on its simple geometry and flags plus the
 * outer's mesh id, and UProceduralMeshComponent has no mesh id. So in an editor build every complex-only procedural
 * mesh shares one derived-data entry and can come back with another mesh's rock (found by the cavern lane: stop 2's
 * shell had stop 1's rock; the shaft's long stretches answered no traces). A 1 cm convex tetrahedron placed by a hash
 * of the mesh is part of the key, so each mesh gets its own entry. With complex-as-simple collision it makes no shape
 * (ChaosInterfaceUtils skips simple geometry when a triangle mesh exists); keep it out of reach anyway.
 */
namespace DeepCookKey
{
	/** Hash of a mesh's vertices and triangles (chain several sections through Seed). */
	DEEP_API uint32 Hash(TConstArrayView<FVector> Vertices, TConstArrayView<int32> Triangles, uint32 Seed = 0);

	/** The tetrahedron: corner at CornerCm (component space), nudged by the hash. */
	DEEP_API TArray<FVector> TetraCm(const FVector& CornerCm, uint32 MeshHash);

	/** Sets the tetrahedron as the mesh's only convex element. Call before CreateMeshSection. */
	DEEP_API void Apply(UProceduralMeshComponent* Mesh, const FVector& CornerCm, uint32 MeshHash);
}
