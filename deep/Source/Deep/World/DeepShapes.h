// Engine basic shapes as stand-in art. No project assets.

#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;

namespace DeepShapes
{
	inline const TCHAR* Cube = TEXT("/Engine/BasicShapes/Cube.Cube");
	inline const TCHAR* Sphere = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	inline const TCHAR* Cylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	inline const TCHAR* Cone = TEXT("/Engine/BasicShapes/Cone.Cone");

	/** Basic shapes are 100 cm across, centred on their pivot. */
	constexpr float ShapeSizeCm = 100.f;

	/** Loads a basic-shape mesh. Safe in constructors and at runtime. */
	DEEP_API UStaticMesh* LoadMesh(const TCHAR* Path);

	/** Creates a mesh subobject in a constructor, with no collision unless told otherwise. */
	DEEP_API UStaticMeshComponent* CreateShape(UObject* Outer, FName Name, const TCHAR* MeshPath);

	/** Gives the component a tinted dynamic instance of the basic shape material. Call at runtime (BeginPlay or later). */
	DEEP_API void Tint(UStaticMeshComponent* Component, const FLinearColor& Color);

	/** Worn painted steel (FDeepLookTuning::SteelMaterial) on any mesh: paint chipped by Wear (0..1) to bare metal, Rust (0..1) streaks. Falls back to Tint. Runtime only. */
	DEEP_API void Steel(UStaticMeshComponent* Component, const FLinearColor& Paint, float Wear, float Rust, float PaintRough, float PaintMetal);
}
