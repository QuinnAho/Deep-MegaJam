#include "Camera/DeepCameraSubsystem.h"

#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/DeepCameraRules.h"
#include "Camera/DeepCameraShake.h"
#include "Camera/PlayerCameraManager.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "World/DeepEnvironment.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Components/PrimitiveComponent.h"
#include "Monster/DeepSerpent.h"
#include "Sim/DeepTuningSettings.h"
#include "Sonar/DeepSonarRules.h"
#include "Story/DeepEventBus.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepCamera, Log, All);

namespace
{
	TAutoConsoleVariable<int32> CVarShake(TEXT("Deep.Camera.Shake"), -1,
		TEXT("Deep.Camera.Shake 0|1: camera shakes on events. -1 = DEEP Tuning > Camera > bShakeOn."));
	TAutoConsoleVariable<int32> CVarFootage(TEXT("Deep.Look.Footage"), -1,
		TEXT("Deep.Look.Footage 0|1: the helmet-camera footage look (grain, edges, lens, handheld sway). -1 = DEEP Tuning > Camera > bFootageOn."));
	TAutoConsoleVariable<int32> CVarPulse(TEXT("Deep.Sonar.Pulse"), -1,
		TEXT("Deep.Sonar.Pulse 0|1: each sonar ping draws an amber shell over the surfaces and leaves lidar dots. -1 = DEEP Tuning > Camera > bPulseOn."));

	bool Switch(const TAutoConsoleVariable<int32>& Var, bool bDefault)
	{
		const int32 Value = Var.GetValueOnGameThread();
		return Value < 0 ? bDefault : Value != 0;
	}
}

UDeepCameraSubsystem* UDeepCameraSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDeepCameraSubsystem>() : nullptr;
}

bool UDeepCameraSubsystem::IsShakeOn()
{
	return Switch(CVarShake, UDeepTuningSettings::Get().Camera.bShakeOn);
}

bool UDeepCameraSubsystem::IsFootageOn()
{
	return Switch(CVarFootage, UDeepTuningSettings::Get().Camera.bFootageOn);
}

bool UDeepCameraSubsystem::IsPulseOn()
{
	return Switch(CVarPulse, UDeepTuningSettings::Get().Camera.bPulseOn);
}

bool UDeepCameraSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UDeepCameraSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UDeepEventBus>();
	if (UDeepEventBus* Bus = UDeepEventBus::Get(this))
	{
		EventHandle = Bus->OnEvent.AddUObject(this, &UDeepCameraSubsystem::HandleEvent);
	}
}

void UDeepCameraSubsystem::Deinitialize()
{
	if (UDeepEventBus* Bus = UDeepEventBus::Get(this))
	{
		Bus->OnEvent.Remove(EventHandle);
	}
	Super::Deinitialize();
}

TStatId UDeepCameraSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDeepCameraSubsystem, STATGROUP_Tickables);
}

APlayerCameraManager* UDeepCameraSubsystem::LocalCamera() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (PC && PC->IsLocalController() && PC->PlayerCameraManager)
		{
			return PC->PlayerCameraManager;
		}
	}
	return nullptr;
}

void UDeepCameraSubsystem::HandleEvent(FName Event, const FDeepEventPayload& Payload)
{
	// The sonar pings on the local HUD; Source is the sub, DepthM the sonar's range.
	if (Event == FName(DeepSonarEvent::Ping) && IsPulseOn())
	{
		const AActor* Sub = Payload.Source.Get();
		const APlayerCameraManager* Camera = LocalCamera();
		const FVector Origin = Sub ? Sub->GetActorLocation() : (Camera ? Camera->GetCameraLocation() : FVector::ZeroVector);
		StartPulse(Origin, Payload.DepthM > 0.f ? Payload.DepthM : UDeepTuningSettings::Get().Camera.PulseRangeM);
	}
	// The host's bus also carries events about other players' divers; their own machines shake for those.
	if (const APawn* Pawn = Cast<APawn>(Payload.Source.Get()))
	{
		if (Cast<APlayerController>(Pawn->GetController()) && !Pawn->IsLocallyControlled())
		{
			return;
		}
	}
	PlayEventShake(Event);
}

bool UDeepCameraSubsystem::PlayEventShake(FName Event)
{
	if (!IsShakeOn())
	{
		return false;
	}
	const FDeepShakeSpec Spec = DeepCameraRules::ShakeForEvent(Event, UDeepTuningSettings::Get().Camera);
	return !Spec.IsNone() && PlayShake(Spec, Event);
}

bool UDeepCameraSubsystem::PlayShake(const FDeepShakeSpec& Spec, FName Label)
{
	APlayerCameraManager* Camera = LocalCamera();
	if (!Camera || Spec.IsNone())
	{
		return false;
	}
	FAddCameraShakeParams Params(1.f);
	Params.Initializer.BindLambda([Spec](UCameraShakeBase* Shake)
	{
		if (UDeepShakePattern* Pattern = Cast<UDeepShakePattern>(Shake ? Shake->GetRootShakePattern() : nullptr))
		{
			Pattern->Spec = Spec;
		}
	});
	UCameraShakeBase* Shake = Camera->StartCameraShake(UDeepCameraShake::StaticClass(), Params);
	if (!Shake)
	{
		return false;
	}
	++ShakesPlayed;
	LastShake = Label;
	UE_LOG(LogDeepCamera, Display, TEXT("DEEPCAMERA shake=%s rot=%.2f loc=%.1f dur=%.2f"), *Label.ToString(), Spec.RotationDeg, Spec.LocationCm, Spec.DurationS);
	return true;
}

void UDeepCameraSubsystem::UpdateSway()
{
	APlayerCameraManager* Camera = LocalCamera();
	const bool bWant = Camera && IsFootageOn();
	const bool bRunning = Sway.IsValid() && SwayCamera.Get() == Camera && Sway->IsActive();
	if (bWant && !bRunning)
	{
		const FDeepShakeSpec Spec = UDeepTuningSettings::Get().Camera.Sway;
		FAddCameraShakeParams Params(1.f);
		Params.Initializer.BindLambda([Spec](UCameraShakeBase* Shake)
		{
			if (UDeepShakePattern* Pattern = Cast<UDeepShakePattern>(Shake ? Shake->GetRootShakePattern() : nullptr))
			{
				Pattern->Spec = Spec;
			}
		});
		Sway = Camera->StartCameraShake(UDeepSwayShake::StaticClass(), Params);
		SwayCamera = Camera;
	}
	else if (!bWant && Sway.IsValid())
	{
		if (APlayerCameraManager* Old = SwayCamera.Get())
		{
			// Eases out over the sway's blend-out.
			Old->StopAllInstancesOfCameraShake(UDeepSwayShake::StaticClass(), false);
		}
		Sway.Reset();
		SwayCamera.Reset();
	}
}

void UDeepCameraSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateSway();
	UpdateLens();
	UpdatePulse(DeltaTime);
}

TSharedRef<FJsonObject> UDeepCameraSubsystem::BuildStateJson() const
{
	const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("shake"), IsShakeOn());
	Json->SetBoolField(TEXT("footage"), IsFootageOn());
	Json->SetNumberField(TEXT("shakes_played"), ShakesPlayed);
	Json->SetStringField(TEXT("last_shake"), LastShake.IsNone() ? FString() : LastShake.ToString());
	Json->SetBoolField(TEXT("sway"), IsSwaying());
	Json->SetBoolField(TEXT("lens"), bLensOn);
	Json->SetBoolField(TEXT("pulse"), IsPulseOn());
	Json->SetNumberField(TEXT("pulses"), Pulses);
	Json->SetBoolField(TEXT("pulse_live"), IsPulseLive());
	return Json;
}

ADeepEnvironment* UDeepCameraSubsystem::FindEnvironment()
{
	if (!Environment.IsValid() && GetWorld())
	{
		TActorIterator<ADeepEnvironment> It(GetWorld());
		Environment = It ? *It : nullptr;
	}
	return Environment.Get();
}

UMaterialInstanceDynamic* UDeepCameraSubsystem::MakeMaterial(const FSoftObjectPath& Path, const TCHAR* What)
{
	UMaterialInterface* Base = Cast<UMaterialInterface>(Path.TryLoad());
	if (!Base)
	{
		UE_LOG(LogDeepCamera, Warning, TEXT("DEEPCAMERA %s material missing: %s (run Content/Deep/Camera/build_camera_materials.py)"), What, *Path.ToString());
		return nullptr;
	}
	return UMaterialInstanceDynamic::Create(Base, this);
}

void UDeepCameraSubsystem::UpdateLens()
{
	// The footage look's lens: a slight barrel bend after the tonemapper (M_FootageLens), on while the look is on.
	const bool bWant = IsFootageOn();
	if (!bWant && !LensMID)
	{
		bLensOn = false;
		return;
	}
	const FDeepCameraTuning& T = UDeepTuningSettings::Get().Camera;
	if (!LensMID && !bLensTried)
	{
		bLensTried = true;
		LensMID = MakeMaterial(T.LensMaterial, TEXT("lens"));
	}
	ADeepEnvironment* Env = FindEnvironment();
	if (!LensMID || !Env)
	{
		bLensOn = false;
		return;
	}
	LensMID->SetScalarParameterValue(TEXT("K"), T.FootageBarrel);
	const bool bOn = bWant && T.FootageBarrel > 0.f;
	// Set every tick (a short list): survives the environment being respawned.
	Env->SetCameraBlendable(LensMID, bOn ? 1.f : 0.f);
	bLensOn = bOn;
}

bool UDeepCameraSubsystem::StartPulse(const FVector& Origin, float RangeM)
{
	const FDeepCameraTuning& T = UDeepTuningSettings::Get().Camera;
	if (!PulseMID && !bPulseTried)
	{
		bPulseTried = true;
		PulseMID = MakeMaterial(T.PulseMaterial, TEXT("pulse"));
	}
	if (!PulseMID || RangeM <= 0.f)
	{
		return false;
	}
	MarkSerpents();
	PulseOrigin = Origin;
	PulseRangeM = RangeM;
	PulseAgeS = 0.f;
	++Pulses;
	UE_LOG(LogDeepCamera, Display, TEXT("DEEPCAMERA pulse=%d origin=%s range=%.0fm"), Pulses, *Origin.ToCompactString(), RangeM);
	return true;
}

void UDeepCameraSubsystem::MarkSerpents()
{
	if (!GetWorld())
	{
		return;
	}
	for (TActorIterator<ADeepSerpent> It(GetWorld()); It; ++It)
	{
		TArray<UPrimitiveComponent*> Parts;
		It->GetComponents<UPrimitiveComponent>(Parts);
		for (UPrimitiveComponent* Part : Parts)
		{
			if (Part && !Part->bRenderCustomDepth)
			{
				Part->SetRenderCustomDepth(true);
			}
		}
	}
}

void UDeepCameraSubsystem::UpdatePulse(float DeltaTime)
{
	const FDeepCameraTuning& T = UDeepTuningSettings::Get().Camera;
	if (PulseAgeS >= 0.f)
	{
		PulseAgeS += DeltaTime;
		if (!DeepCameraRules::IsPulseLive(PulseAgeS, T))
		{
			PulseAgeS = -1.f;
		}
	}
	ADeepEnvironment* Env = FindEnvironment();
	if (!PulseMID || !Env)
	{
		return;
	}
	const bool bShow = PulseAgeS >= 0.f;
	if (bShow)
	{
		// After the tonemapper the buffer is display-encoded: the amber goes in as its sRGB value, not linear.
		const FLinearColor Amber = T.PulseColor.ToFColor(true).ReinterpretAsLinear();
		PulseMID->SetVectorParameterValue(TEXT("Origin"), FLinearColor(PulseOrigin.X, PulseOrigin.Y, PulseOrigin.Z, 0.f));
		PulseMID->SetVectorParameterValue(TEXT("Color"), Amber);
		PulseMID->SetScalarParameterValue(TEXT("RadiusCm"), DeepCameraRules::PulseRadiusCm(PulseAgeS, PulseRangeM, T));
		PulseMID->SetScalarParameterValue(TEXT("MaxRadiusCm"), PulseRangeM * 100.f);
		PulseMID->SetScalarParameterValue(TEXT("StartCm"), T.PulseStartM * 100.f);
		PulseMID->SetScalarParameterValue(TEXT("BandCm"), T.PulseBandM * 100.f);
		PulseMID->SetScalarParameterValue(TEXT("AgeS"), PulseAgeS);
		PulseMID->SetScalarParameterValue(TEXT("ShellS"), T.PulseSeconds);
		PulseMID->SetScalarParameterValue(TEXT("SpeedCmPerS"), PulseRangeM * 100.f / FMath::Max(T.PulseSeconds, 0.01f));
		PulseMID->SetScalarParameterValue(TEXT("DotFadeS"), T.PulseDotFadeS);
		PulseMID->SetScalarParameterValue(TEXT("SpacingCm"), T.PulseDotSpacingM * 100.f);
		PulseMID->SetScalarParameterValue(TEXT("DotFarCm"), T.PulseDotFarM * 100.f);
		PulseMID->SetScalarParameterValue(TEXT("BandIntensity"), T.PulseBandIntensity);
		PulseMID->SetScalarParameterValue(TEXT("DotIntensity"), T.PulseDotIntensity);
		PulseMID->SetScalarParameterValue(TEXT("Intensity"), 1.f);
	}
	// Weight 0 between pings: the material costs nothing when it is not drawn.
	if (bShow || bPulseShown)
	{
		Env->SetCameraBlendable(PulseMID, bShow ? 1.f : 0.f);
		bPulseShown = bShow;
	}
}
