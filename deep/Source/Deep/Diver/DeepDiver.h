// First-person diver. In the water: ACharacter in MOVE_Flying with our own sink and drag (UDeepRules::SwimVelocityMps).
// Aboard the sub: MOVE_Walking with gravity; seated at the helm the view is the sub's helm camera.
// Holds cargo, the lamp (needs the flashlight) and the tether. Input arrives from ADeepPlayerController as plain calls.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Sim/DeepTuningTypes.h"
#include "DeepDiver.generated.h"

class UCameraComponent;
class USpotLightComponent;
class UStaticMeshComponent;
class UDeepTetherComponent;
class UDeepRopeComponent;
class UDeepRunComponent;
class UDeepHarpoonComponent;
class ADeepSalvage;

UCLASS()
class DEEP_API ADeepDiver : public ACharacter
{
	GENERATED_BODY()

public:
	ADeepDiver();

	// ---- Input (called by the player controller) ----
	void SetMoveInput(const FVector2D& Move) { MoveInput = Move; }
	void SetVerticalInput(float Up) { VerticalInput = Up; }
	void SetSprinting(bool bInSprint) { bSprinting = bInSprint; }
	/** Grab in the water; aboard it is the use key (console, helm). */
	void Grab();
	/** Aboard: stand up from the helm, or use the helm or the console in range. */
	void Use();
	void Drop();
	void SetWinching(bool bInWinching);
	void ToggleLamp();
	/** The diver's own sonar ping (in the water, every DiverPingCooldownS; no flashlight needed): sonar_ping on every
	 * machine's bus from this diver, which the camera draws as a pulse out to DiverPingRangeM. True if it went out (on a
	 * client: if it was sent to the server, which decides). */
	bool Ping();

	/** X at the helm: all stop (throttle to stop, ballast to neutral). Does nothing out of the seat. */
	void AllStopKey();
	/** In the water (tethered or cut loose), where the ping works. */
	bool IsInWater() const;
	/** Seconds until the next ping is allowed (server's count; 0 = ready). */
	float GetPingWaitS() const;
	/** Hold to ask the Captain to cut the tether (CutHoldSeconds). */
	void SetCutHeld(bool bHeld) { bCutHeld = bHeld; }
	/** The continue key on the settlement screen. */
	void Continue();

	// ---- State ----
	UFUNCTION(BlueprintPure, Category = "Deep")
	float GetCargoKg() const;

	UFUNCTION(BlueprintPure, Category = "Deep")
	int32 GetCargoValue() const;

	UFUNCTION(BlueprintPure, Category = "Deep")
	FDeepWeightResult GetWeight() const;

	UFUNCTION(BlueprintPure, Category = "Deep")
	float GetDepthM() const;

	UFUNCTION(BlueprintPure, Category = "Deep")
	FString GetZoneName() const;

	bool IsWinching() const { return bWinching; }
	/** The lamp shines: switched on and the flashlight has been bought. */
	bool IsLampOn() const;

	/** The light is lit: the lamp is on and the diver is in the water (stowed aboard). */
	bool ShowsLamp() const;
	/** The flashlight has been bought (the lamp key works). */
	bool HasFlashlight() const;
	const TArray<FDeepCargoItem>& GetCargo() const { return Cargo; }
	UDeepTetherComponent* GetTether() const { return Tether; }
	UDeepRunComponent* GetRun() const { return Run; }
	UDeepHarpoonComponent* GetHarpoon() const { return Harpoon; }
	/** 0..1 progress of the held cut key. */
	float GetCutHoldFraction() const;

	/** Server: empties the cargo (bank, death). */
	void ClearCargo();

	/** Back under the airlock (in the water), stopped, looking down at the salvage. */
	void ResetToStart();

	/** On the deck at the room start, stopped, facing the hatch and the helm. */
	void ResetToRoom();

	/** Server (and owner): walking with gravity aboard, or swimming without it. Stops the diver. */
	void SetAboard(bool bInside);

	/** The salvage piece Grab would take right now, or null. */
	ADeepSalvage* FindGrabTarget() const;

	/** Where the line meets the diver (the harness, below the eyes). */
	FVector GetTetherAttachLocation() const;

	/** Server: adds cargo directly (cheats and tests). */
	void AddCargo(const FDeepCargoItem& Item);

	/** Moves the diver, stops it, and re-applies the tether limit. */
	void TeleportDiver(const FVector& LocationCm);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	void ApplySwimTuning();
	void DoGrab();
	void DoDrop();

	UFUNCTION(Server, Reliable)
	void ServerGrab();

	UFUNCTION(Server, Reliable)
	void ServerDrop();

	UFUNCTION(Server, Reliable)
	void ServerUse();

	/** Keeps the movement mode and the view target in step with the run (aboard, seated) on the owning machine. */
	void SyncAboardMode();
	float LastHelmInputSent = 0.f;
	FVector2D LastHelmSteerSent = FVector2D::ZeroVector;
	/** The pilot's levers as last sent: the throttle key held (-1 S, 0, 1 W; a press is an edge), the ballast and rudder holds, X held. */
	int32 LastThrottleAxis = 0;
	FVector2D LastPilotHoldSent = FVector2D::ZeroVector;

	UFUNCTION(Server, Reliable)
	void ServerSetWinching(bool bInWinching);

	UFUNCTION(Server, Reliable)
	void ServerSetLamp(bool bOn);

	UFUNCTION(Server, Reliable)
	void ServerPing();

	/** Every machine hears the ping (its sound and its pulse). */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPing(float RangeM);

	/** Server: when the last ping went out (world seconds; -1 = never). */
	float LastPingS = -1.f;

	UFUNCTION()
	void OnRep_Lamp();

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<USpotLightComponent> Lamp;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UDeepTetherComponent> Tether;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UDeepRopeComponent> TetherLine;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UDeepRunComponent> Run;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UDeepHarpoonComponent> Harpoon;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Deep")
	TArray<FDeepCargoItem> Cargo;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Deep")
	bool bWinching = false;

	UPROPERTY(ReplicatedUsing = OnRep_Lamp, VisibleAnywhere, Category = "Deep")
	bool bLampOn = true;

	/** Close-range dimming of the lamp, followed smoothly (UDeepRules::LampNearScale). Local look only. */
	float LampNearScale = 1.f;

	FVector2D MoveInput = FVector2D::ZeroVector;
	float VerticalInput = 0.f;
	bool bSprinting = false;
	bool bCutHeld = false;
	float CutHoldSeconds = 0.f;
};
