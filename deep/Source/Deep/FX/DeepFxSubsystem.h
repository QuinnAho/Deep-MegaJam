// Spawns the water effects from what the divers do: breath bubbles on each breath out (timed from the breath loop the
// player hears), a stream as a diver drops through the hatch, a churn under the hatch while the winch runs, and a
// silt puff where salvage leaves the bed or a diver touches it. Rules are in DeepFxRules; numbers in DEEP FX.
// Every effect is a pooled one-shot (Epic's Niagara interop skill: repeating one-shots use auto-release pooling),
// simulated in world space so bubbles stay in the water they were breathed into while the arena root moves.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeepFxSubsystem.generated.h"

class ADeepDiver;
class UNiagaraSystem;

UCLASS()
class DEEP_API UDeepFxSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDeepFxSubsystem* Get(const UObject* WorldContext);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	/** Spawn a burst now. Returns false when the asset is missing or effects are off. */
	bool SpawnBubbles(const FVector& Location, int32 Count, float SizeScale);
	bool SpawnSilt(const FVector& Location, int32 Count, float SizeScale = 1.f);

	/** Bursts asked for since the world started (tests and `Deep.Fx.State`). */
	int32 GetBubbleBursts() const { return BubbleBursts; }
	int32 GetSiltPuffs() const { return SiltPuffs; }
	/** Breath bursts only (on the breath-out rhythm). */
	int32 GetBreathBursts() const { return BreathBursts; }

	/** Runtime switch for frame-time A/B (`Deep.Fx 0|1`). */
	void SetEnabled(bool bInEnabled) { bRuntimeEnabled = bInEnabled; }
	bool IsEnabled() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FDiverFx
	{
		bool bWasSwimming = false;
		bool bWasWinching = false;
		int32 CargoCount = 0;
		float BreathPhase = 0.f;
		float OwnClockS = 0.f;
		float EntryLeftS = 0.f;
		float EntryNextS = 0.f;
		float ChurnNextS = 0.f;
		float SinceSiltS = 100.f;
		int32 PuffSide = 1;
	};

	void StepDiver(ADeepDiver& Diver, FDiverFx& Fx, float DeltaTime);
	void StepBreath(ADeepDiver& Diver, FDiverFx& Fx, float DeltaTime);
	bool FindBedBelow(const ADeepDiver& Diver, const FVector& From, FVector& OutBed) const;
	UNiagaraSystem* LoadSystem(bool bSilt);

	TMap<TWeakObjectPtr<ADeepDiver>, FDiverFx> Divers;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> BubbleSystem;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> SiltSystem;

	bool bTriedLoad[2] = { false, false };
	bool bRuntimeEnabled = true;
	int32 BubbleBursts = 0;
	int32 SiltPuffs = 0;
	int32 BreathBursts = 0;
};
