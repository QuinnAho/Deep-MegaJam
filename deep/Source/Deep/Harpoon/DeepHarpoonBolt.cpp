#include "Harpoon/DeepHarpoonBolt.h"

#include "Components/StaticMeshComponent.h"
#include "Diver/DeepDiver.h"
#include "Harpoon/DeepHarpoonComponent.h"
#include "Harpoon/DeepHarpoonSettings.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepRopeComponent.h"
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

	// The line is the tether's rope, thinner (World/DeepRopeComponent), driven from the gun to the bolt's tail.
	Line = CreateDefaultSubobject<UDeepRopeComponent>(TEXT("Line"));
	Line->SetupAttachment(Root);
}

void ADeepHarpoonBolt::BeginPlay()
{
	Super::BeginPlay();
	// A red bolt that reads in the dark; the line is a dull cord like the tether, seen where light lands on it.
	DeepShapes::Tint(Shaft, FLinearColor(0.9f, 0.15f, 0.1f));
	DeepShapes::Tint(Tip, FLinearColor(1.f, 0.3f, 0.1f));
	const FDeepRopeTuning& Rope = UDeepTuningSettings::Get().Rope;
	Line->Configure(Rope.HarpoonPoints, UDeepHarpoonSettings::Get().LineThicknessCm, Rope.HarpoonColor, false);
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
			Line->HideRope();
		}
		return;
	}
	const FVector Start = UDeepHarpoonComponent::GunLocation(Diver);
	const FVector End = GetActorLocation() - GetActorForwardVector() * UDeepHarpoonSettings::Get().BoltLengthCm;
	const float Length = static_cast<float>(FVector::Dist(Start, End));
	if (Length < 1.f)
	{
		Line->HideRope();
		return;
	}
	// Paid out a little longer than the straight line, so it hangs in a shallow curve and trails the bolt in flight.
	const FDeepRopeTuning& Rope = UDeepTuningSettings::Get().Rope;
	const UWorld* World = GetWorld();
	const float DeltaSeconds = (World && LastLineFrame != GFrameCounter) ? World->GetDeltaSeconds() : 0.f;
	LastLineFrame = GFrameCounter;
	Line->Drive(DeltaSeconds, &Start, &End, Length * (1.f + Rope.SlackShare * 0.5f), Rope.SinkCmS2);
}
