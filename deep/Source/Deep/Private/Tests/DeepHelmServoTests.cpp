// D2 (Nov 25): the pilot's look on a servo (Sim/DeepHelmViewRules StepServo), the judder measure (Sim/DeepJudderRules),
// and the sonar's ping on demand (UDeepSonarRules, ADeepDiver::HelmPing: the server decides, every bus hears it).
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "Misc/AutomationTest.h"
#include "Sim/DeepHelmViewRules.h"
#include "Sim/DeepJudderRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Sonar/DeepSonarRules.h"
#include "Story/DeepEventBus.h"
#include "World/DeepArena.h"
#include "World/DeepSubmarine.h"
#include "World/DeepSubRoom.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_SERVO_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepHelmServoTest
{
	/** Runs the servo toward an aim at 60 fps for Seconds; returns the samples of the yaw axis (angle, rate) per frame. */
	TArray<FVector2D> Run(FDeepServo& Servo, const FVector2D& Aim, float Seconds, const FDeepHelmViewTuning& T)
	{
		TArray<FVector2D> Out;
		for (float Time = 0.f; Time < Seconds; Time += 1.f / 60.f)
		{
			Servo = DeepHelmView::StepServo(Servo, Aim, 1.f / 60.f, T);
			Out.Add(FVector2D(Servo.AngleDeg.X, Servo.RateDegS.X));
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHelmServoTest, "Deep.Sim.HelmView.Servo", DEEP_SERVO_TEST_FLAGS)

bool FDeepHelmServoTest::RunTest(const FString& Parameters)
{
	using namespace DeepHelmServoTest;
	const FDeepHelmViewTuning T;
	// A 60 deg swing to port, as a quick flick of the mouse.
	FDeepServo Servo;
	const TArray<FVector2D> Swing = Run(Servo, FVector2D(-60.0, 0.0), 4.f, T);
	double Fastest = 0.0;
	double Overshoot = 0.0;
	double FirstStep = FMath::Abs(Swing[0].X);
	int32 Arrived = INDEX_NONE;
	int32 Settled = INDEX_NONE;
	for (int32 I = 0; I < Swing.Num(); ++I)
	{
		Fastest = FMath::Max(Fastest, FMath::Abs(Swing[I].Y));
		Overshoot = FMath::Max(Overshoot, -60.0 - Swing[I].X);
		if (Arrived == INDEX_NONE && Swing[I].X <= -59.0)
		{
			Arrived = I;
		}
		if (Settled == INDEX_NONE && Swing[I].X == -60.0 && Swing[I].Y == 0.0)
		{
			Settled = I;
		}
	}
	AddInfo(FString::Printf(TEXT("60 deg: fastest %.1f deg/s (cap %.0f), first frame %.2f deg, within 1 deg at %.2f s, overshoot %.2f deg, held at %.2f s"),
		Fastest, T.ServoRateDegS, FirstStep, Arrived / 60.f, Overshoot, Settled / 60.f));
	// The rate cap: never faster than ServoRateDegS.
	TestTrue(TEXT("never faster than the cap"), Fastest <= T.ServoRateDegS + 1e-3);
	TestTrue(TEXT("and it reaches the cap on a big swing"), Fastest >= T.ServoRateDegS - 0.5);
	// A motor: it eases in (the first frame moves under a degree, not a jump).
	TestTrue(TEXT("eases in: the first frame moves little"), FirstStep < 0.5);
	// The swing takes about its length over the cap.
	TestTrue(TEXT("arrives in about 60 / cap seconds"), Arrived != INDEX_NONE && Arrived / 60.f > 60.f / T.ServoRateDegS && Arrived / 60.f < 60.f / T.ServoRateDegS + 0.8f);
	// Settles with a small overshoot, then holds on the aim.
	TestTrue(TEXT("a small overshoot (0.1 to 2 deg)"), Overshoot > 0.1 && Overshoot < 2.0);
	TestTrue(TEXT("then it settles"), Settled != INDEX_NONE && Settled / 60.f < 2.6f);
	bool bHeld = Settled != INDEX_NONE;
	for (int32 I = FMath::Max(0, Settled); I < Swing.Num() && bHeld; ++I)
	{
		bHeld = Swing[I].X == -60.0 && Swing[I].Y == 0.0;
	}
	TestTrue(TEXT("and holds exactly, no creep"), bHeld);

	// The return: the aim goes back ahead and the view swings back at the same pace.
	const TArray<FVector2D> Back = Run(Servo, FVector2D::ZeroVector, 4.f, T);
	double BackFastest = 0.0;
	for (const FVector2D& S : Back)
	{
		BackFastest = FMath::Max(BackFastest, FMath::Abs(S.Y));
	}
	TestEqual(TEXT("back ahead"), Servo.AngleDeg.X, 0.0, 1e-6);
	TestEqual(TEXT("at the same pace"), BackFastest, Fastest, 0.5);

	// A small nudge moves slowly and still settles; the pitch axis is its own servo.
	FDeepServo Nudge;
	Run(Nudge, FVector2D(2.0, -10.0), 2.f, T);
	TestEqual(TEXT("a 2 deg nudge settles on the aim"), Nudge.AngleDeg.X, 2.0, 1e-6);
	TestEqual(TEXT("the pitch settles on its own aim"), Nudge.AngleDeg.Y, -10.0, 1e-6);

	// A long frame (a hitch) does not throw it: the step is sliced.
	FDeepServo Hitch;
	Hitch = DeepHelmView::StepServo(Hitch, FVector2D(-60.0, 0.0), 0.25f, T);
	TestTrue(TEXT("a 0.25 s frame still keeps the cap"), FMath::Abs(Hitch.RateDegS.X) <= T.ServoRateDegS + 1e-3 && FMath::Abs(Hitch.AngleDeg.X) <= T.ServoRateDegS * 0.25f + 1e-3);
	TestEqual(TEXT("the whine is silent at rest"), DeepHelmView::ServoSpeed01(Servo, T), 0.f);
	FDeepServo Mid;
	Mid.RateDegS = FVector2D(-T.ServoRateDegS * 0.5, 0.0);
	TestEqual(TEXT("the whine's share at half speed"), DeepHelmView::ServoSpeed01(Mid, T), 0.5f, 1e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepJudderRuleTest, "Deep.Sim.Judder", DEEP_SERVO_TEST_FLAGS)

bool FDeepJudderRuleTest::RunTest(const FString& Parameters)
{
	// A steady drive and a smooth turn at about 55 fps with uneven frames: almost nothing off the smooth path.
	TArray<FDeepJudderSample> Smooth;
	double Time = 0.0;
	for (int32 I = 0; I < 300; ++I)
	{
		FDeepJudderSample S;
		S.TimeS = Time;
		S.HullCm = FVector(400.0 * Time, 0.0, 0.0);
		S.HullRot = FRotator(0.0, 30.0 * Time, 0.0).Quaternion();
		S.CamCm = S.HullCm + FVector(350.0, 0.0, 145.0);
		S.CamRot = S.HullRot;
		Smooth.Add(S);
		Time += (I % 3 == 0) ? 0.02 : 0.017;
	}
	const FDeepJudderStats Calm = DeepJudder::Measure(Smooth);
	AddInfo(DeepJudder::ToJson(Calm, TEXT("smooth")));
	TestTrue(TEXT("a smooth drive leaves under a millimetre"), Calm.CamCm.Max < 0.1 && Calm.HullCm.Max < 0.1);
	TestTrue(TEXT("a smooth turn leaves under a hundredth of a degree"), Calm.CamDeg.Max < 0.01);
	TestEqual(TEXT("4 m/s"), Calm.HullMeanMps, 4.0, 0.01);
	// One frame the hull stalls (a step missed) and the next catches up: centimetres, and a stall counted.
	TArray<FDeepJudderSample> Stall = Smooth;
	Stall[100].HullCm = Stall[99].HullCm;
	Stall[100].CamCm = Stall[99].CamCm;
	const FDeepJudderStats Bad = DeepJudder::Measure(Stall);
	AddInfo(DeepJudder::ToJson(Bad, TEXT("stall")));
	TestTrue(TEXT("a stalled frame shows as judder"), Bad.HullCm.Max > 3.0 && Bad.CamCm.Max > 3.0);
	TestEqual(TEXT("one stall"), Bad.StallFrames, 1);
	// A camera shaking at a frequency over half the frame rate is all judder, whatever its size.
	TArray<FDeepJudderSample> Shiver = Smooth;
	for (FDeepJudderSample& S : Shiver)
	{
		S.CamRot = S.HullRot * FRotator(0.35 * FMath::Sin(UE_TWO_PI * 23.0 * S.TimeS), 0.0, 0.0).Quaternion();
	}
	const FDeepJudderStats Shaky = DeepJudder::Measure(Shiver);
	TestTrue(TEXT("a 23 Hz shiver reads as judder"), Shaky.CamDeg.Rms > 0.15);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepPingRulesTest, "Deep.UI.Sonar.PingOnDemand", DEEP_SERVO_TEST_FLAGS)

bool FDeepPingRulesTest::RunTest(const FString& Parameters)
{
	FDeepSonarTuning T;
	TestTrue(TEXT("on demand by default"), T.bPingOnDemand);
	TestFalse(TEXT("the soft style never pings on its own"), UDeepSonarRules::AutoPings(1, T));
	TestTrue(TEXT("the jam style keeps its ping a sweep"), UDeepSonarRules::AutoPings(0, T));
	T.bPingOnDemand = false;
	TestTrue(TEXT("off, the soft style pings every PingPeriodS again"), UDeepSonarRules::AutoPings(1, T));
	T.bPingOnDemand = true;
	TestTrue(TEXT("about 2 s between pings (brief)"), T.HelmPingCooldownS >= 1.5f && T.HelmPingCooldownS <= 3.f);
	TestTrue(TEXT("never pinged: now"), UDeepSonarRules::CanHelmPing(10.0, -1.0, true, T));
	TestFalse(TEXT("not at the helm: never"), UDeepSonarRules::CanHelmPing(10.0, -1.0, false, T));
	TestFalse(TEXT("just pinged: wait"), UDeepSonarRules::CanHelmPing(10.5, 10.0, true, T));
	TestEqual(TEXT("the wait left"), UDeepSonarRules::HelmPingWaitS(10.5, 10.0, T), T.HelmPingCooldownS - 0.5f, 1e-4f);
	TestTrue(TEXT("after the cooldown: again"), UDeepSonarRules::CanHelmPing(10.0 + T.HelmPingCooldownS, 10.0, true, T));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHelmPingWorldTest, "Deep.World.Sonar.HelmPing", DEEP_SERVO_TEST_FLAGS)

bool FDeepHelmPingWorldTest::RunTest(const FString& Parameters)
{
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
	ADeepDiver* Diver = Sub ? World->SpawnActor<ADeepDiver>(DeepArena::EnsureRoom(World)->GetRoomStartTransform().GetLocation(), FRotator::ZeroRotator) : nullptr;
	UDeepRunComponent* Run = Diver ? Diver->GetRun() : nullptr;
	UDeepEventBus* Bus = UDeepEventBus::Get(World);
	ON_SCOPE_EXIT
	{
		if (World->HasBegunPlay())
		{
			World->EndPlay(EEndPlayReason::Quit);
		}
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};
	UTEST_TRUE(TEXT("World, sub, diver, run, bus"), Sub && Diver && Run && Bus);
	Run->StepRun(0.f);
	const FName PingEvent(DeepSonarEvent::Ping);
	AActor* From = nullptr;
	float RangeM = 0.f;
	const FDelegateHandle Listen = Bus->OnEvent.AddLambda([&](FName Event, const FDeepEventPayload& Payload)
	{
		if (Event == PingEvent)
		{
			From = Payload.Source.Get();
			RangeM = Payload.DepthM;
		}
	});
	// Out of the seat: refused.
	TestFalse(TEXT("not at the helm: no ping"), Diver->HelmPing());
	TestEqual(TEXT("no sonar_ping"), Bus->CountOf(PingEvent), 0);
	// At the helm: one ping, from the sub, over the sonar's range.
	Run->TakeHelm();
	UTEST_TRUE(TEXT("seated"), Run->IsSeated());
	TestTrue(TEXT("at the helm the ping goes"), Diver->HelmPing());
	TestEqual(TEXT("one sonar_ping"), Bus->CountOf(PingEvent), 1);
	TestTrue(TEXT("from the sub"), From == Sub);
	TestEqual(TEXT("over the sonar's range"), RangeM, UDeepTuningSettings::Get().Sonar.RangeM);
	// Inside the cooldown: refused.
	TestTrue(TEXT("a wait is left"), Diver->GetHelmPingWaitS() > 0.f);
	TestFalse(TEXT("a second ping at once is refused"), Diver->HelmPing());
	TestEqual(TEXT("still one sonar_ping"), Bus->CountOf(PingEvent), 1);
	Bus->OnEvent.Remove(Listen);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
