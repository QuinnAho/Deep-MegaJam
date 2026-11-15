// Tests for the run loop and the Captain: fee rules, CaptainLines.csv picking, the subtitle queue, and a world test
// that drives UDeepRunComponent through cut, bailout, death, respawn, re-tether, bank, settlement and descent.

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
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepCaptain.h"
#include "Story/DeepCaptainLines.h"
#include "Story/DeepEventBus.h"
#include "World/DeepSalvage.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_RUN_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepRunTest
{
	struct FRunWorld
	{
		UWorld* World = nullptr;

		FRunWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}

		~FRunWorld()
		{
			if (World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};

	int32 CountSalvage(UWorld* World)
	{
		int32 N = 0;
		for (TActorIterator<ADeepSalvage> It(World); It; ++It)
		{
			N += IsValid(*It) ? 1 : 0;
		}
		return N;
	}

	FDeepCargoItem Item(EDeepSalvageSize Size, float Kg, int32 Value)
	{
		FDeepCargoItem Out;
		Out.Size = Size;
		Out.Kg = Kg;
		Out.Value = Value;
		return Out;
	}

	const TCHAR* SampleCsv = TEXT(
		"\"id\",\"event\",\"variant\",\"text\",\"voice_note\"\n"
		"\"a_01\",\"alpha\",1,\"One, with a comma.\",\"bored\"\n"
		"\"a_02\",\"alpha\",2,\"Two.\",\"bored\"\n"
		"\"a_03\",\"alpha\",3,\"Three.\",\"bored\"\n"
		"\"b_01\",\"bank\",1,\"{gross} gross, {cut} cut, {net} net, diver {n} at {depth} m.\",\"smug\"\n"
		"\"c_01\",\"chatter\",1,\"Chatter.\",\"bored\"\n"
		"\"u_01\",\"cut_patience\",1,\"Urgent.\",\"gleeful\"\n");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRulesFeesTest, "Deep.Sim.Rules.Fees", DEEP_RUN_TEST_FLAGS)
bool FDeepRulesFeesTest::RunTest(const FString& Parameters)
{
	FDeepEconomyTuning E;
	E.FeePercent = 10;
	E.FeeFlat = 5;
	E.GoodBankNet = 50;
	TestEqual(TEXT("10% of 70 rounds up to 7, plus 5"), UDeepRules::SettlementFeeTotal(70, E), 12);
	TestEqual(TEXT("10% of 71 rounds up to 8, plus 5"), UDeepRules::SettlementFeeTotal(71, E), 13);
	TestEqual(TEXT("The flat fee is charged on nothing"), UDeepRules::SettlementFeeTotal(0, E), 5);
	TestEqual(TEXT("Negative gross counts as zero"), UDeepRules::SettlementFeeTotal(-40, E), 5);

	for (int32 Seed = 0; Seed < 40; ++Seed)
	{
		for (const int32 Total : { 0, 1, 2, 3, 12, 57, 999 })
		{
			const int32 Lines = 1 + Seed % 4;
			const TArray<int32> Split = UDeepRules::SplitFee(Total, Lines, Seed);
			int32 Sum = 0;
			bool bAllNonNegative = true;
			bool bAllPositive = true;
			for (const int32 Amount : Split)
			{
				Sum += Amount;
				bAllNonNegative &= Amount >= 0;
				bAllPositive &= Amount >= 1;
			}
			if (Sum != Total || Split.Num() != Lines || !bAllNonNegative || (Total >= Lines && !bAllPositive))
			{
				AddError(FString::Printf(TEXT("SplitFee(%d, %d, %d) gave %d lines summing to %d"), Total, Lines, Seed, Split.Num(), Sum));
			}
		}
	}
	TestEqual(TEXT("Zero lines still gives one line"), UDeepRules::SplitFee(10, 0, 1).Num(), 1);
	TestTrue(TEXT("Same seed, same split"), UDeepRules::SplitFee(57, 3, 9) == UDeepRules::SplitFee(57, 3, 9));

	TestEqual(TEXT("Nothing banked is empty"), UDeepRules::BankMood(0, 0, E), EDeepBankMood::Empty);
	TestEqual(TEXT("Small net is poor"), UDeepRules::BankMood(70, 30, E), EDeepBankMood::Poor);
	TestEqual(TEXT("Net at the threshold is good"), UDeepRules::BankMood(120, 50, E), EDeepBankMood::Good);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRulesWinchToAirlockTest, "Deep.Sim.Rules.WinchToAirlock", DEEP_RUN_TEST_FLAGS)
bool FDeepRulesWinchToAirlockTest::RunTest(const FString& Parameters)
{
	FDeepTetherTuning T;
	T.WinchSpeedMps = 7.f;
	T.WinchArriveM = 3.f;
	T.WinchApproachBelowM = 2.f;
	const FVector Anchor(0.f, 0.f, 0.f);

	// From every side at airlock height, and from above and below: home within the time the distance allows.
	const TArray<FVector> Starts = { FVector(900, 0, 0), FVector(-900, 0, 0), FVector(0, 900, 0), FVector(600, 600, 300), FVector(0, 0, -4000), FVector(0, 0, 800) };
	for (const FVector& Start : Starts)
	{
		FVector At = Start;
		float MaxZ = static_cast<float>(At.Z);
		int32 Steps = 0;
		while (!UDeepRules::IsHome(At, Anchor, T) && Steps < 1000)
		{
			At = UDeepRules::WinchStepToAirlock(At, Anchor, 0.1f, T);
			MaxZ = FMath::Max(MaxZ, static_cast<float>(At.Z));
			++Steps;
		}
		const float Allowed = static_cast<float>(FVector::Dist(Start, Anchor - FVector(0, 0, 200.f))) / (T.WinchSpeedMps * 100.f) + 0.2f;
		TestTrue(FString::Printf(TEXT("Home from %s"), *Start.ToString()), UDeepRules::IsHome(At, Anchor, T));
		TestTrue(FString::Printf(TEXT("In time from %s"), *Start.ToString()), Steps * 0.1f <= Allowed);
		if (Start.Z <= 0.f)
		{
			TestTrue(FString::Printf(TEXT("Never rises above the airlock from %s"), *Start.ToString()), MaxZ <= 0.f);
		}
	}
	const FVector Home(0.f, 0.f, -250.f);
	TestTrue(TEXT("Home does not move"), UDeepRules::WinchStepToAirlock(Home, Anchor, 1.f, T).Equals(Home));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaptainLinesTest, "Deep.Story.CaptainLines", DEEP_RUN_TEST_FLAGS)
bool FDeepCaptainLinesTest::RunTest(const FString& Parameters)
{
	FDeepCaptainLines Lines;
	TestEqual(TEXT("Parses every row, commas inside quotes included"), Lines.LoadFromString(DeepRunTest::SampleCsv), 6);
	TestEqual(TEXT("Three alpha variants"), Lines.NumVariants(TEXT("alpha")), 3);

	FRandomStream Rng(42);
	FDeepCaptainLine Line;
	FString Previous;
	bool bRepeated = false;
	TSet<FString> Seen;
	for (int32 Pick = 0; Pick < 60; ++Pick)
	{
		TestTrue(TEXT("Picks a line"), Lines.Pick(TEXT("alpha"), Rng, Line));
		bRepeated |= Line.Text == Previous;
		Previous = Line.Text;
		Seen.Add(Line.Text);
	}
	TestFalse(TEXT("Never the same variant twice in a row"), bRepeated);
	TestEqual(TEXT("Every variant gets used"), Seen.Num(), 3);
	TestEqual(TEXT("The comma survived"), Lines.NumVariants(TEXT("alpha")) > 0 && Seen.Contains(TEXT("One, with a comma.")), true);
	TestTrue(TEXT("Single-variant event repeats fine"), Lines.Pick(TEXT("chatter"), Rng, Line) && Lines.Pick(TEXT("chatter"), Rng, Line));
	TestFalse(TEXT("Unknown event has no line"), Lines.Pick(TEXT("nope"), Rng, Line));

	FDeepEventPayload Payload;
	Payload.Gross = 70;
	Payload.Cut = 28;
	Payload.Net = 30;
	Payload.DiverNumber = 3;
	Payload.DepthM = 123.6f;
	Lines.Pick(TEXT("bank"), Rng, Line);
	TestEqual(TEXT("Tokens filled"), FDeepCaptainLines::FillTokens(Line.Text, Payload), FString(TEXT("70 gross, 28 cut, 30 net, diver 3 at 124 m.")));

	// The shipped file: loads, and has a line for every event the game fires.
	FDeepCaptainLines Shipped;
	TestTrue(TEXT("CaptainLines.csv loads from Content/Data"), Shipped.LoadDefault() > 50);
	for (const FName& Event : DeepEvent::AllFired())
	{
		TestTrue(FString::Printf(TEXT("CaptainLines.csv has %s"), *Event.ToString()), Shipped.HasEvent(Event));
	}
	for (const float Fraction : UDeepTuningSettings::Get().Patience.WarningFractions)
	{
		const FString Event = FString::Printf(TEXT("patience_%d"), FMath::RoundToInt(Fraction * 100.f));
		TestTrue(FString::Printf(TEXT("CaptainLines.csv has %s for a tuned warning"), *Event), Shipped.HasEvent(FName(*Event)));
	}
	TestTrue(TEXT("SettlementFees.csv gives several labels"), DeepData::FeeLabels().Num() >= 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaptainQueueTest, "Deep.Story.CaptainQueue", DEEP_RUN_TEST_FLAGS)
bool FDeepCaptainQueueTest::RunTest(const FString& Parameters)
{
	DeepRunTest::FRunWorld Scoped;
	UDeepCaptain* Captain = UDeepCaptain::Get(Scoped.World);
	UTEST_NOT_NULL(TEXT("The Captain is a world subsystem"), Captain);
	UTEST_NOT_NULL(TEXT("So is the event bus"), UDeepEventBus::Get(Scoped.World));
	Captain->GetLines().LoadFromString(DeepRunTest::SampleCsv);
	const FDeepEventPayload Payload;

	TestTrue(TEXT("Silent at first"), Captain->GetLineAt(0.0).IsEmpty());
	Captain->HandleEventAt(TEXT("chatter"), Payload, 1.0);
	TestEqual(TEXT("Shows with the CAPTAIN: prefix"), Captain->GetLineAt(1.1), FString(TEXT("CAPTAIN: Chatter.")));
	Captain->HandleEventAt(TEXT("cut_patience"), Payload, 1.5);
	TestEqual(TEXT("A more urgent line replaces it"), Captain->GetLineAt(1.6), FString(TEXT("CAPTAIN: Urgent.")));
	Captain->HandleEventAt(TEXT("chatter"), Payload, 2.0);
	TestEqual(TEXT("A less urgent line waits"), Captain->GetLineAt(2.1), FString(TEXT("CAPTAIN: Urgent.")));
	TestEqual(TEXT("Then shows when the urgent one ends"), Captain->GetLineAt(1.5 + 3.0), FString(TEXT("CAPTAIN: Chatter.")));
	TestTrue(TEXT("And then silence"), Captain->GetLineAt(100.0).IsEmpty());

	// The bus reaches the Captain.
	UDeepEventBus::Get(Scoped.World)->Broadcast(TEXT("cut_patience"), Payload);
	TestEqual(TEXT("Bus events reach the Captain"), Captain->GetLastText(), FString(TEXT("Urgent.")));
	TestTrue(TEXT("Death outranks chatter"), UDeepCaptain::Urgency(DeepEvent::DeathAir) > UDeepCaptain::Urgency(DeepEvent::SalvageSmall));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRunLoopTest, "Deep.World.RunLoop", DEEP_RUN_TEST_FLAGS)
bool FDeepRunLoopTest::RunTest(const FString& Parameters)
{
	using namespace DeepRunTest;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();

	FRunWorld Scoped;
	UWorld* World = Scoped.World;
	ADeepGameMode* GameMode = World->SpawnActor<ADeepGameMode>();
	UTEST_NOT_NULL(TEXT("Game mode"), GameMode);
	ADeepSubmarine* Sub = GameMode->EnsureArena();
	UTEST_NOT_NULL(TEXT("Sub"), Sub);
	GameMode->StartPlay();
	ADeepGameState* State = World->GetGameState<ADeepGameState>();
	UTEST_NOT_NULL(TEXT("Game state"), State);
	UDeepEventBus* Bus = UDeepEventBus::Get(World);
	UTEST_NOT_NULL(TEXT("Event bus"), Bus);

	ADeepDiver* Diver = World->SpawnActor<ADeepDiver>(Sub->GetDiverStartTransform().GetLocation(), FRotator::ZeroRotator);
	UTEST_NOT_NULL(TEXT("Diver"), Diver);
	UDeepRunComponent* Run = Diver->GetRun();
	UTEST_NOT_NULL(TEXT("Run component"), Run);
	const FVector Anchor = Sub->GetAnchorLocation();
	const FVector Start = Sub->GetDiverStartTransform().GetLocation();

	// A dive starts tethered with full Patience.
	TestEqual(TEXT("Starts tethered"), Run->GetState(), EDeepRunState::Tethered);
	TestEqual(TEXT("Starts with full Patience"), Run->GetPatienceFraction(), 1.f, 0.0001f);
	TestEqual(TEXT("dive_start fired"), Bus->CountOf(DeepEvent::DiveStart), 1);

	// Patience drains, warns at 50%, and refills on a pickup by size.
	Run->SetPatienceFraction(0.505f);
	Run->StepRun(1.f);
	TestEqual(TEXT("patience_50 fired crossing half"), Bus->CountOf(DeepEvent::Patience50), 1);
	const float BeforePickup = Run->GetPatienceSeconds();
	Run->NotifyPickup(Item(EDeepSalvageSize::Medium, 6.f, 70));
	TestEqual(TEXT("A medium piece refills 10% of full"), Run->GetPatienceSeconds() - BeforePickup, Tuning.Patience.MaxSeconds * Tuning.Patience.RefillMedium, 0.01f);
	TestEqual(TEXT("First pickup is first_salvage"), Bus->CountOf(DeepEvent::FirstSalvage), 1);
	Run->NotifyPickup(Item(EDeepSalvageSize::Small, 2.f, 20));
	TestEqual(TEXT("Next pickup comments on its size"), Bus->CountOf(DeepEvent::SalvageSmall), 1);

	// Patience forced to zero cuts the tether: bailout air starts, the line is gone.
	Diver->AddCargo(Item(EDeepSalvageSize::Medium, 6.f, 70));
	Run->SetPatienceFraction(0.f);
	Run->StepRun(0.05f);
	TestEqual(TEXT("Zero Patience leads to Cut"), Run->GetState(), EDeepRunState::Cut);
	TestEqual(TEXT("cut_patience fired"), Bus->CountOf(DeepEvent::CutPatience), 1);
	TestEqual(TEXT("Bailout air starts full"), Run->GetBailoutAirSeconds(), Tuning.Bailout.AirSeconds, 0.001f);
	Diver->TeleportDiver(Anchor - FVector(0.f, 0.f, 200.f * UDeepRules::CmPerM));
	TestTrue(TEXT("A cut diver is not held by the tether"), FVector::Dist(Diver->GetActorLocation(), Anchor) / UDeepRules::CmPerM > 150.0);
	Diver->SetWinching(true);
	TestFalse(TEXT("A cut diver cannot winch"), Diver->IsWinching());

	// Bailout expiring: the 10 s warning, death, fee, cargo lost; then the next diver, aboard.
	State->Credits = 100;
	const int32 DiverBefore = State->DiverNumber;
	float Elapsed = 0.f;
	while (Run->GetState() == EDeepRunState::Cut && Elapsed < 60.f)
	{
		Run->StepRun(0.1f);
		Elapsed += 0.1f;
	}
	TestEqual(TEXT("bailout_10s fired"), Bus->CountOf(DeepEvent::Bailout10s), 1);
	TestEqual(TEXT("Air runs out: Dead"), Run->GetState(), EDeepRunState::Dead);
	const float RealAir = Tuning.Bailout.AirSeconds - Tuning.Bailout.SlowWindowSeconds + Tuning.Bailout.SlowWindowSeconds / FMath::Max(0.01f, Tuning.Bailout.SlowDrainScale);
	TestEqual(TEXT("The last seconds run slow"), Elapsed, RealAir, 0.15f);
	TestEqual(TEXT("death_air fired"), Bus->CountOf(DeepEvent::DeathAir), 1);
	TestEqual(TEXT("Carried salvage is lost"), Diver->GetCargo().Num(), 0);
	TestEqual(TEXT("Death fee charged"), State->Credits, 100 - Tuning.Economy.DeathFee);
	Run->StepRun(Tuning.Run.DeathScreenSeconds + 0.1f);
	TestEqual(TEXT("Respawned aboard"), Run->GetState(), EDeepRunState::Aboard);
	TestEqual(TEXT("Diver number goes up"), State->DiverNumber, DiverBefore + 1);
	TestEqual(TEXT("respawn fired"), Bus->CountOf(DeepEvent::Respawn), 1);
	TestTrue(TEXT("Respawned in the room"), Sub->IsInRoom(Diver->GetActorLocation()));
	TestEqual(TEXT("Respawn refills Patience"), Run->GetPatienceFraction(), 1.f, 0.0001f);
	Run->StepRun(10.f);
	TestEqual(TEXT("Patience does not run aboard"), Run->GetPatienceFraction(), 1.f, 0.0001f);
	TestTrue(TEXT("Through the hatch"), Run->LeaveSub());
	Diver->TeleportDiver(Start);
	TestEqual(TEXT("Salvage respawned for the dive"), CountSalvage(World), Tuning.Arena.SalvageSpots.Num());

	// Credits never go below zero.
	State->Credits = 10;
	Run->SetPatienceFraction(0.f);
	Run->StepRun(0.05f);
	Run->StepRun(60.f);
	TestEqual(TEXT("Death with 10 credits leaves 0"), State->Credits, 0);
	Run->StepRun(Tuning.Run.DeathScreenSeconds + 0.1f);
	Run->LeaveSub();
	Diver->TeleportDiver(Start);

	// Asking to be cut loose, then reaching the hatch: re-tethered for a fee, aboard.
	State->Credits = 100;
	Run->RequestCut();
	TestEqual(TEXT("Requested cut"), Run->GetState(), EDeepRunState::Cut);
	TestEqual(TEXT("cut_requested fired"), Bus->CountOf(DeepEvent::CutRequested), 1);
	Run->StepRun(5.f);
	Diver->TeleportDiver(Anchor);
	Run->StepRun(0.05f);
	TestEqual(TEXT("Home at the hatch on bailout air: aboard"), Run->GetState(), EDeepRunState::Aboard);
	TestEqual(TEXT("retether fired"), Bus->CountOf(DeepEvent::Retether), 1);
	TestEqual(TEXT("Re-tether fee charged"), State->Credits, 100 - Tuning.Economy.RetetherFee);

	// A haul: up to the hatch is home (aboard, nothing banked yet); the console banks exactly the rule's net.
	Run->LeaveSub();
	Diver->TeleportDiver(Start);
	Run->StepRun(0.05f);
	State->Credits = 0;
	Diver->AddCargo(Item(EDeepSalvageSize::Medium, 6.f, 70));
	Diver->TeleportDiver(Anchor);
	Run->StepRun(0.05f);
	TestEqual(TEXT("Arriving home with cargo brings the diver aboard"), Run->GetState(), EDeepRunState::Aboard);
	TestEqual(TEXT("enter_sub fired"), Bus->CountOf(DeepEvent::EnterSub), 1);
	TestEqual(TEXT("Nothing banked at the hatch"), Diver->GetCargo().Num(), 1);
	Run->UseConsole();
	TestEqual(TEXT("The console with cargo opens the settlement"), Run->GetState(), EDeepRunState::Settling);
	const FDeepSettlementReport& Report = Run->GetSettlement();
	const int32 FeeTotal = UDeepRules::SettlementFeeTotal(70, Tuning.Economy);
	const FDeepSettlement Expected = UDeepRules::Settle(70, { FeeTotal }, Tuning.Economy);
	TestEqual(TEXT("Gross is the cargo value"), Report.Gross, 70);
	TestEqual(TEXT("The Captain's cut is the rule's"), Report.DirectorCut, Expected.DirectorCut);
	TestEqual(TEXT("Fees total the rule's"), Report.Fees, FeeTotal);
	int32 FeeSum = 0;
	for (const int32 Amount : Report.FeeAmounts)
	{
		FeeSum += Amount;
	}
	TestEqual(TEXT("Itemised fee lines sum to the fee"), FeeSum, FeeTotal);
	TestEqual(TEXT("One label per fee line"), Report.FeeLabels.Num(), Report.FeeAmounts.Num());
	TestTrue(TEXT("At least the minimum number of fee lines"), Report.FeeAmounts.Num() >= Tuning.Economy.FeeLinesMin);
	TestEqual(TEXT("Net is the rule's"), Report.Net, Expected.Net);
	TestEqual(TEXT("Credits gain exactly the net"), State->Credits, Expected.Net - Report.HullSpent);
	TestEqual(TEXT("Cargo banked"), Diver->GetCargo().Num(), 0);
	TestTrue(TEXT("A bank event fired"), Bus->CountOf(DeepEvent::BankPoor) + Bus->CountOf(DeepEvent::BankGood) == 1);

	// The settlement screen ignores the key at first, then a key press returns to the deck.
	Run->ContinueAfterSettlement();
	TestEqual(TEXT("Continue is locked at first"), Run->GetState(), EDeepRunState::Settling);
	Run->StepRun(Tuning.Run.SettlementMinSeconds + 0.05f);
	Run->ContinueAfterSettlement();
	TestEqual(TEXT("Continue returns to the deck"), Run->GetState(), EDeepRunState::Aboard);
	TestEqual(TEXT("Same stop: nothing drives the sub but the helm"), State->Stop, 1);

	// With the flashlight affordable it is bought automatically; then the helm drives to stop 2.
	State->Credits = Tuning.Economy.HullPrices.IsEmpty() ? 0 : Tuning.Economy.HullPrices[0];
	Run->Bank();
	TestEqual(TEXT("An empty bank still settles"), Run->GetState(), EDeepRunState::Settling);
	TestEqual(TEXT("bank_empty fired"), Bus->CountOf(DeepEvent::BankEmpty), 1);
	TestTrue(TEXT("Flashlight bought"), Run->GetSettlement().bHullBought);
	TestEqual(TEXT("hull_bought fired"), Bus->CountOf(DeepEvent::HullBought), 1);
	TestEqual(TEXT("Gate 1 (flashlight)"), State->HullRating, 2);
	TestTrue(TEXT("Has the flashlight"), State->HasFlashlight());
	Run->StepRun(Tuning.Run.SettlementMinSeconds + 0.05f);
	Run->ContinueAfterSettlement();
	Run->SetAutopilot(Tuning.GetStop(2).SubDepthM);
	for (int32 Step = 0; Step < 400 && Run->IsAutopiloting(); ++Step)
	{
		Run->StepRun(0.5f);
	}
	TestEqual(TEXT("Drove to stop 2"), State->Stop, 2);
	TestEqual(TEXT("Parked at stop 2"), State->ParkedStop(), 2);
	TestEqual(TEXT("descend_stop2 fired"), Bus->CountOf(DeepEvent::DescendStop2), 1);
	Run->LeaveHelm();
	TestTrue(TEXT("Through the hatch at stop 2"), Run->LeaveSub());
	Diver->GetTether()->UpdateTether(0.f);
	TestEqual(TEXT("Tether is longer at stop 2"), Diver->GetTether()->GetSolve().LengthM, UDeepRules::TetherLengthM(2, Tuning.Tether), 0.01f);

	// DEEPSTATE reports the run.
	TSharedPtr<FJsonObject> Json;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(DeepStateReport::BuildJson(World)), Json);
	UTEST_TRUE(TEXT("DEEPSTATE parses"), Json.IsValid());
	TestEqual(TEXT("run_state"), Json->GetStringField(TEXT("run_state")), FString(TEXT("tethered")));
	TestTrue(TEXT("patience"), Json->HasField(TEXT("patience")));
	TestTrue(TEXT("bailout_air_s"), Json->HasField(TEXT("bailout_air_s")));
	TestEqual(TEXT("diver_number"), static_cast<int32>(Json->GetNumberField(TEXT("diver_number"))), State->DiverNumber);
	TestTrue(TEXT("last_bank after banking"), Json->HasField(TEXT("last_bank")));

	// Winch from the side at airlock height, past the end of the hull (the playtest stall): reaches home.
	Diver->TeleportDiver(Anchor + FVector(900.f, 0.f, 0.f));
	Diver->SetWinching(true);
	for (int32 Step = 0; Step < 60 && !Diver->GetTether()->IsHome(); ++Step)
	{
		Diver->GetTether()->UpdateTether(0.1f);
	}
	TestTrue(TEXT("Winch from the side reaches the airlock"), Diver->GetTether()->IsHome());
	Diver->SetWinching(false);

	// Alongside the hull at airlock height (the hull's own side, not past its end) and from above the sub: home too.
	const FVector SubCentre = Sub->GetActorLocation();
	for (const FVector& From : { Anchor + FVector(0.f, 900.f, 0.f), Anchor + FVector(-300.f, -1200.f, 150.f), SubCentre + FVector(200.f, 0.f, 900.f) })
	{
		Diver->TeleportDiver(From);
		Diver->SetWinching(true);
		for (int32 Step = 0; Step < 80 && !Diver->GetTether()->IsHome(); ++Step)
		{
			Diver->GetTether()->UpdateTether(0.1f);
		}
		TestTrue(FString::Printf(TEXT("Winch from %s reaches the airlock"), *(From - Anchor).ToString()), Diver->GetTether()->IsHome());
		Diver->SetWinching(false);
	}

	// Stop 2, a full dive: grab, Patience runs out, cut, swim home with the cargo (re-tether, aboard), bank at the
	// console, dive again at stop 2, cut again and die: the replacement diver starts aboard at stop 2.
	Diver->ResetToStart();
	Run->StepRun(0.05f);
	TestEqual(TEXT("Stop 2: tethered"), Run->GetState(), EDeepRunState::Tethered);
	State->Credits = 50;
	const FDeepCargoItem Haul = Item(EDeepSalvageSize::Large, 15.f, 200);
	Diver->AddCargo(Haul);
	Run->NotifyPickup(Haul);
	Run->SetPatienceFraction(0.f);
	Run->StepRun(0.05f);
	TestEqual(TEXT("Stop 2: Patience out cuts the tether"), Run->GetState(), EDeepRunState::Cut);
	Run->StepRun(3.f);
	Diver->TeleportDiver(Anchor);
	Run->StepRun(0.05f);
	TestEqual(TEXT("Stop 2: home after a cut is aboard"), Run->GetState(), EDeepRunState::Aboard);
	Run->UseConsole();
	TestEqual(TEXT("Stop 2: the console settles"), Run->GetState(), EDeepRunState::Settling);
	TestEqual(TEXT("Stop 2: settled the haul"), Run->GetSettlement().Gross, 200);
	TestEqual(TEXT("Stop 2: cargo banked"), Diver->GetCargo().Num(), 0);
	Run->StepRun(Tuning.Run.SettlementMinSeconds + 0.05f);
	Run->ContinueAfterSettlement();
	TestTrue(TEXT("Stop 2: through the hatch again"), Run->LeaveSub());
	TestEqual(TEXT("Stop 2: diving again"), Run->GetState(), EDeepRunState::Tethered);
	Diver->TeleportDiver(Start);
	const int32 StopBeforeDeath = State->Stop;
	const int32 DiverBeforeStop2Death = State->DiverNumber;
	Run->SetPatienceFraction(0.f);
	Run->StepRun(0.05f);
	Run->StepRun(60.f);
	TestEqual(TEXT("Stop 2: air runs out"), Run->GetState(), EDeepRunState::Dead);
	Run->StepRun(Tuning.Run.DeathScreenSeconds + 0.1f);
	TestEqual(TEXT("Stop 2: next diver aboard"), Run->GetState(), EDeepRunState::Aboard);
	TestEqual(TEXT("Stop 2: next diver number"), State->DiverNumber, DiverBeforeStop2Death + 1);
	TestEqual(TEXT("Stop 2: death keeps the stop"), State->Stop, StopBeforeDeath);
	Run->LeaveSub();
	Diver->GetTether()->UpdateTether(0.f);
	TestEqual(TEXT("Stop 2: tether length still the stop's"), Diver->GetTether()->GetSolve().LengthM, UDeepRules::TetherLengthM(StopBeforeDeath, Tuning.Tether), 0.01f);

	// Reset.
	Run->ResetRun();
	TestEqual(TEXT("Reset: stop 1"), State->Stop, 1);
	TestEqual(TEXT("Reset: diver 1"), State->DiverNumber, 1);
	TestEqual(TEXT("Reset: hull 1"), State->HullRating, 1);
	TestEqual(TEXT("Reset: starting credits"), State->Credits, Tuning.Economy.StartingCredits);

	for (const TCHAR* Command : { TEXT("Deep.SetPatience"), TEXT("Deep.ForceCut"), TEXT("Deep.Bank"), TEXT("Deep.ResetRun"), TEXT("Deep.Continue") })
	{
		TestNotNull(FString::Printf(TEXT("%s is registered"), Command), IConsoleManager::Get().FindConsoleObject(Command));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
