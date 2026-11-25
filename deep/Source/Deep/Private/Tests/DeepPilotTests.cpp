// Tests for the real sub controls (Sim/DeepPilotRules, UDeepRunComponent::StepPilot, UDeepHelmLevers): the throttle's
// notches, the ballast lever and its detent, motion along the heading with sideways drag, the world turning about the
// sub, walls that dent and trip the throttle, parking that glides to the stop's frame, and the levers that follow.

#include "CoreMinimal.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Private/Tests/DeepTestCVar.h"
#include "Sim/DeepCaveRules.h"
#include "Sim/DeepPilotRules.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepEventBus.h"
#include "World/DeepArena.h"
#include "World/DeepArenaRoot.h"
#include "World/DeepHelmLevers.h"
#include "World/DeepShaft.h"
#include "World/DeepSubmarine.h"
#include "World/DeepSubRoom.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_PILOT_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepPilotTest
{
	/** Deep.Cave.Shaft set for the test's lifetime. */
	struct FShaftScope
	{
		IConsoleVariable* Var = nullptr;
		int32 Before = -1;

		explicit FShaftScope(int32 Value)
		{
			Var = IConsoleManager::Get().FindConsoleVariable(TEXT("Deep.Cave.Shaft"));
			if (Var)
			{
				Before = Var->GetInt();
				Var->Set(Value, ECVF_SetByCode);
			}
		}

		~FShaftScope()
		{
			if (Var)
			{
				Var->Set(Before, ECVF_SetByCode);
			}
		}
	};

	/** A game world with the arena and one diver in the room, as a run starts (as Deep.World.Cave's). */
	struct FPilotWorld
	{
		/** These are the lever mode's tests (D-048), behind its switch since D-070's direct drive. */
		DeepTest::FCVarScope Levers{ TEXT("Deep.Pilot.Levers"), 1 };
		UWorld* World = nullptr;
		ADeepGameMode* GameMode = nullptr;
		ADeepGameState* State = nullptr;
		ADeepSubmarine* Sub = nullptr;
		ADeepDiver* Diver = nullptr;
		UDeepRunComponent* Run = nullptr;
		UDeepEventBus* Bus = nullptr;

		FPilotWorld()
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

		~FPilotWorld()
		{
			if (World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		bool Valid() const { return GameMode && State && Sub && Diver && Run && Bus; }

		ADeepArenaRoot* Root() const { return DeepArena::FindArenaRoot(World); }

		ADeepShaft* Shaft() const
		{
			for (TActorIterator<ADeepShaft> It(World); It; ++It)
			{
				return *It;
			}
			return nullptr;
		}

		/** Seated, unparked at a depth on the passage's centre, heading as parked, levers home. */
		void HoldAt(float DepthM) const
		{
			if (!Run->IsSeated())
			{
				Run->TakeHelm();
			}
			const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
			Run->AllStop();
			Run->SetPilotHold(0.f, 0.f);
			State->SubDepthM = DepthM;
			State->SubOffsetM = UDeepCaveRules::CentreM(DepthM, Tuning.StopDepthsM(), Tuning.Cave);
			State->SubYawDeg = 0.f;
			State->bParked = false;
			DeepArena::ApplySubDepth(World);
		}

		/** Runs the helm (and the game state's park glide, as its tick would) in 0.05 s steps. */
		void Fly(float Seconds) const
		{
			for (float T = 0.f; T < Seconds; T += 0.05f)
			{
				Run->StepRun(0.05f);
				State->StepParkGlide(0.05f);
			}
		}

		/** Where the old placement (no yaw) put the root for the current offset and depth. */
		FVector OldRootCm() const
		{
			const float UpM = State->SubDepthM - UDeepTuningSettings::Get().GetStop(State->Stop).SubDepthM;
			const FVector Side = Sub->GetActorRotation().RotateVector(FVector(State->SubOffsetM.X, State->SubOffsetM.Y, 0.f));
			return Sub->GetActorLocation() + (FVector(0.f, 0.f, UpM) - Side) * UDeepRules::CmPerM;
		}
	};
}

// ---------------------------------------------------------------- Deep.Sim.Pilot

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepPilotNotchTest, "Deep.Sim.Pilot.Notch", DEEP_PILOT_TEST_FLAGS)
bool FDeepPilotNotchTest::RunTest(const FString& Parameters)
{
	const FDeepPilotTuning T;
	TestEqual(TEXT("W from stop: one notch ahead"), UDeepPilotRules::StepNotch(0, 1, T), 1);
	TestEqual(TEXT("S from stop: one notch astern"), UDeepPilotRules::StepNotch(0, -1, T), -1);
	TestEqual(TEXT("Held at full ahead"), UDeepPilotRules::StepNotch(T.AheadNotches, 1, T), T.AheadNotches);
	TestEqual(TEXT("Held at full astern"), UDeepPilotRules::StepNotch(-T.AsternNotches, -1, T), -T.AsternNotches);
	TestEqual(TEXT("A press is one notch, however hard"), UDeepPilotRules::StepNotch(0, 5, T), 1);
	TestEqual(TEXT("Six positions: full astern .. full ahead"), T.AheadNotches + T.AsternNotches + 1, 6);
	TestEqual(TEXT("Stop asks for nothing"), UDeepPilotRules::NotchSpeedMps(0, T), 0.f);
	TestEqual(TEXT("Full ahead"), UDeepPilotRules::NotchSpeedMps(T.AheadNotches, T), T.AheadSpeedMps);
	TestEqual(TEXT("Full astern"), UDeepPilotRules::NotchSpeedMps(-T.AsternNotches, T), -T.AsternSpeedMps);
	TestTrue(TEXT("Slow < half < full"), UDeepPilotRules::NotchSpeedMps(1, T) < UDeepPilotRules::NotchSpeedMps(2, T) && UDeepPilotRules::NotchSpeedMps(2, T) < UDeepPilotRules::NotchSpeedMps(3, T));
	TestEqual(TEXT("Lever fraction at full astern"), UDeepPilotRules::NotchFraction(-T.AsternNotches, T), -1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepPilotBallastTest, "Deep.Sim.Pilot.Ballast", DEEP_PILOT_TEST_FLAGS)
bool FDeepPilotBallastTest::RunTest(const FString& Parameters)
{
	const FDeepPilotTuning T;
	float Detent = 0.f;
	// Holding C floods: the lever moves, and stays where it is left.
	float Lever = 0.f;
	for (int32 Step = 0; Step < 10; ++Step)
	{
		Lever = UDeepPilotRules::StepBallast(Lever, 1.f, 0.05f, Detent, T);
	}
	TestTrue(TEXT("Half a second of C: flooding"), Lever > 0.4f && Lever < 0.8f);
	const float Left = Lever;
	Lever = UDeepPilotRules::StepBallast(Lever, 0.f, 1.f, Detent, T);
	TestEqual(TEXT("Let go: it stays"), Lever, Left);
	for (int32 Step = 0; Step < 60; ++Step)
	{
		Lever = UDeepPilotRules::StepBallast(Lever, 1.f, 0.05f, Detent, T);
	}
	TestEqual(TEXT("Held: full flood, no further"), Lever, 1.f);

	// Space blows it back: it stops in the neutral detent a moment before going on to blow.
	bool bHeldAtNeutral = false;
	float HeldFor = 0.f;
	for (int32 Step = 0; Step < 40; ++Step)
	{
		Lever = UDeepPilotRules::StepBallast(Lever, -1.f, 0.05f, Detent, T);
		if (Lever == 0.f)
		{
			bHeldAtNeutral = true;
			HeldFor += 0.05f;
		}
	}
	TestTrue(TEXT("Passing neutral, it sits in the detent"), bHeldAtNeutral && HeldFor >= T.BallastDetentS - 0.051f);
	TestTrue(TEXT("Then goes on to blow"), Lever < 0.f);

	// Let go close to neutral: it drops into the detent.
	Lever = UDeepPilotRules::StepBallast(T.BallastDetentSnap * 0.5f, 0.f, 0.05f, Detent, T);
	TestEqual(TEXT("Near neutral: snaps to neutral"), Lever, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepPilotMotionTest, "Deep.Sim.Pilot.Motion", DEEP_PILOT_TEST_FLAGS)
bool FDeepPilotMotionTest::RunTest(const FString& Parameters)
{
	const FDeepPilotTuning T;
	// Full ahead from rest: speed builds along the heading and settles at the notch's speed.
	FDeepPilotMotion M;
	M = UDeepPilotRules::StepMotion(M, T.AheadNotches, 0.f, 0.f, 1.f, T);
	TestTrue(TEXT("A second in: moving ahead, not yet at full"), M.VelocityMps.X > 0.5f && M.VelocityMps.X < T.AheadSpeedMps);
	TestTrue(TEXT("Nothing sideways"), FMath::IsNearlyZero(M.VelocityMps.Y, 1e-4f));
	for (int32 Step = 0; Step < 300; ++Step)
	{
		M = UDeepPilotRules::StepMotion(M, T.AheadNotches, 0.f, 0.f, 0.1f, T);
	}
	TestTrue(TEXT("Settles at full ahead"), FMath::IsNearlyEqual(M.VelocityMps.X, T.AheadSpeedMps, 0.01f));

	// Turned 90 degrees at once: the way across the new heading dies fast (high sideways drag), the way along slow.
	FDeepPilotMotion Skid;
	Skid.VelocityMps = FVector2D(2.f, 0.f);
	Skid.YawDeg = 90.f;
	const FDeepPilotMotion SkidOut = UDeepPilotRules::StepMotion(Skid, 0, 0.f, 0.f, 0.5f, T);
	FDeepPilotMotion Coast;
	Coast.VelocityMps = FVector2D(2.f, 0.f);
	const FDeepPilotMotion CoastOut = UDeepPilotRules::StepMotion(Coast, 0, 0.f, 0.f, 0.5f, T);
	TestTrue(TEXT("Sideways drag is higher than along"), SkidOut.VelocityMps.Size() < CoastOut.VelocityMps.Size() * 0.5f);

	// Rudder: the turn rate builds (inertia) and the heading turns to starboard.
	FDeepPilotMotion Turn;
	Turn = UDeepPilotRules::StepMotion(Turn, 0, 0.f, 1.f, 0.1f, T);
	TestTrue(TEXT("Rudder over: the turn rate starts small"), Turn.YawRateDegS > 0.f && Turn.YawRateDegS < T.YawRateDegS * 0.5f);
	for (int32 Step = 0; Step < 50; ++Step)
	{
		Turn = UDeepPilotRules::StepMotion(Turn, 0, 0.f, 1.f, 0.1f, T);
	}
	TestTrue(TEXT("Then near the full rate"), Turn.YawRateDegS > T.YawRateDegS * 0.9f);
	TestTrue(TEXT("Turned to starboard"), Turn.YawDeg > 30.f);
	const float RateAtRelease = Turn.YawRateDegS;
	Turn = UDeepPilotRules::StepMotion(Turn, 0, 0.f, 0.f, 0.2f, T);
	TestTrue(TEXT("Rudder amidships: the turn carries on a little"), Turn.YawRateDegS > 0.f && Turn.YawRateDegS < RateAtRelease);

	// Ballast: buoyancy against drag caps the vertical speed at the lever's share of HeaveSpeedMps.
	FDeepPilotMotion Sink;
	for (int32 Step = 0; Step < 300; ++Step)
	{
		Sink = UDeepPilotRules::StepMotion(Sink, 0, 1.f, 0.f, 0.1f, T);
	}
	TestTrue(TEXT("Flooded: sinks, capped"), FMath::IsNearlyEqual(Sink.HeaveMps, T.HeaveSpeedMps, 0.01f));
	FDeepPilotMotion Rise;
	Rise = UDeepPilotRules::StepMotion(Rise, 0, -0.5f, 0.f, 30.f, T);
	TestTrue(TEXT("Half blown: rises at half"), FMath::IsNearlyEqual(Rise.HeaveMps, -0.5f * T.HeaveSpeedMps, 0.01f));

	// The pace (brief: 100 m of shaft in about 40-60 s): full flood from rest, no bends, is the floor of that.
	FDeepPilotMotion Dive;
	float Depth = 0.f;
	float Seconds = 0.f;
	while (Depth < 100.f && Seconds < 200.f)
	{
		Dive = UDeepPilotRules::StepMotion(Dive, 0, 1.f, 0.f, 0.05f, T);
		Depth += Dive.HeaveMps * 0.05f;
		Seconds += 0.05f;
	}
	AddInfo(FString::Printf(TEXT("100 m at full flood: %.1f s"), Seconds));
	TestTrue(TEXT("100 m at full flood takes 38-50 s (bends add the rest)"), Seconds >= 38.f && Seconds <= 50.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepPilotParkTest, "Deep.Sim.Pilot.Park", DEEP_PILOT_TEST_FLAGS)
bool FDeepPilotParkTest::RunTest(const FString& Parameters)
{
	const FDeepPilotTuning T;
	TestTrue(TEXT("Near, slow, stopped, neutral: parks"), UDeepPilotRules::ShouldPark(103.f, 100.f, 0.5f, 0, 0.f, T));
	TestFalse(TEXT("Throttle on: no"), UDeepPilotRules::ShouldPark(103.f, 100.f, 0.5f, 1, 0.f, T));
	TestFalse(TEXT("Ballast off neutral: no"), UDeepPilotRules::ShouldPark(103.f, 100.f, 0.5f, 0, 0.3f, T));
	TestFalse(TEXT("Too fast: no"), UDeepPilotRules::ShouldPark(103.f, 100.f, T.ParkSpeedMps + 1.f, 0, 0.f, T));
	TestFalse(TEXT("Too far: no"), UDeepPilotRules::ShouldPark(100.f + T.ParkNearM + 1.f, 100.f, 0.f, 0, 0.f, T));

	// The glide home: offset, depth and heading reach the stop's frame in about two seconds.
	float Off = 4.f;
	float Depth = 104.f;
	float Yaw = 170.f;
	float Seconds = 0.f;
	while ((Off != 0.f || Depth != 100.f || Yaw != 0.f) && Seconds < 10.f)
	{
		Off = UDeepPilotRules::GlideStep(Off, 0.f, 0.05f, T.ParkGlideMinMps, T);
		Depth = UDeepPilotRules::GlideStep(Depth, 100.f, 0.05f, T.ParkGlideMinMps, T);
		Yaw = UDeepPilotRules::GlideYawStep(Yaw, 0.05f, T);
		Seconds += 0.05f;
	}
	AddInfo(FString::Printf(TEXT("Glide home from 4 m off, 4 m up, 170 deg: %.2f s"), Seconds));
	TestTrue(TEXT("Home exactly, in 1-4 s"), Off == 0.f && Depth == 100.f && Yaw == 0.f && Seconds >= 1.f && Seconds <= 4.f);
	TestTrue(TEXT("The short way round: 350 deg glides up to 360, not down"), UDeepPilotRules::GlideYawStep(350.f, 0.1f, T) > -10.f && UDeepPilotRules::GlideYawStep(350.f, 0.1f, T) <= 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepPilotBoxTest, "Deep.Sim.Pilot.TurnFreeBox", DEEP_PILOT_TEST_FLAGS)
bool FDeepPilotBoxTest::RunTest(const FString& Parameters)
{
	FVector Min, Max;
	UDeepPilotRules::TurnFreeBoxM(FVector(-6.f, -3.f, -2.f), FVector(8.f, 3.f, 2.f), Min, Max);
	TestTrue(TEXT("Square"), FMath::IsNearlyEqual(Max.X, Max.Y) && FMath::IsNearlyEqual(Min.X, -Max.X) && FMath::IsNearlyEqual(Min.Y, -Max.Y));
	TestTrue(TEXT("Corners on the hull's circumcircle"), FMath::IsNearlyEqual(FVector2D(Max.X, Max.Y).Size(), FVector2D(8.f, 3.f).Size(), 1e-3f));
	TestTrue(TEXT("Z kept"), Min.Z == -2.f && Max.Z == 2.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepPilotHangTest, "Deep.Sim.Pilot.Hang", DEEP_PILOT_TEST_FLAGS)
bool FDeepPilotHangTest::RunTest(const FString& Parameters)
{
	const FVector Sub(1000.f, -200.f, 500.f);
	const FQuat SubRot = FRotator(0.f, 30.f, 0.f).Quaternion();
	const FVector2D Off(2.f, -1.f);
	FQuat Turn;
	const FVector At = UDeepPilotRules::HangLocationCm(Sub, SubRot, Off, 7.f, 0.f, Turn);
	const FVector Old = Sub + (FVector(0.f, 0.f, 7.f) - SubRot.RotateVector(FVector(Off.X, Off.Y, 0.f))) * 100.f;
	TestTrue(TEXT("Yaw 0: the old placement"), At.Equals(Old, 1e-3f) && Turn.Equals(FQuat::Identity));

	// Turned: a pure yaw of -YawDeg about the vertical through the sub; the height is untouched.
	UDeepPilotRules::HangLocationCm(Sub, SubRot, Off, 7.f, 40.f, Turn);
	TestTrue(TEXT("A pure yaw of -40"), FMath::IsNearlyEqual(Turn.Rotator().Yaw, -40.f, 1e-2f) && FMath::IsNearlyZero(Turn.Rotator().Pitch, 1e-3f) && FMath::IsNearlyZero(Turn.Rotator().Roll, 1e-3f));
	const FVector Turned = UDeepPilotRules::HangLocationCm(Sub, SubRot, Off, 7.f, 40.f, Turn);
	TestTrue(TEXT("Same distance from the sub"), FMath::IsNearlyEqual((Turned - Sub).Size(), (Old - Sub).Size(), 1e-2f));
	TestTrue(TEXT("Same height"), FMath::IsNearlyEqual(Turned.Z, Old.Z, 1e-3f));

	// Ahead in the stop frame at heading Y is straight ahead of the sub once the world has turned.
	const FVector Ahead3 = FVector(UDeepPilotRules::Ahead(40.f), 0.f);
	TestTrue(TEXT("The heading lines up with the sub's nose"), Turn.RotateVector(Ahead3).Equals(FVector::ForwardVector, 1e-4f));
	return true;
}

// ---------------------------------------------------------------- Deep.World.Pilot

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepPilotThrottleWorldTest, "Deep.World.Pilot.Throttle", DEEP_PILOT_TEST_FLAGS)
bool FDeepPilotThrottleWorldTest::RunTest(const FString& Parameters)
{
	using namespace DeepPilotTest;
	DeepPilotTest::FShaftScope On(1);
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FPilotWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus"), W.Valid());
	W.HoldAt(Tuning.GetStop(1).SubDepthM + 50.f);
	const FVector2D Start = W.State->SubOffsetM;
	const FVector SubAt = W.Sub->GetActorLocation();
	const FVector Ahead = W.Sub->GetActorForwardVector();

	W.Run->StepThrottle(1);
	W.Run->StepThrottle(1);
	TestEqual(TEXT("Two presses: half ahead"), W.Run->GetThrottleNotch(), 2);
	W.Fly(1.5f);
	TestEqual(TEXT("The lever stays where it was left"), W.Run->GetThrottleNotch(), 2);
	TestTrue(TEXT("Moving ahead along the heading"), W.State->SubOffsetM.X > Start.X + 0.5f && FMath::Abs(W.State->SubOffsetM.Y - Start.Y) < 0.05f);
	TestTrue(TEXT("Speed reported"), W.Run->GetSpeedMps() > 0.5f && W.Run->GetSurgeMps() > 0.5f);
	// Deep.World.Still: the hull moves through a still world (D-022 slid the world aft past a fixed hull).
	TestTrue(TEXT("The world stays put"), W.Root()->GetActorTransform().Equals(FTransform::Identity, 0.01));
	TestTrue(TEXT("The hull moves ahead"), FVector::DotProduct(W.Sub->GetActorLocation() - SubAt, Ahead) > 50.0);

	// All stop: throttle to stop; the sub coasts down.
	W.Run->AllStop();
	TestEqual(TEXT("X: throttle at stop"), W.Run->GetThrottleNotch(), 0);
	const float Coasting = W.Run->GetSpeedMps();
	W.Fly(2.f);
	TestTrue(TEXT("Coasting down"), W.Run->GetSpeedMps() < Coasting * 0.6f);

	// Astern: two notches, no more.
	W.Run->StepThrottle(-1);
	W.Run->StepThrottle(-1);
	W.Run->StepThrottle(-1);
	TestEqual(TEXT("Full astern is two notches"), W.Run->GetThrottleNotch(), -2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepPilotRudderWorldTest, "Deep.World.Pilot.Rudder", DEEP_PILOT_TEST_FLAGS)
bool FDeepPilotRudderWorldTest::RunTest(const FString& Parameters)
{
	using namespace DeepPilotTest;
	DeepPilotTest::FShaftScope On(1);
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FPilotWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus"), W.Valid());
	W.HoldAt(Tuning.GetStop(1).SubDepthM + 50.f);
	const FRotator SubRot = W.Sub->GetActorRotation();
	TestTrue(TEXT("Yaw 0: the root is not turned"), W.Root()->GetActorQuat().Equals(FQuat::Identity, 1e-4f));
	ADeepShaft* Shaft = W.Shaft();
	const FTransform ShaftAt = Shaft ? Shaft->GetActorTransform() : FTransform::Identity;

	W.Run->SetPilotHold(0.f, 1.f);
	W.Fly(2.f);
	W.Run->SetPilotHold(0.f, 0.f);
	const float Yaw = W.State->SubYawDeg;
	AddInfo(FString::Printf(TEXT("Two seconds of starboard rudder: %.1f deg"), Yaw));
	TestTrue(TEXT("Turned to starboard"), Yaw > 10.f);
	// Deep.World.Still: the hull turns and the world stays (D-022 turned the world the other way about a fixed hull).
	TestTrue(TEXT("The world did not turn"), W.Root()->GetActorQuat().Equals(FQuat::Identity, 1e-4f));
	TestTrue(TEXT("The hull turned to starboard by the yaw"), FMath::IsNearlyEqual(FRotator::NormalizeAxis(W.Sub->GetActorRotation().Yaw - SubRot.Yaw), Yaw, 0.05f));
	if (Shaft)
	{
		TestTrue(TEXT("The shaft stayed where it was placed"), Shaft->GetActorTransform().Equals(ShaftAt, 0.01));
	}
	W.Fly(1.f);
	TestTrue(TEXT("Rudder amidships: it carried on a little, then held"), W.State->SubYawDeg > Yaw);

	// Now ahead runs along the new heading.
	const FVector2D Before = W.State->SubOffsetM;
	W.Run->StepThrottle(1);
	W.Fly(2.f);
	const FVector2D Moved = (W.State->SubOffsetM - Before).GetSafeNormal();
	TestTrue(TEXT("Moves along the new heading"), FVector2D::DotProduct(Moved, UDeepPilotRules::Ahead(W.State->SubYawDeg)) > 0.95f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepPilotBallastWorldTest, "Deep.World.Pilot.Ballast", DEEP_PILOT_TEST_FLAGS)
bool FDeepPilotBallastWorldTest::RunTest(const FString& Parameters)
{
	using namespace DeepPilotTest;
	DeepPilotTest::FShaftScope On(1);
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FPilotWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus"), W.Valid());
	const float Mid = Tuning.GetStop(1).SubDepthM + 50.f;
	W.HoldAt(Mid);

	// C held a second: the lever floods and stays; the sub sinks and keeps sinking.
	W.Run->SetPilotHold(1.f, 0.f);
	W.Fly(1.f);
	W.Run->SetPilotHold(0.f, 0.f);
	const float Lever = W.Run->GetBallast();
	TestTrue(TEXT("Flooding"), Lever > 0.5f);
	W.Fly(3.f);
	TestEqual(TEXT("The lever stays where it was left"), W.Run->GetBallast(), Lever);
	TestTrue(TEXT("Sinking"), W.State->SubDepthM > Mid + 2.f);
	TestEqual(TEXT("helm_descend fired"), W.Bus->CountOf(DeepEvent::HelmDescend), 1);

	// Space held: blows through neutral to rise.
	W.Run->SetPilotHold(-1.f, 0.f);
	W.Fly(2.5f);
	W.Run->SetPilotHold(0.f, 0.f);
	TestTrue(TEXT("Blown"), W.Run->GetBallast() < 0.f);
	const float Low = W.State->SubDepthM;
	W.Fly(4.f);
	TestTrue(TEXT("Rising"), W.State->SubDepthM < Low - 1.f);

	// The hull's rated depth: flooded hard, the sub stops at the limit and the Captain says so.
	W.HoldAt(W.State->DepthLimitM() - 3.f);
	W.Run->SetPilotHold(1.f, 0.f);
	W.Fly(6.f);
	TestTrue(TEXT("Never past the limit"), W.State->SubDepthM <= W.State->DepthLimitM() + 1e-3f);
	TestTrue(TEXT("helm_depth_limit fired"), W.Bus->CountOf(DeepEvent::HelmDepthLimit) >= 1);
	W.Run->SetPilotHold(0.f, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepPilotWallWorldTest, "Deep.World.Pilot.Wall", DEEP_PILOT_TEST_FLAGS)
bool FDeepPilotWallWorldTest::RunTest(const FString& Parameters)
{
	using namespace DeepPilotTest;
	DeepPilotTest::FShaftScope On(1);
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FPilotWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus"), W.Valid());
	W.HoldAt(Tuning.GetStop(1).SubDepthM + 50.f);

	// Full ahead into the rock.
	W.Run->StepThrottle(1);
	W.Run->StepThrottle(1);
	W.Run->StepThrottle(1);
	float T = 0.f;
	while (T < 30.f && W.Bus->CountOf(DeepEvent::HullHit) == 0)
	{
		W.Fly(0.05f);
		T += 0.05f;
	}
	AddInfo(FString::Printf(TEXT("Hit the wall after %.1f s at full ahead"), T));
	TestEqual(TEXT("hull_hit fired"), W.Bus->CountOf(DeepEvent::HullHit), 1);
	TestTrue(TEXT("Dented by impact speed"), W.State->HullDamage >= 1 && W.State->HullDamage <= Tuning.Cave.MaxHitDamage);
	TestEqual(TEXT("The knock tripped the throttle to stop"), W.Run->GetThrottleNotch(), 0);
	TestTrue(TEXT("The knock took the way off"), W.Run->GetSurgeMps() < Tuning.Pilot.AheadSpeedMps * 0.5f);
	FVector RealMin, RealMax, BoxMin, BoxMax;
	DeepShaft::SubBoxM(W.Sub, RealMin, RealMax);
	UDeepPilotRules::TurnFreeBoxM(RealMin, RealMax, BoxMin, BoxMax);
	const float Pen = UDeepCaveRules::Contact(W.State->SubOffsetM, W.State->SubDepthM, BoxMin, BoxMax, Tuning.StopDepthsM(), UDeepCaveRules::MinRadiusM(RealMin, RealMax, Tuning.Cave), Tuning.Cave).PenetrationM;
	TestTrue(TEXT("Pushed out: not in the rock"), Pen <= 0.01f);

	// Turning in the passage never jams the hull (the contact box is the same at every heading).
	W.HoldAt(Tuning.GetStop(1).SubDepthM + 50.f);
	const int32 Hits = W.Bus->CountOf(DeepEvent::HullHit);
	const int32 Damage = W.State->HullDamage;
	W.Run->SetPilotHold(0.f, 1.f);
	W.Fly(14.f);
	W.Run->SetPilotHold(0.f, 0.f);
	TestEqual(TEXT("No knock from turning on the spot"), W.Bus->CountOf(DeepEvent::HullHit), Hits);
	TestEqual(TEXT("No dent from turning on the spot"), W.State->HullDamage, Damage);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepPilotParkWorldTest, "Deep.World.Pilot.Park", DEEP_PILOT_TEST_FLAGS)
bool FDeepPilotParkWorldTest::RunTest(const FString& Parameters)
{
	using namespace DeepPilotTest;
	DeepPilotTest::FShaftScope On(1);
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FPilotWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus"), W.Valid());
	const float StopDepth = Tuning.GetStop(1).SubDepthM;

	// Coming in to stop 1, a little high, off centre and turned: levers home.
	W.HoldAt(StopDepth - 3.f);
	W.State->SubOffsetM = FVector2D(1.5f, -1.f);
	W.State->SubYawDeg = 25.f;
	DeepArena::ApplySubDepth(W.World);
	const int32 Arrivals = W.Bus->CountOf(DeepEvent::DescendStop2) + W.Bus->CountOf(DeepEvent::HelmArrive);
	W.Run->StepRun(0.05f);
	TestEqual(TEXT("Slow, stopped, neutral, near: parked at stop 1"), W.State->ParkedStop(), 1);
	TestEqual(TEXT("Park's event fired"), W.Bus->CountOf(DeepEvent::DescendStop2) + W.Bus->CountOf(DeepEvent::HelmArrive), Arrivals + 1);
	TestFalse(TEXT("Not there yet: the hatch stays shut"), W.Sub->IsHatchOpen());

	// The glide home: offset, depth and heading reach the stop's frame in about two seconds.
	float T = 0.f;
	while (T < 6.f && !W.State->IsSettledAtStop())
	{
		W.Fly(0.05f);
		T += 0.05f;
	}
	AddInfo(FString::Printf(TEXT("Glided home in %.2f s"), T));
	TestTrue(TEXT("Settled in the stop's frame"), W.State->IsSettledAtStop());
	TestTrue(TEXT("In about two seconds"), T >= 1.f && T <= 4.f);
	TestTrue(TEXT("At the stop's depth"), W.State->SubDepthM == StopDepth);
	TestTrue(TEXT("Heading as parked"), W.State->SubYawDeg == 0.f);
	// Deep.World.Still: the hull parks in the stop's frame, over its home at the stop's depth (D-022 put the root back).
	const FTransform Parked = DeepArena::StillStopFrame(W.World, 1);
	TestTrue(TEXT("The hull parked in the stop's frame"), W.Sub->GetActorLocation().Equals(Parked.GetLocation(), 0.01) && W.Sub->GetActorQuat().Equals(W.Sub->GetHomeTransform().GetRotation(), 1e-4f));
	TestTrue(TEXT("The world stays put"), W.Root()->GetActorTransform().Equals(FTransform::Identity, 0.01));
	TestTrue(TEXT("The hatch opens"), W.Sub->IsHatchOpen());

	// Casting off (ballast) unparks and shuts the hatch.
	W.Run->SetPilotHold(1.f, 0.f);
	W.Fly(0.6f);
	W.Run->SetPilotHold(0.f, 0.f);
	TestEqual(TEXT("Under way: unparked"), W.State->ParkedStop(), 0);
	TestFalse(TEXT("Hatch shut under way"), W.Sub->IsHatchOpen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepPilotHangWorldTest, "Deep.World.Pilot.YawZeroIdentity", DEEP_PILOT_TEST_FLAGS)
bool FDeepPilotHangWorldTest::RunTest(const FString& Parameters)
{
	using namespace DeepPilotTest;
	DeepPilotTest::FShaftScope On(1);
	// D-022's own placement of the root and the shaft (Deep.World.Still.SameGeometry checks the still world against it).
	DeepTest::FCVarScope D022(TEXT("Deep.World.Still"), 0);
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FPilotWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus"), W.Valid());
	W.HoldAt(Tuning.GetStop(1).SubDepthM + 50.f);
	W.State->SubOffsetM += FVector2D(1.2f, -0.7f);
	DeepArena::ApplySubDepth(W.World);
	ADeepShaft* Shaft = W.Shaft();
	UTEST_NOT_NULL(TEXT("The shaft"), Shaft);

	// Yaw 0: the root and the shaft sit exactly where the old code put them.
	TestTrue(TEXT("Root: the old placement"), W.Root()->GetActorLocation().Equals(W.OldRootCm(), 0.01f) && W.Root()->GetActorQuat().Equals(FQuat::Identity, 1e-5f));
	const FVector Side = W.Sub->GetActorRotation().RotateVector(FVector(W.State->SubOffsetM.X, W.State->SubOffsetM.Y, 0.f));
	const FVector OldShaft = W.Sub->GetActorLocation() + (FVector(0.f, 0.f, W.State->SubDepthM) - Side) * UDeepRules::CmPerM;
	TestTrue(TEXT("Shaft: the old placement"), Shaft->GetActorLocation().Equals(OldShaft, 0.01f) && Shaft->GetActorQuat().Equals(W.Sub->GetActorQuat(), 1e-5f));

	// Turned: both turn the same way about the sub.
	W.State->SubYawDeg = 35.f;
	DeepArena::ApplySubDepth(W.World);
	TestTrue(TEXT("Root turned -35"), FMath::IsNearlyEqual(W.Root()->GetActorRotation().Yaw, -35.f, 0.01f));
	TestTrue(TEXT("Shaft turned -35"), FMath::IsNearlyEqual(FRotator::NormalizeAxis(Shaft->GetActorRotation().Yaw - W.Sub->GetActorRotation().Yaw), -35.f, 0.01f));
	const FVector Sub = W.Sub->GetActorLocation();
	TestTrue(TEXT("Root the same distance from the sub's vertical"), FMath::IsNearlyEqual(FVector2D(W.Root()->GetActorLocation() - Sub).Size(), FVector2D(W.OldRootCm() - Sub).Size(), 0.05f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepPilotLeversWorldTest, "Deep.World.Pilot.Levers", DEEP_PILOT_TEST_FLAGS)
bool FDeepPilotLeversWorldTest::RunTest(const FString& Parameters)
{
	using namespace DeepPilotTest;
	FPilotWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus"), W.Valid());
	UDeepHelmLevers* Levers = DeepHelmLevers::Ensure(W.World);
	UTEST_NOT_NULL(TEXT("Levers in the room"), Levers);
	TestTrue(TEXT("Once only"), DeepHelmLevers::Ensure(W.World) == Levers);
	TestTrue(TEXT("Attached to the room set"), Levers->GetAttachParent() == DeepArena::EnsureRoom(W.World)->GetRootComponent());
	W.Run->TakeHelm();
	W.Run->StepThrottle(1);
	W.Run->StepThrottle(1);
	W.Run->StepThrottle(1);
	W.State->HelmBallast = 1.f;
	for (int32 Step = 0; Step < 40; ++Step)
	{
		Levers->TickComponent(0.05f, LEVELTICK_All, nullptr);
	}
	const float Throw = UDeepTuningSettings::Get().Pilot.LeverThrowDeg;
	TestTrue(TEXT("Full ahead: the throttle lever thrown forward"), FMath::IsNearlyEqual(Levers->GetThrottlePitchDeg(), -Throw, 0.5f));
	TestTrue(TEXT("Flooded: the ballast lever thrown"), FMath::IsNearlyEqual(Levers->GetBallastPitchDeg(), -Throw, 0.5f));
	W.Run->AllStop();
	for (int32 Step = 0; Step < 40; ++Step)
	{
		Levers->TickComponent(0.05f, LEVELTICK_All, nullptr);
	}
	TestTrue(TEXT("All stop: both levers home"), FMath::IsNearlyZero(Levers->GetThrottlePitchDeg(), 0.5f) && FMath::IsNearlyZero(Levers->GetBallastPitchDeg(), 0.5f));
	return true;
}

#endif
