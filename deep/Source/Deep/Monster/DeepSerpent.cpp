#include "Monster/DeepSerpent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Monster/DeepMonsterRules.h"
#include "Monster/DeepMonsterSettings.h"
#include "Monster/DeepSerpentController.h"
#include "World/DeepShapes.h"

namespace
{
	using DeepMonster::CmPerM;
	constexpr float TrailStepCm = 10.f;
	const TCHAR* EyeMaterialPath = TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial");

	/** Head centre to the start of the neck, in metres (the head is an egg 1.5x its radius long). */
	float HeadBackM(const FDeepSerpentBodyTuning& Body)
	{
		return Body.HeadRadiusM * 1.2f;
	}

	float SegmentSpacingM(const FDeepSerpentBodyTuning& Body)
	{
		const int32 N = FMath::Max(2, Body.Segments);
		const float BodyM = FMath::Max(0.5f, Body.LengthM - Body.HeadRadiusM * 1.5f);
		return BodyM / N;
	}
}

ADeepSerpent::ADeepSerpent()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);

	// Unreal game AI: our controller possesses it as soon as it is spawned. It steers itself (SwimToward),
	// so the controller's rotation must not override the heading.
	AIControllerClass = ADeepSerpentController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	Head = DeepShapes::CreateShape(this, TEXT("Head"), DeepShapes::Sphere);
	Head->SetupAttachment(Root);

	Jaw = DeepShapes::CreateShape(this, TEXT("Jaw"), DeepShapes::Cone);
	Jaw->SetupAttachment(Root);

	EyeLeft = DeepShapes::CreateShape(this, TEXT("EyeLeft"), DeepShapes::Sphere);
	EyeLeft->SetupAttachment(Root);
	EyeLeft->SetCastShadow(false);
	EyeRight = DeepShapes::CreateShape(this, TEXT("EyeRight"), DeepShapes::Sphere);
	EyeRight->SetupAttachment(Root);
	EyeRight->SetCastShadow(false);

	EyeLightLeft = CreateDefaultSubobject<UPointLightComponent>(TEXT("EyeLightLeft"));
	EyeLightLeft->SetupAttachment(Root);
	EyeLightRight = CreateDefaultSubobject<UPointLightComponent>(TEXT("EyeLightRight"));
	EyeLightRight->SetupAttachment(Root);
	for (UPointLightComponent* Light : { EyeLightLeft.Get(), EyeLightRight.Get() })
	{
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetCastShadows(false);
		Light->SetIntensityUnits(ELightUnits::Unitless);
	}

	Body = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetStaticMesh(DeepShapes::LoadMesh(DeepShapes::Sphere));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetGenerateOverlapEvents(false);
	Body->SetMobility(EComponentMobility::Movable);
	// Instances are given in world space; keep the component at the origin.
	Body->SetUsingAbsoluteLocation(true);
	Body->SetUsingAbsoluteRotation(true);
	Body->SetUsingAbsoluteScale(true);
}

void ADeepSerpent::BeginPlay()
{
	Super::BeginPlay();

	const FDeepSerpentBodyTuning& Tuning = UDeepMonsterSettings::Get().Body;
	const float R = Tuning.HeadRadiusM;

	// Sizes in metres: basic shapes are 1 m across, so scale = metres.
	Head->SetRelativeScale3D(FVector(R * 3.f, R * 2.f, R * 1.6f));
	Jaw->SetRelativeLocation(FVector(R * CmPerM * 0.9f, 0.f, -R * CmPerM * 0.35f));
	Jaw->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	Jaw->SetRelativeScale3D(FVector(R * 1.3f, R * 1.3f, R * 1.4f));

	const FVector EyeOffset(R * CmPerM * 1.05f, R * CmPerM * 0.55f, R * CmPerM * 0.35f);
	// Half the head radius: at 0.32 (first screenshot) the eyes were a faint glint at 7 m.
	const FVector EyeScale(R * 0.5f);
	EyeLeft->SetRelativeLocation(FVector(EyeOffset.X, -EyeOffset.Y, EyeOffset.Z));
	EyeRight->SetRelativeLocation(EyeOffset);
	EyeLeft->SetRelativeScale3D(EyeScale);
	EyeRight->SetRelativeScale3D(EyeScale);

	const FVector LightForward(R * CmPerM * 0.25f, 0.f, 0.f);
	EyeLightLeft->SetRelativeLocation(EyeLeft->GetRelativeLocation() + LightForward);
	EyeLightRight->SetRelativeLocation(EyeRight->GetRelativeLocation() + LightForward);
	for (UPointLightComponent* Light : { EyeLightLeft.Get(), EyeLightRight.Get() })
	{
		Light->SetLightColor(Tuning.EyeColor);
		Light->SetIntensity(Tuning.EyeLightIntensity);
		Light->SetAttenuationRadius(Tuning.EyeLightRadiusM * CmPerM);
	}

	DeepShapes::Tint(Head, Tuning.SkinColor);
	DeepShapes::Tint(Jaw, Tuning.SkinColor * 0.7f);
	DeepShapes::Tint(Body, Tuning.SkinColor);

	// Eyes: the engine's unlit additive material, so they read as points of light in black water.
	if (UMaterialInterface* EyeBase = LoadObject<UMaterialInterface>(nullptr, EyeMaterialPath))
	{
		for (UStaticMeshComponent* Eye : { EyeLeft.Get(), EyeRight.Get() })
		{
			UMaterialInstanceDynamic* Glow = UMaterialInstanceDynamic::Create(EyeBase, Eye);
			Glow->SetVectorParameterValue(TEXT("Color"), Tuning.EyeColor * 8.f);
			Eye->SetMaterial(0, Glow);
		}
	}
	else
	{
		DeepShapes::Tint(EyeLeft, Tuning.EyeColor);
		DeepShapes::Tint(EyeRight, Tuning.EyeColor);
	}

	Heading = GetActorForwardVector();
	PlaceHead(GetActorLocation(), Heading);
}

void ADeepSerpent::PlaceHead(const FVector& HeadCm, const FVector& Facing)
{
	Heading = Facing.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	SetActorLocationAndRotation(HeadCm, Heading.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);

	const FDeepSerpentBodyTuning& Tuning = UDeepMonsterSettings::Get().Body;
	Trail.Reset();
	const int32 Points = FMath::CeilToInt((Tuning.LengthM + 1.f) * CmPerM / TrailStepCm) + 1;
	for (int32 i = 0; i < Points; ++i)
	{
		Trail.Add(HeadCm - Heading * (i * TrailStepCm));
	}
	LayBody();
}

void ADeepSerpent::SwimToward(const FVector& AimCm, float SpeedMps, float TurnRateDegPerS, float DeltaSeconds)
{
	const float Dt = FMath::Max(0.f, DeltaSeconds);
	const FVector Here = GetActorLocation();
	const FVector ToAim = AimCm - Here;
	Heading = UDeepMonsterRules::TurnToward(Heading, ToAim, TurnRateDegPerS * Dt);

	const float StepCm = FMath::Min(FMath::Max(0.f, SpeedMps) * CmPerM * Dt, static_cast<float>(ToAim.Size()));
	SetActorLocationAndRotation(Here + Heading * StepCm, Heading.Rotation());
	RecordTrail();
}

void ADeepSerpent::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		Heading = GetActorForwardVector();
	}
	RecordTrail();
	LayBody();
}

void ADeepSerpent::RecordTrail()
{
	const FVector HeadCm = GetActorLocation();
	if (Trail.Num() == 0 || FVector::Dist(HeadCm, Trail[0]) >= TrailStepCm)
	{
		Trail.Insert(HeadCm, 0);
	}

	// Keep just enough trail for the body plus a margin.
	const float KeepCm = (UDeepMonsterSettings::Get().Body.LengthM + 1.f) * CmPerM;
	float Total = 0.f;
	for (int32 i = 1; i < Trail.Num(); ++i)
	{
		Total += static_cast<float>(FVector::Dist(Trail[i], Trail[i - 1]));
		if (Total > KeepCm && i + 1 < Trail.Num())
		{
			Trail.SetNum(i + 1);
			break;
		}
	}
}

void ADeepSerpent::LayBody()
{
	const FDeepSerpentBodyTuning& Tuning = UDeepMonsterSettings::Get().Body;
	const int32 N = FMath::Max(2, Tuning.Segments);
	const float SpacingM = SegmentSpacingM(Tuning);
	const float Time = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;

	// The path starts at the head as it is now, then the recorded trail.
	TArray<FVector> Path;
	Path.Reserve(Trail.Num() + 1);
	Path.Add(GetActorLocation());
	Path.Append(Trail);

	SegmentTransforms.SetNum(N);
	FVector Previous = GetActorLocation();
	for (int32 i = 0; i < N; ++i)
	{
		const float T = (i + 1) / static_cast<float>(N);
		const float AlongM = HeadBackM(Tuning) + (i + 0.5f) * SpacingM;
		const FVector OnPath = UDeepMonsterRules::PointAlongTrail(Path, AlongM * CmPerM);
		const FVector Ahead = UDeepMonsterRules::PointAlongTrail(Path, (AlongM - SpacingM) * CmPerM);
		const FVector Dir = (Ahead - OnPath).GetSafeNormal(UE_SMALL_NUMBER, Heading);
		FVector Side = FVector::CrossProduct(FVector::UpVector, Dir);
		if (!Side.Normalize())
		{
			Side = FVector::RightVector;
		}

		// A wave travelling down the body, growing toward the tail.
		const float Phase = 2.f * PI * (Tuning.WeaveHz * Time - AlongM / FMath::Max(0.1f, Tuning.WeaveWavelengthM));
		const float SwingM = Tuning.WeaveAmplitudeM * (0.25f + 0.75f * T) * FMath::Sin(Phase);
		const FVector Location = OnPath + Side * (SwingM * CmPerM);

		const float RadiusM = FMath::Lerp(Tuning.NeckRadiusM, Tuning.TailRadiusM, FMath::Pow(T, 1.2f));
		const FVector Facing = (Previous - Location).GetSafeNormal(UE_SMALL_NUMBER, Dir);
		const FVector Scale(FMath::Max(RadiusM * 2.f, SpacingM * 1.6f), RadiusM * 2.f, RadiusM * 2.f);
		SegmentTransforms[i] = FTransform(Facing.Rotation(), Location, Scale);
		Previous = Location;
	}

	if (Body->GetInstanceCount() != N)
	{
		Body->ClearInstances();
		for (const FTransform& Segment : SegmentTransforms)
		{
			Body->AddInstance(Segment, true);
		}
	}
	else
	{
		Body->BatchUpdateInstancesTransforms(0, SegmentTransforms, true, true, false);
	}
}
