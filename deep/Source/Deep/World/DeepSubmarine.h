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
class UCameraComponent;

UCLASS()
class DEEP_API ADeepSubmarine : public AActor
{
	GENERATED_BODY()

public:
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

protected:
	virtual void BeginPlay() override;
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

	/** Lights the water under the hatch (was the airlock light). */
	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UPointLightComponent> AirlockLight;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UCameraComponent> HelmCamera;

	UPROPERTY(ReplicatedUsing = OnRep_Hatch)
	bool bHatchOpen = true;

	UFUNCTION()
	void OnRep_Hatch();

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
