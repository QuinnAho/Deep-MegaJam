#include "World/DeepSalvage.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepShapes.h"

ADeepSalvage::ADeepSalvage()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
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

	// The relic: built from basic shapes so it reads as obviously special from far away.
	RelicPivot = CreateDefaultSubobject<USceneComponent>(TEXT("RelicPivot"));
	RelicPivot->SetupAttachment(Root);
	RelicPivot->SetMobility(EComponentMobility::Movable);
	const TCHAR* PartShapes[] = { DeepShapes::Cone, DeepShapes::Cone, DeepShapes::Cube, DeepShapes::Cube, DeepShapes::Cube, DeepShapes::Cube };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(PartShapes); ++Index)
	{
		UStaticMeshComponent* Part = DeepShapes::CreateShape(this, *FString::Printf(TEXT("RelicPart%d"), Index), PartShapes[Index]);
		Part->SetupAttachment(RelicPivot);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetVisibility(false);
		RelicParts.Add(Part);
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
	// Pearls are an orb; the relic shows a small pale orb as its core inside the gold shapes.
	const bool bOrb = Kind.Size == EDeepSalvageSize::Pearl || IsRelic();
	UStaticMeshComponent* Shown = bOrb ? Orb.Get() : Cube.Get();
	UStaticMeshComponent* Hidden = bOrb ? Cube.Get() : Orb.Get();

	Shown->SetVisibility(true);
	Shown->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Hidden->SetVisibility(false);
	Hidden->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	const float Scale = FMath::Max(0.05f, Kind.SizeM * UDeepRules::CmPerM / DeepShapes::ShapeSizeCm) * (IsRelic() ? 0.7f : 1.f);
	// Scrap is a little flattened and tilted so it reads as debris, not a crate.
	Shown->SetRelativeScale3D(bOrb ? FVector(Scale) : FVector(Scale * 1.3f, Scale, Scale * 0.6f));
	Shown->SetRelativeRotation(bOrb ? FRotator::ZeroRotator : FRotator(12.f, 30.f, -8.f));

	DeepShapes::Tint(Shown, IsRelic() ? FLinearColor(0.85f, 0.95f, 1.f) : Kind.Color);

	// The relic: two gold cones point to point form a tall crystal around the core; four shards circle it.
	const float Size = Kind.SizeM * UDeepRules::CmPerM / DeepShapes::ShapeSizeCm;
	for (int32 Index = 0; Index < RelicParts.Num(); ++Index)
	{
		UStaticMeshComponent* Part = RelicParts[Index];
		Part->SetVisibility(IsRelic());
		if (!IsRelic())
		{
			continue;
		}
		if (Index < 2)
		{
			// Cone pivots sit at their centre; the top cone points up, the bottom one down.
			const float Sign = Index == 0 ? 1.f : -1.f;
			Part->SetRelativeLocation(FVector(0.f, 0.f, Sign * Size * 0.5f * DeepShapes::ShapeSizeCm));
			Part->SetRelativeRotation(Index == 0 ? FRotator::ZeroRotator : FRotator(180.f, 0.f, 0.f));
			Part->SetRelativeScale3D(FVector(Size * 0.55f, Size * 0.55f, Size));
		}
		else
		{
			const float Angle = (Index - 2) * 90.f;
			const FVector Out = FRotator(0.f, Angle, 0.f).Vector() * Size * 0.75f * DeepShapes::ShapeSizeCm;
			Part->SetRelativeLocation(Out);
			Part->SetRelativeRotation(FRotator(45.f, Angle, 45.f));
			Part->SetRelativeScale3D(FVector(Size * 0.16f));
		}
		DeepShapes::Tint(Part, Index < 2 ? Kind.Color : FLinearColor(0.2f, 0.9f, 0.85f));
	}
	SetActorTickEnabled(IsRelic());

	const FDeepArenaTuning& Arena = UDeepTuningSettings::Get().Arena;
	Glint->SetRelativeLocation(FVector(0.f, 0.f, Kind.SizeM * UDeepRules::CmPerM));
	Glint->SetIntensity(IsRelic() ? Arena.RelicLightIntensity : Arena.SalvageGlintIntensity * GlintScale);
	Glint->SetAttenuationRadius((IsRelic() ? Arena.RelicLightRadiusM : Arena.SalvageGlintRadiusM) * UDeepRules::CmPerM);
	Glint->SetLightColor(Kind.Color);
}

void ADeepSalvage::SetGlintScale(float Scale)
{
	if (FMath::IsNearlyEqual(GlintScale, Scale))
	{
		return;
	}
	GlintScale = FMath::Max(0.f, Scale);
	if (!IsRelic())
	{
		Glint->SetIntensity(UDeepTuningSettings::Get().Arena.SalvageGlintIntensity * GlintScale);
	}
}

void ADeepSalvage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The relic turns slowly and bobs, so it reads as alive among the dead scrap.
	if (IsRelic() && RelicPivot)
	{
		RelicPivot->AddLocalRotation(FRotator(0.f, 40.f * DeltaSeconds, 0.f));
		const float Bob = FMath::Sin(GetWorld()->GetTimeSeconds() * 1.5f) * 12.f;
		RelicPivot->SetRelativeLocation(FVector(0.f, 0.f, Bob));
	}
}

void ADeepSalvage::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADeepSalvage, Kind);
}
