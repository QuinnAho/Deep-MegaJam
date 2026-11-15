// First-person diver. ACharacter in MOVE_Flying with our own sink and drag (UDeepRules::SwimVelocityMps).
// Holds cargo, the lamp and the tether. Input arrives from ADeepPlayerController as plain calls.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Sim/DeepTuningTypes.h"
#include "DeepDiver.generated.h"

class UCameraComponent;
class USpotLightComponent;
class UStaticMeshComponent;
class UDeepTetherComponent;
class UDeepRunComponent;
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
	void Grab();
	void Drop();
	void SetWinching(bool bInWinching);
	void ToggleLamp();
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
	bool IsLampOn() const { return bLampOn; }
	const TArray<FDeepCargoItem>& GetCargo() const { return Cargo; }
	UDeepTetherComponent* GetTether() const { return Tether; }
	UDeepRunComponent* GetRun() const { return Run; }
	/** 0..1 progress of the held cut key. */
	float GetCutHoldFraction() const;

	/** Server: empties the cargo (bank, death). */
	void ClearCargo();

	/** Back under the airlock, stopped, looking down at the salvage. */
	void ResetToStart();

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
	void ServerSetWinching(bool bInWinching);

	UFUNCTION(Server, Reliable)
	void ServerSetLamp(bool bOn);

	UFUNCTION()
	void OnRep_Lamp();

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<USpotLightComponent> Lamp;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UDeepTetherComponent> Tether;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UStaticMeshComponent> TetherLine;

	UPROPERTY(VisibleAnywhere, Category = "Deep")
	TObjectPtr<UDeepRunComponent> Run;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Deep")
	TArray<FDeepCargoItem> Cargo;

	UPROPERTY(Replicated, VisibleAnywhere, Category = "Deep")
	bool bWinching = false;

	UPROPERTY(ReplicatedUsing = OnRep_Lamp, VisibleAnywhere, Category = "Deep")
	bool bLampOn = true;

	FVector2D MoveInput = FVector2D::ZeroVector;
	float VerticalInput = 0.f;
	bool bSprinting = false;
	bool bCutHeld = false;
	float CutHoldSeconds = 0.f;
};
