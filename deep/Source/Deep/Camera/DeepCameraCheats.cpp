// Camera cheats: Deep.Camera.TestShake <event> plays an event's shake without firing the event; Deep.Camera.TestPing
// [rangeM] draws a sonar pulse from the sub (even with Deep.Sonar.Pulse 0, so it can be previewed); Deep.Camera.Interference
// <0..1|-1> forces the feed interference for shots (-1 = follow the serpent again).

#include "CoreMinimal.h"
#include "Camera/DeepCameraSubsystem.h"
#include "Debug/DeepStateReport.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepSubmarine.h"
#include "HAL/IConsoleManager.h"

namespace
{
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
		TEXT("Deep.Camera.TestPing [rangeM]: draws a sonar pulse from the sub (or the camera if there is no sub) over rangeM (default: the sonar range)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UDeepCameraSubsystem* Camera = UDeepCameraSubsystem::Get(World);
			if (!Camera)
			{
				UE_LOG(LogTemp, Warning, TEXT("Deep.Camera.TestPing needs a game world"));
				return;
			}
			const float RangeM = Args.Num() > 0 ? FCString::Atof(*Args[0]) : UDeepTuningSettings::Get().Sonar.RangeM;
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
