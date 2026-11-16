// The harpoon bolt and its line: engine basic shapes, no collision. Pure looks. UDeepHarpoonComponent (server)
// moves it and decides what it hits; movement replicates. Every machine lays the line from the diver's gun to the
// bolt each tick, so the line shows in flight and while latched.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
class UDeepRopeComponent;

#include "DeepHarpoonBolt.generated.h"

class UStaticMeshComponent;

UCLASS()
class DEEP_API ADeepHarpoonBolt : public AActor
{
	GENERATED_BODY()

public:
	ADeepHarpoonBolt();

	/** Places the bolt with its tip at TipCm, pointing along Direction. */
	void PlaceBolt(const FVector& TipCm, const FVector& Direction);

	/** Lays the line from the owning diver's gun to the bolt. Tick calls it; tests and cheats may too. */
	void UpdateLine();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> Shaft;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> Tip;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UDeepRopeComponent> Line;

	/** The frame the line last stepped: PlaceBolt and Tick both lay it, and only one may advance it. */
	uint64 LastLineFrame = 0;
};
