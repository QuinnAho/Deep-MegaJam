#include "Game/DeepGameMode.h"

#include "Diver/DeepDiver.h"
#include "Game/DeepGameState.h"
#include "Game/DeepHUD.h"
#include "Game/DeepPlayerController.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepArena.h"
#include "World/DeepSubmarine.h"

ADeepGameMode::ADeepGameMode()
{
	DefaultPawnClass = ADeepDiver::StaticClass();
	PlayerControllerClass = ADeepPlayerController::StaticClass();
	GameStateClass = ADeepGameState::StaticClass();
	HUDClass = ADeepHUD::StaticClass();
}

void ADeepGameMode::InitGameState()
{
	Super::InitGameState();
	if (ADeepGameState* State = GetGameState<ADeepGameState>())
	{
		State->Credits = FMath::Max(0, UDeepTuningSettings::Get().Economy.StartingCredits);
	}
}

ADeepSubmarine* ADeepGameMode::EnsureArena()
{
	const ADeepGameState* State = GetGameState<ADeepGameState>();
	return DeepArena::EnsureArena(GetWorld(), State ? State->Stop : 1);
}

void ADeepGameMode::StartPlay()
{
	EnsureArena();
	Super::StartPlay();
}

void ADeepGameMode::RestartPlayer(AController* NewPlayer)
{
	// Players can log in before StartPlay, so the arena is built on demand here too.
	if (ADeepSubmarine* Sub = EnsureArena())
	{
		RestartPlayerAtTransform(NewPlayer, Sub->GetDiverStartTransform());
		return;
	}
	Super::RestartPlayer(NewPlayer);
}

void ADeepGameMode::SetPlayerDefaults(APawn* PlayerPawn)
{
	Super::SetPlayerDefaults(PlayerPawn);
	// Start looking slightly down so the first frame shows salvage glinting below.
	if (AController* Controller = PlayerPawn ? PlayerPawn->GetController() : nullptr)
	{
		FRotator View = Controller->GetControlRotation();
		View.Pitch = UDeepTuningSettings::Get().Arena.DiverStartPitchDeg;
		Controller->SetControlRotation(View);
	}
}

void ADeepGameMode::GotoStop(int32 Stop)
{
	ADeepGameState* State = GetGameState<ADeepGameState>();
	if (!State)
	{
		return;
	}
	State->Stop = FMath::Clamp(Stop, 1, FMath::Max(1, UDeepTuningSettings::Get().NumStops()));
	DeepArena::ApplyStop(GetWorld(), State->Stop);
}
