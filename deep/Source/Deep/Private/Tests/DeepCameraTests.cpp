// The camera's pure rules: which events shake and how hard, the eased envelope every shake follows, the footage
// grain by depth, the sonar pulse shell and the interference strength.

#include "Misc/AutomationTest.h"
#include "Camera/DeepCameraRules.h"
#include "Sim/DeepTuningTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCameraShakeMapTest, "Deep.Camera.ShakeMap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeepCameraShakeMapTest::RunTest(const FString& Parameters)
{
	const FDeepCameraTuning T;
	const TCHAR* Shaking[] = { TEXT("hull_hit"), TEXT("hull_critical"), TEXT("hull_breach"), TEXT("bite_alarm"), TEXT("cut_bite"),
		TEXT("death_monster"), TEXT("harpoon_fired"), TEXT("harpoon_hit_sub"), TEXT("enter_sub"), TEXT("descend_stop2"),
		TEXT("descend_stop3"), TEXT("monster_near") };
	for (const TCHAR* Event : Shaking)
	{
		const FDeepShakeSpec S = DeepCameraRules::ShakeForEvent(FName(Event), T);
		TestFalse(FString::Printf(TEXT("%s shakes"), Event), S.IsNone());
		TestTrue(FString::Printf(TEXT("%s ends"), Event), S.DurationS > 0.f);
		TestTrue(FString::Printf(TEXT("%s eases in"), Event), S.BlendInS > 0.f);
		TestTrue(FString::Printf(TEXT("%s eases out"), Event), S.BlendOutS > 0.f);
		TestTrue(FString::Printf(TEXT("%s blends fit its duration"), Event), S.BlendInS + S.BlendOutS <= S.DurationS + KINDA_SMALL_NUMBER);
	}
	TestTrue(TEXT("Unknown events do not shake"), DeepCameraRules::ShakeForEvent(FName(TEXT("first_salvage")), T).IsNone());
	TestTrue(TEXT("None does not shake"), DeepCameraRules::ShakeForEvent(NAME_None, T).IsNone());

	auto Spec = [&T](const TCHAR* E) { return DeepCameraRules::ShakeForEvent(FName(E), T); };
	auto Rot = [&Spec](const TCHAR* E) { return Spec(E).RotationDeg; };
	TestTrue(TEXT("The harpoon kick is small next to a hull hit"), Rot(TEXT("harpoon_fired")) < Rot(TEXT("hull_hit")));
	TestTrue(TEXT("Hull damage escalates: hit < critical < breach"), Rot(TEXT("hull_hit")) < Rot(TEXT("hull_critical")) && Rot(TEXT("hull_critical")) < Rot(TEXT("hull_breach")));
	TestTrue(TEXT("The bite is harder than its alarm"), Rot(TEXT("cut_bite")) > Rot(TEXT("bite_alarm")));
	TestTrue(TEXT("Death by the serpent is the hardest"), Rot(TEXT("death_monster")) >= Rot(TEXT("cut_bite")));
	TestTrue(TEXT("monster_near is faint"), Rot(TEXT("monster_near")) < Rot(TEXT("harpoon_fired")));
	TestTrue(TEXT("descend_stop2 is a long low rumble"), Spec(TEXT("descend_stop2")).DurationS >= 2.f && Spec(TEXT("descend_stop2")).FrequencyHz <= 4.f);
	TestTrue(TEXT("descend_stop3 is a long low rumble"), Spec(TEXT("descend_stop3")).DurationS >= 2.f && Spec(TEXT("descend_stop3")).FrequencyHz <= 4.f);

	FDeepCameraTuning Half;
	Half.ShakeScale = 0.5f;
	TestTrue(TEXT("ShakeScale scales every shake"), FMath::IsNearlyEqual(DeepCameraRules::ShakeForEvent(FName(TEXT("cut_bite")), Half).RotationDeg, Rot(TEXT("cut_bite")) * 0.5f));
	FDeepCameraTuning Off;
	Off.ShakeScale = 0.f;
	TestTrue(TEXT("ShakeScale 0 = no shake"), DeepCameraRules::ShakeForEvent(FName(TEXT("cut_bite")), Off).IsNone());

	FDeepCameraTuning Custom;
	Custom.EventShakes.Reset();
	FDeepEventShake Row;
	Row.Event = FName(TEXT("sonar_ping"));
	Row.Shake.RotationDeg = 0.2f;
	Custom.EventShakes.Add(Row);
	TestFalse(TEXT("A new event shakes by data alone"), DeepCameraRules::ShakeForEvent(FName(TEXT("sonar_ping")), Custom).IsNone());
	TestTrue(TEXT("The sway never ends by itself and eases in"), T.Sway.DurationS <= 0.f && T.Sway.BlendInS > 0.f);
	TestTrue(TEXT("The sway is under a degree"), T.Sway.RotationDeg > 0.f && T.Sway.RotationDeg < 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCameraEnvelopeTest, "Deep.Camera.Envelope", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeepCameraEnvelopeTest::RunTest(const FString& Parameters)
{
	FDeepShakeSpec S;
	S.RotationDeg = 1.f;
	S.DurationS = 1.f;
	S.BlendInS = 0.2f;
	S.BlendOutS = 0.4f;
	TestEqual(TEXT("Starts at 0"), DeepCameraRules::Envelope(0.f, S), 0.f);
	TestTrue(TEXT("Eases in (smoothstep: half way = 0.5)"), FMath::IsNearlyEqual(DeepCameraRules::Envelope(0.1f, S), 0.5f, 1e-4f));
	TestTrue(TEXT("Slow at the start of the ease"), DeepCameraRules::Envelope(0.02f, S) < 0.05f);
	TestEqual(TEXT("Full between the blends"), DeepCameraRules::Envelope(0.4f, S), 1.f);
	TestTrue(TEXT("Eases out"), FMath::IsNearlyEqual(DeepCameraRules::Envelope(0.8f, S), 0.5f, 1e-4f));
	TestEqual(TEXT("0 at the end"), DeepCameraRules::Envelope(1.f, S), 0.f);
	TestFalse(TEXT("Not done before the end"), DeepCameraRules::IsShakeDone(0.99f, S));
	TestTrue(TEXT("Done at the end"), DeepCameraRules::IsShakeDone(1.f, S));

	FDeepShakeSpec Endless = S;
	Endless.DurationS = 0.f;
	TestEqual(TEXT("Endless holds"), DeepCameraRules::Envelope(100.f, Endless), 1.f);
	TestFalse(TEXT("Endless is never done unstopped"), DeepCameraRules::IsShakeDone(1000.f, Endless));
	TestTrue(TEXT("Stopped eases out from the stop"), FMath::IsNearlyEqual(DeepCameraRules::Envelope(10.2f, Endless, 10.f), 0.5f, 1e-4f));
	TestTrue(TEXT("Stopped is done after its blend-out"), DeepCameraRules::IsShakeDone(10.4f, Endless, 10.f));
	TestEqual(TEXT("Ease clamps low"), DeepCameraRules::Ease01(-1.f), 0.f);
	TestEqual(TEXT("Ease clamps high"), DeepCameraRules::Ease01(2.f), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCameraLookTest, "Deep.Camera.Look", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeepCameraLookTest::RunTest(const FString& Parameters)
{
	const FDeepCameraTuning T;
	TestTrue(TEXT("Switches: shakes on; footage, pulse, interference off until Quinn approves"),
		T.bShakeOn && !T.bFootageOn && !T.bPulseOn && !T.bInterferenceOn);
	const FVector2D G100 = DeepCameraRules::FootageGrain(100.f, T);
	const FVector2D G300 = DeepCameraRules::FootageGrain(300.f, T);
	const FVector2D G500 = DeepCameraRules::FootageGrain(500.f, T);
	TestTrue(TEXT("Grain rises with depth"), G100.X < G300.X && G300.X < G500.X);
	TestTrue(TEXT("Shadow grain rises with depth"), G100.Y < G500.Y);
	TestEqual(TEXT("Grain clamps above"), DeepCameraRules::FootageGrain(0.f, T).X, G100.X);
	TestEqual(TEXT("Grain clamps below"), DeepCameraRules::FootageGrain(900.f, T).X, G500.X);

	// Pulse: the shell reaches the range at PulseSeconds and holds; drawn until the dots fade.
	TestEqual(TEXT("Shell starts at the sub"), DeepCameraRules::PulseRadiusCm(0.f, 80.f, T), 0.f);
	TestTrue(TEXT("Half way at half time"), FMath::IsNearlyEqual(DeepCameraRules::PulseRadiusCm(T.PulseSeconds * 0.5f, 80.f, T), 4000.f, 1.f));
	TestTrue(TEXT("Holds at range"), FMath::IsNearlyEqual(DeepCameraRules::PulseRadiusCm(T.PulseSeconds * 3.f, 80.f, T), 8000.f, 1.f));
	TestTrue(TEXT("Live while growing"), DeepCameraRules::IsPulseLive(0.5f, T));
	TestTrue(TEXT("Live while the dots fade"), DeepCameraRules::IsPulseLive(T.PulseSeconds + T.PulseDotFadeS * 0.5f, T));
	TestFalse(TEXT("Gone after"), DeepCameraRules::IsPulseLive(T.PulseSeconds + T.PulseDotFadeS + 0.01f, T));
	TestTrue(TEXT("About 1.5 s and 2 s (brief)"), FMath::IsNearlyEqual(T.PulseSeconds, 1.5f) && FMath::IsNearlyEqual(T.PulseDotFadeS, 2.f));
	TestTrue(TEXT("The pulse skips the sub's own room and reaches past it"), T.PulseStartM > 0.f && T.PulseStartM < T.PulseRangeM && T.PulseBandM > 0.f);
	TestTrue(TEXT("Lidar dots fade with distance, inside the range"), T.PulseDotFarM > 0.f && T.PulseDotFarM <= T.PulseRangeM);

	// Interference: static before shape.
	TestEqual(TEXT("None far away"), DeepCameraRules::InterferenceTarget(100.f, false, 0.f, T), 0.f);
	TestEqual(TEXT("None with no serpent"), DeepCameraRules::InterferenceTarget(-1.f, false, 0.f, T), 0.f);
	const float Mid = DeepCameraRules::InterferenceTarget((T.InterferenceFarNearM.X + T.InterferenceFarNearM.Y) * 0.5f, false, 0.f, T);
	TestTrue(TEXT("Rises as it closes"), Mid > 0.f && Mid < T.InterferenceNearMax);
	TestTrue(TEXT("NearMax at the near distance"), FMath::IsNearlyEqual(DeepCameraRules::InterferenceTarget(T.InterferenceFarNearM.Y, false, 0.f, T), T.InterferenceNearMax));
	TestEqual(TEXT("Full in a bite"), DeepCameraRules::InterferenceTarget(100.f, true, 0.f, T), 1.f);
	TestEqual(TEXT("A burst on a hit"), DeepCameraRules::InterferenceTarget(100.f, false, 0.2f, T), T.InterferenceBurst);
	return true;
}

#endif
