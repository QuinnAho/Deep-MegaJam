// Console commands for testing. Add a new one with another FAutoConsoleCommandWithWorldAndArgs below;
// names start with "Deep." and the command should end by logging state if it changes any.

#include "CoreMinimal.h"
#include "Debug/DeepStateReport.h"
#include "Diver/DeepDiver.h"
#include "Engine/World.h"
#include "Game/DeepGameMode.h"
#include "HAL/IConsoleManager.h"
#include "Sim/DeepTuningSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepCheats, Log, All);

namespace
{
	ADeepDiver* RequireDiver(UWorld* World, const TCHAR* Command)
	{
		ADeepDiver* Diver = DeepStateReport::FindDiver(World);
		if (!Diver)
		{
			UE_LOG(LogDeepCheats, Warning, TEXT("%s: no diver in this world"), Command);
		}
		return Diver;
	}

	FAutoConsoleCommandWithWorldAndArgs StateCommand(
		TEXT("Deep.State"),
		TEXT("Logs one line: DEEPSTATE {json} with depth, zone, cargo, tether, credits, hull, stop and diver position (cm)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs GiveSalvageCommand(
		TEXT("Deep.GiveSalvage"),
		TEXT("Deep.GiveSalvage <kg> <value>: adds a piece of cargo to the diver."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 2)
			{
				UE_LOG(LogDeepCheats, Warning, TEXT("Usage: Deep.GiveSalvage <kg> <value>"));
				return;
			}
			if (ADeepDiver* Diver = RequireDiver(World, TEXT("Deep.GiveSalvage")))
			{
				FDeepCargoItem Item;
				Item.Kg = FMath::Max(0.f, FCString::Atof(*Args[0]));
				Item.Value = FMath::Max(0, FCString::Atoi(*Args[1]));
				// Size (for Patience refill later) is the tuned kind closest in mass.
				float BestGap = TNumericLimits<float>::Max();
				for (const FDeepSalvageKind& Kind : UDeepTuningSettings::Get().SalvageKinds)
				{
					if (FMath::Abs(Kind.Kg - Item.Kg) < BestGap)
					{
						BestGap = FMath::Abs(Kind.Kg - Item.Kg);
						Item.Size = Kind.Size;
					}
				}
				Diver->AddCargo(Item);
				DeepStateReport::LogState(World);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GotoStopCommand(
		TEXT("Deep.GotoStop"),
		TEXT("Deep.GotoStop <n>: moves the sub to stop n (1-based): light, fog and tether length change."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogDeepCheats, Warning, TEXT("Usage: Deep.GotoStop <n>"));
				return;
			}
			ADeepGameMode* GameMode = World ? World->GetAuthGameMode<ADeepGameMode>() : nullptr;
			if (!GameMode)
			{
				UE_LOG(LogDeepCheats, Warning, TEXT("Deep.GotoStop: needs the DEEP game mode on the server"));
				return;
			}
			GameMode->GotoStop(FCString::Atoi(*Args[0]));
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs TeleportCommand(
		TEXT("Deep.Teleport"),
		TEXT("Deep.Teleport <x> <y> <z>: moves the diver to a world position in cm (the tether limit still applies)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 3)
			{
				UE_LOG(LogDeepCheats, Warning, TEXT("Usage: Deep.Teleport <x> <y> <z>"));
				return;
			}
			if (ADeepDiver* Diver = RequireDiver(World, TEXT("Deep.Teleport")))
			{
				Diver->TeleportDiver(FVector(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), FCString::Atof(*Args[2])));
				DeepStateReport::LogState(World);
			}
		}));
}
