// The Director's submarine: the fixed point of the game (D-022). It never moves; driving moves the arena root.
// Its box hull is also the one room inside: a deck with an open hatch above the tether anchor, a console, a helm,
// a ceiling light, and a helm camera mounted outside under the hull. Shapes are laid out from FDeepSubTuning.
// Placed in a map by art later, or spawned by DeepArena::EnsureArena when the map has none.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeepSubmarine.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
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

	/** Where a diver stands at the start of a run and after a respawn: on the deck, facing the hatch and the helm. */
	UFUNCTION(BlueprintPure, Category = "Deep")
	FTransform GetRoomStartTransform() const;

	/** Where a diver who comes up through the hatch is put (capsule centre on the deck). */
	FVector GetEnterStandLocation() const;

	/** World Z of the deck's top. */
	double GetDeckTopZ() const;

	/** Centre of the hatch opening on the deck's top. */
	FVector GetHatchLocation() const;
	FVector GetConsoleLocation() const;
	FVector GetHelmLocation() const;

	/** Inside the room's box (world cm), above the deck's top minus Margin. */
	bool IsInRoom(const FVector& WorldCm, double MarginCm = 0.0) const;

	/** In the hatch opening (any height). */
	bool IsInHatchColumn(const FVector& WorldCm) const;

	/**
	 * The box (sub-local cm) round every shape that has collision: each is a visible engine cube, so this is the
	 * visible solid hull and nothing more. bHullOnly: the box hull (deck, walls, ceiling) without the tower on top.
	 */
	FBox GetSolidBoundsLocal(bool bHullOnly) const;

	/** Horizontal distance to a deck point is within the use range. */
	bool IsWithinUseRange(const FVector& WorldCm, const FVector& PointCm) const;

	/** Locked: a red cover closes the hatch (collision on). Open while parked at a stop. */
	void SetHatchOpen(bool bOpen);
	bool IsHatchOpen() const { return bHatchOpen; }

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
	/** The console screen's glow fraction (eased): dims under the settlement card. */
	float ConsoleGlowScale = 1.f;
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
	TObjectPtr<UStaticMeshComponent> Console;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> ConsoleScreen;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> HelmSeat;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> HelmPanel;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> Window;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> NoseCone;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> Tower;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UPointLightComponent> RoomLight;

	/** The outside dress, one component per DeepHull::Parts entry (same order). */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UStaticMeshComponent>> HullDress;

	/** Work lights on the top rails amidships, aimed down each side (Look.HullLight*). */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<USpotLightComponent>> HullLights;

	/** A housing on each bow light, attached to the light so it turns with it when the helm floods aim. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UStaticMeshComponent>> BowHousings;

	/** The helm window from inside: a bezel round the glass (top, bottom, port, starboard). */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UStaticMeshComponent>> WindowBezel;

	/** Lights the water under the hatch (was the airlock light). */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UPointLightComponent> AirlockLight;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UCameraComponent> HelmCamera;

	/** The helm's own screen, on the panel's face toward the seat. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> HelmScreen;

	/** Thin fixtures for the ceiling strips (they carry the strips' glow, so the light has a source). */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UStaticMeshComponent>> StripFixtures;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<URectLightComponent>> Strips;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<URectLightComponent> ConsoleLight;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<URectLightComponent> HelmLight;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UPointLightComponent> HatchRed;

	/** Ahead and down from under the nose: what the helm camera sees the sea move in. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<USpotLightComponent>> BowLights;

	/** Red port, green starboard, blinking. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UPointLightComponent>> Markers;

	/** The room's box: inside it the room keeps its own exposure. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UBoxComponent> RoomVolume;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UPostProcessComponent> RoomGrade;

	UPROPERTY(ReplicatedUsing = OnRep_Hatch)
	bool bHatchOpen = true;

	UFUNCTION()
	void OnRep_Hatch();

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
