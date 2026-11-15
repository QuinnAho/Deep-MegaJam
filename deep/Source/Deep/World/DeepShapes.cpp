#include "World/DeepShapes.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sim/DeepTuningSettings.h"

namespace DeepShapes
{
	static const TCHAR* BasicMaterial = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	UStaticMesh* LoadMesh(const TCHAR* Path)
	{
		return LoadObject<UStaticMesh>(nullptr, Path);
	}

	UStaticMeshComponent* CreateShape(UObject* Outer, FName Name, const TCHAR* MeshPath)
	{
		UStaticMeshComponent* Component = Outer->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Component->SetStaticMesh(LoadMesh(MeshPath));
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetMobility(EComponentMobility::Movable);
		return Component;
	}

	void Tint(UStaticMeshComponent* Component, const FLinearColor& Color)
	{
		if (!Component)
		{
			return;
		}
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, BasicMaterial);
		if (!Base)
		{
			return;
		}
		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Base, Component);
		Instance->SetVectorParameterValue(TEXT("Color"), Color);
		Component->SetMaterial(0, Instance);
	}

	void Steel(UStaticMeshComponent* Component, const FLinearColor& Paint, float Wear, float Rust, float PaintRough, float PaintMetal)
	{
		UMaterialInterface* Base = Component ? Cast<UMaterialInterface>(UDeepTuningSettings::Get().Look.SteelMaterial.TryLoad()) : nullptr;
		if (!Base)
		{
			Tint(Component, Paint);
			return;
		}
		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Base, Component);
		Instance->SetVectorParameterValue(TEXT("PaintColor"), Paint);
		Instance->SetScalarParameterValue(TEXT("Wear"), Wear);
		Instance->SetScalarParameterValue(TEXT("Rust"), Rust);
		Instance->SetScalarParameterValue(TEXT("PaintRoughness"), PaintRough);
		Instance->SetScalarParameterValue(TEXT("PaintMetal"), PaintMetal);
		Component->SetMaterial(0, Instance);
	}
}
