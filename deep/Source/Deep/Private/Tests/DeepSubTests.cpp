// Tests for the submarine (D-022, D-023): the helm rules, the hatch between walking and swimming, the helm's depth
// limits by layer gate, and the fixed sub with the world moving around it.

#include "CoreMinimal.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Diver/DeepTetherComponent.h"
#include "Engine/Engine.h"
#include "AIController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepEventBus.h"
#include "World/DeepArena.h"
#include "World/DeepArenaRoot.h"
#include "World/DeepSalvage.h"
#include "World/DeepSubmarine.h"
#include "World/DeepSubRoom.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_SUB_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepSubTest
{
	/** A game world with the arena and one diver standing in the room (as a real run starts). */
	struct FSubWorld
	{
		UWorld* World = nullptr;
		ADeepGameMode* GameMode = nullptr;
		ADeepGameState* State = nullptr;
		ADeepSubmarine* Sub = nullptr;
		ADeepDiver* Diver = nullptr;
		UDeepRunComponent* Run = nullptr;
		UDeepEventBus* Bus = nullptr;

		FSubWorld()
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
				Diver = World->SpawnActor<ADeepDiver>(DeepArena::EnsureRoom(World)->GetRoomStartTransform().GetLocation(), FRotator::ZeroRotator);
			}
			Run = Diver ? Diver->GetRun() : nullptr;
			if (Run)
			{
				Run->StepRun(0.f);
			}
		}

		~FSubWorld()
		{
			if (World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		bool Valid() const { return GameMode && State && Sub && Diver && Run && Bus; }

		/** Autopilot to a depth at the helm's speed, in half-second steps. */
		void DriveTo(float DepthM) const
		{
			Run->SetAutopilot(DepthM);
			for (int32 Step = 0; Step < 2000 && Run->IsAutopiloting(); ++Step)
			{
				Run->StepRun(0.5f);
			}
		}

		/** Stands at the airlock valve in the room set (World/DeepSubRoom), off the helm and the console. */
		void StandAtValve() const
		{
			Diver->TeleportDiver(DeepArena::EnsureRoom(World)->GetValveLocation() + FVector(-50.f, 0.f, 100.f));
		}

		/** Out through the airlock: hold the use key on the valve and step until the cycle has put the diver in the water. */
		bool OutByValve(float MaxSeconds = 4.f) const
		{
			StandAtValve();
			Run->Use();
			for (float T = 0.f; T < MaxSeconds && Run->GetState() == EDeepRunState::Aboard; T += 0.05f)
			{
				Run->StepRun(0.05f);
			}
			return Run->GetState() == EDeepRunState::Tethered;
		}

		/** Steps until the airlock's fade has finished (nothing on screen blocks the next use). */
		void FinishAirlock() const
		{
			for (int32 Step = 0; Step < 100 && Run->IsInAirlock(); ++Step)
			{
				Run->StepRun(0.05f);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHelmRulesTest, "Deep.Sim.Rules.Helm", DEEP_SUB_TEST_FLAGS)
bool FDeepHelmRulesTest::RunTest(const FString& Parameters)
{
	FDeepSubTuning T;
	T.DepthLimitsM = { 200.f, 400.f, 600.f };
	T.DriveSpeedMps = 8.f;
	TestEqual(TEXT("No gate: 200 m"), UDeepRules::HelmDepthLimitM(0, T), 200.f);
	TestEqual(TEXT("Flashlight: 400 m"), UDeepRules::HelmDepthLimitM(1, T), 400.f);
	TestEqual(TEXT("Hull upgrade: 600 m"), UDeepRules::HelmDepthLimitM(2, T), 600.f);
	TestEqual(TEXT("Past the table: the last limit"), UDeepRules::HelmDepthLimitM(9, T), 600.f);

	const FDeepHelmStep Down = UDeepRules::HelmStep(100.f, 1.f, 1.f, 100.f, 200.f, T);
	TestEqual(TEXT("8 m per second down"), Down.DepthM, 108.f, 0.001f);
	TestFalse(TEXT("Not at the limit yet"), Down.bAtLimit);
	const FDeepHelmStep Blocked = UDeepRules::HelmStep(198.f, 1.f, 1.f, 100.f, 200.f, T);
	TestEqual(TEXT("Never past the limit"), Blocked.DepthM, 200.f, 0.001f);
	TestTrue(TEXT("Pushing against the limit says so"), Blocked.bAtLimit);
	const FDeepHelmStep Up = UDeepRules::HelmStep(103.f, -1.f, 1.f, 100.f, 200.f, T);
	TestEqual(TEXT("Never above the first stop"), Up.DepthM, 100.f, 0.001f);
	TestTrue(TEXT("Pushing against the top says so"), Up.bAtTop);
	TestEqual(TEXT("No input, no move"), UDeepRules::HelmStep(150.f, 0.f, 1.f, 100.f, 200.f, T).DepthM, 150.f, 0.001f);

	const TArray<float> Stops = { 100.f, 300.f, 500.f };
	TestEqual(TEXT("Within 5 m of 300: stop 2"), UDeepRules::HelmParkStop(296.f, Stops, 5.f), 2);
	TestEqual(TEXT("10 m off: no stop"), UDeepRules::HelmParkStop(310.f, Stops, 5.f), 0);
	TestEqual(TEXT("Nearest stop at 150 m is 1"), UDeepRules::NearestStop(150.f, Stops), 1);
	TestEqual(TEXT("A tie goes to the shallower stop"), UDeepRules::NearestStop(200.f, Stops), 1);
	TestEqual(TEXT("Nearest stop at 201 m is 2"), UDeepRules::NearestStop(201.f, Stops), 2);

	FDeepSubTuning H;
	H.HatchHalfSizeM = 0.8f;
	TestTrue(TEXT("Centre of the hatch"), UDeepRules::InHatchColumn(FVector(0.f, 0.f, -500.f), H));
	TestFalse(TEXT("Beside the hatch"), UDeepRules::InHatchColumn(FVector(100.f, 0.f, 0.f), H));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSubHatchTest, "Deep.World.SubHatch", DEEP_SUB_TEST_FLAGS)
bool FDeepSubHatchTest::RunTest(const FString& Parameters)
{
	using namespace DeepSubTest;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FSubWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus"), W.Valid());
	UCharacterMovementComponent* Movement = W.Diver->GetCharacterMovement();

	// Aboard: walking with gravity, no Patience drain, no tether.
	TestEqual(TEXT("Starts aboard"), W.Run->GetState(), EDeepRunState::Aboard);
	TestTrue(TEXT("Walking aboard"), Movement->MovementMode == MOVE_Walking);
	TestTrue(TEXT("Gravity aboard"), Movement->GravityScale > 0.f);
	TestFalse(TEXT("No tether aboard"), W.Run->HasTether());
	W.Run->StepRun(30.f);
	TestEqual(TEXT("Patience does not run aboard"), W.Run->GetPatienceFraction(), 1.f, 0.0001f);
	TestTrue(TEXT("The hatch is open while parked"), W.Sub->IsHatchOpen());
	TestEqual(TEXT("The idle line came"), W.Bus->CountOf(DeepEvent::IdleInside), 1);

	// Out through the airlock (hold the use key on the valve): swimming, tethered, the dive starts.
	TestTrue(TEXT("Out through the airlock: tethered"), W.OutByValve());
	W.FinishAirlock();
	TestTrue(TEXT("Swimming below the hatch"), Movement->MovementMode == MOVE_Flying);
	TestEqual(TEXT("No gravity in the water"), Movement->GravityScale, 0.f);
	TestEqual(TEXT("dive_start fired"), W.Bus->CountOf(DeepEvent::DiveStart), 1);
	W.Run->StepRun(0.5f);
	TestEqual(TEXT("Not straight back in"), W.Run->GetState(), EDeepRunState::Tethered);
	// The air line (rescope, Nov 15): clipped on, Patience no longer drains.
	TestEqual(TEXT("Patience holds on the air line"), W.Run->GetPatienceFraction(), 1.f, 0.0001f);

	// Rising up through the hatch: on the deck, walking.
	W.Diver->TeleportDiver(W.Sub->GetHatchLocation() - FVector(0.f, 0.f, 20.f));
	W.Run->StepRun(0.05f);
	TestEqual(TEXT("Up through the hatch: aboard"), W.Run->GetState(), EDeepRunState::Aboard);
	TestTrue(TEXT("Walking again"), Movement->MovementMode == MOVE_Walking);
	TestTrue(TEXT("Gravity again"), Movement->GravityScale > 0.f);
	const ADeepSubRoom* Room = DeepArena::EnsureRoom(W.World);
	UTEST_NOT_NULL(TEXT("The room set"), Room);
	TestTrue(TEXT("Standing in the room set, off the hatch"), Room->IsInRoom(W.Diver->GetActorLocation())
		&& !UDeepRules::InHatchColumn(W.Diver->GetActorLocation() - Room->GetHatchLocation(), Tuning.Sub));
	TestEqual(TEXT("enter_sub fired"), W.Bus->CountOf(DeepEvent::EnterSub), 1);
	TestTrue(TEXT("Back in through the airlock: the view fades in"), W.Run->IsInAirlock() && !W.Run->IsLeavingByAirlock());
	W.FinishAirlock();

	// The console without cargo buys nothing it cannot afford; the helm seats the diver.
	W.Diver->TeleportDiver(Room->GetConsoleLocation() - FVector(0.f, 100.f, 0.f));
	TestTrue(TEXT("Near the console"), W.Run->IsNearConsole());
	W.Run->Use();
	TestEqual(TEXT("No cargo: no settlement"), W.Run->GetState(), EDeepRunState::Aboard);
	W.Diver->TeleportDiver(Room->GetHelmLocation() - FVector(80.f, 0.f, 0.f));
	TestTrue(TEXT("Near the helm"), W.Run->IsNearHelm());
	W.Run->Use();
	TestTrue(TEXT("Seated at the helm"), W.Run->IsSeated());
	TestEqual(TEXT("helm_take fired"), W.Bus->CountOf(DeepEvent::HelmTake), 1);
	TestFalse(TEXT("Seated: no walking"), W.Run->CanWalk());

	// Between stops the hatch is locked.
	W.DriveTo(150.f);
	TestFalse(TEXT("Hatch locked between stops"), W.Sub->IsHatchOpen());
	W.Run->Use();
	TestFalse(TEXT("Stood up"), W.Run->IsSeated());
	TestFalse(TEXT("Cannot drop between stops"), W.Run->LeaveSub());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSubAirlockTest, "Deep.World.SubAirlock", DEEP_SUB_TEST_FLAGS)
bool FDeepSubAirlockTest::RunTest(const FString& Parameters)
{
	using namespace DeepSubTest;
	const FDeepAirlockTuning& Lock = UDeepTuningSettings::Get().Airlock;
	FSubWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus"), W.Valid());
	const ADeepSubRoom* Room = DeepArena::EnsureRoom(W.World);
	UTEST_NOT_NULL(TEXT("The room set"), Room);
	TestTrue(TEXT("The room set stands apart from the hull"), FVector::Dist(Room->GetActorLocation(), W.Sub->GetActorLocation()) > 10000.0);
	TestTrue(TEXT("Starts in the room set"), Room->IsInRoom(W.Diver->GetActorLocation(), 100.0));
	TestEqual(TEXT("Aboard, the depth is the sub's"), W.Diver->GetDepthM(), W.State->SubDepthM, 0.01f);

	// A short hold turns the wheel part way; letting go springs it back.
	W.StandAtValve();
	TestTrue(TEXT("Near the valve"), W.Run->IsNearValve());
	W.Run->Use();
	W.Run->StepRun(Lock.ValveHoldSeconds * 0.5f);
	TestTrue(TEXT("Half a hold: the wheel half turned"), W.Run->GetValveTurn01() > 0.3f && W.Run->GetValveTurn01() < 0.7f);
	TestFalse(TEXT("Not cycling yet"), W.Run->IsInAirlock());
	W.Run->ReleaseUse();
	W.Run->StepRun(0.05f);
	TestEqual(TEXT("Let go: the wheel springs back"), W.Run->GetValveTurn01(), 0.f, 0.0001f);

	// A full hold: the hiss, the fade to black, then the diver is in the water below the hull's hatch.
	W.Run->Use();
	for (float T = 0.f; T < Lock.ValveHoldSeconds + 0.1f && !W.Run->IsInAirlock(); T += 0.05f)
	{
		W.Run->StepRun(0.05f);
	}
	TestTrue(TEXT("A full turn starts the airlock"), W.Run->IsLeavingByAirlock());
	TestEqual(TEXT("airlock_cycle fired"), W.Bus->CountOf(DeepEvent::AirlockCycle), 1);
	TestFalse(TEXT("No walking while the airlock cycles"), W.Run->CanWalk());
	W.Run->StepRun(Lock.HissSeconds * 0.5f);
	TestEqual(TEXT("Hissing, still aboard"), W.Run->GetState(), EDeepRunState::Aboard);
	float Waited = 0.f;
	for (; Waited < 3.f && W.Run->GetState() == EDeepRunState::Aboard; Waited += 0.05f)
	{
		W.Run->StepRun(0.05f);
	}
	TestEqual(TEXT("Out: tethered in the water"), W.Run->GetState(), EDeepRunState::Tethered);
	TestTrue(TEXT("Moved while the view is black"), W.Run->GetAirlockState().Fade01 > 0.99f);
	TestFalse(TEXT("Out of the room set"), Room->IsInRoom(W.Diver->GetActorLocation(), 100.0));
	TestEqual(TEXT("dive_start fired"), W.Bus->CountOf(DeepEvent::DiveStart), 1);
	W.FinishAirlock();
	TestFalse(TEXT("The view has faded back in"), W.Run->IsInAirlock());

	// Back in: put in the room set at once, the view starts black and fades in; walking is allowed.
	W.Run->EnterSub();
	TestEqual(TEXT("Aboard again"), W.Run->GetState(), EDeepRunState::Aboard);
	TestTrue(TEXT("In the room set"), Room->IsInRoom(W.Diver->GetActorLocation(), 100.0));
	TestTrue(TEXT("Coming in: black first"), W.Run->IsInAirlock() && W.Run->GetAirlockState().Fade01 > 0.99f);
	TestTrue(TEXT("Coming in: free to walk"), W.Run->CanWalk());
	TestEqual(TEXT("airlock_cycle fired again"), W.Bus->CountOf(DeepEvent::AirlockCycle), 2);
	W.FinishAirlock();
	TestFalse(TEXT("Faded in"), W.Run->IsInAirlock());

	// Between stops the hatch is locked: the wheel does not turn.
	W.DriveTo(150.f);
	W.Run->LeaveHelm();
	W.StandAtValve();
	TestFalse(TEXT("Between stops: locked"), W.Run->CanCycleAirlock());
	W.Run->Use();
	W.Run->StepRun(Lock.ValveHoldSeconds + 0.5f);
	TestEqual(TEXT("Locked: the wheel stays put"), W.Run->GetValveTurn01(), 0.f, 0.0001f);
	TestFalse(TEXT("Locked: no cycle"), W.Run->IsInAirlock());
	TestEqual(TEXT("Locked: still aboard"), W.Run->GetState(), EDeepRunState::Aboard);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSubAboardPitchTest, "Deep.World.SubAboardPitch", DEEP_SUB_TEST_FLAGS)
bool FDeepSubAboardPitchTest::RunTest(const FString& Parameters)
{
	using namespace DeepSubTest;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FSubWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus"), W.Valid());
	AAIController* Controller = W.World->SpawnActor<AAIController>();
	UTEST_NOT_NULL(TEXT("A controller"), Controller);
	Controller->Possess(W.Diver);

	// Out through the airlock, then come back pitched steeply up: the view is levelled on arrival, yaw kept.
	UTEST_TRUE(TEXT("In the water"), W.OutByValve());
	Controller->SetControlRotation(FRotator(60.f, 123.f, 5.f));
	W.Run->EnterSub();
	const FRotator View = Controller->GetControlRotation();
	TestEqual(TEXT("Aboard"), W.Run->GetState(), EDeepRunState::Aboard);
	TestEqual(TEXT("Pitch is the tuned level"), FRotator::NormalizeAxis(View.Pitch), (double)Tuning.Arena.AboardPitchDeg, 0.01);
	TestEqual(TEXT("Yaw unchanged"), FRotator::NormalizeAxis(View.Yaw), 123.0, 0.01);
	TestEqual(TEXT("No roll"), View.Roll, 0.0, 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSubHelmLimitsTest, "Deep.World.SubHelmLimits", DEEP_SUB_TEST_FLAGS)
bool FDeepSubHelmLimitsTest::RunTest(const FString& Parameters)
{
	using namespace DeepSubTest;
	// The depth limits on the straight drive: the cave shaft off, so 100 s of holding the helm down meets no rock.
	struct FShaftOff
	{
		IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("Deep.Cave.Shaft"));
		int32 Before = Var ? Var->GetInt() : -1;
		FShaftOff() { if (Var) { Var->Set(0, ECVF_SetByCode); } }
		~FShaftOff() { if (Var) { Var->Set(Before, ECVF_SetByCode); } }
	} ShaftOff;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FSubWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus"), W.Valid());
	const float Deepest =Tuning.GetStop(Tuning.NumStops()).SubDepthM + 100.f;

	for (int32 Gates = 0; Gates <= 2; ++Gates)
	{
		const float Limit = Tuning.Sub.DepthLimitsM.IsValidIndex(Gates) ? Tuning.Sub.DepthLimitsM[Gates] : 0.f;
		TestEqual(FString::Printf(TEXT("Gates %d: the game state's limit"), Gates), W.State->DepthLimitM(), Limit, 0.01f);
		const int32 LimitEventsBefore = W.Bus->CountOf(DeepEvent::HelmDepthLimit);
		W.Run->TakeHelm();
		W.Run->SetHelmInput(1.f);
		for (int32 Step = 0; Step < 400; ++Step)
		{
			W.Run->StepRun(0.25f);
		}
		TestEqual(FString::Printf(TEXT("Gates %d: held down, the helm stops at %.0f m"), Gates, Limit), W.State->SubDepthM, FMath::Min(Limit, Deepest), 0.01f);
		TestTrue(FString::Printf(TEXT("Gates %d: says why"), Gates), W.Run->IsHelmAtLimit());
		TestTrue(FString::Printf(TEXT("Gates %d: helm_depth_limit"), Gates), W.Bus->CountOf(DeepEvent::HelmDepthLimit) > LimitEventsBefore || Gates > 0);
		W.Run->SetHelmInput(0.f);
		W.Run->StepRun(0.1f);
		TestFalse(FString::Printf(TEXT("Gates %d: released, no longer pushing"), Gates), W.Run->IsHelmAtLimit());
		// Back to the first stop for the next gate.
		W.DriveTo(Tuning.GetStop(1).SubDepthM);
		TestEqual(FString::Printf(TEXT("Gates %d: parked at stop 1 again"), Gates), W.State->ParkedStop(), 1);
		W.Run->LeaveHelm();
		W.Run->BuyNextGate(true);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSubFixedTest, "Deep.World.SubFixedWorldMoves", DEEP_SUB_TEST_FLAGS)
bool FDeepSubFixedTest::RunTest(const FString& Parameters)
{
	using namespace DeepSubTest;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FSubWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus"), W.Valid());
	ADeepArenaRoot* Root = DeepArena::FindArenaRoot(W.World);
	UTEST_NOT_NULL(TEXT("The arena root exists"), Root);
	ADeepSalvage* Piece = nullptr;
	for (TActorIterator<ADeepSalvage> It(W.World); It; ++It)
	{
		Piece = *It;
		break;
	}
	UTEST_NOT_NULL(TEXT("Salvage exists"), Piece);
	TestTrue(TEXT("Salvage hangs from the arena root"), Piece->GetAttachParentActor() == Root);

	const FVector SubBefore = W.Sub->GetActorLocation();
	const FVector RootBefore = Root->GetActorLocation();
	const FVector PieceBefore = Piece->GetActorLocation();
	W.Run->BuyNextGate(true);
	W.Run->TakeHelm();
	W.Run->SetHelmInput(1.f);
	TestEqual(TEXT("helm_descend on the first push"), W.Bus->CountOf(DeepEvent::HelmDescend), 0);
	for (int32 Step = 0; Step < 10; ++Step)
	{
		W.Run->StepRun(0.5f);
	}
	TestEqual(TEXT("helm_descend fired"), W.Bus->CountOf(DeepEvent::HelmDescend), 1);
	const float Driven = W.State->SubDepthM - Tuning.GetStop(1).SubDepthM;
	TestEqual(TEXT("5 s at the helm's speed"), Driven, Tuning.Sub.DriveSpeedMps * 5.f, 0.01f);
	TestEqual(TEXT("The submarine has not moved"), FVector::Dist(W.Sub->GetActorLocation(), SubBefore), 0.0, 0.001);
	TestEqual(TEXT("The arena root rose by the distance driven"), Root->GetActorLocation().Z - RootBefore.Z, static_cast<double>(Driven * UDeepRules::CmPerM), 0.5);
	TestEqual(TEXT("The salvage rose with it"), Piece->GetActorLocation().Z - PieceBefore.Z, static_cast<double>(Driven * UDeepRules::CmPerM), 0.5);
	TestEqual(TEXT("Not parked while driving"), W.State->ParkedStop(), 0);

	// Release within the snap of stop 2: parked there, its arena in place (root back at the sub), the hatch open.
	W.DriveTo(Tuning.GetStop(2).SubDepthM - Tuning.Sub.ParkSnapM * 0.5f);
	TestEqual(TEXT("Parked at stop 2"), W.State->ParkedStop(), 2);
	TestEqual(TEXT("Snapped to the stop's depth"), W.State->SubDepthM, Tuning.GetStop(2).SubDepthM, 0.01f);
	TestEqual(TEXT("The arena root is back level with the sub"), FVector::Dist(Root->GetActorLocation(), W.Sub->GetActorLocation()), 0.0, 0.5);
	TestEqual(TEXT("The submarine still has not moved"), FVector::Dist(W.Sub->GetActorLocation(), SubBefore), 0.0, 0.001);
	TestTrue(TEXT("Hatch open when parked"), W.Sub->IsHatchOpen());
	TestEqual(TEXT("descend_stop2 on the first arrival"), W.Bus->CountOf(DeepEvent::DescendStop2), 1);
	TestEqual(TEXT("Stop 2's salvage ring"), [&W]() { int32 N = 0; for (TActorIterator<ADeepSalvage> It(W.World); It; ++It) { N += IsValid(*It) ? 1 : 0; } return N; }(), Tuning.Arena.SalvageSpots.Num());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
