// Console commands for testing. Add a new one with another FAutoConsoleCommandWithWorldAndArgs below;
// names start with "Deep." and the command should end by logging state if it changes any.

#include "CoreMinimal.h"
#include "Debug/DeepStateReport.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Diver/DeepTetherComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "Game/DeepPlayerController.h"
#include "World/DeepArena.h"
#include "World/DeepRopeComponent.h"
#include "World/DeepSalvage.h"
#include "GameFramework/Controller.h"
#include "HAL/IConsoleManager.h"
#include "Sim/DeepTuningSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepCheats, Log, All);

namespace
{
	/** The server's DEEP game mode; also finds one spawned by hand in a test world (no AuthorityGameMode there). */
	ADeepGameMode* FindGameMode(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		if (ADeepGameMode* GameMode = World->GetAuthGameMode<ADeepGameMode>())
		{
			return GameMode;
		}
		for (TActorIterator<ADeepGameMode> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	ADeepDiver* RequireDiver(UWorld* World, const TCHAR* Command)
	{
		ADeepDiver* Diver = DeepStateReport::FindDiver(World);
		if (!Diver)
		{
			UE_LOG(LogDeepCheats, Warning, TEXT("%s: no diver in this world"), Command);
		}
		return Diver;
	}

	FAutoConsoleCommandWithWorldAndArgs StateCommand(
		TEXT("Deep.State"),
		TEXT("Logs one line: DEEPSTATE {json} with depth, zone, cargo, tether, credits, hull, stop and diver position (cm)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs GiveSalvageCommand(
		TEXT("Deep.GiveSalvage"),
		TEXT("Deep.GiveSalvage <kg> <value>: adds a piece of cargo to the diver."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 2)
			{
				UE_LOG(LogDeepCheats, Warning, TEXT("Usage: Deep.GiveSalvage <kg> <value>"));
				return;
			}
			if (ADeepDiver* Diver = RequireDiver(World, TEXT("Deep.GiveSalvage")))
			{
				FDeepCargoItem Item;
				Item.Kg = FMath::Max(0.f, FCString::Atof(*Args[0]));
				Item.Value = FMath::Max(0, FCString::Atoi(*Args[1]));
				// Size (for Patience refill later) is the tuned kind closest in mass.
				float BestGap = TNumericLimits<float>::Max();
				for (const FDeepSalvageKind& Kind : UDeepTuningSettings::Get().SalvageKinds)
				{
					if (FMath::Abs(Kind.Kg - Item.Kg) < BestGap)
					{
						BestGap = FMath::Abs(Kind.Kg - Item.Kg);
						Item.Size = Kind.Size;
					}
				}
				Diver->AddCargo(Item);
				// Counts as a pickup: Patience refill and the Captain's comment.
				Diver->GetRun()->NotifyPickup(Item);
				DeepStateReport::LogState(World);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GotoStopCommand(
		TEXT("Deep.GotoStop"),
		TEXT("Deep.GotoStop <n>: moves the sub to stop n (1-based): light, fog and tether length change."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogDeepCheats, Warning, TEXT("Usage: Deep.GotoStop <n>"));
				return;
			}
			ADeepGameMode* GameMode = FindGameMode(World);
			if (!GameMode)
			{
				UE_LOG(LogDeepCheats, Warning, TEXT("Deep.GotoStop: needs the DEEP game mode on the server"));
				return;
			}
			GameMode->GotoStop(FCString::Atoi(*Args[0]));
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs TeleportCommand(
		TEXT("Deep.Teleport"),
		TEXT("Deep.Teleport <x> <y> <z>: moves the diver to a world position in cm (the tether limit still applies)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 3)
			{
				UE_LOG(LogDeepCheats, Warning, TEXT("Usage: Deep.Teleport <x> <y> <z>"));
				return;
			}
			if (ADeepDiver* Diver = RequireDiver(World, TEXT("Deep.Teleport")))
			{
				Diver->TeleportDiver(FVector(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), FCString::Atof(*Args[2])));
				DeepStateReport::LogState(World);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs LookCommand(
		TEXT("Deep.Look"),
		TEXT("Deep.Look <pitch> [yaw]: points the diver's view (degrees; pitch -90 straight down, +90 up; yaw 0 = toward the sub's nose)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogDeepCheats, Warning, TEXT("Usage: Deep.Look <pitch> [yaw]"));
				return;
			}
			ADeepDiver* Diver = RequireDiver(World, TEXT("Deep.Look"));
			AController* Controller = Diver ? Diver->GetController() : nullptr;
			if (!Controller)
			{
				UE_LOG(LogDeepCheats, Warning, TEXT("Deep.Look: the diver has no controller"));
				return;
			}
			FRotator View = Controller->GetControlRotation();
			View.Pitch = FMath::Clamp(FCString::Atof(*Args[0]), -89.f, 89.f);
			if (Args.Num() >= 2)
			{
				View.Yaw = FCString::Atof(*Args[1]);
			}
			View.Roll = 0.f;
			Controller->SetControlRotation(View);
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs LampCommand(
		TEXT("Deep.Lamp"),
		TEXT("Deep.Lamp <0|1>: turns the diver's lamp off or on."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ADeepDiver* Diver = RequireDiver(World, TEXT("Deep.Lamp"));
			if (Diver && Args.Num() >= 1 && Diver->IsLampOn() != (FCString::Atoi(*Args[0]) != 0))
			{
				Diver->ToggleLamp();
			}
			DeepStateReport::LogState(World);
		}));

	/** Runs Action on the diver's run component (server), then logs state. */
	void WithRun(UWorld* World, const TCHAR* Command, TFunctionRef<void(UDeepRunComponent&)> Action)
	{
		ADeepDiver* Diver = RequireDiver(World, Command);
		if (!Diver || !Diver->GetRun())
		{
			return;
		}
		if (!Diver->HasAuthority())
		{
			UE_LOG(LogDeepCheats, Warning, TEXT("%s: run on the server"), Command);
			return;
		}
		Action(*Diver->GetRun());
		DeepStateReport::LogState(World);
	}

	FAutoConsoleCommandWithWorldAndArgs SetPatienceCommand(
		TEXT("Deep.SetPatience"),
		TEXT("Deep.SetPatience <0..1>: sets the Captain's Patience as a fraction of full. 0 cuts the tether on the next tick."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogDeepCheats, Warning, TEXT("Usage: Deep.SetPatience <0..1>"));
				return;
			}
			const float Fraction = FCString::Atof(*Args[0]);
			WithRun(World, TEXT("Deep.SetPatience"), [Fraction](UDeepRunComponent& Run) { Run.SetPatienceFraction(Fraction); });
		}));

	FAutoConsoleCommandWithWorldAndArgs ForceCutCommand(
		TEXT("Deep.ForceCut"),
		TEXT("Deep.ForceCut: cuts the tether now (as if Patience ran out); bailout air starts."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			WithRun(World, TEXT("Deep.ForceCut"), [](UDeepRunComponent& Run) { Run.CutTether(false); });
		}));

	FAutoConsoleCommandWithWorldAndArgs BankCommand(
		TEXT("Deep.Bank"),
		TEXT("Deep.Bank: banks the diver's cargo at the console (comes aboard first) and opens the settlement screen."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			WithRun(World, TEXT("Deep.Bank"), [](UDeepRunComponent& Run) { Run.Bank(); });
		}));

	FAutoConsoleCommandWithWorldAndArgs ContinueCommand(
		TEXT("Deep.Continue"),
		TEXT("Deep.Continue: leaves the settlement screen, back to the deck."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			WithRun(World, TEXT("Deep.Continue"), [](UDeepRunComponent& Run) { Run.ContinueAfterSettlement(); });
		}));

	FAutoConsoleCommandWithWorldAndArgs ResetRunCommand(
		TEXT("Deep.ResetRun"),
		TEXT("Deep.ResetRun: credits, hull, stop and diver number back to the start; a fresh dive at stop 1."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			// The game mode resets every diver; without one (bare test worlds) the diver resets the run itself.
			if (ADeepGameMode* GameMode = FindGameMode(World))
			{
				GameMode->RestartRun();
				DeepStateReport::LogState(World);
				return;
			}
			WithRun(World, TEXT("Deep.ResetRun"), [](UDeepRunComponent& Run) { Run.ResetRun(); });
		}));

	ADeepGameMode* RequireGameMode(UWorld* World, const TCHAR* Command)
	{
		ADeepGameMode* GameMode = FindGameMode(World);
		if (!GameMode)
		{
			UE_LOG(LogDeepCheats, Warning, TEXT("%s: needs the DEEP game mode on the server"), Command);
		}
		return GameMode;
	}

	FAutoConsoleCommandWithWorldAndArgs TitleCommand(
		TEXT("Deep.Title"),
		TEXT("Deep.Title: resets the run and shows the title screen (nobody dives until Deep.Start or any key)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ADeepGameMode* GameMode = RequireGameMode(World, TEXT("Deep.Title")))
			{
				GameMode->EnterTitle();
				DeepStateReport::LogState(World);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs StartCommand(
		TEXT("Deep.Start"),
		TEXT("Deep.Start: leaves the title screen; the first dive starts (as if a key was pressed)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ADeepGameMode* GameMode = RequireGameMode(World, TEXT("Deep.Start")))
			{
				GameMode->StartRun();
				DeepStateReport::LogState(World);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CreditsCommand(
		TEXT("Deep.Credits"),
		TEXT("Deep.Credits <n>: sets the company account (the next settlement buys a hull rating if this affords it)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
			if (!State || Args.Num() < 1)
			{
				UE_LOG(LogDeepCheats, Warning, TEXT("Usage: Deep.Credits <n> (needs the DEEP game state)"));
				return;
			}
			State->Credits = FMath::Max(0, FCString::Atoi(*Args[0]));
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs RelicCommand(
		TEXT("Deep.Relic"),
		TEXT("Deep.Relic: puts the diver 2 m from the relic, looking at it (stop 3; use Deep.GotoStop 3 first)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ADeepDiver* Diver = RequireDiver(World, TEXT("Deep.Relic"));
			const ADeepSalvage* Relic = DeepArena::FindRelic(World);
			if (!Diver || !Relic)
			{
				UE_LOG(LogDeepCheats, Warning, TEXT("Deep.Relic: no relic in the world (it lies at stop %d)"), UDeepTuningSettings::Get().Arena.RelicStop);
				return;
			}
			// Approach from the anchor's side so the line stays inside the tether; the eye level with the relic.
			const FVector Anchor = Diver->GetTether() && Diver->GetTether()->HasAnchor() ? Diver->GetTether()->GetAnchorLocation() : Diver->GetActorLocation();
			FVector Flat = Anchor - Relic->GetActorLocation();
			Flat.Z = 0.f;
			const FVector Back = Flat.IsNearlyZero() ? FVector(-1.f, 0.f, 0.f) : Flat.GetSafeNormal();
			const FVector At = Relic->GetActorLocation() + Back * 200.f - FVector(0.f, 0.f, 55.f);
			Diver->TeleportDiver(At);
			const FRotator View = (-Back).Rotation();
			Diver->SetActorRotation(FRotator(0.f, View.Yaw, 0.f));
			if (AController* Controller = Diver->GetController())
			{
				Controller->SetControlRotation(FRotator(0.f, View.Yaw, 0.f));
			}
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs GrabCommand(
		TEXT("Deep.Grab"),
		TEXT("Deep.Grab: the diver grabs what it is looking at (as the grab key)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ADeepDiver* Diver = RequireDiver(World, TEXT("Deep.Grab")))
			{
				Diver->Grab();
				DeepStateReport::LogState(World);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs WinchCommand(
		TEXT("Deep.Winch"),
		TEXT("Deep.Winch <0|1>: holds or releases the winch (as the winch key)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ADeepDiver* Diver = RequireDiver(World, TEXT("Deep.Winch")))
			{
				Diver->SetWinching(Args.Num() < 1 || FCString::Atoi(*Args[0]) != 0);
				DeepStateReport::LogState(World);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs WinCommand(
		TEXT("Deep.Win"),
		TEXT("Deep.Win: gives the diver the relic and banks it: straight to the performance review."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			FDeepSalvageKind Kind;
			if (!UDeepTuningSettings::Get().FindSalvageKind(EDeepSalvageSize::Relic, Kind))
			{
				UE_LOG(LogDeepCheats, Warning, TEXT("Deep.Win: no relic salvage kind is tuned"));
				return;
			}
			WithRun(World, TEXT("Deep.Win"), [&Kind](UDeepRunComponent& Run)
			{
				if (ADeepDiver* Diver = Cast<ADeepDiver>(Run.GetOwner()))
				{
					FDeepCargoItem Item;
					Item.Size = EDeepSalvageSize::Relic;
					Item.Kg = Kind.Kg;
					Item.Value = Kind.Value;
					Diver->AddCargo(Item);
				}
				Run.Bank();
			});
		}));


	/** Brings the diver aboard if it is still in the water (a cut diver pays the re-tether fee). */
	void AboardFirst(UDeepRunComponent& Run)
	{
		if (Run.CanSwim())
		{
			Run.EnterSub();
		}
	}

	FAutoConsoleCommandWithWorldAndArgs EnterSubCommand(
		TEXT("Deep.EnterSub"),
		TEXT("Deep.EnterSub: the diver comes up through the hatch onto the deck (as swimming up into it)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			WithRun(World, TEXT("Deep.EnterSub"), [](UDeepRunComponent& Run) { AboardFirst(Run); });
		}));

	FAutoConsoleCommandWithWorldAndArgs HatchCommand(
		TEXT("Deep.Hatch"),
		TEXT("Deep.Hatch: drops the diver through the hatch: swimming, tethered, the dive starts (needs the sub parked at a stop)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			WithRun(World, TEXT("Deep.Hatch"), [](UDeepRunComponent& Run)
			{
				Run.LeaveHelm();
				if (!Run.LeaveSub())
				{
					UE_LOG(LogDeepCheats, Warning, TEXT("Deep.Hatch: not aboard, or the hatch is locked (the sub is not parked at a stop)"));
				}
			});
		}));

	FAutoConsoleCommandWithWorldAndArgs ConsoleCommand(
		TEXT("Deep.Console"),
		TEXT("Deep.Console: uses the console (comes aboard first): banks cargo, or shows the account and buys the next gate if affordable."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			WithRun(World, TEXT("Deep.Console"), [](UDeepRunComponent& Run)
			{
				AboardFirst(Run);
				Run.LeaveHelm();
				Run.UseConsole();
			});
		}));

	FAutoConsoleCommandWithWorldAndArgs HelmCommand(
		TEXT("Deep.Helm"),
		TEXT("Deep.Helm [0|1]: takes (1, default) or leaves (0) the helm; comes aboard first."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const bool bTake = Args.Num() < 1 || FCString::Atoi(*Args[0]) != 0;
			WithRun(World, TEXT("Deep.Helm"), [bTake](UDeepRunComponent& Run)
			{
				AboardFirst(Run);
				if (bTake)
				{
					Run.TakeHelm();
				}
				else
				{
					Run.LeaveHelm();
				}
			});
		}));

	FAutoConsoleCommandWithWorldAndArgs DriveCommand(
		TEXT("Deep.Drive"),
		TEXT("Deep.Drive <metres|s1|s2|s3> [now]: takes the helm and drives there at the helm's speed (now: at once), parking at a stop. The limit applies."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogDeepCheats, Warning, TEXT("Usage: Deep.Drive <metres|s1|s2|s3> [now]"));
				return;
			}
			const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
			const FString Where = Args[0].ToLower();
			const float Target = Where.StartsWith(TEXT("s")) ? Tuning.GetStop(FCString::Atoi(*Where.Mid(1))).SubDepthM : FCString::Atof(*Where);
			const bool bNow = Args.Num() >= 2 && Args[1].ToLower() == TEXT("now");
			WithRun(World, TEXT("Deep.Drive"), [Target, bNow](UDeepRunComponent& Run)
			{
				AboardFirst(Run);
				Run.SetAutopilot(Target);
				if (bNow)
				{
					// Long steps until it arrives (the helm's own speed, just not in real time).
					for (int32 Step = 0; Step < 4000 && Run.IsAutopiloting(); ++Step)
					{
						Run.StepRun(0.25f);
					}
				}
			});
		}));

	FAutoConsoleCommandWithWorldAndArgs BuyGateCommand(
		TEXT("Deep.BuyGate"),
		TEXT("Deep.BuyGate [free]: buys the next layer gate (flashlight 50 cr, then hull upgrade 200 cr) if affordable, or free."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const bool bFree = Args.Num() >= 1 && Args[0].ToLower() == TEXT("free");
			WithRun(World, TEXT("Deep.BuyGate"), [bFree](UDeepRunComponent& Run)
			{
				if (!Run.BuyNextGate(bFree))
				{
					UE_LOG(LogDeepCheats, Warning, TEXT("Deep.BuyGate: nothing bought (not enough credits, or both gates owned)"));
				}
			});
		}));

	FAutoConsoleCommandWithWorldAndArgs MenuCommand(
		TEXT("Deep.Menu"),
		TEXT("Deep.Menu <0|1>: opens or closes the pause menu of the first local player."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ADeepPlayerController* Player = World ? Cast<ADeepPlayerController>(World->GetFirstPlayerController()) : nullptr;
			if (!Player)
			{
				UE_LOG(LogDeepCheats, Warning, TEXT("Deep.Menu: no local DEEP player controller"));
				return;
			}
			Player->SetPauseMenu(Args.Num() < 1 || FCString::Atoi(*Args[0]) != 0);
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs RopeStageCommand(
		TEXT("Deep.Rope.Stage"),
		TEXT("Deep.Rope.Stage <out_m> <down_m> [hatch|mid|diver] [yaw]: shots. Moves the diver out_m from the anchor (along yaw, default the view's) and down_m below it, then turns the view to the hatch, the rope's middle, or back along the line from the hatch (diver)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ADeepDiver* Diver = RequireDiver(World, TEXT("Deep.Rope.Stage"));
			UDeepTetherComponent* Tether = Diver ? Diver->GetTether() : nullptr;
			AController* Controller = Diver ? Diver->GetController() : nullptr;
			if (!Tether || !Controller || !Tether->HasAnchor())
			{
				return;
			}
			const float OutM = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 12.f;
			const float DownM = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 6.f;
			const FString Look = Args.Num() > 2 ? Args[2].ToLower() : FString(TEXT("hatch"));
			const float Yaw = Args.Num() > 3 ? FCString::Atof(*Args[3]) : Controller->GetControlRotation().Yaw;
			const FVector Anchor = Tether->GetAnchorLocation();
			Diver->TeleportDiver(Anchor + FRotator(0.f, Yaw, 0.f).Vector() * (OutM * 100.f) - FVector(0.f, 0.f, DownM * 100.f));
			const FVector Eye = Diver->GetPawnViewLocation();
			FVector Target = Anchor;
			if (Look == TEXT("mid"))
			{
				Target = (Anchor + Diver->GetTetherAttachLocation()) * 0.5 - FVector(0.f, 0.f, 100.f);
			}
			else if (Look == TEXT("diver"))
			{
				Target = Eye + (Eye - Anchor);
			}
			Controller->SetControlRotation((Target - Eye).Rotation());
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorld RopeLookCutCommand(
		TEXT("Deep.Rope.LookCut"),
		TEXT("Deep.Rope.LookCut: shots. Turns the view to where the tether parted (between the two loose ends)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			ADeepDiver* Diver = RequireDiver(World, TEXT("Deep.Rope.LookCut"));
			UDeepTetherComponent* Tether = Diver ? Diver->GetTether() : nullptr;
			const UDeepRopeComponent* Rope = Tether ? Tether->GetLineRope() : nullptr;
			AController* Controller = Diver ? Diver->GetController() : nullptr;
			if (!Rope || !Controller)
			{
				return;
			}
			const FDeepRopeSim& Sim = Rope->GetSim();
			for (int32 i = 0; i < Sim.NumLinks(); ++i)
			{
				if (!Sim.IsLinked(i))
				{
					const FVector Target = (Sim.Pos[i] + Sim.Pos[i + 1]) * 0.5;
					Controller->SetControlRotation((Target - Diver->GetPawnViewLocation()).Rotation());
					UE_LOG(LogDeepCheats, Display, TEXT("Deep.Rope.LookCut: link %d, %.1f m away"), i, FVector::Dist(Target, Diver->GetPawnViewLocation()) / 100.0);
					return;
				}
			}
			UE_LOG(LogDeepCheats, Warning, TEXT("Deep.Rope.LookCut: the rope is whole"));
		}));

	FAutoConsoleCommandWithWorld RopeStateCommand(
		TEXT("Deep.Rope.State"),
		TEXT("Deep.Rope.State: logs the tether rope (points, joined links, shown length, cut)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			ADeepDiver* Diver = RequireDiver(World, TEXT("Deep.Rope.State"));
			UDeepTetherComponent* Tether = Diver ? Diver->GetTether() : nullptr;
			const UDeepRopeComponent* Rope = Tether ? Tether->GetLineRope() : nullptr;
			if (!Rope)
			{
				return;
			}
			const FDeepRopeSim& Sim = Rope->GetSim();
			int32 Parted = INDEX_NONE;
			for (int32 i = 0; i < Sim.NumLinks(); ++i)
			{
				if (!Sim.IsLinked(i))
				{
					Parted = i;
					break;
				}
			}
			UE_LOG(LogDeepCheats, Display, TEXT("DEEPROPE {\"points\":%d,\"linked\":%d,\"links\":%d,\"parted\":%d,\"shown_m\":%.2f,\"visible\":%s}"),
				Sim.Pos.Num(), Sim.CountLinked(), Sim.NumLinks(), Parted, Tether->GetShownLengthCm() / 100.f, Rope->IsShown() ? TEXT("true") : TEXT("false"));
		}));
}
