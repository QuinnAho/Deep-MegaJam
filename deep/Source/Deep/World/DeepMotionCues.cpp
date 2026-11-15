#include "World/DeepMotionCues.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Game/DeepGameState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepShapes.h"

namespace
{
	const TCHAR* GlowMaterialPath = TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial");

	UInstancedStaticMeshComponent* MakeInstanced(AActor* Owner, FName Name, const TCHAR* MeshPath, USceneComponent* Parent)
	{
		UInstancedStaticMeshComponent* Component = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Component->SetStaticMesh(DeepShapes::LoadMesh(MeshPath));
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		Component->SetupAttachment(Parent);
		return Component;
	}
}

ADeepMotionCues::ADeepMotionCues()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Specks = MakeInstanced(this, TEXT("Specks"), DeepShapes::Sphere, Root);
	Ledges = MakeInstanced(this, TEXT("Ledges"), DeepShapes::Cube, Root);
	WallLeft = DeepShapes::CreateShape(this, TEXT("WallLeft"), DeepShapes::Cube);
	WallLeft->SetupAttachment(Root);
	WallRight = DeepShapes::CreateShape(this, TEXT("WallRight"), DeepShapes::Cube);
	WallRight->SetupAttachment(Root);
}

void ADeepMotionCues::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

void ADeepMotionCues::Build()
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	const float M = UDeepRules::CmPerM;

	// Specks: a fixed random scatter (seeded, so every machine sees the same water), glowing so they read as pale.
	FRandomStream Random(0xDEE9);
	SpeckM.Reset();
	SpeckXf.Reset();
	Specks->ClearInstances();
	for (int32 Index = 0; Index < FMath::Max(0, T.SpeckCount); ++Index)
	{
		SpeckM.Add(FVector(Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f)) * T.SpeckBoxHalfM);
		SpeckXf.Add(FTransform::Identity);
	}
	Specks->AddInstances(SpeckXf, false);
	if (UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, GlowMaterialPath))
	{
		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Glow, Specks);
		Instance->SetVectorParameterValue(TEXT("Color"), T.SpeckColor);
		Specks->SetMaterial(0, Instance);
	}

	// Shaft walls on both sides; ledges are instanced across both.
	const FVector WallScale(T.ShaftWallLengthM, 1.f, T.ShaftWallHeightM);
	WallLeft->SetRelativeLocation(FVector(T.ShaftWallCentreXM, -T.ShaftWallOffsetM - 0.5f, 0.f) * M);
	WallRight->SetRelativeLocation(FVector(T.ShaftWallCentreXM, T.ShaftWallOffsetM + 0.5f, 0.f) * M);
	WallLeft->SetRelativeScale3D(WallScale);
	WallRight->SetRelativeScale3D(WallScale);
	DeepShapes::Tint(WallLeft, T.ShaftWallColor);
	DeepShapes::Tint(WallRight, T.ShaftWallColor);
	LedgesPerSide = T.LedgeSpacingM > 0.f ? FMath::CeilToInt(T.ShaftWallHeightM / T.LedgeSpacingM) : 0;
	LedgeXf.Init(FTransform::Identity, LedgesPerSide * 2);
	Ledges->ClearInstances();
	Ledges->AddInstances(LedgeXf, false);
	DeepShapes::Tint(Ledges, T.LedgeColor);

	const ADeepGameState* State = GetWorld() ? GetWorld()->GetGameState<ADeepGameState>() : nullptr;
	LastDepthM = State ? State->SubDepthM : 0.f;
	Place(0.f);
}

void ADeepMotionCues::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Place(DeltaSeconds);
}

void ADeepMotionCues::Place(float DeltaSeconds)
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	const float M = UDeepRules::CmPerM;
	const ADeepGameState* State = GetWorld() ? GetWorld()->GetGameState<ADeepGameState>() : nullptr;
	const float DepthM = State ? State->SubDepthM : 0.f;
	if (DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		// A jump (Deep.GotoStop, a restart) is not speed: ignore steps faster than any helm could drive.
		const float Raw = (DepthM - LastDepthM) / DeltaSeconds;
		const float Measured = FMath::Abs(Raw) > T.DriveSpeedMps * 4.f ? 0.f : Raw;
		SpeedMps = FMath::FInterpTo(SpeedMps, Measured, DeltaSeconds, 6.f);
	}
	LastDepthM = DepthM;

	// Specks: the sub going down = the water going up past it; parked, they sink slowly.
	const float RiseMps = SpeedMps - T.SpeckDriftMps;
	const float Size = T.SpeckSizeM;
	const FVector Scale(Size, Size, Size * (1.f + FMath::Abs(SpeedMps) * T.SpeckStretchPerMps));
	for (int32 Index = 0; Index < SpeckM.Num(); ++Index)
	{
		FVector& P = SpeckM[Index];
		P.Z = DeepMotion::Wrap(P.Z + RiseMps * DeltaSeconds, T.SpeckBoxHalfM.Z);
		SpeckXf[Index] = FTransform(FQuat::Identity, (T.SpeckBoxCentreM + P) * M, Scale);
	}
	if (SpeckXf.Num() > 0)
	{
		Specks->BatchUpdateInstancesTransforms(0, SpeckXf, false, true, true);
	}

	// Walls and ledges only between stops: parked, the arena is where the diver swims and they would cut through it.
	const bool bShaft = State && !State->bParked;
	WallLeft->SetVisibility(bShaft);
	WallRight->SetVisibility(bShaft);
	Ledges->SetVisibility(bShaft);
	if (bShaft && LedgesPerSide > 0)
	{
		const FVector LedgeScale(T.ShaftWallLengthM, T.LedgeOutM, T.LedgeThickM);
		for (int32 Index = 0; Index < LedgesPerSide; ++Index)
		{
			const float Z = DeepMotion::LedgeOffsetM(Index, DepthM, T.LedgeSpacingM, LedgesPerSide);
			const float Y = T.ShaftWallOffsetM - T.LedgeOutM * 0.5f;
			LedgeXf[Index * 2] = FTransform(FQuat::Identity, FVector(T.ShaftWallCentreXM, -Y, Z) * M, LedgeScale);
			LedgeXf[Index * 2 + 1] = FTransform(FQuat::Identity, FVector(T.ShaftWallCentreXM, Y, Z) * M, LedgeScale);
		}
		Ledges->BatchUpdateInstancesTransforms(0, LedgeXf, false, true, true);
	}
}

float ADeepMotionCues::GetSpeckMeanZM() const
{
	float Sum = 0.f;
	for (const FVector& P : SpeckM)
	{
		Sum += P.Z;
	}
	return SpeckM.Num() > 0 ? Sum / SpeckM.Num() : 0.f;
}
