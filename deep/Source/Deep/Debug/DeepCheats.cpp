// Console commands for testing. Add a new one with another FAutoConsoleCommandWithWorldAndArgs below;
// names start with "Deep." and the command should end by logging state if it changes any.

#include "CoreMinimal.h"
#include "Debug/DeepStateReport.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/World.h"
#include "Game/DeepGameMode.h"
#include "GameFramework/Controller.h"
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
				// Counts as a pickup: Patience refill and the Captain's comment.
				Diver->GetRun()->NotifyPickup(Item);
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

	FAutoConsoleCommandWithWorldAndArgs LookCommand(
		TEXT("Deep.Look"),
		TEXT("Deep.Look <pitch> [yaw]: points the diver's view (degrees; pitch -90 straight down, +90 up; yaw 0 = toward the sub's nose)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogDeepCheats, Warning, TEXT("Usage: Deep.Look <pitch> [yaw]"));
				return;
			}
			ADeepDiver* Diver = RequireDiver(World, TEXT("Deep.Look"));
			AController* Controller = Diver ? Diver->GetController() : nullptr;
			if (!Controller)
			{
				UE_LOG(LogDeepCheats, Warning, TEXT("Deep.Look: the diver has no controller"));
				return;
			}
			FRotator View = Controller->GetControlRotation();
			View.Pitch = FMath::Clamp(FCString::Atof(*Args[0]), -89.f, 89.f);
			if (Args.Num() >= 2)
			{
				View.Yaw = FCString::Atof(*Args[1]);
			}
			View.Roll = 0.f;
			Controller->SetControlRotation(View);
			DeepStateReport::LogState(World);
		}));

	/** Runs Action on the diver's run component (server), then logs state. */
	void WithRun(UWorld* World, const TCHAR* Command, TFunctionRef<void(UDeepRunComponent&)> Action)
	{
		ADeepDiver* Diver = RequireDiver(World, Command);
		if (!Diver || !Diver->GetRun())
		{
			return;
		}
		if (!Diver->HasAuthority())
		{
			UE_LOG(LogDeepCheats, Warning, TEXT("%s: run on the server"), Command);
			return;
		}
		Action(*Diver->GetRun());
		DeepStateReport::LogState(World);
	}

	FAutoConsoleCommandWithWorldAndArgs SetPatienceCommand(
		TEXT("Deep.SetPatience"),
		TEXT("Deep.SetPatience <0..1>: sets the Captain's Patience as a fraction of full. 0 cuts the tether on the next tick."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogDeepCheats, Warning, TEXT("Usage: Deep.SetPatience <0..1>"));
				return;
			}
			const float Fraction = FCString::Atof(*Args[0]);
			WithRun(World, TEXT("Deep.SetPatience"), [Fraction](UDeepRunComponent& Run) { Run.SetPatienceFraction(Fraction); });
		}));

	FAutoConsoleCommandWithWorldAndArgs ForceCutCommand(
		TEXT("Deep.ForceCut"),
		TEXT("Deep.ForceCut: cuts the tether now (as if Patience ran out); bailout air starts."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			WithRun(World, TEXT("Deep.ForceCut"), [](UDeepRunComponent& Run) { Run.CutTether(false); });
		}));

	FAutoConsoleCommandWithWorldAndArgs BankCommand(
		TEXT("Deep.Bank"),
		TEXT("Deep.Bank: banks the diver's cargo wherever it is and opens the settlement screen."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			WithRun(World, TEXT("Deep.Bank"), [](UDeepRunComponent& Run) { Run.Bank(); });
		}));

	FAutoConsoleCommandWithWorldAndArgs ContinueCommand(
		TEXT("Deep.Continue"),
		TEXT("Deep.Continue: leaves the settlement screen (descends if the hull allows)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			WithRun(World, TEXT("Deep.Continue"), [](UDeepRunComponent& Run) { Run.ContinueAfterSettlement(); });
		}));

	FAutoConsoleCommandWithWorldAndArgs ResetRunCommand(
		TEXT("Deep.ResetRun"),
		TEXT("Deep.ResetRun: credits, hull, stop and diver number back to the start; a fresh dive at stop 1."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			WithRun(World, TEXT("Deep.ResetRun"), [](UDeepRunComponent& Run) { Run.ResetRun(); });
		}));
}
