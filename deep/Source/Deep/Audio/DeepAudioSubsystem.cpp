#include "Audio/DeepAudioSubsystem.h"

#include "Audio/DeepAudioSettings.h"
#include "Audio/DeepProcWave.h"
#include "Audio/DeepSynth.h"
#include "Components/AudioComponent.h"
#include "Debug/DeepStateReport.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/DeepGameState.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Monster/DeepMonsterBrain.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Sound/SoundBase.h"
#include "Story/DeepEventBus.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepAudio, Log, All);

namespace
{
	constexpr int32 MaxAudioHistory = 64;
	/** A loop under this (0..1 of its fade) counts as silent: stopped and not reported. */
	constexpr float SilentLoop = 0.001f;
	/** A jump of the sub (GotoStop) bigger than this in one step is not driving. */
	constexpr float HelmJumpM = 50.f;
	constexpr float HelmHoldSeconds = 0.3f;

	FString ToJson(const TSharedRef<FJsonObject>& Json)
	{
		FString Out;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Json, Writer);
		return Out;
	}

	double Round2(float V) { return FMath::RoundToDouble(double(V) * 100.0) / 100.0; }
}

UDeepAudioSubsystem* UDeepAudioSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDeepAudioSubsystem>() : nullptr;
}

const TCHAR* UDeepAudioSubsystem::SourceName(EDeepSoundSource Source)
{
	switch (Source)
	{
	case EDeepSoundSource::Asset: return TEXT("asset");
	case EDeepSoundSource::Generated: return TEXT("generated");
	default: return TEXT("none");
	}
}

bool UDeepAudioSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UDeepAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UDeepEventBus>();
	Reload();
	if (UDeepEventBus* Bus = UDeepEventBus::Get(this))
	{
		EventHandle = Bus->OnEvent.AddUObject(this, &UDeepAudioSubsystem::HandleEvent);
	}
}

void UDeepAudioSubsystem::Deinitialize()
{
	if (UDeepEventBus* Bus = UDeepEventBus::Get(this))
	{
		Bus->OnEvent.Remove(EventHandle);
	}
	for (const TPair<FName, TObjectPtr<UAudioComponent>>& Pair : LoopComponents)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->Stop();
		}
	}
	for (UAudioComponent* Comp : OneShotComponents)
	{
		if (IsValid(Comp))
		{
			Comp->Stop();
		}
	}
	LoopComponents.Reset();
	OneShotComponents.Reset();
	OneShotStopAt.Reset();
	Super::Deinitialize();
}

TStatId UDeepAudioSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDeepAudioSubsystem, STATGROUP_Tickables);
}

bool UDeepAudioSubsystem::IsTickable() const
{
	const UWorld* World = GetWorld();
	return World && World->HasBegunPlay() && World->GetNetMode() != NM_DedicatedServer;
}

void UDeepAudioSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	StepAudio(DeltaTime);
}

void UDeepAudioSubsystem::Reload()
{
	Table.LoadDefault();
	Assets.Reset();
	for (const FDeepSoundRow& Row : Table.GetRows())
	{
		if (Row.Asset.IsEmpty())
		{
			continue;
		}
		FString Path = Row.Asset;
		if (!Path.Contains(TEXT(".")))
		{
			Path = Path + TEXT(".") + FPackageName::GetShortName(Path);
		}
		USoundBase* Sound = nullptr;
		if (FPackageName::IsValidObjectPath(Path) && FPackageName::DoesPackageExist(FPackageName::ObjectPathToPackageName(Path)))
		{
			Sound = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		}
		if (Sound)
		{
			Assets.Add(Row.Event, Sound);
		}
		else
		{
			UE_LOG(LogDeepAudio, Warning, TEXT("Sounds.csv: %s: asset %s did not load; using built-in '%s'"),
				*Row.Event.ToString(), *Row.Asset, *Row.Builtin.ToString());
		}
	}
	// Loops pick up new rows on the next step.
	for (const TPair<FName, TObjectPtr<UAudioComponent>>& Pair : LoopComponents)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->Stop();
		}
	}
	LoopComponents.Reset();
}

EDeepSoundSource UDeepAudioSubsystem::Resolve(const FDeepSoundRow& Row) const
{
	if (!Row.Asset.IsEmpty() && Assets.Contains(Row.Event))
	{
		return EDeepSoundSource::Asset;
	}
	if (UDeepAudioSettings::Get().bGeneratedSounds && DeepSynth::IsBuiltin(Row.Builtin))
	{
		return EDeepSoundSource::Generated;
	}
	return EDeepSoundSource::None;
}

float UDeepAudioSubsystem::RowVolume(const FDeepSoundRow& Row) const
{
	if (bMuted)
	{
		return 0.f;
	}
	const UDeepAudioSettings& Settings = UDeepAudioSettings::Get();
	return FMath::Clamp(Row.Volume * Settings.GroupVolume(Row.Group) * Settings.MasterVolume, 0.f, 1.f);
}

bool UDeepAudioSubsystem::CanPlayAudio() const
{
	const UWorld* World = GetWorld();
	return GEngine && GEngine->UseSound() && World && World->bAllowAudioPlayback
		&& World->GetNetMode() != NM_DedicatedServer && World->GetAudioDeviceRaw() != nullptr;
}

TSharedPtr<const TArray<int16>, ESPMode::ThreadSafe> UDeepAudioSubsystem::GetPcm(FName Builtin)
{
	if (const TSharedPtr<const TArray<int16>, ESPMode::ThreadSafe>* Found = PcmCache.Find(Builtin))
	{
		return *Found;
	}
	DeepSynth::FBuffer Buffer = DeepSynth::Generate(Builtin);
	TSharedPtr<const TArray<int16>, ESPMode::ThreadSafe> Pcm = MakeShared<const TArray<int16>, ESPMode::ThreadSafe>(MoveTemp(Buffer.Samples));
	PcmCache.Add(Builtin, Pcm);
	return Pcm;
}

UAudioComponent* UDeepAudioSubsystem::StartSound(const FDeepSoundRow& Row, EDeepSoundSource Source, float Volume, float Pitch)
{
	if (Source == EDeepSoundSource::None || !CanPlayAudio())
	{
		return nullptr;
	}
	USoundBase* Sound = nullptr;
	if (Source == EDeepSoundSource::Asset)
	{
		Sound = Assets.FindRef(Row.Event);
	}
	else
	{
		TSharedPtr<const TArray<int16>, ESPMode::ThreadSafe> Pcm = GetPcm(Row.Builtin);
		if (!Pcm.IsValid() || Pcm->IsEmpty())
		{
			return nullptr;
		}
		UDeepProcWave* Wave = NewObject<UDeepProcWave>(this);
		Wave->Init(Pcm, DeepSynth::SampleRate, Row.bLoop || DeepSynth::IsLoopBuiltin(Row.Builtin));
		// Loops fade in from near silence: keep them running instead of letting the engine cull them.
		Wave->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
		Sound = Wave;
	}
	if (!Sound)
	{
		return nullptr;
	}
	UAudioComponent* Comp = UGameplayStatics::CreateSound2D(this, Sound, Volume, Pitch, 0.f, nullptr, false, true);
	if (Comp)
	{
		Comp->Play();
	}
	return Comp;
}

void UDeepAudioSubsystem::LogSound(FName Event, const FDeepSoundRow& Row, EDeepSoundSource Source, float Volume, float Pitch, const TCHAR* Action) const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("event"), Event.ToString());
	Json->SetStringField(TEXT("action"), Action);
	Json->SetStringField(TEXT("source"), SourceName(Source));
	Json->SetStringField(TEXT("sound"), Source == EDeepSoundSource::Asset ? Row.Asset : (Source == EDeepSoundSource::Generated ? Row.Builtin.ToString() : FString()));
	Json->SetNumberField(TEXT("volume"), Round2(Volume));
	Json->SetNumberField(TEXT("pitch"), Round2(Pitch));
	Json->SetBoolField(TEXT("loop"), Row.bLoop);
	Json->SetBoolField(TEXT("device"), CanPlayAudio());
	Json->SetBoolField(TEXT("muted"), bMuted);
	UE_LOG(LogDeepAudio, Display, TEXT("DEEPAUDIO %s"), *ToJson(Json));
}

void UDeepAudioSubsystem::HandleEvent(FName Event, const FDeepEventPayload& Payload)
{
	// The host's bus also carries events about other players' divers; their own machines play those.
	if (const APawn* Pawn = Cast<APawn>(Payload.Source.Get()))
	{
		if (Cast<APlayerController>(Pawn->GetController()) && !Pawn->IsLocallyControlled())
		{
			return;
		}
	}
	PlayEvent(Event);
}

bool UDeepAudioSubsystem::PlayEvent(FName Event, bool bForce)
{
	const FDeepSoundRow* Row = Table.Find(Event);
	if (!Row || Row->bLoop)
	{
		return false;
	}
	const UDeepAudioSettings& Settings = UDeepAudioSettings::Get();
	if (!bForce)
	{
		if (const double* Last = LastPlayed.Find(Event); Last && Clock - *Last < Settings.RepeatGuardSeconds)
		{
			return false;
		}
	}
	LastPlayed.Add(Event, Clock);

	const EDeepSoundSource Source = Resolve(*Row);
	const float Volume = RowVolume(*Row);
	if (History.Num() >= MaxAudioHistory)
	{
		History.RemoveAt(0);
	}
	History.Add({ Event, Source, Volume });
	LogSound(Event, *Row, Source, Volume, Row->Pitch, TEXT("start"));

	// Prune finished one-shots before counting.
	for (int32 I = OneShotComponents.Num() - 1; I >= 0; --I)
	{
		if (!IsValid(OneShotComponents[I]))
		{
			OneShotComponents.RemoveAt(I);
			OneShotStopAt.RemoveAt(I);
		}
	}
	if (OneShotComponents.Num() >= Settings.MaxOneShots)
	{
		return true;
	}
	if (UAudioComponent* Comp = StartSound(*Row, Source, Volume, Row->Pitch))
	{
		double StopAt = TNumericLimits<double>::Max();
		if (Source == EDeepSoundSource::Generated)
		{
			StopAt = Clock + DeepSynth::ExpectedSamples(Row->Builtin) / double(DeepSynth::SampleRate) / FMath::Max(Row->Pitch, 0.1f) + 0.1;
		}
		OneShotComponents.Add(Comp);
		OneShotStopAt.Add(StopAt);
	}
	return true;
}

void UDeepAudioSubsystem::SetMuted(bool bInMuted)
{
	bMuted = bInMuted;
	UE_LOG(LogDeepAudio, Display, TEXT("DEEPAUDIO {\"action\":\"mute\",\"muted\":%s}"), bMuted ? TEXT("true") : TEXT("false"));
	StepAudio(0.f);
}

FDeepAudioMixInputs UDeepAudioSubsystem::GatherInputs(float DeltaSeconds)
{
	FDeepAudioMixInputs In;
	UWorld* World = GetWorld();
	ADeepDiver* Diver = DeepStateReport::FindDiver(World);
	const UDeepRunComponent* Run = Diver ? Diver->GetRun() : nullptr;
	const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
	if (!Diver || !Run)
	{
		return In;
	}
	In.bHasDiver = true;
	In.bInside = Run->IsInside() || (State && State->bOnTitle);
	In.bSwimming = Run->CanSwim();
	In.bCut = Run->GetState() == EDeepRunState::Cut;
	In.BailoutAirS = Run->GetBailoutAirSeconds();
	In.DepthM = In.bInside && State ? State->SubDepthM : Diver->GetDepthM();
	In.bWinching = Diver->IsWinching();
	const UDeepMonsterBrain* Brain = World->GetSubsystem<UDeepMonsterBrain>();
	In.Threat01 = Brain && World->GetNetMode() != NM_Client ? Brain->GetThreat01() : Run->GetMonsterThreat01();

	if (State)
	{
		if (LastSubDepthM >= 0.f && DeltaSeconds > 0.f)
		{
			const float Moved = FMath::Abs(State->SubDepthM - LastSubDepthM);
			if (Moved < HelmJumpM && Moved / DeltaSeconds > UDeepAudioSettings::Get().HelmMovingMps)
			{
				HelmHoldS = HelmHoldSeconds;
			}
		}
		LastSubDepthM = State->SubDepthM;
	}
	HelmHoldS = FMath::Max(0.f, HelmHoldS - DeltaSeconds);
	In.bHelmMoving = HelmHoldS > 0.f;
	return In;
}

void UDeepAudioSubsystem::StepLoop(FName Loop, const FDeepLoopTarget& Target, float DeltaSeconds)
{
	const UDeepAudioSettings& Settings = UDeepAudioSettings::Get();
	const FDeepSoundRow* Row = Table.Find(Loop);
	FLoopVoice& Voice = Loops.FindOrAdd(Loop);
	const float Wanted = Row ? Target.Volume01 : 0.f;
	Voice.Volume = DeepAudioRules::FadeToward(Voice.Volume, Wanted, DeltaSeconds, Settings.FadeInSeconds, Settings.FadeOutSeconds);
	const bool bWasOn = Voice.bOn;
	Voice.bOn = Voice.Volume > SilentLoop;

	TObjectPtr<UAudioComponent>* Found = LoopComponents.Find(Loop);
	UAudioComponent* Comp = Found && IsValid(*Found) ? Found->Get() : nullptr;
	if (!Voice.bOn || !Row)
	{
		if (bWasOn && Row)
		{
			LogSound(Loop, *Row, Resolve(*Row), 0.f, Row->Pitch * Target.Pitch, TEXT("stop"));
		}
		// Only ever stopped once faded to silence, so it never cuts hard.
		if (Comp)
		{
			Comp->Stop();
		}
		LoopComponents.Remove(Loop);
		return;
	}
	const EDeepSoundSource Source = Resolve(*Row);
	const float Volume = Voice.Volume * RowVolume(*Row);
	const float Pitch = FMath::Clamp(Row->Pitch * Target.Pitch, 0.5f, 2.f);
	if (!bWasOn)
	{
		LogSound(Loop, *Row, Source, RowVolume(*Row), Pitch, TEXT("start"));
	}
	if (!Comp)
	{
		// New, or an asset that does not loop by itself finished: start it again.
		Comp = StartSound(*Row, Source, Volume, Pitch);
		if (!Comp)
		{
			return;
		}
		LoopComponents.Add(Loop, Comp);
	}
	Comp->SetVolumeMultiplier(Volume);
	Comp->SetPitchMultiplier(Pitch);
	Comp->SetLowPassFilterEnabled(Target.LowPassHz > 0.f);
	if (Target.LowPassHz > 0.f)
	{
		Comp->SetLowPassFilterFrequency(Target.LowPassHz);
	}
}

void UDeepAudioSubsystem::StepAudio(float DeltaSeconds)
{
	Clock += DeltaSeconds;
	const FDeepAudioMixInputs Inputs = GatherInputs(DeltaSeconds);
	const TMap<FName, FDeepLoopTarget> Targets = DeepAudioRules::LoopTargets(Inputs, UDeepAudioSettings::Get());
	for (const FName& Loop : DeepAudioLoop::All())
	{
		StepLoop(Loop, Targets.FindRef(Loop), DeltaSeconds);
	}
	for (int32 I = OneShotComponents.Num() - 1; I >= 0; --I)
	{
		UAudioComponent* Comp = OneShotComponents[I];
		if (IsValid(Comp) && Clock < OneShotStopAt[I])
		{
			continue;
		}
		// Generated buffers end in silence, so stopping after their length cannot click.
		if (IsValid(Comp))
		{
			Comp->Stop();
		}
		OneShotComponents.RemoveAt(I);
		OneShotStopAt.RemoveAt(I);
	}
}

int32 UDeepAudioSubsystem::CountOf(FName Event) const
{
	int32 Count = 0;
	for (const FDeepAudioPlay& Play : History)
	{
		Count += Play.Event == Event ? 1 : 0;
	}
	return Count;
}

TArray<FName> UDeepAudioSubsystem::GetPlayingLoops() const
{
	TArray<FName> Out;
	for (const FName& Loop : DeepAudioLoop::All())
	{
		if (const FLoopVoice* Voice = Loops.Find(Loop); Voice && Voice->bOn)
		{
			Out.Add(Loop);
		}
	}
	return Out;
}

float UDeepAudioSubsystem::GetLoopVolume(FName Loop) const
{
	const FLoopVoice* Voice = Loops.Find(Loop);
	return Voice ? Voice->Volume : 0.f;
}

TSharedRef<FJsonObject> UDeepAudioSubsystem::BuildStateJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> LoopValues;
	for (const FName& Loop : GetPlayingLoops())
	{
		LoopValues.Add(MakeShared<FJsonValueString>(Loop.ToString()));
	}
	Json->SetArrayField(TEXT("loops"), LoopValues);
	Json->SetBoolField(TEXT("muted"), bMuted);
	Json->SetStringField(TEXT("last"), History.IsEmpty() ? FString() : History.Last().Event.ToString());
	return Json;
}

void UDeepAudioSubsystem::LogRows() const
{
	for (const FDeepSoundRow& Row : Table.GetRows())
	{
		UE_LOG(LogDeepAudio, Display, TEXT("DEEPAUDIO_ROW event=%s source=%s builtin=%s asset=%s volume=%.2f pitch=%.2f loop=%d"),
			*Row.Event.ToString(), SourceName(Resolve(Row)), *Row.Builtin.ToString(), Row.Asset.IsEmpty() ? TEXT("-") : *Row.Asset,
			Row.Volume, Row.Pitch, Row.bLoop ? 1 : 0);
	}
	UE_LOG(LogDeepAudio, Display, TEXT("DEEPAUDIO_ROW total=%d device=%d muted=%d"), Table.GetRows().Num(), CanPlayAudio() ? 1 : 0, bMuted ? 1 : 0);
}

// ---------------------------------------------------------------- console commands

namespace
{
	UDeepAudioSubsystem* RequireAudio(UWorld* World, const TCHAR* Command)
	{
		UDeepAudioSubsystem* Audio = UDeepAudioSubsystem::Get(World);
		if (!Audio)
		{
			UE_LOG(LogDeepAudio, Warning, TEXT("%s: no audio subsystem in this world"), Command);
		}
		return Audio;
	}

	FAutoConsoleCommandWithWorldAndArgs AudioListCommand(
		TEXT("Deep.Audio.List"),
		TEXT("Logs every Sounds.csv row and whether it resolves to an asset, a generated sound or nothing."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDeepAudioSubsystem* Audio = RequireAudio(World, TEXT("Deep.Audio.List")))
			{
				Audio->LogRows();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs AudioPlayCommand(
		TEXT("Deep.Audio.Play"),
		TEXT("Deep.Audio.Play <event>: plays that event's sound now (logs DEEPAUDIO)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UDeepAudioSubsystem* Audio = RequireAudio(World, TEXT("Deep.Audio.Play"));
			if (!Audio || Args.IsEmpty())
			{
				UE_LOG(LogDeepAudio, Warning, TEXT("Deep.Audio.Play <event>"));
				return;
			}
			if (!Audio->PlayEvent(FName(*Args[0]), true))
			{
				UE_LOG(LogDeepAudio, Warning, TEXT("Deep.Audio.Play: no one-shot row for '%s'"), *Args[0]);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs AudioMuteCommand(
		TEXT("Deep.Audio.Mute"),
		TEXT("Deep.Audio.Mute <0|1>: silences every sound (DEEPAUDIO lines are still logged)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDeepAudioSubsystem* Audio = RequireAudio(World, TEXT("Deep.Audio.Mute")))
			{
				Audio->SetMuted(Args.IsEmpty() ? !Audio->IsMuted() : FCString::Atoi(*Args[0]) != 0);
				DeepStateReport::LogState(World);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs AudioReloadCommand(
		TEXT("Deep.Audio.Reload"),
		TEXT("Reloads Content/Data/Sounds.csv and its assets."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDeepAudioSubsystem* Audio = RequireAudio(World, TEXT("Deep.Audio.Reload")))
			{
				Audio->Reload();
				Audio->LogRows();
			}
		}));
}
