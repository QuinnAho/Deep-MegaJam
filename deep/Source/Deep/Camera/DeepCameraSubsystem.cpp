#include "Camera/DeepCameraSubsystem.h"

#include "Camera/CameraModifier_CameraShake.h"
#include "Camera/DeepCameraRules.h"
#include "Camera/DeepCameraShake.h"
#include "Camera/PlayerCameraManager.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepEventBus.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepCamera, Log, All);

namespace
{
	TAutoConsoleVariable<int32> CVarShake(TEXT("Deep.Camera.Shake"), -1,
		TEXT("Deep.Camera.Shake 0|1: camera shakes on events. -1 = DEEP Tuning > Camera > bShakeOn."));
	TAutoConsoleVariable<int32> CVarFootage(TEXT("Deep.Look.Footage"), -1,
		TEXT("Deep.Look.Footage 0|1: the helmet-camera footage look (grain, edges, lens, handheld sway). -1 = DEEP Tuning > Camera > bFootageOn."));

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
}

TSharedRef<FJsonObject> UDeepCameraSubsystem::BuildStateJson() const
{
	const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("shake"), IsShakeOn());
	Json->SetBoolField(TEXT("footage"), IsFootageOn());
	Json->SetNumberField(TEXT("shakes_played"), ShakesPlayed);
	Json->SetStringField(TEXT("last_shake"), LastShake.IsNone() ? FString() : LastShake.ToString());
	Json->SetBoolField(TEXT("sway"), IsSwaying());
	return Json;
}
