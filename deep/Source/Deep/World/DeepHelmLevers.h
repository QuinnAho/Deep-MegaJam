// The helm's two levers, beside the seat: the throttle (W/S, a notch a press) inboard, the ballast (Space/C) outboard.
// Built at runtime on the room set (ADeepSubRoom) on every machine from engine shapes in worn steel; each arm follows the replicated
// lever state on the game state (HelmThrottleNotch, HelmBallast), and a notch change clicks. Look only, no collision.
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "DeepHelmLevers.generated.h"

class UStaticMeshComponent;

UCLASS()
class DEEP_API UDeepHelmLevers : public USceneComponent
{
	GENERATED_BODY()

public:
	UDeepHelmLevers();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The arms' current pitch (degrees, + = pulled back), for tests and the frame. */
	float GetThrottlePitchDeg() const { return ThrottlePitchDeg; }
	float GetBallastPitchDeg() const { return BallastPitchDeg; }

	/** Builds the shapes (once) where FDeepPilotTuning puts them. */
	void Build();

private:
	struct FLever
	{
		TObjectPtr<UStaticMeshComponent> Base;
		TObjectPtr<USceneComponent> Pivot;
		TObjectPtr<UStaticMeshComponent> Arm;
		TObjectPtr<UStaticMeshComponent> Knob;
	};
	void BuildLever(FLever& Lever, const TCHAR* Name, const FVector2D& AtM, const FLinearColor& KnobPaint);

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> Parts;

	FLever Throttle;
	FLever Ballast;
	bool bBuilt = false;
	float ThrottlePitchDeg = 0.f;
	float BallastPitchDeg = 0.f;
	int32 LastNotch = 0;
	int32 LastBallastStep = 0;
};

namespace DeepHelmLevers
{
	/** Puts the levers on the sub if they are not there yet. Cheap; call every tick on every machine. */
	DEEP_API UDeepHelmLevers* Ensure(UWorld* World);
}
