// The Director's submarine: the fixed point of the game (D-022). It never moves; driving moves the arena root.
// Its box hull is what the water sees: a deck with an open hatch above the tether anchor, the window, the outside
// dress and lights, and a helm camera under the hull. The room divers walk in is a fixed set (World/DeepSubRoom).
// Placed in a map by art later, or spawned by DeepArena::EnsureArena when the map has none.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sim/DeepMovingSubRules.h"
#include "DeepSubmarine.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UInstancedStaticMeshComponent;
class ULocalLightComponent;
class USpotLightComponent;
class UCameraComponent;
class URectLightComponent;
class UBoxComponent;
class UPostProcessComponent;
class UDeepHullModel;
namespace DeepHull { enum class ELamp : uint8; }

UCLASS()
class DEEP_API ADeepSubmarine : public AActor
{
	GENERATED_BODY()

public:
	/** Water look: the outside lights (airlock light, belly flood) at a fraction of full, and how much they light the fog. */
	void SetOutsideLights(float Scale, float Fog);

	ADeepSubmarine();

	/** World position the tether is tied to: just under the hatch. */
	UFUNCTION(BlueprintPure, Category = "Deep")
	FVector GetAnchorLocation() const;

	/** Where a diver appears in the water (tests and the old airlock start): under the hatch, facing along the sub. */
	UFUNCTION(BlueprintPure, Category = "Deep")
	FTransform GetDiverStartTransform() const;

	/** World Z of the deck's top. */
	double GetDeckTopZ() const;

	/** Centre of the hatch opening on the hull deck's top. */
	FVector GetHatchLocation() const;

	/** In the hatch opening (any height). */
	bool IsInHatchColumn(const FVector& WorldCm) const;

	/**
	 * The box (sub-local cm) round every shape that has collision: each is a visible engine cube, so this is the
	 * visible solid hull and nothing more. bHullOnly: the box hull (deck, walls, ceiling) without the tower on top.
	 * With the model hull (Deep.Sub.Model 1) it is the box round the model's simple collision, for both values: the
	 * model is one solid with its tower.
	 */
	FBox GetSolidBoundsLocal(bool bHullOnly) const;

	/**
	 * A swimmer's velocity near the hull (world cm and m/s in, world m/s out): pushing into a side slides down it and
	 * under (UDeepRules). The old box hull uses its collision box; the model hull sweeps the capsule ContactCm along the
	 * velocity against the model's own collision, so the slide happens only at its real surface.
	 */
	FVector SlideAlongHull(const FVector& WorldCm, const FVector& VelocityMps, float RadiusCm, float HalfHeightCm, float ContactCm) const;

	/** BT-01 is shown and is the hull's solid shape (Deep.Sub.Model 1, applied). */
	bool IsModelHull() const;

	UDeepHullModel* GetHullModel() const { return HullModel; }

	/**
	 * Deep.Sub.Model switched (UDeepHullModel::ApplySwitch): box colliders off with the model, on without; the square
	 * hatch cover and water hidden with the model (the diver still leaves and comes home through the hatch column);
	 * the lamps and their bulbs onto the model's surface, or back to the box.
	 */
	void OnHullModelSwitched();

	/** An outside lamp's light (bow, hull, marker, airlock, belly flood). */
	class ULocalLightComponent* GetLamp(DeepHull::ELamp Lamp) const;

	/** The bulb on a lamp: a small glowing sphere at the light, shown with the model hull. */
	UStaticMeshComponent* GetBulb(DeepHull::ELamp Lamp) const;

	/** Locked: a red cover closes the hatch (collision on). Open while parked at a stop. */
	void SetHatchOpen(bool bOpen);
	bool IsHatchOpen() const { return bHatchOpen; }

	/** Where the hull stood when the arena was built (server, once). Deep.World.Still moves the hull from it; D-022 never does. */
	void SetHome(const FTransform& Home);

	/** The home transform, or the hull's own until one is set. */
	FTransform GetHomeTransform() const;

	/** The home was set (the server when the arena is built; replicated to clients). */
	bool HasHome() const { return bHasHome; }

	/**
	 * Deep.World.Still: puts the hull where the game state's depth, offset and yaw say (UDeepMovingSubRules::StillHull).
	 * Every machine does it from the replicated state; the hull's movement is not replicated. No-op with the flag off.
	 */
	void PlaceStill();

	UCameraComponent* GetHelmCamera() const { return HelmCamera; }

	/** Vertical speed of the sub (m/s, + = down), from the replicated depth, smoothed. 0 when parked. */
	float GetDriveSpeedMps() const { return DriveSpeedMps; }

	/** Someone is seated at the helm: the floods are on and the trench wall shows. */
	bool IsHelmManned() const { return bHelmManned; }

	/** Sway amplitude as a fraction of full: |speed| / drive speed, clamped to 0..1. */
	static float HelmSwayScale(float SpeedMps, float DriveSpeedMps);

	/** Streak length (m) of a speck seen from the helm at this speed: |speed| x shutter seconds. */
	static float HelmStreakM(float SpeedMps, float ShutterSeconds);

	/** The trench wall's distance out from its base plane (m) at a point (along the wall, absolute depth): rock relief plus a ledge every LedgeEveryM. */
	static float TrenchReliefM(float AlongM, float DepthM, float LedgeEveryM, float LedgeOutM);

	/** The trench wall (its frame: -X out of the face toward the sub, Y along the wall, Z = -absolute depth). Dressing rows with parent "trench" hang from it. */
	class UProceduralMeshComponent* GetTrench() const;

	/** The outside dress (DeepHull::Parts): frame, foam, tanks, skids, thrusters, hatch well. Visual only, no collision. */
	const TArray<TObjectPtr<UStaticMeshComponent>>& GetHullDress() const { return HullDress; }

	/** Shows or hides the outside dress and the lamp housings (Deep.Hull, frame-time A/B). */
	void SetHullDressVisible(bool bVisible);

	/** The trench shows to a local viewer: through the helm camera, or from inside the room while the sub is not parked (the window). */
	static bool TrenchShows(bool bHelmView, bool bInRoomView, bool bParked);

	// ---- D-070: what the pilot sees (Sim/DeepHelmViewRules). Local to each machine: only the seated pilot views it. ----
	/** V: the chase view (behind and above, on the hull's trail) or the cockpit (level at the bow). */
	void ToggleHelmView();
	bool IsHelmCockpit() const { return bHelmCockpit; }
	void SetHelmCockpit(bool bCockpit);
	/** The mouse: looks round the view (degrees, + right, + up), within the view's limits; eases back after a moment. */
	void AddHelmLook(float YawDeg, float PitchDeg);
	/** The look's offset from straight ahead (degrees, X + starboard, Y + up): Deep.Pilot.MouseSteer steers by it. */
	FVector2D GetHelmLookOffsetDeg() const { return HelmLookDeg; }
	/**
	 * One step of the pilot's view: the trail, the chase spot (the trail, else straight astern), the probe from over the
	 * hull to it (never in rock, never in the hull), the look ahead along the travel, the mouse's look. Tick calls it
	 * while the helm is manned; tests call it directly. Jolt/sway/shiver are the helm's camera motions in degrees.
	 */
	void StepHelmView(float DeltaSeconds, float PitchShakeDeg = 0.f, float YawShakeDeg = 0.f, float RollShakeDeg = 0.f, float DropCm = 0.f);
	/** Forgets the trail and snaps the chase view on the next step (a jump: a placement, a stop's arena loading). */
	void ResetHelmView();
	/** The hull's centre (its solid box's centre) in the world, cm. */
	FVector GetHullCentreWorld() const;
	/** The world spot the chase view's probe starts from (over the hull, out of rock when it can be). */
	FVector GetChaseAnchorWorld() const { return ChaseAnchorCm; }
	/** D-070 idle: the visual hull's parent (it bobs; the colliding box does not), the amount (0..1) and the bob now. */
	USceneComponent* GetHullVisual() const { return HullVisual; }
	float GetIdleAmount() const { return IdleAmount; }
	const FDeepIdleBob& GetIdleBob() const { return IdleNow; }

protected:
	/** Helm look each tick: floods, trench wall, sway scaled by speed, the settle on parking. */
	void TickHelmLook(float DeltaSeconds);

	/** Builds the trench wall mesh once (sub-local, absolute depth along -Z). */
	void BuildTrench();

	float OutsideFog = 0.f;
	float DriveSpeedMps = 0.f;
	float LastDepthM = -1.f;
	bool bHelmManned = false;
	bool bWasParked = true;
	float SettleAge = 100.f;
	/** The cave shaft: a knock on the rock jolts the helm camera (from the replicated hull dents, so every machine). */
	int32 LastHullDamageSeen = -1;
	float JoltDeg = 0.f;
	float JoltAge = 0.f;
	/** The last knock's impact (m/s, from the dents): a hard one makes the floods stutter (D-070). */
	float KnockMps = 0.f;
	/** Each helm flood's share of full for the rock in front of it (D-070: no white-out up close). */
	float FloodNear[2] = { 1.f, 1.f };
	float FillNear = 1.f;

	/** D-070's pilot view (local): cockpit or chase, the mouse's look and how long since it moved, the hull's trail. */
	bool bHelmCockpit = false;
	bool bHelmViewInit = false;
	FVector2D HelmLookDeg = FVector2D::ZeroVector;
	float HelmLookIdleS = 100.f;
	float LastHelmYawDeg = 0.f;
	/** The hull's centre, newest last, in the arena root's frame (the shaft moves the root, the caves the hull). */
	TArray<FVector> HelmTrail;
	FVector LastHullLocalCm = FVector::ZeroVector;
	FVector TravelLocalMps = FVector::ZeroVector;
	FVector LookDirLocal = FVector::ForwardVector;
	FVector ChaseSpotCm = FVector::ZeroVector;
	double ChaseUpCm = 0.0;
	/** The bow lights' own spots, kept while the helm floods stand clear ahead of the bow (D-070). */
	FVector BowLampHomeCm[2] = { FVector::ZeroVector, FVector::ZeroVector };
	FVector ChaseAnchorCm = FVector::ZeroVector;
	/** The world frame the trail is kept in (the arena root's, or identity). */
	FTransform HelmTrailFrame() const;
	/** The chase view's probe: a sphere swept from Anchor toward Want against world rock (never the hull); where it stops. */
	FVector ProbeChase(const FVector& AnchorCm, const FVector& WantCm) const;
	/** True when a probe-sized sphere at Cm overlaps world rock. */
	bool ProbeBlocked(const FVector& Cm) const;

	/** A cliff of rock ahead of the helm, slid up as the sub descends; visible only while the helm is manned. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<class UProceduralMeshComponent> Trench;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Room and hull surfaces, lights and screens (FDeepLookTuning, Look|Room). */
	void ApplyLook();

	float OutsideScale = 1.f;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Places and sizes every shape from the tuning. */
	void LayOut();

	FVector Local(double X, double Y, double Z) const;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<USceneComponent> Root;

	/** The deck is four slabs around the hatch. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UStaticMeshComponent>> Deck;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UStaticMeshComponent>> Walls;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> Ceiling;

	/** A yellow rim around the hatch, so it reads from the deck and from the water. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UStaticMeshComponent>> HatchRim;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> HatchCover;

	/** The black water in the open hatch: near-mirror material from FDeepLookTuning::HatchWaterMaterial. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> HatchWater;

	/** Spot light straight down from the hull (FDeepLookTuning::BellyFlood*). */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<USpotLightComponent> BellyFlood;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> Window;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> NoseCone;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> Tower;

	/** The outside dress, one component per DeepHull::Parts entry (same order). */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UStaticMeshComponent>> HullDress;

	/** Work lights on the top rails amidships, aimed down each side (Look.HullLight*). */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<USpotLightComponent>> HullLights;

	/** A housing on each bow light, attached to the light so it turns with it when the helm floods aim. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UStaticMeshComponent>> BowHousings;

	/** Lights the water under the hatch (was the airlock light). */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UPointLightComponent> AirlockLight;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UCameraComponent> HelmCamera;

	/** D-070: the helm's wide, dim fill from the bow (Look.HelmFill*), on only while the helm is manned. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<USpotLightComponent> HelmFill;

	/** D-070: the visual hull (BT-01, its dress and outside lights) under one parent that the idle bob moves; the colliding box stays on Root. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<USceneComponent> HullVisual;
	/** The idle's amount now (0..1, eased) and the bob last applied. */
	float IdleAmount = 0.f;
	FDeepIdleBob IdleNow;
	/** Moves HullVisual by the idle bob (every machine, from the server's clock). */
	void TickIdle(float DeltaSeconds);

	/** Ahead and down from under the nose: what the helm camera sees the sea move in. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<USpotLightComponent>> BowLights;

	/** Red port, green starboard, blinking. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UPointLightComponent>> Markers;

	/** BT-01 (World/DeepHull). */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UDeepHullModel> HullModel;

	/** One bulb per lamp, in DeepHull::ELamp order, each attached to its light. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UStaticMeshComponent>> Bulbs;

	/** Puts the lamps on the model's surface (DeepHull::ModelLamp); LayOut calls it with the model hull. */
	void PlaceLampsOnModel();

	UPROPERTY(ReplicatedUsing = OnRep_Hatch)
	bool bHatchOpen = true;

	UFUNCTION()
	void OnRep_Hatch();

	UPROPERTY(Replicated)
	FVector HomeLocation = FVector::ZeroVector;

	UPROPERTY(Replicated)
	FRotator HomeRotation = FRotator::ZeroRotator;

	UPROPERTY(Replicated)
	bool bHasHome = false;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
