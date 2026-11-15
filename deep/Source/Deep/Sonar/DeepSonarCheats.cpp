// Sonar cheats (Sonar lane): Deep.Sonar [0|1], Deep.Sonar.Bench <s>, Deep.Sonar.State. Each logs `DEEPSONAR {...}`.
// The sonar is local: these act on this machine's player HUD only.

#include "CoreMinimal.h"
#include "Debug/DeepStateReport.h"
#include "Engine/World.h"
#include "Game/DeepHUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Sonar/DeepSonarComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepSonarCheats, Log, All);

namespace
{
	UDeepSonarComponent* LocalSonar(UWorld* World, const TCHAR* Command)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const ADeepHUD* Hud = PC ? Cast<ADeepHUD>(PC->GetHUD()) : nullptr;
		UDeepSonarComponent* Sonar = Hud ? Hud->GetSonar() : nullptr;
		if (!Sonar)
		{
			UE_LOG(LogDeepSonarCheats, Warning, TEXT("%s: no local player HUD with a sonar"), Command);
		}
		return Sonar;
	}

	void LogSonar(UWorld* World, const UDeepSonarComponent& Sonar)
	{
		UE_LOG(LogDeepSonarCheats, Display, TEXT("DEEPSONAR %s"), *Sonar.StateJson());
		DeepStateReport::LogState(World);
	}

	FAutoConsoleCommandWithWorldAndArgs SonarCommand(
		TEXT("Deep.Sonar"),
		TEXT("Deep.Sonar [0|1]: turns the helm sonar off or on (no argument toggles). Off is the helm without it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDeepSonarComponent* Sonar = LocalSonar(World, TEXT("Deep.Sonar")))
			{
				Sonar->SetSonarEnabled(Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !Sonar->IsSonarEnabled());
				LogSonar(World, *Sonar);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs SonarBenchCommand(
		TEXT("Deep.Sonar.Bench"),
		TEXT("Deep.Sonar.Bench [seconds=5]: after that long, logs DEEPSONAR bench with the mean frame time and the sonar's own cost."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDeepSonarComponent* Sonar = LocalSonar(World, TEXT("Deep.Sonar.Bench")))
			{
				Sonar->StartBench(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 5.f);
				LogSonar(World, *Sonar);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs SonarStateCommand(
		TEXT("Deep.Sonar.State"),
		TEXT("Deep.Sonar.State: logs the sonar's state (DEEPSONAR {...})."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDeepSonarComponent* Sonar = LocalSonar(World, TEXT("Deep.Sonar.State")))
			{
				LogSonar(World, *Sonar);
			}
		}));
}
