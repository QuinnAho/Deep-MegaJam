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

/** The serpent's look and motion. Values follow the Style Guide, section 6 ("chosen here" marks a value it did not give). */
USTRUCT(BlueprintType)
struct FDeepSerpentBodyTuning
{
	GENERATED_BODY()

	/** Head tip to tail tip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float LengthM = 11.f;

	/** Hit spheres along the body (the harpoon tests against these; the sphere fallback draws them). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body", meta = (ClampMin = "2", ClampMax = "64"))
	int32 Segments = 20;

	/** Gameplay size of the head (the lamp test uses it). The drawn head is sized by the Head settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float HeadRadiusM = 0.4f;

	/** Body radius at the front, where it meets the head (style guide: 0.9 m thick). Match a mesh head's neck here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float NeckRadiusM = 0.45f;

	/** Body radius at the tail tip (style guide: 0.1 m thick). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float TailRadiusM = 0.05f;

	/** Taper shape from neck to tail: 1 = straight cone, above 1 = stays thick longer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float TaperPower = 1.6f;

	/** The body swells to this times NeckRadiusM a little behind the head (chosen here). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float BellySwell = 1.12f;

	/** Sideways swing at the widest point of the weave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float WeaveAmplitudeM = 0.45f;

	/** Length of one full S of the weave along the body. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float WeaveWavelengthM = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float WeaveHz = 0.6f;

	// ---- The tube: one continuous body, rebuilt each frame along the head's path ----

	/** False = the old instanced spheres (fallback). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tube")
	bool bTubeBody = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tube", meta = (ClampMin = "4", ClampMax = "128"))
	int32 TubeRings = 48;

	/** Vertices per ring; a multiple of 4 keeps one vertex on the ridge line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tube", meta = (ClampMin = "8", ClampMax = "32"))
	int32 TubeSides = 12;

	/** Dorsal ridge: the top vertex raised by this fraction of the radius (plan: 30 percent). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tube")
	float RidgeHeight = 0.3f;

	/** Bank into each curve of the weave (plan: 10 degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tube")
	float WeaveRollDeg = 10.f;

	/** Bank of the head and neck per 90 deg/s of turning (chosen here). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tube")
	float TurnRollDeg = 14.f;

	// ---- The head: a socket at the front of the body; a code-built wedge with a jaw, or a mesh ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Head")
	float HeadLengthM = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Head")
	float HeadWidthM = 1.f;

	/** Skull height above the head centre at its highest (the jaw hangs below). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Head")
	float HeadHeightM = 0.36f;

	/** Head yaw sway with the weave, so the head never rides rigid (chosen here). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Head")
	float HeadSwayDeg = 5.f;

	/**
	 * Mesh head: a static mesh set here replaces the code-built head and jaw at the head socket (the head centre,
	 * X forward, Z up). Eyes, eyeshine and the rim stay on top. Empty = the code-built head.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Head", meta = (AllowedClasses = "/Script/Engine.StaticMesh"))
	FSoftObjectPath HeadMesh;

	/** Mesh head placement relative to the head socket, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Head")
	FVector HeadMeshOffsetCm = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Head")
	FRotator HeadMeshRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Head")
	FVector HeadMeshScale = FVector::OneVector;

	/** Material for the mesh head. Empty = the mesh's own materials. Set it to the body material to make one animal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Head", meta = (AllowedClasses = "/Script/Engine.MaterialInterface"))
	FSoftObjectPath HeadMeshMaterial;

	/** Eye centre from the head socket: forward, half the spacing, up (metres). Move to a mesh's eye sockets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Head")
	FVector EyeOffsetM = FVector(0.5f, 0.3f, 0.15f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Head")
	float EyeRadiusM = 0.045f;

	// ---- Skin (the body material slot) and the markings layer ----

	/**
	 * The body and code-built head material. Default: M_SerpentSkin (built by Content/Deep/Serpent/build_serpent_skin.py).
	 * A third-party material (a pack dragon's, graded through an instance in Content/Deep/Serpent) can go here; it
	 * reads UV0, which is tiled by the two settings below.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin", meta = (AllowedClasses = "/Script/Engine.MaterialInterface"))
	FSoftObjectPath SkinMaterial = FSoftObjectPath(TEXT("/Game/Deep/Serpent/M_SerpentSkin.M_SerpentSkin"));

	/** UV0 repeats per metre of body length, and around the girth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	float SkinTilesPerM = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	float SkinTilesAround = 2.f;

	/** The markings, an unlit additive layer drawn just over the body whatever it wears. Empty = no markings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin", meta = (AllowedClasses = "/Script/Engine.MaterialInterface"))
	FSoftObjectPath MarksMaterial = FSoftObjectPath(TEXT("/Game/Deep/Serpent/M_SerpentMarks.M_SerpentMarks"));

	/** Near-black (linear). Used directly when the skin material is missing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	FLinearColor SkinColor = FLinearColor(0.012f, 0.016f, 0.016f);

	/** Rim and spots: cyan-green #3FE0B0 (chosen here; the style guide names the hue only). Linear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	FLinearColor MarkColor = FLinearColor(0.05f, 0.75f, 0.43f);

	/** Spot brightness at full reveal and full threat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	float SpotGain = 6.f;

	/** Spot radius on the thickest part of the body, and the spacing along each row (metres). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	float SpotSizeM = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	float SpotSpacingM = 0.5f;

	/** Spot brightness kept at threat 0 (fraction of SpotGain). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	float SpotCalmFraction = 0.25f;

	/** Rim brightness at threat 0 and threat 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	float RimGainCalm = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	float RimGainThreat = 0.45f;

	/** Spot pulse rate at threat 0 and threat 1 (it speeds up as threat rises). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	float PulseHzCalm = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	float PulseHzThreat = 2.5f;

	/** Not shown in the first minute: the markings stay dark this long after it spawns, and until it first stalks (a sound). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	float RevealDelayS = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	float RevealFadeS = 4.f;

	// ---- Eyes: retro-reflective, they shine back only while a diver's lamp is on them ----

	/** Eyeshine colour (pale cyan-green: a tapetum tint of the white lamp). Linear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
	FLinearColor EyeColor = FLinearColor(0.55f, 1.f, 0.8f);

	/** Emissive multiplier at full shine. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
	float EyeShineEmissive = 20.f;

	/** How tightly an eye must face the lamp to shine (power on the facing cosine). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
	float EyeShineFacingPower = 2.f;

	/** Shine rise and fall rates (per second, exponential). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
	float EyeShineAttack = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
	float EyeShineRelease = 8.f;

	/** Eye point light intensity at full shine (unitless, like the sub's airlock light) and reach. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
	float EyeLightIntensity = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
	float EyeLightRadiusM = 1.2f;

	// ---- Motion cues ----

	/** Recoil from the lamp: the flinch lasts this long (chosen here). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float RecoilFlinchS = 0.9f;

	/** Head thrown up and back at the flinch's peak. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float RecoilPitchDeg = 28.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float RecoilPullM = 0.5f;

	/** Weave amplitude and rate multiplier at the flinch's peak (the body convulses). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float RecoilWeaveBoost = 2.5f;

	/** The bite: the head strikes forward this far in LungeS, then thrashes while it holds the line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float LungeM = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float LungeS = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float BiteThrashDeg = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float BiteThrashHz = 4.5f;

	/** Jaw gape: wide on the strike and the hiss, a little open while hunting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float JawOpenDeg = 42.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float JawHuntDeg = 14.f;
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

	/** The lamp drove it off (held on its head, or any beam during a bite). Not set by a sever or a catch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	bool bRepelled = false;

	/** Recoil reached the lurk radius: start the cooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	bool bStartCooldown = false;

	bool Changed(EDeepMonsterState From) const { return Next != From; }
};
