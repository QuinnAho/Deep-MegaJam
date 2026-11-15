#include "World/DeepSubmarine.h"

#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepShapes.h"

namespace
{
	const FLinearColor HullColor(0.55f, 0.36f, 0.12f);
	const FLinearColor DeckColor(0.32f, 0.34f, 0.36f);
	const FLinearColor CeilingColor(0.6f, 0.6f, 0.58f);
	const FLinearColor RimColor(1.f, 0.8f, 0.05f);
	const FLinearColor CoverColor(0.8f, 0.08f, 0.05f);

	void Solid(UStaticMeshComponent* Shape)
	{
		Shape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Shape->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}

	/** Centre and size in cm, relative to the sub. */
	void Place(UStaticMeshComponent* Shape, const FVector& CentreCm, const FVector& SizeCm)
	{
		Shape->SetRelativeLocation(CentreCm);
		Shape->SetRelativeScale3D(SizeCm / DeepShapes::ShapeSizeCm);
	}
}

ADeepSubmarine::ADeepSubmarine()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	auto Make = [this](const TCHAR* Name, const TCHAR* Mesh, bool bSolid)
	{
		UStaticMeshComponent* Shape = DeepShapes::CreateShape(this, Name, Mesh);
		Shape->SetupAttachment(Root);
		if (bSolid)
		{
			Solid(Shape);
		}
		return Shape;
	};
	for (const TCHAR* Name : { TEXT("DeckFront"), TEXT("DeckBack"), TEXT("DeckLeft"), TEXT("DeckRight") })
	{
		Deck.Add(Make(Name, DeepShapes::Cube, true));
	}
	for (const TCHAR* Name : { TEXT("WallFront"), TEXT("WallBack"), TEXT("WallLeft"), TEXT("WallRight") })
	{
		Walls.Add(Make(Name, DeepShapes::Cube, true));
	}
	Ceiling = Make(TEXT("Ceiling"), DeepShapes::Cube, true);
	for (const TCHAR* Name : { TEXT("RimA"), TEXT("RimB"), TEXT("RimC"), TEXT("RimD") })
	{
		HatchRim.Add(Make(Name, DeepShapes::Cube, false));
	}
	HatchCover = Make(TEXT("HatchCover"), DeepShapes::Cube, false);
	Console = Make(TEXT("Console"), DeepShapes::Cube, true);
	ConsoleScreen = Make(TEXT("ConsoleScreen"), DeepShapes::Cube, false);
	HelmSeat = Make(TEXT("HelmSeat"), DeepShapes::Cube, false);
	HelmPanel = Make(TEXT("HelmPanel"), DeepShapes::Cube, true);
	Window = Make(TEXT("Window"), DeepShapes::Cube, false);
	// The nose and tower are only silhouette from the water; the nose has no collision so it cannot reach into the room.
	NoseCone = Make(TEXT("NoseCone"), DeepShapes::Sphere, false);
	Tower = Make(TEXT("Tower"), DeepShapes::Cube, true);

	RoomLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RoomLight"));
	RoomLight->SetupAttachment(Root);
	RoomLight->SetMobility(EComponentMobility::Movable);
	RoomLight->SetLightColor(FLinearColor(1.f, 0.92f, 0.8f));
	RoomLight->SetCastShadows(false);

	AirlockLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("AirlockLight"));
	AirlockLight->SetupAttachment(Root);
	AirlockLight->SetRelativeLocation(FVector(0.f, 0.f, -350.f));
	AirlockLight->SetMobility(EComponentMobility::Movable);
	AirlockLight->SetLightColor(FLinearColor(1.f, 0.75f, 0.45f));
	AirlockLight->SetCastShadows(false);

	HelmCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("HelmCamera"));
	HelmCamera->SetupAttachment(Root);

	LayOut();
}

void ADeepSubmarine::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	LayOut();
}

void ADeepSubmarine::LayOut()
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	const double M = UDeepRules::CmPerM;
	const double L = T.RoomHalfLengthM * M;
	const double W = T.RoomHalfWidthM * M;
	const double H = T.RoomHeightM * M;
	const double Th = T.WallThicknessM * M;
	const double Hh = T.HatchHalfSizeM * M;
	const double DeckTop = -H * 0.5;
	const double DeckZ = DeckTop - Th * 0.5;
	const double OuterL = L + Th;
	const double OuterW = W + Th;

	if (Deck.Num() == 4)
	{
		Place(Deck[0], FVector((Hh + OuterL) * 0.5, 0.0, DeckZ), FVector(OuterL - Hh, OuterW * 2.0, Th));
		Place(Deck[1], FVector(-(Hh + OuterL) * 0.5, 0.0, DeckZ), FVector(OuterL - Hh, OuterW * 2.0, Th));
		Place(Deck[2], FVector(0.0, (Hh + OuterW) * 0.5, DeckZ), FVector(Hh * 2.0, OuterW - Hh, Th));
		Place(Deck[3], FVector(0.0, -(Hh + OuterW) * 0.5, DeckZ), FVector(Hh * 2.0, OuterW - Hh, Th));
	}
	if (Walls.Num() == 4)
	{
		Place(Walls[0], FVector(L + Th * 0.5, 0.0, 0.0), FVector(Th, OuterW * 2.0, H));
		Place(Walls[1], FVector(-L - Th * 0.5, 0.0, 0.0), FVector(Th, OuterW * 2.0, H));
		Place(Walls[2], FVector(0.0, W + Th * 0.5, 0.0), FVector(L * 2.0, Th, H));
		Place(Walls[3], FVector(0.0, -W - Th * 0.5, 0.0), FVector(L * 2.0, Th, H));
	}
	Place(Ceiling, FVector(0.0, 0.0, H * 0.5 + Th * 0.5), FVector(OuterL * 2.0, OuterW * 2.0, Th));

	// The rim goes through the deck so the yellow frame shows from the deck and from the water below.
	const double RimW = 16.0;
	const double RimH = 6.0;
	const double RimZ = DeckTop - Th * 0.5;
	const double RimT = Th + RimH * 2.0;
	if (HatchRim.Num() == 4)
	{
		Place(HatchRim[0], FVector(0.0, Hh + RimW * 0.5, RimZ), FVector(Hh * 2.0 + RimW * 2.0, RimW, RimT));
		Place(HatchRim[1], FVector(0.0, -Hh - RimW * 0.5, RimZ), FVector(Hh * 2.0 + RimW * 2.0, RimW, RimT));
		Place(HatchRim[2], FVector(Hh + RimW * 0.5, 0.0, RimZ), FVector(RimW, Hh * 2.0, RimT));
		Place(HatchRim[3], FVector(-Hh - RimW * 0.5, 0.0, RimZ), FVector(RimW, Hh * 2.0, RimT));
	}
	Place(HatchCover, FVector(0.0, 0.0, DeckZ + 2.0), FVector(Hh * 2.0, Hh * 2.0, Th));

	const FVector2D C = T.ConsoleM * M;
	Place(Console, FVector(C.X, C.Y + 20.0, DeckTop + 55.0), FVector(120.0, 50.0, 110.0));
	Place(ConsoleScreen, FVector(C.X, C.Y - 6.0, DeckTop + 100.0), FVector(90.0, 4.0, 45.0));

	const FVector2D Hm = T.HelmM * M;
	Place(HelmSeat, FVector(Hm.X - 60.0, Hm.Y, DeckTop + 25.0), FVector(55.0, 55.0, 50.0));
	Place(HelmPanel, FVector(Hm.X + 30.0, Hm.Y, DeckTop + 45.0), FVector(35.0, 120.0, 90.0));
	Place(Window, FVector(L - 3.0, 0.0, 40.0), FVector(4.0, 170.0, 70.0));

	// Centred just outside the front wall so none of it reaches into the room.
	Place(NoseCone, FVector(OuterL + 150.0, 0.0, 0.0), FVector(300.0, OuterW * 2.0 * 0.9, H + Th));
	Place(Tower, FVector(100.0, 0.0, H * 0.5 + Th + 110.0), FVector(300.0, 140.0, 220.0));

	RoomLight->SetRelativeLocation(FVector(-100.0, 0.0, H * 0.5 - 30.0));
	HelmCamera->SetRelativeLocation(T.HelmCameraM * M);
	HelmCamera->SetRelativeRotation(FRotator(T.HelmCameraPitchDeg, 0.f, 0.f));
	HelmCamera->SetFieldOfView(T.HelmCameraFovDeg);
}

void ADeepSubmarine::BeginPlay()
{
	Super::BeginPlay();
	LayOut();

	const FDeepArenaTuning& Arena = UDeepTuningSettings::Get().Arena;
	const FDeepSubTuning& Sub = UDeepTuningSettings::Get().Sub;
	AirlockLight->SetIntensity(Arena.SubLightIntensity);
	AirlockLight->SetAttenuationRadius(Arena.SubLightRadiusM * UDeepRules::CmPerM);
	RoomLight->SetIntensity(Sub.RoomLightIntensity);
	RoomLight->SetAttenuationRadius(Sub.RoomLightRadiusM * UDeepRules::CmPerM);

	for (UStaticMeshComponent* Shape : Deck)
	{
		DeepShapes::Tint(Shape, DeckColor);
	}
	for (UStaticMeshComponent* Shape : Walls)
	{
		DeepShapes::Tint(Shape, HullColor);
	}
	DeepShapes::Tint(Ceiling, CeilingColor);
	for (UStaticMeshComponent* Shape : HatchRim)
	{
		DeepShapes::Tint(Shape, RimColor);
	}
	DeepShapes::Tint(HatchCover, CoverColor);
	DeepShapes::Tint(Console, FLinearColor(0.12f, 0.13f, 0.15f));
	DeepShapes::Tint(ConsoleScreen, FLinearColor(0.1f, 1.f, 0.4f));
	DeepShapes::Tint(HelmSeat, FLinearColor(0.15f, 0.1f, 0.08f));
	DeepShapes::Tint(HelmPanel, FLinearColor(0.1f, 0.25f, 0.6f));
	DeepShapes::Tint(Window, FLinearColor(0.02f, 0.08f, 0.12f));
	DeepShapes::Tint(NoseCone, HullColor);
	DeepShapes::Tint(Tower, FLinearColor(0.2f, 0.2f, 0.22f));
	OnRep_Hatch();
}

FVector ADeepSubmarine::Local(double X, double Y, double Z) const
{
	return GetActorTransform().TransformPosition(FVector(X, Y, Z));
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

double ADeepSubmarine::GetDeckTopZ() const
{
	return GetActorLocation().Z - UDeepTuningSettings::Get().Sub.RoomHeightM * UDeepRules::CmPerM * 0.5;
}

FTransform ADeepSubmarine::GetRoomStartTransform() const
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	const FVector2D At = T.RoomStartM * UDeepRules::CmPerM;
	const FVector Start = Local(At.X, At.Y, -T.RoomHeightM * UDeepRules::CmPerM * 0.5 + 92.0);
	return FTransform(FRotator(0.f, GetActorRotation().Yaw, 0.f), Start);
}

FVector ADeepSubmarine::GetEnterStandLocation() const
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	const FVector2D At = T.EnterStandM * UDeepRules::CmPerM;
	return Local(At.X, At.Y, -T.RoomHeightM * UDeepRules::CmPerM * 0.5 + 92.0);
}

FVector ADeepSubmarine::GetHatchLocation() const
{
	return Local(0.0, 0.0, -UDeepTuningSettings::Get().Sub.RoomHeightM * UDeepRules::CmPerM * 0.5);
}

FVector ADeepSubmarine::GetConsoleLocation() const
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	return Local(T.ConsoleM.X * UDeepRules::CmPerM, T.ConsoleM.Y * UDeepRules::CmPerM, -T.RoomHeightM * UDeepRules::CmPerM * 0.5 + 100.0);
}

FVector ADeepSubmarine::GetHelmLocation() const
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	return Local(T.HelmM.X * UDeepRules::CmPerM, T.HelmM.Y * UDeepRules::CmPerM, -T.RoomHeightM * UDeepRules::CmPerM * 0.5 + 90.0);
}

bool ADeepSubmarine::IsInRoom(const FVector& WorldCm, double MarginCm) const
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	const FVector P = GetActorTransform().InverseTransformPosition(WorldCm);
	const double M = UDeepRules::CmPerM;
	return FMath::Abs(P.X) <= T.RoomHalfLengthM * M && FMath::Abs(P.Y) <= T.RoomHalfWidthM * M
		&& P.Z >= -T.RoomHeightM * M * 0.5 - MarginCm && P.Z <= T.RoomHeightM * M * 0.5;
}

bool ADeepSubmarine::IsInHatchColumn(const FVector& WorldCm) const
{
	return UDeepRules::InHatchColumn(GetActorTransform().InverseTransformPosition(WorldCm), UDeepTuningSettings::Get().Sub);
}

FBox ADeepSubmarine::GetSolidBoundsLocal(bool bHullOnly) const
{
	FBox Box(ForceInit);
	TArray<UStaticMeshComponent*> Shapes;
	GetComponents<UStaticMeshComponent>(Shapes);
	for (UStaticMeshComponent* Shape : Shapes)
	{
		const bool bHull = Deck.Contains(Shape) || Walls.Contains(Shape) || Shape == Ceiling.Get();
		if (Shape && Shape->IsCollisionEnabled() && (bHull || !bHullOnly) && Shape->GetAttachParent() == Root)
		{
			Box += Shape->CalcBounds(Shape->GetRelativeTransform()).GetBox();
		}
	}
	return Box;
}

bool ADeepSubmarine::IsWithinUseRange(const FVector& WorldCm, const FVector& PointCm) const
{
	return FVector::DistXY(WorldCm, PointCm) <= UDeepTuningSettings::Get().Sub.UseRangeM * UDeepRules::CmPerM;
}

void ADeepSubmarine::SetHatchOpen(bool bOpen)
{
	if (bHatchOpen != bOpen)
	{
		bHatchOpen = bOpen;
		OnRep_Hatch();
	}
}

void ADeepSubmarine::OnRep_Hatch()
{
	HatchCover->SetVisibility(!bHatchOpen);
	if (bHatchOpen)
	{
		HatchCover->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	else
	{
		Solid(HatchCover);
	}
}

void ADeepSubmarine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADeepSubmarine, bHatchOpen);
}
