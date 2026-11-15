// The Director's submarine. Hangs above the arena; the tether anchor (airlock) is under its belly.
// Placed in a map by art later, or spawned by DeepArena::EnsureArena when the map has none.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeepSubmarine.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

UCLASS()
class DEEP_API ADeepSubmarine : public AActor
{
	GENERATED_BODY()

public:
	ADeepSubmarine();

	/** World position the tether is tied to (the airlock). */
	UFUNCTION(BlueprintPure, Category = "Deep")
	FVector GetAnchorLocation() const;

	/** Where a new diver appears: under the airlock, facing along the sub. */
	UFUNCTION(BlueprintPure, Category = "Deep")
	FTransform GetDiverStartTransform() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> Hull;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> NoseCone;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> Tower;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> Airlock;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UPointLightComponent> AirlockLight;
};
