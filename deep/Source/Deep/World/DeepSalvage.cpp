#include "World/DeepSalvage.h"

#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "World/DeepDressing.h"
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
	// Scrap shapes: two cylinders and two boxes, laid out per size in ApplyScrapShape. Look only.
	const TCHAR* ScrapShapes[] = { DeepShapes::Cylinder, DeepShapes::Cylinder, DeepShapes::Cube, DeepShapes::Cube };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ScrapShapes); ++Index)
	{
		UStaticMeshComponent* Part = DeepShapes::CreateShape(this, *FString::Printf(TEXT("ScrapPart%d"), Index), ScrapShapes[Index]);
		Part->SetupAttachment(Root);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetVisibility(false);
		ScrapParts.Add(Part);
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

	RelicMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RelicMesh"));
	RelicMesh->SetupAttachment(RelicPivot);
	RelicMesh->SetMobility(EComponentMobility::Movable);
	RelicMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RelicMesh->SetVisibility(false);

	// A spot light's beam is its +X: pitch 90 points it straight up the shaft.
	Beacon = CreateDefaultSubobject<USpotLightComponent>(TEXT("Beacon"));
	Beacon->SetupAttachment(Root);
	Beacon->SetMobility(EComponentMobility::Movable);
	Beacon->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	Beacon->SetCastShadows(true);
	Beacon->SetCastVolumetricShadow(true);
	Beacon->SetVisibility(false);
}

bool ADeepSalvage::ApplyRelicMesh()
{
	const FDeepArenaTuning& Arena = UDeepTuningSettings::Get().Arena;
	UStaticMesh* Mesh = IsRelic() && !Arena.RelicMesh.IsNull() ? Cast<UStaticMesh>(Arena.RelicMesh.TryLoad()) : nullptr;
	RelicMesh->SetVisibility(Mesh != nullptr);
	if (!Mesh)
	{
		return false;
	}
	if (RelicMesh->GetStaticMesh() != Mesh)
	{
		RelicMesh->SetStaticMesh(Mesh);
		UMaterialInterface* Skin = Arena.RelicMaterial.IsNull() ? nullptr : Cast<UMaterialInterface>(Arena.RelicMaterial.TryLoad());
		UMaterialInterface* Textures = Arena.RelicTextures.IsNull() ? nullptr : Cast<UMaterialInterface>(Arena.RelicTextures.TryLoad());
		for (int32 Slot = 0; Skin && Slot < RelicMesh->GetNumMaterials(); ++Slot)
		{
			// Little silt: it was handled, it is the one thing down here someone kept clean. Its bronze keeps some colour.
			RelicMesh->SetMaterial(Slot, DeepDressing::Skin(Skin, Textures ? Textures : Mesh->GetMaterial(Slot), 0.1f, this, 0.8f));
		}
	}
	// Fit the mesh's largest side to the relic's size, centred on the pivot (the grab collider is the hidden orb).
	const FBox Box = Mesh->GetBoundingBox();
	const double Fit = Kind.SizeM * UDeepRules::CmPerM / FMath::Max(1.0, Box.GetSize().GetMax());
	RelicMesh->SetRelativeScale3D(FVector(Fit));
	// It rests on the bed: the bed passes (half height - sink) under the pivot (ADeepAbyssFloor::RestPointM), and the
	// mesh settles 8 cm into the silt.
	const FDeepLookTuning& Look = UDeepTuningSettings::Get().Look;
	const double HalfCm = Kind.SizeM * 1.1 * UDeepRules::CmPerM;
	const double BedCm = -(HalfCm - 2.0 * HalfCm * FMath::Clamp(Look.SeabedSinkFraction, 0.f, 0.5f));
	RelicMesh->SetRelativeLocation(FVector(-Box.GetCenter().X * Fit, -Box.GetCenter().Y * Fit, BedCm - Box.Min.Z * Fit - 8.0));
	RelicPivot->SetRelativeLocation(FVector::ZeroVector);
	return true;
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

	if (IsRelic() || Kind.Size == EDeepSalvageSize::Pearl)
	{
		// The relic's core and pearls keep their pale tint: the one thing down here that is not metal.
		DeepShapes::Tint(Shown, IsRelic() ? FLinearColor(0.55f, 0.6f, 0.62f) : Kind.Color);
		for (UStaticMeshComponent* Part : ScrapParts)
		{
			Part->SetVisibility(false);
		}
	}
	else
	{
		// Scrap: the kind's colour graded to style guide 4 (saturation and value capped), rusted and dull, so the lamp
		// lands on it without glare.
		const FLinearColor Hsv = Kind.Color.LinearRGBToHSV();
		const FLinearColor Paint = FLinearColor(Hsv.R, FMath::Min(Hsv.G, 0.3f), FMath::Min(Hsv.B, 1.f) * 0.14f + 0.02f).HSVToLinearRGB();
		DeepShapes::Steel(Shown, Paint, 0.45f, 0.6f, 0.7f, 0.f);
		// The box stays as the grab collider; what shows is a pipe, a bent plate or a tank.
		if (ApplyScrapShape(Paint))
		{
			Shown->SetVisibility(false);
		}
	}

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
		// Old dark bronze, polished where it was handled, little rust: metal that was made, among metal that was thrown away.
		DeepShapes::Steel(Part, Index < 2 ? FLinearColor(0.22f, 0.15f, 0.07f) : FLinearColor(0.16f, 0.12f, 0.07f), 0.12f, 0.08f, 0.32f, 1.f);
	}
	SetActorTickEnabled(IsRelic());

	// The pack relic replaces the shapes; the orb stays as the (hidden) grab collider.
	if (ApplyRelicMesh())
	{
		Shown->SetVisibility(false);
		for (UStaticMeshComponent* Part : RelicParts)
		{
			Part->SetVisibility(false);
		}
	}

	const FDeepArenaTuning& Arena = UDeepTuningSettings::Get().Arena;
	Glint->SetRelativeLocation(FVector(0.f, 0.f, Kind.SizeM * UDeepRules::CmPerM));
	Glint->SetIntensity(IsRelic() ? Arena.RelicLightIntensity : Arena.SalvageGlintIntensity * GlintScale);
	Glint->SetAttenuationRadius((IsRelic() ? Arena.RelicLightRadiusM : Arena.SalvageGlintRadiusM) * UDeepRules::CmPerM);
	Glint->SetLightColor(IsRelic() ? Arena.RelicLightColor : Kind.Color);

	Beacon->SetVisibility(IsRelic() && Arena.RelicBeaconIntensity > 0.f);
	if (IsRelic())
	{
		Beacon->SetRelativeLocation(FVector(0.f, 0.f, Kind.SizeM * UDeepRules::CmPerM * 0.6f));
		Beacon->SetIntensity(Arena.RelicBeaconIntensity);
		Beacon->SetInnerConeAngle(Arena.RelicBeaconConeDeg * 0.4f);
		Beacon->SetOuterConeAngle(Arena.RelicBeaconConeDeg);
		Beacon->SetAttenuationRadius(Arena.RelicBeaconRangeM * UDeepRules::CmPerM);
		Beacon->SetVolumetricScatteringIntensity(Arena.RelicBeaconScattering);
		Beacon->SetLightColor(Arena.RelicLightColor);
	}
}

bool ADeepSalvage::ApplyScrapShape(const FLinearColor& Paint)
{
	if (ScrapParts.Num() < 4)
	{
		return false;
	}
	// Sizes in units of the kind's size (S), laid out round the pivot, with the lowest point near -0.3 S like the old box
	// (the bed passes under the pivot there). One yaw for the whole piece so the ring of salvage is not all aligned.
	const float S = Kind.SizeM * UDeepRules::CmPerM;
	const FRotator Base(0.f, 30.f + 47.f * static_cast<float>(static_cast<uint8>(Kind.Size)), 0.f);
	auto Put = [&](int32 Index, const FVector& Loc, const FRotator& Rot, const FVector& SizeInS, float Wear, float Rust)
	{
		UStaticMeshComponent* Part = ScrapParts[Index];
		Part->SetRelativeLocation(Base.RotateVector(Loc * S));
		Part->SetRelativeRotation((Base.Quaternion() * Rot.Quaternion()).Rotator());
		Part->SetRelativeScale3D(SizeInS * (S / DeepShapes::ShapeSizeCm));
		Part->SetVisibility(true);
		DeepShapes::Steel(Part, Paint, Wear, Rust, 0.7f, 0.f);
	};
	// Cylinders stand on Z: pitch 90 lays one along X, roll 90 along Y. Their scale is (diameter, diameter, length).
	const FRotator AlongX(90.f, 0.f, 0.f);
	const FRotator AlongY(0.f, 0.f, 90.f);
	switch (Kind.Size)
	{
	case EDeepSalvageSize::Small:
		// A length of pipe with a flange at one end and a torn bracket.
		Put(0, FVector(0.f, 0.f, -0.14f), AlongX, FVector(0.32f, 0.32f, 1.4f), 0.5f, 0.75f);
		Put(1, FVector(0.62f, 0.f, -0.14f), AlongX, FVector(0.56f, 0.56f, 0.08f), 0.35f, 0.6f);
		Put(2, FVector(-0.3f, 0.08f, 0.04f), FRotator(0.f, 0.f, 25.f), FVector(0.28f, 0.06f, 0.24f), 0.6f, 0.5f);
		ScrapParts[3]->SetVisibility(false);
		return true;
	case EDeepSalvageSize::Medium:
		// A bent hull plate, the bend up off the bed, a pipe run across it.
		Put(2, FVector(-0.2f, 0.f, -0.27f), FRotator(4.f, 0.f, 2.f), FVector(1.1f, 0.85f, 0.06f), 0.55f, 0.6f);
		Put(3, FVector(0.56f, 0.f, -0.08f), FRotator(-38.f, 0.f, 2.f), FVector(0.55f, 0.85f, 0.06f), 0.55f, 0.6f);
		Put(0, FVector(-0.15f, 0.f, -0.17f), AlongY, FVector(0.13f, 0.13f, 1.05f), 0.4f, 0.8f);
		Put(1, FVector(-0.15f, 0.47f, -0.17f), AlongY, FVector(0.22f, 0.22f, 0.06f), 0.35f, 0.7f);
		return true;
	case EDeepSalvageSize::Large:
		// A gas tank on its side, a band round it, a valve block on top and a skid under one end.
		Put(0, FVector(0.f, 0.f, -0.03f), AlongX, FVector(0.54f, 0.54f, 1.3f), 0.45f, 0.65f);
		Put(1, FVector(0.3f, 0.f, -0.03f), AlongX, FVector(0.59f, 0.59f, 0.09f), 0.6f, 0.5f);
		Put(2, FVector(-0.45f, 0.f, 0.3f), FRotator(0.f, 15.f, 0.f), FVector(0.16f, 0.16f, 0.14f), 0.3f, 0.4f);
		Put(3, FVector(-0.42f, 0.f, -0.27f), FRotator::ZeroRotator, FVector(0.14f, 0.62f, 0.08f), 0.5f, 0.7f);
		return true;
	default:
		for (UStaticMeshComponent* Part : ScrapParts)
		{
			Part->SetVisibility(false);
		}
		return false;
	}
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
	// The shape-built relic turns slowly and bobs, so it reads as alive among the dead scrap. The pack relic lies still.
	if (IsRelic() && RelicPivot && !(RelicMesh && RelicMesh->IsVisible()))
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
