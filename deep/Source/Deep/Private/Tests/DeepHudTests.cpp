// The HUD's panel choice per run state, as a pure function (Nov 15, HUD pass): one mode at a time and nothing
// from one mode in another; plus the easing and tween maths the motion uses.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Sim/DeepTuningTypes.h"
#include "UI/DeepHudModel.h"
#include "UI/DeepHudStyle.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_HUD_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepHudTest
{
	FDeepHudModeInputs Diver(EDeepRunState State)
	{
		FDeepHudModeInputs In;
		In.bHasDiver = true;
		In.RunState = static_cast<uint8>(State);
		return In;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHudModeTest, "Deep.UI.Hud.Mode", DEEP_HUD_TEST_FLAGS)
bool FDeepHudModeTest::RunTest(const FString& Parameters)
{
	using namespace DeepHudTest;
	FDeepHudModeInputs Title;
	Title.bOnTitle = true;
	TestEqual(TEXT("title wins over everything"), DeepHudModel::ChooseMode(Title), EDeepHudMode::Title);
	TestEqual(TEXT("no diver, no HUD"), DeepHudModel::ChooseMode(FDeepHudModeInputs()), EDeepHudMode::None);
	TestEqual(TEXT("tethered = water"), DeepHudModel::ChooseMode(Diver(EDeepRunState::Tethered)), EDeepHudMode::Water);
	TestEqual(TEXT("cut"), DeepHudModel::ChooseMode(Diver(EDeepRunState::Cut)), EDeepHudMode::Cut);
	TestEqual(TEXT("dead"), DeepHudModel::ChooseMode(Diver(EDeepRunState::Dead)), EDeepHudMode::Dead);
	TestEqual(TEXT("settling"), DeepHudModel::ChooseMode(Diver(EDeepRunState::Settling)), EDeepHudMode::Settlement);
	TestEqual(TEXT("aboard"), DeepHudModel::ChooseMode(Diver(EDeepRunState::Aboard)), EDeepHudMode::Aboard);
	TestEqual(TEXT("won = review"), DeepHudModel::ChooseMode(Diver(EDeepRunState::Won)), EDeepHudMode::Review);
	FDeepHudModeInputs Seated = Diver(EDeepRunState::Aboard);
	Seated.bSeated = true;
	TestEqual(TEXT("seated aboard = helm"), DeepHudModel::ChooseMode(Seated), EDeepHudMode::Helm);
	FDeepHudModeInputs Paused = Diver(EDeepRunState::Tethered);
	Paused.bPauseOpen = true;
	TestEqual(TEXT("pause over the water"), DeepHudModel::ChooseMode(Paused), EDeepHudMode::Pause);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHudPanelsTest, "Deep.UI.Hud.Panels", DEEP_HUD_TEST_FLAGS)
bool FDeepHudPanelsTest::RunTest(const FString& Parameters)
{
	using namespace EDeepHudPanel;
	FDeepHudModeInputs In;
	auto Has = [&](EDeepHudMode Mode, uint32 Which) { return (DeepHudModel::PanelsFor(Mode, In) & Which) != 0; };

	// Aboard: no HUD at all, only prompts on objects (and the radio line, cues, the one-time hint).
	TestFalse(TEXT("aboard: no depth"), Has(EDeepHudMode::Aboard, Depth | Cargo | Patience | Reticle | Threat | HelmGauge));
	TestTrue(TEXT("aboard: object prompts"), Has(EDeepHudMode::Aboard, ObjectPrompts));
	// Water: depth, cargo, patience, reticle; no helm, no cards, no object prompts.
	TestTrue(TEXT("water: depth"), Has(EDeepHudMode::Water, Depth));
	TestTrue(TEXT("water: cargo"), Has(EDeepHudMode::Water, Cargo));
	TestTrue(TEXT("water: patience"), Has(EDeepHudMode::Water, Patience));
	TestTrue(TEXT("water: reticle"), Has(EDeepHudMode::Water, Reticle));
	TestFalse(TEXT("water: no bailout"), Has(EDeepHudMode::Water, Bailout));
	TestFalse(TEXT("water: no bite band unless bitten"), Has(EDeepHudMode::Water, BiteAlarm));
	TestFalse(TEXT("water: no helm, prompts on objects or cards"), Has(EDeepHudMode::Water, HelmGauge | ObjectPrompts | SettlementCard | PauseCard | TitleCard));
	// Cut: bailout replaces patience.
	TestTrue(TEXT("cut: bailout"), Has(EDeepHudMode::Cut, Bailout));
	TestTrue(TEXT("cut: hatch marker"), Has(EDeepHudMode::Cut, HatchMarker));
	TestFalse(TEXT("cut: no patience"), Has(EDeepHudMode::Cut, Patience));
	// Helm: the gauge only.
	TestTrue(TEXT("helm: gauge"), Has(EDeepHudMode::Helm, HelmGauge));
	TestFalse(TEXT("helm: no water panels"), Has(EDeepHudMode::Helm, Depth | Cargo | Patience | Reticle | ObjectPrompts));
	// Cards are alone.
	TestEqual(TEXT("pause: the card only"), DeepHudModel::PanelsFor(EDeepHudMode::Pause, In), static_cast<uint32>(PauseCard));
	TestEqual(TEXT("review: the card only"), DeepHudModel::PanelsFor(EDeepHudMode::Review, In), static_cast<uint32>(ReviewCard));
	TestFalse(TEXT("settlement: no water"), Has(EDeepHudMode::Settlement, Depth | Patience | Reticle));
	TestFalse(TEXT("title: no run panels"), Has(EDeepHudMode::Title, Depth | Cargo | Feedback | ControlsHint));
	TestEqual(TEXT("none: nothing"), DeepHudModel::PanelsFor(EDeepHudMode::None, In), 0u);

	In.bBiteAlarm = true;
	In.bCarryingOrCut = true;
	TestTrue(TEXT("bitten: band"), Has(EDeepHudMode::Water, BiteAlarm));
	TestTrue(TEXT("carrying: hatch marker"), Has(EDeepHudMode::Water, HatchMarker));
	TestFalse(TEXT("bitten aboard: never"), Has(EDeepHudMode::Aboard, BiteAlarm | HatchMarker));
	TestFalse(TEXT("bitten paused: never"), Has(EDeepHudMode::Pause, BiteAlarm));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHudMotionTest, "Deep.UI.Hud.Motion", DEEP_HUD_TEST_FLAGS)
bool FDeepHudMotionTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("ease 0"), DeepHudModel::EaseOutCubic(0.f), 0.f);
	TestEqual(TEXT("ease 1"), DeepHudModel::EaseOutCubic(1.f), 1.f);
	TestEqual(TEXT("ease clamps: no overshoot"), DeepHudModel::EaseOutCubic(2.f), 1.f);
	TestTrue(TEXT("ease-out: past half at half time"), DeepHudModel::EaseOutCubic(0.5f) > 0.85f);

	// A panel opens in the guide's 200 ms and closes in 120 ms.
	DeepHudModel::FFade Fade;
	for (int32 I = 0; I < 12; ++I)
	{
		Fade.Step(true, 1.f / 60.f, DeepHudStyle::PanelOpen, DeepHudStyle::PanelClose);
	}
	TestTrue(TEXT("open after 200 ms"), Fade.Alpha() >= 0.999f);
	for (int32 I = 0; I < 4; ++I)
	{
		Fade.Step(false, 1.f / 60.f, DeepHudStyle::PanelOpen, DeepHudStyle::PanelClose);
	}
	TestTrue(TEXT("closing but not shut after 67 ms"), Fade.Alpha() > 0.f && Fade.Alpha() < 1.f);
	const float Mid = Fade.Alpha();
	Fade.Step(true, 0.f, DeepHudStyle::PanelOpen, DeepHudStyle::PanelClose);
	TestTrue(TEXT("no jump when the direction flips"), FMath::IsNearlyEqual(Fade.Alpha(), Mid, 0.01f));
	for (int32 I = 0; I < 20; ++I)
	{
		Fade.Step(false, 1.f / 60.f, DeepHudStyle::PanelOpen, DeepHudStyle::PanelClose);
	}
	TestEqual(TEXT("shut"), Fade.Alpha(), 0.f);

	// Numbers tween over 450 ms from what is shown, and never overshoot.
	DeepHudModel::FTween Tween;
	Tween.Duration = DeepHudStyle::NumberTween;
	Tween.Set(100.f, 0.0);
	TestEqual(TEXT("first value is immediate"), Tween.Value(0.0), 100.f);
	Tween.Set(200.f, 1.0);
	TestEqual(TEXT("starts from the shown value"), Tween.Value(1.0), 100.f);
	TestTrue(TEXT("moving at half time"), Tween.Value(1.225) > 150.f && Tween.Value(1.225) < 200.f);
	TestEqual(TEXT("there at 450 ms"), Tween.Value(1.45), 200.f);
	TestEqual(TEXT("holds"), Tween.Value(5.0), 200.f);
	Tween.Set(203.f, 6.0, 5.f);
	TestEqual(TEXT("a small continuous change follows directly"), Tween.Value(6.0), 203.f);
	return true;
}

#endif
