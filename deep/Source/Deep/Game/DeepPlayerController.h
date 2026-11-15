// Builds all input in code (Enhanced Input actions and one mapping context via NewObject; no assets)
// and forwards it to the possessed ADeepDiver.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DeepPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;
class ADeepDiver;

UCLASS()
class DEEP_API ADeepPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Key names for the HUD's hints. */
	static FString KeyHint(FName Action);

	const UInputMappingContext* GetMappingContext() const { return Mapping; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	void BuildInputObjects();
	void AddMappingToLocalPlayer();
	ADeepDiver* GetDiver() const;

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnVertical(const FInputActionValue& Value);
	void OnSprint(const FInputActionValue& Value);
	void OnGrab();
	void OnDrop();
	void OnWinchStart();
	void OnWinchStop();
	void OnLamp();

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> Mapping;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> VerticalAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> GrabAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> DropAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> WinchAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LampAction;
};
