// Pure audio data and rules, tested without a world: the Sounds.csv table and the loop mix (which loops play,
// how loud, how low and how dark, from the diver's state).

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

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
	/** Scared (danger mix, Deep.Music): ragged breathing over a heartbeat. */
	inline const TCHAR* BreathPanic = TEXT("loop_breath_panic");
	inline const TCHAR* Heartbeat = TEXT("loop_heartbeat");

	DEEP_API TArray<FName> All();
}

/** The `music` group (Deep.Music): beds and layers faded like loops, and one-shot cues. A row with no asset is silent. */
namespace DeepAudioMusic
{
	inline const TCHAR* Depth1 = TEXT("music_depth1");
	inline const TCHAR* Depth2 = TEXT("music_depth2");
	inline const TCHAR* Depth3 = TEXT("music_depth3");
	/** A layer over the depth bed while bailout air runs. */
	inline const TCHAR* Tension = TEXT("music_tension");
	inline const TCHAR* Title = TEXT("music_title");
	inline const TCHAR* Settlement = TEXT("music_settlement");
	/** One-shots: the first time the serpent is seen, and the relic banked. */
	inline const TCHAR* StingSerpent = TEXT("music_sting_serpent");
	inline const TCHAR* Win = TEXT("music_win");
	inline const TCHAR* Group = TEXT("music");

	/** The faded ones (loop rows), in report order. */
	DEEP_API TArray<FName> Beds();
	/** Every music row: Beds() then the cues. */
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
	/** D-070: someone is at the helm: at rest the hum idles at HelmIdleVolume01 instead of stopping. */
	bool bHelmManned = false;

	// Deep.Music (step 3 of D-040). Off: music silent, breathing as before.
	bool bMusic = false;
	/** The Captain's danger tier for the local diver: 0 calm, 1 tense, 2 scared. */
	int32 Danger = 0;
	bool bOnTitle = false;
	bool bSettling = false;
	bool bWon = false;
};

/** One loop's target this frame. The subsystem fades toward Volume01; pitch and low-pass apply at once. */
struct FDeepLoopTarget
{
	float Volume01 = 0.f;
	float Pitch = 1.f;
	/** 0 = no low-pass filter. */
	float LowPassHz = 0.f;
	/** Fade times for this loop; 0 = the settings' FadeInSeconds / FadeOutSeconds. */
	float FadeInS = 0.f;
	float FadeOutS = 0.f;
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

	/** 1, 2 or 3: the depth bed for this depth (MusicZone2M, MusicZone3M). */
	DEEP_API int32 MusicZone(float DepthM, const UDeepAudioSettings& Settings);

	/**
	 * Every bed in DeepAudioMusic::Beds() gets an entry. Title, else silence once won, else the settlement, else the
	 * depth bed (crossfading over MusicCrossfadeSeconds) with the tension layer while bailout air runs. All 0 without bMusic.
	 */
	DEEP_API TMap<FName, FDeepLoopTarget> MusicTargets(const FDeepAudioMixInputs& In, const UDeepAudioSettings& Settings);

	/** The serpent sting plays once: the first sight started, or the serpent came within StingSerpentM (a client's view; < 0 = no serpent). */
	DEEP_API bool ShouldSting(bool bAlreadyPlayed, bool bFirstSightDone, float SerpentDistanceM, const UDeepAudioSettings& Settings);

	/** "/Game/X/Y" or "/Game/X/Y.Y" -> the object path "/Game/X/Y.Y"; empty stays empty. */
	DEEP_API FString AssetObjectPath(const FString& Asset);

	/** "/Game/X/Y" -> its package "/Game/X/Y" (drops any ".Object"). */
	DEEP_API FString AssetPackage(const FString& Asset);

	/** The variation files a row may have beside its base: "<base>_01" .. "<base>_NN" (packages). */
	DEEP_API TArray<FString> VariantPackages(const FString& Asset, int32 MaxVariants);

	/** Which variation plays: random, never the last one twice when there are two or more. Rand01 in [0, 1). */
	DEEP_API int32 PickVariant(int32 Count, int32 LastIndex, float Rand01);

	/** A one-shot's pitch with +-PitchJitter (fraction) from Rand01 in [0, 1]; music and the Captain's voice are never jittered. */
	DEEP_API float JitterPitch(float Pitch, FName Group, float Rand01, const UDeepAudioSettings& Settings);

	/**
	 * Packages a cook must add for Sounds.csv: every row asset outside /Game/DeepAudio/ (pack sounds are cooked by
	 * reference only), each with the variations PackageExists finds. /Game/DeepAudio/ is cooked whole (DefaultGame.ini).
	 */
	DEEP_API TArray<FName> PackagesToCook(const FDeepSoundTable& Table, int32 MaxVariants, TFunctionRef<bool(const FString&)> PackageExists);
}
