// The Director's submarine: the fixed point of the game (D-022). It never moves; driving moves the arena root.
// Its box hull is what the water sees: a deck with an open hatch above the tether anchor, the window, the outside
// dress and lights, and a helm camera under the hull. The room divers walk in is a fixed set (World/DeepSubRoom).
// Placed in a map by art later, or spawned by DeepArena::EnsureArena when the map has none.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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
	 */
	FBox GetSolidBoundsLocal(bool bHullOnly) const;

	/** Locked: a red cover closes the hatch (collision on). Open while parked at a stop. */
	void SetHatchOpen(bool bOpen);
	bool IsHatchOpen() const { return bHatchOpen; }

	/** Where the hull stood when the arena was built (server, once). Deep.World.Still moves the hull from it; D-022 never does. */
	void SetHome(const FTransform& Home);

	/** The home transform, or the hull's own until one is set. */
	FTransform GetHomeTransform() const;

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

	/** Ahead and down from under the nose: what the helm camera sees the sea move in. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<USpotLightComponent>> BowLights;

	/** Red port, green starboard, blinking. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UPointLightComponent>> Markers;

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
