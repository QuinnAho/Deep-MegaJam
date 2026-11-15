// Sea serpent console commands. Each ends by logging one `DEEPMONSTER {json}` line (see UDeepMonsterBrain::BuildStateJson).

#include "CoreMinimal.h"
#include "Diver/DeepDiver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Monster/DeepSerpent.h"
#include "Monster/DeepMonsterBrain.h"
#include "Monster/DeepMonsterRules.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepMonsterCheats, Log, All);

namespace
{
	UDeepMonsterBrain* RequireBrain(UWorld* World, const TCHAR* Command)
	{
		UDeepMonsterBrain* Brain = World ? World->GetSubsystem<UDeepMonsterBrain>() : nullptr;
		if (!Brain)
		{
			UE_LOG(LogDeepMonsterCheats, Warning, TEXT("%s: no monster brain in this world (game or PIE worlds only)"), Command);
		}
		return Brain;
	}

	void LogMonster(UDeepMonsterBrain* Brain)
	{
		UE_LOG(LogDeepMonsterCheats, Display, TEXT("DEEPMONSTER %s"), *Brain->BuildStateJson());
	}

	FAutoConsoleCommandWithWorldAndArgs MonsterStateCommand(
		TEXT("Deep.Monster.State"),
		TEXT("Logs one line: DEEPMONSTER {json} with state, noise, lit seconds, cooldown, distance to tether and diver (m), threat, stop."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDeepMonsterBrain* Brain = RequireBrain(World, TEXT("Deep.Monster.State")))
			{
				LogMonster(Brain);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs MonsterSpawnCommand(
		TEXT("Deep.Monster.Spawn"),
		TEXT("Spawns the sea serpent now, at any stop, in the dark at the lurk radius."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDeepMonsterBrain* Brain = RequireBrain(World, TEXT("Deep.Monster.Spawn")))
			{
				Brain->SpawnSerpent(true);
				LogMonster(Brain);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs MonsterPlaceCommand(
		TEXT("Deep.Monster.Place"),
		TEXT("Deep.Monster.Place <metres> [side_deg] [right_m] [up_m]: puts the serpent's head that far in front of the view (and right_m to the right, up_m up), swimming across it (side_deg turns its heading; default 90 = left to right)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UDeepMonsterBrain* Brain = RequireBrain(World, TEXT("Deep.Monster.Place"));
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			if (!Brain || !Player)
			{
				return;
			}
			const float Metres = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 12.f;
			const float SideDeg = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 90.f;
			FVector ViewLocation;
			FRotator ViewRotation;
			Player->GetPlayerViewPoint(ViewLocation, ViewRotation);
			FVector Forward = ViewRotation.Vector();
			ADeepSerpent* Serpent = Brain->SpawnSerpent(true);
			if (!Serpent)
			{
				return;
			}
			// Heading: the view's forward turned SideDeg about up, kept level so the body lies across the screen.
			const FVector Level = FVector(Forward.X, Forward.Y, 0.f).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
			const FVector Facing = Level.RotateAngleAxis(SideDeg, FVector::UpVector);
			// Optional offsets to the view's right and up (metres): put the head outside the beam so only the body
			// crosses it.
			const float RightM = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 0.f;
			const float UpM = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.f;
			const FVector Right = FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Y);
			const FVector Up = FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Z);
			Serpent->PlaceHead(ViewLocation + (Forward * Metres + Right * RightM + Up * UpM) * 100.f, Facing);
			LogMonster(Brain);
		}));

	FAutoConsoleCommandWithWorldAndArgs MonsterPoseCommand(
		TEXT("Deep.Monster.Pose"),
		TEXT("Deep.Monster.Pose <state|-1>: shots only. The serpent shows that state's motion and markings (0 lurk, 1 stalk, 2 recoil, 3 bite, 4 hunt) at full threat; the brain is untouched. -1 turns it off."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDeepMonsterBrain* Brain = RequireBrain(World, TEXT("Deep.Monster.Pose")))
			{
				if (ADeepSerpent* Serpent = Brain->SpawnSerpent(true))
				{
					Serpent->SetPoseOverride(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : -1);
				}
				LogMonster(Brain);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs MonsterNoiseCommand(
		TEXT("Deep.Monster.Noise"),
		TEXT("Deep.Monster.Noise <amount>: adds noise as if the diver made it (60 brings it in by default)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogDeepMonsterCheats, Warning, TEXT("Usage: Deep.Monster.Noise <amount>"));
				return;
			}
			if (UDeepMonsterBrain* Brain = RequireBrain(World, TEXT("Deep.Monster.Noise")))
			{
				// Through the engine hearing sense at the diver, like real noise; straight in if there is no diver.
				const float Amount = FCString::Atof(*Args[0]);
				TActorIterator<ADeepDiver> It(World);
				if (It)
				{
					UDeepMonsterRules::ReportDiverNoise(World, It->GetActorLocation(), Amount, *It);
				}
				else
				{
					Brain->AddNoise(Amount);
				}
				LogMonster(Brain);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs MonsterLitCommand(
		TEXT("Deep.Monster.Lit"),
		TEXT("Deep.Monster.Lit <seconds>: pretends the lamp beam is on its head for that long."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogDeepMonsterCheats, Warning, TEXT("Usage: Deep.Monster.Lit <seconds>"));
				return;
			}
			if (UDeepMonsterBrain* Brain = RequireBrain(World, TEXT("Deep.Monster.Lit")))
			{
				Brain->SetDebugLit(FCString::Atof(*Args[0]));
				LogMonster(Brain);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs MonsterRevealCommand(
		TEXT("Deep.Monster.Reveal"),
		TEXT("Shows the serpent's markings now, skipping the first-minute rule (for shots)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDeepMonsterBrain* Brain = RequireBrain(World, TEXT("Deep.Monster.Reveal")))
			{
				if (ADeepSerpent* Serpent = Brain->SpawnSerpent(true))
				{
					Serpent->RevealNow();
				}
				LogMonster(Brain);
			}
		}));
}
