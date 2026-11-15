#include "Audio/DeepAudioRules.h"

#include "Audio/DeepAudioSettings.h"
#include "Misc/FileHelper.h"
#include "Story/DeepCaptainLines.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepAudioData, Log, All);

TArray<FName> DeepAudioLoop::All()
{
	return { DroneOut, DroneIn, Breath, BreathFast, Winch, Serpent, Helm, BreathPanic, Heartbeat };
}

TArray<FName> DeepAudioMusic::Beds()
{
	return { Depth1, Depth2, Depth3, Tension, Title, Settlement };
}

TArray<FName> DeepAudioMusic::All()
{
	TArray<FName> Out = Beds();
	Out.Add(StingSerpent);
	Out.Add(Win);
	return Out;
}

int32 FDeepSoundTable::LoadFromString(const FString& Csv)
{
	Rows.Reset();
	for (const TMap<FString, FString>& Cells : DeepData::ParseCsv(Csv))
	{
		auto Cell = [&Cells](const TCHAR* Key) -> FString
		{
			const FString* Value = Cells.Find(Key);
			return Value ? Value->TrimStartAndEnd() : FString();
		};
		const FString Event = Cell(TEXT("event"));
		if (Event.IsEmpty())
		{
			continue;
		}
		FDeepSoundRow Row;
		Row.Event = FName(*Event);
		Row.Group = FName(*Cell(TEXT("group")));
		Row.Asset = Cell(TEXT("asset"));
		const FString Builtin = Cell(TEXT("builtin"));
		Row.Builtin = Builtin.IsEmpty() ? NAME_None : FName(*Builtin);
		const FString Volume = Cell(TEXT("volume"));
		const FString Pitch = Cell(TEXT("pitch"));
		Row.Volume = Volume.IsEmpty() ? 1.f : FMath::Clamp(FCString::Atof(*Volume), 0.f, 1.f);
		Row.Pitch = Pitch.IsEmpty() ? 1.f : FMath::Clamp(FCString::Atof(*Pitch), 0.5f, 2.f);
		const FString Loop = Cell(TEXT("loop"));
		Row.bLoop = Loop == TEXT("1") || Loop.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Loop.Equals(TEXT("yes"), ESearchCase::IgnoreCase);
		Rows.Add(MoveTemp(Row));
	}
	return Rows.Num();
}

int32 FDeepSoundTable::LoadDefault()
{
	const FString Path = DeepData::DataPath(TEXT("Sounds.csv"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogDeepAudioData, Warning, TEXT("Could not read %s; the game is silent"), *Path);
		Rows.Reset();
		return 0;
	}
	const int32 Count = LoadFromString(Text);
	UE_LOG(LogDeepAudioData, Log, TEXT("Loaded %d sound rows from %s"), Count, *Path);
	return Count;
}

const FDeepSoundRow* FDeepSoundTable::Find(FName Event) const
{
	return Rows.FindByPredicate([Event](const FDeepSoundRow& Row) { return Row.Event == Event; });
}

namespace DeepAudioRules
{
	TMap<FName, FDeepLoopTarget> LoopTargets(const FDeepAudioMixInputs& In, const UDeepAudioSettings& Settings)
	{
		TMap<FName, FDeepLoopTarget> Out;
		for (const FName& Name : DeepAudioLoop::All())
		{
			Out.Add(Name);
		}
		if (!In.bHasDiver)
		{
			return Out;
		}
		const float Depth01 = FMath::Clamp(In.DepthM / FMath::Max(Settings.DeepestM, 1.f), 0.f, 1.f);

		// Outside: lower and darker with depth. Inside: the sub's own hum, a little lower deep down, never as dark.
		FDeepLoopTarget& Out_ = Out[FName(DeepAudioLoop::DroneOut)];
		Out_.Volume01 = In.bInside ? 0.f : 1.f;
		Out_.Pitch = FMath::Lerp(1.f, Settings.DeepPitch, Depth01);
		Out_.LowPassHz = FMath::Lerp(Settings.SurfaceLowPassHz, Settings.DeepLowPassHz, Depth01);

		FDeepLoopTarget& In_ = Out[FName(DeepAudioLoop::DroneIn)];
		In_.Volume01 = In.bInside ? 1.f : 0.f;
		In_.Pitch = FMath::Lerp(1.f, 0.85f, Depth01);
		In_.LowPassHz = FMath::Lerp(Settings.SurfaceLowPassHz, 2.f * Settings.DeepLowPassHz, Depth01);

		// Breathing follows the danger (Deep.Music): calm, fast when tense, panic over a heartbeat when scared.
		// Low bailout air is always at least fast. Off: only calm and low-air fast, as before.
		const bool bLowAir = In.bSwimming && In.bCut && In.BailoutAirS <= Settings.FastBreathAirS;
		int32 Tier = In.bMusic ? FMath::Clamp(In.Danger, 0, 2) : 0;
		Tier = bLowAir ? FMath::Max(Tier, 1) : Tier;
		Out[FName(DeepAudioLoop::Breath)].Volume01 = In.bSwimming && Tier == 0 ? 1.f : 0.f;
		Out[FName(DeepAudioLoop::BreathFast)].Volume01 = In.bSwimming && Tier == 1 ? 1.f : 0.f;
		Out[FName(DeepAudioLoop::BreathPanic)].Volume01 = In.bSwimming && Tier == 2 ? 1.f : 0.f;
		Out[FName(DeepAudioLoop::Heartbeat)].Volume01 = In.bSwimming && Tier == 2 ? 1.f : 0.f;

		Out[FName(DeepAudioLoop::Winch)].Volume01 = In.bSwimming && In.bWinching ? 1.f : 0.f;

		FDeepLoopTarget& Serpent = Out[FName(DeepAudioLoop::Serpent)];
		const float Threat = FMath::Clamp(In.Threat01, 0.f, 1.f);
		const float Floor = Settings.SerpentThreatFloor;
		const float Above = Threat <= Floor ? 0.f : (Threat - Floor) / FMath::Max(1.f - Floor, 1e-3f);
		Serpent.Volume01 = In.bInside ? 0.f : FMath::SmoothStep(0.f, 1.f, Above);
		Serpent.Pitch = FMath::Lerp(0.8f, 1.1f, Above);

		Out[FName(DeepAudioLoop::Helm)].Volume01 = In.bHelmMoving ? 1.f : 0.f;
		return Out;
	}

	float FadeToward(float Current, float Target, float DeltaS, float FadeInS, float FadeOutS)
	{
		if (Target > Current)
		{
			return FMath::Min(Target, Current + DeltaS / FMath::Max(FadeInS, 1e-3f));
		}
		return FMath::Max(Target, Current - DeltaS / FMath::Max(FadeOutS, 1e-3f));
	}

	float DuckDb(FName Group, const UDeepAudioSettings& Settings)
	{
		if (Group == FName(TEXT("music")))
		{
			return Settings.DuckMusicDb;
		}
		if (Group == FName(TEXT("ambience")))
		{
			return Settings.DuckAmbienceDb;
		}
		if (Group == FName(DeepAudioRadio::Group))
		{
			return 0.f;
		}
		return Settings.DuckEffectsDb;
	}

	float DuckGain(FName Group, float Duck01, const UDeepAudioSettings& Settings)
	{
		const float Db = FMath::Min(0.f, DuckDb(Group, Settings)) * FMath::Clamp(Duck01, 0.f, 1.f);
		return FMath::Pow(10.f, Db / 20.f);
	}

	int32 MusicZone(float DepthM, const UDeepAudioSettings& Settings)
	{
		if (DepthM >= Settings.MusicZone3M)
		{
			return 3;
		}
		return DepthM >= Settings.MusicZone2M ? 2 : 1;
	}

	TMap<FName, FDeepLoopTarget> MusicTargets(const FDeepAudioMixInputs& In, const UDeepAudioSettings& Settings)
	{
		TMap<FName, FDeepLoopTarget> Out;
		const float Cross = FMath::Max(Settings.MusicCrossfadeSeconds, 0.05f);
		for (const FName& Name : DeepAudioMusic::Beds())
		{
			FDeepLoopTarget& Target = Out.Add(Name);
			Target.FadeInS = Cross;
			Target.FadeOutS = Cross;
		}
		if (!In.bMusic)
		{
			return Out;
		}
		if (In.bOnTitle)
		{
			Out[FName(DeepAudioMusic::Title)].Volume01 = 1.f;
			return Out;
		}
		if (!In.bHasDiver || In.bWon)
		{
			// Won: the beds give way to the win cue.
			return Out;
		}
		if (In.bSettling)
		{
			Out[FName(DeepAudioMusic::Settlement)].Volume01 = 1.f;
			return Out;
		}
		static const TCHAR* Zones[] = { DeepAudioMusic::Depth1, DeepAudioMusic::Depth2, DeepAudioMusic::Depth3 };
		Out[FName(Zones[MusicZone(In.DepthM, Settings) - 1])].Volume01 = 1.f;
		FDeepLoopTarget& Tension = Out[FName(DeepAudioMusic::Tension)];
		Tension.Volume01 = In.bSwimming && In.bCut ? 1.f : 0.f;
		Tension.FadeInS = FMath::Max(Settings.MusicTensionFadeInSeconds, 0.05f);
		return Out;
	}

	bool ShouldSting(bool bAlreadyPlayed, bool bFirstSightDone, float SerpentDistanceM, const UDeepAudioSettings& Settings)
	{
		if (bAlreadyPlayed)
		{
			return false;
		}
		return bFirstSightDone || (SerpentDistanceM >= 0.f && SerpentDistanceM <= Settings.StingSerpentM);
	}
}

TArray<FName> DeepAudioRadio::Rows()
{
	return { On, Off };
}
