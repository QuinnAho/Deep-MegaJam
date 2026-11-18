#include "Debug/DeepShot.h"

#include "Containers/Ticker.h"
#include "Debug/DeepStateReport.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepShot, Log, All);

namespace DeepShot
{
	namespace
	{
		constexpr float QuitTimeoutSeconds = 30.f;

		enum class EStep : uint8 { Waiting, Running, Requested, Done };

		struct FShotState
		{
			TWeakObjectPtr<UWorld> World;
			FString Path;
			TArray<FString> Commands;
			float Delay = 6.f;
			float Elapsed = 0.f;
			float SinceRequest = 0.f;
			/** Next command to run, and seconds left on a `wait <s>` pseudo-command. */
			int32 NextCommand = 0;
			float WaitLeft = 0.f;
			EStep Step = EStep::Waiting;
		};

		void RunCommand(UWorld* World, const FString& Command)
		{
			UE_LOG(LogDeepShot, Display, TEXT("DeepShot: running '%s'"), *Command);
			if (APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr)
			{
				Player->ConsoleCommand(Command, true);
			}
			else if (GEngine)
			{
				GEngine->Exec(World, *Command);
			}
		}

		/** The shot's world, or the game world that replaced it (Deep.World.Caves restarts the level). */
		UWorld* ResolveWorld(FShotState& Shot)
		{
			if (UWorld* World = Shot.World.Get())
			{
				return World;
			}
			if (!GEngine)
			{
				return nullptr;
			}
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				if (World && (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE))
				{
					Shot.World = World;
					return World;
				}
			}
			return nullptr;
		}

		bool Tick(FShotState& Shot, float DeltaSeconds)
		{
			Shot.Elapsed += DeltaSeconds;
			switch (Shot.Step)
			{
			case EStep::Waiting:
				if (Shot.Elapsed >= Shot.Delay)
				{
					Shot.Step = EStep::Running;
				}
				return true;
			case EStep::Running:
			{
				// Commands run in order; `wait <seconds>` lets the game play on before the next one (or the shot).
				UWorld* World = ResolveWorld(Shot);
				Shot.WaitLeft -= DeltaSeconds;
				while (Shot.WaitLeft <= 0.f && Shot.Commands.IsValidIndex(Shot.NextCommand))
				{
					const FString& Command = Shot.Commands[Shot.NextCommand++];
					if (Command.StartsWith(TEXT("wait "), ESearchCase::IgnoreCase))
					{
						Shot.WaitLeft = FMath::Max(0.f, FCString::Atof(*Command.Mid(5)));
						UE_LOG(LogDeepShot, Display, TEXT("DeepShot: waiting %.1f s"), Shot.WaitLeft);
					}
					else
					{
						RunCommand(World, Command);
					}
				}
				if (Shot.WaitLeft <= 0.f && !Shot.Commands.IsValidIndex(Shot.NextCommand))
				{
					DeepStateReport::LogState(World);
					// Commands may change the HUD; the request is served on the next rendered frame.
					FScreenshotRequest::RequestScreenshot(Shot.Path, true, false);
					UE_LOG(LogDeepShot, Display, TEXT("DeepShot: requested %s"), *Shot.Path);
					Shot.Step = EStep::Requested;
				}
				return true;
			}
			case EStep::Requested:
				Shot.SinceRequest += DeltaSeconds;
				if (IFileManager::Get().FileSize(*Shot.Path) > 0 || Shot.SinceRequest > QuitTimeoutSeconds)
				{
					UE_LOG(LogDeepShot, Display, TEXT("DeepShot: %s, quitting"), IFileManager::Get().FileSize(*Shot.Path) > 0 ? TEXT("saved") : TEXT("timed out"));
					Shot.Step = EStep::Done;
					FPlatformMisc::RequestExit(false, TEXT("DeepShot"));
					return false;
				}
				return true;
			default:
				return false;
			}
		}
	}

	void StartIfRequested(UWorld* World)
	{
		static bool bStarted = false;
		FString Path;
		if (bStarted || !World || !World->IsGameWorld() || !FParse::Value(FCommandLine::Get(), TEXT("-DeepShot="), Path) || Path.IsEmpty())
		{
			return;
		}
		bStarted = true;

		TSharedRef<FShotState> Shot = MakeShared<FShotState>();
		Shot->World = World;
		Shot->Path = FPaths::ConvertRelativePathToFull(Path);
		float Delay = 6.f;
		if (FParse::Value(FCommandLine::Get(), TEXT("-DeepShotDelay="), Delay))
		{
			Shot->Delay = FMath::Max(0.f, Delay);
		}
		FString Commands;
		if (FParse::Value(FCommandLine::Get(), TEXT("-DeepShotCmds="), Commands))
		{
			Commands.TrimQuotesInline();
			Commands.ParseIntoArray(Shot->Commands, TEXT(";"), true);
			for (FString& Command : Shot->Commands)
			{
				Command.TrimStartAndEndInline();
			}
			Shot->Commands.RemoveAll([](const FString& Command) { return Command.IsEmpty(); });
		}
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Shot->Path), true);
		UE_LOG(LogDeepShot, Display, TEXT("DeepShot: %s after %.1f s, %d command(s)"), *Shot->Path, Shot->Delay, Shot->Commands.Num());
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Shot](float DeltaSeconds)
		{
			return Tick(*Shot, DeltaSeconds);
		}));
	}
}
