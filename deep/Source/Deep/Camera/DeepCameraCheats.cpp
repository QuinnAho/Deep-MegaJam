// Camera cheats: Deep.Camera.TestShake <event> plays an event's shake without firing the event.

#include "CoreMinimal.h"
#include "Camera/DeepCameraSubsystem.h"
#include "Debug/DeepStateReport.h"
#include "Engine/World.h"
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
}
