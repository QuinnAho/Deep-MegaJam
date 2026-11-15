#include "Game/DeepPlayerController.h"

#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/World.h"
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
		{ TEXT("Drop"), TEXT("G") },
		{ TEXT("Winch"), TEXT("R") },
		{ TEXT("Lamp"), TEXT("F") },
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
	CutAction = MakeAction(this, TEXT("IA_Cut"), EInputActionValueType::Boolean);
	ContinueAction = MakeAction(this, TEXT("IA_Continue"), EInputActionValueType::Boolean);

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
	Mapping->MapKey(GrabAction, EKeys::LeftMouseButton);
	Mapping->MapKey(DropAction, EKeys::G);
	Mapping->MapKey(WinchAction, EKeys::R);
	Mapping->MapKey(LampAction, EKeys::F);
	Mapping->MapKey(CutAction, EKeys::Q);
	// Continue shares keys with swim-up and grab; it only acts on the settlement screen.
	Mapping->MapKey(ContinueAction, EKeys::SpaceBar);
	Mapping->MapKey(ContinueAction, EKeys::Enter);
	Mapping->MapKey(ContinueAction, EKeys::E);
	Mapping->MapKey(ContinueAction, EKeys::LeftMouseButton);
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
	Input->BindAction(CutAction, ETriggerEvent::Started, this, &ADeepPlayerController::OnCutStart);
	Input->BindAction(CutAction, ETriggerEvent::Completed, this, &ADeepPlayerController::OnCutStop);
	Input->BindAction(ContinueAction, ETriggerEvent::Started, this, &ADeepPlayerController::OnContinue);

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
	const FVector2D Look = Value.Get<FVector2D>() * UDeepTuningSettings::Get().Swim.LookSensitivity;
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
	if (bOpen)
	{
		ClearDiverInput();
	}
	SetPause(bOpen);
}

bool ADeepPlayerController::HandleMenuKey(const FKey& Key)
{
	const ADeepGameState* State = GetWorld() ? GetWorld()->GetGameState<ADeepGameState>() : nullptr;
	const ADeepDiver* Diver = GetDiver();
	const UDeepRunComponent* Run = Diver ? Diver->GetRun() : nullptr;
	const bool bUp = Key == EKeys::Up || Key == EKeys::W;
	const bool bDown = Key == EKeys::Down || Key == EKeys::S;
	const bool bSelect = Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::E || Key == EKeys::LeftMouseButton;
	const bool bMenuKey = Key == EKeys::Escape || Key == EKeys::P;

	// The title: any key starts the run.
	if (State && State->bOnTitle)
	{
		ClearDiverInput();
		ServerStartRun();
		return true;
	}

	// The win screen: dive again or quit.
	if (Run && Run->GetState() == EDeepRunState::Won && !bPauseMenu)
	{
		const TArray<EMenuItem> Items = WinItems();
		MenuIndex = FMath::Clamp(MenuIndex, 0, Items.Num() - 1);
		if (bUp || bDown)
		{
			MenuIndex = (MenuIndex + (bDown ? 1 : Items.Num() - 1)) % Items.Num();
		}
		else if (bSelect && Run->CanLeaveReview())
		{
			ActivateMenuItem(Items[MenuIndex]);
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
		else if (bUp || bDown)
		{
			MenuIndex = (MenuIndex + (bDown ? 1 : Items.Num() - 1)) % Items.Num();
		}
		else if (bSelect)
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
	case EMenuItem::Restart:
	case EMenuItem::DiveAgain:
		SetPauseMenu(false);
		ClearDiverInput();
		ServerRestartRun();
		break;
	case EMenuItem::Quit:
		UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
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

void ADeepPlayerController::OnContinue()
{
	if (ADeepDiver* Diver = GetDiver())
	{
		Diver->Continue();
	}
}
