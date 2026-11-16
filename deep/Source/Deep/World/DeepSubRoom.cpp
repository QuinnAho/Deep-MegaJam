#include "World/DeepSubRoom.h"

#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LocalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Diver/DeepRunComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepRoomDress.h"
#include "World/DeepShapes.h"
#include "World/DeepSubParts.h"

namespace
{
	using namespace DeepSubParts;

	int32 GNoseStrip = 1;
	FAutoConsoleVariableRef CVarNoseStrip(
		TEXT("Deep.Room.NoseStrip"),
		GNoseStrip,
		TEXT("Deep.Room.NoseStrip <0|1>: 1 (default) = the ceiling strip across the nose lights the helm end and the window; 0 = off."));

	// The valve wheel's parts (local cm, on the pivot): rim, two spokes, hub.
	constexpr double ValveRimCm = 56.0;
	constexpr double ValveRimThickCm = 5.0;
	constexpr double ValveSpokeCm = 4.0;
	constexpr double ValveHubCm = 14.0;
	constexpr double ValveTurnDeg = 270.0;
	// The porthole's backdrop: this far outside the front wall, this big (it fills every view through the glass).
	constexpr double BackdropOutCm = 500.0;
	constexpr double BackdropSizeCm = 3000.0;
}

ADeepSubRoom::ADeepSubRoom()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;

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
	for (const TCHAR* Name : { TEXT("WallFront"), TEXT("WallBack"), TEXT("WallLeft"), TEXT("WallRight"), TEXT("WallFrontTop"), TEXT("WallFrontPort"), TEXT("WallFrontStarboard") })
	{
		Walls.Add(Make(Name, DeepShapes::Cube, true));
	}
	Ceiling = Make(TEXT("Ceiling"), DeepShapes::Cube, true);
	for (const TCHAR* Name : { TEXT("RimA"), TEXT("RimB"), TEXT("RimC"), TEXT("RimD") })
	{
		HatchRim.Add(Make(Name, DeepShapes::Cube, false));
	}
	// Shut for good: the room's hatch is a door you work, not a hole.
	HatchCover = Make(TEXT("HatchCover"), DeepShapes::Cube, true);

	ValvePivot = CreateDefaultSubobject<USceneComponent>(TEXT("ValvePivot"));
	ValvePivot->SetupAttachment(Root);
	ValvePivot->SetMobility(EComponentMobility::Movable);
	auto MakeValve = [this](const TCHAR* Name, const TCHAR* Mesh, const FVector& Centre, const FVector& Size)
	{
		UStaticMeshComponent* Shape = DeepShapes::CreateShape(this, Name, Mesh);
		Shape->SetupAttachment(ValvePivot);
		Shape->SetCastShadow(false);
		Place(Shape, Centre, Size);
		ValveParts.Add(Shape);
	};
	MakeValve(TEXT("ValveRim"), DeepShapes::Cylinder, FVector(0.0, 0.0, ValveRimThickCm * 0.5 + ValveHubCm * 0.5), FVector(ValveRimCm, ValveRimCm, ValveRimThickCm));
	MakeValve(TEXT("ValveSpokeA"), DeepShapes::Cube, FVector(0.0, 0.0, ValveHubCm * 0.5), FVector(ValveRimCm - 4.0, ValveSpokeCm, ValveSpokeCm));
	MakeValve(TEXT("ValveSpokeB"), DeepShapes::Cube, FVector(0.0, 0.0, ValveHubCm * 0.5), FVector(ValveSpokeCm, ValveRimCm - 4.0, ValveSpokeCm));
	MakeValve(TEXT("ValveHub"), DeepShapes::Cylinder, FVector(0.0, 0.0, ValveHubCm * 0.5), FVector(ValveHubCm, ValveHubCm, ValveHubCm));

	Console = Make(TEXT("Console"), DeepShapes::Cube, true);
	ConsoleScreen = Make(TEXT("ConsoleScreen"), DeepShapes::Cube, false);
	ConsoleScreen->SetCastShadow(false);
	HelmSeat = Make(TEXT("HelmSeat"), DeepShapes::Cube, false);
	HelmPanel = Make(TEXT("HelmPanel"), DeepShapes::Cube, true);
	HelmScreen = Make(TEXT("HelmScreen"), DeepShapes::Cube, false);
	HelmScreen->SetCastShadow(false);
	Window = Make(TEXT("Window"), DeepShapes::Cube, true);
	Window->SetCastShadow(false);
	PortholeBackdrop = Make(TEXT("PortholeBackdrop"), DeepShapes::Cube, false);
	PortholeBackdrop->SetCastShadow(false);
	for (const TCHAR* Name : { TEXT("WindowBezelTop"), TEXT("WindowBezelBottom"), TEXT("WindowBezelPort"), TEXT("WindowBezelStbd") })
	{
		UStaticMeshComponent* Shape = Make(Name, DeepShapes::Cube, false);
		Shape->SetCastShadow(false);
		WindowBezel.Add(Shape);
	}
	for (const TCHAR* Name : { TEXT("StripFixtureA"), TEXT("StripFixtureB"), TEXT("StripFixtureC") })
	{
		StripFixtures.Add(Make(Name, DeepShapes::Cube, false));
	}

	RoomLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RoomLight"));
	RoomLight->SetupAttachment(Root);
	RoomLight->SetMobility(EComponentMobility::Movable);
	RoomLight->SetLightColor(FLinearColor(1.f, 0.92f, 0.8f));
	RoomLight->SetCastShadows(false);

	PortholeLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PortholeLight"));
	PortholeLight->SetupAttachment(Root);
	PortholeLight->SetMobility(EComponentMobility::Movable);
	PortholeLight->SetLightColor(FLinearColor(0.55f, 0.7f, 0.75f));
	PortholeLight->SetCastShadows(false);
	PortholeLight->SetVisibility(false);

	auto MakeRect = [this](const TCHAR* Name)
	{
		URectLightComponent* Light = CreateDefaultSubobject<URectLightComponent>(Name);
		Light->SetupAttachment(Root);
		Light->SetMobility(EComponentMobility::Movable);
		return Light;
	};
	Strips.Add(MakeRect(TEXT("StripA")));
	Strips.Add(MakeRect(TEXT("StripB")));
	Strips.Add(MakeRect(TEXT("StripC")));
	ConsoleLight = MakeRect(TEXT("ConsoleLight"));
	HelmLight = MakeRect(TEXT("HelmLight"));

	HatchRed = CreateDefaultSubobject<UPointLightComponent>(TEXT("HatchRed"));
	HatchRed->SetupAttachment(Root);
	HatchRed->SetMobility(EComponentMobility::Movable);

	RoomVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("RoomVolume"));
	RoomVolume->SetupAttachment(Root);
	RoomVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RoomVolume->SetGenerateOverlapEvents(false);
	RoomVolume->SetHiddenInGame(true);
	RoomGrade = CreateDefaultSubobject<UPostProcessComponent>(TEXT("RoomGrade"));
	RoomGrade->SetupAttachment(RoomVolume);
	RoomGrade->bUnbound = false;
	RoomGrade->Priority = 10.f;
	RoomGrade->BlendRadius = 40.f;

	// The room's dressing: ribs, pipes, cables, gauges, lockers, bench, ladder, caged lights (World/DeepRoomDress).
	DeepRoomDress::Create(this, Root, RoomDressMeshes, RoomDressLights);

	LayOut();
}

void ADeepSubRoom::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	LayOut();
}

void ADeepSubRoom::LayOut()
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	const FDeepLookTuning& Look = UDeepTuningSettings::Get().Look;
	const double M = UDeepRules::CmPerM;
	const double L = T.RoomHalfLengthM * M;
	const double W = T.RoomHalfWidthM * M;
	const double H = T.RoomHeightM * M;
	const double Th = T.WallThicknessM * M;
	const double Hh = T.HatchHalfSizeM * M;
	const double DeckTop = -H * 0.5;
	const double DeckZ = DeckTop - Th * 0.5;

	LayOutShell(Deck, Walls, Ceiling);
	LayOutRim(HatchRim);
	Place(HatchCover, FVector(0.0, 0.0, DeckZ + 2.0), FVector(Hh * 2.0, Hh * 2.0, Th));
	ValvePivot->SetRelativeLocation(FVector(T.ValveM.X * M, T.ValveM.Y * M, DeckTop + 2.0));

	const FVector2D C = T.ConsoleM * M;
	Place(Console, FVector(C.X, C.Y + 20.0, DeckTop + 55.0), FVector(120.0, 50.0, 110.0));
	Place(ConsoleScreen, FVector(C.X, C.Y - 6.0, DeckTop + 100.0), FVector(90.0, 4.0, 45.0));

	const FVector2D Hm = T.HelmM * M;
	Place(HelmSeat, FVector(Hm.X - 60.0, Hm.Y, DeckTop + 25.0), FVector(55.0, 55.0, 50.0));
	Place(HelmPanel, FVector(Hm.X + 30.0, Hm.Y, DeckTop + 45.0), FVector(35.0, 120.0, 90.0));
	Place(Window, FVector(L + Th * 0.5, 0.0, WinZ), FVector(2.0, WinHalfW * 2.0, WinHalfH * 2.0));
	const double HelmScreenX = Hm.X + 30.0 - 17.5 - 1.0;
	Place(HelmScreen, FVector(HelmScreenX, Hm.Y, DeckTop + 62.0), FVector(2.0, 95.0, 45.0));
	Place(PortholeBackdrop, FVector(L + Th + BackdropOutCm, 0.0, WinZ), FVector(10.0, BackdropSizeCm, BackdropSizeCm));

	const double StripY = W - 45.0;
	for (int32 Index = 0; Index < Strips.Num() && Index < StripFixtures.Num(); ++Index)
	{
		if (Index == 2)
		{
			const double NoseX = L - Look.NoseStripBackCm;
			const double Span = W * 2.0 - 90.0;
			Place(StripFixtures[Index], FVector(NoseX, 0.0, H * 0.5 - 4.0), FVector(14.0, Span + 20.0, 8.0));
			Strips[Index]->SetRelativeLocation(FVector(NoseX, 0.0, H * 0.5 - 9.0));
			Strips[Index]->SetRelativeRotation(FRotator(-90.f, 90.f, 0.f));
			Strips[Index]->SetSourceWidth(10.f);
			Strips[Index]->SetSourceHeight(static_cast<float>(Span));
			continue;
		}
		const double Y = Index == 0 ? StripY : -StripY;
		const double AftX = -L + 30.0;
		const double ForeX = 200.0;
		const double MidX = (AftX + ForeX) * 0.5;
		const double Len = ForeX - AftX;
		Place(StripFixtures[Index], FVector(MidX, Y, H * 0.5 - 4.0), FVector(Len + 20.0, 14.0, 8.0));
		Strips[Index]->SetRelativeLocation(FVector(MidX, Y, H * 0.5 - 9.0));
		Strips[Index]->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
		Strips[Index]->SetSourceWidth(10.f);
		Strips[Index]->SetSourceHeight(static_cast<float>(Len));
	}
	ConsoleLight->SetRelativeLocation(FVector(C.X, C.Y - 12.0, DeckTop + 100.0));
	ConsoleLight->SetRelativeRotation(FRotator(-20.f, -90.f, 0.f));
	ConsoleLight->SetSourceWidth(90.f);
	ConsoleLight->SetSourceHeight(45.f);
	HelmLight->SetRelativeLocation(FVector(HelmScreenX - 6.0, Hm.Y, DeckTop + 62.0));
	HelmLight->SetRelativeRotation(FRotator(-10.f, 180.f, 0.f));
	HelmLight->SetSourceWidth(95.f);
	HelmLight->SetSourceHeight(45.f);
	HatchRed->SetRelativeLocation(FVector(0.0, -Hh - 60.0, DeckTop + 35.0));

	const double BezelX = L - 4.0;
	const double BezelW = 12.0;
	if (WindowBezel.Num() == 4)
	{
		Place(WindowBezel[0], FVector(BezelX, 0.0, WinZ + WinHalfH + BezelW * 0.5), FVector(8.0, 2.0 * (WinHalfW + BezelW), BezelW));
		Place(WindowBezel[1], FVector(BezelX, 0.0, WinZ - WinHalfH - BezelW * 0.5), FVector(8.0, 2.0 * (WinHalfW + BezelW), BezelW));
		Place(WindowBezel[2], FVector(BezelX, -(WinHalfW + BezelW * 0.5), WinZ), FVector(8.0, BezelW, 2.0 * WinHalfH));
		Place(WindowBezel[3], FVector(BezelX, WinHalfW + BezelW * 0.5, WinZ), FVector(8.0, BezelW, 2.0 * WinHalfH));
	}

	RoomVolume->SetRelativeLocation(FVector::ZeroVector);
	RoomVolume->SetBoxExtent(FVector(L, W, H * 0.5));
	RoomLight->SetRelativeLocation(FVector(-100.0, 0.0, H * 0.5 - 30.0));
	PortholeLight->SetRelativeLocation(FVector(L + Th + BackdropOutCm * 0.5, 0.0, WinZ));
}

void ADeepSubRoom::ApplyLook()
{
	const FDeepLookTuning& Look = UDeepTuningSettings::Get().Look;
	const FDeepSubTuning& Sub = UDeepTuningSettings::Get().Sub;
	const FSubPaints P = Paints();

	const FVector Hatch = GetHatchLocation();
	const float PuddleRadius = Sub.HatchHalfSizeM * UDeepRules::CmPerM * 3.2f;
	for (UStaticMeshComponent* Shape : Deck)
	{
		SubSteel(Shape, P.Floor, Hatch, PuddleRadius);
	}
	for (UStaticMeshComponent* Shape : Walls)
	{
		SubSteel(Shape, P.Wall);
	}
	SubSteel(Ceiling, P.Ceil);
	for (UStaticMeshComponent* Shape : HatchRim)
	{
		SubSteel(Shape, P.Rim);
	}
	SubSteel(HatchCover, P.Cover);
	for (UStaticMeshComponent* Shape : ValveParts)
	{
		SubSteel(Shape, P.Rim);
	}
	SubSteel(Console, P.Machine);
	SubSteel(HelmPanel, P.Machine);
	SubSteel(HelmSeat, P.Seat);
	for (UStaticMeshComponent* Shape : StripFixtures)
	{
		SubSteel(Shape, P.Machine);
		Shape->SetCastShadow(false);
	}
	SubScreen(ConsoleScreen, Look.ScreenGlow);
	SubScreen(HelmScreen, Look.ScreenGlow);
	FSubSteel Bezel = P.Wall;
	Bezel.Wear = 0.45f;
	for (UStaticMeshComponent* Shape : WindowBezel)
	{
		SubSteel(Shape, Bezel);
	}
	// Black water: nothing out there but what drifts past the glass.
	DeepShapes::Tint(PortholeBackdrop, FLinearColor(0.004f, 0.008f, 0.01f));
	if (UMaterialInterface* Glass = Cast<UMaterialInterface>(Look.GlassMaterial.TryLoad()))
	{
		UMaterialInstanceDynamic* G = UMaterialInstanceDynamic::Create(Glass, Window);
		G->SetScalarParameterValue(TEXT("OpacityFacing"), 0.06f);
		G->SetScalarParameterValue(TEXT("OpacityGrazing"), 0.6f);
		G->SetScalarParameterValue(TEXT("Roughness"), 0.03f);
		Window->SetMaterial(0, G);
	}
	else
	{
		DeepShapes::Tint(Window, FLinearColor(0.01f, 0.015f, 0.018f));
	}

	// Lights (style guide 3): cool strips kept low, amber only at the screens, red at the hatch.
	const bool bGreyBox = UDeepTuningSettings::Get().Arena.bGreyBox;
	for (int32 Index = 0; Index < Strips.Num(); ++Index)
	{
		URectLightComponent* Light = Strips[Index];
		Light->SetLightColor(Look.StripColor);
		Light->SetIntensity(Index == 2 ? Look.NoseStripIntensity : Look.StripIntensity);
		Light->SetAttenuationRadius(500.f);
		Light->SetBarnDoorAngle(Look.StripBarnDoorDeg);
		Light->SetBarnDoorLength(12.f);
		Light->SetCastShadows(true);
		Light->SetVisibility(!bGreyBox);
	}
	for (URectLightComponent* Light : { ConsoleLight.Get(), HelmLight.Get() })
	{
		Light->SetLightColor(Look.PracticalColor);
		Light->SetIntensity(Look.PracticalIntensity);
		Light->SetAttenuationRadius(450.f);
		Light->SetBarnDoorAngle(70.f);
		Light->SetBarnDoorLength(8.f);
		Light->SetCastShadows(true);
		Light->SetVisibility(!bGreyBox);
	}
	HatchRed->SetLightColor(Look.EmergencyColor);
	HatchRed->SetIntensity(Look.EmergencyIntensity);
	HatchRed->SetAttenuationRadius(320.f);
	HatchRed->SetSourceRadius(4.f);
	HatchRed->SetCastShadows(true);
	HatchRed->SetVisibility(!bGreyBox);
	if (!bGreyBox)
	{
		RoomLight->SetIntensity(Sub.RoomLightIntensity * Look.RoomFillScale);
	}

	// The room keeps its own exposure.
	FPostProcessSettings& S = RoomGrade->Settings;
	S.bOverride_AutoExposureMethod = true;
	S.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	S.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	S.AutoExposureApplyPhysicalCameraExposure = false;
	S.bOverride_AutoExposureBias = true;
	S.AutoExposureBias = Look.RoomExposureBias;
	DeepRoomDress::Skin(RoomDressMeshes, RoomDressLights, bGreyBox);
	RoomGrade->bEnabled = !bGreyBox;
}

void ADeepSubRoom::BeginPlay()
{
	Super::BeginPlay();
	LayOut();
	const FDeepSubTuning& Sub = UDeepTuningSettings::Get().Sub;
	RoomLight->SetIntensity(Sub.RoomLightIntensity);
	RoomLight->SetAttenuationRadius(Sub.RoomLightRadiusM * UDeepRules::CmPerM);
	PortholeLight->SetIntensity(UDeepTuningSettings::Get().MovingSub.PortholeLightIntensity);
	PortholeLight->SetAttenuationRadius(static_cast<float>(BackdropOutCm * 1.5));
	ApplyLook();
}

void ADeepSubRoom::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const FDeepLookTuning& Look = Tuning.Look;
	const FDeepMovingSubTuning& Moving = Tuning.MovingSub;
	const bool bGreyBox = Tuning.Arena.bGreyBox;
	UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	if (Strips.IsValidIndex(2))
	{
		Strips[2]->SetVisibility(GNoseStrip != 0 && !bGreyBox);
	}

	// One pass over the divers: anyone at the settlement card dims the console; the valve shows the most turned.
	bool bSettling = false;
	float Turn = 0.f;
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		if (const UDeepRunComponent* Run = It->FindComponentByClass<UDeepRunComponent>())
		{
			bSettling |= Run->GetState() == EDeepRunState::Settling;
			Turn = FMath::Max(Turn, Run->GetValveTurn01());
		}
	}
	if (UMaterialInstanceDynamic* Screen = ConsoleScreen ? Cast<UMaterialInstanceDynamic>(ConsoleScreen->GetMaterial(0)) : nullptr)
	{
		ConsoleGlowScale = FMath::FInterpTo(ConsoleGlowScale, bSettling ? Look.ScreenGlowUnderCard : 1.f, DeltaSeconds, 6.f);
		Screen->SetScalarParameterValue(TEXT("Glow"), Look.ScreenGlow * ConsoleGlowScale);
	}
	ValveTurn01 = Turn;
	ValvePivot->SetRelativeRotation(FRotator(0.f, static_cast<float>(ValveTurnDeg * Turn), 0.f));

	// The blank porthole: every so often a faint light drifts across outside, port to starboard, swelling then gone.
	const float Every = FMath::Max(Moving.PortholeLightEveryS, 0.1f);
	const float Pass = FMath::Clamp(Moving.PortholeLightPassS, 0.01f, Every);
	const float Into = static_cast<float>(FMath::Fmod(Now, static_cast<double>(Every)));
	const bool bPassing = !bGreyBox && Into < Pass;
	PortholeLight->SetVisibility(bPassing);
	if (bPassing)
	{
		const float U = Into / Pass;
		const FVector At = PortholeLight->GetRelativeLocation();
		PortholeLight->SetRelativeLocation(FVector(At.X, FMath::Lerp(-BackdropSizeCm * 0.25, BackdropSizeCm * 0.25, static_cast<double>(U)), At.Z));
		PortholeLight->SetIntensity(Moving.PortholeLightIntensity * FMath::Sin(PI * U));
	}
}

FVector ADeepSubRoom::Local(double X, double Y, double Z) const
{
	return GetActorTransform().TransformPosition(FVector(X, Y, Z));
}

double ADeepSubRoom::GetDeckTopZ() const
{
	return GetActorLocation().Z - UDeepTuningSettings::Get().Sub.RoomHeightM * UDeepRules::CmPerM * 0.5;
}

FTransform ADeepSubRoom::GetRoomStartTransform() const
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	const FVector2D At = T.RoomStartM * UDeepRules::CmPerM;
	const FVector Start = Local(At.X, At.Y, -T.RoomHeightM * UDeepRules::CmPerM * 0.5 + 92.0);
	return FTransform(FRotator(0.f, GetActorRotation().Yaw, 0.f), Start);
}

FVector ADeepSubRoom::GetEnterStandLocation() const
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	const FVector2D At = T.EnterStandM * UDeepRules::CmPerM;
	return Local(At.X, At.Y, -T.RoomHeightM * UDeepRules::CmPerM * 0.5 + 92.0);
}

FVector ADeepSubRoom::GetHatchLocation() const
{
	return Local(0.0, 0.0, -UDeepTuningSettings::Get().Sub.RoomHeightM * UDeepRules::CmPerM * 0.5);
}

FVector ADeepSubRoom::GetValveLocation() const
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	return Local(T.ValveM.X * UDeepRules::CmPerM, T.ValveM.Y * UDeepRules::CmPerM, -T.RoomHeightM * UDeepRules::CmPerM * 0.5 + 10.0);
}

FVector ADeepSubRoom::GetConsoleLocation() const
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	return Local(T.ConsoleM.X * UDeepRules::CmPerM, T.ConsoleM.Y * UDeepRules::CmPerM, -T.RoomHeightM * UDeepRules::CmPerM * 0.5 + 100.0);
}

FVector ADeepSubRoom::GetHelmLocation() const
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	return Local(T.HelmM.X * UDeepRules::CmPerM, T.HelmM.Y * UDeepRules::CmPerM, -T.RoomHeightM * UDeepRules::CmPerM * 0.5 + 90.0);
}

bool ADeepSubRoom::IsInRoom(const FVector& WorldCm, double MarginCm) const
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	const FVector P = GetActorTransform().InverseTransformPosition(WorldCm);
	const double M = UDeepRules::CmPerM;
	return FMath::Abs(P.X) <= T.RoomHalfLengthM * M && FMath::Abs(P.Y) <= T.RoomHalfWidthM * M
		&& P.Z >= -T.RoomHeightM * M * 0.5 - MarginCm && P.Z <= T.RoomHeightM * M * 0.5;
}

bool ADeepSubRoom::IsWithinUseRange(const FVector& WorldCm, const FVector& PointCm) const
{
	return FVector::DistXY(WorldCm, PointCm) <= UDeepTuningSettings::Get().Sub.UseRangeM * UDeepRules::CmPerM;
}

void ADeepSubRoom::SetRoomDressVisible(bool bVisible)
{
	DeepRoomDress::SetVisible(RoomDressMeshes, RoomDressLights, bVisible);
}
