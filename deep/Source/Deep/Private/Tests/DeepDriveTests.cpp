// D-070: the helm drives like a game sub. Deep.Sim.Drive.* are the pure rules (Sim/DeepPilotRules::StepDrive,
// MouseSteer): time to full speed, distance to rest after letting go, the turn's build-up and drift, strafe, heave, and
// that nothing latches. Deep.Sim.Drive.Wall* is the rock's answer (UDeepMovingSubRules::ResolveWallHitKeeping, HitDamage).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Sim/DeepCaveRules.h"
#include "Sim/DeepMovingSubRules.h"
#include "Sim/DeepPilotRules.h"
#include "Sim/DeepTuningTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_DRIVE_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepDriveTest
{
	constexpr float Dt = 0.05f;

	FDeepDriveInput Hold(float Surge, float Turn = 0.f, float Heave = 0.f, float Strafe = 0.f)
	{
		FDeepDriveInput In;
		In.Surge = Surge;
		In.Turn = Turn;
		In.Heave = Heave;
		In.Strafe = Strafe;
		return In;
	}

	/** Steps until Done (or MaxS); returns the seconds it took (MaxS + 1 if never). */
	template <typename FDone>
	float StepUntil(FDeepPilotMotion& M, const FDeepDriveInput& In, const FDeepDriveTuning& T, float MaxS, FDone Done)
	{
		for (float S = 0.f; S <= MaxS + UE_KINDA_SMALL_NUMBER; S += Dt)
		{
			if (Done(M))
			{
				return S;
			}
			M = UDeepPilotRules::StepDrive(M, In, Dt, T);
		}
		return MaxS + 1.f;
	}

	float Surge(const FDeepPilotMotion& M)
	{
		return FVector2D::DotProduct(M.VelocityMps, UDeepPilotRules::Ahead(M.YawDeg));
	}

	float Sway(const FDeepPilotMotion& M)
	{
		return FVector2D::DotProduct(M.VelocityMps, UDeepPilotRules::Starboard(M.YawDeg));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepDriveSpeedTest, "Deep.Sim.Drive.Speed", DEEP_DRIVE_TEST_FLAGS)
bool FDeepDriveSpeedTest::RunTest(const FString& Parameters)
{
	using namespace DeepDriveTest;
	const FDeepDriveTuning T;
	FDeepPilotMotion M;
	// W held from rest: full ahead in under 2 s, and not at once (it has weight).
	FDeepPilotMotion First = UDeepPilotRules::StepDrive(M, Hold(1.f), Dt, T);
	TestTrue(TEXT("one step does not jump to full speed"), Surge(First) < T.AheadSpeedMps * 0.25f);
	const float ToFull = StepUntil(M, Hold(1.f), T, 4.f, [&T](const FDeepPilotMotion& X) { return Surge(X) >= T.AheadSpeedMps * 0.99f; });
	AddInfo(FString::Printf(TEXT("full ahead (%.1f m/s) in %.2f s"), T.AheadSpeedMps, ToFull));
	TestTrue(TEXT("full ahead in under 2 s"), ToFull < 2.f);
	TestTrue(TEXT("ahead along the heading, no sideways way"), FMath::Abs(Sway(M)) < 0.01f && FMath::IsNearlyZero(M.YawDeg));

	// Let go: at rest within about 1.5 s, a short coast.
	FVector2D Travel = FVector2D::ZeroVector;
	float ToRest = 0.f;
	for (; ToRest < 5.f && UDeepPilotRules::SpeedMps(M) > 0.02f; ToRest += Dt)
	{
		M = UDeepPilotRules::StepDrive(M, Hold(0.f), Dt, T);
		Travel += M.VelocityMps * Dt;
	}
	AddInfo(FString::Printf(TEXT("let go at full ahead: at rest in %.2f s after %.2f m"), ToRest, Travel.Size()));
	TestTrue(TEXT("at rest within 1.5 s of letting go"), ToRest <= 1.5f);
	TestTrue(TEXT("coasts a little (more than 1 m), not far (under 3.5 m)"), Travel.Size() > 1.f && Travel.Size() < 3.5f);

	// S held: astern, slower than ahead.
	M = FDeepPilotMotion();
	StepUntil(M, Hold(-1.f), T, 4.f, [](const FDeepPilotMotion&) { return false; });
	TestTrue(TEXT("astern at the astern speed"), FMath::IsNearlyEqual(Surge(M), -T.AsternSpeedMps, 0.01f));
	TestTrue(TEXT("astern is slower than ahead"), T.AsternSpeedMps < T.AheadSpeedMps);

	// From full ahead, S held: it brakes through zero into astern (no reverse gear to wait for).
	M = FDeepPilotMotion();
	StepUntil(M, Hold(1.f), T, 3.f, [](const FDeepPilotMotion&) { return false; });
	const float Reverse = StepUntil(M, Hold(-1.f), T, 4.f, [](const FDeepPilotMotion& X) { return Surge(X) < 0.f; });
	TestTrue(TEXT("reversing stops the way within 1.5 s"), Reverse <= 1.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepDriveTurnTest, "Deep.Sim.Drive.Turn", DEEP_DRIVE_TEST_FLAGS)
bool FDeepDriveTurnTest::RunTest(const FString& Parameters)
{
	using namespace DeepDriveTest;
	const FDeepDriveTuning T;
	FDeepPilotMotion M;
	// D held: the turn rate builds over about 0.3 s, not at once.
	M = UDeepPilotRules::StepDrive(M, Hold(0.f, 1.f), Dt, T);
	TestTrue(TEXT("the first step turns, but not at full rate"), M.YawRateDegS > 0.f && M.YawRateDegS < T.TurnRateDegS * 0.5f);
	const float Built = StepUntil(M, Hold(0.f, 1.f), T, 2.f, [&T](const FDeepPilotMotion& X) { return X.YawRateDegS >= T.TurnRateDegS * 0.99f; }) + Dt;
	AddInfo(FString::Printf(TEXT("turn rate %.0f deg/s built in %.2f s"), T.TurnRateDegS, Built));
	TestTrue(TEXT("full turn rate within 0.35 s"), Built <= 0.35f + UE_KINDA_SMALL_NUMBER);
	TestTrue(TEXT("turned to starboard"), M.YawDeg > 0.f);
	// Let go: it turns on a little, then stops.
	const float YawLetGo = M.YawDeg;
	const float Stopped = StepUntil(M, Hold(0.f), T, 2.f, [](const FDeepPilotMotion& X) { return FMath::IsNearlyZero(X.YawRateDegS); });
	AddInfo(FString::Printf(TEXT("let go: %.1f deg more in %.2f s"), M.YawDeg - YawLetGo, Stopped));
	TestTrue(TEXT("a short overrun (some degrees, under 15)"), M.YawDeg - YawLetGo > 1.f && M.YawDeg - YawLetGo < 15.f);
	TestTrue(TEXT("the turn stops within 0.35 s"), Stopped <= 0.35f + UE_KINDA_SMALL_NUMBER);
	// A held turn at a held heading is level and in place: no way made.
	TestTrue(TEXT("a turn in place makes no way"), M.VelocityMps.IsNearlyZero(0.001f) && FMath::IsNearlyZero(M.HeaveMps));

	// Full ahead, full turn: the hull drifts a little wide (the way lags the heading), never a skid.
	M = FDeepPilotMotion();
	StepUntil(M, Hold(1.f), T, 3.f, [](const FDeepPilotMotion&) { return false; });
	StepUntil(M, Hold(1.f, 1.f), T, 1.f, [](const FDeepPilotMotion&) { return false; });
	const float Lag = FMath::Abs(FRotator::NormalizeAxis(M.YawDeg - FMath::RadiansToDegrees(FMath::Atan2(M.VelocityMps.Y, M.VelocityMps.X))));
	AddInfo(FString::Printf(TEXT("turning at full ahead: the way lags the heading by %.1f deg"), Lag));
	TestTrue(TEXT("a little drift (over 1 deg, under 20)"), Lag > 1.f && Lag < 20.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepDriveAxesTest, "Deep.Sim.Drive.StrafeAndHeave", DEEP_DRIVE_TEST_FLAGS)
bool FDeepDriveAxesTest::RunTest(const FString& Parameters)
{
	using namespace DeepDriveTest;
	const FDeepDriveTuning T;
	FDeepPilotMotion M;
	M.YawDeg = 30.f;
	// E held: sideways to starboard at the strafe speed, the heading kept, no way ahead.
	StepUntil(M, Hold(0.f, 0.f, 0.f, 1.f), T, 3.f, [](const FDeepPilotMotion&) { return false; });
	TestTrue(TEXT("strafe at its speed"), FMath::IsNearlyEqual(Sway(M), T.StrafeSpeedMps, 0.01f));
	TestTrue(TEXT("strafe keeps the heading"), FMath::IsNearlyEqual(M.YawDeg, 30.f, 0.01f));
	TestTrue(TEXT("strafe makes no way ahead"), FMath::Abs(Surge(M)) < 0.01f);
	// Q held: to port.
	StepUntil(M, Hold(0.f, 0.f, 0.f, -1.f), T, 4.f, [](const FDeepPilotMotion&) { return false; });
	TestTrue(TEXT("Q strafes to port"), FMath::IsNearlyEqual(Sway(M), -T.StrafeSpeedMps, 0.01f));

	// C held: down at the heave speed within about a second; Space: up.
	M = FDeepPilotMotion();
	const float Down = StepUntil(M, Hold(0.f, 0.f, 1.f), T, 3.f, [&T](const FDeepPilotMotion& X) { return X.HeaveMps >= T.HeaveSpeedMps * 0.99f; });
	TestTrue(TEXT("C sinks at full heave within 1.2 s"), Down <= 1.2f);
	TestTrue(TEXT("heave moves no way across"), M.VelocityMps.IsNearlyZero(0.001f));
	StepUntil(M, Hold(0.f, 0.f, -1.f), T, 4.f, [](const FDeepPilotMotion&) { return false; });
	TestTrue(TEXT("Space rises"), FMath::IsNearlyEqual(M.HeaveMps, -T.HeaveSpeedMps, 0.01f));
	const float Rest = StepUntil(M, Hold(0.f), T, 3.f, [](const FDeepPilotMotion& X) { return FMath::IsNearlyZero(X.HeaveMps); });
	TestTrue(TEXT("let go, the heave stops within a second"), Rest <= 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepDriveLatchTest, "Deep.Sim.Drive.NothingLatches", DEEP_DRIVE_TEST_FLAGS)
bool FDeepDriveLatchTest::RunTest(const FString& Parameters)
{
	using namespace DeepDriveTest;
	const FDeepDriveTuning T;
	FDeepPilotMotion M;
	// Every key held at once, then all let go: within 2 s the sub is still, with no turn left.
	StepUntil(M, Hold(1.f, -1.f, 1.f, 1.f), T, 3.f, [](const FDeepPilotMotion&) { return false; });
	TestTrue(TEXT("held, it moves"), UDeepPilotRules::SpeedMps(M) > 1.f && M.YawRateDegS < 0.f);
	StepUntil(M, Hold(0.f), T, 2.f, [](const FDeepPilotMotion&) { return false; });
	TestTrue(TEXT("let go: no way"), M.VelocityMps.IsNearlyZero(0.001f));
	TestTrue(TEXT("let go: no heave"), FMath::IsNearlyZero(M.HeaveMps));
	TestTrue(TEXT("let go: no turn"), FMath::IsNearlyZero(M.YawRateDegS));
	// The held input is all there is: the same input from rest twice gives the same motion (no state but the motion).
	const FDeepPilotMotion A = UDeepPilotRules::StepDrive(FDeepPilotMotion(), Hold(0.5f, 0.2f), Dt, T);
	const FDeepPilotMotion B = UDeepPilotRules::StepDrive(FDeepPilotMotion(), Hold(0.5f, 0.2f), Dt, T);
	TestTrue(TEXT("no hidden state"), A.VelocityMps.Equals(B.VelocityMps) && A.YawRateDegS == B.YawRateDegS);
	// Out of range input is clamped: a cheat's 5 is full ahead, no more.
	FDeepPilotMotion C;
	StepUntil(C, Hold(5.f), T, 4.f, [](const FDeepPilotMotion&) { return false; });
	TestTrue(TEXT("input clamps at full"), FMath::IsNearlyEqual(Surge(C), T.AheadSpeedMps, 0.01f));
	TestTrue(TEXT("idle input is idle"), Hold(0.f).IsIdle() && !Hold(0.f, 0.f, 0.f, 0.3f).IsIdle());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepDriveMouseTest, "Deep.Sim.Drive.MouseSteer", DEEP_DRIVE_TEST_FLAGS)
bool FDeepDriveMouseTest::RunTest(const FString& Parameters)
{
	const FDeepDriveTuning T;
	// Only while thrusting.
	TestTrue(TEXT("no thrust, no steer"), UDeepPilotRules::MouseSteer(40.f, 20.f, 0.f, T).IsZero());
	// The view far to starboard: a full turn to starboard; half as far: about half.
	TestTrue(TEXT("far right: full starboard"), FMath::IsNearlyEqual(UDeepPilotRules::MouseSteer(T.MouseSteerFullTurnDeg + 10.f, 0.f, 1.f, T).X, 1.f));
	const float Half = UDeepPilotRules::MouseSteer(T.MouseSteerFullTurnDeg * 0.5f, 0.f, 1.f, T).X;
	TestTrue(TEXT("half way: part turn"), Half > 0.3f && Half < 0.6f);
	TestTrue(TEXT("left: port"), UDeepPilotRules::MouseSteer(-20.f, 0.f, 1.f, T).X < 0.f);
	TestTrue(TEXT("dead ahead: no turn"), FMath::IsNearlyZero(UDeepPilotRules::MouseSteer(1.f, 0.f, 1.f, T).X));
	// Up climbs (heave is + down), down dives.
	TestTrue(TEXT("look up: climb"), UDeepPilotRules::MouseSteer(0.f, 20.f, 1.f, T).Y < -0.5f);
	TestTrue(TEXT("look down: dive"), UDeepPilotRules::MouseSteer(0.f, -40.f, 1.f, T).Y >= 1.f - UE_KINDA_SMALL_NUMBER);
	// Wraps: 350 off is 10 to port.
	TestTrue(TEXT("wraps round"), UDeepPilotRules::MouseSteer(350.f, 0.f, 1.f, T).X < 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepDriveWallTest, "Deep.Sim.Drive.Wall", DEEP_DRIVE_TEST_FLAGS)
bool FDeepDriveWallTest::RunTest(const FString& Parameters)
{
	const FDeepDriveTuning T;
	const FDeepCaveTuning Cave;
	const FVector Normal(-1.f, 0.f, 0.f);
	// A glancing knock along the rock keeps most of the speed along it, and the drive is not stopped.
	const FVector Glance(3.f, 2.f, 0.f);
	const FDeepWallHit Hit = UDeepMovingSubRules::ResolveWallHitKeeping(Glance, Normal, Cave, T.KnockKeepAlong, T.KnockBounce);
	TestTrue(TEXT("3 m/s into the rock is a knock"), Hit.bKnock);
	TestTrue(TEXT("most of the speed along the rock stays"), Hit.VelocityMps.Y >= Glance.Y * 0.8f);
	TestTrue(TEXT("a small bounce off it"), Hit.VelocityMps.X < 0.f && Hit.VelocityMps.X > -Glance.X * 0.5f);
	// A slow touch slides with all of its speed along the rock.
	const FDeepWallHit Slide = UDeepMovingSubRules::ResolveWallHitKeeping(FVector(0.5f, 3.f, 0.f), Normal, Cave, T.KnockKeepAlong, T.KnockBounce);
	TestTrue(TEXT("a slow touch is a scrape"), Slide.bScrape && Slide.Damage == 0);
	TestTrue(TEXT("a scrape keeps all the slide"), FMath::IsNearlyEqual(float(Slide.VelocityMps.Y), 3.f) && FMath::IsNearlyZero(float(Slide.VelocityMps.X)));
	// Damage grows with the speed into the rock: a scrape costs nothing at once, head on at full ahead the most a knock can.
	int32 Last = 0;
	bool bGrows = true;
	for (float Mps = 0.f; Mps <= 6.f; Mps += 0.25f)
	{
		const int32 D = UDeepCaveRules::HitDamage(Mps, Cave);
		bGrows = bGrows && D >= Last;
		Last = D;
	}
	TestTrue(TEXT("damage never falls as the impact grows"), bGrows);
	TestEqual(TEXT("a touch under the knock speed: no dent"), UDeepCaveRules::HitDamage(Cave.HitMinSpeedMps - 0.1f, Cave), 0);
	TestTrue(TEXT("half speed into the rock dents less than full"), UDeepCaveRules::HitDamage(T.AheadSpeedMps * 0.5f, Cave) < UDeepCaveRules::HitDamage(T.AheadSpeedMps, Cave));
	TestEqual(TEXT("head on at full ahead: the cap"), UDeepCaveRules::HitDamage(T.AheadSpeedMps, Cave), Cave.MaxHitDamage);
	TestTrue(TEXT("a breach stays possible: a few full knocks"), Cave.MaxHitDamage * 4 >= Cave.HullBreachDamage);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepDriveIdleTest, "Deep.Sim.Drive.Idle", DEEP_DRIVE_TEST_FLAGS)
bool FDeepDriveIdleTest::RunTest(const FString& Parameters)
{
	// D-070, Quinn's idle: the hull breathes at rest, bounded, smooth, faded out by speed and small with a diver out.
	const FDeepIdleTuning T;
	TestTrue(TEXT("roll, pitch and yaw wander are under a degree"), T.RollDeg < 1.f && T.PitchDeg < 1.f && T.YawDeg < 1.f);
	TestTrue(TEXT("the heave is a few cm"), T.HeaveCm > 0.f && T.HeaveCm <= 6.f);
	const double Dt = 1.0 / 60.0;
	// The steepest a frame can step: each axis is 0.7 + 0.3 of its amplitude, the second sine at 1.7x the rate.
	const auto MaxStep = [Dt](float Amp, float Hz) { return Amp * UE_TWO_PI * Hz * (0.7 + 0.3 * 1.7) * Dt * 1.01 + 1e-5; };
	FDeepIdleBob Last = UDeepMovingSubRules::IdleBob(0.0, 1.f, T);
	bool bBounded = true;
	bool bSmooth = true;
	float Widest = 0.f;
	for (double S = Dt; S < 300.0; S += Dt)
	{
		const FDeepIdleBob B = UDeepMovingSubRules::IdleBob(S, 1.f, T);
		bBounded = bBounded && FMath::Abs(B.HeaveCm) <= T.HeaveCm + 1e-3f && FMath::Abs(B.RollDeg) <= T.RollDeg + 1e-3f
			&& FMath::Abs(B.PitchDeg) <= T.PitchDeg + 1e-3f && FMath::Abs(B.YawDeg) <= T.YawDeg + 1e-3f;
		bSmooth = bSmooth && FMath::Abs(B.HeaveCm - Last.HeaveCm) <= MaxStep(T.HeaveCm, T.HeaveHz) && FMath::Abs(B.RollDeg - Last.RollDeg) <= MaxStep(T.RollDeg, T.RollHz)
			&& FMath::Abs(B.PitchDeg - Last.PitchDeg) <= MaxStep(T.PitchDeg, T.PitchHz) && FMath::Abs(B.YawDeg - Last.YawDeg) <= MaxStep(T.YawDeg, T.YawHz);
		Widest = FMath::Max(Widest, FMath::Abs(B.HeaveCm));
		Last = B;
	}
	AddInfo(FString::Printf(TEXT("idle heave reaches %.2f cm of %.2f"), Widest, T.HeaveCm));
	TestTrue(TEXT("bounded by its amplitudes over 5 minutes"), bBounded);
	TestTrue(TEXT("smooth: no step between frames"), bSmooth);
	TestTrue(TEXT("it does move (at least half its heave)"), Widest >= T.HeaveCm * 0.5f);
	TestTrue(TEXT("the same time gives the same breath on every machine"), UDeepMovingSubRules::IdleBob(42.0, 1.f, T).HeaveCm == UDeepMovingSubRules::IdleBob(42.0, 1.f, T).HeaveCm);
	TestEqual(TEXT("amount 0: still"), UDeepMovingSubRules::IdleBob(17.0, 0.f, T).RollDeg, 0.f);
	// Full at rest, none when driving, small with a diver out of the hatch.
	TestEqual(TEXT("at rest: full"), UDeepMovingSubRules::IdleTarget(0.f, false, T), 1.f);
	TestEqual(TEXT("driving: none"), UDeepMovingSubRules::IdleTarget(T.FadeOutMps + 1.f, false, T), 0.f);
	TestTrue(TEXT("slowing: part"), UDeepMovingSubRules::IdleTarget(T.FadeOutMps * 0.5f, false, T) > 0.4f && UDeepMovingSubRules::IdleTarget(T.FadeOutMps * 0.5f, false, T) < 0.6f);
	TestEqual(TEXT("a diver out: small"), UDeepMovingSubRules::IdleTarget(0.f, true, T), T.DiverOutShare);
	// The amount eases: one frame moves it only a little toward a new target.
	const float Step = UDeepMovingSubRules::IdleStep(0.f, 1.f, float(Dt), T);
	TestTrue(TEXT("eases in, no jump"), Step > 0.f && Step < 0.05f);
	return true;
}

#endif
