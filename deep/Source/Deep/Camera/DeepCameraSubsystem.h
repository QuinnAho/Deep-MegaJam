// The camera's reactions, per machine: listens on the event bus and plays shakes through the local player's camera
// manager (DeepCameraRules::ShakeForEvent picks them); keeps the footage look's handheld sway running while it is on.
// Draws the sonar ping as a pulse (M_SonarPulse) on each sonar_ping.
// Switches (console variables, -1 = the FDeepCameraTuning default): Deep.Camera.Shake, Deep.Look.Footage, Deep.Sonar.Pulse.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Sim/DeepTuningTypes.h"
#include "DeepCameraSubsystem.generated.h"

struct FDeepEventPayload;
class APlayerCameraManager;
class UCameraShakeBase;
class FJsonObject;
class ADeepEnvironment;
class UMaterialInstanceDynamic;

UCLASS()
class DEEP_API UDeepCameraSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDeepCameraSubsystem* Get(const UObject* WorldContext);

	static bool IsShakeOn();
	static bool IsFootageOn();
	static bool IsPulseOn();

	/** Starts a sonar pulse from Origin over RangeM (restarts one in flight). Ignores the switch: the caller decides. */
	bool StartPulse(const FVector& Origin, float RangeM);
	int32 GetPulses() const { return Pulses; }
	bool IsPulseLive() const { return PulseAgeS >= 0.f; }

	/** Plays an event's shake on the local camera (if it has one and shakes are on). Returns true if one started. */
	bool PlayEventShake(FName Event);

	/** Plays a shake spec on the local camera. */
	bool PlayShake(const FDeepShakeSpec& Spec, FName Label);

	int32 GetShakesPlayed() const { return ShakesPlayed; }
	FName GetLastShake() const { return LastShake; }
	bool IsSwaying() const { return Sway.IsValid(); }

	/** DEEPSTATE `camera`: the switches and what played. */
	TSharedRef<FJsonObject> BuildStateJson() const;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	void HandleEvent(FName Event, const FDeepEventPayload& Payload);
	APlayerCameraManager* LocalCamera() const;
	void UpdateSway();
	void UpdateLens();
	void UpdatePulse(float DeltaTime);
	/** The serpent writes custom depth so the pulse can leave it out of the dots (only the band crosses it). */
	void MarkSerpents();
	ADeepEnvironment* FindEnvironment();
	/** A dynamic instance of a camera post-process material (loaded by path once; null if missing). */
	UMaterialInstanceDynamic* MakeMaterial(const FSoftObjectPath& Path, const TCHAR* What);

	FDelegateHandle EventHandle;
	int32 ShakesPlayed = 0;
	FName LastShake;
	TWeakObjectPtr<UCameraShakeBase> Sway;
	TWeakObjectPtr<APlayerCameraManager> SwayCamera;

	TWeakObjectPtr<ADeepEnvironment> Environment;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LensMID;
	bool bLensTried = false;
	bool bLensOn = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PulseMID;
	bool bPulseTried = false;
	bool bPulseShown = false;
	float PulseAgeS = -1.f;
	float PulseRangeM = 0.f;
	FVector PulseOrigin = FVector::ZeroVector;
	int32 Pulses = 0;
};
