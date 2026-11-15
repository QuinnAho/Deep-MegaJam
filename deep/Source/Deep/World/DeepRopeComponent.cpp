#include "World/DeepRopeComponent.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepShapes.h"

UDeepRopeComponent::UDeepRopeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
}

void UDeepRopeComponent::Configure(int32 InPoints, float InThicknessCm, const FLinearColor& InColor, bool bInCastShadow)
{
	InPoints = FMath::Clamp(InPoints, 2, 128);
	if (InPoints != Points)
	{
		bRestart = true;
	}
	Points = InPoints;
	ThicknessCm = FMath::Max(0.1f, InThicknessCm);
	Color = InColor;
	bCastShadow = bInCastShadow;
	// Rebuild the segments with the new look next time.
	for (UStaticMeshComponent* Segment : Segments)
	{
		if (Segment)
		{
			Segment->DestroyComponent();
		}
	}
	Segments.Reset();
}

void UDeepRopeComponent::EnsureSegments()
{
	AActor* Owner = GetOwner();
	const int32 Want = Points - 1;
	if (!Owner || !GetWorld() || Segments.Num() == Want)
	{
		return;
	}
	UStaticMesh* Mesh = DeepShapes::LoadMesh(DeepShapes::Cylinder);
	UMaterialInterface* Shared = nullptr;
	for (int32 i = Segments.Num(); i < Want; ++i)
	{
		UStaticMeshComponent* Segment = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
		Segment->SetStaticMesh(Mesh);
		Segment->SetMobility(EComponentMobility::Movable);
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Segment->SetGenerateOverlapEvents(false);
		Segment->SetCanEverAffectNavigation(false);
		Segment->SetUsingAbsoluteLocation(true);
		Segment->SetUsingAbsoluteRotation(true);
		Segment->SetUsingAbsoluteScale(true);
		Segment->SetCastShadow(bCastShadow);
		Segment->SetupAttachment(this);
		Segment->SetVisibility(false);
		Segment->RegisterComponent();
		// One tinted instance shared by every segment of this rope.
		if (!Shared)
		{
			DeepShapes::Tint(Segment, Color);
			Shared = Segment->GetMaterial(0);
		}
		else
		{
			Segment->SetMaterial(0, Shared);
		}
		Segments.Add(Segment);
	}
}

void UDeepRopeComponent::Drive(float DeltaSeconds, const FVector* StartPin, const FVector* EndPin, float LengthCm, float SinkCmS2)
{
	EnsureSegments();
	const FDeepRopeTuning& Tuning = UDeepTuningSettings::Get().Rope;
	const double SnapCm = Tuning.SnapResetM * 100.0;
	const bool bJumped = (StartPin && FVector::Dist(*StartPin, LastStartPin) > SnapCm) || (EndPin && FVector::Dist(*EndPin, LastEndPin) > SnapCm);
	const bool bWhole = !IsCut();
	if (!Sim.IsValid() || Sim.Pos.Num() != Points || bRestart || (bJumped && bWhole))
	{
		const FVector A = StartPin ? *StartPin : (EndPin ? *EndPin : GetComponentLocation());
		const FVector B = EndPin ? *EndPin : A;
		Sim.Init(Points, A, B);
		bRestart = false;
	}
	if (StartPin)
	{
		LastStartPin = *StartPin;
	}
	if (EndPin)
	{
		LastEndPin = *EndPin;
	}
	Sim.BendStiffness = FMath::Clamp(Tuning.BendStiffness, 0.f, 1.f);
	Sim.Advance(DeltaSeconds, StartPin, EndPin, LengthCm, SinkCmS2, Tuning.DragPerS, Tuning.SolverIterations);
	LaySegments();
}

void UDeepRopeComponent::LaySegments()
{
	bShown = true;
	const float Radius01 = ThicknessCm / DeepShapes::ShapeSizeCm;
	for (int32 i = 0; i < Segments.Num(); ++i)
	{
		UStaticMeshComponent* Segment = Segments[i];
		if (!Segment)
		{
			continue;
		}
		if (!Sim.IsLinked(i))
		{
			Segment->SetVisibility(false);
			continue;
		}
		const FVector A = Sim.Pos[i];
		const FVector B = Sim.Pos[i + 1];
		const FVector Span = B - A;
		const double Length = Span.Size();
		if (Length < 0.01)
		{
			Segment->SetVisibility(false);
			continue;
		}
		// Each segment overruns its ends by the rope's thickness, so the joints close on a bend.
		const FTransform Xf(FRotationMatrix::MakeFromZ(Span / Length).ToQuat(), (A + B) * 0.5,
			FVector(Radius01, Radius01, (Length + ThicknessCm) / DeepShapes::ShapeSizeCm));
		Segment->SetWorldTransform(Xf);
		Segment->SetVisibility(true);
	}
}

int32 UDeepRopeComponent::CutNear(const FVector& Point)
{
	const int32 Link = Sim.NearestLink(Point);
	if (Link != INDEX_NONE)
	{
		const FDeepRopeTuning& Tuning = UDeepTuningSettings::Get().Rope;
		Sim.Cut(Link, Tuning.WhipCmS, Tuning.WhipPoints);
	}
	return Link;
}

void UDeepRopeComponent::CutAtStart()
{
	const FDeepRopeTuning& Tuning = UDeepTuningSettings::Get().Rope;
	Sim.Cut(0, Tuning.WhipCmS, Tuning.WhipPoints);
}

void UDeepRopeComponent::HideRope()
{
	if (!bShown)
	{
		return;
	}
	bShown = false;
	for (UStaticMeshComponent* Segment : Segments)
	{
		if (Segment)
		{
			Segment->SetVisibility(false);
		}
	}
}
