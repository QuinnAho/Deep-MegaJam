// Tests for the moving sub's pure rules (Sim/DeepMovingSubRules): depth from height, the hull against a wall, parking
// onto a stop point, the fixed room's faked motion and the airlock's timeline.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Sim/DeepCaveRules.h"
#include "Sim/DeepMovingSubRules.h"
#include "Sim/DeepTuningSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_MOVINGSUB_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMovingSubDepthTest, "Deep.Sim.MovingSub.DepthFromZ", DEEP_MOVINGSUB_TEST_FLAGS)
bool FDeepMovingSubDepthTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("At the datum: depth 0"), UDeepMovingSubRules::DepthFromZ(0.0, 0.0), 0.f);
	TestEqual(TEXT("100 m under the datum"), UDeepMovingSubRules::DepthFromZ(-10000.0, 0.0), 100.f);
	TestEqual(TEXT("Above the datum is negative"), UDeepMovingSubRules::DepthFromZ(500.0, 0.0), -5.f);
	TestEqual(TEXT("A raised datum"), UDeepMovingSubRules::DepthFromZ(-20000.0, 10000.0), 300.f);
	TestEqual(TEXT("ZFromDepth inverts it"), UDeepMovingSubRules::ZFromDepth(UDeepMovingSubRules::DepthFromZ(-31234.0, 500.0), 500.0), -31234.0, 0.01);
	// Deep in a big world the cm stay exact enough (doubles).
	TestEqual(TEXT("600 m down to the cm"), UDeepMovingSubRules::DepthFromZ(-60000.5, 0.0), 600.005f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMovingSubWallTest, "Deep.Sim.MovingSub.ResolveWallHit", DEEP_MOVINGSUB_TEST_FLAGS)
bool FDeepMovingSubWallTest::RunTest(const FString& Parameters)
{
	const FDeepCaveTuning Cave;
	const FDeepPilotTuning Pilot;
	// The wall ahead: its normal points back at the hull (-X).
	const FVector Normal(-1.f, 0.f, 0.f);

	// Head on at 4 m/s: a knock, thrown back by Bounce, damage as HitDamage says.
	FDeepWallHit Hit = UDeepMovingSubRules::ResolveWallHit(FVector(4.f, 0.f, 0.f), Normal, Cave, Pilot);
	TestTrue(TEXT("Head on: a knock"), Hit.bKnock && !Hit.bScrape);
	TestEqual(TEXT("Impact speed"), Hit.ImpactMps, 4.f, 0.001f);
	TestEqual(TEXT("Thrown back by Bounce"), Hit.VelocityMps.X, double(-4.f * Cave.Bounce), 0.001);
	TestEqual(TEXT("Damage from HitDamage"), Hit.Damage, UDeepCaveRules::HitDamage(4.f, Cave));

	// At an angle: the speed along the wall keeps KnockKeepAlong.
	Hit = UDeepMovingSubRules::ResolveWallHit(FVector(3.f, 2.f, -1.f), Normal, Cave, Pilot);
	TestTrue(TEXT("Angled: a knock"), Hit.bKnock);
	TestEqual(TEXT("Along the wall (Y) kept by KnockKeepAlong"), Hit.VelocityMps.Y, double(2.f * Pilot.KnockKeepAlong), 0.001);
	TestEqual(TEXT("Along the wall (Z) too"), Hit.VelocityMps.Z, double(-1.f * Pilot.KnockKeepAlong), 0.001);
	TestTrue(TEXT("Leaving the wall after a knock"), FVector::DotProduct(Hit.VelocityMps, Normal) > 0.0);

	// Slow: a scrape. Into the wall stops; the slide goes on at full speed; no damage.
	Hit = UDeepMovingSubRules::ResolveWallHit(FVector(0.5f, 2.f, 0.f), Normal, Cave, Pilot);
	TestTrue(TEXT("Slow: a scrape"), Hit.bScrape && !Hit.bKnock);
	TestEqual(TEXT("Scrape: no damage"), Hit.Damage, 0);
	TestEqual(TEXT("Scrape: no speed into the wall"), Hit.VelocityMps.X, 0.0, 0.001);
	TestEqual(TEXT("Scrape: the slide goes on"), Hit.VelocityMps.Y, 2.0, 0.001);

	// Just at HitMinSpeedMps is a knock; just under is a scrape.
	TestTrue(TEXT("At the threshold: a knock"), UDeepMovingSubRules::ResolveWallHit(FVector(Cave.HitMinSpeedMps, 0.f, 0.f), Normal, Cave, Pilot).bKnock);
	TestTrue(TEXT("Under it: a scrape"), UDeepMovingSubRules::ResolveWallHit(FVector(Cave.HitMinSpeedMps - 0.01f, 0.f, 0.f), Normal, Cave, Pilot).bScrape);

	// Moving away from the wall, or a missing normal: nothing changes.
	Hit = UDeepMovingSubRules::ResolveWallHit(FVector(-2.f, 1.f, 0.f), Normal, Cave, Pilot);
	TestTrue(TEXT("Moving away: no contact"), !Hit.bKnock && !Hit.bScrape && Hit.ImpactMps == 0.f && Hit.VelocityMps.Equals(FVector(-2.f, 1.f, 0.f)));
	Hit = UDeepMovingSubRules::ResolveWallHit(FVector(2.f, 0.f, 0.f), FVector::ZeroVector, Cave, Pilot);
	TestTrue(TEXT("No normal: no contact"), !Hit.bKnock && !Hit.bScrape);

	// A long normal is the same wall.
	Hit = UDeepMovingSubRules::ResolveWallHit(FVector(4.f, 0.f, 0.f), Normal * 7.f, Cave, Pilot);
	TestEqual(TEXT("Normal length does not matter"), Hit.ImpactMps, 4.f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMovingSubParkTest, "Deep.Sim.MovingSub.ParkNear", DEEP_MOVINGSUB_TEST_FLAGS)
bool FDeepMovingSubParkTest::RunTest(const FString& Parameters)
{
	const FDeepPilotTuning Pilot;
	const FVector Stop(0.f, 0.f, -300.f);
	const float Dt = 0.05f;

	// Out of range, too fast, or a hand on the controls: no glide.
	FDeepParkStep Step = UDeepMovingSubRules::ParkNear(Stop + FVector(Pilot.ParkNearM + 1.f, 0.f, 0.f), 0.f, Stop, 0.f, 0.f, true, Dt, Pilot);
	TestFalse(TEXT("Too far: out of range"), Step.bInRange);
	TestTrue(TEXT("Too far: stays put"), Step.LocationM.Equals(Stop + FVector(Pilot.ParkNearM + 1.f, 0.f, 0.f)));
	TestFalse(TEXT("Too fast"), UDeepMovingSubRules::ParkNear(Stop + FVector(1.f, 0.f, 0.f), 0.f, Stop, 0.f, Pilot.ParkSpeedMps + 0.1f, true, Dt, Pilot).bInRange);
	TestFalse(TEXT("Controls in use"), UDeepMovingSubRules::ParkNear(Stop + FVector(1.f, 0.f, 0.f), 0.f, Stop, 0.f, 0.f, false, Dt, Pilot).bInRange);

	// In range: glides in a straight line onto the point and turns the short way to its heading; arrives in a few seconds.
	FVector Hull = Stop + FVector(3.f, -2.f, 1.f);
	float Yaw = 170.f;
	const float StopYaw = -170.f;
	float LastDistance = FVector::Dist(Hull, Stop);
	bool bArrived = false;
	bool bStraight = true;
	bool bShortWay = true;
	int32 Steps = 0;
	for (; Steps < 200 && !bArrived; ++Steps)
	{
		Step = UDeepMovingSubRules::ParkNear(Hull, Yaw, Stop, StopYaw, 0.f, true, Dt, Pilot);
		TestTrue(TEXT("In range"), Step.bInRange);
		const FVector Before = (Hull - Stop).GetSafeNormal();
		const FVector After = (Step.LocationM - Stop).GetSafeNormal();
		bStraight &= After.IsZero() || FVector::DotProduct(Before, After) > 0.999;
		bShortWay &= FMath::Abs(FRotator::NormalizeAxis(Step.YawDeg - StopYaw)) <= 20.f + 0.01f;
		const float Distance = FVector::Dist(Step.LocationM, Stop);
		TestTrue(TEXT("Never further"), Distance <= LastDistance + 0.0001f);
		LastDistance = Distance;
		Hull = Step.LocationM;
		Yaw = Step.YawDeg;
		bArrived = Step.bArrived;
	}
	TestTrue(TEXT("Arrives"), bArrived);
	TestTrue(TEXT("Within about 3 s"), Steps * Dt <= 3.f);
	TestTrue(TEXT("Straight onto the point"), bStraight);
	TestTrue(TEXT("Turned the short way (through 180, not 0)"), bShortWay);
	TestTrue(TEXT("On the point"), Hull.Equals(Stop));
	TestEqual(TEXT("On its heading"), Yaw, StopYaw, 0.001f);

	// Already there: arrived at once.
	TestTrue(TEXT("On the point: arrived"), UDeepMovingSubRules::ParkNear(Stop, StopYaw, Stop, StopYaw, 0.f, true, Dt, Pilot).bArrived);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMovingSubSwayTest, "Deep.Sim.MovingSub.RoomSway", DEEP_MOVINGSUB_TEST_FLAGS)
bool FDeepMovingSubSwayTest::RunTest(const FString& Parameters)
{
	const FDeepMovingSubTuning T;
	const float QuarterRoll = 0.25f / T.SwayHz;

	// At rest: a faint roll, no lean, no hum, no creak, no shake.
	FDeepRoomSway Rest = UDeepMovingSubRules::RoomSway(0.f, 0.f, QuarterRoll, -1.f, 0.f, T);
	TestEqual(TEXT("Rest: idle roll at its peak"), Rest.RollDeg, T.SwayIdleDeg, 0.001f);
	TestEqual(TEXT("Rest: no lean"), Rest.PitchDeg, 0.f);
	TestEqual(TEXT("Rest: silent"), Rest.Hum01, 0.f);
	TestEqual(TEXT("Rest: no creak"), Rest.Creak01, 0.f);
	TestEqual(TEXT("Rest: no shake"), Rest.ShakeDeg, 0.f);

	// Under way: more roll, capped; full hum at HumFullSpeedMps.
	const FDeepRoomSway Cruise = UDeepMovingSubRules::RoomSway(3.f, 0.f, QuarterRoll, -1.f, 0.f, T);
	TestTrue(TEXT("Faster rolls more"), FMath::Abs(Cruise.RollDeg) > FMath::Abs(Rest.RollDeg));
	TestTrue(TEXT("Never past SwayMaxDeg"), FMath::Abs(UDeepMovingSubRules::RoomSway(100.f, 0.f, QuarterRoll, -1.f, 0.f, T).RollDeg) <= T.SwayMaxDeg + 0.001f);
	TestEqual(TEXT("Full hum"), UDeepMovingSubRules::RoomSway(T.HumFullSpeedMps, 0.f, 0.f, -1.f, 0.f, T).Hum01, 1.f);
	TestTrue(TEXT("Lights swing under way"), FMath::Abs(UDeepMovingSubRules::RoomSway(3.f, 0.f, 0.25f / T.LightSwingHz, -1.f, 0.f, T).LightSwingDeg) > 1.f);
	TestTrue(TEXT("Lights within LightSwingMaxDeg"), FMath::Abs(UDeepMovingSubRules::RoomSway(100.f, 100.f, 0.25f / T.LightSwingHz, -1.f, 0.f, T).LightSwingDeg) <= T.LightSwingMaxDeg + 0.001f);

	// Speeding up tips the view back (up), slowing tips it forward; both creak.
	const FDeepRoomSway Speeding = UDeepMovingSubRules::RoomSway(2.f, 1.f, 0.f, -1.f, 0.f, T);
	const FDeepRoomSway Slowing = UDeepMovingSubRules::RoomSway(2.f, -1.f, 0.f, -1.f, 0.f, T);
	TestTrue(TEXT("Speeding up: view tips up"), Speeding.PitchDeg > 0.f);
	TestTrue(TEXT("Slowing: view tips down"), Slowing.PitchDeg < 0.f);
	TestTrue(TEXT("Both creak"), Speeding.Creak01 > 0.f && Slowing.Creak01 > 0.f);
	TestTrue(TEXT("Lean capped"), FMath::Abs(UDeepMovingSubRules::RoomSway(0.f, 100.f, 0.f, -1.f, 0.f, T).PitchDeg) <= T.SwayMaxDeg + 0.001f);

	// A knock: a hard shake that dies away, and a full creak at once.
	const FDeepRoomSway Knock = UDeepMovingSubRules::RoomSway(0.f, 0.f, 0.f, 0.f, 4.f, T);
	TestEqual(TEXT("Knock: shake at its peak"), FMath::Abs(Knock.ShakeDeg), FMath::Min(T.KnockShakeMaxDeg, 4.f * T.KnockShakeDegPerMps), 0.001f);
	TestEqual(TEXT("Knock: full creak"), Knock.Creak01, 1.f, 0.001f);
	const FDeepRoomSway Later = UDeepMovingSubRules::RoomSway(0.f, 0.f, 0.f, 2.f, 4.f, T);
	TestTrue(TEXT("Shake dies away"), FMath::Abs(Later.ShakeDeg) < 0.01f * FMath::Abs(Knock.ShakeDeg) + 0.001f);
	TestTrue(TEXT("Shake capped"), FMath::Abs(UDeepMovingSubRules::RoomSway(0.f, 0.f, 0.f, 0.f, 50.f, T).ShakeDeg) <= T.KnockShakeMaxDeg + 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMovingSubAirlockTest, "Deep.Sim.MovingSub.AirlockPhase", DEEP_MOVINGSUB_TEST_FLAGS)
bool FDeepMovingSubAirlockTest::RunTest(const FString& Parameters)
{
	const FDeepAirlockTuning T;
	const float Eps = 0.001f;

	TestTrue(TEXT("Not started: idle"), UDeepMovingSubRules::AirlockPhase(-1.f, true, T).Phase == EDeepAirlockPhase::Idle);

	// Out: hiss (clear), fade out, black (move due), fade in, done.
	FDeepAirlockState S = UDeepMovingSubRules::AirlockPhase(0.f, true, T);
	TestTrue(TEXT("Out starts with the hiss, view clear"), S.Phase == EDeepAirlockPhase::Hiss && S.Fade01 == 0.f && !S.bMoveDue);
	S = UDeepMovingSubRules::AirlockPhase(T.HissSeconds + 0.5f * T.FadeOutSeconds, true, T);
	TestTrue(TEXT("Then fades out"), S.Phase == EDeepAirlockPhase::FadeOut && !S.bMoveDue);
	TestEqual(TEXT("Halfway black"), S.Fade01, 0.5f, Eps);
	S = UDeepMovingSubRules::AirlockPhase(T.HissSeconds + T.FadeOutSeconds + Eps, true, T);
	TestTrue(TEXT("Black: the move is due"), S.Phase == EDeepAirlockPhase::Black && S.Fade01 == 1.f && S.bMoveDue);
	S = UDeepMovingSubRules::AirlockPhase(T.HissSeconds + T.FadeOutSeconds + T.BlackSeconds + 0.5f * T.FadeInSeconds, true, T);
	TestTrue(TEXT("Fades in, still moved"), S.Phase == EDeepAirlockPhase::FadeIn && S.bMoveDue);
	TestEqual(TEXT("Halfway clear"), S.Fade01, 0.5f, Eps);
	S = UDeepMovingSubRules::AirlockPhase(UDeepMovingSubRules::AirlockSeconds(true, T) + Eps, true, T);
	TestTrue(TEXT("Done, clear"), S.Phase == EDeepAirlockPhase::Done && S.Fade01 == 0.f && S.bMoveDue);

	// Back: no hiss first (it plays at the move), so it is shorter by HissSeconds.
	TestEqual(TEXT("Back is shorter by the hiss"), UDeepMovingSubRules::AirlockSeconds(false, T), UDeepMovingSubRules::AirlockSeconds(true, T) - T.HissSeconds, Eps);
	TestTrue(TEXT("Back starts fading at once"), UDeepMovingSubRules::AirlockPhase(0.f, false, T).Phase == EDeepAirlockPhase::FadeOut);
	TestTrue(TEXT("Back: black after the fade"), UDeepMovingSubRules::AirlockPhase(T.FadeOutSeconds + Eps, false, T).bMoveDue);

	// The fade never jumps: sampled finely, Fade01 changes by little per step.
	float Last = 0.f;
	float Biggest = 0.f;
	const float Step = 0.01f;
	for (float At = 0.f; At <= UDeepMovingSubRules::AirlockSeconds(true, T) + Step; At += Step)
	{
		const float Fade = UDeepMovingSubRules::AirlockPhase(At, true, T).Fade01;
		Biggest = FMath::Max(Biggest, FMath::Abs(Fade - Last));
		Last = Fade;
	}
	TestTrue(TEXT("No jump in the fade"), Biggest <= Step / FMath::Min(T.FadeOutSeconds, T.FadeInSeconds) + Eps);

	// Zero-length phases: straight through, no division by zero.
	FDeepAirlockTuning Instant;
	Instant.HissSeconds = Instant.FadeOutSeconds = Instant.BlackSeconds = Instant.FadeInSeconds = 0.f;
	TestTrue(TEXT("All zero: done at once"), UDeepMovingSubRules::AirlockPhase(0.f, true, Instant).Phase == EDeepAirlockPhase::Done);

	// The valve: a tap does not open it; holding does.
	TestEqual(TEXT("Valve shut"), UDeepMovingSubRules::ValveTurn01(0.f, T), 0.f);
	TestTrue(TEXT("A tap is not enough"), UDeepMovingSubRules::ValveTurn01(0.1f, T) < 1.f);
	TestEqual(TEXT("Held: open"), UDeepMovingSubRules::ValveTurn01(T.ValveHoldSeconds, T), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMovingSubTuningTest, "Deep.Sim.MovingSub.Tuning", DEEP_MOVINGSUB_TEST_FLAGS)
bool FDeepMovingSubTuningTest::RunTest(const FString& Parameters)
{
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	// The room's set is far above anything the dive reaches (the deepest helm limit).
	float Deepest = 0.f;
	for (const float Limit : Tuning.Sub.DepthLimitsM)
	{
		Deepest = FMath::Max(Deepest, Limit);
	}
	TestTrue(TEXT("Room set above the deepest dive's world"), Tuning.Sub.RoomSetLocationM.Z - Tuning.MovingSub.DepthDatumZM > Deepest);
	// Out and back together stay short: the first 90 seconds are diving, not watching fades.
	const float Cycle = Tuning.Airlock.ValveHoldSeconds + UDeepMovingSubRules::AirlockSeconds(true, Tuning.Airlock);
	TestTrue(TEXT("Leaving takes under 3 s"), Cycle > 0.f && Cycle < 3.f);
	TestTrue(TEXT("Coming back takes under 2 s"), UDeepMovingSubRules::AirlockSeconds(false, Tuning.Airlock) < 2.f);
	return true;
}

#endif
