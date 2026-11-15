#include "World/DeepEnvironment.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Net/UnrealNetwork.h"
#include "Sim/DeepTuningSettings.h"

ADeepEnvironment::ADeepEnvironment()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	Downlight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Downlight"));
	Downlight->SetupAttachment(Root);
	Downlight->SetMobility(EComponentMobility::Movable);
	Downlight->SetCastShadows(true);
	// The sub's shadow in the lit water is the light shaft.
	Downlight->bCastVolumetricShadow = true;

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);
	// Water is about equally thick at every depth in the arena.
	Fog->SetFogHeightFalloff(0.001f);
	Fog->SetStartDistance(0.f);
	Fog->SetVolumetricFog(true);
	// No artistic sun glow in the far fog: it lifted the whole view to milk.
	Fog->SetDirectionalInscatteringColor(FLinearColor::Black);

	Exposure = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Exposure"));
	Exposure->SetupAttachment(Root);
	Exposure->bUnbound = true;
}

void ADeepEnvironment::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Stop();
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

void ADeepEnvironment::ApplyStop(const FDeepStopTuning& StopTuning, const FDeepArenaTuning& Arena)
{
	Downlight->SetRelativeRotation(FRotator(SunPitchDeg, SunYawDeg, 0.f));
	Downlight->SetLightColor(SunColor);
	Downlight->SetIntensity(StopTuning.SunIntensity);
	Downlight->SetVolumetricScatteringIntensity(SunScattering);

	Fog->SetFogDensity(StopTuning.FogDensity);
	// Luminance of the far fog past the volumetric range; keep it near what the volume shows there.
	Fog->SetFogInscatteringColor(StopTuning.FogColor);
	Fog->SetFogMaxOpacity(FogMaxOpacity);
	Fog->SetVolumetricFogAlbedo(WaterAlbedo.ToFColor(true));
	Fog->SetVolumetricFogScatteringDistribution(ScatteringDistribution);
	Fog->SetVolumetricFogExtinctionScale(ExtinctionScale);
	Fog->SetVolumetricFogEmissive(FLinearColor::Black);
	Fog->SetVolumetricFogDistance(VolumetricDistanceM * 100.f);
	Fog->SetVolumetricFogStartDistance(VolumetricStartM * 100.f);

	// Exposure held in a narrow EV100 band (min = max locks it), so the deep stops stay dark instead of brightened.
	FPostProcessSettings& Settings = Exposure->Settings;
	Settings.bOverride_AutoExposureMinBrightness = true;
	Settings.AutoExposureMinBrightness = Arena.ExposureMinEV;
	Settings.bOverride_AutoExposureMaxBrightness = true;
	Settings.AutoExposureMaxBrightness = FMath::Max(Arena.ExposureMinEV, Arena.ExposureMaxEV);
	Settings.bOverride_AutoExposureBias = true;
	Settings.AutoExposureBias = 0.f;
	// Local exposure lifts shadows; in the deep that undoes the darkness.
	Settings.bOverride_LocalExposureShadowContrastScale = true;
	Settings.LocalExposureShadowContrastScale = 1.f;
	Settings.bOverride_LocalExposureHighlightContrastScale = true;
	Settings.LocalExposureHighlightContrastScale = 1.f;
	Settings.bOverride_BloomIntensity = true;
	Settings.BloomIntensity = BloomIntensity;
	Settings.bOverride_VignetteIntensity = true;
	Settings.VignetteIntensity = VignetteIntensity;
}

void ADeepEnvironment::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADeepEnvironment, Stop);
}
