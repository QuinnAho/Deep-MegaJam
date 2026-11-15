#include "Game/DeepPlayerController.h"

#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "InputKeyEventArgs.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Sim/DeepTuningSettings.h"
#include "UI/DeepUserSettings.h"

namespace
{
	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = Type;
		return Action;
	}

	/** Maps a 1D key onto the Y axis of a 2D action (swizzle), optionally negated. */
	void MapToY(UInputMappingContext* Mapping, UObject* Outer, const UInputAction* Action, const FKey& Key, bool bNegate)
	{
		FEnhancedActionKeyMapping& Map = Mapping->MapKey(Action, Key);
		Map.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Outer));
		if (bNegate)
		{
			Map.Modifiers.Add(NewObject<UInputModifierNegate>(Outer));
		}
	}

	void MapNegated(UInputMappingContext* Mapping, UObject* Outer, const UInputAction* Action, const FKey& Key, bool bX, bool bY)
	{
		FEnhancedActionKeyMapping& Map = Mapping->MapKey(Action, Key);
		UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(Outer);
		Negate->bX = bX;
		Negate->bY = bY;
		Negate->bZ = false;
		Map.Modifiers.Add(Negate);
	}
}

FString ADeepPlayerController::KeyHint(FName Action)
{
	static const TMap<FName, FString> Hints = {
		{ TEXT("Grab"), TEXT("E") },
		{ TEXT("Use"), TEXT("E") },
		{ TEXT("Drop"), TEXT("G") },
		{ TEXT("Winch"), TEXT("R") },
		{ TEXT("Lamp"), TEXT("F") },
		{ TEXT("Ping"), TEXT("V") },
		{ TEXT("Sprint"), TEXT("Shift") },
		{ TEXT("Up"), TEXT("Space") },
		{ TEXT("Down"), TEXT("C") },
		{ TEXT("Cut"), TEXT("Q") },
		{ TEXT("Continue"), TEXT("Space") },
		{ TEXT("Menu"), TEXT("Esc") },
	};
	const FString* Hint = Hints.Find(Action);
	return Hint ? *Hint : FString(TEXT("?"));
}

void ADeepPlayerController::BuildInputObjects()
{
	if (Mapping)
	{
		return;
	}
	MoveAction = MakeAction(this, TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(this, TEXT("IA_Look"), EInputActionValueType::Axis2D);
	VerticalAction = MakeAction(this, TEXT("IA_Vertical"), EInputActionValueType::Axis1D);
	SprintAction = MakeAction(this, TEXT("IA_Sprint"), EInputActionValueType::Boolean);
	GrabAction = MakeAction(this, TEXT("IA_Grab"), EInputActionValueType::Boolean);
	DropAction = MakeAction(this, TEXT("IA_Drop"), EInputActionValueType::Boolean);
	WinchAction = MakeAction(this, TEXT("IA_Winch"), EInputActionValueType::Boolean);
	LampAction = MakeAction(this, TEXT("IA_Lamp"), EInputActionValueType::Boolean);
	PingAction = MakeAction(this, TEXT("IA_Ping"), EInputActionValueType::Boolean);
	CutAction = MakeAction(this, TEXT("IA_Cut"), EInputActionValueType::Boolean);
	ContinueAction = MakeAction(this, TEXT("IA_Continue"), EInputActionValueType::Boolean);
	TitleAction = MakeAction(this, TEXT("IA_Title"), EInputActionValueType::Boolean);
	// Listens to every key: it must never block another action's key.
	TitleAction->bConsumeInput = false;

	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_Diver"));

	// Move: X = strafe, Y = forward.
	MapToY(Mapping, this, MoveAction, EKeys::W, false);
	MapToY(Mapping, this, MoveAction, EKeys::S, true);
	Mapping->MapKey(MoveAction, EKeys::D);
	MapNegated(Mapping, this, MoveAction, EKeys::A, true, false);

	// Look: mouse up is +Y; legacy input scales (on in DefaultInput.ini) invert pitch, so negate Y.
	MapNegated(Mapping, this, LookAction, EKeys::Mouse2D, false, true);

	Mapping->MapKey(VerticalAction, EKeys::SpaceBar);
	MapNegated(Mapping, this, VerticalAction, EKeys::C, true, false);
	MapNegated(Mapping, this, VerticalAction, EKeys::LeftControl, true, false);

	Mapping->MapKey(SprintAction, EKeys::LeftShift);
	Mapping->MapKey(GrabAction, EKeys::E);
	// Left mouse is the harpoon (UDeepHarpoonComponent maps it itself).
	Mapping->MapKey(DropAction, EKeys::G);
	Mapping->MapKey(WinchAction, EKeys::R);
	Mapping->MapKey(LampAction, EKeys::F);
	// The diver's sonar ping (in the water only; the diver checks).
	Mapping->MapKey(PingAction, EKeys::V);
	Mapping->MapKey(CutAction, EKeys::Q);
	// Continue shares keys with swim-up and grab; it only acts on the settlement screen.
	Mapping->MapKey(ContinueAction, EKeys::SpaceBar);
	Mapping->MapKey(ContinueAction, EKeys::Enter);
	Mapping->MapKey(ContinueAction, EKeys::E);
	Mapping->MapKey(ContinueAction, EKeys::LeftMouseButton);
	// The title: any non-analog key or mouse button. A real press is caught first in InputKey; this catches keys
	// that reach PlayerInput directly (input injection, as the scripted playtests do).
	Mapping->MapKey(TitleAction, EKeys::AnyKey);
}

void ADeepPlayerController::AddMappingToLocalPlayer()
{
	ULocalPlayer* Local = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = Local ? Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (Subsystem && Mapping && !Subsystem->HasMappingContext(Mapping))
	{
		Subsystem->AddMappingContext(Mapping, 0);
	}
}

void ADeepPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BuildInputObjects();

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Input)
	{
		UE_LOG(LogTemp, Error, TEXT("DEEP: InputComponent is not an EnhancedInputComponent; check DefaultInput.ini"));
		return;
	}
	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADeepPlayerController::OnMove);
	Input->BindAction(MoveAction, ETriggerEvent::Completed, this, &ADeepPlayerController::OnMove);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADeepPlayerController::OnLook);
	Input->BindAction(VerticalAction, ETriggerEvent::Triggered, this, &ADeepPlayerController::OnVertical);
	Input->BindAction(VerticalAction, ETriggerEvent::Completed, this, &ADeepPlayerController::OnVertical);
	Input->BindAction(SprintAction, ETriggerEvent::Started, this, &ADeepPlayerController::OnSprint);
	Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &ADeepPlayerController::OnSprint);
	Input->BindAction(GrabAction, ETriggerEvent::Started, this, &ADeepPlayerController::OnGrab);
	Input->BindAction(DropAction, ETriggerEvent::Started, this, &ADeepPlayerController::OnDrop);
	Input->BindAction(WinchAction, ETriggerEvent::Started, this, &ADeepPlayerController::OnWinchStart);
	Input->BindAction(WinchAction, ETriggerEvent::Completed, this, &ADeepPlayerController::OnWinchStop);
	Input->BindAction(LampAction, ETriggerEvent::Started, this, &ADeepPlayerController::OnLamp);
	Input->BindAction(PingAction, ETriggerEvent::Started, this, &ADeepPlayerController::OnPing);
	Input->BindAction(CutAction, ETriggerEvent::Started, this, &ADeepPlayerController::OnCutStart);
	Input->BindAction(CutAction, ETriggerEvent::Completed, this, &ADeepPlayerController::OnCutStop);
	Input->BindAction(ContinueAction, ETriggerEvent::Started, this, &ADeepPlayerController::OnContinue);
	Input->BindAction(TitleAction, ETriggerEvent::Started, this, &ADeepPlayerController::OnTitleKey);

	AddMappingToLocalPlayer();
}

void ADeepPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController())
	{
		BuildInputObjects();
		AddMappingToLocalPlayer();
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
		// The player's saved volume and ray tracing choice, before the first frame.
		UDeepUserSettings::Get().ApplyAll(this);
	}
}

ADeepDiver* ADeepPlayerController::GetDiver() const
{
	return Cast<ADeepDiver>(GetPawn());
}

void ADeepPlayerController::OnMove(const FInputActionValue& Value)
{
	if (ADeepDiver* Diver = GetDiver())
	{
		Diver->SetMoveInput(Value.Get<FVector2D>());
	}
}

void ADeepPlayerController::OnLook(const FInputActionValue& Value)
{
	// The tuned sensitivity times the player's own scale, and their invert choice (UDeepUserSettings).
	const UDeepUserSettings& Settings = UDeepUserSettings::Get();
	const FVector2D Look = DeepSettingsRules::ScaleLook(Value.Get<FVector2D>(), UDeepTuningSettings::Get().Swim.LookSensitivity,
		Settings.LookSensitivity, Settings.bInvertLook);
	AddYawInput(Look.X);
	AddPitchInput(Look.Y);
}

void ADeepPlayerController::OnVertical(const FInputActionValue& Value)
{
	if (ADeepDiver* Diver = GetDiver())
	{
		Diver->SetVerticalInput(Value.Get<float>());
	}
}

void ADeepPlayerController::OnSprint(const FInputActionValue& Value)
{
	if (ADeepDiver* Diver = GetDiver())
	{
		Diver->SetSprinting(Value.Get<bool>());
	}
}

void ADeepPlayerController::OnGrab()
{
	if (ADeepDiver* Diver = GetDiver())
	{
		Diver->Grab();
	}
}

void ADeepPlayerController::OnDrop()
{
	if (ADeepDiver* Diver = GetDiver())
	{
		Diver->Drop();
	}
}

void ADeepPlayerController::OnWinchStart()
{
	if (ADeepDiver* Diver = GetDiver())
	{
		Diver->SetWinching(true);
	}
}

void ADeepPlayerController::OnWinchStop()
{
	if (ADeepDiver* Diver = GetDiver())
	{
		Diver->SetWinching(false);
	}
}

void ADeepPlayerController::OnLamp()
{
	if (ADeepDiver* Diver = GetDiver())
	{
		Diver->ToggleLamp();
	}
}

void ADeepPlayerController::OnPing()
{
	if (ADeepDiver* Diver = GetDiver())
	{
		Diver->Ping();
	}
}

void ADeepPlayerController::OnCutStart()
{
	if (ADeepDiver* Diver = GetDiver())
	{
		Diver->SetCutHeld(true);
	}
}

void ADeepPlayerController::OnCutStop()
{
	if (ADeepDiver* Diver = GetDiver())
	{
		Diver->SetCutHeld(false);
	}
}

FString ADeepPlayerController::MenuItemLabel(EMenuItem Item)
{
	switch (Item)
	{
	case EMenuItem::Resume: return TEXT("RESUME");
	case EMenuItem::Restart: return TEXT("RESTART THE RUN");
	case EMenuItem::DiveAgain: return TEXT("DIVE AGAIN");
	case EMenuItem::Quit: return TEXT("QUIT");
	case EMenuItem::Start: return TEXT("START THE DIVE");
	case EMenuItem::Settings: return TEXT("SETTINGS");
	case EMenuItem::Credits: return TEXT("CREDITS");
	}
	return FString();
}

bool ADeepPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	// Menus read raw key presses: they work while the world is paused and keep keys away from the diver.
	if (Params.Event == IE_Pressed && !Params.Key.IsAnalog() && HandleMenuKey(Params.Key))
	{
		return true;
	}
	return Super::InputKey(Params);
}

void ADeepPlayerController::ClearDiverInput()
{
	if (ADeepDiver* Diver = GetDiver())
	{
		Diver->SetMoveInput(FVector2D::ZeroVector);
		Diver->SetVerticalInput(0.f);
		Diver->SetSprinting(false);
		Diver->SetWinching(false);
		Diver->SetCutHeld(false);
	}
}

void ADeepPlayerController::SetPauseMenu(bool bOpen)
{
	bPauseMenu = bOpen;
	MenuIndex = 0;
	Screen = EMenuScreen::None;
	ScreenIndex = 0;
	if (bOpen)
	{
		ClearDiverInput();
	}
	SetPause(bOpen);
}

void ADeepPlayerController::OpenMenuScreen(EMenuScreen InScreen)
{
	Screen = InScreen;
	// Quit starts on "stay": a stray Enter never leaves the game.
	ScreenIndex = 0;
}

bool ADeepPlayerController::StepMenuCursor(const FKey& Key, int32 Count, int32& Index)
{
	const bool bUp = Key == EKeys::Up || Key == EKeys::W || Key == EKeys::MouseScrollUp || Key == EKeys::Gamepad_DPad_Up;
	const bool bDown = Key == EKeys::Down || Key == EKeys::S || Key == EKeys::MouseScrollDown || Key == EKeys::Gamepad_DPad_Down;
	if (!bUp && !bDown)
	{
		return false;
	}
	if (Count > 0)
	{
		Index = (FMath::Clamp(Index, 0, Count - 1) + (bDown ? 1 : Count - 1)) % Count;
	}
	return true;
}

namespace
{
	bool DeepMenuIsSelectKey(const FKey& Key)
	{
		return Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::E || Key == EKeys::LeftMouseButton || Key == EKeys::Gamepad_FaceButton_Bottom;
	}

	bool DeepMenuIsBackKey(const FKey& Key)
	{
		return Key == EKeys::Escape || Key == EKeys::P || Key == EKeys::RightMouseButton || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right;
	}

	/** -1 left, +1 right, 0 neither. */
	int32 DeepMenuSideways(const FKey& Key)
	{
		if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left)
		{
			return -1;
		}
		if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right)
		{
			return 1;
		}
		return 0;
	}
}

void ADeepPlayerController::HandleScreenKey(const FKey& Key)
{
	const bool bSelect = DeepMenuIsSelectKey(Key);
	const bool bBack = DeepMenuIsBackKey(Key);
	switch (Screen)
	{
	case EMenuScreen::Settings:
	{
		const int32 Count = static_cast<int32>(EDeepSettingRow::Count);
		if (bBack)
		{
			Screen = EMenuScreen::None;
			return;
		}
		if (StepMenuCursor(Key, Count, ScreenIndex))
		{
			return;
		}
		UDeepUserSettings& Settings = UDeepUserSettings::Get();
		const int32 Side = DeepMenuSideways(Key);
		if (Side == 0 && !bSelect)
		{
			return;
		}
		switch (static_cast<EDeepSettingRow>(ScreenIndex))
		{
		case EDeepSettingRow::Volume:
			if (Side != 0)
			{
				Settings.SetVolume(this, DeepSettingsRules::StepVolume(Settings.MasterVolume, Side));
			}
			break;
		case EDeepSettingRow::Sensitivity:
			if (Side != 0)
			{
				Settings.SetSensitivity(DeepSettingsRules::StepSensitivity(Settings.LookSensitivity, Side));
			}
			break;
		case EDeepSettingRow::InvertLook:
			Settings.SetInvertLook(!Settings.bInvertLook);
			break;
		case EDeepSettingRow::RayTracing:
			Settings.SetRayTracing(!Settings.IsRayTracingOn());
			break;
		case EDeepSettingRow::Back:
			if (bSelect)
			{
				Screen = EMenuScreen::None;
			}
			break;
		default:
			break;
		}
		return;
	}
	case EMenuScreen::Credits:
		if (bBack || bSelect)
		{
			Screen = EMenuScreen::None;
		}
		return;
	case EMenuScreen::ConfirmQuit:
		if (bBack)
		{
			Screen = EMenuScreen::None;
		}
		else if (StepMenuCursor(Key, 2, ScreenIndex))
		{
		}
		else if (bSelect)
		{
			if (ScreenIndex == 1)
			{
				UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
			}
			Screen = EMenuScreen::None;
		}
		return;
	default:
		Screen = EMenuScreen::None;
		return;
	}
}

bool ADeepPlayerController::HandleMenuKey(const FKey& Key)
{
	const ADeepDiver* Diver = GetDiver();
	const UDeepRunComponent* Run = Diver ? Diver->GetRun() : nullptr;
	const ADeepGameState* State = GetWorld() ? GetWorld()->GetGameState<ADeepGameState>() : nullptr;
	const bool bOnTitle = State && State->bOnTitle;
	const bool bWon = Run && Run->GetState() == EDeepRunState::Won && !bPauseMenu;
	const bool bSelect = DeepMenuIsSelectKey(Key);
	const bool bMenuKey = Key == EKeys::Escape || Key == EKeys::P || Key == EKeys::Gamepad_Special_Right;

	// A screen over the title, the pause menu or the review takes every key until it closes.
	if (Screen != EMenuScreen::None)
	{
		if (bOnTitle || bPauseMenu || bWon)
		{
			HandleScreenKey(Key);
			return true;
		}
		Screen = EMenuScreen::None;
	}

	// The title: a menu, and any other key starts the run (the title has always left on any key).
	if (bOnTitle)
	{
		const TArray<EMenuItem> Items = TitleItems();
		if (StepMenuCursor(Key, Items.Num(), MenuIndex))
		{
			return true;
		}
		if (bSelect)
		{
			ActivateMenuItem(Items[FMath::Clamp(MenuIndex, 0, Items.Num() - 1)]);
			return true;
		}
		if (Key == EKeys::Escape)
		{
			OpenMenuScreen(EMenuScreen::ConfirmQuit);
			return true;
		}
		return TryStartFromTitle();
	}

	// The win screen: dive again, credits or quit.
	if (bWon)
	{
		const TArray<EMenuItem> Items = WinItems();
		if (!StepMenuCursor(Key, Items.Num(), MenuIndex) && bSelect && Run->CanLeaveReview())
		{
			ActivateMenuItem(Items[FMath::Clamp(MenuIndex, 0, Items.Num() - 1)]);
		}
		return true;
	}

	// The pause menu.
	if (bPauseMenu)
	{
		const TArray<EMenuItem> Items = PauseItems();
		if (bMenuKey)
		{
			SetPauseMenu(false);
		}
		else if (!StepMenuCursor(Key, Items.Num(), MenuIndex) && bSelect)
		{
			ActivateMenuItem(Items[FMath::Clamp(MenuIndex, 0, Items.Num() - 1)]);
		}
		return true;
	}
	if (bMenuKey)
	{
		SetPauseMenu(true);
		return true;
	}
	return false;
}

void ADeepPlayerController::ActivateMenuItem(EMenuItem Item)
{
	switch (Item)
	{
	case EMenuItem::Resume:
		SetPauseMenu(false);
		break;
	case EMenuItem::Start:
		TryStartFromTitle();
		break;
	case EMenuItem::Restart:
	case EMenuItem::DiveAgain:
		SetPauseMenu(false);
		ClearDiverInput();
		ServerRestartRun();
		break;
	case EMenuItem::Settings:
		OpenMenuScreen(EMenuScreen::Settings);
		break;
	case EMenuItem::Credits:
		OpenMenuScreen(EMenuScreen::Credits);
		break;
	case EMenuItem::Quit:
		// Quit asks once.
		OpenMenuScreen(EMenuScreen::ConfirmQuit);
		break;
	}
}

void ADeepPlayerController::ServerStartRun_Implementation()
{
	if (ADeepGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeepGameMode>())
	{
		GameMode->StartRun();
	}
}

void ADeepPlayerController::ServerRestartRun_Implementation()
{
	if (ADeepGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeepGameMode>())
	{
		GameMode->RestartRun();
	}
}

bool ADeepPlayerController::TryStartFromTitle()
{
	const ADeepGameState* State = GetWorld() ? GetWorld()->GetGameState<ADeepGameState>() : nullptr;
	if (!State || !State->bOnTitle)
	{
		return false;
	}
	ClearDiverInput();
	MenuIndex = 0;
	Screen = EMenuScreen::None;
	// On the server (standalone, listen host) start directly; a remote client asks the server.
	if (HasAuthority())
	{
		ADeepGameMode* GameMode = GetWorld()->GetAuthGameMode<ADeepGameMode>();
		// Test worlds spawn the game mode as a plain actor (as the Deep.* cheats allow for).
		for (TActorIterator<ADeepGameMode> It(GetWorld()); It && !GameMode; ++It)
		{
			GameMode = *It;
		}
		if (GameMode)
		{
			GameMode->StartRun();
		}
	}
	else
	{
		ServerStartRun();
	}
	return true;
}

void ADeepPlayerController::OnTitleKey()
{
	// A screen over the title (settings, credits, quit) keeps the title up.
	if (Screen == EMenuScreen::None)
	{
		TryStartFromTitle();
	}
}

void ADeepPlayerController::OnContinue()
{
	if (ADeepDiver* Diver = GetDiver())
	{
		Diver->Continue();
	}
}
