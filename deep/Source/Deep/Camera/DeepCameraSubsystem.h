// The camera's reactions, per machine: listens on the event bus and plays shakes through the local player's camera
// manager (DeepCameraRules::ShakeForEvent picks them); keeps the footage look's handheld sway running while it is on.
// Draws the sonar ping as a pulse (M_SonarPulse) on each sonar_ping.
// Degrades the feed as the serpent closes (M_FeedInterference): static, torn lines, colour split; full in a bite, a burst on hull_hit.
// Switches (console variables, -1 = the FDeepCameraTuning default): Deep.Camera.Shake, Deep.Look.Footage, Deep.Sonar.Pulse, Deep.Look.Interference.

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
	static bool IsInterferenceOn();

	/** Starts a sonar pulse from Origin over RangeM (restarts one in flight). Ignores the switch: the caller decides. */
	bool StartPulse(const FVector& Origin, float RangeM);
	int32 GetPulses() const { return Pulses; }
	bool IsPulseLive() const { return PulseAgeS >= 0.f; }
	/** The live pulse's origin and range, and whether a diver sent it (a diver's ping is not cut short by the helm's). */
	FVector GetPulseOrigin() const { return PulseOrigin; }
	float GetPulseRangeM() const { return PulseRangeM; }
	bool IsPulseFromDiver() const { return bPulseFromDiver; }

	/** Interference shown now (0..1), and a forced strength for shots (-1 = follow the serpent; Deep.Camera.Interference). */
	float GetInterference() const { return Interference; }
	void SetInterferenceOverride(float Strength) { InterferenceOverride = Strength; }

	/**
	 * Plays an event's shake on the local camera (if it has one and shakes are on), sized by ImpactMps for hull hits
	 * (DeepCameraRules::ShakeForHit). Returns true if one started. The chosen spec is kept even with no camera.
	 */
	bool PlayEventShake(FName Event, float ImpactMps = 0.f);

	/** The last event shake chosen (played or not: a headless world has no camera), for tests and DEEPSTATE. */
	FName GetLastEventShake() const { return LastEventShake; }
	const FDeepShakeSpec& GetLastEventSpec() const { return LastEventSpec; }

	/** The box round the sub's static-mesh parts in the sub's space (cm): what the pulse must never light. */
	static FBox SubPartsLocal(const AActor* Sub);

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
	void UpdateInterference(float DeltaTime);
	/** The serpent writes custom depth so the pulse can leave it out of the dots (only the band crosses it). */
	void MarkSerpents();
	ADeepEnvironment* FindEnvironment();
	/** A dynamic instance of a camera post-process material (loaded by path once; null if missing). */
	UMaterialInstanceDynamic* MakeMaterial(const FSoftObjectPath& Path, const TCHAR* What);

	FDelegateHandle EventHandle;
	int32 ShakesPlayed = 0;
	FName LastShake;
	FName LastEventShake;
	FDeepShakeSpec LastEventSpec;
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
	bool bPulseFromDiver = false;
	/** The sub the pulse left from and its parts' box (sub space, cm): the material skips everything inside it. */
	TWeakObjectPtr<AActor> PulseSub;
	FBox PulseSubParts = FBox(ForceInit);
	int32 Pulses = 0;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> InterferenceMID;
	bool bInterferenceTried = false;
	bool bInterferenceShown = false;
	float Interference = 0.f;
	float InterferenceOverride = -1.f;
	float BurstLeftS = 0.f;
	float FeedClockS = 0.f;
};
