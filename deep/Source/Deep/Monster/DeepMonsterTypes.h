// The sea serpent's tuning structs, state enum and rule inputs/results. Numbers live in UDeepMonsterSettings.

#pragma once

#include "CoreMinimal.h"
#include "DeepMonsterTypes.generated.h"

namespace DeepMonster
{
	inline constexpr float CmPerM = 100.f;
}

/** What the serpent is doing. Each state has one readable cause the player can learn. */
UENUM(BlueprintType)
enum class EDeepMonsterState : uint8
{
	/** Circles in the dark beyond lamp range. Only eyes and sound give it away. */
	Lurk,
	/** Noise passed the threshold: spirals in on the middle of the tether. */
	Stalk,
	/** The lamp drove it off: flees to the lurk radius, then a cooldown. */
	Recoil,
	/** Holding the tether: a fixed alarm, then the line is severed unless the beam hits it. */
	Bite,
	/** The diver has no tether: chases the diver directly. */
	Hunt
};

/** Every serpent behaviour number. Per-stop overrides in FDeepMonsterStopTuning are applied on top. */
USTRUCT(BlueprintType)
struct FDeepMonsterTuning
{
	GENERATED_BODY()

	// ---- Noise ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float NoiseMax = 100.f;

	/** Noise lost per second, always. Silence always calms it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float NoiseDecayPerS = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float WinchNoisePerS = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float SprintNoisePerS = 15.f;

	/** Flat noise for any rise in cargo (a grab). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float GrabNoise = 30.f;

	/** Extra grab noise per kg picked up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float GrabNoisePerKg = 1.f;

	/** Lurk -> Stalk at or above this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float StalkNoise = 60.f;

	/** Stalk -> Lurk below this (lower than StalkNoise so it does not flicker). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float CalmNoise = 25.f;

	/**
	 * Engine hearing sense range for a noise of loudness 1. Noise events carry loudness = noise units (at least 1),
	 * and the engine scales range by loudness, so keep this larger than the arena.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float HearingRangeM = 150.f;

	/** Noise kept (fraction) when it recoils, so it does not come straight back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Noise")
	float NoiseKeptOnRecoil = 0.5f;

	// ---- Lamp ----

	/** Seconds the beam must stay on its head to drive it off (Bite breaks at once). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float LitToRecoilS = 0.75f;

	/** Lit seconds lost per second out of the beam. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float LitDecayPerS = 1.5f;

	/** The head counts as lit if the beam passes within this of its centre (aiming leniency). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lamp")
	float BeamLeniencyM = 0.8f;

	// ---- Movement ----

	/** Lurk circle: this far beyond the tether (keep above the lamp range, 35 m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float LurkRadiusM = 45.f;

	/** How far ahead on the circle it aims while lurking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float LurkLeadDeg = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float LurkSpeedMps = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float StalkSpeedMps = 5.f;

	/** Stalk aims this many degrees around the target, so it closes in an arc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float StalkArcDeg = 35.f;

	/** Each stalk aim point is this fraction of the current distance from the target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float StalkInwardFraction = 0.75f;

	/** Inside this distance it stops arcing and goes straight for the tether. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float StalkDirectWithinM = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float RecoilSpeedMps = 10.f;

	/** Recoil gives up fleeing after this long even if it has not reached the lurk radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float RecoilMaxS = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float HuntSpeedMps = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float TurnRateDegPerS = 90.f;

	// ---- Contact ----

	/** After a recoil, seconds before noise can bring it in again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contact")
	float RecoilCooldownS = 8.f;

	/** Head this close to the tether line = bite. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contact")
	float BiteReachM = 2.f;

	/** The bite alarm: seconds the player has to hit it with the beam. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contact")
	float BiteAlarmS = 5.f;

	/** Head this close to an untethered diver = caught. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contact")
	float CatchReachM = 1.5f;

	/** Threat is 0 at or beyond this distance from the diver, 1 at CatchReachM. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contact")
	float ThreatFarM = 70.f;
};

/** Stop-specific overrides: the stop with the greatest Stop not above the current one applies. */
USTRUCT(BlueprintType)
struct FDeepMonsterStopTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stop")
	int32 Stop = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stop")
	float RecoilCooldownS = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stop")
	float StalkSpeedMps = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stop")
	float HuntSpeedMps = 8.f;
};

/** The body built from basic shapes. */
USTRUCT(BlueprintType)
struct FDeepSerpentBodyTuning
{
	GENERATED_BODY()

	/** Head tip to tail tip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float LengthM = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body", meta = (ClampMin = "2", ClampMax = "64"))
	int32 Segments = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float HeadRadiusM = 0.4f;

	/** Radius of the first segment behind the head. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float NeckRadiusM = 0.33f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float TailRadiusM = 0.06f;

	/** Sideways swing at the widest point of the weave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float WeaveAmplitudeM = 0.35f;

	/** Length of one full S of the weave along the body. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float WeaveWavelengthM = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float WeaveHz = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	FLinearColor SkinColor = FLinearColor(0.05f, 0.09f, 0.08f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	FLinearColor EyeColor = FLinearColor(0.9f, 1.f, 0.35f);

	/** Eye point light intensity (unitless, like the sub's airlock light) and reach. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float EyeLightIntensity = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float EyeLightRadiusM = 2.5f;
};

/** Everything the state rule needs for one step. Distances in metres. */
USTRUCT(BlueprintType)
struct FDeepMonsterInputs
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	EDeepMonsterState State = EDeepMonsterState::Lurk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	float Noise = 0.f;

	/** Accumulated seconds the beam has been on its head (see UDeepMonsterRules::LitStep). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	float LitSeconds = 0.f;

	/** The beam is on its head this frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	bool bLitNow = false;

	/** Seconds spent in the current state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	float StateSeconds = 0.f;

	/** Recoil cooldown left; noise cannot bring it in while above zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	float CooldownS = 0.f;

	/** Head to the tether line. With no tether, head to the diver. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	float TetherDistanceM = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	float DiverDistanceM = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	bool bDiverTethered = true;

	/** False when there is no diver at all: it just keeps doing what it does. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	bool bHasDiver = true;
};

/** The next state, plus which one-off events this step caused. */
USTRUCT(BlueprintType)
struct FDeepMonsterStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	EDeepMonsterState Next = EDeepMonsterState::Lurk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	bool bBiteStarted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	bool bBiteBroken = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	bool bSevered = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	bool bCaught = false;

	/** Recoil reached the lurk radius: start the cooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	bool bStartCooldown = false;

	bool Changed(EDeepMonsterState From) const { return Next != From; }
};
