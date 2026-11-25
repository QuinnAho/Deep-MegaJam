// Immersion I1 (D-072): silt, the thing that steals your sight in a flooded cave. One per machine (transient, not
// replicated; UDeepImmersion spawns it), hung from the arena root so the puffs stay put in the cave while the hull moves.
// A few times a second it probes the rock round the hull's solid box (three traces down, five out) and round each
// swimming diver (one down): the hull running within LiftHeightM of the floor lifts puffs there, rock within
// ScrapeGapM of a side while moving scrapes puffs off it, a knock (hull_hit) bursts them, a diver kicking near the
// floor lifts a small one. Each puff rises off the surface on its open side, spreads, hangs 10 to 20 s, drifts and
// settles (Sim/DeepImmersionRules::PuffAt); the hull shoves puffs out of its box (KeepOffBox), never into the floor.
// Drawn as instanced engine spheres in M_SiltPuff (Content/Deep/Immersion, built by build_immersion_materials.py):
// translucent, lit by the lamp and floods through the translucency volume, soft edge, depth-faded into rock.
// Everything a machine sees from the replicated hull pose and diver movement, so every player gets the same clouds.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"
#include "Sim/DeepImmersionRules.h"
#include "DeepSilt.generated.h"

class FJsonObject;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
struct FDeepEventPayload;

UCLASS(NotPlaceable, Transient)
class DEEP_API ADeepSilt : public AActor
{
	GENERATED_BODY()

public:
	ADeepSilt();

	/** The world's silt actor, spawned (and hung from the arena root) if missing. */
	static ADeepSilt* Ensure(UWorld* World);
	static ADeepSilt* Find(const UWorld* World);

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/** One step: probe (at ProbeHz), age the puffs, keep them off the hull, draw. Tests call it directly. */
	void StepSilt(float DeltaSeconds);

	/** Puffs at a world surface point with its normal (cm, unit). Returns how many were added. */
	int32 Burst(const FVector& WorldCm, const FVector& WorldNormal, int32 Count, float Scale = 1.f);
	/** Puffs on the floor under the hull (cheats, frames). False when no floor is within reach. */
	bool BurstUnderHull(int32 Count);
	void Clear();

	/** Deep.Silt.Hide: draw nothing (the puffs still live), for a GPU A/B. */
	void SetDrawHidden(bool bHide);

	const FDeepSiltPool& GetPool() const { return Pool; }
	int32 GetPuffCount() const { return Pool.Num(); }
	/** The hull's underside to the floor (m) at the last probe; < 0 = no floor within reach. */
	float GetHullClearanceM() const { return HullClearanceM; }
	/** The nearest rock to a side of the hull at the last probe (m); < 0 = none in reach. */
	float GetHullGapM() const { return HullGapM; }
	/** The hull's speed through the water (m/s) as the probes measure it. */
	float GetHullSpeedMps() const { return HullSpeedMps; }
	int32 GetLifted() const { return Lifted; }
	int32 GetScraped() const { return Scraped; }
	int32 GetKnocked() const { return Knocked; }
	int32 GetKicked() const { return Kicked; }
	bool HasMaterial() const { return PuffMaterial != nullptr; }

	/** True when any puff (at its radius x HullKeepShare) reaches into the hull's solid box now. */
	bool AnyPuffInHull() const;
	/** Where a puff is now, in the world. */
	FDeepSiltPuffNow PuffNowWorld(int32 Index) const;

	/** Deep.Silt.Bench: after Seconds, logs `DEEPSILT bench {frames, frame_ms, gpu_ms, puffs, hidden}` (means over the span). */
	void StartBench(float Seconds);

	/** Tests: probe now on the next step regardless of ProbeHz. */
	void ProbeNextStep() { ProbeClock = 1000.f; }

	/** `{puffs, max, lifted, scraped, knocked, kicked, clearance_m, gap_m, speed_mps, material, hidden}` for DEEPSTATE. */
	TSharedRef<FJsonObject> BuildStateJson() const;

private:
	/** A probe hit, in the silt frame. */
	struct FContact
	{
		FVector HitCm = FVector::ZeroVector;
		FVector Normal = FVector::UpVector;
		float GapM = -1.f;
	};

	void HandleEvent(FName Event, const FDeepEventPayload& Payload);
	void ProbeHull(float ProbeDt);
	void ProbeDivers(float ProbeDt);
	void KeepOffHull();
	void Draw();
	int32 AddPuffs(const FVector& SiltCm, const FVector& SiltNormal, int32 Count, float Scale, float JitterCm);
	bool Trace(const FVector& FromCm, const FVector& ToCm, FHitResult& Hit) const;
	FTransform SiltFrame() const { return GetActorTransform(); }

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UInstancedStaticMeshComponent> Puffs;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PuffMaterial;

	FDeepSiltPool Pool;
	FRandomStream Rand{ 0x5117 };
	FDelegateHandle EventHandle;
	float ProbeClock = 0.f;
	float LiftCarry = 0.f;
	/** Seconds since each hull probe last scraped a puff off (3 down + 5 out). */
	float ScrapeClocks[8] = { 0.f };
	TMap<TWeakObjectPtr<AActor>, float> DiverClocks;
	TArray<FContact> Contacts;
	FVector LastHullSiltCm = FVector::ZeroVector;
	bool bHasLastHull = false;
	float HullClearanceM = -1.f;
	float HullGapM = -1.f;
	float HullSpeedMps = 0.f;
	bool bDrawHidden = false;
	bool bWarnedMaterial = false;
	int32 Lifted = 0;
	int32 Scraped = 0;
	int32 Knocked = 0;
	int32 Kicked = 0;
	TArray<FTransform> Xforms;
	TArray<float> Opacity;
	float BenchLeft = 0.f;
	int32 BenchFrames = 0;
	double BenchFrameS = 0.0;
	double BenchGpuMs = 0.0;
	int32 BenchPuffs = 0;
};
