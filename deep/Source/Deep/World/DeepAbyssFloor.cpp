#include "World/DeepAbyssFloor.h"

#include "Components/StaticMeshComponent.h"
#include "Sim/DeepRules.h"
#include "World/DeepShapes.h"

ADeepAbyssFloor::ADeepAbyssFloor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	Slab = DeepShapes::CreateShape(this, TEXT("Slab"), DeepShapes::Cube);
	Slab->SetupAttachment(Root);
	Slab->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Slab->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	SetSizeM(600.f);
}

void ADeepAbyssFloor::BeginPlay()
{
	Super::BeginPlay();
	DeepShapes::Tint(Slab, FLinearColor(0.03f, 0.035f, 0.04f));
}

void ADeepAbyssFloor::SetSizeM(float SizeM)
{
	const float Scale = FMath::Max(1.f, SizeM) * UDeepRules::CmPerM / DeepShapes::ShapeSizeCm;
	Slab->SetRelativeScale3D(FVector(Scale, Scale, 1.f));
	Slab->SetRelativeLocation(FVector(0.f, 0.f, -DeepShapes::ShapeSizeCm * 0.5f));
}
