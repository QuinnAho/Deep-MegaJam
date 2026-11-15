// Sound levels and switches. Project Settings > Game > DEEP Audio, saved to Config/DefaultGame.ini.
// Defaults are in code. Per-sound levels live in Content/Data/Sounds.csv; these multiply them.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DeepAudioSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "DEEP Audio"))
class DEEP_API UDeepAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDeepAudioSettings();

	static const UDeepAudioSettings& Get() { return *GetDefault<UDeepAudioSettings>(); }

	/** Volume of the `group` column of Sounds.csv (sfx, ui, ambience, creature, voice, music); 1 for an unknown group. */
	float GroupVolume(FName Group) const;

	UPROPERTY(Config, EditAnywhere, Category = "Levels", meta = (ClampMin = "0", ClampMax = "1"))
	float MasterVolume = 0.8f;

	/** Off: rows without a working asset are silent instead of using their built-in generated sound. */
	UPROPERTY(Config, EditAnywhere, Category = "Levels")
	bool bGeneratedSounds = true;

	UPROPERTY(Config, EditAnywhere, Category = "Levels", meta = (ClampMin = "0", ClampMax = "1"))
	float SfxVolume = 1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Levels", meta = (ClampMin = "0", ClampMax = "1"))
	float UiVolume = 1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Levels", meta = (ClampMin = "0", ClampMax = "1"))
	float AmbienceVolume = 1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Levels", meta = (ClampMin = "0", ClampMax = "1"))
	float CreatureVolume = 1.f;

	/** Seconds for a loop to rise from silence to full. */
	UPROPERTY(Config, EditAnywhere, Category = "Loops", meta = (ClampMin = "0.05"))
	float FadeInSeconds = 0.8f;

	/** Seconds for a loop to fall from full to silence. Loops never stop before they are silent. */
	UPROPERTY(Config, EditAnywhere, Category = "Loops", meta = (ClampMin = "0.05"))
	float FadeOutSeconds = 1.2f;

	/** The ambience is at its lowest and darkest at this depth. */
	UPROPERTY(Config, EditAnywhere, Category = "Loops", meta = (ClampMin = "1"))
	float DeepestM = 700.f;

	/** Low-pass cutoff of the outside drone at the surface and at DeepestM. */
	UPROPERTY(Config, EditAnywhere, Category = "Loops")
	float SurfaceLowPassHz = 6000.f;

	UPROPERTY(Config, EditAnywhere, Category = "Loops")
	float DeepLowPassHz = 700.f;

	/** Pitch of the outside drone at DeepestM (1 at the surface). */
	UPROPERTY(Config, EditAnywhere, Category = "Loops", meta = (ClampMin = "0.5", ClampMax = "1"))
	float DeepPitch = 0.6f;

	/** Breathing switches to the fast loop when bailout air is at or under this. */
	UPROPERTY(Config, EditAnywhere, Category = "Loops")
	float FastBreathAirS = 12.f;

	/** The serpent loop is silent at or under this threat. */
	UPROPERTY(Config, EditAnywhere, Category = "Loops", meta = (ClampMin = "0", ClampMax = "1"))
	float SerpentThreatFloor = 0.05f;

	/** The helm hum plays while the submarine's depth changes faster than this. */
	UPROPERTY(Config, EditAnywhere, Category = "Loops")
	float HelmMovingMps = 0.2f;

	/** The same event cannot start its sound again sooner than this. */
	UPROPERTY(Config, EditAnywhere, Category = "One-shots")
	float RepeatGuardSeconds = 0.08f;

	UPROPERTY(Config, EditAnywhere, Category = "One-shots")
	int32 MaxOneShots = 16;

	// ---------------------------------------------------------------- the Captain's radio (D-040, Deep.Captain.Radio)

	/** Voice files: a SoundWave named after the CaptainLines.csv id in this folder plays with the line. */
	UPROPERTY(Config, EditAnywhere, Category = "Radio")
	FString VoiceRoot = TEXT("/Game/DeepAudio/Captain");

	/** The `voice` group: the Captain's voice files and the radio squelch. */
	UPROPERTY(Config, EditAnywhere, Category = "Radio", meta = (ClampMin = "0", ClampMax = "1"))
	float VoiceVolume = 1.f;

	/** The voice starts this long after the opening squelch. */
	UPROPERTY(Config, EditAnywhere, Category = "Radio", meta = (ClampMin = "0"))
	float VoiceDelaySeconds = 0.15f;

	/** The radio stays keyed this long after the voice ends, then closes with the squelch. */
	UPROPERTY(Config, EditAnywhere, Category = "Radio", meta = (ClampMin = "0"))
	float VoiceTailSeconds = 0.3f;

	/** The radio band on voice files (style guide 9). The crush cannot be added at runtime: voice files arrive crushed. */
	UPROPERTY(Config, EditAnywhere, Category = "Radio")
	float RadioHighPassHz = 450.f;

	UPROPERTY(Config, EditAnywhere, Category = "Radio")
	float RadioLowPassHz = 3200.f;

	/** How far each group drops while the Captain's voice plays (dB, 0 = no duck). Effects = sfx, ui and creature. */
	UPROPERTY(Config, EditAnywhere, Category = "Radio", meta = (ClampMax = "0"))
	float DuckMusicDb = -9.f;

	UPROPERTY(Config, EditAnywhere, Category = "Radio", meta = (ClampMax = "0"))
	float DuckAmbienceDb = -6.f;

	UPROPERTY(Config, EditAnywhere, Category = "Radio", meta = (ClampMax = "0"))
	float DuckEffectsDb = -3.f;

	UPROPERTY(Config, EditAnywhere, Category = "Radio", meta = (ClampMin = "0.01"))
	float DuckAttackSeconds = 0.05f;

	UPROPERTY(Config, EditAnywhere, Category = "Radio", meta = (ClampMin = "0.01"))
	float DuckReleaseSeconds = 0.5f;

	// ---------------------------------------------------------------- music and the danger mix (Deep.Music)

	/** The `music` group. Music rows without an asset are silent: there is no generated music. */
	UPROPERTY(Config, EditAnywhere, Category = "Music", meta = (ClampMin = "0", ClampMax = "1"))
	float MusicVolume = 0.7f;

	/** Seconds for one depth bed to hand over to the next (and to and from the title and settlement). */
	UPROPERTY(Config, EditAnywhere, Category = "Music", meta = (ClampMin = "0.05"))
	float MusicCrossfadeSeconds = 3.f;

	/** The tension layer rises this fast once the tether is cut; it falls over MusicCrossfadeSeconds. */
	UPROPERTY(Config, EditAnywhere, Category = "Music", meta = (ClampMin = "0.05"))
	float MusicTensionFadeInSeconds = 1.f;

	/** Depth where the second and third beds start: halfway between the stops at 100, 300 and 500 m. */
	UPROPERTY(Config, EditAnywhere, Category = "Music")
	float MusicZone2M = 200.f;

	UPROPERTY(Config, EditAnywhere, Category = "Music")
	float MusicZone3M = 400.f;

	/** The serpent sting plays the first time the first sight starts, or the serpent comes this close (m). */
	UPROPERTY(Config, EditAnywhere, Category = "Music", meta = (ClampMin = "0"))
	float StingSerpentM = 30.f;
};
