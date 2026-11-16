// The player's own settings: master volume, mouse sensitivity, invert vertical look, ray tracing.
// Saved per user to GameUserSettings.ini in the user's saved config (packaged Windows build:
// %LOCALAPPDATA%\Deep\Saved\Config\Windows\GameUserSettings.ini), the same file and the same call
// (SaveConfig(CPF_Config, GGameUserSettingsIni)) the engine's UGameUserSettings::SaveSettings uses.
// A separate config object instead of a UGameUserSettings subclass: a subclass needs
// GameUserSettingsClassName in DefaultEngine.ini, and this needs nothing outside this folder.
// The CDO loads the file when the class is first used; ApplyAll pushes the values to the engine.
// Also the credits table (Content/Data/Credits.csv) the credits screen lists.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DeepUserSettings.generated.h"

/** The settings screen's rows, in order. */
enum class EDeepSettingRow : uint8 { Volume, Sensitivity, InvertLook, RayTracing, Back, Count };

/** Pure steps and labels for the settings screen (tested in Deep.UI.Settings.*). */
namespace DeepSettingsRules
{
	constexpr float VolumeStep = 0.05f;
	constexpr float SensitivityMin = 0.2f;
	constexpr float SensitivityMax = 3.0f;
	constexpr float SensitivityStep = 0.1f;

	/** Volume 0..1 moved one step (Direction -1 or +1), snapped to the step and clamped. */
	DEEP_API float StepVolume(float Volume01, int32 Direction);
	/** Sensitivity multiplier moved one step, snapped and clamped to [SensitivityMin, SensitivityMax]. */
	DEEP_API float StepSensitivity(float Scale, int32 Direction);
	/** 0..1 position of a sensitivity on its bar. */
	DEEP_API float SensitivityBar(float Scale);
	DEEP_API FString VolumeLabel(float Volume01);
	DEEP_API FString SensitivityLabel(float Scale);
	DEEP_API FString RowLabel(EDeepSettingRow Row);
	/** Look input after the player's scale and invert: X yaw, Y pitch. */
	DEEP_API FVector2D ScaleLook(const FVector2D& Look, float BaseSensitivity, float Scale, bool bInvert);
}

/** One line of Content/Data/Credits.csv. */
struct FDeepCreditRow
{
	FString Item;
	FString Author;
	FString Licence;
	FString Note;
};

namespace DeepCredits
{
	/** Rows of a credits CSV text (columns item, author, licence, note); rows with no item are skipped. */
	DEEP_API TArray<FDeepCreditRow> Parse(const FString& CsvText);
	/** Content/Data/Credits.csv, read once and cached. Empty when the file is missing. */
	DEEP_API const TArray<FDeepCreditRow>& Get();
}

UCLASS(Config = GameUserSettings, ConfigDoNotCheckDefaults)
class DEEP_API UDeepUserSettings : public UObject
{
	GENERATED_BODY()

public:
	/** The loaded settings (the class default object, loaded from the user's GameUserSettings.ini). */
	static UDeepUserSettings& Get() { return *GetMutableDefault<UDeepUserSettings>(); }

	/** Writes the values to the user's GameUserSettings.ini now. */
	void Save();

	/** Pushes every value to the engine: the audio device's volume and, if the player chose, ray tracing. */
	void ApplyAll(const UObject* WorldContext) const;
	/** The master volume on the world's audio device (all sound, playing or not). */
	void ApplyVolume(const UObject* WorldContext) const;

	void SetVolume(const UObject* WorldContext, float Volume01);
	void SetSensitivity(float Scale);
	void SetInvertLook(bool bInvert);
	void SetRayTracing(bool bOn);
	/** What ray tracing is now: the player's choice, or the project's setting when they never chose. */
	bool IsRayTracingOn() const;

	/** Player master volume, 0..1, on top of the mix (UDeepAudioSettings::MasterVolume). */
	UPROPERTY(Config)
	float MasterVolume = 1.f;

	/** Multiplier on the tuned look sensitivity. */
	UPROPERTY(Config)
	float LookSensitivity = 1.f;

	UPROPERTY(Config)
	bool bInvertLook = false;

	/** False until the player changes ray tracing; then bRayTracing overrides the project's setting. */
	UPROPERTY(Config)
	bool bRayTracingChosen = false;

	UPROPERTY(Config)
	bool bRayTracing = true;
};
