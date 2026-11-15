// Grey-box motion cues for the helm. The sub stays still and the world moves (DeepArena::ApplySubDepth), so without
// these the helm view shows nothing but a number changing. Pale specks stream past opposite to travel and wrap
// round a box around the sub; between stops, two shaft walls with a ledge every few metres slide past the window.
// Speed comes from the change in ADeepGameState::SubDepthM, so it works on every machine. Numbers: FDeepSubTuning.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeepMotionCues.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

namespace DeepMotion
{
	/** Wraps V into [-Half, Half). */
	inline float Wrap(float V, float Half)
	{
		if (Half <= 0.f)
		{
			return 0.f;
		}
		const float Span = Half * 2.f;
		return FMath::Fmod(FMath::Fmod(V + Half, Span) + Span, Span) - Half;
	}

	/** Where ledge Index sits relative to the sub (m, up positive) at sub depth DepthM: ledges are fixed in the world. */
	inline float LedgeOffsetM(int32 Index, float DepthM, float SpacingM, int32 Count)
	{
		const float Half = SpacingM * Count * 0.5f;
		return Wrap(Index * SpacingM + DepthM, Half);
	}
}

UCLASS()
class DEEP_API ADeepMotionCues : public AActor
{
	GENERATED_BODY()

public:
	ADeepMotionCues();

	virtual void Tick(float DeltaSeconds) override;

	/** The sub's vertical speed (m/s, down positive), smoothed. */
	float GetSpeedMps() const { return SpeedMps; }

	/** Mean speck height relative to the box centre (m); tests read it to see the specks move. */
	float GetSpeckMeanZM() const;

protected:
	virtual void BeginPlay() override;

private:
	void Build();
	void Place(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Specks;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Ledges;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> WallLeft;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> WallRight;

	/** Speck positions relative to the box centre (m). */
	TArray<FVector> SpeckM;
	TArray<FTransform> SpeckXf;
	TArray<FTransform> LedgeXf;
	int32 LedgesPerSide = 0;
	float LastDepthM = -1.f;
	float SpeedMps = 0.f;
};
