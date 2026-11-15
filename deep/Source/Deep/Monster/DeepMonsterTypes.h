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
	Hunt,
	/**
	 * The first sight, once per run before its first hunt: after a sound, one slow pass across the diver's view at
	 * the edge of the lamp's reach (part of the body, never the whole animal), then it leaves. Ignores noise.
	 */
	Pass
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

	// ---- The bite, staged: it takes the line a few metres from the diver, striking across it from the side ----

	/** The bite point: this far up the tether from the diver (so a diver who turns sees it a few metres away). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contact")
	float BiteFromDiverM = 4.f;

	/** Final approach: it first swims to a point this far out to the side of the line, then strikes across it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contact")
	float BiteSideLeadM = 3.f;

	/** Recoil: for this long it turns its head away from the lamp nearly in place (in frame), then it flees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contact")
	float RecoilTurnS = 0.9f;

	/** Speed during that turn, as a fraction of RecoilSpeedMps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contact")
	float RecoilTurnSpeedFraction = 0.15f;

	// ---- The first sight (state Pass): once per run, before the first hunt ----

	/** Off = no first sight; it hunts from the first noise as before. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First sight")
	bool bFirstSight = true;

	/** The pass starts this long after it begins lurking, or sooner if noise would bring it in or the diver loses the tether. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First sight")
	float FirstSightAfterS = 20.f;

	/** Distance in front of the lamp of the pass line (the edge of the lamp's reach in this water: 6 to 8 m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First sight")
	float PassDistanceM = 7.f;

	/** The head starts this far to the side of the view (out of view and out of the beam): the sound comes first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First sight")
	float PassSideStartM = 18.f;

	/** The line runs this far above the lamp (negative: below; the beam leaves the helmet angled down), so the body crosses the bright part of the beam. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First sight")
	float PassUpM = -0.6f;

	/** It turns away from the diver this far past the middle of the view (at the beam's axis)... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First sight")
	float PassTurnAtM = 0.f;

	/** ...toward a point this much farther out and this far to the side: the head leaves the beam sideways into the dark (not down the beam, where it would stay lit), and the body drags through the beam after it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First sight")
	float PassLeaveM = 9.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First sight")
	float PassLeaveSideM = 12.f;

	/** Slow: the body takes several seconds to slide through the beam. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First sight")
	float PassSpeedMps = 3.f;

	/** The pass is over once the head is this far from the diver after the last turn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First sight")
	float PassEndM = 24.f;

	/**
	 * In the pass the beam must stay on its head this long to drive it off (a head crossing the beam is not enough:
	 * it is passing, not coming for you, and the first sight should play out).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First sight")
	float PassLitToRecoilS = 5.f;

	/** The pass gives up after this long whatever happens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "First sight")
	float PassMaxS = 25.f;
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

/**
 * A static mesh worn as the serpent's head, placed from landmarks measured on the mesh (mesh-local cm, its own axes):
 * the skull point goes to the head socket (so the lamp, the bite and DEEPSTATE use the visible head), the head looks
 * along Forward, the eyes are level (their line gives the head's right), and the mesh is clipped by its material to
 * what lies in front of the neck cut and inside a capsule from the cut to KeepEnd. The body tube starts at the cut,
 * as a sleeve that grows from inside the neck to just over it, so the cut is always inside the body.
 * Defaults: SM_DragonPose04 (Dragon Cave), measured from an OBJ export of its fallback mesh (devlog Nov 15, "dragon head").
 */
USTRUCT(BlueprintType)
struct FDeepSerpentMeshHead
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head", meta = (AllowedClasses = "/Script/Engine.StaticMesh"))
	FSoftObjectPath Mesh = FSoftObjectPath(TEXT("/Game/DragonCave/Meshes/SM_DragonPose04.SM_DragonPose04"));

	/** Worn on every slot. It must have the clip parameters (CutOrigin, CutNormal, KeepEnd, KeepRadius) to clip. Empty = the mesh's own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head", meta = (AllowedClasses = "/Script/Engine.MaterialInterface"))
	FSoftObjectPath Material = FSoftObjectPath(TEXT("/Game/Deep/Serpent/M_DragonHead.M_DragonHead"));

	/** Mesh to world scale (the dragon is about 250 m across). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	float Scale = 0.075f;

	/** The point that becomes the serpent's head (the socket): between and a little under the eyes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	FVector SkullCm = FVector(250.f, 6500.f, -850.f);

	/** Where the head looks (mesh-local direction). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	FVector Forward = FVector(0.22f, 0.95f, -0.25f);

	/** The eyes (from the emissive mask's two spots); the eyeshine sits here and their line levels the head. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	FVector EyeLeftCm = FVector(790.f, 6294.f, -420.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	FVector EyeRightCm = FVector(-290.f, 6344.f, -1140.f);

	/** Centre of the neck where it is cut, and the neck's direction there (toward the head). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	FVector CutCm = FVector(270.f, 2400.f, -700.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	FVector CutNormal = FVector(0.21f, 0.96f, -0.21f);

	/** The far end of the kept capsule (past the snout) and its radius: wings and legs outside it are never drawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	FVector KeepEndCm = FVector(650.f, 8300.f, -1300.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	float KeepRadiusCm = 1700.f;

	/**
	 * The body's sleeve over the neck: it starts SleeveM in front of the cut at SleeveStartScale of the body's radius
	 * (inside the neck) and reaches SleeveEndScale at the cut (just over it), easing back to 1 over NeckEaseM.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	float SleeveM = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	float SleeveStartScale = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	float SleeveEndScale = 1.04f;

	/**
	 * The body is this much taller than wide at the cut, easing to round over NeckEaseM. The dragon's neck at the
	 * cut is 1,460 x 2,400 mesh cm (wide x tall, in the head's frame), so 1.6.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	float NeckTall = 1.5f;

	/** And this much narrower: the neck's flanks are flat (600 cm half-width against a 1,200 cm half-height). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	float NeckWide = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	float NeckEaseM = 2.5f;

	/**
	 * The bite (pass two): the jaw cannot close, so the head holds the line in its open mouth. This point, in the
	 * head's own frame relative to the skull (forward, right, up; mesh cm), between the tooth rows, is kept on the
	 * line while the whole head worries it: a slow shake about the line (pitch) and a roll.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	FVector MouthCm = FVector(750.f, 0.f, -560.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	float BiteShakeDeg = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	float BiteShakeHz = 0.8f;

	/** The body's radius behind the neck, as a fraction of the code-built body's: thinner, so the neck runs into it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	float BodyRadiusScale = 0.88f;

	/** Base colour gain of the head material (near-black like the body), set on its instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	float ColorGain = 0.06f;

	/**
	 * Dorsal spikes carried on from the dragon's neck down the body's ridge, so head and body share one outline:
	 * one every SpikeSpacingM over the first SpikeRunM, SpikeLengthM long at the neck, shrinking to a fifth.
	 * 0 spacing = none.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	float SpikeSpacingM = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	float SpikeRunM = 9.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	float SpikeLengthM = 0.3f;

	/**
	 * The body's skin with this head: the dragon's own neck skin mirror-tiled along the tube (M_DragonBody), graded
	 * like the head, so head and body are one skin. Missing = the normal body skin with BodyScaleSizeM.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head", meta = (AllowedClasses = "/Script/Engine.MaterialInterface"))
	FSoftObjectPath BodyMaterial = FSoftObjectPath(TEXT("/Game/Deep/Serpent/M_DragonBody.M_DragonBody"));

	/** The patch of the mesh's UVs (UDIM units) the body wears, and how many metres of body one patch covers (along, around). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	FVector2D PatchOrigin = FVector2D(4.56f, 1.55f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	FVector2D PatchSize = FVector2D(0.12f, 0.2f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	FVector2D PatchMetres = FVector2D(0.92f, 1.53f);

	/** Body scale size with this head when BodyMaterial is missing (M_SerpentSkin ScaleSize). 0 = leave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	float BodyScaleSizeM = 0.12f;

	/** The body's swell behind the neck with this head (BellySwell for the code-built head): the dragon's neck already thickens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh head")
	float BodySwell = 1.f;
};

/** The serpent's look and motion. Values follow the Style Guide, section 6 ("chosen here" marks a value it did not give). */
USTRUCT(BlueprintType)
struct FDeepSerpentBodyTuning
{
	GENERATED_BODY()

	/** Head tip to tail tip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float LengthM = 16.f;

	/** Hit spheres along the body (the harpoon tests against these; the sphere fallback draws them). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body", meta = (ClampMin = "2", ClampMax = "64"))
	int32 Segments = 20;

	/** Gameplay size of the head (the lamp test uses it). The drawn head is sized by the Head settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float HeadRadiusM = 0.4f;

	/** Body radius at the front, where it meets the head. Style guide: 0.9 m thick; second pass: 1.1 m, swelling to 1.4 m, 16 m long, so it is many times the diver. Match a mesh head's neck here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float NeckRadiusM = 0.55f;

	/** Body radius at the tail tip (style guide: 0.1 m thick). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float TailRadiusM = 0.06f;

	/** Taper shape from neck to tail: 1 = straight cone, above 1 = stays thick longer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float TaperPower = 1.8f;

	/** The body swells to this times NeckRadiusM a little behind the head (chosen here). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float BellySwell = 1.25f;

	/** Sideways swing at the widest point of the weave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float WeaveAmplitudeM = 0.6f;

	/** Length of one full S of the weave along the body. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float WeaveWavelengthM = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Body")
	float WeaveHz = 0.4f;

	// ---- The tube: one continuous body, rebuilt each frame along the head's path ----

	/** False = the old instanced spheres (fallback). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tube")
	bool bTubeBody = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tube", meta = (ClampMin = "4", ClampMax = "128"))
	int32 TubeRings = 64;

	/** Vertices per ring; a multiple of 4 keeps one vertex on the ridge line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tube", meta = (ClampMin = "8", ClampMax = "32"))
	int32 TubeSides = 16;

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
	float HeadLengthM = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Head")
	float HeadWidthM = 0.95f;

	/** Skull height above the head centre at its highest (the jaw hangs below). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Head")
	float HeadHeightM = 0.22f;

	/** Head yaw sway with the weave, so the head never rides rigid (chosen here). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Head")
	float HeadSwayDeg = 5.f;

	/**
	 * Mesh head: the Dragon Cave dragon (a whole winged animal, pose 4: mouth open, teeth showing), clipped to its head
	 * and neck by M_DragonHead and set in the head socket from the landmarks below. If the mesh does not load (pack
	 * not installed) or `Deep.Monster.MeshHead 0`, the code-built head is used. Empty Mesh = the code-built head.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Head")
	FDeepSerpentMeshHead MeshHead;

	/** Eye centre from the head socket: forward, half the spacing, up (metres). Move to a mesh's eye sockets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Head")
	FVector EyeOffsetM = FVector(0.5f, 0.32f, 0.08f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Head")
	float EyeRadiusM = 0.028f;

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
	FLinearColor SkinColor = FLinearColor(0.008f, 0.01f, 0.01f);

	/** Spots (and the rim, if it is ever turned back on): cyan-green #3FE0B0 (chosen here; the style guide names the hue only). Linear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	FLinearColor MarkColor = FLinearColor(0.05f, 0.75f, 0.43f);

	/** Spot brightness at full reveal and full threat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	float SpotGain = 2.5f;

	/** Spot radius on the thickest part of the body, and the spacing along each row (metres). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	float SpotSizeM = 0.035f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	float SpotSpacingM = 0.9f;

	/** Spot brightness kept at threat 0 (fraction of SpotGain). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	float SpotCalmFraction = 0.f;

	/** Rim brightness at threat 0 and threat 1. Zero: a glowing outline made it a cartoon eel (second pass). Seen by lamp highlights and silhouette only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	float RimGainCalm = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	float RimGainThreat = 0.f;

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
	float EyeShineEmissive = 60.f;

	/** How tightly an eye must face the lamp to shine (power on the facing cosine). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
	float EyeShineFacingPower = 3.f;

	/** Shine rise and fall rates (per second, exponential). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
	float EyeShineAttack = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
	float EyeShineRelease = 8.f;

	/** Eye point light intensity at full shine (unitless, like the sub's airlock light) and reach. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
	float EyeLightIntensity = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eyes")
	float EyeLightRadiusM = 0.6f;

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
	float JawOpenDeg = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float JawHuntDeg = 14.f;

	/** Jaw gape while it holds the line after the strike, so the teeth stay visible in the thrash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float BiteHoldGapeDeg = 20.f;

	/** The drawn body keeps its centre line at least this far from a diver's eyes (look only; hit segments are not moved). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	float BodyClearM = 2.f;

	/** Spots follow threat to this power: near zero until it peaks (1 = linear). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skin")
	float SpotThreatPower = 2.f;

	/** Teeth on the code-built head and jaw, per side, and their colour (dull, yellowed bone, under the value cap). Linear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Head")
	int32 TeethPerSide = 9;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Head")
	FLinearColor TeethColor = FLinearColor(0.16f, 0.15f, 0.12f);
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

	/** The first sight has not happened yet this run and there is a player to stage it for. False = the old rules exactly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	bool bFirstSightPending = false;

	/** Pass: the head has made its last turn and is out in the dark. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	bool bPassComplete = false;
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

	/** Lurk -> Pass: the first sight begins (fire the first sound, lay out the pass). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	bool bPassStarted = false;

	/** Recoil reached the lurk radius: start the cooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster")
	bool bStartCooldown = false;

	bool Changed(EDeepMonsterState From) const { return Next != From; }
};
