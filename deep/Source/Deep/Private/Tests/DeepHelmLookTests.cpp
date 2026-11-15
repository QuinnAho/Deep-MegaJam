// The helm's look while driving: sway and streaks scale with speed and stop when parked; the trench wall has a ledge
// at every LedgeEveryM so the descent can be counted.

#include "Misc/AutomationTest.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHelmLookTest, "Deep.World.HelmLook", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeepHelmLookTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("No sway parked"), ADeepSubmarine::HelmSwayScale(0.f, 8.f), 0.f);
	TestEqual(TEXT("Full sway at drive speed"), ADeepSubmarine::HelmSwayScale(8.f, 8.f), 1.f);
	TestEqual(TEXT("Half sway at half speed, either direction"), ADeepSubmarine::HelmSwayScale(-4.f, 8.f), 0.5f);
	TestEqual(TEXT("Sway clamps"), ADeepSubmarine::HelmSwayScale(30.f, 8.f), 1.f);
	TestEqual(TEXT("No streak parked"), ADeepSubmarine::HelmStreakM(0.f, 0.025f), 0.f);
	TestTrue(TEXT("20 cm streak at 8 m/s"), FMath::IsNearlyEqual(ADeepSubmarine::HelmStreakM(8.f, 0.025f), 0.2f, 1e-4f));

	// A ledge at every level: the wall stands out further just under a level than halfway between two.
	int32 Ledges = 0;
	for (float Level = 110.f; Level <= 300.f; Level += 10.f)
	{
		float Under = 0.f;
		float Between = 0.f;
		for (float A = -10.f; A <= 10.f; A += 1.f)
		{
			Under += ADeepSubmarine::TrenchReliefM(A, Level + 0.1f, 10.f, 1.6f) - ADeepSubmarine::TrenchReliefM(A, Level + 0.1f, 10.f, 0.f);
			Between += ADeepSubmarine::TrenchReliefM(A, Level + 5.f, 10.f, 1.6f) - ADeepSubmarine::TrenchReliefM(A, Level + 5.f, 10.f, 0.f);
		}
		Ledges += Under > Between + 5.f ? 1 : 0;
	}
	TestEqual(TEXT("A ledge every 10 m from 110 to 300 m"), Ledges, 20);
	return true;
}

#endif
