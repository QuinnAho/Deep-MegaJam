// Harpoon console commands for testers. Each ends with the DEEPSTATE line (which carries `harpoon` and `hull_damage`).

#include "CoreMinimal.h"
#include "Debug/DeepStateReport.h"
#include "Diver/DeepDiver.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "HAL/IConsoleManager.h"
#include "Harpoon/DeepHarpoonComponent.h"
#include "World/DeepArena.h"
#include "World/DeepSubmarine.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepHarpoonCheats, Log, All);

namespace
{
	UDeepHarpoonComponent* RequireHarpoon(UWorld* World, const TCHAR* Command)
	{
		ADeepDiver* Diver = DeepStateReport::FindDiver(World);
		UDeepHarpoonComponent* Harpoon = Diver ? Diver->GetHarpoon() : nullptr;
		if (!Harpoon || !Diver->HasAuthority())
		{
			UE_LOG(LogDeepHarpoonCheats, Warning, TEXT("%s: no diver with a harpoon on this machine's server"), Command);
			return nullptr;
		}
		return Harpoon;
	}

	FAutoConsoleCommandWithWorldAndArgs HarpoonFireCommand(
		TEXT("Deep.Harpoon.Fire"),
		TEXT("Deep.Harpoon.Fire: fires the harpoon along the current view (or lets go if a line is out)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDeepHarpoonComponent* Harpoon = RequireHarpoon(World, TEXT("Deep.Harpoon.Fire")))
			{
				FVector Origin, Direction;
				if (Harpoon->GetView(Origin, Direction))
				{
					Harpoon->FireAlong(Origin, Direction);
				}
			}
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs HarpoonAimSubCommand(
		TEXT("Deep.Harpoon.AimSub"),
		TEXT("Deep.Harpoon.AimSub [fire]: points the view at the middle of the submarine's hull; with 'fire', fires too."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UDeepHarpoonComponent* Harpoon = RequireHarpoon(World, TEXT("Deep.Harpoon.AimSub"));
			ADeepDiver* Diver = Harpoon ? Cast<ADeepDiver>(Harpoon->GetOwner()) : nullptr;
			const ADeepSubmarine* Sub = DeepArena::FindSubmarine(World);
			AController* Controller = Diver ? Diver->GetController() : nullptr;
			if (!Diver || !Sub || !Controller)
			{
				UE_LOG(LogDeepHarpoonCheats, Warning, TEXT("Deep.Harpoon.AimSub: needs a controlled diver and a submarine"));
				DeepStateReport::LogState(World);
				return;
			}
			FVector Origin, Direction;
			Harpoon->GetView(Origin, Direction);
			FRotator View = (Sub->GetActorLocation() - Origin).Rotation();
			View.Roll = 0.f;
			Controller->SetControlRotation(View);
			if (Args.Num() > 0 && Args[0].Equals(TEXT("fire"), ESearchCase::IgnoreCase))
			{
				Harpoon->FireAlong(Origin, View.Vector());
			}
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs HarpoonResetCommand(
		TEXT("Deep.Harpoon.Reset"),
		TEXT("Deep.Harpoon.Reset: lets go of any line and reloads the harpoon to full shots."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDeepHarpoonComponent* Harpoon = RequireHarpoon(World, TEXT("Deep.Harpoon.Reset")))
			{
				Harpoon->Release();
				Harpoon->Reload();
			}
			DeepStateReport::LogState(World);
		}));
}
