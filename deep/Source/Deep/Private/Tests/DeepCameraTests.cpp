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
		TEXT("death_monster"), TEXT("harpoon_fired"), TEXT("harpoon_hit_sub"), TEXT("harpoon_hit_monster"), TEXT("enter_sub"), TEXT("descend_stop2"),
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
	TestTrue(TEXT("The harpoon kick is firm but under a critical hull"), Rot(TEXT("harpoon_fired")) >= 1.f && Rot(TEXT("harpoon_fired")) < Rot(TEXT("hull_critical")));
	TestTrue(TEXT("A bolt in the serpent thuds harder than the shot"), Rot(TEXT("harpoon_hit_monster")) > Rot(TEXT("harpoon_fired")));
	TestTrue(TEXT("The serpent thud sits between a hull hit and a critical"), Rot(TEXT("harpoon_hit_monster")) > Rot(TEXT("hull_hit")) && Rot(TEXT("harpoon_hit_monster")) < Rot(TEXT("hull_critical")));
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
	TestTrue(TEXT("Switches: shakes and the sonar pulse on (D-041); footage, interference off until Quinn approves"),
		T.bShakeOn && !T.bFootageOn && T.bPulseOn && !T.bInterferenceOn);
	const FVector2D G100 = DeepCameraRules::FootageGrain(100.f, T);
	const FVector2D G300 = DeepCameraRules::FootageGrain(300.f, T);
	const FVector2D G500 = DeepCameraRules::FootageGrain(500.f, T);
	TestTrue(TEXT("Grain rises with depth"), G100.X < G300.X && G300.X < G500.X);
	TestTrue(TEXT("Shadow grain rises with depth"), G100.Y < G500.Y);
	TestEqual(TEXT("Grain clamps above"), DeepCameraRules::FootageGrain(0.f, T).X, G100.X);
	TestEqual(TEXT("Grain clamps below"), DeepCameraRules::FootageGrain(900.f, T).X, G500.X);

	// Pulse: the shell reaches the range at the look's Seconds and holds; drawn until the dots fade.
	const FDeepPulseLook& Soft = DeepCameraRules::PulseLook(T, 1);
	const FDeepPulseLook& Jam = DeepCameraRules::PulseLook(T, 0);
	TestTrue(TEXT("Style 0 is the jam look, 1 the soft one"), &Jam == &T.PulseJam && &Soft == &T.PulseSoft);
	TestEqual(TEXT("The soft look is the default (Nov 17)"), T.SonarStyle, 1);
	TestEqual(TEXT("Shell starts at the sub"), DeepCameraRules::PulseRadiusCm(0.f, 80.f, Soft), 0.f);
	TestTrue(TEXT("Half way at half time"), FMath::IsNearlyEqual(DeepCameraRules::PulseRadiusCm(Soft.Seconds * 0.5f, 80.f, Soft), 4000.f, 1.f));
	TestTrue(TEXT("Holds at range"), FMath::IsNearlyEqual(DeepCameraRules::PulseRadiusCm(Soft.Seconds * 3.f, 80.f, Soft), 8000.f, 1.f));
	TestTrue(TEXT("Live while growing"), DeepCameraRules::IsPulseLive(0.5f, Soft));
	TestTrue(TEXT("Live while the dots fade"), DeepCameraRules::IsPulseLive(Soft.Seconds + Soft.DotFadeS * 0.5f, Soft));
	TestFalse(TEXT("Gone after"), DeepCameraRules::IsPulseLive(Soft.Seconds + Soft.DotFadeS + 0.01f, Soft));
	TestTrue(TEXT("Jam: about 1.5 s and 2 s (D-041)"), FMath::IsNearlyEqual(Jam.Seconds, 1.5f) && FMath::IsNearlyEqual(Jam.DotFadeS, 2.f));
	TestTrue(TEXT("The pulse skips the sub's own room and reaches past it"), T.PulseStartM > 0.f && T.PulseStartM < T.PulseRangeM && Soft.BandM > 0.f);
	TestTrue(TEXT("Lidar dots fade with distance, past the diver ping's far walls (D-041)"), T.PulseDotFarM > 0.f && T.PulseDotFarM >= T.DiverPingRangeM);

	// The soft look (brief, Nov 17): pale cyan #BFE3EA, a thin dim band, sparse faint dots gone within 0.8 s, so the
	// screen is clean between pings; the helm reaches 40-50 m; the serpent's outline still catches the band.
	TestTrue(TEXT("Soft: #BFE3EA"), Soft.Color.ToFColor(true) == FColor(0xBF, 0xE3, 0xEA));
	// Tuned by eye from the brief's 0.3 / 0.1 start (Nov 17 frames: at 0.3 / 0.1 the helm pulse did not show at all).
	TestTrue(TEXT("Soft: a thin band at about half the jam's strength"), Soft.BandM <= 0.4f && Soft.BandIntensity <= 0.5f * Jam.BandIntensity + 1e-4f && Soft.BandIntensity > 0.f);
	TestTrue(TEXT("Soft: dots at least 0.7 m apart, under half the jam's, fade 0.8 s at most"), Soft.DotSpacingM >= 0.7f && Soft.DotIntensity <= 0.5f * Jam.DotIntensity && Soft.DotFadeS <= 0.8f);
	TestTrue(TEXT("Soft: the helm reaches 40-50 m"), Soft.HelmRangeM >= 40.f && Soft.HelmRangeM <= 50.f);
	TestTrue(TEXT("Soft: gone well before the next ping"), Soft.Seconds + Soft.DotFadeS < 0.5f * FDeepSonarTuning().PingPeriodS);
	TestTrue(TEXT("Soft: the diver's ping a little stronger"), Soft.DiverGain > 1.f && Soft.DiverGain < 2.5f);
	TestTrue(TEXT("Soft: the band still finds the serpent"), Soft.SerpentBandGain * Soft.BandIntensity >= 0.5f);
	TestTrue(TEXT("Soft: quieter than the jam look"), Soft.BandIntensity < Jam.BandIntensity && Soft.DotIntensity < Jam.DotIntensity);
	TestEqual(TEXT("Helm range: the look's own"), DeepCameraRules::HelmPulseRangeM(Soft, 60.f, T), Soft.HelmRangeM);
	TestEqual(TEXT("Helm range: the jam look uses the sonar's"), DeepCameraRules::HelmPulseRangeM(Jam, 60.f, T), 60.f);
	TestEqual(TEXT("Helm range: else the fallback"), DeepCameraRules::HelmPulseRangeM(Jam, 0.f, T), T.PulseRangeM);
	TestEqual(TEXT("Full strength at the origin"), DeepCameraRules::PulseRangeFall(0.f, 45.f, Soft), 1.f);
	TestTrue(TEXT("Dims toward the range"), FMath::IsNearlyEqual(DeepCameraRules::PulseRangeFall(45.f, 45.f, Soft), 1.f - Soft.RangeFade, 1e-4f));
	TestEqual(TEXT("The jam look does not dim"), DeepCameraRules::PulseRangeFall(60.f, 60.f, Jam), 1.f);

	// Interference: static before shape.
	TestEqual(TEXT("None far away"), DeepCameraRules::InterferenceTarget(100.f, false, 0.f, T), 0.f);
	TestEqual(TEXT("None with no serpent"), DeepCameraRules::InterferenceTarget(-1.f, false, 0.f, T), 0.f);
	const float Mid = DeepCameraRules::InterferenceTarget((T.InterferenceFarNearM.X + T.InterferenceFarNearM.Y) * 0.5f, false, 0.f, T);
	TestTrue(TEXT("Rises as it closes"), Mid > 0.f && Mid < T.InterferenceNearMax);
	TestTrue(TEXT("NearMax at the near distance"), FMath::IsNearlyEqual(DeepCameraRules::InterferenceTarget(T.InterferenceFarNearM.Y, false, 0.f, T), T.InterferenceNearMax));
	TestEqual(TEXT("Full in a bite"), DeepCameraRules::InterferenceTarget(100.f, true, 0.f, T), 1.f);
	TestEqual(TEXT("A burst on a hit"), DeepCameraRules::InterferenceTarget(100.f, false, 0.2f, T), T.InterferenceBurst);
	const float Up = DeepCameraRules::InterferenceStep(0.f, 1.f, 0.05f, T);
	const float Down = 1.f - DeepCameraRules::InterferenceStep(1.f, 0.f, 0.05f, T);
	TestTrue(TEXT("A hit reads at once: it rises faster than it falls"), Up > Down && Down > 0.f);
	TestEqual(TEXT("Never past its target"), DeepCameraRules::InterferenceStep(0.5f, 0.6f, 10.f, T), 0.6f);
	TestTrue(TEXT("The feed bends, it does not dissolve"), T.InterferenceStatic > 0.f && T.InterferenceStatic < 0.5f && T.InterferenceSplit > 0.f && T.InterferenceTear > 0.f);
	return true;
}

#endif
