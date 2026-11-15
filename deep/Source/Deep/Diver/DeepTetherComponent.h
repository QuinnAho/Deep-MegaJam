// The diver's tether: enforces the length limit, reels the diver in while winching, and draws the line.
// Ticks after physics so it corrects the frame's movement before rendering. All maths is in UDeepRules.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sim/DeepTuningTypes.h"
#include "DeepTetherComponent.generated.h"

class ADeepSubmarine;
class UStaticMeshComponent;

UCLASS(ClassGroup = (Deep), meta = (BlueprintSpawnableComponent))
class DEEP_API UDeepTetherComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeepTetherComponent();

	/** The visible line (a stretched cylinder) this component positions every frame. */
	void SetLineMesh(UStaticMeshComponent* InLine) { Line = InLine; }

	/** Re-runs the solve and correction now (cheats and tests call this after moving the diver). */
	void UpdateTether(float DeltaSeconds);

	const FDeepTetherSolve& GetSolve() const { return Solve; }
	bool HasAnchor() const;
	FVector GetAnchorLocation() const;
	float GetAnchorDepthM() const;
	bool IsHome() const { return bHome; }
	ADeepSubmarine* GetSubmarine() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	int32 CurrentStop() const;
	void UpdateLine();

	mutable TWeakObjectPtr<ADeepSubmarine> Submarine;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Line;

	FDeepTetherSolve Solve;
	bool bHome = false;
};
