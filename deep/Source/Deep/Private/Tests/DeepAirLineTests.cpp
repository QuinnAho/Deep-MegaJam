// The tether as the air line (rescope, Nov 15): Patience no longer drains, the serpent's head on the rope cuts it once
// (tether_bitten), the diver can unclip by choice (bailout air, free re-clip at the hatch), Deep.SetPatience 0 still cuts.

#include "CoreMinimal.h"
#include "AI/AISystemBase.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Diver/DeepTetherComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Monster/DeepMonsterBrain.h"
#include "Monster/DeepSerpent.h"
#include "Sim/DeepAirLineRules.h"
#include "Sim/DeepRopeSim.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepEventBus.h"
#include "World/DeepRopeComponent.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_AIRLINE_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

// ---------------------------------------------------------------- pure rules

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepAirLinePatienceTest, "Deep.Sim.AirLine.NoDrain", DEEP_AIRLINE_TEST_FLAGS)
bool FDeepAirLinePatienceTest::RunTest(const FString& Parameters)
{
	FDeepPatienceTuning Patience;
	FDeepAirLineTuning AirLine;
	TestFalse(TEXT("Shipped: Patience does not drain"), AirLine.bPatienceDrains);
	TestEqual(TEXT("Holds over a minute"), DeepAirLine::PatienceAfterStep(80.f, 60.f, false, Patience, AirLine), 80.f, 0.0001f);
	TestEqual(TEXT("Holds while hunted"), DeepAirLine::PatienceAfterStep(80.f, 60.f, true, Patience, AirLine), 80.f, 0.0001f);
	TestEqual(TEXT("Zero stays zero (the cheat's cut)"), DeepAirLine::PatienceAfterStep(0.f, 1.f, false, Patience, AirLine), 0.f, 0.0001f);
	AirLine.bPatienceDrains = true;
	TestEqual(TEXT("The old rule comes back as data"), DeepAirLine::PatienceAfterStep(80.f, 1.f, true, Patience, AirLine),
		UDeepRules::PatienceStep(80.f, 1.f, true, Patience), 0.0001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepAirLineTouchTest, "Deep.Sim.AirLine.Touch", DEEP_AIRLINE_TEST_FLAGS)
bool FDeepAirLineTouchTest::RunTest(const FString& Parameters)
{
	// A line of three links straight down from the origin, 10 m each.
	const TArray<FVector> Points = { FVector(0, 0, 0), FVector(0, 0, -1000), FVector(0, 0, -2000), FVector(0, 0, -3000) };
	const TArray<uint8> Whole = { 1, 1, 1 };
	TestEqual(TEXT("Head on the middle link"), DeepAirLine::TouchedLink(Points, Whole, FVector(30, 0, -1500), 0.6f), 1);
	TestEqual(TEXT("Head just inside the reach"), DeepAirLine::TouchedLink(Points, Whole, FVector(59, 0, -2500), 0.6f), 2);
	TestEqual(TEXT("Head a metre off: no bite"), DeepAirLine::TouchedLink(Points, Whole, FVector(100, 0, -1500), 0.6f), INDEX_NONE);
	TestEqual(TEXT("Past the diver's end: no bite"), DeepAirLine::TouchedLink(Points, Whole, FVector(0, 0, -3100), 0.6f), INDEX_NONE);
	TestEqual(TEXT("Empty Linked = all joined"), DeepAirLine::TouchedLink(Points, TArray<uint8>(), FVector(0, 0, -500), 0.6f), 0);
	const TArray<uint8> CutMiddle = { 1, 0, 1 };
	TestEqual(TEXT("A link already cut is not bitten again"), DeepAirLine::TouchedLink(Points, CutMiddle, FVector(30, 0, -1500), 0.6f), INDEX_NONE);
	TestEqual(TEXT("No rope, no bite"), DeepAirLine::TouchedLink(TArray<FVector>(), TArray<uint8>(), FVector::ZeroVector, 0.6f), INDEX_NONE);

	// The rope sim's own points (what the player sees) work the same.
	FDeepRopeSim Sim;
	Sim.Init(8, FVector(0, 0, 0), FVector(0, 0, -2100));
	TestTrue(TEXT("A head on a sim point bites"), DeepAirLine::TouchedLink(Sim.Pos, Sim.Linked, Sim.Pos[4], 0.6f) != INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepAirLineStatesTest, "Deep.Sim.AirLine.States", DEEP_AIRLINE_TEST_FLAGS)
bool FDeepAirLineStatesTest::RunTest(const FString& Parameters)
{
	FDeepAirLineTuning AirLine;
	TestTrue(TEXT("Stalking bites"), DeepAirLine::StateBitesLine(EDeepMonsterState::Stalk, AirLine));
	TestTrue(TEXT("Biting bites"), DeepAirLine::StateBitesLine(EDeepMonsterState::Bite, AirLine));
	TestFalse(TEXT("A lurk brushing the line does not"), DeepAirLine::StateBitesLine(EDeepMonsterState::Lurk, AirLine));
	TestFalse(TEXT("A recoil does not"), DeepAirLine::StateBitesLine(EDeepMonsterState::Recoil, AirLine));
	AirLine.bOnlyAttackingBites = false;
	TestTrue(TEXT("Tuned: any touch bites"), DeepAirLine::StateBitesLine(EDeepMonsterState::Lurk, AirLine));

	FDeepEconomyTuning Economy;
	TestEqual(TEXT("A cut pays the re-tether fee"), DeepAirLine::ReclipFee(false, Economy, FDeepAirLineTuning()), Economy.RetetherFee);
	TestEqual(TEXT("An unclip re-clips free"), DeepAirLine::ReclipFee(true, Economy, FDeepAirLineTuning()), 0);
	return true;
}

// ---------------------------------------------------------------- in a world

namespace DeepAirLineTest
{
	struct FAirWorld
	{
		UWorld* World = nullptr;
		ADeepGameMode* GameMode = nullptr;
		ADeepGameState* State = nullptr;
		ADeepSubmarine* Sub = nullptr;
		ADeepDiver* Diver = nullptr;
		UDeepRunComponent* Run = nullptr;
		UDeepEventBus* Bus = nullptr;

		FAirWorld()
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

		~FAirWorld()
		{
			if (World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		bool Valid() const { return GameMode && State && Sub && Diver && Run && Bus && Diver->GetTether(); }

		/** The line hanging straight down: the diver DepthBelowM under the hatch, the rope laid over a few frames. */
		void HangBelow(float DepthBelowM) const
		{
			Diver->TeleportDiver(Sub->GetAnchorLocation() - FVector(0.f, 0.f, DepthBelowM * UDeepRules::CmPerM));
			for (int32 i = 0; i < 10; ++i)
			{
				Diver->GetTether()->UpdateTether(0.05f);
			}
		}

		/** A point on the line as the player sees it (the rope's middle point), else the straight line's middle. */
		FVector LineMiddle() const
		{
			const UDeepRopeComponent* Rope = Diver->GetTether()->GetLineRope();
			if (Rope && Rope->IsShown() && Rope->GetSim().IsValid())
			{
				return Rope->GetSim().Pos[Rope->GetSim().Pos.Num() / 2];
			}
			return (Diver->GetTether()->GetAnchorLocation() + Diver->GetTetherAttachLocation()) * 0.5f;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepAirLineWorldNoDrainTest, "Deep.World.AirLine.NoDrain", DEEP_AIRLINE_TEST_FLAGS)
bool FDeepAirLineWorldNoDrainTest::RunTest(const FString& Parameters)
{
	DeepAirLineTest::FAirWorld W;
	UTEST_TRUE(TEXT("World, diver and run"), W.Valid());
	UTEST_EQUAL(TEXT("Starts tethered"), W.Run->GetState(), EDeepRunState::Tethered);
	const float Max = UDeepTuningSettings::Get().Patience.MaxSeconds;
	for (float T = 0.f; T < Max * 1.5f; T += 0.5f)
	{
		W.Run->StepRun(0.5f);
	}
	TestEqual(TEXT("Clipped on, the dive does not end by time"), W.Run->GetState(), EDeepRunState::Tethered);
	TestEqual(TEXT("Patience holds"), W.Run->GetPatienceFraction(), 1.f, 0.0001f);
	TestEqual(TEXT("No Patience warning"), W.Bus->CountOf(DeepEvent::Patience50), 0);
	TestEqual(TEXT("No Patience cut"), W.Bus->CountOf(DeepEvent::CutPatience), 0);

	// The cheat still cuts: Deep.SetPatience 0 (the S4-cut playtest relies on it).
	GEngine->Exec(W.World, TEXT("Deep.SetPatience 0"));
	W.Run->StepRun(0.05f);
	TestEqual(TEXT("Deep.SetPatience 0 cuts the line"), W.Run->GetState(), EDeepRunState::Cut);
	TestEqual(TEXT("cut_patience fired once"), W.Bus->CountOf(DeepEvent::CutPatience), 1);
	TestTrue(TEXT("Bailout air is running"), W.Run->GetBailoutAirSeconds() > 0.f);
	TestFalse(TEXT("A Patience cut is not an unclip"), W.Run->IsUnclipped());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepAirLineWorldBiteTest, "Deep.World.AirLine.Bite", DEEP_AIRLINE_TEST_FLAGS)
bool FDeepAirLineWorldBiteTest::RunTest(const FString& Parameters)
{
	DeepAirLineTest::FAirWorld W;
	UTEST_TRUE(TEXT("World, diver and run"), W.Valid());
	UDeepMonsterBrain* Brain = W.World->GetSubsystem<UDeepMonsterBrain>();
	UTEST_NOT_NULL(TEXT("Brain"), Brain);
	W.GameMode->GotoStop(2);
	Brain->Step(0.1f);
	ADeepSerpent* Serpent = Brain->GetSerpent();
	UTEST_NOT_NULL(TEXT("Serpent at stop 2"), Serpent);
	UTEST_EQUAL(TEXT("Tethered"), W.Run->GetState(), EDeepRunState::Tethered);
	UDeepTetherComponent* Tether = W.Diver->GetTether();

	W.HangBelow(20.f);
	const FVector OnLine = W.LineMiddle();

	// Not attacking: a lurking head on the line does not bite.
	Serpent->PlaceHead(OnLine, FVector::ForwardVector);
	Brain->ForceState(EDeepMonsterState::Lurk);
	TestFalse(TEXT("A lurk on the line does not bite"), Tether->CheckSerpentBite());
	// Attacking but off the line.
	Serpent->PlaceHead(OnLine + FVector(300.f, 0.f, 0.f), FVector::ForwardVector);
	Brain->ForceState(EDeepMonsterState::Stalk);
	TestFalse(TEXT("A stalk 3 m off the line does not bite"), Tether->CheckSerpentBite());
	TestEqual(TEXT("Still clipped on"), W.Run->GetState(), EDeepRunState::Tethered);

	// The head on the rope: cut once, through the one Cut path, with its own event.
	Serpent->PlaceHead(OnLine, FVector::ForwardVector);
	Brain->ForceState(EDeepMonsterState::Stalk);
	TestTrue(TEXT("A stalking head on the rope bites it"), Tether->CheckSerpentBite());
	TestEqual(TEXT("Bitten: Cut"), W.Run->GetState(), EDeepRunState::Cut);
	TestEqual(TEXT("tether_bitten fired once"), W.Bus->CountOf(DeepAirLine::TetherBitten), 1);
	TestEqual(TEXT("Not a timed bite"), W.Bus->CountOf(DeepEvent::CutBite), 0);
	TestTrue(TEXT("Bailout air is running"), W.Run->GetBailoutAirSeconds() > 0.f);
	TestFalse(TEXT("A bite is not an unclip"), W.Run->IsUnclipped());

	// Once: the head still on the line changes nothing.
	TestFalse(TEXT("No second bite"), Tether->CheckSerpentBite());
	for (int32 i = 0; i < 5; ++i)
	{
		Tether->UpdateTether(0.05f);
		W.Run->StepRun(0.05f);
	}
	TestEqual(TEXT("Still exactly one tether_bitten"), W.Bus->CountOf(DeepAirLine::TetherBitten), 1);

	// Home on bailout air after a bite: the re-tether fee, as after any cut.
	W.State->Credits = 100;
	W.Diver->TeleportDiver(W.Sub->GetAnchorLocation());
	W.Run->StepRun(0.05f);
	TestEqual(TEXT("Home: aboard"), W.Run->GetState(), EDeepRunState::Aboard);
	TestEqual(TEXT("Re-tether fee after a bite"), W.State->Credits, 100 - UDeepTuningSettings::Get().Economy.RetetherFee);
	TestEqual(TEXT("retether fired"), W.Bus->CountOf(DeepEvent::Retether), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepAirLineWorldUnclipTest, "Deep.World.AirLine.Unclip", DEEP_AIRLINE_TEST_FLAGS)
bool FDeepAirLineWorldUnclipTest::RunTest(const FString& Parameters)
{
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	DeepAirLineTest::FAirWorld W;
	UTEST_TRUE(TEXT("World, diver and run"), W.Valid());
	UDeepTetherComponent* Tether = W.Diver->GetTether();
	TestTrue(TEXT("The unclip key is T"), UDeepTetherComponent::UnclipKey() == EKeys::T);

	// Unclip: bailout air starts, the diver is loose.
	W.State->Credits = 100;
	W.Run->Unclip();
	TestEqual(TEXT("Unclipped: Cut"), W.Run->GetState(), EDeepRunState::Cut);
	TestTrue(TEXT("It is an unclip"), W.Run->IsUnclipped());
	TestEqual(TEXT("tether_unclip fired"), W.Bus->CountOf(DeepAirLine::Unclip), 1);
	TestEqual(TEXT("Not a requested cut"), W.Bus->CountOf(DeepEvent::CutRequested), 0);
	const float AirAtUnclip = W.Run->GetBailoutAirSeconds();
	TestTrue(TEXT("Bailout air starts"), AirAtUnclip > 0.f);
	W.Run->StepRun(1.f);
	TestTrue(TEXT("Bailout air runs"), W.Run->GetBailoutAirSeconds() < AirAtUnclip - 0.5f);
	W.Run->Unclip();
	TestEqual(TEXT("A second unclip does nothing"), W.Bus->CountOf(DeepAirLine::Unclip), 1);
	W.Diver->SetWinching(true);
	TestFalse(TEXT("An unclipped diver cannot winch"), W.Diver->IsWinching());

	// Beyond the line's length: nothing holds the diver.
	const float LineM = UDeepRules::TetherLengthM(W.State->Stop, Tuning.Tether);
	const FVector Far = Tether->GetAnchorLocation() - FVector(0.f, 0.f, (LineM + 30.f) * UDeepRules::CmPerM);
	W.Diver->TeleportDiver(Far);
	Tether->UpdateTether(0.05f);
	TestTrue(TEXT("Past the line's length, not pulled back"),
		FVector::Dist(W.Diver->GetActorLocation(), Tether->GetAnchorLocation()) / UDeepRules::CmPerM > LineM + 20.f);

	// Re-clip at the hatch: aboard, no fee, the plain enter_sub.
	W.Diver->TeleportDiver(W.Sub->GetAnchorLocation());
	W.Run->StepRun(0.05f);
	TestEqual(TEXT("Home: aboard"), W.Run->GetState(), EDeepRunState::Aboard);
	TestEqual(TEXT("No fee for a voluntary unclip"), W.State->Credits, 100);
	TestEqual(TEXT("No retether event"), W.Bus->CountOf(DeepEvent::Retether), 0);
	TestEqual(TEXT("enter_sub fired"), W.Bus->CountOf(DeepEvent::EnterSub), 1);
	TestFalse(TEXT("Aboard is clipped back on"), W.Run->IsUnclipped());
	W.Run->Unclip();
	TestEqual(TEXT("No unclip aboard"), W.Run->GetState(), EDeepRunState::Aboard);

	// Down the hatch again: clipped on; unclipped and left too long, the air runs out.
	UTEST_TRUE(TEXT("Through the hatch"), W.Run->LeaveSub());
	TestEqual(TEXT("Clipped on again"), W.Run->GetState(), EDeepRunState::Tethered);
	W.Diver->TeleportDiver(Far);
	GEngine->Exec(W.World, TEXT("Deep.Unclip"));
	TestTrue(TEXT("Deep.Unclip unclips"), W.Run->IsUnclipped());
	for (float T = 0.f; T < 300.f && W.Run->GetState() == EDeepRunState::Cut; T += 0.5f)
	{
		W.Run->StepRun(0.5f);
	}
	TestEqual(TEXT("Out of bailout air: Dead"), W.Run->GetState(), EDeepRunState::Dead);
	TestEqual(TEXT("death_air fired"), W.Bus->CountOf(DeepEvent::DeathAir), 1);
	TestFalse(TEXT("Death clears the unclip"), W.Run->IsUnclipped());
	return true;
}

#endif
