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
#include "World/DeepRoomLayout.h"
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
	// The round hatch: the cover's plate stands this far proud of the deck, on a wider rim ring with three dogs.
	constexpr double HatchProudCm = 1.0;
	constexpr double HatchRimWideCm = 10.0;
	// The wet patch round the hatch, in hatch radii.
	constexpr double PuddleRadii = 2.4;

	/** `Deep.Room.Roof 0|1`: hides the room's ceiling and its strips' fixtures, for the plan view from above. */
	FAutoConsoleCommandWithWorldAndArgs GRoomRoofCmd(
		TEXT("Deep.Room.Roof"),
		TEXT("Deep.Room.Roof <0|1>: 0 hides the sub room's ceiling (a plan view from above), 1 shows it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const bool bOn = Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0;
			for (TActorIterator<ADeepSubRoom> It(World); It; ++It)
			{
				It->SetRoofVisible(bOn);
			}
			UE_LOG(LogTemp, Log, TEXT("DEEPROOMROOF visible=%d"), bOn ? 1 : 0);
		}));
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
	// One slab: the room's round hatch is shut for good (the airlock is the way out), so the deck runs under it.
	Deck.Add(Make(TEXT("DeckSlab"), DeepShapes::Cube, true));
	for (const TCHAR* Name : { TEXT("WallFront"), TEXT("WallBack"), TEXT("WallLeft"), TEXT("WallRight"), TEXT("WallFrontTop"), TEXT("WallFrontPort"), TEXT("WallFrontStarboard") })
	{
		Walls.Add(Make(Name, DeepShapes::Cube, true));
	}
	Ceiling = Make(TEXT("Ceiling"), DeepShapes::Cube, true);
	// The rim ring under the cover and three dogs that clamp it.
	HatchRim.Add(Make(TEXT("RimRing"), DeepShapes::Cylinder, false));
	for (const TCHAR* Name : { TEXT("RimDogA"), TEXT("RimDogB"), TEXT("RimDogC") })
	{
		HatchRim.Add(Make(Name, DeepShapes::Cube, false));
	}
	// Shut for good: the room's hatch is a door you work, not a hole. A plate flush with the deck, safe to walk over.
	HatchCover = Make(TEXT("HatchCover"), DeepShapes::Cylinder, false);

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
	// The room's own inside (World/DeepRoomLayout), fitted to BT-01's cabin; the hatch's centre is the origin.
	const FBox In = DeepRoomLayout::Inside(T);
	const double Aft = In.Min.X;
	const double Fore = In.Max.X;
	const double W = In.Max.Y;
	const double DeckTop = In.Min.Z;
	const double Ceil = In.Max.Z;
	const double H = Ceil - DeckTop;
	const double Len = Fore - Aft;
	const double MidX = (Aft + Fore) * 0.5;
	const double Th = T.InsideWallThicknessM * M;
	const double OuterW = W + Th;
	const FBox Port = DeepRoomLayout::Porthole(T);
	const double PortZ = Port.GetCenter().Z;
	const double PortHalfW = Port.GetExtent().Y;
	const double PortHalfH = Port.GetExtent().Z;
	const double PortBottom = PortZ - PortHalfH;
	const double PortTop = PortZ + PortHalfH;

	// The shell: one deck slab, the aft and side walls, the front wall in four pieces round the porthole, the ceiling.
	if (Deck.Num() > 0)
	{
		Place(Deck[0], FVector(MidX, 0.0, DeckTop - Th * 0.5), FVector(Len + Th * 2.0, OuterW * 2.0, Th));
	}
	if (Walls.Num() == 7)
	{
		Place(Walls[0], FVector(Fore + Th * 0.5, 0.0, (DeckTop + PortBottom) * 0.5), FVector(Th, OuterW * 2.0, PortBottom - DeckTop));
		Place(Walls[1], FVector(Aft - Th * 0.5, 0.0, 0.0), FVector(Th, OuterW * 2.0, H));
		Place(Walls[2], FVector(MidX, W + Th * 0.5, 0.0), FVector(Len, Th, H));
		Place(Walls[3], FVector(MidX, -W - Th * 0.5, 0.0), FVector(Len, Th, H));
		Place(Walls[4], FVector(Fore + Th * 0.5, 0.0, (PortTop + Ceil) * 0.5), FVector(Th, OuterW * 2.0, Ceil - PortTop));
		Place(Walls[5], FVector(Fore + Th * 0.5, -(PortHalfW + OuterW) * 0.5, PortZ), FVector(Th, OuterW - PortHalfW, PortHalfH * 2.0));
		Place(Walls[6], FVector(Fore + Th * 0.5, (PortHalfW + OuterW) * 0.5, PortZ), FVector(Th, OuterW - PortHalfW, PortHalfH * 2.0));
	}
	Place(Ceiling, FVector(MidX, 0.0, Ceil + Th * 0.5), FVector(Len + Th * 2.0, OuterW * 2.0, Th));

	// The round hatch: a plate just proud of the deck on a wider ring, three dogs round its edge.
	const double R = DeepRoomLayout::HatchRadiusCm(T);
	Place(HatchCover, FVector(0.0, 0.0, DeckTop + HatchProudCm - 2.0), FVector(R * 2.0, R * 2.0, 4.0));
	if (HatchRim.Num() == 4)
	{
		Place(HatchRim[0], FVector(0.0, 0.0, DeckTop + HatchProudCm * 0.5 - 2.0), FVector((R + HatchRimWideCm) * 2.0, (R + HatchRimWideCm) * 2.0, 4.0));
		for (int32 Dog = 0; Dog < 3; ++Dog)
		{
			const double Yaw = 60.0 + 120.0 * Dog;
			const FVector Dir(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)), 0.0);
			Place(HatchRim[Dog + 1], Dir * (R + 2.0) + FVector(0.0, 0.0, DeckTop + 2.0), FVector(16.0, 8.0, 4.0));
			HatchRim[Dog + 1]->SetRelativeRotation(FRotator(0.f, static_cast<float>(Yaw), 0.f));
		}
	}
	ValvePivot->SetRelativeLocation(FVector(T.ValveM.X * M, T.ValveM.Y * M, DeckTop + 2.0));

	// The console against the side wall, its screen on the face toward the room.
	const FVector2D C = T.ConsoleM * M;
	const FBox ConsoleBox = DeepRoomLayout::Console(T);
	Place(Console, ConsoleBox.GetCenter(), ConsoleBox.GetSize());
	const double Toward = C.Y >= 0.0 ? -1.0 : 1.0;
	const double ConsoleFaceY = Toward < 0.0 ? ConsoleBox.Min.Y : ConsoleBox.Max.Y;
	Place(ConsoleScreen, FVector(C.X, ConsoleFaceY + Toward * 1.0, DeckTop + 100.0), FVector(80.0, 4.0, 45.0));

	// The helm at the bow: the seat, the panel in front of it, the panel's screen facing the seat, the porthole above.
	const FVector2D Hm = T.HelmM * M;
	const FBox Seat = DeepRoomLayout::HelmSeat(T);
	const FBox Panel = DeepRoomLayout::HelmPanel(T);
	Place(HelmSeat, Seat.GetCenter(), Seat.GetSize());
	Place(HelmPanel, Panel.GetCenter(), Panel.GetSize());
	const double HelmScreenX = Panel.Min.X - 1.0;
	Place(HelmScreen, FVector(HelmScreenX, Hm.Y, DeckTop + 62.0), FVector(2.0, 95.0, 45.0));
	Place(Window, FVector(Fore + Th * 0.5, 0.0, PortZ), FVector(2.0, PortHalfW * 2.0, PortHalfH * 2.0));
	Place(PortholeBackdrop, FVector(Fore + Th + BackdropOutCm, 0.0, PortZ), FVector(10.0, BackdropSizeCm, BackdropSizeCm));

	// Two ceiling strips from the scuttle to the helm seat, and one across the nose over the helm.
	const double StripY = W - 45.0;
	for (int32 Index = 0; Index < Strips.Num() && Index < StripFixtures.Num(); ++Index)
	{
		if (Index == 2)
		{
			const double NoseX = Fore - Look.NoseStripBackCm;
			const double Span = W * 2.0 - 60.0;
			Place(StripFixtures[Index], FVector(NoseX, 0.0, Ceil - 4.0), FVector(14.0, Span + 20.0, 8.0));
			Strips[Index]->SetRelativeLocation(FVector(NoseX, 0.0, Ceil - 9.0));
			Strips[Index]->SetRelativeRotation(FRotator(-90.f, 90.f, 0.f));
			Strips[Index]->SetSourceWidth(10.f);
			Strips[Index]->SetSourceHeight(static_cast<float>(Span));
			continue;
		}
		const double Y = Index == 0 ? StripY : -StripY;
		const double AftX = Aft + 60.0;  // forward of the ceiling scuttle (World/DeepRoomDress)
		const double ForeX = Seat.Min.X - 10.0;
		const double StripMidX = (AftX + ForeX) * 0.5;
		const double StripLen = ForeX - AftX;
		Place(StripFixtures[Index], FVector(StripMidX, Y, Ceil - 4.0), FVector(StripLen + 20.0, 14.0, 8.0));
		Strips[Index]->SetRelativeLocation(FVector(StripMidX, Y, Ceil - 9.0));
		Strips[Index]->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
		Strips[Index]->SetSourceWidth(10.f);
		Strips[Index]->SetSourceHeight(static_cast<float>(StripLen));
	}
	ConsoleLight->SetRelativeLocation(FVector(C.X, ConsoleFaceY + Toward * 7.0, DeckTop + 100.0));
	ConsoleLight->SetRelativeRotation(FRotator(-20.f, Toward < 0.0 ? -90.f : 90.f, 0.f));
	ConsoleLight->SetSourceWidth(80.f);
	ConsoleLight->SetSourceHeight(45.f);
	HelmLight->SetRelativeLocation(FVector(HelmScreenX - 6.0, Hm.Y, DeckTop + 62.0));
	HelmLight->SetRelativeRotation(FRotator(-10.f, 180.f, 0.f));
	HelmLight->SetSourceWidth(95.f);
	HelmLight->SetSourceHeight(45.f);
	HatchRed->SetRelativeLocation(DeepRoomLayout::HatchLampPost(T) + FVector(0.0, 0.0, 35.0));

	const double BezelX = Fore - 4.0;
	const double BezelW = 10.0;
	if (WindowBezel.Num() == 4)
	{
		Place(WindowBezel[0], FVector(BezelX, 0.0, PortTop + BezelW * 0.5), FVector(8.0, 2.0 * (PortHalfW + BezelW), BezelW));
		Place(WindowBezel[1], FVector(BezelX, 0.0, PortBottom - BezelW * 0.5), FVector(8.0, 2.0 * (PortHalfW + BezelW), BezelW));
		Place(WindowBezel[2], FVector(BezelX, -(PortHalfW + BezelW * 0.5), PortZ), FVector(8.0, BezelW, 2.0 * PortHalfH));
		Place(WindowBezel[3], FVector(BezelX, PortHalfW + BezelW * 0.5, PortZ), FVector(8.0, BezelW, 2.0 * PortHalfH));
	}

	RoomVolume->SetRelativeLocation(FVector(MidX, 0.0, 0.0));
	RoomVolume->SetBoxExtent(FVector(Len * 0.5, W, H * 0.5));
	RoomLight->SetRelativeLocation(FVector(60.0, 0.0, Ceil - 25.0));
	PortholeLight->SetRelativeLocation(FVector(Fore + Th + BackdropOutCm * 0.5, 0.0, PortZ));
}

void ADeepSubRoom::ApplyLook()
{
	const FDeepLookTuning& Look = UDeepTuningSettings::Get().Look;
	const FDeepSubTuning& Sub = UDeepTuningSettings::Get().Sub;
	const FSubPaints P = Paints();

	const FVector Hatch = GetHatchLocation();
	const float PuddleRadius = static_cast<float>(DeepRoomLayout::HatchRadiusCm(Sub) * PuddleRadii);
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
	return GetActorLocation().Z + DeepRoomLayout::DeckTopCm(UDeepTuningSettings::Get().Sub);
}

FTransform ADeepSubRoom::GetRoomStartTransform() const
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	const FVector2D At = T.RoomStartM * UDeepRules::CmPerM;
	const FVector Start = Local(At.X, At.Y, DeepRoomLayout::DeckTopCm(T) + 92.0);
	return FTransform(FRotator(0.f, GetActorRotation().Yaw, 0.f), Start);
}

FVector ADeepSubRoom::GetEnterStandLocation() const
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	const FVector2D At = T.EnterStandM * UDeepRules::CmPerM;
	return Local(At.X, At.Y, DeepRoomLayout::DeckTopCm(T) + 92.0);
}

FVector ADeepSubRoom::GetHatchLocation() const
{
	return Local(0.0, 0.0, DeepRoomLayout::DeckTopCm(UDeepTuningSettings::Get().Sub));
}

FVector ADeepSubRoom::GetValveLocation() const
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	return Local(T.ValveM.X * UDeepRules::CmPerM, T.ValveM.Y * UDeepRules::CmPerM, DeepRoomLayout::DeckTopCm(T) + 10.0);
}

FVector ADeepSubRoom::GetConsoleLocation() const
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	return Local(T.ConsoleM.X * UDeepRules::CmPerM, T.ConsoleM.Y * UDeepRules::CmPerM, DeepRoomLayout::DeckTopCm(T) + 100.0);
}

FVector ADeepSubRoom::GetHelmLocation() const
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	return Local(T.HelmM.X * UDeepRules::CmPerM, T.HelmM.Y * UDeepRules::CmPerM, DeepRoomLayout::DeckTopCm(T) + 90.0);
}

bool ADeepSubRoom::IsInRoom(const FVector& WorldCm, double MarginCm) const
{
	const FBox In = DeepRoomLayout::Inside(UDeepTuningSettings::Get().Sub);
	const FVector P = GetActorTransform().InverseTransformPosition(WorldCm);
	return P.X >= In.Min.X && P.X <= In.Max.X && P.Y >= In.Min.Y && P.Y <= In.Max.Y && P.Z >= In.Min.Z - MarginCm && P.Z <= In.Max.Z;
}

bool ADeepSubRoom::IsWithinUseRange(const FVector& WorldCm, const FVector& PointCm) const
{
	return FVector::DistXY(WorldCm, PointCm) <= UDeepTuningSettings::Get().Sub.UseRangeM * UDeepRules::CmPerM;
}

EDeepRoomStation ADeepSubRoom::GetStationInReach(const FVector& WorldCm) const
{
	const TPair<EDeepRoomStation, FVector> Stations[] = {
		{ EDeepRoomStation::Helm, GetHelmLocation() }, { EDeepRoomStation::Console, GetConsoleLocation() }, { EDeepRoomStation::Valve, GetValveLocation() } };
	EDeepRoomStation Best = EDeepRoomStation::None;
	double BestCm = UDeepTuningSettings::Get().Sub.UseRangeM * UDeepRules::CmPerM;
	for (const TPair<EDeepRoomStation, FVector>& Station : Stations)
	{
		const double Cm = FVector::DistXY(WorldCm, Station.Value);
		if (Cm < BestCm || (Best == EDeepRoomStation::None && Cm <= BestCm))
		{
			Best = Station.Key;
			BestCm = Cm;
		}
	}
	return Best;
}

void ADeepSubRoom::SetRoomDressVisible(bool bVisible)
{
	DeepRoomDress::SetVisible(RoomDressMeshes, RoomDressLights, bVisible);
}

void ADeepSubRoom::SetRoofVisible(bool bVisible)
{
	Ceiling->SetVisibility(bVisible);
	for (UStaticMeshComponent* Shape : StripFixtures)
	{
		Shape->SetVisibility(bVisible);
	}
}
