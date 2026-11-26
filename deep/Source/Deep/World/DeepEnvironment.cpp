#include "World/DeepEnvironment.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "Camera/DeepCameraRules.h"
#include "Camera/DeepCameraSubsystem.h"
#include "Game/DeepGameState.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepArenaRoot.h"
#include "World/DeepSalvage.h"
#include "World/DeepShapes.h"
#include "World/DeepSubmarine.h"
#include "World/DeepSubRoom.h"
#include "World/DeepArena.h"

namespace
{
	UDirectionalLightComponent* MakeDirectional(AActor* Owner, USceneComponent* Root, FName Name, const FRotator& Rotation)
	{
		UDirectionalLightComponent* Light = Owner->CreateDefaultSubobject<UDirectionalLightComponent>(Name);
		Light->SetupAttachment(Root);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetRelativeRotation(Rotation);
		// Channel 1 too: the tether line is on channel 1 only (so the helmet lamp does not make it glow) and must
		// still be lit by the scene.
		Light->SetLightingChannels(true, true, false);
		return Light;
	}

	void SetCVarInt(const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(Value, ECVF_SetByConsole);
		}
	}

	FAutoConsoleCommand RayTracingCommand(
		TEXT("Deep.RayTracing"),
		TEXT("Deep.RayTracing <0|1>: hardware ray tracing for Lumen (reflections with hit lighting) and ray-traced shadows."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			ADeepEnvironment::SetRayTracing(Args.Num() < 1 || FCString::Atoi(*Args[0]) != 0);
		}));

	/** Wraps a coordinate into [-Size/2, Size/2). */
	double Wrap(double Value, double Size)
	{
		return FMath::Fmod(FMath::Fmod(Value + Size * 0.5, Size) + Size, Size) - Size * 0.5;
	}
}

void ADeepEnvironment::SetRayTracing(bool bOn)
{
	// Hardware ray tracing for Lumen GI and reflections; off falls back to software ray tracing plus screen traces.
	SetCVarInt(TEXT("r.Lumen.HardwareRayTracing"), bOn ? 1 : 0);
	// 2 = full material and lighting at the reflection hit: the hatch water reflects the lit room, not the surface cache.
	SetCVarInt(TEXT("r.Lumen.HardwareRayTracing.LightingMode"), bOn ? 2 : 0);
	// Ray-traced shadows (the lamp's hard shadows); off = virtual shadow maps, same light and shape.
	SetCVarInt(TEXT("r.RayTracing.Shadows"), bOn ? 1 : 0);
	UE_LOG(LogTemp, Display, TEXT("DEEPLOOK raytracing=%d"), bOn ? 1 : 0);
}

ADeepEnvironment::ADeepEnvironment()
{
	PrimaryActorTick.bCanEverTick = true;
	// After the camera has moved this frame, so the snow box is centred on this frame's view.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	bReplicates = true;
	bAlwaysRelevant = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	// Nearly straight down, a little slanted so tops and one side are lit: "up" is where the light comes from.
	Downlight = MakeDirectional(this, Root, TEXT("Downlight"), FRotator(-65.f, 30.f, 0.f));
	// The key light is the one forward shading and fog use (the engine warns when two directionals tie).
	Downlight->SetForwardShadingPriority(1);
	// The hull and anything above cut shafts out of the lit fog.
	Downlight->SetCastVolumetricShadow(true);
	// From below and the opposite side, no shadows: undersides are dimmer but never black.
	Fill = MakeDirectional(this, Root, TEXT("Fill"), FRotator(50.f, 210.f, 0.f));
	Fill->SetCastShadows(false);

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);
	// Water is about equally thick at every depth in the arena.
	Fog->SetFogHeightFalloff(0.001f);
	Fog->SetStartDistance(0.f);

	Exposure = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Exposure"));
	Exposure->SetupAttachment(Root);
	Exposure->bUnbound = true;

	Snow = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Snow"));
	Snow->SetupAttachment(Root);
	Snow->SetUsingAbsoluteLocation(true);
	Snow->SetUsingAbsoluteRotation(true);
	Snow->SetUsingAbsoluteScale(true);
	Snow->SetMobility(EComponentMobility::Movable);
	Snow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Snow->SetCastShadow(false);
	// Specks are seen only where a light reaches them: no part in GI, distance fields or ray-traced reflections
	// (also spares the ray tracing scene 9,000 instances).
	Snow->bAffectDynamicIndirectLighting = false;
	Snow->bAffectDistanceFieldLighting = false;
	Snow->bVisibleInRayTracing = false;
	Snow->SetStaticMesh(DeepShapes::LoadMesh(DeepShapes::Cube));
	Snow->bNeverDistanceCull = true;
}

void ADeepEnvironment::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Stop();
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	if (!Tuning.Arena.bGreyBox && GetNetMode() != NM_DedicatedServer)
	{
		BuildSnow(Tuning.Look);
		ApplyFogQuality(Tuning.Look);
	}
}

void ADeepEnvironment::ApplyFogQuality(const FDeepLookTuning& Look)
{
	// Render settings on this machine only (the look is local). Set by code, so a later scalability change keeps them.
	// A zero keeps the engine's value. Epic, "Volumetric Fog": a heavy temporal filter trails a moving flashlight.
	const auto SetIf = [](const TCHAR* Name, bool bSet, const FString& Value)
	{
		if (bSet)
		{
			if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
			{
				Var->Set(*Value, ECVF_SetByCode);
			}
		}
	};
	SetIf(TEXT("r.VolumetricFog.HistoryWeight"), Look.FogHistoryWeight > 0.f, FString::SanitizeFloat(Look.FogHistoryWeight));
	SetIf(TEXT("r.VolumetricFog.GridPixelSize"), Look.FogGridPixelSize > 0, FString::FromInt(Look.FogGridPixelSize));
	SetIf(TEXT("r.VolumetricFog.GridSizeZ"), Look.FogGridSizeZ > 0, FString::FromInt(Look.FogGridSizeZ));
	SetIf(TEXT("r.VolumetricFog.HistoryMissSupersampleCount"), Look.FogHistoryMissSamples > 0, FString::FromInt(Look.FogHistoryMissSamples));
}

void ADeepEnvironment::SetStop(int32 InStop)
{
	Stop = InStop;
	OnRep_Stop();
}

void ADeepEnvironment::OnRep_Stop()
{
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	ApplyStop(Tuning.GetStop(Stop), Tuning.Arena);
}

FLinearColor ADeepEnvironment::GreyBoxTint(const FDeepStopTuning& StopTuning, const FDeepArenaTuning& Arena)
{
	// RGB only: FLinearColor::GetMax counts alpha.
	const FLinearColor& Water = StopTuning.FogColor;
	const float Brightest = FMath::Max3(Water.R, Water.G, Water.B);
	const FLinearColor Hue = Brightest > UE_KINDA_SMALL_NUMBER ? Water / Brightest : FLinearColor::White;
	FLinearColor Tint = FMath::Lerp(FLinearColor::White, Hue, FMath::Clamp(Arena.GreyBoxTintStrength, 0.f, 1.f));
	Tint.A = 1.f;
	return Tint;
}

FDeepLookPreset ADeepEnvironment::BlendLook(const FDeepLookTuning& Look, float DepthM)
{
	const TArray<FDeepLookPreset>& P = Look.Presets;
	if (P.IsEmpty())
	{
		return FDeepLookPreset();
	}
	if (DepthM <= P[0].DepthM)
	{
		return P[0];
	}
	for (int32 i = 1; i < P.Num(); ++i)
	{
		if (DepthM > P[i].DepthM)
		{
			continue;
		}
		const FDeepLookPreset& A = P[i - 1];
		const FDeepLookPreset& B = P[i];
		const float T = FMath::Clamp((DepthM - A.DepthM) / FMath::Max(B.DepthM - A.DepthM, UE_KINDA_SMALL_NUMBER), 0.f, 1.f);
		FDeepLookPreset O;
		O.DepthM = DepthM;
		O.DownlightLux = FMath::Lerp(A.DownlightLux, B.DownlightLux, T);
		O.DownlightVolumetric = FMath::Lerp(A.DownlightVolumetric, B.DownlightVolumetric, T);
		O.FogDensity = FMath::Lerp(A.FogDensity, B.FogDensity, T);
		O.FogColor = FMath::Lerp(A.FogColor, B.FogColor, T);
		O.ExtinctionScale = FMath::Lerp(A.ExtinctionScale, B.ExtinctionScale, T);
		O.VolumetricDistanceM = FMath::Lerp(A.VolumetricDistanceM, B.VolumetricDistanceM, T);
		O.ExposureBias = FMath::Lerp(A.ExposureBias, B.ExposureBias, T);
		O.Bloom = FMath::Lerp(A.Bloom, B.Bloom, T);
		O.Vignette = FMath::Lerp(A.Vignette, B.Vignette, T);
		O.Grain = FMath::Lerp(A.Grain, B.Grain, T);
		O.Saturation = FMath::Lerp(A.Saturation, B.Saturation, (double)T);
		O.Contrast = FMath::Lerp(A.Contrast, B.Contrast, T);
		O.Fringe = FMath::Lerp(A.Fringe, B.Fringe, T);
		O.GlintScale = FMath::Lerp(A.GlintScale, B.GlintScale, T);
		O.SubLightScale = FMath::Lerp(A.SubLightScale, B.SubLightScale, T);
		O.SubLightFog = FMath::Lerp(A.SubLightFog, B.SubLightFog, T);
		return O;
	}
	return P.Last();
}

FDeepLookPreset ADeepEnvironment::MixRoomLook(const FDeepLookPreset& Cave, const FDeepMovingSubTuning& MovingSub, float InRoom01)
{
	const float T = FMath::Clamp(InRoom01, 0.f, 1.f);
	FDeepLookPreset O = Cave;
	// Thin dry haze in the room's own colour; no light in the fog (volumetric fog is off once fully in, ApplyLook).
	O.FogDensity = FMath::Lerp(Cave.FogDensity, FMath::Max(0.f, MovingSub.RoomFogDensity), T);
	O.FogColor = FMath::Lerp(Cave.FogColor, MovingSub.RoomFogColor, T);
	O.DownlightVolumetric = FMath::Lerp(Cave.DownlightVolumetric, 0.f, T);
	O.ExtinctionScale = FMath::Lerp(Cave.ExtinctionScale, 0.f, T);
	return O;
}

float ADeepEnvironment::LookDepthM() const
{
	const ADeepGameState* State = GetWorld() ? GetWorld()->GetGameState<ADeepGameState>() : nullptr;
	return State ? State->SubDepthM : UDeepTuningSettings::Get().GetStop(Stop).SubDepthM;
}

void ADeepEnvironment::StepRoomLook(float DeltaSeconds, const FVector& ViewCm)
{
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const ADeepSubRoom* Room = DeepArena::FindRoom(GetWorld());
	const bool bInRoom = Room && Room->IsInRoomLook(ViewCm, 100.0);
	const float Ease = FMath::Max(0.01f, Tuning.MovingSub.RoomLookEaseSeconds);
	Room01 = FMath::FInterpConstantTo(Room01, bInRoom ? 1.f : 0.f, FMath::Max(0.f, DeltaSeconds), 1.f / Ease);
	const float DepthM = LookDepthM();
	if (!Tuning.Arena.bGreyBox && (!FMath::IsNearlyEqual(Room01, AppliedRoom01, 1.e-4f) || !FMath::IsNearlyEqual(DepthM, AppliedDepthM, 0.01f)))
	{
		AppliedDepthM = DepthM;
		ApplyLook(BlendLook(Tuning.Look, AppliedDepthM), Tuning.Look);
	}
}

bool ADeepEnvironment::IsVolumetricFogOn() const
{
	return Fog && Fog->bEnableVolumetricFog;
}

void ADeepEnvironment::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FollowStillHull();
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	if (Tuning.Arena.bGreyBox)
	{
		return;
	}
	// The Room look follows this machine's own view (a dedicated server has none and keeps the cave look).
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (PC && PC->IsLocalController() && PC->PlayerCameraManager)
		{
			StepRoomLook(DeltaSeconds, PC->PlayerCameraManager->GetCameraLocation());
			break;
		}
	}
	// Every machine blends by the replicated depth, so driving fades continuously and clients match the server.
	const float DepthM = LookDepthM();
	// Toggling the footage look (Deep.Look.Footage) reapplies at once, not only on the next depth change.
	if (!FMath::IsNearlyEqual(DepthM, AppliedDepthM, 0.01f) || UDeepCameraSubsystem::IsFootageOn() != bAppliedFootage)
	{
		AppliedDepthM = DepthM;
		ApplyLook(BlendLook(Tuning.Look, DepthM), Tuning.Look);
	}
	// D2: from BT-01's cabin the water starts at the glass: no height fog or volumetric haze in the dry cabin, the
	// water's look unchanged past the ports (Style Guide 5). Local: only this machine's pilot looks from the cabin.
	{
		bool bCabin = false;
		if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			const ADeepSubmarine* Sub = Cast<ADeepSubmarine>(PC->GetViewTarget());
			bCabin = Sub && Sub->IsHelmCockpit() && Sub->IsHelmManned();
		}
		const float GlassCm = Tuning.HelmView.CabinFogStartM * UDeepRules::CmPerM;
		const float WantStartCm = bCabin ? GlassCm : 0.f;
		const float WantVolumetricCm = FMath::Max(Tuning.Look.VolumetricStartM * UDeepRules::CmPerM, bCabin ? GlassCm : 0.f);
		if (!FMath::IsNearlyEqual(Fog->StartDistance, WantStartCm, 0.5f))
		{
			Fog->SetStartDistance(WantStartCm);
		}
		if (!FMath::IsNearlyEqual(Fog->VolumetricFogStartDistance, WantVolumetricCm, 0.5f))
		{
			Fog->SetVolumetricFogStartDistance(WantVolumetricCm);
		}
	}
	// Salvage respawns each dive: keep every glint at this depth's scale (a no-op when unchanged).
	for (TActorIterator<ADeepSalvage> It(GetWorld()); It; ++It)
	{
		It->SetGlintScale(Applied.GlintScale);
	}
	UpdateSnow(DeltaSeconds, Tuning.Look);
}

void ADeepEnvironment::ApplyStop(const FDeepStopTuning& StopTuning, const FDeepArenaTuning& Arena)
{
	FPostProcessSettings& Settings = Exposure->Settings;

	if (Arena.bGreyBox)
	{
		const FLinearColor Tint = GreyBoxTint(StopTuning, Arena);
		Downlight->SetIntensity(Arena.GreyBoxKeyLux);
		Downlight->SetLightColor(Tint);
		Downlight->SetVolumetricScatteringIntensity(0.f);
		Fill->SetVisibility(true);
		Fill->SetIntensity(Arena.GreyBoxFillLux);
		Fill->SetLightColor(Tint);
		Fill->SetVolumetricScatteringIntensity(0.f);

		// Only the empty background takes the colour; nothing in the arena is hazed.
		Fog->SetVolumetricFog(false);
		Fog->SetFogDensity(FMath::Max(0.f, Arena.GreyBoxBackdropFogDensity));
		Fog->SetFogInscatteringColor(Arena.GreyBoxBackdropColor);

		// Fixed exposure: the image never pumps when looking at the lamp or into the dark.
		Settings.bOverride_AutoExposureMethod = true;
		Settings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		Settings.AutoExposureApplyPhysicalCameraExposure = false;
		Settings.bOverride_AutoExposureBias = true;
		Settings.AutoExposureBias = Arena.GreyBoxExposureEV;
		Settings.bOverride_AutoExposureMinBrightness = false;
		Settings.bOverride_AutoExposureMaxBrightness = false;
		// Sharp frames: small salvage smeared into streaks while turning.
		Settings.bOverride_MotionBlurAmount = true;
		Settings.MotionBlurAmount = 0.f;
		return;
	}

	// The water look is blended by depth every tick (Tick -> ApplyLook). Apply it now too, so the first frame is right.
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const ADeepGameState* State = GetWorld() ? GetWorld()->GetGameState<ADeepGameState>() : nullptr;
	AppliedDepthM = State ? State->SubDepthM : StopTuning.SubDepthM;
	ApplyLook(BlendLook(Tuning.Look, AppliedDepthM), Tuning.Look);
}

void ADeepEnvironment::ApplyLook(const FDeepLookPreset& CaveLook, const FDeepLookTuning& Look)
{
	// The local view in the room set: the cave look eased toward the Room look (StepRoomLook).
	const FDeepLookPreset L = MixRoomLook(CaveLook, UDeepTuningSettings::Get().MovingSub, Room01);
	AppliedRoom01 = Room01;
	Applied = L;
	Downlight->SetVisibility(L.DownlightLux > 0.0005f);
	Downlight->SetIntensity(L.DownlightLux);
	Downlight->SetLightColor(Look.DownlightColor);
	Downlight->SetVolumetricScatteringIntensity(L.DownlightVolumetric);
	Downlight->SetRelativeRotation(Look.DownlightRotation);
	// Shafts: the surface pattern far above, as a light function. It cuts the downlight in the fog too (the light
	// function atlas feeds volumetric fog: Epic, "Volumetric Fog"). Set once; light-space UVs in metres.
	if (!Downlight->LightFunctionMaterial && !Look.ShaftLightFunction.IsNull())
	{
		if (UMaterialInterface* Shafts = Cast<UMaterialInterface>(Look.ShaftLightFunction.TryLoad()))
		{
			Downlight->SetLightFunctionMaterial(Shafts);
			Downlight->SetLightFunctionScale(FVector(Look.ShaftScaleCm));
		}
	}
	Fill->SetVisibility(false);

	// The sub's outside lights fade with depth, and stop lighting the water before the water turns black.
	for (TActorIterator<ADeepSubmarine> It(GetWorld()); It; ++It)
	{
		It->SetOutsideLights(L.SubLightScale, L.SubLightFog);
	}

	Fog->SetFogDensity(L.FogDensity);
	Fog->SetFogHeightFalloff(Look.FogHeightFalloff);
	Fog->SetFogMaxOpacity(1.f);
	Fog->SetFogInscatteringColor(L.FogColor);
	// The far fog's directional inscattering lifted the view to milk (look/pass1): keep it black.
	Fog->SetDirectionalInscatteringColor(FLinearColor::Black);
	// Fully in the room: no volumetric fog (dry air). Easing in, its extinction has already gone toward zero.
	Fog->SetVolumetricFog(Room01 < 1.f);
	Fog->SetVolumetricFogAlbedo(Look.FogAlbedo);
	Fog->SetVolumetricFogScatteringDistribution(Look.FogScatteringDistribution);
	Fog->SetVolumetricFogExtinctionScale(L.ExtinctionScale);
	Fog->SetVolumetricFogDistance(L.VolumetricDistanceM * UDeepRules::CmPerM);
	Fog->SetVolumetricFogStartDistance(Look.VolumetricStartM * UDeepRules::CmPerM);

	FPostProcessSettings& S = Exposure->Settings;
	// Locked exposure: auto exposure undoes darkness.
	S.bOverride_AutoExposureMethod = true;
	S.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	S.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	S.AutoExposureApplyPhysicalCameraExposure = false;
	S.bOverride_AutoExposureBias = true;
	S.AutoExposureBias = L.ExposureBias;
	S.bOverride_AutoExposureMinBrightness = false;
	S.bOverride_AutoExposureMaxBrightness = false;
	// Local exposure lifts shadows: both scales 1.0.
	S.bOverride_LocalExposureHighlightContrastScale = true;
	S.LocalExposureHighlightContrastScale = Look.LocalExposureContrast;
	S.bOverride_LocalExposureShadowContrastScale = true;
	S.LocalExposureShadowContrastScale = Look.LocalExposureContrast;
	S.bOverride_BloomIntensity = true;
	S.BloomIntensity = L.Bloom;
	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = L.Vignette;
	S.bOverride_FilmGrainIntensity = true;
	S.FilmGrainIntensity = L.Grain;
	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = L.Saturation;
	S.bOverride_ColorContrast = true;
	S.ColorContrast = FVector4(1.0, 1.0, 1.0, L.Contrast);
	S.bOverride_SceneFringeIntensity = true;
	S.SceneFringeIntensity = L.Fringe;
	S.bOverride_MotionBlurAmount = true;
	S.MotionBlurAmount = Look.MotionBlur;

	// The footage look (Camera lane, Deep.Look.Footage): a helmet camera's picture on top of the depth look. Grain that
	// rises with depth (heavier in the shadows), a darker vignette, and colour fringe only toward the edges. No
	// exposure or fog change: darkness stays the design. The lens bend and the sway are the camera subsystem's.
	const FDeepCameraTuning& Cam = UDeepTuningSettings::Get().Camera;
	bAppliedFootage = UDeepCameraSubsystem::IsFootageOn();
	const FVector2D Grain = bAppliedFootage ? DeepCameraRules::FootageGrain(AppliedDepthM, Cam) : FVector2D(0.f, 1.f);
	S.FilmGrainIntensity = L.Grain + Grain.X;
	S.bOverride_FilmGrainIntensityShadows = bAppliedFootage;
	S.FilmGrainIntensityShadows = Grain.Y;
	S.VignetteIntensity = L.Vignette + (bAppliedFootage ? Cam.FootageVignetteAdd : 0.f);
	S.SceneFringeIntensity = L.Fringe + (bAppliedFootage ? Cam.FootageFringeAdd : 0.f);
	S.bOverride_ChromaticAberrationStartOffset = bAppliedFootage;
	S.ChromaticAberrationStartOffset = Cam.FootageFringeStart;
	if (bAppliedFootage)
	{
		S.MotionBlurAmount = Cam.FootageMotionBlur;
	}
}

void ADeepEnvironment::SetCameraBlendable(UMaterialInterface* Material, float Weight)
{
	if (!Material || !Exposure)
	{
		return;
	}
	TArray<FWeightedBlendable>& Blendables = Exposure->Settings.WeightedBlendables.Array;
	for (FWeightedBlendable& B : Blendables)
	{
		if (B.Object == Material)
		{
			B.Weight = Weight;
			return;
		}
	}
	if (Weight > 0.f)
	{
		Blendables.Add(FWeightedBlendable(Weight, Material));
	}
}

void ADeepEnvironment::BuildSnow(const FDeepLookTuning& Look)
{
	Snow->ClearInstances();
	SnowHome.Reset();
	SnowDrift.Reset();
	SnowScale.Reset();
	SnowSpin.Reset();
	SnowXforms.Reset();
	const int32 Count = FMath::Max(0, Look.SnowCount);
	if (Count == 0)
	{
		return;
	}
	DeepShapes::Tint(Snow, Look.SnowColor);
	const double Box = Look.SnowBoxM * UDeepRules::CmPerM;
	FRandomStream Rand(0xDEE9);
	for (int32 i = 0; i < Count; ++i)
	{
		SnowHome.Add(FVector(Rand.FRandRange(-0.5f, 0.5f), Rand.FRandRange(-0.5f, 0.5f), Rand.FRandRange(-0.5f, 0.5f)) * Box);
		SnowDrift.Add(FVector(Rand.VRand()) * (Look.SnowWanderMps * Rand.FRandRange(0.3f, 1.f) * UDeepRules::CmPerM)
			- FVector(0.0, 0.0, Look.SnowSinkMps * UDeepRules::CmPerM));
		// Basic shapes are 100 cm across: scale to the speck size.
		SnowScale.Add(Rand.FRandRange(Look.SnowMinSizeM, Look.SnowMaxSizeM) * UDeepRules::CmPerM / DeepShapes::ShapeSizeCm);
		const FRotator Spin(Rand.FRandRange(0.f, 360.f), Rand.FRandRange(0.f, 360.f), 0.f);
		SnowSpin.Add(Spin.Quaternion());
		SnowXforms.Add(FTransform(Spin.Quaternion(), GetActorLocation(), FVector(SnowScale.Last())));
	}
	Snow->AddInstances(SnowXforms, false, true, false);
}

void ADeepEnvironment::UpdateSnow(float DeltaSeconds, const FDeepLookTuning& Look)
{
	if (SnowHome.IsEmpty() || !GetWorld())
	{
		return;
	}
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}
	SnowTime += DeltaSeconds;
	const FVector View = PC->PlayerCameraManager->GetCameraLocation();
	const double ClearSq = FMath::Square(Look.SnowClearM * UDeepRules::CmPerM);

	// The specks hang in the arena: when the sub drives, the root moves and the snow streams past the still sub.
	FVector Origin = GetActorLocation();
	for (TActorIterator<ADeepArenaRoot> It(GetWorld()); It; ++It)
	{
		Origin = It->GetActorLocation();
		break;
	}
	// No specks inside the hull: the room is dry.
	FBox Hull(ForceInit);
	FTransform HullXf = FTransform::Identity;
	// Seen through the helm camera (D1c, Sim/DeepHelmLightRules): the specks fill a bigger box round a point ahead of the
	// camera, where the floods' beams are; only specks a sub light reaches show (an unlit speck in front of lit rock read as a
	// dark dash); far ones keep about a pixel; the cockpit streaks them along the hull's real travel, the chase view never.
	const ADeepSubmarine* HelmSub = nullptr;
	for (TActorIterator<ADeepSubmarine> It(GetWorld()); It; ++It)
	{
		Hull = It->GetSolidBoundsLocal(false);
		HullXf = It->GetActorTransform();
		HelmSub = PC->GetViewTarget() == *It ? *It : nullptr;
		break;
	}
	const bool bHelmView = HelmSub != nullptr;
	const bool bCockpit = bHelmView && HelmSub->IsHelmCockpit();
	const double Box = (bHelmView ? FMath::Max(Look.SnowBoxM, Look.HelmSnowBoxM) : Look.SnowBoxM) * UDeepRules::CmPerM;
	// The homes were laid out in SnowBoxM: spread them over the helm's bigger box.
	const double HomeScale = Box / FMath::Max(1.0, double(Look.SnowBoxM) * UDeepRules::CmPerM);
	const FVector Centre = bHelmView
		? View + PC->PlayerCameraManager->GetCameraRotation().Vector() * ((bCockpit ? Look.HelmSnowCockpitAheadM : Look.HelmSnowChaseAheadM) * UDeepRules::CmPerM)
		: View;
	const FVector TravelMps = bHelmView ? HelmSub->GetHelmTravelWorldMps() : FVector::ZeroVector;
	const double StreakCm = bHelmView ? DeepHelmLight::SnowStreakCm(float(TravelMps.Size()), bCockpit, Look) : 0.0;
	// Below a few cm of streak a speck keeps its tumble; above, it is a line along the hull's travel.
	const bool bStreak = StreakCm > 1.0;
	const FQuat Along = bStreak ? FRotationMatrix::MakeFromZ(TravelMps.GetSafeNormal()).ToQuat() : FQuat::Identity;
	if (bHelmView)
	{
		HelmSub->GatherSnowLights(SnowLights, Look.HelmSnowLitMin);
	}
	// In the room set (far above the dive, World/DeepSubRoom) there is no water: no specks at all.
	const ADeepSubRoom* Room = DeepArena::FindRoom(GetWorld());
	const bool bInRoom = Room01 > 0.f || (Room && Room->IsInRoomLook(View, 100.0));

	for (int32 i = 0; i < SnowHome.Num(); ++i)
	{
		const FVector P = Origin + SnowHome[i] * HomeScale + SnowDrift[i] * SnowTime;
		const FVector At = Centre + FVector(Wrap(P.X - Centre.X, Box), Wrap(P.Y - Centre.Y, Box), Wrap(P.Z - Centre.Z, Box));
		const double DistSq = FVector::DistSquared(At, View);
		const bool bSpeckHidden = bInRoom || (Hull.IsValid && Hull.IsInsideOrOn(HullXf.InverseTransformPosition(At))) || DistSq < ClearSq
			|| (bHelmView && !DeepHelmLight::SnowLit(At, SnowLights));
		SnowXforms[i].SetLocation(At);
		if (bSpeckHidden)
		{
			SnowXforms[i].SetScale3D(FVector::ZeroVector);
			continue;
		}
		const double S = bHelmView ? DeepHelmLight::SnowSizeCm(SnowScale[i] * DeepShapes::ShapeSizeCm, float(FMath::Sqrt(DistSq)), Look) / DeepShapes::ShapeSizeCm
			: SnowScale[i];
		if (bStreak)
		{
			// Thin the streak as it lengthens so a fast speck does not read as a rod.
			SnowXforms[i].SetRotation(Along);
			SnowXforms[i].SetScale3D(FVector(S * 0.7, S * 0.7, S + StreakCm / DeepShapes::ShapeSizeCm));
		}
		else
		{
			SnowXforms[i].SetRotation(SnowSpin[i]);
			SnowXforms[i].SetScale3D(FVector(S));
		}
	}
	Snow->BatchUpdateInstancesTransforms(0, SnowXforms, true, true, true);
}

void ADeepEnvironment::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADeepEnvironment, Stop);
}

void ADeepEnvironment::FollowStillHull()
{
	UWorld* World = GetWorld();
	const ADeepSubmarine* Sub = DeepArena::IsWorldStill(World) ? DeepArena::FindSubmarine(World) : nullptr;
	if (Sub && !GetActorLocation().Equals(Sub->GetActorLocation(), 0.01))
	{
		SetActorLocation(Sub->GetActorLocation());
	}
}
