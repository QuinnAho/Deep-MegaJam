// Lane G (Nov 15): the serpent hears more, answers, and taunts before it bites. Pure rule tests.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Monster/DeepMonsterRules.h"
#include "Monster/DeepMonsterTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_TAUNT_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepTauntTest
{
	FDeepMonsterInputs Stalking(float TetherM, int32 TauntsLeft)
	{
		FDeepMonsterInputs In;
		In.State = EDeepMonsterState::Stalk;
		In.Noise = 90.f;
		In.bHasDiver = true;
		In.bDiverTethered = true;
		In.TetherDistanceM = TetherM;
		In.DiverDistanceM = TetherM;
		In.TauntsLeft = TauntsLeft;
		return In;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMonsterTauntBeatsTest, "Deep.Sim.Monster.Taunt", DEEP_TAUNT_TEST_FLAGS)
bool FDeepMonsterTauntBeatsTest::RunTest(const FString& Parameters)
{
	const FDeepMonsterTuning T;
	using namespace DeepTauntTest;

	TestTrue(TEXT("Taunts on by default (2+)"), T.TauntCount >= 2);
	TestTrue(TEXT("Taunt starts outside bite reach"), T.TauntStartM > T.BiteReachM);

	// Stalk with taunts left, inside the taunt range: it taunts instead of biting, even within bite reach.
	FDeepMonsterStep S = UDeepMonsterRules::NextState(Stalking(T.TauntStartM - 1.f, 2), T);
	TestEqual(TEXT("Stalk -> Taunt"), S.Next, EDeepMonsterState::Taunt);
	TestTrue(TEXT("Taunt started flag"), S.bTauntStarted);
	S = UDeepMonsterRules::NextState(Stalking(T.BiteReachM * 0.5f, 1), T);
	TestEqual(TEXT("Taunts before biting even in reach"), S.Next, EDeepMonsterState::Taunt);
	TestFalse(TEXT("No bite while taunts are left"), S.bBiteStarted);

	// No taunts left: the old rule, it bites.
	S = UDeepMonsterRules::NextState(Stalking(T.BiteReachM * 0.5f, 0), T);
	TestEqual(TEXT("No taunts left -> Bite"), S.Next, EDeepMonsterState::Bite);

	// Further than the taunt range: keeps stalking.
	S = UDeepMonsterRules::NextState(Stalking(T.TauntStartM + 5.f, 2), T);
	TestEqual(TEXT("Far: stays in Stalk"), S.Next, EDeepMonsterState::Stalk);

	// In the taunt: never bites, even in reach; completes back to Stalk; times out; the lamp drives it off.
	FDeepMonsterInputs In = Stalking(T.BiteReachM * 0.5f, 1);
	In.State = EDeepMonsterState::Taunt;
	In.StateSeconds = 1.f;
	S = UDeepMonsterRules::NextState(In, T);
	TestEqual(TEXT("Taunt holds in bite reach"), S.Next, EDeepMonsterState::Taunt);
	TestFalse(TEXT("Taunt never bites"), S.bBiteStarted);
	TestFalse(TEXT("Taunt never severs"), S.bSevered);

	In.bTauntComplete = true;
	S = UDeepMonsterRules::NextState(In, T);
	TestEqual(TEXT("Taunt complete -> Stalk"), S.Next, EDeepMonsterState::Stalk);

	In.bTauntComplete = false;
	In.StateSeconds = T.TauntMaxS + 0.1f;
	S = UDeepMonsterRules::NextState(In, T);
	TestEqual(TEXT("Taunt timeout -> Stalk"), S.Next, EDeepMonsterState::Stalk);

	In.StateSeconds = 1.f;
	In.LitSeconds = T.TauntLitToRecoilS + 0.1f;
	S = UDeepMonsterRules::NextState(In, T);
	TestEqual(TEXT("Lit long enough -> Recoil"), S.Next, EDeepMonsterState::Recoil);
	TestTrue(TEXT("Repelled"), S.bRepelled);

	In.LitSeconds = 0.f;
	In.bDiverTethered = false;
	S = UDeepMonsterRules::NextState(In, T);
	TestEqual(TEXT("Untethered -> Hunt"), S.Next, EDeepMonsterState::Hunt);

	TestEqual(TEXT("State name"), UDeepMonsterRules::StateName(EDeepMonsterState::Taunt), FString(TEXT("Taunt")));

	// The path: edge in front, brush outside bite reach of the bite point, withdraw far out.
	const FVector Diver(0.f, 0.f, -5000.f);
	const FVector Bite = Diver + FVector(0.f, 0.f, 300.f);
	for (const float Side : { 1.f, -1.f })
	{
		TArray<FVector> P;
		UDeepMonsterRules::TauntPath(Diver, Bite, FVector(1.f, 0.f, 0.f), Side, T, P);
		if (!TestEqual(TEXT("Three beats"), P.Num(), 3))
		{
			return false;
		}
		TestTrue(TEXT("Edge is ahead of the view"), (P[0] - Diver).X > 0.f);
		TestNearlyEqual(TEXT("Edge at TauntEdgeM"), static_cast<float>(FVector::Dist(P[0], Diver) / 100.0), T.TauntEdgeM, 0.05f);
		TestTrue(TEXT("Brush outside bite reach"), FVector::Dist(P[1], Bite) / 100.f >= T.BiteReachM + 0.99f);
		TestTrue(TEXT("Withdraw is into the dark"), FVector::Dist(P[2], Diver) / 100.f >= T.TauntWithdrawM - 0.05f);
		TestTrue(TEXT("Withdraw is behind the view"), (P[2] - Diver).X < 0.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMonsterHearingTest, "Deep.Sim.Monster.Hearing", DEEP_TAUNT_TEST_FLAGS)
bool FDeepMonsterHearingTest::RunTest(const FString& Parameters)
{
	const FDeepMonsterTuning T;
	TestTrue(TEXT("Hearing range is wide (>= 250 m)"), T.HearingRangeM >= 250.f);

	TestEqual(TEXT("Ping"), UDeepMonsterRules::NoiseForEvent(TEXT("sonar_ping"), 0.f, T), T.PingNoise);
	TestEqual(TEXT("Harpoon"), UDeepMonsterRules::NoiseForEvent(TEXT("harpoon_fired"), 0.f, T), T.HarpoonNoise);
	TestTrue(TEXT("Harder knock is louder"), UDeepMonsterRules::NoiseForEvent(TEXT("hull_hit"), 4.f, T) > UDeepMonsterRules::NoiseForEvent(TEXT("hull_hit"), 1.f, T));
	TestEqual(TEXT("Unknown event is silent"), UDeepMonsterRules::NoiseForEvent(TEXT("chime"), 0.f, T), 0.f);
	TestTrue(TEXT("A ping is loud enough to answer"), T.PingNoise >= T.AnswerNoise);

	TestEqual(TEXT("Parked sub is silent"), UDeepMonsterRules::HelmNoise(0.f, 8.f, 1.f, T), 0.f);
	TestNearlyEqual(TEXT("Full throttle"), UDeepMonsterRules::HelmNoise(8.f, 8.f, 1.f, T), T.HelmNoisePerS, 0.001f);
	TestNearlyEqual(TEXT("Half throttle (either way)"), UDeepMonsterRules::HelmNoise(-4.f, 8.f, 1.f, T), T.HelmNoisePerS * 0.5f, 0.001f);

	TestTrue(TEXT("Answers a loud sound"), UDeepMonsterRules::ShouldAnswer(T.AnswerNoise, T.AnswerCooldownS, T));
	TestFalse(TEXT("Not a quiet one"), UDeepMonsterRules::ShouldAnswer(T.AnswerNoise * 0.5f, 100.f, T));
	TestFalse(TEXT("Not right after a call"), UDeepMonsterRules::ShouldAnswer(100.f, T.AnswerCooldownS * 0.5f, T));

	// Voice: full inside, falls with distance, silent past the falloff.
	TestEqual(TEXT("Full inside"), UDeepMonsterRules::VoiceGain(1.f, 4.f, 60.f), 1.f);
	float Last = 2.f;
	for (float D = 0.f; D <= 80.f; D += 5.f)
	{
		const float G = UDeepMonsterRules::VoiceGain(D, 4.f, 60.f);
		TestTrue(TEXT("Never louder further away"), G <= Last + KINDA_SMALL_NUMBER);
		Last = G;
	}
	TestEqual(TEXT("Silent past falloff"), UDeepMonsterRules::VoiceGain(70.f, 4.f, 60.f), 0.f);
	TestTrue(TEXT("Calls carry further than clicks"), T.CallFalloffM > T.ClickFalloffM);

	// Clicks quicken as it closes.
	const float Calm = UDeepMonsterRules::ClickEveryS(EDeepMonsterState::Lurk, 0.f, T);
	const float Near = UDeepMonsterRules::ClickEveryS(EDeepMonsterState::Taunt, 0.5f, T);
	TestNearlyEqual(TEXT("Calm rate"), Calm, T.ClickEveryCalmS, 0.01f);
	TestTrue(TEXT("Faster when close"), Near < Calm);
	return true;
}

#endif
