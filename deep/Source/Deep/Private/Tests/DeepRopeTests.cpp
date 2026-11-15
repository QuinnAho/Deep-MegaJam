// Tests for the rope the tether and the harpoon line are drawn with (Sim/DeepRopeSim, UDeepRules::RopeShownLengthCm).
// Visual only, but where it parts and that it falls are what the player reads from a cut.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Sim/DeepRopeSim.h"
#include "Sim/DeepRules.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepRopeTest
{
	const FVector Hatch(0.f, 0.f, 0.f);
	const FVector Diver(2000.f, 0.f, -500.f);

	FDeepRopeSim Settled(float LengthCm, float Seconds = 4.f)
	{
		FDeepRopeSim Sim;
		Sim.Init(21, Hatch, Diver);
		for (float T = 0.f; T < Seconds; T += 1.f / 60.f)
		{
			Sim.Advance(1.f / 60.f, &Hatch, &Diver, LengthCm, 120.f, 2.2f, 24);
		}
		return Sim;
	}

	/** How far the middle point hangs below the straight line between the ends (cm). */
	float MidSagCm(const FDeepRopeSim& Sim)
	{
		const int32 Mid = Sim.Pos.Num() / 2;
		const FVector OnLine = FMath::ClosestPointOnSegment(Sim.Pos[Mid], Sim.Pos[0], Sim.Pos.Last());
		return static_cast<float>(OnLine.Z - Sim.Pos[Mid].Z);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRopeShownLengthTest, "Deep.Sim.Rope.ShownLength", DEEP_TEST_FLAGS)
bool FDeepRopeShownLengthTest::RunTest(const FString& Parameters)
{
	FDeepRopeTuning Rope;
	Rope.SlackShare = 0.1f;
	Rope.SlackMaxM = 2.f;
	TestEqual(TEXT("Taut: the straight line"), UDeepRules::RopeShownLengthCm(1000.f, 4000.f, true, Rope), 1000.f, 0.01f);
	TestEqual(TEXT("Slack: a share of the distance"), UDeepRules::RopeShownLengthCm(1000.f, 4000.f, false, Rope), 1100.f, 0.01f);
	TestEqual(TEXT("Slack capped by SlackMaxM"), UDeepRules::RopeShownLengthCm(3000.f, 9000.f, false, Rope), 3200.f, 0.01f);
	TestEqual(TEXT("Never more than the line on the reel"), UDeepRules::RopeShownLengthCm(3950.f, 4000.f, false, Rope), 4000.f, 0.01f);
	TestEqual(TEXT("At or past full length: straight"), UDeepRules::RopeShownLengthCm(4100.f, 4000.f, false, Rope), 4100.f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRopeSagTest, "Deep.Sim.Rope.SagAndTaut", DEEP_TEST_FLAGS)
bool FDeepRopeSagTest::RunTest(const FString& Parameters)
{
	using namespace DeepRopeTest;
	const float Straight = static_cast<float>(FVector::Dist(Hatch, Diver));
	const FDeepRopeSim Slack = Settled(Straight * 1.06f);
	const FDeepRopeSim Taut = Settled(Straight);
	TestTrue(TEXT("Ends stay pinned"), Slack.Pos[0].Equals(Hatch, 0.01) && Slack.Pos.Last().Equals(Diver, 0.01));
	TestTrue(FString::Printf(TEXT("With slack the middle sags (%.0f cm)"), MidSagCm(Slack)), MidSagCm(Slack) > 100.f);
	TestTrue(FString::Printf(TEXT("Taut hangs nearly straight (%.0f cm)"), MidSagCm(Taut)), MidSagCm(Taut) < 0.25f * MidSagCm(Slack));
	TestEqual(TEXT("Whole"), Slack.CountLinked(), Slack.NumLinks());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRopeBiteCutTest, "Deep.Sim.Rope.BiteCut", DEEP_TEST_FLAGS)
bool FDeepRopeBiteCutTest::RunTest(const FString& Parameters)
{
	using namespace DeepRopeTest;
	FDeepRopeSim Sim = Settled(static_cast<float>(FVector::Dist(Hatch, Diver)) * 1.03f);
	// The jaws close 60% of the way from the hatch, a little to the side of the line.
	const FVector Bite = FMath::Lerp(Hatch, Diver, 0.6) + FVector(0.f, 80.f, 0.f);
	const int32 Link = Sim.NearestLink(Bite);
	TestTrue(FString::Printf(TEXT("Parts near the bite (link %d of %d)"), Link, Sim.NumLinks()), Link >= 11 && Link <= 13);
	const FVector CutEndUp = Sim.Pos[Link];
	const FVector CutEndDown = Sim.Pos[Link + 1];
	Sim.Cut(Link, 900.f, 8);
	TestFalse(TEXT("That link is parted"), Sim.IsLinked(Link));
	TestEqual(TEXT("Only that link"), Sim.CountLinked(), Sim.NumLinks() - 1);
	for (int32 i = 0; i < 60; ++i)
	{
		Sim.Advance(1.f / 60.f, &Hatch, &Diver, static_cast<float>(FVector::Dist(Hatch, Diver)) * 1.03f, 260.f, 2.2f, 24);
	}
	TestTrue(TEXT("A second later the two ends are well apart"), FVector::Dist(Sim.Pos[Link], Sim.Pos[Link + 1]) > 3.f * FVector::Dist(CutEndUp, CutEndDown));
	// The upper end first recoils up the line toward the hatch, then hangs and sinks.
	for (int32 i = 0; i < 120; ++i)
	{
		Sim.Advance(1.f / 60.f, &Hatch, &Diver, static_cast<float>(FVector::Dist(Hatch, Diver)) * 1.03f, 260.f, 2.2f, 24);
	}
	TestTrue(TEXT("Three seconds later the hatch-side end has fallen"), Sim.Pos[Link].Z < CutEndUp.Z - 50.f);
	TestTrue(TEXT("The diver-side end falls"), Sim.Pos[Link + 1].Z < CutEndDown.Z - 50.f);
	TestTrue(TEXT("Both pins still hold"), Sim.Pos[0].Equals(Hatch, 0.01) && Sim.Pos.Last().Equals(Diver, 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRopeHatchCutTest, "Deep.Sim.Rope.HatchCut", DEEP_TEST_FLAGS)
bool FDeepRopeHatchCutTest::RunTest(const FString& Parameters)
{
	using namespace DeepRopeTest;
	const float Length = static_cast<float>(FVector::Dist(Hatch, Diver)) * 1.03f;
	FDeepRopeSim Sim = Settled(Length);
	const FVector Before = Sim.Pos[1];
	Sim.Cut(0, 900.f, 8);
	// One step: the loose end whips away from the hatch, toward the diver.
	Sim.Advance(1.f / 60.f, &Hatch, &Diver, Length, 260.f, 2.2f, 24);
	const FVector Toward = (Diver - Hatch).GetSafeNormal();
	TestTrue(TEXT("Whips toward the diver"), FVector::DotProduct(Sim.Pos[1] - Before, Toward) > 5.0);
	for (int32 i = 0; i < 60; ++i)
	{
		Sim.Advance(1.f / 60.f, &Hatch, &Diver, Length, 260.f, 2.2f, 24);
	}
	TestTrue(TEXT("A second later the loose end is well clear of the hatch"), FVector::Dist(Sim.Pos[1], Hatch) > 300.0);
	TestTrue(TEXT("And sinking"), Sim.Pos[1].Z < Before.Z - 100.f);
	TestTrue(TEXT("Still on the diver"), Sim.Pos.Last().Equals(Diver, 0.01));
	TestEqual(TEXT("Everything below the cut still joined"), Sim.CountLinked(), Sim.NumLinks() - 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRopeFrameRateTest, "Deep.Sim.Rope.FixedStep", DEEP_TEST_FLAGS)
bool FDeepRopeFrameRateTest::RunTest(const FString& Parameters)
{
	using namespace DeepRopeTest;
	FDeepRopeSim A;
	FDeepRopeSim B;
	A.Init(11, Hatch, Diver);
	B.Init(11, Hatch, Diver);
	const float Length = static_cast<float>(FVector::Dist(Hatch, Diver)) * 1.05f;
	for (int32 i = 0; i < 120; ++i)
	{
		A.Advance(1.f / 60.f, &Hatch, &Diver, Length, 120.f, 2.2f, 24);
	}
	for (int32 i = 0; i < 60; ++i)
	{
		B.Advance(1.f / 30.f, &Hatch, &Diver, Length, 120.f, 2.2f, 24);
	}
	TestTrue(TEXT("Same rope at 30 and 60 fps"), A.Pos[5].Equals(B.Pos[5], 5.0));
	return true;
}

#endif
