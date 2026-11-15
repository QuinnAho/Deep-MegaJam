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

	/** Volume of the `group` column of Sounds.csv (sfx, ui, ambience, creature); 1 for an unknown group. */
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
};
