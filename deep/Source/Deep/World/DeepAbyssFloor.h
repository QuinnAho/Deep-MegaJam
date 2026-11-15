// A wide dark slab far below the sub so a sinking diver has somewhere to land.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeepAbyssFloor.generated.h"

class UStaticMeshComponent;

UCLASS()
class DEEP_API ADeepAbyssFloor : public AActor
{
	GENERATED_BODY()

public:
	ADeepAbyssFloor();

	/** Sizes the slab to SizeM by SizeM, 1 m thick, top face at the actor's location. */
	void SetSizeM(float SizeM);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> Slab;
};
