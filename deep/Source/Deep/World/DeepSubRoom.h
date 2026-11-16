// The sub's one room as a fixed set (moving sub, step 2): the deck with the closed airlock hatch and its valve wheel,
// the console, the helm seat and panel, the strips and practicals, the room dressing, and a blank porthole (black water,
// now and then a faint light drifting past). It stands at Sub.RoomSetLocationM, far above every dive, and never moves:
// the hull (ADeepSubmarine) is what the water sees. Divers walk here; the airlock (UDeepRunComponent) moves them between.
// Not replicated: every machine spawns its own (DeepArena::EnsureRoom). Same local frame as the hull (World/DeepSubParts).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeepSubRoom.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UInstancedStaticMeshComponent;
class ULocalLightComponent;
class URectLightComponent;
class UBoxComponent;
class UPostProcessComponent;

UCLASS()
class DEEP_API ADeepSubRoom : public AActor
{
	GENERATED_BODY()

public:
	ADeepSubRoom();

	/** Where a diver stands at the start of a run and after a respawn: on the deck, facing the hatch and the helm. */
	UFUNCTION(BlueprintPure, Category = "Deep")
	FTransform GetRoomStartTransform() const;

	/** Where a diver who comes back in through the airlock is put (capsule centre on the deck). */
	FVector GetEnterStandLocation() const;

	/** World Z of the deck's top. */
	double GetDeckTopZ() const;

	/** Centre of the closed hatch on the deck's top. */
	FVector GetHatchLocation() const;
	/** The valve wheel on the hatch: hold E here to go out. */
	FVector GetValveLocation() const;
	FVector GetConsoleLocation() const;
	FVector GetHelmLocation() const;

	/** Inside the room's box (world cm), above the deck's top minus Margin. */
	bool IsInRoom(const FVector& WorldCm, double MarginCm = 0.0) const;

	/** Horizontal distance to a deck point is within the use range. */
	bool IsWithinUseRange(const FVector& WorldCm, const FVector& PointCm) const;

	/** Shows or hides the room's dressing and its lights (World/DeepRoomDress, `Deep.Room.Dress`). */
	void SetRoomDressVisible(bool bVisible);

	/** The valve wheel's turn as last drawn (0..1): the most any diver aboard has turned it. */
	float GetValveTurn01() const { return ValveTurn01; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Places and sizes every shape from the tuning. */
	void LayOut();
	/** Surfaces, lights and screens (FDeepLookTuning, Look|Room). */
	void ApplyLook();

	FVector Local(double X, double Y, double Z) const;

	/** The console screen's glow fraction (eased): dims under the settlement card. */
	float ConsoleGlowScale = 1.f;
	float ValveTurn01 = 0.f;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<USceneComponent> Root;

	/** Four slabs round the hatch, which is shut for good: the airlock is the way out. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UStaticMeshComponent>> Deck;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UStaticMeshComponent>> Walls;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> Ceiling;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UStaticMeshComponent>> HatchRim;

	/** The hatch, closed and solid. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> HatchCover;

	/** The valve wheel lying on the hatch; it turns while a diver holds E on it. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<USceneComponent> ValvePivot;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UStaticMeshComponent>> ValveParts;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> Console;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> ConsoleScreen;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> HelmSeat;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> HelmPanel;

	/** The helm's own screen, on the panel's face toward the seat. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> HelmScreen;

	/** The porthole's glass (solid), a dark backdrop out in the "water" and the light that drifts past between them. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> Window;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> PortholeBackdrop;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UPointLightComponent> PortholeLight;

	/** A bezel round the window glass (top, bottom, port, starboard). */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UStaticMeshComponent>> WindowBezel;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UPointLightComponent> RoomLight;

	/** The room's dressing (DeepRoomDress::Pieces): one instanced mesh per shape and material, no collision. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> RoomDressMeshes;

	/** The dressing's caged lights (DeepRoomDress::Lights). */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TArray<TObjectPtr<ULocalLightComponent>> RoomDressLights;

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

	/** The room's box: inside it the room keeps its own exposure. */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UBoxComponent> RoomVolume;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UPostProcessComponent> RoomGrade;
};
