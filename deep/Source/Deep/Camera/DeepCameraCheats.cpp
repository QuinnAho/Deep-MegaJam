// Camera cheats: Deep.Camera.TestShake <event> plays an event's shake without firing the event; Deep.Camera.TestPing
// [rangeM] draws a sonar pulse from the sub (even with Deep.Sonar.Pulse 0, so it can be previewed); Deep.Camera.Interference
// <0..1|-1> forces the feed interference for shots (-1 = follow the serpent again). Deep.Ping sends the local diver's
// sonar ping, as the V key does (in the water, after the cooldown).

#include "CoreMinimal.h"
#include "Camera/DeepCameraRules.h"
#include "Camera/DeepCameraSubsystem.h"
#include "Debug/DeepStateReport.h"
#include "Diver/DeepDiver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepSubmarine.h"
#include "HAL/IConsoleManager.h"

namespace
{
	FAutoConsoleCommandWithWorldAndArgs DiverPingCommand(
		TEXT("Deep.Ping"),
		TEXT("Deep.Ping: the local diver sends his sonar ping (V): in the water only, once per DEEP Tuning > Camera > DiverPingCooldownS."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			ADeepDiver* Diver = PC ? Cast<ADeepDiver>(PC->GetPawn()) : nullptr;
			if (!Diver)
			{
				UE_LOG(LogTemp, Warning, TEXT("Deep.Ping: no local diver"));
				return;
			}
			const float WaitS = Diver->GetPingWaitS();
			const bool bSent = Diver->Ping();
			UE_LOG(LogTemp, Display, TEXT("DEEPCAMERA diverping sent=%d inwater=%d wait=%.1fs"), bSent ? 1 : 0, Diver->IsInWater() ? 1 : 0, WaitS);
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs TestShakeCommand(
		TEXT("Deep.Camera.TestShake"),
		TEXT("Deep.Camera.TestShake <event>: plays that event's camera shake (e.g. hull_hit, cut_bite, descend_stop2) without firing it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UDeepCameraSubsystem* Camera = UDeepCameraSubsystem::Get(World);
			if (!Camera || Args.Num() < 1)
			{
				UE_LOG(LogTemp, Warning, TEXT("Usage: Deep.Camera.TestShake <event> (needs a game world)"));
				return;
			}
			Camera->PlayEventShake(FName(*Args[0]));
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs TestPingCommand(
		TEXT("Deep.Camera.TestPing"),
		TEXT("Deep.Camera.TestPing [rangeM]: draws a sonar pulse from the sub (or the camera if there is no sub) over rangeM (default: the helm range of the current Deep.Sonar.Style)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UDeepCameraSubsystem* Camera = UDeepCameraSubsystem::Get(World);
			if (!Camera)
			{
				UE_LOG(LogTemp, Warning, TEXT("Deep.Camera.TestPing needs a game world"));
				return;
			}
			const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
			const float RangeM = Args.Num() > 0 ? FCString::Atof(*Args[0])
				: DeepCameraRules::HelmPulseRangeM(DeepCameraRules::PulseLook(Tuning.Camera, UDeepCameraSubsystem::GetSonarStyle()), Tuning.Sonar.RangeM, Tuning.Camera);
			FVector Origin = FVector::ZeroVector;
			TActorIterator<ADeepSubmarine> Sub(World);
			if (Sub)
			{
				Origin = Sub->GetActorLocation();
			}
			else if (APlayerController* PC = World->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
			{
				Origin = PC->PlayerCameraManager->GetCameraLocation();
			}
			Camera->StartPulse(Origin, RangeM);
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs InterferenceCommand(
		TEXT("Deep.Camera.Interference"),
		TEXT("Deep.Camera.Interference <0..1|-1>: forces the feed interference to that strength (even with Deep.Look.Interference 0); -1 = follow the serpent again."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UDeepCameraSubsystem* Camera = UDeepCameraSubsystem::Get(World);
			if (!Camera || Args.Num() < 1)
			{
				UE_LOG(LogTemp, Warning, TEXT("Usage: Deep.Camera.Interference <0..1|-1> (needs a game world)"));
				return;
			}
			Camera->SetInterferenceOverride(FCString::Atof(*Args[0]));
			DeepStateReport::LogState(World);
		}));
}
