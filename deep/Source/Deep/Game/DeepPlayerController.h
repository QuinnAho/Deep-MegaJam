// Builds all input in code (Enhanced Input actions and one mapping context via NewObject; no assets)
// and forwards it to the possessed ADeepDiver. Also owns the menus, read raw in InputKey so they work while paused:
// the title screen (any key), the pause menu (Esc or P) and the win screen's choices. The HUD draws them.

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

	/** The menus. Pause: resume, restart the run, quit. Win: dive again, quit. */
	enum class EMenuItem : uint8 { Resume, Restart, DiveAgain, Quit };
	static TArray<EMenuItem> PauseItems() { return { EMenuItem::Resume, EMenuItem::Restart, EMenuItem::Quit }; }
	static TArray<EMenuItem> WinItems() { return { EMenuItem::DiveAgain, EMenuItem::Quit }; }
	static FString MenuItemLabel(EMenuItem Item);

	bool IsPauseMenuOpen() const { return bPauseMenu; }
	int32 GetMenuIndex() const { return MenuIndex; }

	/** Opens or closes the pause menu (pauses the world while open). */
	void SetPauseMenu(bool bOpen);

	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

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
	void OnCutStart();
	void OnCutStop();
	void OnContinue();

	/** A key press while a menu or the title is up. True when it was used (and must not reach the diver). */
	bool HandleMenuKey(const FKey& Key);
	void ActivateMenuItem(EMenuItem Item);
	void ClearDiverInput();

	UFUNCTION(Server, Reliable)
	void ServerStartRun();

	UFUNCTION(Server, Reliable)
	void ServerRestartRun();

	bool bPauseMenu = false;
	int32 MenuIndex = 0;

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

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CutAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ContinueAction;
};
