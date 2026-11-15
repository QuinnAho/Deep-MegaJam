// Permanent harness canary: gives `tools\ue.ps1 test` at least one test to run.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHarnessAliveTest, "Deep.Harness.Alive",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepHarnessAliveTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Harness is alive"), true);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
