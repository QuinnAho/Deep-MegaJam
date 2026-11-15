// The diver's harpoon: one shot per dive along the view, a visible bolt on a line. On the submarine it latches and
// reels the diver to the airlock (the run's re-tether rule takes over there) and dents the hull (a repair fee on the
// next settlement). On anything else solid it pulls the diver to that point; on the serpent it drives it off.
// Server-authoritative; the owning client sends fire/release and moves its own diver while reeling (like the winch).
// Builds its own input (left mouse button) on the owning machine, so the controller needs no changes.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Harpoon/DeepHarpoonRules.h"
#include "Story/DeepEventBus.h"
#include "DeepHarpoonComponent.generated.h"

class ADeepDiver;
class ADeepHarpoonBolt;
class ADeepGameState;
class FJsonObject;
class UInputAction;
class UInputMappingContext;

/** What a ray along the aim meets first, for the bolt and the crosshair. */
struct FDeepHarpoonTrace
{
	EDeepHarpoonTarget Target = EDeepHarpoonTarget::None;
	FVector PointCm = FVector::ZeroVector;
	FVector NormalCm = FVector::UpVector;
	float DistanceCm = 0.f;
	TWeakObjectPtr<AActor> Actor;
};

UCLASS(ClassGroup = (Deep), meta = (BlueprintSpawnableComponent))
class DEEP_API UDeepHarpoonComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeepHarpoonComponent();

	// ---- Input (owning machine) ----
	/** Left mouse pressed: fire along the view, or let go if a line is out. */
	void PressFire();
	/** Left mouse released: lets go of a terrain latch. */
	void ReleaseFire();

	// ---- Server ----
	/** Fires from Origin along Direction (cm, world). Refused (harpoon_empty) when no shot is left; lets go if a line is out. */
	void FireAlong(const FVector& OriginCm, const FVector& Direction);
	/** Lets go of whatever the line holds. */
	void Release();
	/** Full shots again (a dive started). */
	void Reload();
	/** Advances the bolt and the reel. Tick calls it; tests call it with chosen steps. */
	void StepHarpoon(float DeltaSeconds);

	// ---- State (any machine) ----
	int32 GetShotsLeft() const { return ShotsLeft; }
	EDeepHarpoonPhase GetPhase() const { return Phase; }
	EDeepHarpoonTarget GetLatchTarget() const { return LatchTarget; }
	bool IsReady() const { return Phase == EDeepHarpoonPhase::Idle && ShotsLeft > 0; }
	FVector GetLatchPoint() const { return LatchPointCm; }
	EDeepHarpoonResult GetLastResult() const { return LastResult; }
	/** Seconds since LastResult (server clock on every machine). */
	float GetSecondsSinceResult() const;
	/** Metres to what the last out-of-range shot was aimed at. */
	float GetLastOutOfRangeM() const { return LastOutOfRangeM; }
	ADeepHarpoonBolt* GetBolt() const { return Bolt; }

	/** What a ray from Origin along Direction meets within MaxCm: submarine, serpent or anything solid. */
	FDeepHarpoonTrace TraceAim(const FVector& OriginCm, const FVector& Direction, float MaxCm) const;
	/** The owning player's view (camera) on this machine; false without a player controller. */
	bool GetView(FVector& OutOriginCm, FVector& OutDirection) const;
	/** Where the line leaves the diver: the gun, low and right of the eyes. */
	static FVector GunLocation(const ADeepDiver* Diver);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	ADeepDiver* GetDiver() const;
	ADeepGameState* GetGameState() const;
	void BindInput();
	void HandleBusEvent(FName Event, const FDeepEventPayload& Payload);
	void StepFlight(float DeltaSeconds);
	void StepReel(float DeltaSeconds);
	void HandleHit(const FDeepHarpoonTrace& Hit);
	void EndFlight(EDeepHarpoonResult Result);
	void SetResult(EDeepHarpoonResult Result);
	void DestroyBolt(float AfterSeconds = 0.f);
	void Fire(const TCHAR* Event);
	double ServerNow() const;

	UFUNCTION(Server, Reliable)
	void ServerFire(FVector_NetQuantize OriginCm, FVector_NetQuantizeNormal Direction);

	UFUNCTION(Server, Reliable)
	void ServerRelease(bool bOnlyTerrain);

	UFUNCTION(Client, Reliable)
	void ClientEvent(FName Event, const FDeepEventPayload& Payload);

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Deep")
	int32 ShotsLeft = 1;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Deep")
	EDeepHarpoonPhase Phase = EDeepHarpoonPhase::Idle;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Deep")
	EDeepHarpoonTarget LatchTarget = EDeepHarpoonTarget::None;

	UPROPERTY(Replicated)
	FVector LatchPointCm = FVector::ZeroVector;

	/** Where the reel takes the diver: under the airlock, or just off the surface hit. */
	UPROPERTY(Replicated)
	FVector ReelTargetCm = FVector::ZeroVector;

	UPROPERTY(Replicated)
	EDeepHarpoonResult LastResult = EDeepHarpoonResult::None;

	UPROPERTY(Replicated)
	double LastResultTime = -100.0;

	UPROPERTY(Replicated)
	float LastOutOfRangeM = 0.f;

	UPROPERTY(Replicated)
	TObjectPtr<ADeepHarpoonBolt> Bolt;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> FireMapping;

	// Server flight state.
	FVector BoltPosCm = FVector::ZeroVector;
	FVector BoltDir = FVector::ForwardVector;
	float TravelledCm = 0.f;
	/** Something solid along the shot beyond range (for "out of range"), 0 if nothing. */
	float BeyondRangeCm = 0.f;
	float LatchSeconds = 0.f;
	TWeakObjectPtr<UObject> BoundInputComponent;
	FDelegateHandle BusHandle;
};

namespace DeepHarpoon
{
	/**
	 * Settlement hook: appends one repair line per unbilled submarine hit to the fee lines, clears the unbilled
	 * count (the damage itself stays until repaired) and returns the repair total. Server.
	 */
	DEEP_API int32 BillHullRepairs(ADeepGameState* GameState, TArray<FString>& Labels, TArray<int32>& Amounts);

	/** The `harpoon` object in DEEPSTATE: shots, phase, latch, last, latch_cm. */
	DEEP_API TSharedRef<FJsonObject> BuildStateJson(const UDeepHarpoonComponent& Harpoon);

	DEEP_API FString PhaseName(EDeepHarpoonPhase Phase);
	DEEP_API FString TargetName(EDeepHarpoonTarget Target);
	DEEP_API FString ResultName(EDeepHarpoonResult Result);
}
