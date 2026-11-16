#include "Game/DeepGameMode.h"

#include "Debug/DeepShot.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "EngineUtils.h"
#include "Game/DeepGameState.h"
#include "Game/DeepHUD.h"
#include "Game/DeepPlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepEventBus.h"
#include "World/DeepArena.h"
#include "World/DeepSubmarine.h"
#include "World/DeepSubRoom.h"

ADeepGameMode::ADeepGameMode()
{
	DefaultPawnClass = ADeepDiver::StaticClass();
	PlayerControllerClass = ADeepPlayerController::StaticClass();
	GameStateClass = ADeepGameState::StaticClass();
	HUDClass = ADeepHUD::StaticClass();
}

void ADeepGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	bStartOnTitle = !FParse::Param(FCommandLine::Get(), TEXT("DeepNoTitle"));
}

void ADeepGameMode::InitGameState()
{
	Super::InitGameState();
	if (ADeepGameState* State = GetGameState<ADeepGameState>())
	{
		State->Credits = FMath::Max(0, UDeepTuningSettings::Get().Economy.StartingCredits);
		State->SubDepthM = UDeepTuningSettings::Get().GetStop(1).SubDepthM;
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
	// Before any actor begins play, so the divers' first dive waits for the title.
	if (ADeepGameState* State = GetGameState<ADeepGameState>(); State && bStartOnTitle)
	{
		State->bOnTitle = true;
	}
	Super::StartPlay();
	if (const ADeepGameState* State = GetGameState<ADeepGameState>(); State && State->bOnTitle)
	{
		if (UDeepEventBus* Bus = UDeepEventBus::Get(this))
		{
			Bus->Broadcast(DeepEvent::Title, FDeepEventPayload());
		}
	}
	DeepShot::StartIfRequested(GetWorld());
}

void ADeepGameMode::RestartPlayer(AController* NewPlayer)
{
	// Players can log in before StartPlay, so the arena is built on demand here too.
	if (EnsureArena())
	{
		// Every run starts aboard: on the room's deck (World/DeepSubRoom), facing the hatch and the helm.
		if (const ADeepSubRoom* Room = DeepArena::EnsureRoom(GetWorld()))
		{
			RestartPlayerAtTransform(NewPlayer, Room->GetRoomStartTransform());
			return;
		}
	}
	Super::RestartPlayer(NewPlayer);
}

void ADeepGameMode::SetPlayerDefaults(APawn* PlayerPawn)
{
	Super::SetPlayerDefaults(PlayerPawn);
	// Start looking a little down so the first frame shows the hatch in the deck.
	if (AController* Controller = PlayerPawn ? PlayerPawn->GetController() : nullptr)
	{
		FRotator View = Controller->GetControlRotation();
		View.Pitch = UDeepTuningSettings::Get().Sub.RoomStartPitchDeg;
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
	// A jump, not a drive: the sub is parked at the stop at once.
	State->Stop = FMath::Clamp(Stop, 1, FMath::Max(1, UDeepTuningSettings::Get().NumStops()));
	State->SubDepthM = UDeepTuningSettings::Get().GetStop(State->Stop).SubDepthM;
	State->bParked = true;
	State->DeepestStopReached = FMath::Max(State->DeepestStopReached, State->Stop);
	DeepArena::ApplyStop(GetWorld(), State->Stop);
}

void ADeepGameMode::EnterTitle()
{
	ADeepGameState* State = GetGameState<ADeepGameState>();
	if (!State)
	{
		return;
	}
	State->ResetRun();
	State->bOnTitle = true;
	for (TActorIterator<ADeepDiver> It(GetWorld()); It; ++It)
	{
		if (UDeepRunComponent* Run = It->GetRun())
		{
			Run->ResetRun(true);
		}
	}
	if (UDeepEventBus* Bus = UDeepEventBus::Get(this))
	{
		Bus->Broadcast(DeepEvent::Title, FDeepEventPayload());
	}
}

void ADeepGameMode::StartRun()
{
	ADeepGameState* State = GetGameState<ADeepGameState>();
	if (!State || !State->bOnTitle)
	{
		return;
	}
	State->bOnTitle = false;
	State->RunSeconds = 0.f;
	// Each diver's run component starts its first dive (dive_start) on its next step; do it now so it is immediate.
	for (TActorIterator<ADeepDiver> It(GetWorld()); It; ++It)
	{
		if (UDeepRunComponent* Run = It->GetRun())
		{
			Run->StepRun(0.f);
		}
	}
}

void ADeepGameMode::RestartRun()
{
	if (ADeepGameState* State = GetGameState<ADeepGameState>())
	{
		State->ResetRun();
		State->bOnTitle = false;
	}
	for (TActorIterator<ADeepDiver> It(GetWorld()); It; ++It)
	{
		if (UDeepRunComponent* Run = It->GetRun())
		{
			Run->ResetRun(false);
		}
	}
}
