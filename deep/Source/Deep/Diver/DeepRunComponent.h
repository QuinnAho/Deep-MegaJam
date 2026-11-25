// The diver's run loop: aboard the sub (deck, console, helm), the dive through the hatch, Patience, the cut, bailout
// air, re-tether at the hatch, death and respawn aboard, banking at the console, settlement, and driving the sub.
// Server-authoritative state machine (StepRun) that calls UDeepRules and fires DeepEvent names on the event bus.
// Replicated so each player's HUD can draw its own diver's state.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sim/DeepMovingSubRules.h"
#include "Sim/DeepPilotRules.h"
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
	/** Aboard, standing, not going out through the airlock, and the title is not up: WASD walks. */
	bool CanWalk() const { return State == EDeepRunState::Aboard && !bSeated && !IsOnTitle() && !IsLeavingByAirlock(); }
	/** Seated at the helm: the view is the helm camera and up/down drive the sub. */
	bool IsSeated() const { return bSeated; }
	/** Within use range of the console / the helm (aboard, standing). */
	bool IsNearConsole() const;
	bool IsNearHelm() const;

	// ---- The airlock (World/DeepSubRoom): hold the use key on the valve to go out; coming back runs the same fade ----
	/** Within use range of the valve on the room's hatch (aboard, standing). */
	bool IsNearValve() const;
	/** The valve can open the airlock now: the sub is parked at a stop (the hull's hatch is open below). */
	bool CanCycleAirlock() const;
	/** How far this diver has turned the valve (0..1); it springs back when let go. */
	float GetValveTurn01() const;
	/** The airlock's cycle for this diver (Idle when none): the phase and how black the view is. */
	FDeepAirlockState GetAirlockState() const;
	bool IsInAirlock() const { return AirlockElapsedS >= 0.f; }
	/** Going out and not yet through: the diver stands still in the room. */
	bool IsLeavingByAirlock() const { return IsInAirlock() && bAirlockOut; }
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
	/** Unclip the air line by choice (the tether component's key): bailout air, free re-clip at the hatch. */
	void RequestUnclip();
	void RequestContinue();
	/** The helm control: > 0 descends, < 0 ascends (sent to the server when it changes). */
	void SetHelmInput(float Input);
	/** The cave shaft's sideways steer at the helm, in the sub's frame: X forward (W/S), Y starboard (D/A). */
	void SetHelmSteer(FVector2D Steer);
	/** Server: the sideways speed in the shaft (m/s, the sub's frame). */
	FVector2D GetSteerVelocityMps() const { return SteerVelMps; }

	// ---- Real sub controls at the helm (Sim/DeepPilotRules; state on the game state so every machine sees it) ----
	/** W/S: one throttle notch ahead (Dir > 0) or astern (Dir < 0). The lever stays where it is left. */
	void StepThrottle(int32 Dir);
	/** Held controls: ballast input (> 0 floods with C, < 0 blows with Space) and rudder (-1 port .. 1 starboard). */
	void SetPilotHold(float BallastInput, float Rudder);
	/** X: throttle to stop, ballast to neutral. */
	void AllStop();
	/** For the HUD, on any machine: throttle notch (- astern, 0 stop, + ahead), ballast lever (-1 blown .. 1 flooded), speeds. */
	int32 GetThrottleNotch() const;
	float GetBallast() const;
	float GetSpeedMps() const;
	float GetSurgeMps() const;
	/** Server: the sub's motion under the pilot (stop frame). */
	const FDeepPilotMotion& GetPilotMotion() const { return PilotMotion; }

	// ---- D-070 direct drive (the default; the levers above stay behind Deep.Pilot.Mode levers) ----
	/** The pilot's held keys (Sim/DeepPilotRules::StepDrive); sent to the server when they change. Nothing latches. */
	void SetDriveHold(const FDeepDriveInput& Hold);
	/** Server: what the pilot holds now. */
	const FDeepDriveInput& GetDriveHold() const { return DriveHold; }
	/** The helm drives direct (true) or by the levers: the server's wish (ADeepGameState::WantsDirectDrive), else the replicated mode. */
	bool IsDirectDrive() const;

	// ---- Server ----
	/** Advances the state machine. Ticks call it; tests call it with chosen steps. */
	void StepRun(float DeltaSeconds);
	/**
	 * Deep.World.Caves, cheats and tests: puts the hull at once at a pose in the string's frame (cm, degrees), at rest
	 * with the levers home; parked there (a berth) or under way.
	 */
	void PlaceInCave(const FVector& LocationCm, float YawDeg, bool bPark);
	/** Tethered -> Cut. */
	void CutTether(bool bRequested);
	/** Tethered -> Cut, announcing why (cut_patience, cut_requested, cut_bite). The one Cut path. */
	void CutTetherFor(const TCHAR* Event);
	/** Tethered -> Cut by the diver's own hand (tether_unclip): no re-tether fee at the hatch. */
	void Unclip();
	/** Off the line by choice (an unclip, not a cut): the line hangs whole from the hatch. */
	bool IsUnclipped() const { return bUnclipped; }
	/**
	 * Banks the cargo at the console: settles it, pays credits, buys the next gate if affordable, opens the settlement
	 * screen (or wins, with the relic). A diver still in the water is brought aboard first (cheats and tests).
	 */
	void Bank();
	/** Settlement -> Aboard (ignored until the screen has been up SettlementMinSeconds). */
	void ContinueAfterSettlement();

	// ---- Aboard (server) ----
	/** The use key: stand up from the helm, use the helm or the console if in range, or start turning the valve. */
	void Use();
	/** The use key let go: the valve stops turning and springs back. */
	void ReleaseUse();
	/** Server: starts an airlock cycle, out (hiss, fade, then down through the hull's hatch) or back in (already moved: starts black). */
	void StartAirlock(bool bOut);
	/** Up through the hull's hatch (or a cut diver home: re-tether fee). The diver stands in the room, the view fading in. */
	void EnterSub();
	/** Down through the hull's hatch at once (the airlock calls it when black): swimming, tethered, Patience full, salvage respawned. Only while parked. */
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
	/** Server: advances the airlock; going out, moves the diver once the view is black. True if it moved the diver now. */
	bool StepAirlock(float DeltaSeconds);
	void ClearAirlock();
	void StepHelm(float DeltaSeconds);
	/** The helm's depth: input or autopilot, the limit, parking. */
	void StepHelmDepth(float DeltaSeconds);
	/**
	 * The cave shaft between stops (Sim/DeepCaveRules): steer sideways with inertia (the autopilot follows the centre),
	 * pushed out of the rock in substeps; a hit or a scrape dents the hull, a breach kills the diver aboard.
	 */
	void StepShaft(float DeltaSeconds, bool bAutopilot);
	void DentHull(int32 Points, const TCHAR* Event, float ImpactMps);
	/** Real sub controls (server): one step of the throttle, rudder and ballast, the walls, the limit and parking. */
	void StepPilot(float DeltaSeconds);
	/**
	 * Deep.World.Caves (server): the same levers through the chunk string. The hull's real box is swept against the
	 * chunks each substep (stopping short, then sliding); a turn holds where the box would not fit; no depth limit; it
	 * parks at a berth (ParkNear at the Park sockets), never by depth.
	 */
	void StepPilotCave(float DeltaSeconds);
	/**
	 * The rock's toll after a step (shaft or caves): a knock (impact over the threshold, past the cooldown) dents by
	 * impact speed and trips the throttle; holding against rock scrapes. KnockNormal, when set, keeps only part of the
	 * motion along the wall (the shaft; null in the caves, which resolve it per hit). False when the diver is no longer at the helm.
	 */
	bool StepRockToll(float DeltaSeconds, float ImpactMps, bool bTouching, const FVector2D* KnockNormal);
	/**
	 * D-070, the caves: the hull turned from YawBefore to YawWant and its box overlaps rock. Pushes it clear along the
	 * rock's normal (Drive.TurnPushMaxM at most), then tries half the turn, then a quarter; returns the heading it got
	 * (YawBefore if none fits) and moves AtCm by the push.
	 */
	float TurnClearOfRock(const class ADeepCaveString& String, const FBox& Box, FVector& AtCm, float YawBefore, float YawWant) const;
	/** Server: keeps the game state's mode and the held controls in step with WantsDirectDrive (a switch lets go of everything). */
	void SyncDriveMode();
	/** The held controls ask for nothing: the levers home and the rudder amidships, or no key held. */
	bool ControlsIdle() const;
	/** The old direct controls are in use (tests, cheats, the autopilot, a sideways drift still settling). */
	bool UsesDirectHelm() const;
	/** Parks at Stop: snaps to its depth, or (bGlide, the pilot) leaves the game state to glide there. */
	void Park(int32 Stop, bool bGlide = false);
	/** Deep.World.Caves: parked where the hull is (a berth), levers home, helm_arrive. */
	/** Parks at the berth of Stop (1-based, CaveString.Berths): the gauge snaps to its depth and the stop's line plays. */
	void ParkInCave(int32 Stop);
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
	void ServerRequestUnclip();

	UFUNCTION(Server, Reliable)
	void ServerRequestContinue();

	UFUNCTION(Server, Unreliable)
	void ServerSetHelmInput(float Input);

	UFUNCTION(Server, Unreliable)
	void ServerSetHelmSteer(FVector2D Steer);

	UFUNCTION(Server, Reliable)
	void ServerStepThrottle(int32 Dir);

	UFUNCTION(Server, Reliable)
	void ServerSetPilotHold(float BallastInput, float Rudder);

	UFUNCTION(Server, Reliable)
	void ServerAllStop();

	UFUNCTION(Server, Reliable)
	void ServerSetDriveHold(FDeepDriveInput Hold);

	UFUNCTION(Client, Reliable)
	void ClientEvent(FName Event, const FDeepEventPayload& Payload);

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Deep")
	EDeepRunState State = EDeepRunState::Tethered;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Deep")
	float PatienceSeconds = 0.f;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Deep")
	float BailoutAirSeconds = 0.f;

	/** The diver unclipped (Cut by choice); cleared by any other state. */
	UPROPERTY(Replicated, VisibleAnywhere, Category = "Deep")
	bool bUnclipped = false;

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

	/** The airlock: seconds into the cycle (< 0 none), its direction, and how long the valve has been held. */
	UPROPERTY(Replicated)
	float AirlockElapsedS = -1.f;

	UPROPERTY(Replicated)
	bool bAirlockOut = false;

	UPROPERTY(Replicated)
	float ValveHeldS = 0.f;

	/** Server: the use key is down on the valve; the diver has been moved this cycle. */
	bool bValveHeld = false;
	bool bAirlockMoved = false;

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

	/** Server: the pilot's held controls, the sub's motion and the ballast detent. */
	float PilotBallastInput = 0.f;
	float PilotRudder = 0.f;
	float BallastDetentLeftS = 0.f;
	FDeepPilotMotion PilotMotion;
	/** Server: D-070's held keys, and the mode they were held in (a switch lets go). */
	FDeepDriveInput DriveHold;
	int8 LastDriveMode = -1;

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
