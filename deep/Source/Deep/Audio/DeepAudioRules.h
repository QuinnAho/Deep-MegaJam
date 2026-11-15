// Pure audio data and rules, tested without a world: the Sounds.csv table and the loop mix (which loops play,
// how loud, how low and how dark, from the diver's state).

#pragma once

#include "CoreMinimal.h"

class UDeepAudioSettings;

/** The state names of the loops (the `event` column of their Sounds.csv rows). */
namespace DeepAudioLoop
{
	inline const TCHAR* DroneOut = TEXT("loop_drone_out");
	inline const TCHAR* DroneIn = TEXT("loop_drone_in");
	inline const TCHAR* Breath = TEXT("loop_breath");
	inline const TCHAR* BreathFast = TEXT("loop_breath_fast");
	inline const TCHAR* Winch = TEXT("loop_winch");
	inline const TCHAR* Serpent = TEXT("loop_serpent");
	inline const TCHAR* Helm = TEXT("loop_helm");

	DEEP_API TArray<FName> All();
}

/** The Captain's radio (D-040): rows the subsystem plays itself, not from a bus event, and their group. */
namespace DeepAudioRadio
{
	inline const TCHAR* On = TEXT("radio_on");
	inline const TCHAR* Off = TEXT("radio_off");
	/** Voice files and the squelch; never ducked. */
	inline const TCHAR* Group = TEXT("voice");

	DEEP_API TArray<FName> Rows();
}

/** One row of Content/Data/Sounds.csv. */
struct DEEP_API FDeepSoundRow
{
	FName Event;
	FName Group;
	/** A SoundWave or SoundCue path, e.g. /Game/Audio/S_Ping.S_Ping. Empty: use Builtin. */
	FString Asset;
	/** A DeepSynth built-in name. Empty: silent when there is no asset. */
	FName Builtin;
	float Volume = 1.f;
	float Pitch = 1.f;
	bool bLoop = false;
};

class DEEP_API FDeepSoundTable
{
public:
	/** Replaces every row from CSV text (columns event, group, asset, builtin, volume, pitch, loop). Returns the row count. */
	int32 LoadFromString(const FString& Csv);
	/** Loads Content/Data/Sounds.csv. */
	int32 LoadDefault();

	const FDeepSoundRow* Find(FName Event) const;
	const TArray<FDeepSoundRow>& GetRows() const { return Rows; }

private:
	TArray<FDeepSoundRow> Rows;
};

/** What the loop mix reads (gathered by UDeepAudioSubsystem from the local diver, the game state and the brain). */
struct FDeepAudioMixInputs
{
	bool bHasDiver = false;
	bool bInside = true;
	bool bSwimming = false;
	bool bCut = false;
	float DepthM = 0.f;
	float BailoutAirS = 0.f;
	bool bWinching = false;
	float Threat01 = 0.f;
	bool bHelmMoving = false;
};

/** One loop's target this frame. The subsystem fades toward Volume01; pitch and low-pass apply at once. */
struct FDeepLoopTarget
{
	float Volume01 = 0.f;
	float Pitch = 1.f;
	/** 0 = no low-pass filter. */
	float LowPassHz = 0.f;
};

namespace DeepAudioRules
{
	/** Every loop in DeepAudioLoop::All() gets an entry. */
	DEEP_API TMap<FName, FDeepLoopTarget> LoopTargets(const FDeepAudioMixInputs& In, const UDeepAudioSettings& Settings);

	/** Moves Current toward Target: up at 1/FadeInS per second, down at 1/FadeOutS per second. */
	DEEP_API float FadeToward(float Current, float Target, float DeltaS, float FadeInS, float FadeOutS);

	/** How far a group drops while the Captain speaks (dB <= 0): music, ambience, voice 0, everything else = effects. */
	DEEP_API float DuckDb(FName Group, const UDeepAudioSettings& Settings);

	/** Linear gain for a group at Duck01 (0 = no duck, 1 = full duck; faded by the subsystem). */
	DEEP_API float DuckGain(FName Group, float Duck01, const UDeepAudioSettings& Settings);
}
