// Tests for the air bubbles' maths (Sim/DeepBubbleSim): when the diver breathes out, and that bubbles rise and wobble
// (the marine snow drifts; bubbles must not read as snow).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Sim/DeepBubbleSim.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepBubbleExhaleTest, "Deep.Sim.Bubbles.Exhale", DEEP_TEST_FLAGS)
bool FDeepBubbleExhaleTest::RunTest(const FString& Parameters)
{
	FDeepBubbleTuning Tuning;
	Tuning.ExhaleStart01 = 0.45f;
	Tuning.ExhaleEnd01 = 0.9f;
	TestFalse(TEXT("Breathing in at the start of a calm cycle"), DeepBubbleSim::IsExhaling(0.5f, 4.f, Tuning));
	TestTrue(TEXT("Breathing out in the middle of it"), DeepBubbleSim::IsExhaling(2.5f, 4.f, Tuning));
	TestFalse(TEXT("A pause at the end"), DeepBubbleSim::IsExhaling(3.8f, 4.f, Tuning));
	TestTrue(TEXT("The next cycle"), DeepBubbleSim::IsExhaling(6.5f, 4.f, Tuning));
	TestTrue(TEXT("Fast breathing twice as often"), DeepBubbleSim::IsExhaling(1.2f, 2.f, Tuning) && DeepBubbleSim::IsExhaling(3.2f, 2.f, Tuning));
	TestFalse(TEXT("No period, no breath"), DeepBubbleSim::IsExhaling(1.f, 0.f, Tuning));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepBubbleRiseTest, "Deep.Sim.Bubbles.RiseAndWobble", DEEP_TEST_FLAGS)
bool FDeepBubbleRiseTest::RunTest(const FString& Parameters)
{
	FDeepBubbleTuning Tuning;
	FDeepBubble Small;
	Small.bAlive = true;
	Small.SizeCm = 0.4f;
	Small.LifeS = 3.f;
	Small.KickCmS = FVector(25.f, 0.f, 0.f);
	FDeepBubble Big = Small;
	Big.SizeCm = 1.4f;
	float MaxSide = 0.f;
	for (int32 i = 0; i < 120; ++i)
	{
		DeepBubbleSim::Step(Small, 1.f / 60.f, Tuning);
		DeepBubbleSim::Step(Big, 1.f / 60.f, Tuning);
		const FVector Drawn = DeepBubbleSim::DrawnCm(Small, Tuning);
		MaxSide = FMath::Max(MaxSide, static_cast<float>(FVector::Dist2D(Drawn, Small.CentreCm)));
	}
	TestTrue(FString::Printf(TEXT("Rises (%.0f cm in 2 s)"), Small.CentreCm.Z), Small.CentreCm.Z > 40.f);
	TestTrue(TEXT("Big bubbles rise faster"), Big.CentreCm.Z > Small.CentreCm.Z);
	TestTrue(TEXT("The push from the mouth dies away"), Small.KickCmS.Size() < 1.f && Small.CentreCm.X < 12.f);
	TestTrue(FString::Printf(TEXT("Wobbles, a little (%.2f cm)"), MaxSide), MaxSide > 0.5f && MaxSide <= Tuning.WobbleCm * 1.5f);
	for (int32 i = 0; i < 120; ++i)
	{
		DeepBubbleSim::Step(Small, 1.f / 60.f, Tuning);
	}
	TestFalse(TEXT("Gone after its life"), Small.bAlive);
	FDeepBubble Ending;
	Ending.SizeCm = 1.f;
	Ending.LifeS = 3.f;
	Ending.AgeS = 2.85f;
	TestTrue(TEXT("Shrinks out instead of popping"), DeepBubbleSim::DrawnSizeCm(Ending) < 0.6f && DeepBubbleSim::DrawnSizeCm(Ending) > 0.f);
	return true;
}

#endif
