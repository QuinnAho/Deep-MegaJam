// The diver's sonar ping (V in the water, D-041): the cooldown and in-water rule (DeepCameraRules::CanDiverPing), the
// pulse's far scaling that keeps a cavern wall at 60-95 m readable (band width and dot grid in pixels), and in a world:
// refused aboard, sent in the water as sonar_ping from the diver, the camera's pulse leaves from the diver over
// DiverPingRangeM, a second ping inside the cooldown is refused, and the helm's ping does not cut the diver's short.

#include "CoreMinimal.h"
#include "Camera/DeepCameraRules.h"
#include "Camera/DeepCameraSubsystem.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Sonar/DeepSonarRules.h"
#include "Story/DeepEventBus.h"
#include "World/DeepArena.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCameraDiverPingRuleTest, "Deep.Camera.DiverPing.Rule", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeepCameraDiverPingRuleTest::RunTest(const FString& Parameters)
{
	FDeepCameraTuning T;
	T.DiverPingCooldownS = 3.f;
	TestTrue(TEXT("The first ping in the water goes"), DeepCameraRules::CanDiverPing(0.f, -1.f, true, T));
	TestFalse(TEXT("Never aboard"), DeepCameraRules::CanDiverPing(100.f, -1.f, false, T));
	TestFalse(TEXT("Not 1 s after the last"), DeepCameraRules::CanDiverPing(11.f, 10.f, true, T));
	TestTrue(TEXT("Again once the cooldown is over"), DeepCameraRules::CanDiverPing(13.f, 10.f, true, T));
	TestTrue(TEXT("Wait counts down"), FMath::IsNearlyEqual(DeepCameraRules::DiverPingWaitS(11.f, 10.f, T), 2.f, 1e-4f));
	TestEqual(TEXT("No wait before the first"), DeepCameraRules::DiverPingWaitS(0.f, -1.f, T), 0.f);

	// The shipped numbers: about 3 s, and the range passes the far cavern walls (stops 2 and 3 are 60-94 m from the hatch).
	const FDeepCameraTuning D;
	TestTrue(TEXT("Cooldown about 3 s"), D.DiverPingCooldownS >= 2.f && D.DiverPingCooldownS <= 4.f);
	TestTrue(TEXT("Range reaches a wall at 94 m"), D.DiverPingRangeM >= 94.f);
	TestTrue(TEXT("Dots still drawn at 94 m"), D.PulseDotFarM * 0.8f >= 94.f);

	// On screen at 90 m (1707 px wide, 90 degree field: 2 * 9000 / 1707 = 10.5 cm a pixel): the band's trail is
	// several pixels wide and the dots stay apart; near the diver the grid keeps its fine spacing.
	const float CmPerPx = 2.f * 9000.f / 1707.f;
	const float BandPx = DeepCameraRules::PulseBandCm(9000.f, D) / CmPerPx;
	const float DotPx = DeepCameraRules::PulseDotSpacingCm(9000.f, D) / CmPerPx;
	AddInfo(FString::Printf(TEXT("At 90 m: band %.1f px, dot spacing %.1f px"), BandPx, DotPx));
	TestTrue(TEXT("Band at least 10 px wide at 90 m"), BandPx >= 10.f);
	TestTrue(TEXT("Dots at least 8 px apart at 90 m"), DotPx >= 8.f);
	TestEqual(TEXT("Near grid unchanged"), DeepCameraRules::PulseDotSpacingCm(500.f, D), D.PulseDotSpacingM * UDeepRules::CmPerM);
	TestEqual(TEXT("Near band unchanged"), DeepCameraRules::PulseBandCm(500.f, D), D.PulseBandM * UDeepRules::CmPerM);
	TestTrue(TEXT("Each doubling past the near distance doubles the grid"),
		FMath::IsNearlyEqual(DeepCameraRules::PulseDotSpacingCm(D.PulseDotNearM * 400.f, D), 4.f * D.PulseDotSpacingM * UDeepRules::CmPerM, 1e-2f));
	return true;
}

namespace DeepDiverPingTest
{
	struct FCVarScope
	{
		IConsoleVariable* Var = nullptr;
		int32 Before = -1;

		FCVarScope(const TCHAR* Name, int32 Value)
		{
			Var = IConsoleManager::Get().FindConsoleVariable(Name);
			if (Var)
			{
				Before = Var->GetInt();
				Var->Set(Value, ECVF_SetByCode);
			}
		}

		~FCVarScope()
		{
			if (Var)
			{
				Var->Set(Before, ECVF_SetByCode);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCameraDiverPingWorldTest, "Deep.Camera.DiverPing.World", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepCameraDiverPingWorldTest::RunTest(const FString& Parameters)
{
	using namespace DeepDiverPingTest;
	FCVarScope Pulse(TEXT("Deep.Sonar.Pulse"), 1);
	const FDeepCameraTuning& T = UDeepTuningSettings::Get().Camera;

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	ADeepGameMode* GameMode = World->SpawnActor<ADeepGameMode>();
	ADeepSubmarine* Sub = GameMode ? GameMode->EnsureArena() : nullptr;
	if (GameMode)
	{
		GameMode->StartPlay();
	}
	ADeepGameState* State = World->GetGameState<ADeepGameState>();
	UDeepEventBus* Bus = UDeepEventBus::Get(World);
	ADeepDiver* Diver = Sub ? World->SpawnActor<ADeepDiver>(Sub->GetRoomStartTransform().GetLocation(), FRotator::ZeroRotator) : nullptr;
	UDeepRunComponent* Run = Diver ? Diver->GetRun() : nullptr;
	UDeepCameraSubsystem* Camera = UDeepCameraSubsystem::Get(World);
	ON_SCOPE_EXIT
	{
		if (World->HasBegunPlay())
		{
			World->EndPlay(EEndPlayReason::Quit);
		}
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus, camera"), GameMode && State && Sub && Diver && Run && Bus && Camera);
	Run->StepRun(0.f);

	const FName PingEvent(DeepSonarEvent::Ping);
	AActor* PingFrom = nullptr;
	float PingRangeM = 0.f;
	const FDelegateHandle Listen = Bus->OnEvent.AddLambda([&](FName Event, const FDeepEventPayload& Payload)
	{
		if (Event == PingEvent)
		{
			PingFrom = Payload.Source.Get();
			PingRangeM = Payload.DepthM;
		}
	});

	// Aboard: no ping.
	TestFalse(TEXT("Aboard: not in the water"), Diver->IsInWater());
	TestFalse(TEXT("Aboard: the ping is refused"), Diver->Ping());
	TestEqual(TEXT("Aboard: no sonar_ping"), Bus->CountOf(PingEvent), 0);

	// Through the hatch: the ping goes out from the diver, with no flashlight.
	State->bOnTitle = false;
	State->bParked = true;
	UTEST_TRUE(TEXT("Drops through the hatch"), Run->LeaveSub());
	TestTrue(TEXT("In the water"), Diver->IsInWater());
	TestFalse(TEXT("No flashlight bought"), Diver->HasFlashlight());
	const int32 PulsesBefore = Camera->GetPulses();
	TestTrue(TEXT("In the water: the ping goes"), Diver->Ping());
	TestEqual(TEXT("One sonar_ping"), Bus->CountOf(PingEvent), 1);
	TestTrue(TEXT("From the diver"), PingFrom == Diver);
	TestEqual(TEXT("Over the diver's range"), PingRangeM, T.DiverPingRangeM);
	TestEqual(TEXT("The camera drew a pulse"), Camera->GetPulses(), PulsesBefore + 1);
	TestTrue(TEXT("The pulse leaves from the diver"), Camera->GetPulseOrigin().Equals(Diver->GetActorLocation(), 1.0));
	TestEqual(TEXT("The pulse reaches the diver's range"), Camera->GetPulseRangeM(), T.DiverPingRangeM);
	TestTrue(TEXT("Marked as the diver's"), Camera->IsPulseFromDiver());

	// Inside the cooldown: refused.
	TestTrue(TEXT("A wait is left"), Diver->GetPingWaitS() > 0.f);
	TestFalse(TEXT("A second ping at once is refused"), Diver->Ping());
	TestEqual(TEXT("Still one sonar_ping"), Bus->CountOf(PingEvent), 1);

	// The helm's sweep does not cut the diver's pulse short.
	FDeepEventPayload Helm;
	Helm.Source = Sub;
	Helm.DepthM = 60.f;
	Bus->Broadcast(PingEvent, Helm);
	TestEqual(TEXT("The helm's ping waits for the diver's pulse"), Camera->GetPulses(), PulsesBefore + 1);
	TestEqual(TEXT("Range still the diver's"), Camera->GetPulseRangeM(), T.DiverPingRangeM);

	Bus->OnEvent.Remove(Listen);
	return true;
}

#endif
