// Tests for the pure rules in Sim/DeepRules. Each test builds its own tuning so tuning changes never break them;
// Deep.Sim.Tuning.DesignInvariants checks the shipped tuning for things the design depends on.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepRulesTest
{
	TArray<FDeepZone> Zones()
	{
		TArray<FDeepZone> Out;
		auto Add = [&Out](const TCHAR* Name, float Min) { FDeepZone Z; Z.Name = Name; Z.MinDepthM = Min; Out.Add(Z); };
		// Deliberately unsorted.
		Add(TEXT("Midnight"), 1000.f);
		Add(TEXT("Sunlight"), 0.f);
		Add(TEXT("Twilight"), 200.f);
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRulesDepthTest, "Deep.Sim.Rules.Depth", DEEP_TEST_FLAGS)
bool FDeepRulesDepthTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("At the anchor depth equals the stop depth"), UDeepRules::DepthM(500.f, 500.f, 300.f), 300.f, 0.001f);
	TestEqual(TEXT("10 m below the anchor"), UDeepRules::DepthM(-1000.f, 0.f, 300.f), 310.f, 0.001f);
	TestEqual(TEXT("5 m above the anchor"), UDeepRules::DepthM(500.f, 0.f, 300.f), 295.f, 0.001f);
	TestEqual(TEXT("Never above the surface"), UDeepRules::DepthM(100000.f, 0.f, 300.f), 0.f, 0.001f);

	const TArray<FDeepZone> Zones = DeepRulesTest::Zones();
	TestEqual(TEXT("Zone at 0"), UDeepRules::ZoneName(0.f, Zones), FString(TEXT("Sunlight")));
	TestEqual(TEXT("Zone just above Twilight"), UDeepRules::ZoneName(199.9f, Zones), FString(TEXT("Sunlight")));
	TestEqual(TEXT("Zone boundary is inclusive"), UDeepRules::ZoneName(200.f, Zones), FString(TEXT("Twilight")));
	TestEqual(TEXT("Zone in unsorted table"), UDeepRules::ZoneName(5000.f, Zones), FString(TEXT("Midnight")));
	TestEqual(TEXT("Negative depth falls back to the shallowest"), UDeepRules::ZoneName(-5.f, Zones), FString(TEXT("Sunlight")));
	TestEqual(TEXT("No zones gives -1"), UDeepRules::ZoneIndex(10.f, TArray<FDeepZone>()), INDEX_NONE);
	TestEqual(TEXT("No zones gives empty name"), UDeepRules::ZoneName(10.f, TArray<FDeepZone>()), FString());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRulesTetherLengthTest, "Deep.Sim.Rules.TetherLength", DEEP_TEST_FLAGS)
bool FDeepRulesTetherLengthTest::RunTest(const FString& Parameters)
{
	FDeepTetherTuning T;
	T.BaseLengthM = 40.f;
	T.LengthPerStopM = 20.f;
	TestEqual(TEXT("Stop 1"), UDeepRules::TetherLengthM(1, T), 40.f, 0.001f);
	TestEqual(TEXT("Stop 2"), UDeepRules::TetherLengthM(2, T), 60.f, 0.001f);
	TestEqual(TEXT("Stop 3"), UDeepRules::TetherLengthM(3, T), 80.f, 0.001f);
	TestEqual(TEXT("Stop 0 counts as 1"), UDeepRules::TetherLengthM(0, T), 40.f, 0.001f);
	TestEqual(TEXT("Negative stop counts as 1"), UDeepRules::TetherLengthM(-4, T), 40.f, 0.001f);
	T.BaseLengthM = -10.f;
	T.LengthPerStopM = 0.f;
	TestEqual(TEXT("Never negative"), UDeepRules::TetherLengthM(1, T), 0.f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRulesTetherSolveTest, "Deep.Sim.Rules.TetherSolve", DEEP_TEST_FLAGS)
bool FDeepRulesTetherSolveTest::RunTest(const FString& Parameters)
{
	FDeepTetherTuning T;
	T.TautStrain = 0.95f;
	const FVector Anchor(100.f, 200.f, 300.f);

	{
		const FDeepTetherSolve S = UDeepRules::SolveTether(Anchor, Anchor + FVector(0, 0, -2000.f), 40.f, T);
		TestEqual(TEXT("Slack: distance"), S.DistanceM, 20.f, 0.001f);
		TestEqual(TEXT("Slack: strain"), S.Strain, 0.5f, 0.001f);
		TestEqual(TEXT("Slack: left"), S.LeftM, 20.f, 0.001f);
		TestFalse(TEXT("Slack: not taut"), S.bTaut);
		TestFalse(TEXT("Slack: not clamped"), S.bClamped);
		TestTrue(TEXT("Slack: position unchanged"), S.ClampedPositionCm.Equals(Anchor + FVector(0, 0, -2000.f), 0.01));
	}
	{
		const FDeepTetherSolve S = UDeepRules::SolveTether(Anchor, Anchor + FVector(3900.f, 0, 0), 40.f, T);
		TestTrue(TEXT("Near the end: taut"), S.bTaut);
		TestFalse(TEXT("Near the end: not clamped"), S.bClamped);
	}
	{
		const FDeepTetherSolve S = UDeepRules::SolveTether(Anchor, Anchor + FVector(3000.f, 4000.f, 0), 40.f, T);
		TestEqual(TEXT("Past the end: distance is the wanted distance"), S.DistanceM, 50.f, 0.001f);
		TestEqual(TEXT("Past the end: strain clamps to 1"), S.Strain, 1.f, 0.001f);
		TestEqual(TEXT("Past the end: nothing left"), S.LeftM, 0.f, 0.001f);
		TestTrue(TEXT("Past the end: taut"), S.bTaut);
		TestTrue(TEXT("Past the end: clamped"), S.bClamped);
		TestTrue(TEXT("Past the end: pulled back along the line"), S.ClampedPositionCm.Equals(Anchor + FVector(2400.f, 3200.f, 0), 0.01));
		TestEqual(TEXT("Past the end: clamped point is exactly at length"), FVector::Dist(S.ClampedPositionCm, Anchor), 4000.0, 0.01);
	}
	{
		const FDeepTetherSolve S = UDeepRules::SolveTether(Anchor, Anchor, 40.f, T);
		TestEqual(TEXT("At the anchor: zero strain"), S.Strain, 0.f, 0.001f);
		TestTrue(TEXT("At the anchor: stays"), S.ClampedPositionCm.Equals(Anchor, 0.01));
	}
	{
		const FDeepTetherSolve S = UDeepRules::SolveTether(Anchor, Anchor + FVector(500.f, 0, 0), 0.f, T);
		TestTrue(TEXT("Zero length: held at anchor"), S.ClampedPositionCm.Equals(Anchor, 0.01));
		TestTrue(TEXT("Zero length: taut"), S.bTaut);
		TestTrue(TEXT("Zero length: clamped"), S.bClamped);
		TestEqual(TEXT("Zero length: strain 1"), S.Strain, 1.f, 0.001f);
	}
	{
		const FDeepTetherSolve S = UDeepRules::SolveTether(Anchor, Anchor + FVector(500.f, 0, 0), -10.f, T);
		TestEqual(TEXT("Negative length treated as zero"), S.LengthM, 0.f, 0.001f);
		TestTrue(TEXT("Negative length: held at anchor"), S.ClampedPositionCm.Equals(Anchor, 0.01));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRulesWinchTest, "Deep.Sim.Rules.Winch", DEEP_TEST_FLAGS)
bool FDeepRulesWinchTest::RunTest(const FString& Parameters)
{
	FDeepTetherTuning T;
	T.WinchSpeedMps = 7.f;
	T.WinchArriveM = 3.f;
	const FVector Anchor(0, 0, 0);

	const FVector A = UDeepRules::WinchStep(FVector(0, 0, -3000.f), Anchor, 1.f, T);
	TestTrue(TEXT("Reels 7 m in a second"), A.Equals(FVector(0, 0, -2300.f), 0.01));

	const FVector B = UDeepRules::WinchStep(FVector(0, 0, -500.f), Anchor, 1.f, T);
	TestTrue(TEXT("Stops at the arrive distance, no overshoot"), B.Equals(FVector(0, 0, -300.f), 0.01));
	TestTrue(TEXT("Arrived counts as home"), UDeepRules::IsHome(B, Anchor, T));

	const FVector C = UDeepRules::WinchStep(FVector(100.f, 0, 0), Anchor, 1.f, T);
	TestTrue(TEXT("Inside the arrive distance it does not push away"), C.Equals(FVector(100.f, 0, 0), 0.01));

	T.RelicWinchSpeedMps = 4.f;
	TestEqual(TEXT("Without the relic the winch runs at its speed"), UDeepRules::WinchSpeedMps(false, T), 7.f);
	TestEqual(TEXT("With the relic aboard it runs at the relic speed"), UDeepRules::WinchSpeedMps(true, T), 4.f);
	T.RelicWinchSpeedMps = -1.f;
	TestEqual(TEXT("Never negative"), UDeepRules::WinchSpeedMps(true, T), 0.f);
	T.RelicWinchSpeedMps = 4.f;

	const FVector D = UDeepRules::WinchStep(FVector(0, 0, -3000.f), Anchor, 0.f, T);
	TestTrue(TEXT("Zero dt does nothing"), D.Equals(FVector(0, 0, -3000.f), 0.01));

	const FVector E = UDeepRules::WinchStep(FVector(0, 0, -3000.f), Anchor, -1.f, T);
	TestTrue(TEXT("Negative dt does nothing"), E.Equals(FVector(0, 0, -3000.f), 0.01));

	const FVector F = UDeepRules::WinchStep(Anchor, Anchor, 1.f, T);
	TestTrue(TEXT("At the anchor stays put"), F.Equals(Anchor, 0.01));

	TestFalse(TEXT("Far away is not home"), UDeepRules::IsHome(FVector(0, 0, -3000.f), Anchor, T));

	// Diagonal: the step is along the line.
	const FVector G = UDeepRules::WinchStep(FVector(3000.f, 4000.f, 0), Anchor, 1.f, T);
	TestEqual(TEXT("Diagonal step length"), FVector::Dist(G, FVector(3000.f, 4000.f, 0)), 700.0, 0.01);
	TestEqual(TEXT("Diagonal step toward the anchor"), FVector::Dist(G, Anchor), 4300.0, 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRulesWeightTest, "Deep.Sim.Rules.Weight", DEEP_TEST_FLAGS)
bool FDeepRulesWeightTest::RunTest(const FString& Parameters)
{
	FDeepWeightTuning T;
	T.FreeKg = 10.f;
	T.SinkMpsPerKg = 0.1f;
	T.MaxSinkMps = 3.f;
	T.SwimSlowdownPerKg = 0.01f;
	T.MinSwimMultiplier = 0.25f;
	T.OverloadKg = 15.f;

	const FDeepWeightResult Empty = UDeepRules::SolveWeight(0.f, T);
	TestEqual(TEXT("Empty: no sink"), Empty.SinkSpeedMps, 0.f, 0.0001f);
	TestEqual(TEXT("Empty: full speed"), Empty.SwimMultiplier, 1.f, 0.0001f);
	TestFalse(TEXT("Empty: not overloaded"), Empty.bOverloaded);

	const FDeepWeightResult Free = UDeepRules::SolveWeight(10.f, T);
	TestEqual(TEXT("10 kg is free: no sink"), Free.SinkSpeedMps, 0.f, 0.0001f);
	TestEqual(TEXT("10 kg is free: full speed"), Free.SwimMultiplier, 1.f, 0.0001f);

	const FDeepWeightResult Twelve = UDeepRules::SolveWeight(12.f, T);
	TestEqual(TEXT("12 kg: 2 kg excess"), Twelve.ExcessKg, 2.f, 0.0001f);
	TestEqual(TEXT("12 kg: sinks 0.2 m/s"), Twelve.SinkSpeedMps, 0.2f, 0.0001f);
	TestEqual(TEXT("12 kg: 2% slower"), Twelve.SwimMultiplier, 0.98f, 0.0001f);
	TestFalse(TEXT("12 kg: can still swim up"), Twelve.bOverloaded);

	const FDeepWeightResult Fifteen = UDeepRules::SolveWeight(15.f, T);
	TestTrue(TEXT("15 kg: overloaded at the threshold"), Fifteen.bOverloaded);

	const FDeepWeightResult Relic = UDeepRules::SolveWeight(500.f, T);
	TestEqual(TEXT("Huge load: sink capped"), Relic.SinkSpeedMps, 3.f, 0.0001f);
	TestEqual(TEXT("Huge load: speed floored"), Relic.SwimMultiplier, 0.25f, 0.0001f);

	const FDeepWeightResult Negative = UDeepRules::SolveWeight(-5.f, T);
	TestEqual(TEXT("Negative mass treated as zero"), Negative.CarriedKg, 0.f, 0.0001f);
	TestFalse(TEXT("Negative mass not overloaded"), Negative.bOverloaded);

	TArray<FDeepCargoItem> Cargo;
	FDeepCargoItem Item;
	Item.Kg = 6.f; Item.Value = 70; Cargo.Add(Item);
	Item.Kg = 2.f; Item.Value = 20; Cargo.Add(Item);
	Item.Kg = -1.f; Item.Value = -50; Cargo.Add(Item);
	TestEqual(TEXT("Cargo kg ignores negatives"), UDeepRules::CargoKg(Cargo), 8.f, 0.0001f);
	TestEqual(TEXT("Cargo value ignores negatives"), UDeepRules::CargoValue(Cargo), 90);
	TestEqual(TEXT("Empty cargo"), UDeepRules::CargoKg(TArray<FDeepCargoItem>()), 0.f, 0.0001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRulesSwimTest, "Deep.Sim.Rules.Swim", DEEP_TEST_FLAGS)
bool FDeepRulesSwimTest::RunTest(const FString& Parameters)
{
	FDeepSwimTuning S;
	S.SwimSpeedMps = 4.f;
	S.SprintMultiplier = 1.5f;
	FDeepWeightTuning W;
	W.FreeKg = 10.f;
	W.SinkMpsPerKg = 0.1f;
	W.SwimSlowdownPerKg = 0.01f;
	W.OverloadKg = 15.f;

	const FDeepWeightResult Light = UDeepRules::SolveWeight(0.f, W);
	TestTrue(TEXT("Light diver swims forward at full speed"), UDeepRules::SwimVelocityMps(FVector(1, 0, 0), false, Light, S).Equals(FVector(4, 0, 0), 0.001));
	TestTrue(TEXT("Sprint multiplies speed"), UDeepRules::SwimVelocityMps(FVector(1, 0, 0), true, Light, S).Equals(FVector(6, 0, 0), 0.001));
	TestTrue(TEXT("Light diver can rise"), UDeepRules::SwimVelocityMps(FVector(0, 0, 1), false, Light, S).Equals(FVector(0, 0, 4), 0.001));
	TestTrue(TEXT("Wish longer than 1 is clamped"), UDeepRules::SwimVelocityMps(FVector(3, 0, 0), false, Light, S).Equals(FVector(4, 0, 0), 0.001));
	TestTrue(TEXT("No input, no weight: still"), UDeepRules::SwimVelocityMps(FVector::ZeroVector, false, Light, S).IsNearlyZero(0.001));

	const FDeepWeightResult Twelve = UDeepRules::SolveWeight(12.f, W);
	TestTrue(TEXT("Heavy diver idles downward"), UDeepRules::SwimVelocityMps(FVector::ZeroVector, false, Twelve, S).Equals(FVector(0, 0, -0.2), 0.001));
	TestTrue(TEXT("Heavy diver rises slower, minus sink"), UDeepRules::SwimVelocityMps(FVector(0, 0, 1), false, Twelve, S).Equals(FVector(0, 0, 4 * 0.98 - 0.2), 0.001));

	const FDeepWeightResult Over = UDeepRules::SolveWeight(20.f, W);
	const FVector Up = UDeepRules::SwimVelocityMps(FVector(0, 0, 1), true, Over, S);
	TestTrue(TEXT("Overloaded diver cannot rise even sprinting"), Up.Z < 0.0);
	TestEqual(TEXT("Overloaded diver sinks at sink speed when trying to rise"), Up.Z, -1.0, 0.001);
	const FVector Diagonal = UDeepRules::SwimVelocityMps(FVector(1, 0, 1).GetSafeNormal(), false, Over, S);
	TestTrue(TEXT("Overloaded diver keeps horizontal swim"), Diagonal.X > 0.0);
	TestTrue(TEXT("Overloaded diver diagonal still sinks"), Diagonal.Z < 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRulesPatienceTest, "Deep.Sim.Rules.Patience", DEEP_TEST_FLAGS)
bool FDeepRulesPatienceTest::RunTest(const FString& Parameters)
{
	FDeepPatienceTuning T;
	T.MaxSeconds = 100.f;
	T.DrainPerSecond = 1.f;
	T.HuntedMultiplier = 2.f;
	T.RefillSmall = 0.05f;
	T.RefillMedium = 0.1f;
	T.RefillLarge = 0.2f;
	T.RefillPearl = 0.2f;
	T.RefillRelic = 0.2f;
	T.WarningFractions = { 0.5f, 0.25f, 0.1f };

	TestEqual(TEXT("Drains 1/s"), UDeepRules::PatienceStep(100.f, 1.f, false, T), 99.f, 0.0001f);
	TestEqual(TEXT("Hunted drains twice as fast"), UDeepRules::PatienceStep(100.f, 1.f, true, T), 98.f, 0.0001f);
	TestEqual(TEXT("Never below zero"), UDeepRules::PatienceStep(0.5f, 10.f, true, T), 0.f, 0.0001f);
	TestEqual(TEXT("Negative dt does not refill"), UDeepRules::PatienceStep(50.f, -5.f, false, T), 50.f, 0.0001f);
	TestEqual(TEXT("Above max is clamped"), UDeepRules::PatienceStep(500.f, 0.f, false, T), 100.f, 0.0001f);

	TestEqual(TEXT("Small refills 5 s"), UDeepRules::PatienceRefillSeconds(EDeepSalvageSize::Small, T), 5.f, 0.0001f);
	TestEqual(TEXT("Medium refills 10 s"), UDeepRules::PatienceRefillSeconds(EDeepSalvageSize::Medium, T), 10.f, 0.0001f);
	TestEqual(TEXT("Large refills 20 s"), UDeepRules::PatienceRefillSeconds(EDeepSalvageSize::Large, T), 20.f, 0.0001f);
	TestEqual(TEXT("Refill caps at max"), UDeepRules::PatienceAfterSalvage(95.f, EDeepSalvageSize::Large, T), 100.f, 0.0001f);
	TestEqual(TEXT("Refill from zero"), UDeepRules::PatienceAfterSalvage(0.f, EDeepSalvageSize::Medium, T), 10.f, 0.0001f);

	TestEqual(TEXT("Crossing 50%"), UDeepRules::PatienceWarningCrossed(51.f, 50.f, T), 0);
	TestEqual(TEXT("Crossing 25%"), UDeepRules::PatienceWarningCrossed(26.f, 24.f, T), 1);
	TestEqual(TEXT("Crossing 10%"), UDeepRules::PatienceWarningCrossed(10.5f, 9.f, T), 2);
	TestEqual(TEXT("Crossing several at once reports the most severe"), UDeepRules::PatienceWarningCrossed(60.f, 5.f, T), 2);
	TestEqual(TEXT("No crossing"), UDeepRules::PatienceWarningCrossed(40.f, 30.f, T), INDEX_NONE);
	TestEqual(TEXT("Rising does not warn"), UDeepRules::PatienceWarningCrossed(20.f, 60.f, T), INDEX_NONE);
	TestEqual(TEXT("Sitting at a threshold does not re-warn"), UDeepRules::PatienceWarningCrossed(50.f, 49.f, T), INDEX_NONE);
	T.MaxSeconds = 0.f;
	TestEqual(TEXT("Zero max never warns"), UDeepRules::PatienceWarningCrossed(1.f, 0.f, T), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRulesSettlementTest, "Deep.Sim.Rules.Settlement", DEEP_TEST_FLAGS)
bool FDeepRulesSettlementTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("40% of 100"), UDeepRules::DirectorCut(100, 40), 40);
	TestEqual(TEXT("40% of 101 rounds up"), UDeepRules::DirectorCut(101, 40), 41);
	TestEqual(TEXT("40% of 1 rounds up to 1"), UDeepRules::DirectorCut(1, 40), 1);
	TestEqual(TEXT("40% of 20 is 8"), UDeepRules::DirectorCut(20, 40), 8);
	TestEqual(TEXT("40% of 0"), UDeepRules::DirectorCut(0, 40), 0);
	TestEqual(TEXT("Negative gross gives no cut"), UDeepRules::DirectorCut(-50, 40), 0);
	TestEqual(TEXT("Percent over 100 is clamped"), UDeepRules::DirectorCut(50, 250), 50);
	TestEqual(TEXT("Huge gross does not overflow"), UDeepRules::DirectorCut(2000000000, 40), 800000000);

	FDeepEconomyTuning E;
	E.DirectorCutPercent = 40;

	const FDeepSettlement A = UDeepRules::Settle(290, { 20 }, E);
	TestEqual(TEXT("Gross"), A.Gross, 290);
	TestEqual(TEXT("Cut 116"), A.DirectorCut, 116);
	TestEqual(TEXT("Fees"), A.Fees, 20);
	TestEqual(TEXT("Net 154"), A.Net, 154);

	const FDeepSettlement B = UDeepRules::Settle(20, { 20, 30 }, E);
	TestEqual(TEXT("Net never below zero"), B.Net, 0);
	TestEqual(TEXT("Fees still itemised in total"), B.Fees, 50);

	const FDeepSettlement C = UDeepRules::Settle(100, { -500 }, E);
	TestEqual(TEXT("Negative fees count as zero"), C.Net, 60);

	const FDeepSettlement D = UDeepRules::Settle(-100, {}, E);
	TestEqual(TEXT("Negative gross settles to zero"), D.Net, 0);
	TestEqual(TEXT("Negative gross reported as zero"), D.Gross, 0);

	TestEqual(TEXT("Fee reduces credits"), UDeepRules::ApplyFee(100, 30), 70);
	TestEqual(TEXT("Fee never takes credits below zero"), UDeepRules::ApplyFee(10, 30), 0);
	TestEqual(TEXT("Negative fee does nothing"), UDeepRules::ApplyFee(10, -30), 10);
	TestEqual(TEXT("Negative credits treated as zero"), UDeepRules::ApplyFee(-10, 5), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRulesHullTest, "Deep.Sim.Rules.Hull", DEEP_TEST_FLAGS)
bool FDeepRulesHullTest::RunTest(const FString& Parameters)
{
	FDeepEconomyTuning E;
	E.HullPrices = { 50, 200 };

	TestEqual(TEXT("Rating 1 -> 2 costs 50"), UDeepRules::NextHullPrice(1, E), 50);
	TestEqual(TEXT("Rating 2 -> 3 costs 200"), UDeepRules::NextHullPrice(2, E), 200);
	TestEqual(TEXT("Maxed gives -1"), UDeepRules::NextHullPrice(3, E), INDEX_NONE);
	TestEqual(TEXT("Rating 0 treated as 1"), UDeepRules::NextHullPrice(0, E), 50);
	TestEqual(TEXT("Max rating"), UDeepRules::MaxHullRating(E), 3);

	const FDeepHullPurchase A = UDeepRules::TryBuyHull(49, 1, E);
	TestFalse(TEXT("49 cannot buy"), A.bBought);
	TestEqual(TEXT("49 keeps credits"), A.Credits, 49);

	const FDeepHullPurchase B = UDeepRules::TryBuyHull(50, 1, E);
	TestTrue(TEXT("Exactly 50 buys"), B.bBought);
	TestEqual(TEXT("Rating 2"), B.HullRating, 2);
	TestEqual(TEXT("Zero left"), B.Credits, 0);
	TestEqual(TEXT("Spent 50"), B.Spent, 50);

	const FDeepHullPurchase C = UDeepRules::TryBuyHull(1000, 1, E);
	TestEqual(TEXT("Only one rating per settlement"), C.HullRating, 2);
	TestEqual(TEXT("Rich diver keeps the change"), C.Credits, 950);

	const FDeepHullPurchase D = UDeepRules::TryBuyHull(1000, 3, E);
	TestFalse(TEXT("Maxed hull buys nothing"), D.bBought);
	TestEqual(TEXT("Maxed keeps credits"), D.Credits, 1000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRulesBailoutTest, "Deep.Sim.Rules.Bailout", DEEP_TEST_FLAGS)
bool FDeepRulesBailoutTest::RunTest(const FString& Parameters)
{
	FDeepBailoutTuning T;
	T.AirSeconds = 30.f;
	T.SlowWindowSeconds = 5.f;
	T.SlowDrainScale = 0.5f;

	TestEqual(TEXT("Normal drain"), UDeepRules::BailoutStep(30.f, 1.f, T), 29.f, 0.0001f);
	TestEqual(TEXT("Slow drain inside window"), UDeepRules::BailoutStep(4.f, 1.f, T), 3.5f, 0.0001f);
	TestEqual(TEXT("Step straddling the window edge"), UDeepRules::BailoutStep(6.f, 2.f, T), 4.5f, 0.0001f);
	TestEqual(TEXT("Never negative"), UDeepRules::BailoutStep(1.f, 10.f, T), 0.f, 0.0001f);
	TestEqual(TEXT("Negative dt does nothing"), UDeepRules::BailoutStep(10.f, -1.f, T), 10.f, 0.0001f);

	// Last 5 seconds take 10 real seconds at half speed: total 25 + 10 = 35 s.
	float Air = T.AirSeconds;
	float Real = 0.f;
	while (Air > 0.f && Real < 100.f)
	{
		Air = UDeepRules::BailoutStep(Air, 0.1f, T);
		Real += 0.1f;
	}
	TestEqual(TEXT("30 s of air lasts 35 real seconds"), Real, 35.f, 0.11f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRulesArenaTest, "Deep.Sim.Rules.SalvageSpot", DEEP_TEST_FLAGS)
bool FDeepRulesArenaTest::RunTest(const FString& Parameters)
{
	FDeepSalvageSpot Spot;
	Spot.AngleDeg = 0.f;
	Spot.OutM = 10.f;
	Spot.BelowM = 5.f;
	TestTrue(TEXT("Angle 0 is +X, below is -Z"), UDeepRules::SalvageSpotOffsetCm(Spot).Equals(FVector(1000.f, 0, -500.f), 0.01));
	Spot.AngleDeg = 90.f;
	TestTrue(TEXT("Angle 90 is +Y"), UDeepRules::SalvageSpotOffsetCm(Spot).Equals(FVector(0, 1000.f, -500.f), 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRulesGrabTest, "Deep.Sim.Rules.Grab", DEEP_TEST_FLAGS)
bool FDeepRulesGrabTest::RunTest(const FString& Parameters)
{
	FDeepSwimTuning T;
	T.ReachM = 3.f;
	T.GrabConeDeg = 45.f;
	const FVector Eye = FVector::ZeroVector;
	const FVector View(1, 0, 0);

	TestEqual(TEXT("No candidates"), UDeepRules::PickGrabTarget(Eye, View, {}, {}, T), INDEX_NONE);
	TestEqual(TEXT("In front, in reach"), UDeepRules::PickGrabTarget(Eye, View, { FVector(200, 0, 0) }, {}, T), 0);
	TestEqual(TEXT("Too far"), UDeepRules::PickGrabTarget(Eye, View, { FVector(400, 0, 0) }, {}, T), INDEX_NONE);
	TestEqual(TEXT("Too far by centre but surface in reach"), UDeepRules::PickGrabTarget(Eye, View, { FVector(400, 0, 0) }, { 1.5f }, T), 0);
	TestEqual(TEXT("Behind"), UDeepRules::PickGrabTarget(Eye, View, { FVector(-200, 0, 0) }, {}, T), INDEX_NONE);
	TestEqual(TEXT("Outside the cone"), UDeepRules::PickGrabTarget(Eye, View, { FVector(100, 200, 0) }, {}, T), INDEX_NONE);
	TestEqual(TEXT("Inside the cone"), UDeepRules::PickGrabTarget(Eye, View, { FVector(200, 100, 0) }, {}, T), 0);
	TestEqual(TEXT("Picks the nearest"), UDeepRules::PickGrabTarget(Eye, View, { FVector(250, 0, 0), FVector(120, 10, 0), FVector(200, 0, 0) }, {}, T), 1);
	TestEqual(TEXT("Touching counts even behind"), UDeepRules::PickGrabTarget(Eye, View, { FVector(-20, 0, 0) }, { 0.5f }, T), 0);
	TestEqual(TEXT("Zero view direction only grabs what it touches"), UDeepRules::PickGrabTarget(Eye, FVector::ZeroVector, { FVector(200, 0, 0) }, {}, T), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepTuningInvariantsTest, "Deep.Sim.Tuning.DesignInvariants", DEEP_TEST_FLAGS)
bool FDeepTuningInvariantsTest::RunTest(const FString& Parameters)
{
	const UDeepTuningSettings& S = UDeepTuningSettings::Get();

	TestTrue(TEXT("At least one stop"), S.NumStops() >= 1);
	TestTrue(TEXT("At least one zone"), S.Zones.Num() >= 1);
	TestEqual(TEXT("One stop per hull rating"), S.NumStops(), UDeepRules::MaxHullRating(S.Economy));
	for (int32 Stop = 2; Stop <= S.NumStops(); ++Stop)
	{
		TestTrue(FString::Printf(TEXT("Stop %d is deeper than stop %d"), Stop, Stop - 1), S.GetStop(Stop).SubDepthM > S.GetStop(Stop - 1).SubDepthM);
	}
	// GDD: stops at 100, 300 and 500 m, one per zone; the relic at 600 m, below the last stop.
	for (int32 Stop = 1; Stop <= S.NumStops(); ++Stop)
	{
		TestEqual(FString::Printf(TEXT("Stop %d sits in zone %d"), Stop, Stop), UDeepRules::ZoneIndex(S.GetStop(Stop).SubDepthM, S.Zones), FMath::Min(Stop - 1, S.Zones.Num() - 1));
	}
	TestTrue(TEXT("The relic is below the last stop"), S.Arena.RelicDepthM > S.GetStop(S.NumStops()).SubDepthM);
	for (int32 Index = 1; Index < S.Economy.HullPrices.Num(); ++Index)
	{
		TestTrue(TEXT("Hull prices rise"), S.Economy.HullPrices[Index] > S.Economy.HullPrices[Index - 1]);
	}

	for (const EDeepSalvageSize Size : { EDeepSalvageSize::Small, EDeepSalvageSize::Medium, EDeepSalvageSize::Large, EDeepSalvageSize::Pearl })
	{
		FDeepSalvageKind Kind;
		TestTrue(FString::Printf(TEXT("A kind exists for size %d"), static_cast<int32>(Size)), S.FindSalvageKind(Size, Kind));
	}

	// "Richer salvage is farther and lower"; "Pearl: only beyond tether range"; the first piece is a free grab.
	const float Stop1TetherM = UDeepRules::TetherLengthM(1, S.Tether);
	const float Reach = S.Swim.ReachM;
	bool bAnyInReach = false;
	for (const FDeepSalvageSpot& Spot : S.Arena.SalvageSpots)
	{
		const float DistM = static_cast<float>(UDeepRules::SalvageSpotOffsetCm(Spot).Size()) / UDeepRules::CmPerM;
		TestTrue(TEXT("Salvage is below the anchor"), Spot.BelowM > 0.f);
		if (Spot.Size == EDeepSalvageSize::Pearl)
		{
			TestTrue(TEXT("Pearls are out of reach on the stop-1 tether"), DistM - Reach > Stop1TetherM);
			TestTrue(TEXT("Pearls are reachable on the stop-2 tether"), DistM - Reach <= UDeepRules::TetherLengthM(2, S.Tether));
		}
		else
		{
			TestTrue(TEXT("Scrap is reachable on the stop-1 tether"), DistM - Reach <= Stop1TetherM);
			bAnyInReach = true;
		}
	}
	TestTrue(TEXT("Some scrap is reachable at stop 1"), bAnyInReach);

	// "Too heavy to swim up, so winch home": a single heavy piece overloads the diver.
	FDeepSalvageKind Large;
	if (S.FindSalvageKind(EDeepSalvageSize::Large, Large))
	{
		TestTrue(TEXT("One large piece overloads"), UDeepRules::SolveWeight(Large.Kg, S.Weight).bOverloaded);
	}
	FDeepSalvageKind Small;
	if (S.FindSalvageKind(EDeepSalvageSize::Small, Small))
	{
		TestFalse(TEXT("One small piece is free"), UDeepRules::SolveWeight(Small.Kg, S.Weight).bOverloaded);
	}
	TestTrue(TEXT("Winch is faster than max sink"), S.Tether.WinchSpeedMps > S.Weight.MaxSinkMps);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
