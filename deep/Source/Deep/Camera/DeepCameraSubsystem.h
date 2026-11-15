// The camera's reactions, per machine: listens on the event bus and plays shakes through the local player's camera
// manager (DeepCameraRules::ShakeForEvent picks them); keeps the footage look's handheld sway running while it is on.
// Switches (console variables, -1 = the FDeepCameraTuning default): Deep.Camera.Shake, Deep.Look.Footage.

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
};
