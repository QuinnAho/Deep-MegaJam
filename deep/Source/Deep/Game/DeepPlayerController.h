// Builds all input in code (Enhanced Input actions and one mapping context via NewObject; no assets)
// and forwards it to the possessed ADeepDiver. Also owns the menus, read raw in InputKey so they work while paused:
// the title menu (and any other key starts), the pause menu (Esc or P), the win screen's choices, and the screens
// over them: settings, credits and the quit confirmation. The HUD draws them.

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

	/** The menus. Title: start, settings, credits, quit. Pause: resume, settings, credits, restart, quit. Win: dive again, credits, quit. */
	enum class EMenuItem : uint8 { Resume, Restart, DiveAgain, Quit, Start, Settings, Credits };
	static TArray<EMenuItem> TitleItems() { return { EMenuItem::Start, EMenuItem::Settings, EMenuItem::Credits, EMenuItem::Quit }; }
	static TArray<EMenuItem> PauseItems() { return { EMenuItem::Resume, EMenuItem::Settings, EMenuItem::Credits, EMenuItem::Restart, EMenuItem::Quit }; }
	static TArray<EMenuItem> WinItems() { return { EMenuItem::DiveAgain, EMenuItem::Credits, EMenuItem::Quit }; }
	static FString MenuItemLabel(EMenuItem Item);

	/** A screen over the title, the pause menu or the review. Quit always passes through ConfirmQuit. */
	enum class EMenuScreen : uint8 { None, Settings, Credits, ConfirmQuit };
	void OpenMenuScreen(EMenuScreen Screen);
	EMenuScreen GetMenuScreen() const { return Screen; }
	/** The cursor on the open screen (settings row, or 0 stay / 1 quit). */
	int32 GetScreenIndex() const { return ScreenIndex; }

	bool IsPauseMenuOpen() const { return bPauseMenu; }
	int32 GetMenuIndex() const { return MenuIndex; }

	/** Opens or closes the pause menu (pauses the world while open). */
	void SetPauseMenu(bool bOpen);

	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

	/**
	 * Leaves the title if it is up; true when it did. Both routes a key takes end here: a real press
	 * (viewport -> InputKey) and a key that reaches PlayerInput directly (Enhanced Input AnyKey -> TitleAction).
	 */
	bool TryStartFromTitle();

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
	/** The grab/use key let go (the airlock valve stops turning). */
	void OnGrabStop();
	void OnDrop();
	void OnWinchStart();
	void OnWinchStop();
	void OnLamp();
	void OnPing();
	void OnAllStop();
	void OnCutStart();
	void OnCutStop();
	void OnContinue();
	void OnTitleKey();

	/** A key press while a menu or the title is up. True when it was used (and must not reach the diver). */
	bool HandleMenuKey(const FKey& Key);
	/** Keys on an open screen (settings, credits, quit). Always used. */
	void HandleScreenKey(const FKey& Key);
	/** Up/down on a list of Count items: moves Index, wrapping. True when the key was up or down. */
	static bool StepMenuCursor(const FKey& Key, int32 Count, int32& Index);
	void ActivateMenuItem(EMenuItem Item);
	void ClearDiverInput();

	UFUNCTION(Server, Reliable)
	void ServerStartRun();

	UFUNCTION(Server, Reliable)
	void ServerRestartRun();

	bool bPauseMenu = false;
	int32 MenuIndex = 0;
	EMenuScreen Screen = EMenuScreen::None;
	int32 ScreenIndex = 0;

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
	TObjectPtr<UInputAction> PingAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> AllStopAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CutAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ContinueAction;

	/** Mapped to EKeys::AnyKey: any key or mouse button leaves the title. */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> TitleAction;
};
