#include "World/DeepEnvironment.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Net/UnrealNetwork.h"
#include "Sim/DeepTuningSettings.h"

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
}

ADeepEnvironment::ADeepEnvironment()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	// Nearly straight down, a little slanted so tops and one side are lit: "up" is where the light comes from.
	Downlight = MakeDirectional(this, Root, TEXT("Downlight"), FRotator(-65.f, 30.f, 0.f));
	// The key light is the one forward shading and fog use (the engine warns when two directionals tie).
	Downlight->SetForwardShadingPriority(1);
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

	Downlight->SetIntensity(StopTuning.SunIntensity);
	Downlight->SetLightColor(FLinearColor(0.45f, 0.75f, 0.9f));
	Downlight->SetVolumetricScatteringIntensity(1.f);
	Fill->SetVisibility(false);
	Fog->SetVolumetricFog(true);
	Fog->SetFogDensity(StopTuning.FogDensity);
	Fog->SetFogInscatteringColor(StopTuning.FogColor);

	Settings.bOverride_MotionBlurAmount = false;
	Settings.bOverride_AutoExposureMethod = false;
	Settings.bOverride_AutoExposureApplyPhysicalCameraExposure = false;
	Settings.bOverride_AutoExposureBias = false;
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
