// The diver's run loop: aboard the sub (deck, console, helm), the dive through the hatch, Patience, the cut, bailout
// air, re-tether at the hatch, death and respawn aboard, banking at the console, settlement, and driving the sub.
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
	/** Tethered or cut, and the title screen is not up. */
	bool CanSwim() const;
	/** Inside the sub: aboard, on the settlement screen or on the win screen. Walking, not swimming. */
	bool IsInside() const { return State == EDeepRunState::Aboard || State == EDeepRunState::Settling || State == EDeepRunState::Won; }
	/** Aboard, standing, and the title is not up: WASD walks. */
	bool CanWalk() const { return State == EDeepRunState::Aboard && !bSeated && !IsOnTitle(); }
	/** Seated at the helm: the view is the helm camera and up/down drive the sub. */
	bool IsSeated() const { return bSeated; }
	/** Within use range of the console / the helm (aboard, standing). */
	bool IsNearConsole() const;
	bool IsNearHelm() const;
	/** The helm is pushing against the depth limit right now. */
	bool IsHelmAtLimit() const { return bHelmAtLimit; }
	/** Server: the helm is driving itself toward a depth (Deep.Drive). */
	bool IsAutopiloting() const { return AutopilotDepthM >= 0.f; }
	/** World time the console was last used (the HUD shows the account panel for a while). */
	float GetLastConsoleTime() const { return LastConsoleTime; }
	/** World time the last gate was bought (HUD flash). */
	float GetLastGateTime() const { return LastGateTime; }
	/** The title screen is up (the game state says so): the dive has not started. */
	bool IsOnTitle() const;
	/** The diver carries the relic. */
	bool CarriesRelic() const;
	/** The win screen is up and has been long enough for a key to leave it. */
	bool CanLeaveReview() const { return State == EDeepRunState::Won && StateTimeLeft <= 0.f; }
	/** The tether limits the diver and draws its line. */
	bool HasTether() const { return State == EDeepRunState::Tethered; }
	float GetPatienceSeconds() const { return PatienceSeconds; }
	float GetPatienceFraction() const;
	float GetBailoutAirSeconds() const { return BailoutAirSeconds; }
	float GetStateTimeLeft() const { return StateTimeLeft; }
	const FDeepSettlementReport& GetSettlement() const { return Settlement; }
	/** Fraction of Patience the last pickup refilled and the world time it happened (HUD flash). */
	float GetLastRefillFraction() const { return LastRefillFraction; }
	float GetLastRefillTime() const { return LastRefillTime; }
	/** Credits the last fee (re-tether or death) took, and when (HUD flash). */
	int32 GetLastFee() const { return LastFee; }
	float GetLastFeeTime() const { return LastFeeTime; }

	// ---- The serpent (replicated for the HUD) ----
	/** Seconds until the serpent bites through the tether; > 0 only while the bite alarm is up. */
	float GetBiteAlarmLeft() const { return BiteAlarmLeft; }
	bool IsBiteAlarmOn() const { return bBiteAlarm; }
	/** 0 nothing near, 1 on top of this diver or biting its tether. */
	float GetMonsterThreat01() const { return MonsterThreat01; }
	/** The last death was the serpent, not the air. */
	bool WasLastDeathMonster() const { return bLastDeathMonster; }

	// ---- Requests from the owning player (routed to the server) ----
	void RequestCut();
	void RequestContinue();
	/** The helm control: > 0 descends, < 0 ascends (sent to the server when it changes). */
	void SetHelmInput(float Input);
	/** The cave shaft's sideways steer at the helm, in the sub's frame: X forward (W/S), Y starboard (D/A). */
	void SetHelmSteer(FVector2D Steer);
	/** Server: the sideways speed in the shaft (m/s, the sub's frame). */
	FVector2D GetSteerVelocityMps() const { return SteerVelMps; }

	// ---- Server ----
	/** Advances the state machine. Ticks call it; tests call it with chosen steps. */
	void StepRun(float DeltaSeconds);
	/** Tethered -> Cut. */
	void CutTether(bool bRequested);
	/** Tethered -> Cut, announcing why (cut_patience, cut_requested, cut_bite). The one Cut path. */
	void CutTetherFor(const TCHAR* Event);
	/**
	 * Banks the cargo at the console: settles it, pays credits, buys the next gate if affordable, opens the settlement
	 * screen (or wins, with the relic). A diver still in the water is brought aboard first (cheats and tests).
	 */
	void Bank();
	/** Settlement -> Aboard (ignored until the screen has been up SettlementMinSeconds). */
	void ContinueAfterSettlement();

	// ---- Aboard (server) ----
	/** The use key: stand up from the helm, or use the helm or the console if in range. */
	void Use();
	/** Up through the hatch (or a cut diver home: re-tether fee). The diver stands on the deck, walking. */
	void EnterSub();
	/** Down through the hatch: swimming, tethered, Patience full, salvage respawned. Only while parked at a stop. */
	bool LeaveSub();
	/** The console: with cargo it banks; without, it shows the account and buys the next gate if affordable. */
	void UseConsole();
	void TakeHelm();
	void LeaveHelm();
	/** Drives toward TargetDepthM at the helm's speed (clamped to the limit) and parks if it is a stop. < 0 stops. */
	void SetAutopilot(float TargetDepthM);
	/** Buys the next layer gate (flashlight, then hull upgrade): at its price if affordable, or free. */
	bool BuyNextGate(bool bFree);
	void SetPatienceFraction(float Fraction);
	/**
	 * Credits, hull, stop, diver number, run totals, cargo, Patience and the serpent back to the start. A fresh dive
	 * at stop 1, or (bToTitle) the diver waits under the airlock until the title screen is left.
	 */
	void ResetRun(bool bToTitle = false);
	/** The diver picked a piece up: Patience refill and the Captain's comment. */
	void NotifyPickup(const FDeepCargoItem& Item);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	enum class EDiveStart : uint8 { First, Hatch };

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	ADeepDiver* GetDiver() const;
	ADeepGameState* GetGameState() const;
	bool IsDiverHome() const;

	// ---- The serpent (server): UDeepMonsterBrain events for this diver, noise, threat ----
	void BindMonster();
	void UnbindMonster();
	void StepMonsterWires(float DeltaSeconds);
	void ClearBiteAlarm();
	void HandleBiteStarted(ADeepDiver* Who, float AlarmSeconds);
	void HandleBiteBroken(ADeepDiver* Who);
	void HandleTetherSevered(ADeepDiver* Who);
	void HandleDiverCaught(ADeepDiver* Who);
	void HandleStalkStarted(ADeepDiver* Who);
	void HandleRepelled(ADeepDiver* Who);

	void StartDive(EDiveStart Reason);
	/** First step of a run: aboard if the diver stands in the room, else a first dive (hand-spawned test divers). */
	void BeginRun();
	/** The diver came up through the hatch (or is home at the anchor). */
	bool ReachedHatch(bool bArrivalOnly) const;
	void StepAboard(float DeltaSeconds);
	void StepHelm(float DeltaSeconds);
	/** The helm's depth: input or autopilot, the limit, parking. */
	void StepHelmDepth(float DeltaSeconds);
	/**
	 * The cave shaft between stops (Sim/DeepCaveRules): steer sideways with inertia (the autopilot follows the centre),
	 * pushed out of the rock in substeps; a hit or a scrape dents the hull, a breach kills the diver aboard.
	 */
	void StepShaft(float DeltaSeconds, bool bAutopilot);
	void DentHull(int32 Points, const TCHAR* Event, float ImpactMps);
	void Park(int32 Stop);
	/** relic_seen, once per run, the first time the relic is near or in view. */
	void StepRelicSeen();
	/** Banking the relic: the run is won, the review goes up. */
	void Win();
	void Die(bool bByMonster = false);
	/** Die, firing Event (death_air, death_monster, hull_breach). */
	void DieFor(const TCHAR* Event, bool bByMonster);
	void Respawn();
	void ChargeFee(int32 Fee);
	void SetState(EDeepRunState NewState);

	void Fire(const TCHAR* Event, FDeepEventPayload Payload = FDeepEventPayload());
	bool FireChatter(const TCHAR* Event);

	UFUNCTION(Server, Reliable)
	void ServerRequestCut();

	UFUNCTION(Server, Reliable)
	void ServerRequestContinue();

	UFUNCTION(Server, Unreliable)
	void ServerSetHelmInput(float Input);

	UFUNCTION(Server, Unreliable)
	void ServerSetHelmSteer(FVector2D Steer);

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
	bool bSeated = false;

	UPROPERTY(Replicated)
	bool bHelmAtLimit = false;

	UPROPERTY(Replicated)
	float LastConsoleTime = -100.f;

	UPROPERTY(Replicated)
	float LastGateTime = -100.f;

	/** Server: the helm control, and the autopilot target (< 0 off). */
	float HelmInput = 0.f;
	float AutopilotDepthM = -1.f;
	bool bWasDriving = false;
	float IdleSeconds = 0.f;

	/** Server: the cave shaft. Steer input, sideways speed, and the clock its cooldowns run on (seconds driven). */
	FVector2D HelmSteer = FVector2D::ZeroVector;
	FVector2D SteerVelMps = FVector2D::ZeroVector;
	double ShaftClock = 0.0;
	double LastHitClock = -1000.0;
	double LastScrapeClock = -1000.0;
	float ScrapeSeconds = 0.f;

	UPROPERTY(Replicated)
	float LastRefillFraction = 0.f;

	UPROPERTY(Replicated)
	float LastRefillTime = -100.f;

	UPROPERTY(Replicated)
	int32 LastFee = 0;

	UPROPERTY(Replicated)
	float LastFeeTime = -100.f;

	UPROPERTY(Replicated)
	float BiteAlarmLeft = 0.f;

	UPROPERTY(Replicated)
	bool bBiteAlarm = false;

	UPROPERTY(Replicated)
	float MonsterThreat01 = 0.f;

	UPROPERTY(Replicated)
	bool bLastDeathMonster = false;

	/** Noise made but not yet reported (hearing events go out in whole units). */
	float NoiseOwed = 0.f;
	/** The Captain has already said "did you hear that?" this run. */
	bool bMonsterHeardThisRun = false;
	/** monster_near already said for the current approach. */
	bool bSaidNearThisApproach = false;
	TArray<FDelegateHandle> MonsterHandles;

	bool bStarted = false;
	bool bFirstSalvageThisRun = true;
	bool bWasHome = false;
	bool bWasOverloaded = false;
	bool bWasTaut = false;
	bool bWasWinching = false;
	bool bRelicSeenThisRun = false;
	TMap<FName, double> ChatterLastTime;
	int32 FeeSeed = 0;
};
