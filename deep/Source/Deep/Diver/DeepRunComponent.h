// The diver's run loop: Patience, the cut, bailout air, re-tether, death and respawn, banking, settlement and descent.
// Server-authoritative state machine (StepRun) that calls UDeepRules and fires DeepEvent names on the event bus.
// Replicated so each player's HUD can draw its own diver's state.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sim/DeepTuningTypes.h"
#include "Story/DeepEventBus.h"
#include "DeepRunComponent.generated.h"

class ADeepDiver;
class ADeepGameState;

UCLASS(ClassGroup = (Deep), meta = (BlueprintSpawnableComponent))
class DEEP_API UDeepRunComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeepRunComponent();

	// ---- State (any machine) ----
	EDeepRunState GetState() const { return State; }
	static FString StateName(EDeepRunState InState);
	bool CanSwim() const { return State == EDeepRunState::Tethered || State == EDeepRunState::Cut; }
	/** The tether limits the diver and draws its line. */
	bool HasTether() const { return State != EDeepRunState::Cut && State != EDeepRunState::Dead; }
	float GetPatienceSeconds() const { return PatienceSeconds; }
	float GetPatienceFraction() const;
	float GetBailoutAirSeconds() const { return BailoutAirSeconds; }
	float GetStateTimeLeft() const { return StateTimeLeft; }
	const FDeepSettlementReport& GetSettlement() const { return Settlement; }
	float GetDescendFromDepthM() const { return DescendFromDepthM; }
	float GetDescendToDepthM() const { return DescendToDepthM; }
	/** Fraction of Patience the last pickup refilled and the world time it happened (HUD flash). */
	float GetLastRefillFraction() const { return LastRefillFraction; }
	float GetLastRefillTime() const { return LastRefillTime; }
	/** Credits the last fee (re-tether or death) took, and when (HUD flash). */
	int32 GetLastFee() const { return LastFee; }
	float GetLastFeeTime() const { return LastFeeTime; }

	// ---- Requests from the owning player (routed to the server) ----
	void RequestCut();
	void RequestContinue();

	// ---- Server ----
	/** Advances the state machine. Ticks call it; tests call it with chosen steps. */
	void StepRun(float DeltaSeconds);
	/** Tethered -> Cut. */
	void CutTether(bool bRequested);
	/** Ends the dive: settles the cargo, pays credits, buys a hull rating if affordable, opens the settlement screen. */
	void Bank();
	/** Settlement -> Descending (ignored until the screen has been up SettlementMinSeconds). */
	void ContinueAfterSettlement();
	void SetPatienceFraction(float Fraction);
	/** Credits, hull, stop and diver number back to the start; a fresh dive at stop 1. */
	void ResetRun();
	/** The diver picked a piece up: Patience refill and the Captain's comment. */
	void NotifyPickup(const FDeepCargoItem& Item);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	enum class EDiveStart : uint8 { First, SameStop, NewStop, Respawn, Reset };

	virtual void BeginPlay() override;

	ADeepDiver* GetDiver() const;
	ADeepGameState* GetGameState() const;
	bool IsDiverHome() const;

	void StartDive(EDiveStart Reason);
	void Retether();
	void Die();
	void Respawn();
	void SwapToNextDive();
	void ChargeFee(int32 Fee);
	void SetState(EDeepRunState NewState);

	void Fire(const TCHAR* Event, FDeepEventPayload Payload = FDeepEventPayload());
	bool FireChatter(const TCHAR* Event);

	UFUNCTION(Server, Reliable)
	void ServerRequestCut();

	UFUNCTION(Server, Reliable)
	void ServerRequestContinue();

	UFUNCTION(Client, Reliable)
	void ClientEvent(FName Event, const FDeepEventPayload& Payload);

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Deep")
	EDeepRunState State = EDeepRunState::Tethered;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Deep")
	float PatienceSeconds = 0.f;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Deep")
	float BailoutAirSeconds = 0.f;

	/** Seconds left in a timed state (Dead, Settling's input lock, Descending). */
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Deep")
	float StateTimeLeft = 0.f;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Deep")
	FDeepSettlementReport Settlement;

	UPROPERTY(Replicated)
	float DescendFromDepthM = 0.f;

	UPROPERTY(Replicated)
	float DescendToDepthM = 0.f;

	UPROPERTY(Replicated)
	float LastRefillFraction = 0.f;

	UPROPERTY(Replicated)
	float LastRefillTime = -100.f;

	UPROPERTY(Replicated)
	int32 LastFee = 0;

	UPROPERTY(Replicated)
	float LastFeeTime = -100.f;

	bool bStarted = false;
	bool bSwapped = false;
	bool bFirstSalvageThisRun = true;
	bool bWasHome = false;
	bool bWasOverloaded = false;
	bool bWasTaut = false;
	bool bWasWinching = false;
	TMap<FName, double> ChatterLastTime;
	int32 FeeSeed = 0;
};
