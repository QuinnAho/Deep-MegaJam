#include "Game/DeepPlayerController.h"

#include "Diver/DeepDiver.h"
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

void ADeepPlayerController::OnContinue()
{
	if (ADeepDiver* Diver = GetDiver())
	{
		Diver->Continue();
	}
}
