#include "World/DeepCookKey.h"

#include "Misc/Crc.h"
#include "ProceduralMeshComponent.h"

uint32 DeepCookKey::Hash(TConstArrayView<FVector> Vertices, TConstArrayView<int32> Triangles, uint32 Seed)
{
	uint32 Out = FCrc::MemCrc32(Vertices.GetData(), Vertices.Num() * sizeof(FVector), Seed);
	return FCrc::MemCrc32(Triangles.GetData(), Triangles.Num() * sizeof(int32), Out);
}

TArray<FVector> DeepCookKey::TetraCm(const FVector& CornerCm, uint32 MeshHash)
{
	const FVector Corner = CornerCm + FVector(double(MeshHash & 0xFFFF), double(MeshHash >> 16), 0.0) * 1e-4;
	return { Corner, Corner + FVector(1.0, 0.0, 0.0), Corner + FVector(0.0, 1.0, 0.0), Corner + FVector(0.0, 0.0, 1.0) };
}

void DeepCookKey::Apply(UProceduralMeshComponent* Mesh, const FVector& CornerCm, uint32 MeshHash)
{
	if (Mesh)
	{
		Mesh->SetCollisionConvexMeshes({ TetraCm(CornerCm, MeshHash) });
	}
}
