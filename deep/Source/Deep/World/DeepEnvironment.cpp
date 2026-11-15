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
	// Nearly straight down, a little slanted so shafts read in the fog.
	Downlight->SetRelativeRotation(FRotator(-75.f, 30.f, 0.f));
	Downlight->SetLightColor(FLinearColor(0.45f, 0.75f, 0.9f));

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);
	// Water is about equally thick at every depth in the arena.
	Fog->SetFogHeightFalloff(0.001f);
	Fog->SetStartDistance(0.f);
	Fog->SetVolumetricFog(true);

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
	Downlight->SetIntensity(StopTuning.SunIntensity);
	Fog->SetFogDensity(StopTuning.FogDensity);
	Fog->SetFogInscatteringColor(StopTuning.FogColor);

	FPostProcessSettings& Settings = Exposure->Settings;
	Settings.bOverride_AutoExposureMinBrightness = true;
	Settings.AutoExposureMinBrightness = Arena.ExposureMinEV;
	Settings.bOverride_AutoExposureMaxBrightness = true;
	Settings.AutoExposureMaxBrightness = Arena.ExposureMaxEV;
}

void ADeepEnvironment::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADeepEnvironment, Stop);
}
