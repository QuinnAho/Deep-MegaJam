#include "World/DeepSubmarine.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepShapes.h"

ADeepSubmarine::ADeepSubmarine()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	// Stand-in shapes, sized in metres: a 14 m hull of 4 m diameter lying along +X.
	Hull = DeepShapes::CreateShape(this, TEXT("Hull"), DeepShapes::Cylinder);
	Hull->SetupAttachment(Root);
	Hull->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	Hull->SetRelativeScale3D(FVector(4.f, 4.f, 14.f));
	Hull->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Hull->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	NoseCone = DeepShapes::CreateShape(this, TEXT("NoseCone"), DeepShapes::Sphere);
	NoseCone->SetupAttachment(Root);
	NoseCone->SetRelativeLocation(FVector(700.f, 0.f, 0.f));
	NoseCone->SetRelativeScale3D(FVector(4.f, 4.f, 4.f));

	Tower = DeepShapes::CreateShape(this, TEXT("Tower"), DeepShapes::Cube);
	Tower->SetupAttachment(Root);
	Tower->SetRelativeLocation(FVector(100.f, 0.f, 280.f));
	Tower->SetRelativeScale3D(FVector(3.f, 1.4f, 2.4f));
	Tower->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Tower->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	Airlock = DeepShapes::CreateShape(this, TEXT("Airlock"), DeepShapes::Cylinder);
	Airlock->SetupAttachment(Root);
	Airlock->SetRelativeLocation(FVector(0.f, 0.f, -210.f));
	Airlock->SetRelativeScale3D(FVector(1.8f, 1.8f, 0.5f));

	AirlockLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("AirlockLight"));
	AirlockLight->SetupAttachment(Root);
	AirlockLight->SetRelativeLocation(FVector(0.f, 0.f, -350.f));
	AirlockLight->SetMobility(EComponentMobility::Movable);
	AirlockLight->SetLightColor(FLinearColor(1.f, 0.75f, 0.45f));
	AirlockLight->SetCastShadows(false);
}

void ADeepSubmarine::BeginPlay()
{
	Super::BeginPlay();

	const FDeepArenaTuning& Arena = UDeepTuningSettings::Get().Arena;
	AirlockLight->SetIntensity(Arena.SubLightIntensity);
	AirlockLight->SetAttenuationRadius(Arena.SubLightRadiusM * UDeepRules::CmPerM);

	DeepShapes::Tint(Hull, FLinearColor(0.85f, 0.55f, 0.1f));
	DeepShapes::Tint(NoseCone, FLinearColor(0.85f, 0.55f, 0.1f));
	DeepShapes::Tint(Tower, FLinearColor(0.2f, 0.2f, 0.22f));
	DeepShapes::Tint(Airlock, FLinearColor(0.1f, 0.1f, 0.1f));
}

FVector ADeepSubmarine::GetAnchorLocation() const
{
	return GetActorLocation() - FVector(0.f, 0.f, UDeepTuningSettings::Get().Arena.AnchorBelowSubM * UDeepRules::CmPerM);
}

FTransform ADeepSubmarine::GetDiverStartTransform() const
{
	const FDeepArenaTuning& Arena = UDeepTuningSettings::Get().Arena;
	const FRotator Facing(0.f, GetActorRotation().Yaw, 0.f);
	const FVector Start = GetAnchorLocation()
		- FVector(0.f, 0.f, Arena.DiverStartBelowAnchorM * UDeepRules::CmPerM)
		- Facing.Vector() * (Arena.DiverStartBackM * UDeepRules::CmPerM);
	return FTransform(Facing, Start);
}
