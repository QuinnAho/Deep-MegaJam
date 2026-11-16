// Every harpoon number. Project Settings > Game > DEEP Harpoon, saved to Config/DefaultGame.ini.
// Defaults are in code (the initialisers below). The harpoon is the way home and it costs you: one shot,
// limited range, a real fee. Keep it that way or the cut stops being frightening.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DeepHarpoonSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "DEEP Harpoon"))
class DEEP_API UDeepHarpoonSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDeepHarpoonSettings();

	static const UDeepHarpoonSettings& Get() { return *GetDefault<UDeepHarpoonSettings>(); }

	/** Shots per dive; reloaded when a dive starts (not on re-tether). */
	UPROPERTY(Config, EditAnywhere, Category = "Shot", meta = (ClampMin = "0"))
	int32 ShotsPerDive = 1;

	/** How far the bolt flies before it is a miss. */
	UPROPERTY(Config, EditAnywhere, Category = "Shot", meta = (ClampMin = "1"))
	float RangeM = 25.f;

	/** Bolt speed: quick, but a visible flight you can miss with. */
	UPROPERTY(Config, EditAnywhere, Category = "Shot", meta = (ClampMin = "1"))
	float BoltSpeedMps = 60.f;

	/** The shove back from a shot (opposite the aim), at full strength at once. About 3.5 m/s; a sprint is about 6. */
	UPROPERTY(Config, EditAnywhere, Category = "Recoil", meta = (ClampMin = "0"))
	float RecoilMps = 3.5f;

	/** The shove fades linearly to nothing over this time, so it carries the diver RecoilMps * RecoilSeconds / 2 (~0.9 m). */
	UPROPERTY(Config, EditAnywhere, Category = "Recoil", meta = (ClampMin = "0"))
	float RecoilSeconds = 0.5f;

	/** Latched to the submarine: reel speed up into the hatch. */
	UPROPERTY(Config, EditAnywhere, Category = "Reel", meta = (ClampMin = "0.1"))
	float SubReelSpeedMps = 10.f;

	/** Latched to anything else: pull speed to the latch point. */
	UPROPERTY(Config, EditAnywhere, Category = "Reel", meta = (ClampMin = "0.1"))
	float TerrainReelSpeedMps = 8.f;

	/** The terrain pull stops this far out from the surface it hit. */
	UPROPERTY(Config, EditAnywhere, Category = "Reel", meta = (ClampMin = "0"))
	float TerrainStandOffM = 1.2f;

	/** Within this distance of the reel target the line lets go. */
	UPROPERTY(Config, EditAnywhere, Category = "Reel", meta = (ClampMin = "0.05"))
	float ArriveM = 0.5f;

	/** A latch that has not arrived by now lets go (stuck behind the tether limit, or geometry). */
	UPROPERTY(Config, EditAnywhere, Category = "Reel", meta = (ClampMin = "0.5"))
	float MaxLatchSeconds = 10.f;

	/** Credits per submarine hit, added to the next settlement as one line per hit. */
	UPROPERTY(Config, EditAnywhere, Category = "Hull", meta = (ClampMin = "0"))
	int32 RepairFeePerHit = 40;

	UPROPERTY(Config, EditAnywhere, Category = "Hull")
	FString RepairFeeLabel = TEXT("Hull repair (you shot my submarine)");

	/** Noise units reported where the bolt hits the serpent (60 = the stalk threshold: loud). */
	UPROPERTY(Config, EditAnywhere, Category = "Serpent", meta = (ClampMin = "0"))
	float MonsterHitNoise = 60.f;

	/** Radius of the serpent's head for the bolt; body segments use their own size. */
	UPROPERTY(Config, EditAnywhere, Category = "Serpent", meta = (ClampMin = "0.1"))
	float MonsterHeadRadiusM = 1.2f;

	/** HUD message time after a latch, miss or refusal. */
	UPROPERTY(Config, EditAnywhere, Category = "Feedback", meta = (ClampMin = "0.1"))
	float MessageSeconds = 2.5f;

	/** The crosshair looks this many times the range ahead to say "out of range". */
	UPROPERTY(Config, EditAnywhere, Category = "Feedback", meta = (ClampMin = "1"))
	float AimLookAheadFactor = 4.f;

	UPROPERTY(Config, EditAnywhere, Category = "Look", meta = (ClampMin = "0.1"))
	float LineThicknessCm = 1.5f;

	UPROPERTY(Config, EditAnywhere, Category = "Look", meta = (ClampMin = "1"))
	float BoltLengthCm = 70.f;
};
