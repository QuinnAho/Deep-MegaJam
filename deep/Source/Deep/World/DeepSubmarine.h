// The Director's submarine: the fixed point of the game (D-022). It never moves; driving moves the arena root.
// Its box hull is what the water sees: a deck with an open hatch above the tether anchor, the window, the outside
// dress and lights, and a helm camera under the hull. The room divers walk in is a fixed set (World/DeepSubRoom).
// Placed in a map by art later, or spawned by DeepArena::EnsureArena when the map has none.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sim/DeepHelmLightRules.h"
#include "Sim/DeepHelmViewRules.h"
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
	/** V: the chase view (behind and above, on the hull's trail) or the cabin (D2: BT-01's own, the pilot's chair; "cockpit"). */
	void ToggleHelmView();
	bool IsHelmCockpit() const { return bHelmCockpit; }
	void SetHelmCockpit(bool bCockpit);
	/**
	 * The mouse: moves the aim (degrees, + right, + up), within the view's limits; the view follows on its servo (D2) and
	 * the aim goes back ahead after a pause.
	 */
	void AddHelmLook(float YawDeg, float PitchDeg);
	/** Where the view points now, off straight ahead (degrees, X + starboard, Y + up): the servo's angle. */
	FVector2D GetHelmLookOffsetDeg() const { return HelmServo.AngleDeg; }
	/** Where the pilot aims it (the servo's target): Deep.Pilot.MouseSteer steers by it. */
	FVector2D GetHelmAimDeg() const { return HelmAimDeg; }
	/** The servo's speed as a share of its cap (0..1, local): the whine follows it. */
	float GetHelmServoSpeed01() const;
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

	/**
	 * D1b: the hull's own work lamps (HullLights tuning, Sim/DeepHullLightRules): 0 the tower lamp, 1 and 2 the stern
	 * lamps (port, starboard), under HullVisual. Placed once on the hull's real surface by tracing it (PlaceWorkLamps:
	 * true once placed); the floods' spill lights ride the flood heads.
	 */
	const TArray<TObjectPtr<USpotLightComponent>>& GetWorkLamps() const { return WorkLamps; }
	const TArray<TObjectPtr<UPointLightComponent>>& GetFloodSpills() const { return FloodSpills; }
	/** The helm's lights, for tests: the two bow lights (the floods while manned), the fill, the markers. */
	const TArray<TObjectPtr<USpotLightComponent>>& GetBowLights() const { return BowLights; }
	USpotLightComponent* GetHelmFill() const { return HelmFill; }
	const TArray<TObjectPtr<UPointLightComponent>>& GetMarkers() const { return Markers; }
	bool PlaceWorkLamps();
	/** D2: the cabin's lights and the bow ports' panes (index 0 port, 1 starboard), and whether the ports are cut open. */
	const TArray<TObjectPtr<UPointLightComponent>>& GetCabinLights() const { return CabinLights; }
	const TArray<TObjectPtr<UStaticMeshComponent>>& GetPortPanes() const { return PortPanes; }
	bool ArePortsCut() const { return bPortsCut; }
	bool AreWorkLampsPlaced() const { return bWorkLampsPlaced; }
	/** Re-applies every outside light from the tuning (Deep.Tune): now, and the helm's own on the next tick. */
	void RefreshLights();
	/** Where the tower lamp aims (the after deck), in the visual hull's frame; valid once placed. */
	FVector GetDeckAimLocal() const { return DeckAimCm; }
	/**
	 * Where a straight probe (the visual hull's frame, cm) first meets the hull's solid shape: BT-01's simple collision, or
	 * the box hull's solid box. False when it misses.
	 */
	bool TraceHullSurface(const FVector& FromCm, const FVector& ToCm, FVector& OutCm, FVector& OutNormal) const;
	/** How far a point (the visual hull's frame, cm) is from the hull's solid shape: 0 inside it, -1 if it cannot tell. */
	float HullDistanceCm(const FVector& Cm) const;

	/** For DEEPSTATE: the helm view's last frame (the hull top's place on the frame, the chase height over the trail). */
	float GetHullTopFrameY() const { return HullTopFrameY; }
	float GetChaseUpM() const { return float(ChaseUpCm / 100.0); }
	float GetFloodNear(int32 Index) const { return Index >= 0 && Index < 2 ? FloodNear[Index] : 1.f; }
	float GetFillNear() const { return FillNear; }
	/** D1c: the chase view's heave share (local, from the eased travel; -1 sinking .. +1 rising) and the lamps' (every machine, from the replicated heave). */
	float GetViewHeave() const { return ViewHeave; }
	float GetLampHeave() const { return LampHeave; }
	/** D1c: the hull's travel in the world (m/s, eased) as the pilot's view last measured it; zero when nobody is at the helm here. */
	FVector GetHelmTravelWorldMps() const;
	/**
	 * D1c: the sub's lights that are on, as the marine snow sees them (Sim/DeepHelmLightRules: a speck shows only where one
	 * reaches): the floods, the fill, the work lamps, the spills, the markers and the outside lights.
	 */
	void GatherSnowLights(TArray<DeepHelmLight::FSnowLight>& Out, float LitMin) const;

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
	/** D1c: the heave shares (see GetViewHeave), and the floods' base pitch while manned (the toe-out is kept on each). */
	float ViewHeave = 0.f;
	float LampHeave = 0.f;

	/** D-070's pilot view (local): cabin or chase, the mouse's aim, the servo that follows it, how long since the mouse moved, the hull's trail. */
	bool bHelmCockpit = false;
	bool bHelmViewInit = false;
	FVector2D HelmAimDeg = FVector2D::ZeroVector;
	FDeepServo HelmServo;
	float HelmLookIdleS = 100.f;
	/** D2: the chase probe's soft stop, eased (cm from the anchor; negative = not yet measured). */
	double ChaseSoftCm = -1.0;
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
	/** The chase view's probe: a sphere (RadiusCm; <= 0: ProbeRadiusM) swept from Anchor toward Want against world rock (never the hull); where it stops. */
	FVector ProbeChase(const FVector& AnchorCm, const FVector& WantCm, float RadiusCm = 0.f) const;
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
	/** D2: lays out the cabin's lights and cuts the bow ports from the tuning (the model shown, Cabin.bPorts). */
	void ApplyCabin();

	/** D1b: the work lamps on the hull (tower, stern port, stern starboard) and a small bulb on each. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<USpotLightComponent>> WorkLamps;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UStaticMeshComponent>> WorkBulbs;

	/** D1b: one per bow light, riding it: the flood head's spill on the bow while the helm floods burn. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UPointLightComponent>> FloodSpills;

	bool bWorkLampsPlaced = false;
	int32 WorkLampTries = 0;

	/**
	 * D2: BT-01's own cabin (Cabin tuning, Sim/DeepCabinRules): its lights (on while the helm is manned; never in the
	 * fog), the bow ports' panes (thick grimy glass) and the cut that takes the model's opaque port glass away.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UPointLightComponent>> CabinLights;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UStaticMeshComponent>> PortPanes;

	bool bPortsCut = false;

	FVector DeckAimCm = FVector::ZeroVector;
	float HullTopFrameY = 0.f;

	/** The work lamps, spills and markers for the helm's state and the outside lights' depth fade. */
	void ApplyHullLights();

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
