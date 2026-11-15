// The diver's tether: enforces the length limit, reels the diver in while winching, and draws the line.
// Ticks after physics so it corrects the frame's movement before rendering. All maths is in UDeepRules.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sim/DeepTuningTypes.h"
#include "DeepTetherComponent.generated.h"

class ADeepSubmarine;
class UDeepRopeComponent;

UCLASS(ClassGroup = (Deep), meta = (BlueprintSpawnableComponent))
class DEEP_API UDeepTetherComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeepTetherComponent();

	/** The visible line (a rope) this component drives every frame. */
	void SetLineRope(UDeepRopeComponent* InRope) { Line = InRope; }
	UDeepRopeComponent* GetLineRope() const { return Line; }
	/** The rope length shown now (cm): the straight distance plus slack, or taut. Visual only. */
	float GetShownLengthCm() const { return ShownLengthCm; }

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
	void UpdateLine(float DeltaSeconds);
	/** The line after a cut: parts it where it was cut (bite point or hatch) and lets the loose length fall. */
	void UpdateCutLine(float DeltaSeconds, EDeepRunState RunState);

	mutable TWeakObjectPtr<ADeepSubmarine> Submarine;

	UPROPERTY(Transient)
	TObjectPtr<UDeepRopeComponent> Line;

	FDeepTetherSolve Solve;
	bool bHome = false;
	/** The rope was whole and attached last update; a change to Cut parts it. */
	bool bLineWasTethered = false;
	float ShownLengthCm = 0.f;
};
