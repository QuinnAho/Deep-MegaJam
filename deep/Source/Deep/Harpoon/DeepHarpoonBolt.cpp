#include "Harpoon/DeepHarpoonBolt.h"

#include "Components/StaticMeshComponent.h"
#include "Diver/DeepDiver.h"
#include "Harpoon/DeepHarpoonComponent.h"
#include "Harpoon/DeepHarpoonSettings.h"
#include "World/DeepShapes.h"

ADeepHarpoonBolt::ADeepHarpoonBolt()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	bReplicates = true;
	SetReplicateMovement(true);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	// The bolt lies along +X with its tip at the actor origin.
	const float Length = FMath::Max(1.f, UDeepHarpoonSettings::Get().BoltLengthCm);
	Shaft = DeepShapes::CreateShape(this, TEXT("Shaft"), DeepShapes::Cylinder);
	Shaft->SetupAttachment(Root);
	Shaft->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	Shaft->SetRelativeLocation(FVector(-Length * 0.5f, 0.f, 0.f));
	Shaft->SetRelativeScale3D(FVector(0.05f, 0.05f, Length / DeepShapes::ShapeSizeCm));
	Shaft->SetCastShadow(false);

	Tip = DeepShapes::CreateShape(this, TEXT("Tip"), DeepShapes::Cone);
	Tip->SetupAttachment(Root);
	Tip->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	Tip->SetRelativeLocation(FVector(-6.f, 0.f, 0.f));
	Tip->SetRelativeScale3D(FVector(0.12f, 0.12f, 0.16f));
	Tip->SetCastShadow(false);

	Line = DeepShapes::CreateShape(this, TEXT("Line"), DeepShapes::Cylinder);
	Line->SetupAttachment(Root);
	Line->SetUsingAbsoluteLocation(true);
	Line->SetUsingAbsoluteRotation(true);
	Line->SetUsingAbsoluteScale(true);
	Line->SetCastShadow(false);
	Line->SetVisibility(false);
	// Like the tether: lit by the scene's key and fill only, not the helmet lamp right beside it (first shot: it glowed).
	Line->SetLightingChannels(false, true, false);
}

void ADeepHarpoonBolt::BeginPlay()
{
	Super::BeginPlay();
	// Bright on purpose: a pale steel line (the tether is dull yellow) and a red bolt that reads in the dark.
	DeepShapes::Tint(Shaft, FLinearColor(0.9f, 0.15f, 0.1f));
	DeepShapes::Tint(Tip, FLinearColor(1.f, 0.3f, 0.1f));
	DeepShapes::Tint(Line, FLinearColor(0.7f, 0.78f, 0.85f));
}

void ADeepHarpoonBolt::PlaceBolt(const FVector& TipCm, const FVector& Direction)
{
	SetActorLocationAndRotation(TipCm, Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector).Rotation());
	UpdateLine();
}

void ADeepHarpoonBolt::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateLine();
}

void ADeepHarpoonBolt::UpdateLine()
{
	const ADeepDiver* Diver = Cast<ADeepDiver>(GetOwner());
	if (!Line || !Diver)
	{
		if (Line)
		{
			Line->SetVisibility(false);
		}
		return;
	}
	const FVector Start = UDeepHarpoonComponent::GunLocation(Diver);
	const FVector End = GetActorLocation() - GetActorForwardVector() * UDeepHarpoonSettings::Get().BoltLengthCm;
	const FVector Span = End - Start;
	const float Length = static_cast<float>(Span.Size());
	if (Length < 1.f)
	{
		Line->SetVisibility(false);
		return;
	}
	const float Thickness = UDeepHarpoonSettings::Get().LineThicknessCm / DeepShapes::ShapeSizeCm;
	Line->SetVisibility(true);
	Line->SetWorldLocationAndRotation((Start + End) * 0.5, FRotationMatrix::MakeFromZ(Span / Length).Rotator());
	Line->SetWorldScale3D(FVector(Thickness, Thickness, Length / DeepShapes::ShapeSizeCm));
}
