#include "World/DeepSubmarine.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Components/StaticMeshComponent.h"
#include "Diver/DeepRunComponent.h"
#include "EngineUtils.h"
#include "Game/DeepGameState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "World/DeepAbyssFloor.h"
#include "World/DeepSalvage.h"
#include "World/DeepHull.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LocalLightComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Sim/DeepCaveRules.h"
#include "Sim/DeepMovingSubRules.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepCaveString.h"
#include "World/DeepShaft.h"
#include "World/DeepShapes.h"
#include "World/DeepArena.h"
#include "World/DeepSubParts.h"

namespace
{
	using namespace DeepSubParts;
	constexpr double SubWinZ = WinZ;
	constexpr double SubWinHalfW = WinHalfW;
	constexpr double SubWinHalfH = WinHalfH;

	/** The room's outer half extents (cm): the colliding box the dress is built round. */
	FVector HullOuterCm(const FDeepSubTuning& T)
	{
		const double M = UDeepRules::CmPerM;
		return FVector((T.RoomHalfLengthM + T.WallThicknessM) * M, (T.RoomHalfWidthM + T.WallThicknessM) * M, (T.RoomHeightM * 0.5 + T.WallThicknessM) * M);
	}

	TArray<FDeepHullPart> HullParts()
	{
		const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
		return DeepHull::Parts(HullOuterCm(Tuning.Sub), Tuning.Sub.HatchHalfSizeM * UDeepRules::CmPerM, -Tuning.Arena.AnchorBelowSubM * UDeepRules::CmPerM);
	}
}

ADeepSubmarine::ADeepSubmarine()
{
	// Ticks only for the look: marker blink and the helm camera's sway.
	PrimaryActorTick.bCanEverTick = true;
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
	// The front wall is four pieces round the helm window: WallFront is the part under it, the last three frame it.
	for (const TCHAR* Name : { TEXT("WallFront"), TEXT("WallBack"), TEXT("WallLeft"), TEXT("WallRight"), TEXT("WallFrontTop"), TEXT("WallFrontPort"), TEXT("WallFrontStarboard") })
	{
		Walls.Add(Make(Name, DeepShapes::Cube, true));
	}
	Ceiling = Make(TEXT("Ceiling"), DeepShapes::Cube, true);
	for (const TCHAR* Name : { TEXT("RimA"), TEXT("RimB"), TEXT("RimC"), TEXT("RimD") })
	{
		HatchRim.Add(Make(Name, DeepShapes::Cube, false));
	}
	HatchCover = Make(TEXT("HatchCover"), DeepShapes::Cube, false);
	// The one water surface: a black, near-mirror pool in the open hatch that reflects the lit room. No collision.
	HatchWater = Make(TEXT("HatchWater"), DeepShapes::Cube, false);
	HatchWater->SetCastShadow(false);
	// The glass in the helm window: solid, so the opening is closed.
	Window = Make(TEXT("Window"), DeepShapes::Cube, true);
	// The nose and tower are only silhouette from the water; the nose has no collision so it cannot reach into the room.
	NoseCone = Make(TEXT("NoseCone"), DeepShapes::Sphere, false);
	Tower = Make(TEXT("Tower"), DeepShapes::Cube, true);

	AirlockLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("AirlockLight"));
	AirlockLight->SetupAttachment(Root);
	AirlockLight->SetRelativeLocation(FVector(0.f, 0.f, -350.f));
	AirlockLight->SetMobility(EComponentMobility::Movable);
	AirlockLight->SetLightColor(FLinearColor(1.f, 0.75f, 0.45f));
	AirlockLight->SetCastShadows(false);

	// The belly flood: straight down from the hull, what shows at 300 and 500 m when the lamp looks away.
	BellyFlood = CreateDefaultSubobject<USpotLightComponent>(TEXT("BellyFlood"));
	BellyFlood->SetupAttachment(Root);
	BellyFlood->SetMobility(EComponentMobility::Movable);
	BellyFlood->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	BellyFlood->SetCastShadows(true);

	HelmCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("HelmCamera"));
	HelmCamera->SetupAttachment(Root);

	Window->SetCastShadow(false);
	for (const TCHAR* Name : { TEXT("BowLightPort"), TEXT("BowLightStarboard") })
	{
		USpotLightComponent* Light = CreateDefaultSubobject<USpotLightComponent>(Name);
		Light->SetupAttachment(Root);
		Light->SetMobility(EComponentMobility::Movable);
		// They become the helm floods: their shadows and fog shadows keep the lit water inside the rock (rescope/light).
		Light->SetCastShadows(true);
		Light->SetCastVolumetricShadow(true);
		BowLights.Add(Light);
	}
	for (const TCHAR* Name : { TEXT("HullLightPort"), TEXT("HullLightStarboard") })
	{
		USpotLightComponent* Light = CreateDefaultSubobject<USpotLightComponent>(Name);
		Light->SetupAttachment(Root);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetCastShadows(false);
		HullLights.Add(Light);
	}
	for (const TCHAR* Name : { TEXT("MarkerPort"), TEXT("MarkerStarboard") })
	{
		UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(Name);
		Light->SetupAttachment(Root);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetCastShadows(false);
		Markers.Add(Light);
	}

	// The outside dress: visual only (CreateShape gives no collision), so the colliding box stays the hull.
	for (const FDeepHullPart& Part : HullParts())
	{
		UStaticMeshComponent* Shape = Make(*Part.Name.ToString(), Part.Mesh, false);
		Shape->SetCastShadow(Part.bShadow);
		// Small parts (rods, straps, plates) stay out of ray tracing and Lumen's scene: they cost per instance and
		// nobody sees them in a reflection. The big ones (foam, tanks, ducts, collar) stay in.
		Shape->bAffectDistanceFieldLighting = Part.bShadow;
		Shape->bVisibleInRayTracing = Part.bShadow;
		Shape->bAffectDynamicIndirectLighting = Part.bShadow;
		HullDress.Add(Shape);
	}
	// BT-01, the bathyscaphe: Deep.Sub.Model 1 (default) shows it, makes it the collider and hides the dress above
	// and the box colliders (World/DeepHull, OnHullModelSwitched).
	HullModel = CreateDefaultSubobject<UDeepHullModel>(TEXT("HullModel"));
	HullModel->SetupAttachment(Root);
	// A bulb on every outside lamp, at the light: a small glowing sphere, shown with the model (on the box the lamp
	// housings stand in for it). No collision, no shadow; the markers' bulbs blink with their lights.
	{
		ULocalLightComponent* Lights[] = { BowLights[0], BowLights[1], HullLights[0], HullLights[1], Markers[0], Markers[1], AirlockLight, BellyFlood };
		static_assert(UE_ARRAY_COUNT(Lights) == uint8(DeepHull::ELamp::Count), "One bulb per lamp");
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Lights); ++Index)
		{
			UStaticMeshComponent* Bulb = DeepShapes::CreateShape(this, FName(TEXT("Bulb_") + Lights[Index]->GetName()), DeepShapes::Sphere);
			Bulb->SetupAttachment(Lights[Index]);
			Bulb->SetRelativeScale3D(FVector(DeepHull::BulbRadiusCm * 2.0 / DeepShapes::ShapeSizeCm));
			Bulb->SetCastShadow(false);
			Bulb->bAffectDistanceFieldLighting = false;
			Bulb->bVisibleInRayTracing = false;
			Bulb->bAffectDynamicIndirectLighting = false;
			Bulb->SetHiddenInGame(true);
			Bulbs.Add(Bulb);
		}
	}
	for (int32 Index = 0; Index < BowLights.Num(); ++Index)
	{
		UStaticMeshComponent* Shape = DeepShapes::CreateShape(this, Index == 0 ? TEXT("Hull_BowHousingPort") : TEXT("Hull_BowHousingStbd"), DeepShapes::Cylinder);
		Shape->SetupAttachment(BowLights[Index]);
		// The light faces +X: the housing sits behind it along that axis, its open end at the light.
		Shape->SetRelativeLocation(FVector(-16.0, 0.0, 0.0));
		Shape->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
		Shape->SetRelativeScale3D(FVector(24.0, 24.0, 30.0) / DeepShapes::ShapeSizeCm);
		Shape->SetCastShadow(false);
		BowHousings.Add(Shape);
	}

	// The trench wall ahead of the helm: built at BeginPlay, shown only through the helm camera.
	Trench = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Trench"));
	Trench->SetupAttachment(Root);
	Trench->SetMobility(EComponentMobility::Movable);
	Trench->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Trench->SetGenerateOverlapEvents(false);
	Trench->bAffectDistanceFieldLighting = false;
	Trench->SetVisibility(false);

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
	if (Walls.Num() == 7)
	{
		const double WinBottom = SubWinZ - SubWinHalfH;
		const double WinTop = SubWinZ + SubWinHalfH;
		Place(Walls[0], FVector(L + Th * 0.5, 0.0, (-H * 0.5 + WinBottom) * 0.5), FVector(Th, OuterW * 2.0, WinBottom + H * 0.5));
		Place(Walls[1], FVector(-L - Th * 0.5, 0.0, 0.0), FVector(Th, OuterW * 2.0, H));
		Place(Walls[2], FVector(0.0, W + Th * 0.5, 0.0), FVector(L * 2.0, Th, H));
		Place(Walls[3], FVector(0.0, -W - Th * 0.5, 0.0), FVector(L * 2.0, Th, H));
		Place(Walls[4], FVector(L + Th * 0.5, 0.0, (WinTop + H * 0.5) * 0.5), FVector(Th, OuterW * 2.0, H * 0.5 - WinTop));
		Place(Walls[5], FVector(L + Th * 0.5, -(SubWinHalfW + OuterW) * 0.5, SubWinZ), FVector(Th, OuterW - SubWinHalfW, SubWinHalfH * 2.0));
		Place(Walls[6], FVector(L + Th * 0.5, (SubWinHalfW + OuterW) * 0.5, SubWinZ), FVector(Th, OuterW - SubWinHalfW, SubWinHalfH * 2.0));
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
	const FDeepLookTuning& Look = UDeepTuningSettings::Get().Look;
	Place(HatchWater, FVector(0.0, 0.0, DeckTop - Look.HatchWaterBelowDeckCm - 0.5), FVector(Hh * 2.0, Hh * 2.0, 1.0));
	// Under the hull, ahead of the hatch so the diver's own column is not a white-out.
	BellyFlood->SetRelativeLocation(FVector(150.0, 0.0, DeckZ - Th));
	Place(Window, FVector(L + Th * 0.5, 0.0, SubWinZ), FVector(2.0, SubWinHalfW * 2.0, SubWinHalfH * 2.0));

	// The bow sits under the window line so the window looks out over it, not into it.
	const double NoseTop = SubWinZ - SubWinHalfH - 15.0;
	const double NoseBottom = -H * 0.5 - Th;
	// Narrower than the hull: a chin fairing under the window, the stowed arm along its starboard side (DeepHull).
	Place(NoseCone, FVector(OuterL + 140.0, 0.0, (NoseTop + NoseBottom) * 0.5), FVector(280.0, OuterW * 2.0 * 0.68, NoseTop - NoseBottom));

	// Bow lights under the chin, aimed ahead and down; hull markers high on each side.
	for (int32 Index = 0; Index < BowLights.Num(); ++Index)
	{
		BowLights[Index]->SetRelativeLocation(FVector(OuterL + 120.0, Index == 0 ? -90.0 : 90.0, NoseBottom - 5.0));
		BowLights[Index]->SetRelativeRotation(FRotator(-18.f, Index == 0 ? -8.f : 8.f, 0.f));
	}
	// Hull work lights in the work-lamp heads on the top rails (DeepHull: WorkLamp), aimed down and a little out.
	for (int32 Index = 0; Index < HullLights.Num(); ++Index)
	{
		const double Sgn = Index == 0 ? -1.0 : 1.0;
		HullLights[Index]->SetRelativeLocation(FVector(95.0, Sgn * (OuterW + 14.0), H * 0.5 + Th - 2.0));
		HullLights[Index]->SetRelativeRotation(FRotator(-72.f, Index == 0 ? -90.f : 90.f, 0.f));
	}
	for (int32 Index = 0; Index < Markers.Num(); ++Index)
	{
		Markers[Index]->SetRelativeLocation(FVector(L - 50.0, Index == 0 ? -OuterW - 12.0 : OuterW + 12.0, H * 0.5 - 20.0));
	}
	AirlockLight->SetRelativeLocation(FVector(0.0, 0.0, -350.0));
	// With the model hull every lamp sits on its surface instead (the box positions above would float or be buried).
	if (IsModelHull())
	{
		PlaceLampsOnModel();
	}

	// The outside dress, from the same part list the constructor made the components from.
	const TArray<FDeepHullPart> Parts = HullParts();
	for (int32 Index = 0; Index < Parts.Num() && Index < HullDress.Num(); ++Index)
	{
		Place(HullDress[Index], Parts[Index].Centre, Parts[Index].Size);
		HullDress[Index]->SetRelativeRotation(Parts[Index].Rotation);
	}

	Place(Tower, FVector(100.0, 0.0, H * 0.5 + Th + 110.0), FVector(300.0, 140.0, 220.0));

	HelmCamera->SetRelativeLocation(T.HelmCameraM * M);
	HelmCamera->SetRelativeRotation(FRotator(T.HelmCameraPitchDeg, 0.f, 0.f));
	HelmCamera->SetFieldOfView(T.HelmCameraFovDeg);
}

void ADeepSubmarine::SetOutsideLights(float Scale, float Fog)
{
	const FDeepArenaTuning& Arena = UDeepTuningSettings::Get().Arena;
	if (Arena.bGreyBox)
	{
		return;
	}
	const FDeepLookTuning& Look = UDeepTuningSettings::Get().Look;
	const float S = FMath::Max(0.f, Scale);
	AirlockLight->SetIntensity(Arena.SubLightIntensity * S);
	// Dimmer is also smaller: at 500 m the airlock light marks the hatch rim and nothing else.
	AirlockLight->SetAttenuationRadius(Arena.SubLightRadiusM * UDeepRules::CmPerM * FMath::Clamp(0.4f + 0.6f * S, 0.4f, 1.f));
	AirlockLight->SetVolumetricScatteringIntensity(FMath::Max(0.f, Fog));
	BellyFlood->SetIntensity(Look.BellyFloodIntensity * S);
	BellyFlood->SetVisibility(S > 0.f);
	for (USpotLightComponent* Light : HullLights)
	{
		Light->SetIntensity(Look.HullLightIntensity * S);
		Light->SetVisibility(S > 0.f);
		Light->SetVolumetricScatteringIntensity(FMath::Max(0.f, Fog) * 0.5f);
	}
	OutsideScale = S;
	OutsideFog = FMath::Max(0.f, Fog);
	if (bHelmManned)
	{
		// The helm floods own the bow lights while someone drives; the depth fade applies again when they stand up.
		return;
	}
	for (USpotLightComponent* Light : BowLights)
	{
		Light->SetIntensity(Look.BowLightIntensity * S);
		Light->SetVisibility(S > 0.f);
		Light->SetVolumetricScatteringIntensity(OutsideFog);
	}
}

float ADeepSubmarine::HelmSwayScale(float SpeedMps, float InDriveSpeedMps)
{
	return InDriveSpeedMps > UE_KINDA_SMALL_NUMBER ? FMath::Clamp(FMath::Abs(SpeedMps) / InDriveSpeedMps, 0.f, 1.f) : 0.f;
}

float ADeepSubmarine::HelmStreakM(float SpeedMps, float ShutterSeconds)
{
	return FMath::Abs(SpeedMps) * FMath::Max(0.f, ShutterSeconds);
}

UProceduralMeshComponent* ADeepSubmarine::GetTrench() const
{
	return Trench;
}

bool ADeepSubmarine::TrenchShows(bool bHelmView, bool bInRoomView, bool bParked)
{
	return bHelmView || (bInRoomView && !bParked);
}

void ADeepSubmarine::SetHullDressVisible(bool bVisible)
{
	for (UStaticMeshComponent* Shape : HullDress)
	{
		Shape->SetVisibility(bVisible);
	}
	for (UStaticMeshComponent* Shape : BowHousings)
	{
		Shape->SetVisibility(bVisible);
	}
}

float ADeepSubmarine::TrenchReliefM(float AlongM, float DepthM, float LedgeEveryM, float LedgeOutM)
{
	const float A = AlongM;
	const float D = DepthM;
	// Rock: big bulges, then smaller crossed waves (m out from the base plane, toward the sub).
	float R = 1.4f * FMath::Sin(0.07f * A + 0.045f * D + 0.6f)
		+ 0.6f * FMath::Sin(0.37f * A + 0.21f * D) * FMath::Sin(0.29f * D - 0.5f)
		+ 0.3f * FMath::Sin(1.1f * A - 0.7f * D + 1.3f)
		+ 0.14f * FMath::Sin(2.3f * A + 1.9f * D)
		+ 0.06f * FMath::Sin(4.7f * A - 3.1f * D + 0.4f);
	// Strata: a small step every 2.5 m.
	R += 0.18f * FMath::Pow(FMath::Frac(D / 2.5f), 3.f);
	// A ledge at every LedgeEveryM: a near-flat top that catches light, an underside that recedes over 2 m.
	if (LedgeEveryM > 1.f && LedgeOutM > 0.f)
	{
		const float Q = FMath::Fmod(D, LedgeEveryM);
		const float Level = FMath::RoundToFloat(D / LedgeEveryM);
		float Out = 0.f;
		if (Q < 2.f)
		{
			Out = FMath::Pow(1.f - Q / 2.f, 1.5f);
		}
		else if (Q > LedgeEveryM - 0.4f)
		{
			Out = (Q - (LedgeEveryM - 0.4f)) / 0.4f;
		}
		// Each ledge wanders along the wall, so they read as rock and not as shelving.
		R += LedgeOutM * Out * (0.65f + 0.35f * FMath::Sin(0.19f * A + Level * 1.7f));
	}
	return R;
}

void ADeepSubmarine::BuildTrench()
{
	const FDeepLookTuning& Look = UDeepTuningSettings::Get().Look;
	const double CellM = 0.5;
	const double TopM = 40.0;
	const double BottomM = FMath::Max(TopM + 10.0, double(Look.TrenchBottomM));
	const double HalfW = FMath::Max(5.f, Look.TrenchWidthM * 0.5f);
	const int32 Cols = FMath::CeilToInt(2.0 * HalfW / CellM) + 1;
	const int32 Rows = FMath::CeilToInt((BottomM - TopM) / CellM) + 1;
	const float Every = Look.TrenchLedgeEveryM;
	const float Out = Look.TrenchLedgeOutM;

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FProcMeshTangent> Tangents;
	TArray<int32> Triangles;
	Vertices.Reserve(Cols * Rows);
	Normals.Reserve(Cols * Rows);
	UV0.Reserve(Cols * Rows);
	Tangents.Reserve(Cols * Rows);
	Triangles.Reserve((Cols - 1) * (Rows - 1) * 6);
	const float Eps = 0.1f;
	for (int32 j = 0; j < Rows; ++j)
	{
		const float D = float(TopM + j * CellM);
		for (int32 i = 0; i < Cols; ++i)
		{
			const float A = float(-HalfW + i * CellM);
			const float R = TrenchReliefM(A, D, Every, Out);
			const float Ra = (TrenchReliefM(A + Eps, D, Every, Out) - TrenchReliefM(A - Eps, D, Every, Out)) / (2.f * Eps);
			const float Rd = (TrenchReliefM(A, D + Eps, Every, Out) - TrenchReliefM(A, D - Eps, Every, Out)) / (2.f * Eps);
			// Local: the face looks along -X (toward the sub), Y along the wall, Z = -absolute depth.
			Vertices.Add(FVector(-R, A, -D) * UDeepRules::CmPerM);
			Normals.Add(FVector(-1.0, -Ra, Rd).GetSafeNormal());
			Tangents.Add(FProcMeshTangent(FVector(-Ra, 1.0, 0.0).GetSafeNormal(), false));
			UV0.Add(FVector2D(A, D) / 2.0);
		}
	}
	for (int32 j = 0; j < Rows - 1; ++j)
	{
		for (int32 i = 0; i < Cols - 1; ++i)
		{
			const int32 V00 = j * Cols + i;
			const int32 V10 = V00 + 1;
			const int32 V01 = V00 + Cols;
			const int32 V11 = V01 + 1;
			// Faces -X: Unreal's face normal is (P1 - P2) x (P0 - P2).
			Triangles.Append({ V00, V01, V10, V10, V01, V11 });
		}
	}
	Trench->ClearAllMeshSections();
	Trench->CreateMeshSection(0, Vertices, Triangles, Normals, UV0, TArray<FColor>(), Tangents, /*bCreateCollision*/ false);
	Trench->SetCastShadow(false);
	// Worn steel's noise works from the object's pivot, so it rides with the wall; painted rock-grey with no rust it
	// reads as grimy stone. Falls back to a flat tint.
	UMaterialInterface* Base = Cast<UMaterialInterface>(Look.SteelMaterial.TryLoad());
	if (Base)
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Trench);
		M->SetVectorParameterValue(TEXT("PaintColor"), Look.TrenchColor);
		M->SetScalarParameterValue(TEXT("Wear"), 0.f);
		M->SetScalarParameterValue(TEXT("Rust"), 0.f);
		M->SetScalarParameterValue(TEXT("Wet"), 0.25f);
		M->SetScalarParameterValue(TEXT("PaintRoughness"), 0.75f);
		M->SetScalarParameterValue(TEXT("PaintMetal"), 0.f);
		M->SetScalarParameterValue(TEXT("PuddleRadius"), 0.f);
		Trench->SetMaterial(0, M);
	}
	else if (UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Basic, Trench);
		M->SetVectorParameterValue(TEXT("Color"), Look.TrenchColor);
		Trench->SetMaterial(0, M);
	}
}

void ADeepSubmarine::TickHelmLook(float DeltaSeconds)
{
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const FDeepLookTuning& Look = Tuning.Look;
	const FDeepSubTuning& Sub = Tuning.Sub;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const ADeepGameState* State = World->GetGameState<ADeepGameState>();
	const float DepthM = State ? State->SubDepthM : 0.f;
	const bool bParked = State ? State->bParked : true;

	// Speed from the replicated depth, so every machine agrees; smoothed so the look eases in and out.
	float Raw = 0.f;
	if (LastDepthM >= 0.f && DeltaSeconds > UE_KINDA_SMALL_NUMBER && !bParked)
	{
		Raw = (DepthM - LastDepthM) / DeltaSeconds;
		// A stop jump (Deep.GotoStop) is not driving.
		if (FMath::Abs(Raw) > Sub.DriveSpeedMps * 3.f)
		{
			Raw = 0.f;
		}
	}
	LastDepthM = DepthM;
	DriveSpeedMps = FMath::FInterpTo(DriveSpeedMps, Raw, DeltaSeconds, 4.f);
	if (bParked && FMath::Abs(DriveSpeedMps) < 0.02f)
	{
		DriveSpeedMps = 0.f;
	}

	// Manned: any diver seated at the helm (replicated), so the floods show on every machine.
	bool bManned = false;
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		const UDeepRunComponent* Run = It->FindComponentByClass<UDeepRunComponent>();
		if (Run && Run->IsSeated())
		{
			bManned = true;
			break;
		}
	}
	const bool bGreyBox = Tuning.Arena.bGreyBox;
	bManned = bManned && !bGreyBox;
	if (bManned != bHelmManned)
	{
		bHelmManned = bManned;
		for (int32 Index = 0; Index < BowLights.Num(); ++Index)
		{
			USpotLightComponent* Light = BowLights[Index];
			const float Yaw = Index == 0 ? -8.f : 8.f;
			if (bManned)
			{
				Light->SetIntensity(Look.HelmFloodIntensity);
				Light->SetInnerConeAngle(Look.HelmFloodConeDeg * 0.45f);
				Light->SetOuterConeAngle(Look.HelmFloodConeDeg);
				Light->SetAttenuationRadius(Look.HelmFloodRangeM * UDeepRules::CmPerM);
				Light->SetVolumetricScatteringIntensity(Look.HelmFloodFog);
				// Port lights the wall ahead, starboard the bed below: two lights, two jobs.
				Light->SetRelativeRotation(FRotator(Index == 0 ? Look.HelmFloodPitchDeg : Look.HelmFloodLowPitchDeg, Yaw * 0.6f, 0.f));
				Light->SetVisibility(true);
			}
			else
			{
				Light->SetIntensity(Look.BowLightIntensity * OutsideScale);
				Light->SetInnerConeAngle(Look.BowLightConeDeg * 0.4f);
				Light->SetOuterConeAngle(Look.BowLightConeDeg);
				Light->SetAttenuationRadius(Look.BowLightRangeM * UDeepRules::CmPerM);
				Light->SetVolumetricScatteringIntensity(OutsideFog);
				Light->SetRelativeRotation(FRotator(-18.f, Yaw, 0.f));
				Light->SetVisibility(OutsideScale > 0.f);
			}
		}
	}

	// The trench wall: only for a local player looking through this sub's helm camera (a diver outside never sees it).
	// Also through the window, for a local player standing in the room while the sub moves (the hatch is shut then, so
	// no diver is outside to find the wall missing). Parked, the window shows the stop's open water.
	const APlayerController* PC = World->GetFirstPlayerController();
	// The room is a fixed set now (World/DeepSubRoom) with a blank porthole: no trench through its window.
	const bool bInRoomView = false;
	const bool bLocalTrenchView = !bGreyBox && PC && TrenchShows(PC->GetViewTarget() == this, bInRoomView, bParked);
	// The cave shaft (ADeepShaft) replaces the trench: the rock is all round the sub, not a cliff ahead.
	const bool bLocalHelmView = bLocalTrenchView && !DeepShaft::IsOn(World) && !DeepArena::IsWorldStill(World);
	if (bLocalHelmView && Trench->GetNumSections() == 0)
	{
		BuildTrench();
	}
	if (Trench->IsVisible() != bLocalHelmView)
	{
		Trench->SetVisibility(bLocalHelmView);
	}
	if (bLocalHelmView)
	{
		// The point at absolute depth d sits (DepthM - d) above the sub: the wall slides up as the sub goes down.
		const FVector Cam = Sub.HelmCameraM * UDeepRules::CmPerM;
		Trench->SetRelativeLocation(FVector(Cam.X + Look.TrenchAheadM * UDeepRules::CmPerM, 0.0, DepthM * UDeepRules::CmPerM));
		Trench->SetRelativeRotation(FRotator(0.f, Look.TrenchYawDeg, 0.f));
	}

	// The loaded stop's sea bed lies 8 m under the anchor: once the sub is below that stop (leaving it downward, or
	// climbing up to it) the sub would pass through the bed, so it is hidden until the sub is at or above the stop.
	if (State)
	{
		const bool bBelowStop = DepthM > Tuning.GetStop(State->Stop).SubDepthM + 0.5f;
		// Deep.World.Still: every stop has its own bed, shown by its own depth (DeepArena::UpdateStillWorld).
		for (TActorIterator<ADeepAbyssFloor> It(World); It && !DeepArena::IsWorldStill(World); ++It)
		{
			if (It->IsHidden() != bBelowStop)
			{
				It->SetActorHiddenInGame(bBelowStop);
			}
		}
		// The salvage lying on that bed goes with it (a crate hanging in open water is worse than none).
		for (TActorIterator<ADeepSalvage> It(World); It; ++It)
		{
			if (It->IsHidden() != bBelowStop)
			{
				It->SetActorHiddenInGame(bBelowStop);
			}
		}
	}

	// Settle: when the sub parks, the camera dips and recovers once.
	if (bParked && !bWasParked)
	{
		SettleAge = 0.f;
	}
	bWasParked = bParked;
	SettleAge += DeltaSeconds;
	const float Secs = FMath::Max(0.1f, Look.HelmSettleSeconds);
	const float Env = SettleAge < Secs * 1.5f ? FMath::Exp(-4.f * SettleAge / Secs) * FMath::Sin(UE_TWO_PI * 0.9f * SettleAge) : 0.f;

	// Sway scaled by speed (none parked), plus a faint engine shiver.
	const double T = World->GetTimeSeconds();
	const float Scale = HelmSwayScale(DriveSpeedMps, Sub.DriveSpeedMps);
	const float A = Look.HelmSwayDeg * Scale;
	const float W = UE_TWO_PI * Look.HelmSwayHz;
	float Shiver = 0.04f * Scale * FMath::Sin(UE_TWO_PI * 7.3 * T) + 0.03f * Scale * FMath::Sin(UE_TWO_PI * 11.1 * T + 0.8);

	// The cave shaft: a dent jolts the camera (bigger dents, bigger jolt), dying away; rubbing the rock shivers it.
	const FDeepCaveTuning& Cave = Tuning.Cave;
	const int32 Dents = State ? State->HullDamage : 0;
	if (LastHullDamageSeen >= 0 && Dents > LastHullDamageSeen)
	{
		// Dents back to impact speed: damage is about speed * DamagePerMps.
		const float Mps = float(Dents - LastHullDamageSeen) / FMath::Max(0.05f, Cave.DamagePerMps);
		JoltDeg = FMath::Max(JoltDeg, Cave.JoltDegPerMps * Mps);
		JoltAge = 0.f;
	}
	LastHullDamageSeen = Dents;
	JoltAge += DeltaSeconds;
	JoltDeg *= FMath::Exp(-FMath::Max(0.f, Cave.JoltDecayPerSecond) * DeltaSeconds);
	const float Jolt = JoltDeg * FMath::Cos(UE_TWO_PI * 6.f * JoltAge);
	if (State && !bParked && DeepArena::IsWorldCaves(World))
	{
		// The cave string: rubbing (or all but touching) the chunks' rock, from the replicated pose.
		const ADeepCaveString* String = DeepCaveString::Find(World);
		const FDeepCavePose Pose = DeepCaveString::HullPose(*State);
		if (String && String->OverlapsHull(GetSolidBoundsLocal(false), Pose.LocationCm, Pose.YawDeg, Tuning.CaveString.ShiverNearCm))
		{
			Shiver += 0.25f * FMath::Sin(UE_TWO_PI * 23.f * T) + 0.15f * FMath::Sin(UE_TWO_PI * 37.f * T + 1.3);
		}
	}
	else if (State && !bParked && DeepShaft::IsOn(World))
	{
		FVector BoxMin, BoxMax;
		DeepShaft::SubBoxM(this, BoxMin, BoxMax);
		const TArray<float> Stops = Tuning.StopDepthsM();
		const FDeepCaveContact Touch = UDeepCaveRules::Contact(State->SubOffsetM, DepthM, BoxMin, BoxMax, Stops, UDeepCaveRules::MinRadiusM(BoxMin, BoxMax, Cave), Cave);
		if (Touch.bTouching || Touch.PenetrationM > -0.05f)
		{
			Shiver += 0.25f * FMath::Sin(UE_TWO_PI * 23.f * T) + 0.15f * FMath::Sin(UE_TWO_PI * 37.f * T + 1.3);
		}
	}
	HelmCamera->SetRelativeRotation(FRotator(
		Sub.HelmCameraPitchDeg + A * 0.5f * FMath::Sin(W * 0.73 * T + 1.1) + Shiver - Look.HelmSettleDeg * Env + Jolt,
		A * 0.3f * FMath::Sin(W * 0.41 * T) + Jolt * 0.4f,
		A * FMath::Sin(W * T) + Shiver * 0.5f - Jolt * 0.8f));
	HelmCamera->SetRelativeLocation(Sub.HelmCameraM * UDeepRules::CmPerM - FVector(0.0, 0.0, Look.HelmSettleCm * Env));
}

void ADeepSubmarine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const FDeepLookTuning& Look = UDeepTuningSettings::Get().Look;
	const double T = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	// Markers blink: on for the first half of each cycle. Their own light does not fade with depth: they mark the hull.
	const bool bOn = FMath::Frac(T * Look.MarkerHz) < 0.5;
	for (UPointLightComponent* Light : Markers)
	{
		Light->SetVisibility(bOn && !UDeepTuningSettings::Get().Arena.bGreyBox);
	}
	// A bulb glows while its light is on: the markers' blink, the floods off in grey box or the water look's dimming.
	for (UStaticMeshComponent* Bulb : Bulbs)
	{
		const ULightComponent* Light = Cast<ULightComponent>(Bulb->GetAttachParent());
		Bulb->SetVisibility(Light && Light->GetVisibleFlag() && Light->Intensity > 0.f);
	}
	// Deep.World.Still: every machine places the hull from the replicated depth, offset and yaw.
	PlaceStill();
	DeepArena::UpdateStillWorld(GetWorld());
	TickHelmLook(DeltaSeconds);
}

void ADeepSubmarine::ApplyLook()
{
	const FDeepLookTuning& Look = UDeepTuningSettings::Get().Look;
	const FDeepSubTuning& Sub = UDeepTuningSettings::Get().Sub;

	// Surfaces (style guide 4): the paints the room uses too (World/DeepSubParts); wet near the hatch.
	const FSubPaints SubPaints = DeepSubParts::Paints();
	const FSubSteel& Wall = SubPaints.Wall;
	const FSubSteel& Floor = SubPaints.Floor;
	const FSubSteel& Ceil = SubPaints.Ceil;
	const FSubSteel& Rim = SubPaints.Rim;
	const FSubSteel& Cover = SubPaints.Cover;

	const FVector Hatch = GetHatchLocation();
	const float PuddleRadius = Sub.HatchHalfSizeM * UDeepRules::CmPerM * 3.2f;
	for (UStaticMeshComponent* Shape : Deck)
	{
		SubSteel(Shape, Floor, Hatch, PuddleRadius);
	}
	for (UStaticMeshComponent* Shape : Walls)
	{
		SubSteel(Shape, Wall);
	}
	SubSteel(Ceiling, Ceil);
	for (UStaticMeshComponent* Shape : HatchRim)
	{
		SubSteel(Shape, Rim);
	}
	SubSteel(HatchCover, Cover);
	SubSteel(NoseCone, Wall);
	SubSteel(Tower, Ceil);

	// The outside dress (style guide 4): one shared instance per paint. Saturation under 35%, value under 70%.
	{
		FSubSteel Paints[uint8(EDeepHullPaint::Count)];
		Paints[uint8(EDeepHullPaint::Hull)] = Wall;
		FSubSteel& FrameP = Paints[uint8(EDeepHullPaint::Frame)];
		FrameP.Paint = FLinearColor(0.028f, 0.03f, 0.03f);
		FrameP.Wear = 0.38f;
		FrameP.Rust = 0.42f;
		FrameP.Wet = 0.1f;
		FrameP.PaintRough = 0.55f;
		// Syntactic foam: faded, dirty ochre (sRGB about 101, 87, 69; 32% saturation), matte, stained, hardly chipped.
		FSubSteel& FoamP = Paints[uint8(EDeepHullPaint::Foam)];
		FoamP.Paint = FLinearColor(0.13f, 0.095f, 0.06f);
		FoamP.Wear = 0.05f;
		FoamP.Rust = 0.22f;
		FoamP.PaintRough = 0.85f;
		FSubSteel& TankP = Paints[uint8(EDeepHullPaint::Tank)];
		TankP.Paint = FLinearColor(0.062f, 0.066f, 0.062f);
		TankP.Wear = 0.26f;
		TankP.Rust = 0.45f;
		TankP.Wet = 0.1f;
		TankP.PaintRough = 0.6f;
		FSubSteel& DarkP = Paints[uint8(EDeepHullPaint::Dark)];
		DarkP.Paint = FLinearColor(0.006f, 0.006f, 0.006f);
		DarkP.Wear = 0.f;
		DarkP.Rust = 0.f;
		DarkP.PaintRough = 0.45f;
		Paints[uint8(EDeepHullPaint::Rim)] = Rim;
		FSubSteel& PatchP = Paints[uint8(EDeepHullPaint::Patch)];
		PatchP = Wall;
		PatchP.Paint = FLinearColor(0.055f, 0.058f, 0.05f);
		PatchP.Wear = 0.3f;
		PatchP.Rust = 0.62f;
		UMaterialInstanceDynamic* Shared[uint8(EDeepHullPaint::Count)] = {};
		for (uint8 P = 0; P < uint8(EDeepHullPaint::Count); ++P)
		{
			Shared[P] = SteelInstance(this, Paints[P]);
		}
		const TArray<FDeepHullPart> Parts = HullParts();
		for (int32 Index = 0; Index < Parts.Num() && Index < HullDress.Num(); ++Index)
		{
			const uint8 P = uint8(Parts[Index].Paint);
			if (Shared[P])
			{
				HullDress[Index]->SetMaterial(0, Shared[P]);
			}
			else
			{
				DeepShapes::Tint(HullDress[Index], Paints[P].Paint);
			}
		}
		for (UStaticMeshComponent* Shape : BowHousings)
		{
			Shape->SetMaterial(0, Shared[uint8(EDeepHullPaint::Frame)] ? Shared[uint8(EDeepHullPaint::Frame)] : Shape->GetMaterial(0));
		}
	}
	if (UMaterialInterface* Glass = Cast<UMaterialInterface>(Look.GlassMaterial.TryLoad()))
	{
		// Clearer when looked through straight, more mirror toward the frame (M_Glass's own parameters).
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

	// Lights (style guide 3): outside only; the room keeps its own (World/DeepSubRoom).
	const bool bGreyBox = UDeepTuningSettings::Get().Arena.bGreyBox;
	// Outside: bow lights in the lamp's colour, markers red port and green starboard.
	const FDeepLampTuning& Lamp = UDeepTuningSettings::Get().Lamp;
	for (USpotLightComponent* Light : BowLights)
	{
		Light->SetLightColor(Lamp.Color);
		Light->SetIntensity(Look.BowLightIntensity * OutsideScale);
		Light->SetInnerConeAngle(Look.BowLightConeDeg * 0.4f);
		Light->SetOuterConeAngle(Look.BowLightConeDeg);
		Light->SetAttenuationRadius(Look.BowLightRangeM * UDeepRules::CmPerM);
		Light->SetVisibility(!bGreyBox);
	}
	for (USpotLightComponent* Light : HullLights)
	{
		Light->SetLightColor(Lamp.Color);
		Light->SetIntensity(Look.HullLightIntensity * OutsideScale);
		Light->SetInnerConeAngle(Look.HullLightConeDeg * 0.5f);
		Light->SetOuterConeAngle(Look.HullLightConeDeg);
		Light->SetAttenuationRadius(Look.HullLightRangeM * UDeepRules::CmPerM);
		Light->SetVisibility(!bGreyBox);
	}
	for (int32 Index = 0; Index < Markers.Num(); ++Index)
	{
		Markers[Index]->SetLightColor(Index == 0 ? FLinearColor(FColor(0xFF, 0x2A, 0x1A)) : FLinearColor(FColor(0x22, 0xD9, 0x6A)));
		Markers[Index]->SetIntensity(Look.MarkerIntensity);
		Markers[Index]->SetAttenuationRadius(180.f);
		Markers[Index]->SetSourceRadius(3.f);
	}
	// Each bulb glows in its own light's colour (M_SubBulb; a flat tint if the asset is missing).
	UMaterialInterface* BulbBase = LoadObject<UMaterialInterface>(nullptr, DeepHull::BulbMaterial);
	for (UStaticMeshComponent* Bulb : Bulbs)
	{
		const ULightComponent* Light = Cast<ULightComponent>(Bulb->GetAttachParent());
		const FLinearColor Colour = Light ? Light->GetLightColor() : FLinearColor::White;
		if (BulbBase)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(BulbBase, Bulb);
			M->SetVectorParameterValue(TEXT("Color"), Colour);
			M->SetScalarParameterValue(TEXT("Glow"), DeepHull::BulbGlow);
			Bulb->SetMaterial(0, M);
		}
		else
		{
			DeepShapes::Tint(Bulb, Colour);
		}
	}

}

void ADeepSubmarine::BeginPlay()
{
	Super::BeginPlay();
	LayOut();

	// The cave shaft's rock: one per machine (not replicated), placed from the replicated depth and offset.
	if (UWorld* World = GetWorld())
	{
		// Not in a cave world: the chunk string is the rock there (DeepCaveString::Ensure also removes one spawned before the latch).
		bool bHasShaft = DeepArena::IsWorldCaves(World);
		for (TActorIterator<ADeepShaft> It(World); It; ++It)
		{
			bHasShaft = true;
			break;
		}
		if (!bHasShaft)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			World->SpawnActor<ADeepShaft>(GetActorLocation(), GetActorRotation(), Params);
		}
		// The room the crew walks in is a fixed set far above (World/DeepSubRoom); every machine has its own.
		DeepArena::EnsureRoom(World);
	}

	const FDeepArenaTuning& Arena = UDeepTuningSettings::Get().Arena;
	AirlockLight->SetIntensity(Arena.SubLightIntensity);
	AirlockLight->SetAttenuationRadius(Arena.SubLightRadiusM * UDeepRules::CmPerM);

	const FDeepLookTuning& Look = UDeepTuningSettings::Get().Look;
	const FDeepLampTuning& Lamp = UDeepTuningSettings::Get().Lamp;
	BellyFlood->SetVisibility(!Arena.bGreyBox);
	BellyFlood->SetIntensity(Look.BellyFloodIntensity);
	BellyFlood->SetOuterConeAngle(Look.BellyFloodConeDeg);
	BellyFlood->SetInnerConeAngle(Look.BellyFloodConeDeg * 0.5f);
	BellyFlood->SetAttenuationRadius(Look.BellyFloodRangeM * UDeepRules::CmPerM);
	BellyFlood->SetVolumetricScatteringIntensity(Look.BellyFloodVolumetric);
	// Same lamp make as the diver's: cool neutral white.
	BellyFlood->SetLightColor(Lamp.Color);

	ApplyLook();
	if (UMaterialInterface* Water = Cast<UMaterialInterface>(Look.HatchWaterMaterial.TryLoad()))
	{
		HatchWater->SetMaterial(0, Water);
	}
	else
	{
		// No asset (or not cooked): the same near-black, without the mirror.
		DeepShapes::Tint(HatchWater, FLinearColor(0.002f, 0.006f, 0.008f));
	}
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

void ADeepSubmarine::SetHome(const FTransform& Home)
{
	HomeLocation = Home.GetLocation();
	HomeRotation = Home.Rotator();
	bHasHome = true;
}

FTransform ADeepSubmarine::GetHomeTransform() const
{
	return bHasHome ? FTransform(HomeRotation, HomeLocation) : GetActorTransform();
}

void ADeepSubmarine::PlaceStill()
{
	UWorld* World = GetWorld();
	const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
	if (!State || !bHasHome || !DeepArena::IsWorldStill(World))
	{
		return;
	}
	const double DatumCm = UDeepTuningSettings::Get().MovingSub.DepthDatumZM * UDeepRules::CmPerM;
	// Deep.World.Caves: the gauge's depth placed at its true depth (D-065); the same depth with caves off.
	const FTransform Hull = UDeepMovingSubRules::StillHull(GetHomeTransform(), State->SubOffsetM, DeepCaveString::PhysicalDepthM(World, State->SubDepthM), State->SubYawDeg, DatumCm);
	if (!GetActorTransform().Equals(Hull, 0.001))
	{
		// Teleport: the hull is placed, not swept (its walls are the analytic shaft, UDeepCaveRules); nothing inside rides it.
		SetActorTransform(Hull, false, nullptr, ETeleportType::TeleportPhysics);
	}
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

FVector ADeepSubmarine::GetHatchLocation() const
{
	return Local(0.0, 0.0, -UDeepTuningSettings::Get().Sub.RoomHeightM * UDeepRules::CmPerM * 0.5);
}

bool ADeepSubmarine::IsInHatchColumn(const FVector& WorldCm) const
{
	return UDeepRules::InHatchColumn(GetActorTransform().InverseTransformPosition(WorldCm), UDeepTuningSettings::Get().Sub);
}

FBox ADeepSubmarine::GetSolidBoundsLocal(bool bHullOnly) const
{
	FBox Box(ForceInit);
	if (IsModelHull())
	{
		// The model's own simple collision (its convex hulls), placed by the fit.
		const UStaticMesh* Mesh = HullModel->GetStaticMesh();
		const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
		if (Body && Body->AggGeom.GetElementCount() > 0)
		{
			return Body->AggGeom.CalcAABB(HullModel->GetRelativeTransform());
		}
		return HullModel->CalcBounds(HullModel->GetRelativeTransform()).GetBox();
	}
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
	HatchWater->SetVisibility(bHatchOpen);
	// The model's belly is closed, so its square cover never blocks (it is hidden, and nothing hidden is solid).
	if (bHatchOpen || IsModelHull())
	{
		HatchCover->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	else
	{
		Solid(HatchCover);
	}
}

bool ADeepSubmarine::IsModelHull() const
{
	return HullModel && HullModel->GetShown() == 1;
}

void ADeepSubmarine::OnHullModelSwitched()
{
	const bool bModel = IsModelHull();
	// The box hull's colliders: the model replaces them. The window and tower too; the hatch cover is OnRep_Hatch's.
	TArray<UStaticMeshComponent*> Colliders;
	for (UStaticMeshComponent* Shape : Deck) { Colliders.Add(Shape); }
	for (UStaticMeshComponent* Shape : Walls) { Colliders.Add(Shape); }
	Colliders.Add(Ceiling);
	Colliders.Add(Window);
	Colliders.Add(Tower);
	for (UStaticMeshComponent* Shape : Colliders)
	{
		if (bModel)
		{
			Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		else
		{
			Solid(Shape);
		}
	}
	// The square hatch: the model has its own round one at the same place.
	HatchCover->SetHiddenInGame(bModel);
	HatchWater->SetHiddenInGame(bModel);
	OnRep_Hatch();
	for (UStaticMeshComponent* Bulb : Bulbs)
	{
		Bulb->SetHiddenInGame(!bModel);
	}
	// The lamps: on the model's surface, or back where the box hull has them.
	LayOut();
}

void ADeepSubmarine::PlaceLampsOnModel()
{
	if (!HullModel)
	{
		return;
	}
	const FTransform Fit = HullModel->GetRelativeTransform();
	for (uint8 Index = 0; Index < uint8(DeepHull::ELamp::Count); ++Index)
	{
		const DeepHull::ELamp Lamp = DeepHull::ELamp(Index);
		if (ULocalLightComponent* Light = GetLamp(Lamp))
		{
			Light->SetRelativeLocation(DeepHull::ModelLampLocation(Lamp, Fit));
		}
	}
}

ULocalLightComponent* ADeepSubmarine::GetLamp(DeepHull::ELamp Lamp) const
{
	using DeepHull::ELamp;
	switch (Lamp)
	{
	case ELamp::BowPort: return BowLights.IsValidIndex(0) ? BowLights[0].Get() : nullptr;
	case ELamp::BowStbd: return BowLights.IsValidIndex(1) ? BowLights[1].Get() : nullptr;
	case ELamp::HullPort: return HullLights.IsValidIndex(0) ? HullLights[0].Get() : nullptr;
	case ELamp::HullStbd: return HullLights.IsValidIndex(1) ? HullLights[1].Get() : nullptr;
	case ELamp::MarkerPort: return Markers.IsValidIndex(0) ? Markers[0].Get() : nullptr;
	case ELamp::MarkerStbd: return Markers.IsValidIndex(1) ? Markers[1].Get() : nullptr;
	case ELamp::Airlock: return AirlockLight;
	case ELamp::BellyFlood: return BellyFlood;
	default: return nullptr;
	}
}

UStaticMeshComponent* ADeepSubmarine::GetBulb(DeepHull::ELamp Lamp) const
{
	return Bulbs.IsValidIndex(int32(Lamp)) ? Bulbs[int32(Lamp)].Get() : nullptr;
}

FVector ADeepSubmarine::SlideAlongHull(const FVector& WorldCm, const FVector& VelocityMps, float RadiusCm, float HalfHeightCm, float ContactCm) const
{
	const FTransform& Frame = GetActorTransform();
	if (!IsModelHull())
	{
		const FVector Local = UDeepRules::HullSlideVelocity(Frame.InverseTransformPosition(WorldCm), Frame.InverseTransformVectorNoScale(VelocityMps),
			GetSolidBoundsLocal(true), RadiusCm, HalfHeightCm, ContactCm);
		return Frame.TransformVectorNoScale(Local);
	}
	// The model: sweep the capsule ContactCm along the velocity against the model alone; touching its real surface
	// gives the normal there (a tapered nose or stern, the tower's side), not a box face out in the water.
	const FVector Dir = VelocityMps.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		return VelocityMps;
	}
	FHitResult Hit;
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(RadiusCm, HalfHeightCm);
	if (!HullModel->SweepComponent(Hit, WorldCm, WorldCm + Dir * ContactCm, FQuat::Identity, Capsule, false) || !Hit.bBlockingHit)
	{
		return VelocityMps;
	}
	const FVector Normal = Frame.InverseTransformVectorNoScale(Hit.bStartPenetrating ? Hit.Normal : Hit.ImpactNormal);
	return Frame.TransformVectorNoScale(UDeepRules::HullSlideAlongNormal(Frame.InverseTransformVectorNoScale(VelocityMps), Normal));
}

void ADeepSubmarine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADeepSubmarine, bHatchOpen);
	DOREPLIFETIME(ADeepSubmarine, HomeLocation);
	DOREPLIFETIME(ADeepSubmarine, HomeRotation);
	DOREPLIFETIME(ADeepSubmarine, bHasHome);
}
