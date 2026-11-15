// The sound layer. Loads Content/Data/Sounds.csv, plays a one-shot when the event bus fires an event that has a
// row, and runs the state loops (ambience, breathing, winch, serpent, helm) with fades from DeepAudioRules.
// Each row plays its asset if it loads, else its DeepSynth built-in. Every start logs `DEEPAUDIO {json}`, also
// with no audio device (-NoSound), so tests and playtests can assert which sound fired.
// Runs on every machine for its own local diver: events about another player's diver are skipped.

#pragma once

#include "CoreMinimal.h"
#include "Audio/DeepAudioRules.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeepAudioSubsystem.generated.h"

class FJsonObject;
class UAudioComponent;
class USoundBase;
struct FDeepEventPayload;

enum class EDeepSoundSource : uint8
{
	None,
	Asset,
	Generated,
};

/** One sound start, for tests. */
struct FDeepAudioPlay
{
	FName Event;
	EDeepSoundSource Source = EDeepSoundSource::None;
	float Volume = 0.f;
};

UCLASS()
class DEEP_API UDeepAudioSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDeepAudioSubsystem* Get(const UObject* WorldContext);
	static const TCHAR* SourceName(EDeepSoundSource Source);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	/** One mix step: gathers the local diver's state, fades loops, stops finished one-shots. Tests call it directly. */
	void StepAudio(float DeltaSeconds);

	/** Plays the event's one-shot row. bForce skips the repeat guard. True if a row was found and not a loop. */
	bool PlayEvent(FName Event, bool bForce = false);
	/** PlayEvent with the row's volume and pitch scaled (the sonar's proximity tone rises and quickens). */
	bool PlayEventScaled(FName Event, float VolumeScale, float PitchScale, bool bForce = false);

	void SetMuted(bool bInMuted);
	bool IsMuted() const { return bMuted; }

	/** Reloads Sounds.csv and its assets. Loops restart on the next step. */
	void Reload();

	EDeepSoundSource Resolve(const FDeepSoundRow& Row) const;
	const FDeepSoundTable& GetTable() const { return Table; }

	/** One-shot starts, oldest first (bounded). */
	const TArray<FDeepAudioPlay>& GetHistory() const { return History; }
	int32 CountOf(FName Event) const;
	void ClearHistory() { History.Reset(); }

	/** Loops audible now (faded above silence), in DeepAudioLoop::All() order. */
	TArray<FName> GetPlayingLoops() const;
	float GetLoopVolume(FName Loop) const;

	/** `{loops:[...], muted, last}` for DEEPSTATE. */
	TSharedRef<FJsonObject> BuildStateJson() const;

	/** Logs one `DEEPAUDIO_ROW` line per row: event, source (asset/generated/none), builtin, asset. */
	void LogRows() const;

	// ---------------------------------------------------------------- the Captain's radio (D-040)

	/** The voice file for a CaptainLines.csv id (`<VoiceRoot>/<id>`), loaded once; null when there is none. */
	USoundBase* FindVoice(const FString& LineId);

	/** How long the radio must stay keyed for this line's voice (delay + voice + tail); 0 without a voice file. */
	float GetVoiceHoldSeconds(const FString& LineId);

	/**
	 * The Captain keys the radio for a line: squelch and hiss (`radio_on`) unless already keyed, the voice file if there
	 * is one (on the `voice` group, radio band filtered, ducking the rest), then `radio_off` after HoldSeconds.
	 * A new line while keyed cuts the old voice.
	 */
	void KeyRadio(const FString& LineId, float HoldSeconds);

	bool IsRadioKeyed() const { return bRadioKeyed; }
	/** True from the voice's start to its end (also with no audio device, from the file's length). */
	bool IsVoiceSpeaking() const;
	/** 0..1, the duck envelope now. */
	float GetDuck01() const { return Duck01; }

	// ---------------------------------------------------------------- music and the danger mix (Deep.Music)

	/** Deep.Music: 1 = music beds and cues, breathing by the Captain's danger tier. 0 = the mix before D-040. */
	static bool IsMusicOn();
	/** The loudest music bed fading or playing (DeepAudioMusic::Beds()), None when silent. */
	FName GetMusicBed() const;
	bool HasStungSerpent() const { return bStingPlayed; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void HandleEvent(FName Event, const FDeepEventPayload& Payload);
	FDeepAudioMixInputs GatherInputs(float DeltaSeconds);
	void StepLoop(FName Loop, const FDeepLoopTarget& Target, float DeltaSeconds);
	/** Final volume of a row: row x group x master, 0 when muted, clamped to 0..1. */
	float RowVolume(const FDeepSoundRow& Row) const;
	bool CanPlayAudio() const;
	UAudioComponent* StartSound(const FDeepSoundRow& Row, EDeepSoundSource Source, float Volume, float Pitch);
	TSharedPtr<const TArray<int16>, ESPMode::ThreadSafe> GetPcm(FName Builtin);
	void LogSound(FName Event, const FDeepSoundRow& Row, EDeepSoundSource Source, float Volume, float Pitch, const TCHAR* Action) const;

	FDeepSoundTable Table;

	/** Loaded assets by event. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USoundBase>> Assets;

	TMap<FName, TSharedPtr<const TArray<int16>, ESPMode::ThreadSafe>> PcmCache;

	struct FLoopVoice
	{
		float Volume = 0.f;
		bool bOn = false;
	};
	TMap<FName, FLoopVoice> Loops;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UAudioComponent>> LoopComponents;

	/** Generated one-shots still playing, and when to stop them (Clock); asset one-shots end on their own. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> OneShotComponents;
	TArray<double> OneShotStopAt;

	TMap<FName, double> LastPlayed;
	TArray<FDeepAudioPlay> History;
	double Clock = 0.0;
	bool bMuted = false;
	float LastSubDepthM = -1.f;
	float HelmHoldS = 0.f;
	FDelegateHandle EventHandle;

	void StepRadio(float DeltaSeconds);
	void StopVoice();

	/** Voice files by line id; a null value = looked for and absent. */
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<USoundBase>> VoiceCache;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> PendingVoice;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> VoiceComponent;

	void StepMusic(float DeltaSeconds, const FDeepAudioMixInputs& Inputs);
	bool bStingPlayed = false;
	bool bWasWon = false;
	/** Gathered with the mix inputs for the sting: the serpent's head to the local diver (m, -1 = none). */
	float SerpentDistanceM = -1.f;
	bool bFirstSightDone = false;

	bool bRadioKeyed = false;
	double RadioCloseAt = 0.0;
	double VoiceStartAt = 0.0;
	double VoiceEndAt = 0.0;
	float Duck01 = 0.f;
};
