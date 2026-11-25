// The calm HUD (rescope, Nov 15): what each mode keeps, the controls hint that goes after first use, the hull that
// shows on change and stays when low, one prompt at a time and only critical cues. Plain frames, no world.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Sim/DeepTuningTypes.h"
#include "UI/DeepHudCalm.h"
#include "UI/DeepHudModel.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_HUDCALM_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepHudCalmTest
{
	FDeepHudFrame Make(EDeepHudMode Mode)
	{
		FDeepHudFrame F;
		F.Mode = Mode;
		F.Panels = DeepHudModel::PanelsFor(Mode, FDeepHudModeInputs());
		F.ViewportPx = FVector2f(1920.f, 1080.f);
		FDeepHudPrompt Row;
		Row.Id = TEXT("h1");
		Row.Key = TEXT("WASD");
		Row.Verb = TEXT("SWIM");
		F.Hint = { Row };
		return F;
	}

	bool Has(const FDeepHudFrame& F, uint32 Which) { return (F.Panels & Which) != 0; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHudCalmPanelsTest, "Deep.UI.HudCalm.Panels", DEEP_HUDCALM_TEST_FLAGS)
bool FDeepHudCalmPanelsTest::RunTest(const FString& Parameters)
{
	using namespace EDeepHudPanel;
	FDeepHudModeInputs In;
	In.bSonar = true;
	const uint32 Aboard = DeepHudModel::PanelsFor(EDeepHudMode::Aboard, In);
	const uint32 Helm = DeepHudModel::PanelsFor(EDeepHudMode::Helm, In);
	const uint32 Water = DeepHudModel::PanelsFor(EDeepHudMode::Water, In);
	const uint32 Cut = DeepHudModel::PanelsFor(EDeepHudMode::Cut, In);
	TestEqual(TEXT("aboard: one prompt, critical cues, the hint until used"), Aboard, static_cast<uint32>(ObjectPrompts | Feedback | ControlsHint));
	TestEqual(TEXT("helm: depth readout, hull on change, sonar"), Helm, static_cast<uint32>(HelmGauge | Hull | Feedback | ControlsHint | Sonar));
	TestEqual(TEXT("water: depth, cargo on change, reticle, threat, one prompt"), Water,
		static_cast<uint32>(Depth | Cargo | Reticle | Threat | WaterPrompts | Feedback | ControlsHint));
	TestEqual(TEXT("cut: the water plus bailout and the hatch"), Cut,
		static_cast<uint32>(Depth | Cargo | Bailout | Reticle | Threat | HatchMarker | WaterPrompts | Feedback));
	TestEqual(TEXT("dead: the card only"), DeepHudModel::PanelsFor(EDeepHudMode::Dead, In), static_cast<uint32>(DeadCard));
	for (const EDeepHudMode Mode : { EDeepHudMode::Aboard, EDeepHudMode::Helm, EDeepHudMode::Water, EDeepHudMode::Cut, EDeepHudMode::Dead,
		EDeepHudMode::Settlement, EDeepHudMode::Pause, EDeepHudMode::Review })
	{
		TestEqual(FString::Printf(TEXT("mode %d: no Captain subtitle, no patience"), static_cast<int32>(Mode)),
			DeepHudModel::PanelsFor(Mode, In) & static_cast<uint32>(Subtitle | Patience), 0u);
	}

	// At rest the calm layer hides the hull and the cargo: nothing has changed yet.
	FDeepHudCalm Calm;
	FDeepHudFrame F = DeepHudCalmTest::Make(EDeepHudMode::Water);
	Calm.Apply(F, 0.0);
	TestFalse(TEXT("water at rest: no cargo panel"), DeepHudCalmTest::Has(F, Cargo));
	TestTrue(TEXT("water at rest: depth stays"), DeepHudCalmTest::Has(F, Depth));
	F = DeepHudCalmTest::Make(EDeepHudMode::Water);
	F.CargoKg = 12.f;
	Calm.Apply(F, 1.0);
	TestTrue(TEXT("cargo shows when the load changes"), DeepHudCalmTest::Has(F, Cargo));
	F = DeepHudCalmTest::Make(EDeepHudMode::Water);
	F.CargoKg = 12.f;
	Calm.Apply(F, 1.0 + Calm.Tuning.CargoShowSeconds + 0.1);
	TestFalse(TEXT("cargo goes again"), DeepHudCalmTest::Has(F, Cargo));
	F = DeepHudCalmTest::Make(EDeepHudMode::Water);
	F.CargoKg = 12.f;
	F.bOverloaded = true;
	Calm.Apply(F, 10.0);
	TestTrue(TEXT("too heavy keeps cargo up"), DeepHudCalmTest::Has(F, Cargo));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHudCalmHintTest, "Deep.UI.HudCalm.Hint", DEEP_HUDCALM_TEST_FLAGS)
bool FDeepHudCalmHintTest::RunTest(const FString& Parameters)
{
	using namespace EDeepHudPanel;
	FDeepHudCalm Calm;
	const float Min = Calm.Tuning.HintMinSeconds;

	FDeepHudFrame F = DeepHudCalmTest::Make(EDeepHudMode::Water);
	Calm.Apply(F, 0.0);
	TestTrue(TEXT("the water hint shows on the first dive"), DeepHudCalmTest::Has(F, ControlsHint));
	TestTrue(TEXT("the water hint ends with T unclip"), F.Hint.Num() == 2 && F.Hint.Last().Key == TEXT("T") && F.Hint.Last().Verb == TEXT("UNCLIP"));
	{
		// CUT LOOSE (Q hold) is no longer taught: T UNCLIP leaves the line for free.
		FDeepHudCalm Other;
		FDeepHudFrame G = DeepHudCalmTest::Make(EDeepHudMode::Water);
		FDeepHudPrompt Cut;
		Cut.Id = TEXT("h6");
		Cut.Key = TEXT("Q");
		Cut.Verb = TEXT("CUT LOOSE");
		G.Hint.Add(Cut);
		Other.Apply(G, 0.0);
		TestFalse(TEXT("no CUT LOOSE row in the water hint"), G.Hint.ContainsByPredicate([](const FDeepHudPrompt& P) { return P.Verb == TEXT("CUT LOOSE"); }));
		G = DeepHudCalmTest::Make(EDeepHudMode::Pause);
		Other.Apply(G, 1.0);
		TestFalse(TEXT("no CUT LOOSE row in the pause list"), G.Hint.ContainsByPredicate([](const FDeepHudPrompt& P) { return P.Verb == TEXT("CUT LOOSE"); }));
	}
	F = DeepHudCalmTest::Make(EDeepHudMode::Water);
	F.DepthM = 1.f;
	Calm.Apply(F, 1.0);
	TestTrue(TEXT("a small move is not yet use"), DeepHudCalmTest::Has(F, ControlsHint));
	F = DeepHudCalmTest::Make(EDeepHudMode::Water);
	F.DepthM = Calm.Tuning.WaterUseM + 1.f;
	Calm.Apply(F, 0.5);
	TestTrue(TEXT("used at once, the hint still stays long enough to read"), DeepHudCalmTest::Has(F, ControlsHint));
	F = DeepHudCalmTest::Make(EDeepHudMode::Water);
	F.DepthM = Calm.Tuning.WaterUseM + 1.f;
	Calm.Apply(F, Min + 0.1);
	TestFalse(TEXT("used: the hint goes"), DeepHudCalmTest::Has(F, ControlsHint));
	F = DeepHudCalmTest::Make(EDeepHudMode::Water);
	Calm.Apply(F, Min + 5.0);
	TestFalse(TEXT("and stays gone"), DeepHudCalmTest::Has(F, ControlsHint));

	// Pause keeps the full list; the water hint does not come back after it.
	F = DeepHudCalmTest::Make(EDeepHudMode::Pause);
	F.Hint.Reset();
	Calm.Apply(F, 20.0);
	TestEqual(TEXT("pause carries the last controls list (with T unclip)"), F.Hint.Num(), 2);
	F = DeepHudCalmTest::Make(EDeepHudMode::Water);
	Calm.Apply(F, 21.0);
	TestFalse(TEXT("back from pause: still gone"), DeepHudCalmTest::Has(F, ControlsHint));

	// The helm: its own keys, until the sub moves. D-070: the direct drive's held keys by default, the levers behind their switch.
	F = DeepHudCalmTest::Make(EDeepHudMode::Helm);
	F.SubDepthM = 100.f;
	Calm.Apply(F, 30.0);
	TestTrue(TEXT("the helm hint shows"), DeepHudCalmTest::Has(F, ControlsHint));
	TestEqual(TEXT("the helm hint is the direct drive's keys"), F.Hint.Num(), DeepHudCalm::DirectHelmHint().Num());
	TestEqual(TEXT("ahead and astern first"), F.Hint[0].Verb, FString(TEXT("AHEAD / ASTERN")));
	F = DeepHudCalmTest::Make(EDeepHudMode::Helm);
	F.SubDepthM = 100.f;
	F.bHelmDirect = false;
	Calm.Apply(F, 30.05);
	TestEqual(TEXT("the levers' hint behind their switch"), F.Hint.Num(), DeepHudCalm::HelmHint().Num());
	TestEqual(TEXT("throttle first"), F.Hint.Num() > 0 ? F.Hint[0].Verb : FString(), FString(TEXT("THROTTLE")));
	F = DeepHudCalmTest::Make(EDeepHudMode::Helm);
	F.SubDepthM = 100.f + Calm.Tuning.HelmUseM + 0.5f;
	Calm.Apply(F, 30.0 + Min + 0.1);
	TestFalse(TEXT("the sub moved: the helm hint goes"), DeepHudCalmTest::Has(F, ControlsHint));

	// Aboard: leaving the mode counts as use.
	F = DeepHudCalmTest::Make(EDeepHudMode::Aboard);
	Calm.Apply(F, 40.0);
	TestTrue(TEXT("the aboard hint shows"), DeepHudCalmTest::Has(F, ControlsHint));
	F = DeepHudCalmTest::Make(EDeepHudMode::Helm);
	Calm.Apply(F, 41.0);
	F = DeepHudCalmTest::Make(EDeepHudMode::Aboard);
	Calm.Apply(F, 42.0);
	TestFalse(TEXT("back aboard after the helm: gone"), DeepHudCalmTest::Has(F, ControlsHint));

	// A new run brings every hint back once.
	F = DeepHudCalmTest::Make(EDeepHudMode::Title);
	Calm.Apply(F, 50.0);
	F = DeepHudCalmTest::Make(EDeepHudMode::Water);
	Calm.Apply(F, 51.0);
	TestTrue(TEXT("new run: the water hint again"), DeepHudCalmTest::Has(F, ControlsHint));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHudCalmHullTest, "Deep.UI.HudCalm.Hull", DEEP_HUDCALM_TEST_FLAGS)
bool FDeepHudCalmHullTest::RunTest(const FString& Parameters)
{
	using namespace EDeepHudPanel;
	FDeepHudCalm Calm;
	const float Hold = Calm.Tuning.HullShowSeconds;
	auto Step = [&](float Hull01, double Now)
	{
		FDeepHudFrame F = DeepHudCalmTest::Make(EDeepHudMode::Helm);
		F.Hull01 = Hull01;
		Calm.Apply(F, Now);
		return F;
	};
	FDeepHudFrame F = Step(1.f, 0.0);
	TestFalse(TEXT("a full hull at rest is hidden"), DeepHudCalmTest::Has(F, Hull));
	F = Step(0.8f, 1.0);
	TestTrue(TEXT("a hit shows the hull"), DeepHudCalmTest::Has(F, Hull));
	TestTrue(TEXT("a hit raises HULL HIT"), F.Cues.Num() == 1 && F.Cues[0].Label == TEXT("Hull hit"));
	F = Step(0.8f, 1.0 + Hold - 0.1);
	TestTrue(TEXT("still up inside the hold"), DeepHudCalmTest::Has(F, Hull));
	TestEqual(TEXT("one cue per hit"), F.Cues.Num(), 0);
	F = Step(0.8f, 1.0 + Hold + 0.1);
	TestFalse(TEXT("then it fades"), DeepHudCalmTest::Has(F, Hull));
	F = Step(0.2f, 10.0);
	TestTrue(TEXT("crossing into critical says so"), F.Cues.Num() == 1 && F.Cues[0].Label == TEXT("Hull critical") && F.Cues[0].Tone == EDeepHudTone::Danger);
	F = Step(0.2f, 100.0);
	TestTrue(TEXT("below the threshold it stays up"), DeepHudCalmTest::Has(F, Hull));
	F = Step(-1.f, 101.0);
	TestFalse(TEXT("no hull value, no bar"), DeepHudCalmTest::Has(F, Hull));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHudCalmFilterTest, "Deep.UI.HudCalm.Filter", DEEP_HUDCALM_TEST_FLAGS)
bool FDeepHudCalmFilterTest::RunTest(const FString& Parameters)
{
	auto Cue = [](const TCHAR* Label, EDeepHudTone Tone) { FDeepHudCue C; C.Label = Label; C.Tone = Tone; return C; };
	TestTrue(TEXT("tether cut is critical"), DeepHudCalm::IsCriticalCue(Cue(TEXT("Tether cut"), EDeepHudTone::Danger)));
	TestTrue(TEXT("a bite is critical"), DeepHudCalm::IsCriticalCue(Cue(TEXT("Bitten: the line"), EDeepHudTone::Danger)));
	TestFalse(TEXT("a hull upgrade is not"), DeepHudCalm::IsCriticalCue(Cue(TEXT("Hull upgrade bought"), EDeepHudTone::Primary)));
	TestFalse(TEXT("grabbed is not"), DeepHudCalm::IsCriticalCue(Cue(TEXT("Grabbed +4 kg"), EDeepHudTone::Normal)));
	TestFalse(TEXT("a recoil is not"), DeepHudCalm::IsCriticalCue(Cue(TEXT("Hit it recoils"), EDeepHudTone::Danger)));

	// One prompt: in reach (keyed), nearest the centre.
	FDeepHudFrame F = DeepHudCalmTest::Make(EDeepHudMode::Aboard);
	auto Prompt = [](const TCHAR* Id, const TCHAR* Key, FVector2f At) { FDeepHudPrompt P; P.Id = Id; P.Key = Key; P.Verb = Id; P.bAnchored = true; P.ScreenPx = At; return P; };
	F.Prompts = { Prompt(TEXT("far"), TEXT(""), FVector2f(960.f, 540.f)), Prompt(TEXT("edge"), TEXT("E"), FVector2f(100.f, 100.f)),
		Prompt(TEXT("near"), TEXT("E"), FVector2f(900.f, 500.f)) };
	FDeepHudCalm Calm;
	Calm.Apply(F, 0.0);
	TestTrue(TEXT("one prompt, the nearest in reach"), F.Prompts.Num() == 1 && F.Prompts[0].Id == FName(TEXT("near")));
	F = DeepHudCalmTest::Make(EDeepHudMode::Aboard);
	F.Prompts = { Prompt(TEXT("far"), TEXT(""), FVector2f(960.f, 540.f)) };
	Calm.Apply(F, 1.0);
	TestEqual(TEXT("nothing in reach, no prompt"), F.Prompts.Num(), 0);

	// Air low, once, in the bailout.
	F = DeepHudCalmTest::Make(EDeepHudMode::Cut);
	F.AirMax = 30.f;
	F.AirSeconds = 5.f;
	Calm.Apply(F, 2.0);
	TestTrue(TEXT("air low"), F.Cues.Num() == 1 && F.Cues[0].Label == TEXT("Air low"));
	F = DeepHudCalmTest::Make(EDeepHudMode::Cut);
	F.AirMax = 30.f;
	F.AirSeconds = 4.f;
	Calm.Apply(F, 3.0);
	TestEqual(TEXT("said once"), F.Cues.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHudCalmHelmTest, "Deep.UI.HudCalm.Helm", DEEP_HUDCALM_TEST_FLAGS)
bool FDeepHudCalmHelmTest::RunTest(const FString& Parameters)
{
	using namespace EDeepHudPanel;

	// The seated readout's labels: the throttle telegraph, the ballast lever, the heading.
	TestEqual(TEXT("-2"), DeepHudCalm::ThrottleLabel(-2), FString(TEXT("FULL ASTERN")));
	TestEqual(TEXT("-1"), DeepHudCalm::ThrottleLabel(-1), FString(TEXT("HALF ASTERN")));
	TestEqual(TEXT("0"), DeepHudCalm::ThrottleLabel(0), FString(TEXT("STOP")));
	TestEqual(TEXT("1"), DeepHudCalm::ThrottleLabel(1), FString(TEXT("SLOW")));
	TestEqual(TEXT("2"), DeepHudCalm::ThrottleLabel(2), FString(TEXT("HALF")));
	TestEqual(TEXT("3"), DeepHudCalm::ThrottleLabel(3), FString(TEXT("FULL AHEAD")));
	TestEqual(TEXT("past the stops clamps"), DeepHudCalm::ThrottleLabel(9), FString(TEXT("FULL AHEAD")));

	TestEqual(TEXT("blown"), DeepHudCalm::BallastLabel(-1.f), FString(TEXT("BLOWN")));
	TestEqual(TEXT("neutral"), DeepHudCalm::BallastLabel(0.f), FString(TEXT("NEUTRAL")));
	TestEqual(TEXT("flooded"), DeepHudCalm::BallastLabel(1.f), FString(TEXT("FLOODED")));
	TestEqual(TEXT("part flooded"), DeepHudCalm::BallastLabel(0.4f), FString(TEXT("FLOOD 40%")));
	TestEqual(TEXT("part blown"), DeepHudCalm::BallastLabel(-0.25f), FString(TEXT("BLOW 25%")));

	TestEqual(TEXT("north"), DeepHudCalm::HeadingLabel(0.f), FString(TEXT("000°")));
	TestEqual(TEXT("left of north wraps"), DeepHudCalm::HeadingLabel(-45.f), FString(TEXT("315°")));
	TestEqual(TEXT("just under 360 rounds to 000"), DeepHudCalm::HeadingLabel(359.7f), FString(TEXT("000°")));

	// The hint teaches the ballast as a lever.
	const TArray<FDeepHudPrompt> Hint = DeepHudCalm::HelmHint();
	const FDeepHudPrompt* Ballast = Hint.FindByPredicate([](const FDeepHudPrompt& P) { return P.Verb == TEXT("BALLAST"); });
	TestTrue(TEXT("ballast row"), Ballast != nullptr);
	TestTrue(TEXT("ballast is a lever that stays"), Ballast && Ballast->Detail.Contains(TEXT("stays")) && Ballast->Detail.Contains(TEXT("C floods")));

	// D-070: the direct drive's hint names every held key, and X (not E, which strafes) stands up.
	const TArray<FDeepHudPrompt> Direct = DeepHudCalm::DirectHelmHint();
	const auto Row = [&Direct](const TCHAR* Verb) { return Direct.FindByPredicate([Verb](const FDeepHudPrompt& P) { return P.Verb == Verb; }); };
	TestTrue(TEXT("W / S ahead and astern"), Row(TEXT("AHEAD / ASTERN")) && Row(TEXT("AHEAD / ASTERN"))->Key == TEXT("W / S"));
	TestTrue(TEXT("A / D turn"), Row(TEXT("TURN")) && Row(TEXT("TURN"))->Key == TEXT("A / D"));
	TestTrue(TEXT("SPACE / C up and down"), Row(TEXT("UP / DOWN")) && Row(TEXT("UP / DOWN"))->Key == TEXT("SPACE / C"));
	TestTrue(TEXT("Q / E strafe"), Row(TEXT("STRAFE")) && Row(TEXT("STRAFE"))->Key == TEXT("Q / E"));
	TestTrue(TEXT("V switches the view"), Row(TEXT("VIEW")) && Row(TEXT("VIEW"))->Key == TEXT("V"));
	TestTrue(TEXT("X stands up"), Row(TEXT("STAND UP")) && Row(TEXT("STAND UP"))->Key == TEXT("X"));
	TestFalse(TEXT("nothing latches: no ballast, no all stop"), Row(TEXT("BALLAST")) || Row(TEXT("ALL STOP")));
	// The readout's words: the way along the heading and the way down.
	TestEqual(TEXT("stopped"), DeepHudCalm::SpeedLabel(0.02f), FString(TEXT("STOPPED")));
	TestEqual(TEXT("ahead"), DeepHudCalm::SpeedLabel(2.44f), FString(TEXT("2.4 m/s AHEAD")));
	TestEqual(TEXT("astern"), DeepHudCalm::SpeedLabel(-1.f), FString(TEXT("1.0 m/s ASTERN")));
	TestEqual(TEXT("level"), DeepHudCalm::HeaveLabel(0.f), FString(TEXT("LEVEL")));
	TestEqual(TEXT("down"), DeepHudCalm::HeaveLabel(1.26f), FString(TEXT("1.3 m/s DOWN")));
	TestEqual(TEXT("up"), DeepHudCalm::HeaveLabel(-2.5f), FString(TEXT("2.5 m/s UP")));

	// Deep.Hud.HelmSonar: off by default; 1 shows the panel; 0 keeps the readout.
	FDeepHudModeInputs In;
	In.bSonar = true;
	FDeepHudCalm Calm;
	TestFalse(TEXT("sonar off by default"), Calm.Tuning.bHelmSonar);
	FDeepHudFrame F = DeepHudCalmTest::Make(EDeepHudMode::Helm);
	F.Panels = DeepHudModel::PanelsFor(EDeepHudMode::Helm, In);
	Calm.Tuning.bHelmSonar = true;
	Calm.Apply(F, 0.0);
	TestTrue(TEXT("sonar shows when turned on"), DeepHudCalmTest::Has(F, Sonar));
	Calm.Tuning.bHelmSonar = false;
	F.Panels = DeepHudModel::PanelsFor(EDeepHudMode::Helm, In);
	Calm.Apply(F, 0.1);
	TestFalse(TEXT("sonar hidden"), DeepHudCalmTest::Has(F, Sonar));
	TestTrue(TEXT("readout kept"), DeepHudCalmTest::Has(F, HelmGauge));
	return true;
}

#endif
