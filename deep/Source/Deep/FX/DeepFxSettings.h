// Water effects tuning. Project Settings > Game > DEEP FX, saved to Config/DefaultGame.ini; defaults are here.
// The assets are built by Content/Deep/FX/build_fx_materials.py and build_fx_systems.ps1.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DeepFxSettings.generated.h"

class UNiagaraSystem;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "DEEP FX"))
class DEEP_API UDeepFxSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDeepFxSettings();

	static const UDeepFxSettings& Get() { return *GetDefault<UDeepFxSettings>(); }

	/** Off: no bubbles or silt at all (frame-time A/B; `Deep.Fx 0`). */
	UPROPERTY(Config, EditAnywhere, Category = "General")
	bool bEnabled = true;

	UPROPERTY(Config, EditAnywhere, Category = "General")
	TSoftObjectPtr<UNiagaraSystem> BubbleSystem;

	UPROPERTY(Config, EditAnywhere, Category = "General")
	TSoftObjectPtr<UNiagaraSystem> SiltSystem;

	/** Breath cycle, calm and fast (DeepSynth: breath 4 s, breath_fast 2 s). */
	UPROPERTY(Config, EditAnywhere, Category = "Breath", meta = (ClampMin = "0.5"))
	float CalmBreathPeriodS = 4.f;

	UPROPERTY(Config, EditAnywhere, Category = "Breath", meta = (ClampMin = "0.5"))
	float FastBreathPeriodS = 2.f;

	/** Where in the cycle each burst leaves the regulator. DeepSynth's breath out (the gurgle) runs 0.45 to 0.9. */
	UPROPERTY(Config, EditAnywhere, Category = "Breath")
	TArray<float> ExhalePuffPhases;

	/** Bubbles in the first burst of a breath out; each later burst has PuffFalloff fewer, at least MinPuffCount. */
	UPROPERTY(Config, EditAnywhere, Category = "Breath", meta = (ClampMin = "1"))
	int32 FirstPuffCount = 9;

	UPROPERTY(Config, EditAnywhere, Category = "Breath", meta = (ClampMin = "0"))
	int32 PuffFalloff = 2;

	UPROPERTY(Config, EditAnywhere, Category = "Breath", meta = (ClampMin = "1"))
	int32 MinPuffCount = 3;

	/** The regulator's exhaust, from the eye: forward, right (both sides alternate), up, in cm. A little ahead of the
	 *  lamp (held 20 cm forward), so the first rise of a breath crosses the beam's spill instead of starting behind it. */
	UPROPERTY(Config, EditAnywhere, Category = "Breath")
	FVector MouthOffsetCm = FVector(26.f, 9.f, -16.f);

	/** Breath bubbles' size against NS_Bubbles' own 0.6 to 2.4 cm (so close to the eye they read big). */
	UPROPERTY(Config, EditAnywhere, Category = "Breath")
	float BreathSizeScale = 0.7f;

	/** Fast breath: bubbles a little smaller (shallow breaths). */
	UPROPERTY(Config, EditAnywhere, Category = "Breath")
	float FastSizeScale = 0.8f;

	/** Dropping through the hatch: air carried down with the diver, bursts this often for this long. */
	UPROPERTY(Config, EditAnywhere, Category = "Hatch")
	float EntryStreamS = 1.4f;

	UPROPERTY(Config, EditAnywhere, Category = "Hatch")
	float EntryIntervalS = 0.07f;

	UPROPERTY(Config, EditAnywhere, Category = "Hatch")
	int32 EntryCount = 6;

	UPROPERTY(Config, EditAnywhere, Category = "Hatch")
	float EntrySizeScale = 1.5f;

	/** While the winch runs: a churn under the hatch, round the line, this far below the deck. */
	UPROPERTY(Config, EditAnywhere, Category = "Hatch")
	float ChurnBelowHatchCm = 220.f;

	UPROPERTY(Config, EditAnywhere, Category = "Hatch")
	float ChurnRadiusCm = 45.f;

	UPROPERTY(Config, EditAnywhere, Category = "Hatch")
	float ChurnIntervalS = 0.09f;

	UPROPERTY(Config, EditAnywhere, Category = "Hatch")
	int32 ChurnCount = 4;

	UPROPERTY(Config, EditAnywhere, Category = "Hatch")
	float ChurnSizeScale = 1.3f;

	/** Silt: a puff where salvage leaves the bed, and where the diver touches it. */
	UPROPERTY(Config, EditAnywhere, Category = "Silt")
	int32 GrabSiltCount = 14;

	UPROPERTY(Config, EditAnywhere, Category = "Silt")
	int32 TouchSiltCount = 8;

	/** The diver kicks up silt within this height of the bed (capsule bottom), moving at least this fast. */
	UPROPERTY(Config, EditAnywhere, Category = "Silt")
	float TouchHeightCm = 35.f;

	UPROPERTY(Config, EditAnywhere, Category = "Silt")
	float TouchMinSpeedCmS = 60.f;

	UPROPERTY(Config, EditAnywhere, Category = "Silt")
	float TouchCooldownS = 1.2f;

	/** How far below the diver to look for the bed. */
	UPROPERTY(Config, EditAnywhere, Category = "Silt")
	float BedProbeCm = 400.f;

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
