#include "Audio/DeepAudioRules.h"

#include "Audio/DeepAudioSettings.h"
#include "Misc/FileHelper.h"
#include "Story/DeepCaptainLines.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepAudioData, Log, All);

TArray<FName> DeepAudioLoop::All()
{
	return { DroneOut, DroneIn, Breath, BreathFast, Winch, Serpent, Helm };
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

		const bool bFast = In.bSwimming && In.bCut && In.BailoutAirS <= Settings.FastBreathAirS;
		Out[FName(DeepAudioLoop::Breath)].Volume01 = In.bSwimming && !bFast ? 1.f : 0.f;
		Out[FName(DeepAudioLoop::BreathFast)].Volume01 = bFast ? 1.f : 0.f;

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
}
