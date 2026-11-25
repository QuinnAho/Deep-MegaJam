// Immersion I1 cheats (D-072). Local to this machine, like the sounds and the silt they act on. Each ends with DEEPSTATE.
//   Deep.Hull.Creak [stress]   a creak now, as the rules make it at this depth (stress = as on a hard turn)
//   Deep.Hull.Groan            the band groan now
//   Deep.Silt.Burst [n=8]      n puffs on the floor under the hull (frames)
//   Deep.Silt.Clear            no puffs
//   Deep.Silt.Hide 0|1         draw none (they still live): a GPU A/B with Deep.Silt.Bench
//   Deep.Silt.Bench [s=5]      after s seconds logs DEEPSILT bench {frame_ms, gpu_ms, puffs}
//   Deep.Echo.Test <m> [m ..]  a ping and its echo from these ranges (m; -1 = no return)
//   Deep.Echo.Export <wav> [m ..]  the ping and its echo as the game plays them, from these ranges or the last echo's
// Deep.Immersion 0|1 (the CVar, Immersion/DeepImmersion.cpp) turns all of I1 off and on.

#include "CoreMinimal.h"
#include "Audio/DeepAudioSubsystem.h"
#include "Audio/DeepSynth.h"
#include "Debug/DeepStateReport.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Immersion/DeepImmersion.h"
#include "Immersion/DeepSilt.h"
#include "Misc/Paths.h"
#include "Sim/DeepTuningSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepImmersionCheats, Log, All);

namespace
{
	TArray<float> ImmParseRanges(const TArray<FString>& Args, int32 First)
	{
		TArray<float> Out;
		for (int32 I = First; I < Args.Num(); ++I)
		{
			Out.Add(FCString::Atof(*Args[I]));
		}
		return Out;
	}

	FAutoConsoleCommandWithWorldAndArgs CreakCommand(
		TEXT("Deep.Hull.Creak"),
		TEXT("Deep.Hull.Creak [stress]: a hull creak now, as the rules make it at this depth (stress: as on a hard turn)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDeepImmersion* Immersion = UDeepImmersion::Get(World))
			{
				Immersion->PlayCreakNow(Args.Num() > 0 && Args[0].Equals(TEXT("stress"), ESearchCase::IgnoreCase));
			}
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs GroanCommand(
		TEXT("Deep.Hull.Groan"),
		TEXT("Deep.Hull.Groan: the 100 m band groan now."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDeepImmersion* Immersion = UDeepImmersion::Get(World))
			{
				Immersion->PlayGroanNow();
			}
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs SiltBurstCommand(
		TEXT("Deep.Silt.Burst"),
		TEXT("Deep.Silt.Burst [n=8]: n silt puffs on the floor under the hull."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ADeepSilt* Silt = ADeepSilt::Ensure(World);
			const int32 Count = Args.Num() > 0 ? FMath::Max(1, FCString::Atoi(*Args[0])) : 8;
			if (!Silt || !Silt->BurstUnderHull(Count))
			{
				UE_LOG(LogDeepImmersionCheats, Warning, TEXT("Deep.Silt.Burst: no floor under the hull within 60 m"));
			}
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs SiltClearCommand(
		TEXT("Deep.Silt.Clear"),
		TEXT("Deep.Silt.Clear: removes every silt puff."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ADeepSilt* Silt = ADeepSilt::Find(World))
			{
				Silt->Clear();
			}
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs SiltHideCommand(
		TEXT("Deep.Silt.Hide"),
		TEXT("Deep.Silt.Hide 0|1: 1 draws no silt (the puffs still live), for a GPU A/B; 0 draws it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ADeepSilt* Silt = ADeepSilt::Ensure(World))
			{
				Silt->SetDrawHidden(Args.Num() > 0 && FCString::Atoi(*Args[0]) != 0);
			}
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs SiltBenchCommand(
		TEXT("Deep.Silt.Bench"),
		TEXT("Deep.Silt.Bench [seconds=5]: after that long, logs DEEPSILT bench with the mean frame and GPU times and puffs."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ADeepSilt* Silt = ADeepSilt::Ensure(World))
			{
				Silt->StartBench(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 5.f);
			}
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs EchoTestCommand(
		TEXT("Deep.Echo.Test"),
		TEXT("Deep.Echo.Test <m> [m ..]: the sonar ping and its echo from these beam ranges (metres; -1 = no return)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UDeepImmersion* Immersion = UDeepImmersion::Get(World);
			const TArray<float> Ranges = ImmParseRanges(Args, 0);
			if (Immersion && !Ranges.IsEmpty())
			{
				if (UDeepAudioSubsystem* Audio = UDeepAudioSubsystem::Get(World))
				{
					Audio->PlayEvent(FName(TEXT("sonar_ping")), true);
				}
				Immersion->ScheduleEcho(Ranges);
			}
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs EchoExportCommand(
		TEXT("Deep.Echo.Export"),
		TEXT("Deep.Echo.Export <path.wav> [m ..]: writes the ping and its echo as the game plays them, from these ranges or the last echo's."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UDeepImmersion* Immersion = UDeepImmersion::Get(World);
			if (Args.IsEmpty())
			{
				UE_LOG(LogDeepImmersionCheats, Warning, TEXT("Deep.Echo.Export <path.wav> [m ..]"));
				return;
			}
			TArray<float> Ranges = ImmParseRanges(Args, 1);
			if (Ranges.IsEmpty() && Immersion)
			{
				Ranges = Immersion->GetLastEchoRanges();
			}
			const TArray<FDeepEchoTap> Taps = DeepImmersionRules::EchoTaps(Ranges, UDeepTuningSettings::Get().Immersion.Echo);
			TArray<DeepSynth::FEchoTap> Synth;
			for (const FDeepEchoTap& Tap : Taps)
			{
				Synth.Add({ Tap.DelayS, Tap.Gain, Tap.LowPassHz });
			}
			// The rows' own levels set the echo against the ping, as in the game.
			float EchoGain = 1.f;
			if (const UDeepAudioSubsystem* Audio = UDeepAudioSubsystem::Get(World))
			{
				const FDeepSoundRow* PingRow = Audio->GetTable().Find(FName(TEXT("sonar_ping")));
				const FDeepSoundRow* EchoRow = Audio->GetTable().Find(FName(TEXT("sonar_echo")));
				EchoGain = PingRow && EchoRow && PingRow->Volume > 0.f ? EchoRow->Volume / PingRow->Volume : 1.f;
			}
			const FName PingSound = [World]()
			{
				const UDeepAudioSubsystem* Audio = UDeepAudioSubsystem::Get(World);
				const FDeepSoundRow* Row = Audio ? Audio->GetTable().Find(FName(TEXT("sonar_ping"))) : nullptr;
				return Row ? Row->Builtin : FName(TEXT("sonar_ping_dry"));
			}();
			FString Path = Args[0];
			if (FPaths::IsRelative(Path))
			{
				Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir(), Path);
			}
			const bool bOk = DeepSynth::WriteWav(DeepSynth::RenderEcho(PingSound, FName(TEXT("sonar_echo")), Synth, EchoGain), Path);
			FString List;
			for (const FDeepEchoTap& Tap : Taps)
			{
				List += FString::Printf(TEXT(" [%.1f m: %.2f s, x%.2f, %.0f Hz]"), Tap.DistanceM, Tap.DelayS, Tap.Gain, Tap.LowPassHz);
			}
			UE_LOG(LogDeepImmersionCheats, Display, TEXT("DEEPAUDIO echo export %d beams ->%s -> %s ok=%d"), Ranges.Num(), *List, *Path, bOk ? 1 : 0);
		}));
}
