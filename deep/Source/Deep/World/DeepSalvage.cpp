#include "World/DeepSalvage.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepShapes.h"

ADeepSalvage::ADeepSalvage()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	// Scrap is a box; pearls are an orb. Only one is visible. Queryable by traces, never blocks the diver.
	Cube = DeepShapes::CreateShape(this, TEXT("Cube"), DeepShapes::Cube);
	Cube->SetupAttachment(Root);
	Orb = DeepShapes::CreateShape(this, TEXT("Orb"), DeepShapes::Sphere);
	Orb->SetupAttachment(Root);
	for (UStaticMeshComponent* Mesh : { Cube.Get(), Orb.Get() })
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}

	Glint = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glint"));
	Glint->SetupAttachment(Root);
	Glint->SetMobility(EComponentMobility::Movable);
	Glint->SetCastShadows(false);
}

void ADeepSalvage::BeginPlay()
{
	Super::BeginPlay();
	ApplyKind();
}

void ADeepSalvage::SetKind(const FDeepSalvageKind& InKind)
{
	Kind = InKind;
	ApplyKind();
}

FDeepCargoItem ADeepSalvage::ToCargo() const
{
	FDeepCargoItem Item;
	Item.Size = Kind.Size;
	Item.Kg = Kind.Kg;
	Item.Value = Kind.Value;
	return Item;
}

void ADeepSalvage::OnRep_Kind()
{
	ApplyKind();
}

void ADeepSalvage::ApplyKind()
{
	const bool bOrb = Kind.Size == EDeepSalvageSize::Pearl;
	UStaticMeshComponent* Shown = bOrb ? Orb.Get() : Cube.Get();
	UStaticMeshComponent* Hidden = bOrb ? Cube.Get() : Orb.Get();

	Shown->SetVisibility(true);
	Shown->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Hidden->SetVisibility(false);
	Hidden->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	const float Scale = FMath::Max(0.05f, Kind.SizeM * UDeepRules::CmPerM / DeepShapes::ShapeSizeCm);
	// Scrap is a little flattened and tilted so it reads as debris, not a crate.
	Shown->SetRelativeScale3D(bOrb ? FVector(Scale) : FVector(Scale * 1.3f, Scale, Scale * 0.6f));
	Shown->SetRelativeRotation(bOrb ? FRotator::ZeroRotator : FRotator(12.f, 30.f, -8.f));

	DeepShapes::Tint(Shown, Kind.Color);

	const FDeepArenaTuning& Arena = UDeepTuningSettings::Get().Arena;
	Glint->SetRelativeLocation(FVector(0.f, 0.f, Kind.SizeM * UDeepRules::CmPerM));
	Glint->SetIntensity(Arena.SalvageGlintIntensity);
	Glint->SetAttenuationRadius(Arena.SalvageGlintRadiusM * UDeepRules::CmPerM);
	Glint->SetLightColor(Kind.Color);
}

void ADeepSalvage::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADeepSalvage, Kind);
}
