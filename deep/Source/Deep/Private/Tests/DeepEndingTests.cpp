// Tests for the run's frame: the relic at stop 3, the win and its performance review, the title screen, restart,
// and a full run from the title to the win driven only by console commands.

#include "CoreMinimal.h"
#include "Debug/DeepStateReport.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Diver/DeepTetherComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "Misc/AutomationTest.h"
#include "Monster/DeepMonsterBrain.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepEventBus.h"
#include "World/DeepArena.h"
#include "World/DeepSalvage.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_ENDING_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepEndingTest
{
	struct FEndingWorld
	{
		UWorld* World = nullptr;
		ADeepGameMode* GameMode = nullptr;
		ADeepGameState* State = nullptr;
		ADeepSubmarine* Sub = nullptr;
		ADeepDiver* Diver = nullptr;
		UDeepRunComponent* Run = nullptr;
		UDeepEventBus* Bus = nullptr;

		FEndingWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			GameMode = World->SpawnActor<ADeepGameMode>();
			Sub = GameMode ? GameMode->EnsureArena() : nullptr;
			if (GameMode)
			{
				GameMode->StartPlay();
			}
			State = World->GetGameState<ADeepGameState>();
			Bus = UDeepEventBus::Get(World);
			if (Sub)
			{
				Diver = World->SpawnActor<ADeepDiver>(Sub->GetDiverStartTransform().GetLocation(), FRotator::ZeroRotator);
			}
			Run = Diver ? Diver->GetRun() : nullptr;
		}

		~FEndingWorld()
		{
			if (World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		bool Valid() const { return GameMode && State && Sub && Diver && Run && Bus; }

		void Cmd(const TCHAR* Command) const
		{
			GEngine->Exec(World, Command);
		}

		/** Steps the settlement lock and continues: back on the deck. */
		void ContinueToDeck() const
		{
			const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
			Run->StepRun(Tuning.Run.SettlementMinSeconds + 0.05f);
			Cmd(TEXT("Deep.Continue"));
		}

		/** Holds the winch and runs tether and loop steps until the dive ends (or MaxSeconds). */
		void WinchHome(float MaxSeconds = 40.f) const
		{
			Diver->SetWinching(true);
			for (float T = 0.f; T < MaxSeconds && Run->GetState() == EDeepRunState::Tethered; T += 0.1f)
			{
				Diver->GetTether()->UpdateTether(0.1f);
				Run->StepRun(0.1f);
			}
		}

		int32 CountSalvage() const
		{
			int32 N = 0;
			for (TActorIterator<ADeepSalvage> It(World); It; ++It)
			{
				N += IsValid(*It) ? 1 : 0;
			}
			return N;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRulesReviewTest, "Deep.Sim.Rules.ReviewAndRelic", DEEP_ENDING_TEST_FLAGS)
bool FDeepRulesReviewTest::RunTest(const FString& Parameters)
{
	FDeepReviewTuning R;
	R.MaxDiversExpended = { 0, 2 };
	R.Words = { TEXT("A"), TEXT("B"), TEXT("C") };
	R.SlowRunSeconds = 600.f;
	TestEqual(TEXT("No divers lost, quick: the best word"), UDeepRules::ReviewRating(0, 100.f, R), FString(TEXT("A")));
	TestEqual(TEXT("Two lost: the second word"), UDeepRules::ReviewRating(2, 100.f, R), FString(TEXT("B")));
	TestEqual(TEXT("Past the last step: the last word"), UDeepRules::ReviewRating(9, 100.f, R), FString(TEXT("C")));
	TestEqual(TEXT("Slow drops one word"), UDeepRules::ReviewRating(0, 601.f, R), FString(TEXT("B")));
	TestEqual(TEXT("Never past the last word"), UDeepRules::ReviewRating(9, 601.f, R), FString(TEXT("C")));
	FDeepReviewTuning NoWords;
	NoWords.Words.Reset();
	TestTrue(TEXT("No words, no rating"), UDeepRules::ReviewRating(0, 0.f, NoWords).IsEmpty());
	TestEqual(TEXT("The default best word"), UDeepRules::ReviewRating(0, 0.f, FDeepReviewTuning()), FString(TEXT("ADEQUATE")));

	const FDeepRunReview Review = UDeepRules::BuildReview(3, 1000, 400, 150, 321.f, R);
	TestEqual(TEXT("Kept is gross minus cut minus fees"), Review.Kept, 450);
	TestEqual(TEXT("Divers expended carried"), Review.DiversExpended, 3);
	TestEqual(TEXT("Rating from the rule"), Review.Rating, FString(TEXT("C")));
	TestEqual(TEXT("Kept never below zero"), UDeepRules::BuildReview(0, 10, 4, 50, 0.f, R).Kept, 0);

	// The relic stays inside the tether's reach even when the wanted depth is past it.
	FDeepArenaTuning Arena;
	Arena.RelicDepthM = 600.f;
	Arena.RelicOutM = 8.f;
	Arena.RelicReachMarginM = 3.f;
	const FVector Offset = UDeepRules::RelicOffsetCm(Arena, 500.f, 80.f);
	TestTrue(TEXT("Relic within tether minus margin"), Offset.Size() / UDeepRules::CmPerM <= 77.01f);
	TestTrue(TEXT("Relic is deep below the anchor"), -Offset.Z / UDeepRules::CmPerM > 70.f);
	const FVector Shallow = UDeepRules::RelicOffsetCm(Arena, 560.f, 80.f);
	TestEqual(TEXT("When the tether allows, exactly at RelicDepthM"), static_cast<float>(-Shallow.Z / UDeepRules::CmPerM), 40.f, 0.01f);

	// relic_seen: near, or looked at from far enough.
	Arena.RelicSeenNearM = 20.f;
	Arena.RelicSeenViewM = 50.f;
	Arena.RelicSeenConeDeg = 25.f;
	const FVector Relic(0.f, 0.f, 0.f);
	TestTrue(TEXT("Near counts even looking away"), UDeepRules::IsRelicSeen(FVector(1000.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), Relic, Arena));
	TestTrue(TEXT("Looking at it from 40 m"), UDeepRules::IsRelicSeen(FVector(4000.f, 0.f, 0.f), FVector(-1.f, 0.f, 0.f), Relic, Arena));
	TestFalse(TEXT("Looking away from 40 m"), UDeepRules::IsRelicSeen(FVector(4000.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), Relic, Arena));
	TestFalse(TEXT("Too far to see"), UDeepRules::IsRelicSeen(FVector(6000.f, 0.f, 0.f), FVector(-1.f, 0.f, 0.f), Relic, Arena));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRelicWinTest, "Deep.World.RelicWin", DEEP_ENDING_TEST_FLAGS)
bool FDeepRelicWinTest::RunTest(const FString& Parameters)
{
	using namespace DeepEndingTest;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FEndingWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus"), W.Valid());
	W.Run->StepRun(0.05f);
	TestFalse(TEXT("No relic at stop 1"), DeepArena::FindRelic(W.World) != nullptr);

	// One ordinary bank at stop 1 counts toward the run totals.
	FDeepCargoItem Scrap;
	Scrap.Size = EDeepSalvageSize::Medium;
	Scrap.Kg = 6.f;
	Scrap.Value = 70;
	W.Diver->AddCargo(Scrap);
	W.Run->Bank();
	const FDeepSettlementReport First = W.Run->GetSettlement();
	W.ContinueToDeck();

	// Stop 3: the relic lies deep, inside the tether's reach.
	W.GameMode->GotoStop(3);
	const ADeepSalvage* Relic = DeepArena::FindRelic(W.World);
	UTEST_NOT_NULL(TEXT("The relic lies at stop 3"), Relic);
	const FVector Anchor = W.Sub->GetAnchorLocation();
	const float RelicDistanceM = static_cast<float>(FVector::Dist(Relic->GetActorLocation(), Anchor)) / UDeepRules::CmPerM;
	TestTrue(TEXT("The relic is inside the stop-3 tether"), RelicDistanceM < UDeepRules::TetherLengthM(3, Tuning.Tether));
	const float RelicDepthM = Tuning.GetStop(3).SubDepthM + static_cast<float>(Anchor.Z - Relic->GetActorLocation().Z) / UDeepRules::CmPerM;
	TestTrue(FString::Printf(TEXT("The relic is near the bottom (%.0f m)"), RelicDepthM), RelicDepthM > 560.f && RelicDepthM <= Tuning.Arena.RelicDepthM + 0.1f);

	// A death on the way costs a fee and a diver.
	W.Cmd(TEXT("Deep.Hatch"));
	W.Diver->TeleportDiver(W.Sub->GetDiverStartTransform().GetLocation());
	W.Run->SetPatienceFraction(0.f);
	W.Run->StepRun(0.05f);
	W.Run->StepRun(60.f);
	TestEqual(TEXT("Diver written off"), W.Run->GetState(), EDeepRunState::Dead);
	W.Run->StepRun(Tuning.Run.DeathScreenSeconds + 0.1f);
	TestNotNull(TEXT("The relic is back for the replacement diver"), DeepArena::FindRelic(W.World));

	// Seen, grabbed (too heavy to swim), winched (loud) up to the hatch, banked at the console: the run is won.
	W.Cmd(TEXT("Deep.Hatch"));
	W.Cmd(TEXT("Deep.Relic"));
	W.Run->StepRun(0.05f);
	TestEqual(TEXT("relic_seen fired"), W.Bus->CountOf(DeepEvent::RelicSeen), 1);
	W.Diver->Grab();
	TestTrue(TEXT("Carrying the relic"), W.Run->CarriesRelic());
	TestEqual(TEXT("relic_grabbed fired"), W.Bus->CountOf(DeepEvent::RelicGrabbed), 1);
	TestTrue(TEXT("The relic is too heavy to swim up"), W.Diver->GetWeight().bOverloaded);
	W.WinchHome();
	TestEqual(TEXT("relic_winch fired"), W.Bus->CountOf(DeepEvent::RelicWinch), 1);
	TestEqual(TEXT("The winch brings the relic aboard"), W.Run->GetState(), EDeepRunState::Aboard);
	TestTrue(TEXT("Still carrying it aboard"), W.Run->CarriesRelic());
	W.Cmd(TEXT("Deep.Console"));
	TestEqual(TEXT("Banking the relic at the console wins"), W.Run->GetState(), EDeepRunState::Won);
	TestTrue(TEXT("Game state says won"), W.State->bWon);
	TestEqual(TEXT("win fired"), W.Bus->CountOf(DeepEvent::Win), 1);
	TestFalse(TEXT("The diver cannot swim on the review"), W.Run->CanSwim());

	// The review's totals: both banks, the death fee, one diver expended.
	const FDeepSettlementReport Last = W.Run->GetSettlement();
	FDeepSalvageKind Kind;
	Tuning.FindSalvageKind(EDeepSalvageSize::Relic, Kind);
	const FDeepRunReview Review = W.State->BuildReview();
	TestEqual(TEXT("Relic settled at its value"), Last.Gross, Kind.Value);
	TestEqual(TEXT("Total gross"), Review.Gross, First.Gross + Last.Gross);
	TestEqual(TEXT("Total cut"), Review.DirectorCut, First.DirectorCut + Last.DirectorCut);
	TestEqual(TEXT("Total fees: settlements and the death fee"), Review.Fees, First.Fees + Last.Fees + Tuning.Economy.DeathFee);
	TestEqual(TEXT("Kept"), Review.Kept, Review.Gross - Review.DirectorCut - Review.Fees);
	TestEqual(TEXT("One diver expended"), Review.DiversExpended, 1);
	TestEqual(TEXT("Rating from the rule"), Review.Rating, UDeepRules::ReviewRating(1, W.State->RunSeconds, Tuning.Review));

	// The review ignores keys at first; the game state does not count time after the win.
	TestFalse(TEXT("Review locked at first"), W.Run->CanLeaveReview());
	W.Run->StepRun(Tuning.Run.ReviewMinSeconds + 0.05f);
	TestTrue(TEXT("Then it can be left"), W.Run->CanLeaveReview());
	const float Before = W.State->RunSeconds;
	W.State->Tick(5.f);
	TestEqual(TEXT("Time stops at the win"), W.State->RunSeconds, Before, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRestartTest, "Deep.World.RestartResets", DEEP_ENDING_TEST_FLAGS)
bool FDeepRestartTest::RunTest(const FString& Parameters)
{
	using namespace DeepEndingTest;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FEndingWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus"), W.Valid());
	W.Run->StepRun(0.05f);
	const FVector Start = W.Sub->GetDiverStartTransform().GetLocation();
	W.State->SubDepthM = 333.f;
	W.State->bParked = false;

	// Make a mess of everything.
	W.GameMode->GotoStop(3);
	W.State->Credits = 999;
	W.State->HullRating = 3;
	W.State->DiverNumber = 5;
	W.State->TotalGross = 1234;
	W.State->TotalDirectorCut = 500;
	W.State->TotalFees = 77;
	W.State->RunSeconds = 300.f;
	FDeepCargoItem Heavy;
	Heavy.Size = EDeepSalvageSize::Large;
	Heavy.Kg = 15.f;
	Heavy.Value = 200;
	W.Diver->AddCargo(Heavy);
	W.Diver->TeleportDiver(Start - FVector(0.f, 0.f, 3000.f));
	UDeepMonsterBrain* Brain = W.World->GetSubsystem<UDeepMonsterBrain>();
	UTEST_NOT_NULL(TEXT("Monster brain"), Brain);
	Brain->SpawnSerpent(true);
	TestNotNull(TEXT("Serpent spawned"), Brain->GetSerpent());
	W.Run->CutTether(false);
	TestEqual(TEXT("Cut before the restart"), W.Run->GetState(), EDeepRunState::Cut);

	W.Cmd(TEXT("Deep.ResetRun"));
	TestEqual(TEXT("Credits back to the start"), W.State->Credits, FMath::Max(0, Tuning.Economy.StartingCredits));
	TestEqual(TEXT("Hull rating 1"), W.State->HullRating, 1);
	TestEqual(TEXT("Stop 1"), W.State->Stop, 1);
	TestEqual(TEXT("Diver 1"), W.State->DiverNumber, 1);
	TestEqual(TEXT("Totals cleared: gross"), W.State->TotalGross, 0);
	TestEqual(TEXT("Totals cleared: cut"), W.State->TotalDirectorCut, 0);
	TestEqual(TEXT("Totals cleared: fees"), W.State->TotalFees, 0);
	TestEqual(TEXT("Run time cleared"), W.State->RunSeconds, 0.f, 0.001f);
	TestFalse(TEXT("Not won"), W.State->bWon);
	TestFalse(TEXT("Not on the title"), W.State->bOnTitle);
	TestEqual(TEXT("Cargo empty"), W.Diver->GetCargo().Num(), 0);
	TestEqual(TEXT("Aboard again"), W.Run->GetState(), EDeepRunState::Aboard);
	TestEqual(TEXT("Full Patience"), W.Run->GetPatienceFraction(), 1.f, 0.0001f);
	TestEqual(TEXT("No bailout air running"), W.Run->GetBailoutAirSeconds(), 0.f, 0.0001f);
	TestNull(TEXT("The serpent is gone"), Brain->GetSerpent());
	TestNull(TEXT("The relic is gone with stop 3"), DeepArena::FindRelic(W.World));
	TestEqual(TEXT("Fresh salvage ring"), W.CountSalvage(), Tuning.Arena.SalvageSpots.Num());
	TestTrue(TEXT("Diver back on the deck"), FVector::Dist(W.Diver->GetActorLocation(), W.Sub->GetRoomStartTransform().GetLocation()) < 50.0);
	TestEqual(TEXT("The sub back at the first stop"), W.State->SubDepthM, Tuning.GetStop(1).SubDepthM, 0.01f);
	W.Cmd(TEXT("Deep.Hatch"));
	W.Diver->GetTether()->UpdateTether(0.f);
	TestEqual(TEXT("Tether back to the stop-1 length"), W.Diver->GetTether()->GetSolve().LengthM, UDeepRules::TetherLengthM(1, Tuning.Tether), 0.01f);

	// Restart from the win screen (dive again) works the same way.
	W.Cmd(TEXT("Deep.Win"));
	TestEqual(TEXT("Won"), W.Run->GetState(), EDeepRunState::Won);
	W.GameMode->RestartRun();
	TestEqual(TEXT("Dive again: aboard"), W.Run->GetState(), EDeepRunState::Aboard);
	TestFalse(TEXT("Dive again: not won"), W.State->bWon);
	TestEqual(TEXT("Dive again: totals cleared"), W.State->TotalGross, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepFullRunTest, "Deep.World.FullRunCommands", DEEP_ENDING_TEST_FLAGS)
bool FDeepFullRunTest::RunTest(const FString& Parameters)
{
	using namespace DeepEndingTest;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FEndingWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus"), W.Valid());

	// The title: nobody dives, Patience holds; the diver waits aboard.
	W.Cmd(TEXT("Deep.Title"));
	TestTrue(TEXT("On the title"), W.State->bOnTitle);
	TestTrue(TEXT("title fired"), W.Bus->CountOf(DeepEvent::Title) >= 1);
	W.Bus->ClearHistory();
	W.Run->StepRun(5.f);
	TestFalse(TEXT("No swimming on the title"), W.Run->CanSwim());
	TestEqual(TEXT("Patience holds on the title"), W.Run->GetPatienceFraction(), 1.f, 0.0001f);
	TestEqual(TEXT("No dive_start yet"), W.Bus->CountOf(DeepEvent::DiveStart), 0);

	// Any key (Deep.Start): the run starts aboard, on the deck.
	W.Cmd(TEXT("Deep.Start"));
	TestFalse(TEXT("Title left"), W.State->bOnTitle);
	TestEqual(TEXT("Aboard at the start"), W.Run->GetState(), EDeepRunState::Aboard);
	TestTrue(TEXT("In the room"), W.Sub->IsInRoom(W.Diver->GetActorLocation()));
	TestFalse(TEXT("No lamp yet"), W.Diver->IsLampOn());

	// Stop 1: through the hatch, a haul, up through the hatch, bank at the console: the flashlight.
	W.Cmd(TEXT("Deep.Hatch"));
	TestEqual(TEXT("dive_start fired"), W.Bus->CountOf(DeepEvent::DiveStart), 1);
	TestTrue(TEXT("Diving"), W.Run->CanSwim());
	W.Cmd(TEXT("Deep.GiveSalvage 6 70"));
	W.Cmd(TEXT("Deep.EnterSub"));
	TestEqual(TEXT("enter_sub fired"), W.Bus->CountOf(DeepEvent::EnterSub), 1);
	W.Cmd(TEXT("Deep.Credits 60"));
	W.Cmd(TEXT("Deep.Console"));
	TestEqual(TEXT("Settling"), W.Run->GetState(), EDeepRunState::Settling);
	TestTrue(TEXT("Flashlight bought"), W.State->HasFlashlight());
	TestFalse(TEXT("Hull not yet"), W.State->IsHullUpgraded());
	TestTrue(TEXT("The lamp works now"), W.Diver->IsLampOn());
	W.ContinueToDeck();
	TestEqual(TEXT("Back on the deck"), W.Run->GetState(), EDeepRunState::Aboard);

	// The helm: down to stop 2, then the 400 m limit refuses stop 3.
	W.Cmd(TEXT("Deep.Drive s2 now"));
	TestTrue(TEXT("Seated"), W.Run->IsSeated());
	TestEqual(TEXT("helm_take fired"), W.Bus->CountOf(DeepEvent::HelmTake), 1);
	TestEqual(TEXT("Stop 2"), W.State->ParkedStop(), 2);
	TestEqual(TEXT("descend_stop2 fired"), W.Bus->CountOf(DeepEvent::DescendStop2), 1);
	W.Cmd(TEXT("Deep.Drive s3 now"));
	TestEqual(TEXT("Stopped at the 400 m limit"), W.State->SubDepthM, 400.f, 0.01f);
	TestTrue(TEXT("helm_depth_limit fired"), W.Bus->CountOf(DeepEvent::HelmDepthLimit) >= 1);
	TestEqual(TEXT("Not parked between stops"), W.State->ParkedStop(), 0);
	W.Cmd(TEXT("Deep.Helm 0"));
	W.Cmd(TEXT("Deep.Hatch"));
	TestEqual(TEXT("The hatch is locked between stops"), W.Run->GetState(), EDeepRunState::Aboard);
	W.Cmd(TEXT("Deep.Drive s2 now"));
	TestEqual(TEXT("Back at stop 2"), W.State->ParkedStop(), 2);
	TestTrue(TEXT("helm_arrive fired on the return"), W.Bus->CountOf(DeepEvent::HelmArrive) >= 1);

	// Stop 2: a dive and a bank with credits for the hull upgrade.
	W.Cmd(TEXT("Deep.Hatch"));
	TestEqual(TEXT("Stop 2: diving"), W.Run->GetState(), EDeepRunState::Tethered);
	W.Cmd(TEXT("Deep.GiveSalvage 2 20"));
	W.Cmd(TEXT("Deep.Credits 250"));
	W.Cmd(TEXT("Deep.Console"));
	TestTrue(TEXT("Hull upgraded"), W.State->IsHullUpgraded());
	W.ContinueToDeck();
	W.Cmd(TEXT("Deep.Drive s3 now"));
	TestEqual(TEXT("Stop 3"), W.State->ParkedStop(), 3);
	TestEqual(TEXT("descend_stop3 fired"), W.Bus->CountOf(DeepEvent::DescendStop3), 1);

	// Stop 3: the relic, the winch to the hatch, the console, the win.
	W.Cmd(TEXT("Deep.Hatch"));
	W.Diver->GetTether()->UpdateTether(0.f);
	TestEqual(TEXT("Stop-3 tether"), W.Diver->GetTether()->GetSolve().LengthM, UDeepRules::TetherLengthM(3, Tuning.Tether), 0.01f);
	W.Cmd(TEXT("Deep.Relic"));
	W.Run->StepRun(0.05f);
	TestEqual(TEXT("relic_seen fired"), W.Bus->CountOf(DeepEvent::RelicSeen), 1);
	W.Cmd(TEXT("Deep.Grab"));
	TestTrue(TEXT("Carrying the relic"), W.Run->CarriesRelic());
	W.Cmd(TEXT("Deep.Winch 1"));
	W.WinchHome();
	TestEqual(TEXT("Winched up through the hatch"), W.Run->GetState(), EDeepRunState::Aboard);
	W.Cmd(TEXT("Deep.Console"));
	TestEqual(TEXT("Won"), W.Run->GetState(), EDeepRunState::Won);
	TestEqual(TEXT("win fired"), W.Bus->CountOf(DeepEvent::Win), 1);

	TSharedPtr<FJsonObject> Json;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(DeepStateReport::BuildJson(W.World)), Json);
	UTEST_TRUE(TEXT("DEEPSTATE parses"), Json.IsValid());
	TestTrue(TEXT("DEEPSTATE won"), Json->GetBoolField(TEXT("won")));
	TestEqual(TEXT("DEEPSTATE run_state"), Json->GetStringField(TEXT("run_state")), FString(TEXT("won")));
	TestTrue(TEXT("DEEPSTATE inside"), Json->GetBoolField(TEXT("inside")));
	TestFalse(TEXT("DEEPSTATE seated"), Json->GetBoolField(TEXT("seated")));
	TestEqual(TEXT("DEEPSTATE sub_depth_m"), static_cast<float>(Json->GetNumberField(TEXT("sub_depth_m"))), Tuning.GetStop(3).SubDepthM, 0.01f);
	TestEqual(TEXT("DEEPSTATE depth_limit_m"), static_cast<float>(Json->GetNumberField(TEXT("depth_limit_m"))), 600.f, 0.01f);
	TestTrue(TEXT("DEEPSTATE has_flashlight"), Json->GetBoolField(TEXT("has_flashlight")));
	TestTrue(TEXT("DEEPSTATE hull_upgraded"), Json->GetBoolField(TEXT("hull_upgraded")));
	TestEqual(TEXT("DEEPSTATE parked_stop"), static_cast<int32>(Json->GetNumberField(TEXT("parked_stop"))), 3);
	TestTrue(TEXT("DEEPSTATE run_totals"), Json->HasField(TEXT("run_totals")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
