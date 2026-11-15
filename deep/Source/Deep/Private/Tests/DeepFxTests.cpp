// Deep.Sim.Rules.Fx.*: the bubble and silt rules (pure), and Deep.World.Fx: breath bubbles follow the breath loop.

#include "CoreMinimal.h"
#include "FX/DeepFxRules.h"
#include "FX/DeepFxSettings.h"
#include "Misc/AutomationTest.h"

#define DEEP_FX_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepFxBreathPuffTest, "Deep.Sim.Rules.Fx.BreathPuffs", DEEP_FX_TEST_FLAGS)
bool FDeepFxBreathPuffTest::RunTest(const FString&)
{
	UDeepFxSettings* S = NewObject<UDeepFxSettings>();
	S->ExhalePuffPhases = { 0.47f, 0.56f, 0.66f, 0.77f };
	S->CalmBreathPeriodS = 4.f;
	S->FastBreathPeriodS = 2.f;

	TestEqual(TEXT("calm period"), DeepFxRules::BreathPeriodS(false, *S), 4.f);
	TestEqual(TEXT("fast period"), DeepFxRules::BreathPeriodS(true, *S), 2.f);

	// Breathing in (0 to 0.45) asks for nothing; the breath out asks for each puff once.
	TestEqual(TEXT("breath in: no bubbles"), DeepFxRules::PuffsCrossed(0.0f, 0.44f, S->ExhalePuffPhases), 0);
	TestEqual(TEXT("first puff of the breath out"), DeepFxRules::PuffsCrossed(0.44f, 0.48f, S->ExhalePuffPhases), 1);
	TestEqual(TEXT("the same puff is not asked twice"), DeepFxRules::PuffsCrossed(0.48f, 0.50f, S->ExhalePuffPhases), 0);
	TestEqual(TEXT("a long frame over the whole breath out"), DeepFxRules::PuffsCrossed(0.40f, 0.95f, S->ExhalePuffPhases), 4);
	TestEqual(TEXT("wrap into the next cycle"), DeepFxRules::PuffsCrossed(0.95f, 0.10f, S->ExhalePuffPhases), 0);
	TestEqual(TEXT("wrap past a puff"), DeepFxRules::PuffsCrossed(0.70f, 0.05f, S->ExhalePuffPhases), 1);

	// Over 12 s of calm breathing at 60 fps: 3 breaths out, so 3 first puffs and 12 bursts in all.
	auto Run = [&](float PeriodS, float Seconds, int32& OutFirsts)
	{
		float Prev = 0.f;
		int32 Bursts = 0;
		OutFirsts = 0;
		for (float T = 0.f; T < Seconds; T += 1.f / 60.f)
		{
			const float Phase = DeepFxRules::BreathPhase(T, PeriodS);
			for (const int32 I : DeepFxRules::PuffIndicesCrossed(Prev, Phase, S->ExhalePuffPhases))
			{
				++Bursts;
				OutFirsts += I == 0 ? 1 : 0;
			}
			Prev = Phase;
		}
		return Bursts;
	};
	int32 Firsts = 0;
	TestEqual(TEXT("calm: 4 bursts per breath, 3 breaths in 12 s"), Run(DeepFxRules::BreathPeriodS(false, *S), 12.f, Firsts), 12);
	TestEqual(TEXT("calm: one breath out per 4 s"), Firsts, 3);
	TestEqual(TEXT("fast: twice the breaths out in the same time"), Run(DeepFxRules::BreathPeriodS(true, *S), 12.f, Firsts), 24);
	TestEqual(TEXT("fast: one breath out per 2 s"), Firsts, 6);

	S->FirstPuffCount = 9;
	S->PuffFalloff = 2;
	S->MinPuffCount = 3;
	TestEqual(TEXT("first puff is the biggest"), DeepFxRules::PuffCount(0, *S), 9);
	TestEqual(TEXT("then trails off"), DeepFxRules::PuffCount(2, *S), 5);
	TestEqual(TEXT("never below the minimum"), DeepFxRules::PuffCount(10, *S), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepFxSiltTest, "Deep.Sim.Rules.Fx.SiltTouch", DEEP_FX_TEST_FLAGS)
bool FDeepFxSiltTest::RunTest(const FString&)
{
	UDeepFxSettings* S = NewObject<UDeepFxSettings>();
	S->TouchHeightCm = 35.f;
	S->TouchMinSpeedCmS = 60.f;
	S->TouchCooldownS = 1.2f;
	TestTrue(TEXT("skimming the bed kicks silt"), DeepFxRules::KicksSilt(10.f, 100.f, 5.f, *S));
	TestFalse(TEXT("hovering still does not"), DeepFxRules::KicksSilt(10.f, 20.f, 5.f, *S));
	TestFalse(TEXT("well above the bed does not"), DeepFxRules::KicksSilt(120.f, 200.f, 5.f, *S));
	TestFalse(TEXT("not again within the cooldown"), DeepFxRules::KicksSilt(10.f, 100.f, 0.5f, *S));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepFxSpillTest, "Deep.Sim.Rules.Fx.Spill", DEEP_FX_TEST_FLAGS)
bool FDeepFxSpillTest::RunTest(const FString&)
{
	UDeepFxSettings* S = NewObject<UDeepFxSettings>();
	S->LampSpill = 1.f;
	S->HatchSpill = 0.6f;
	S->HatchSpillRangeM = 10.f;
	S->AmbientSpill = 0.3f;
	S->AmbientFullDepthM = 120.f;
	S->AmbientZeroDepthM = 250.f;
	TestEqual(TEXT("lamp on in black water: full spill"), DeepFxRules::Spill(true, 50.f, 300.f, *S), 1.f);
	TestEqual(TEXT("lamp off in black water, far from the hatch: nothing"), DeepFxRules::Spill(false, 50.f, 300.f, *S), 0.f);
	TestEqual(TEXT("lamp off at 100 m: the blue water's floor"), DeepFxRules::Spill(false, 50.f, 100.f, *S), 0.3f);
	TestEqual(TEXT("lamp off right under the hatch"), DeepFxRules::Spill(false, 0.f, 300.f, *S), 0.6f);
	TestEqual(TEXT("halfway out from the hatch"), DeepFxRules::Spill(false, 5.f, 300.f, *S), 0.3f);
	return true;
}
