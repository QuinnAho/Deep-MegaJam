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
	// Water: depth, cargo (shown on change), reticle; no helm, no cards, no object prompts. The calm HUD (Nov 15) cut the
	// Captain's patience bar and his subtitle from every play mode.
	TestTrue(TEXT("water: depth"), Has(EDeepHudMode::Water, Depth));
	TestTrue(TEXT("water: cargo"), Has(EDeepHudMode::Water, Cargo));
	TestFalse(TEXT("water: no patience (calm HUD)"), Has(EDeepHudMode::Water, Patience));
	for (const EDeepHudMode Mode : { EDeepHudMode::Aboard, EDeepHudMode::Helm, EDeepHudMode::Water, EDeepHudMode::Cut, EDeepHudMode::Dead, EDeepHudMode::Settlement })
	{
		TestFalse(FString::Printf(TEXT("mode %d: no Captain subtitle, no patience (calm HUD)"), static_cast<int32>(Mode)), Has(Mode, Subtitle | Patience));
	}
	TestTrue(TEXT("water: reticle"), Has(EDeepHudMode::Water, Reticle));
	TestFalse(TEXT("water: no bailout"), Has(EDeepHudMode::Water, Bailout));
	TestFalse(TEXT("water: no bite band unless bitten"), Has(EDeepHudMode::Water, BiteAlarm));
	TestFalse(TEXT("water: no helm, prompts on objects or cards"), Has(EDeepHudMode::Water, HelmGauge | ObjectPrompts | SettlementCard | PauseCard | TitleCard));
	// Cut: bailout replaces patience.
	TestTrue(TEXT("cut: bailout"), Has(EDeepHudMode::Cut, Bailout));
	TestTrue(TEXT("cut: hatch marker"), Has(EDeepHudMode::Cut, HatchMarker));
	TestFalse(TEXT("cut: no patience"), Has(EDeepHudMode::Cut, Patience));
	// Helm: the slim depth readout (the HelmGauge bit).
	TestTrue(TEXT("helm: depth readout"), Has(EDeepHudMode::Helm, HelmGauge));
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
	TestFalse(TEXT("bitten: the controls hint gives way to the band"), Has(EDeepHudMode::Water, ControlsHint));
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

// Direction A: one colour rule for every bar (patience, hull).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHudBarToneTest, "Deep.UI.Hud.BarTone", DEEP_HUD_TEST_FLAGS)
bool FDeepHudBarToneTest::RunTest(const FString& Parameters)
{
	using DeepHudModel::EBarTone;
	TestTrue(TEXT("full is primary"), DeepHudModel::BarTone(1.f) == EBarTone::Normal);
	TestTrue(TEXT("just above half is primary"), DeepHudModel::BarTone(0.51f) == EBarTone::Normal);
	TestTrue(TEXT("half is amber"), DeepHudModel::BarTone(0.5f) == EBarTone::Warn);
	TestTrue(TEXT("just above a quarter is amber"), DeepHudModel::BarTone(0.26f) == EBarTone::Warn);
	TestTrue(TEXT("a quarter is alert"), DeepHudModel::BarTone(0.25f) == EBarTone::Danger);
	TestTrue(TEXT("empty is alert"), DeepHudModel::BarTone(0.f) == EBarTone::Danger);
	TestTrue(TEXT("the thresholds are the style's"), DeepHudStyle::BarWarnAt == 0.5f && DeepHudStyle::BarDangerAt == 0.25f);
	return true;
}

// The hull bar: eases down on a hit, leaves a ghost that fades, flashes the hairline, breaks at zero.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHudHullBarTest, "Deep.UI.Hud.HullBar", DEEP_HUD_TEST_FLAGS)
bool FDeepHudHullBarTest::RunTest(const FString& Parameters)
{
	DeepHudModel::FHullBar Bar;
	Bar.Step(1.f, 0.0);
	TestEqual(TEXT("first value shows at once"), Bar.Fill01(0.0), 1.f);
	TestEqual(TEXT("no ghost before a hit"), Bar.GhostAlpha(0.0), 0.f);
	TestEqual(TEXT("no flash before a hit"), Bar.FlashAlpha(0.0), 0.f);
	TestFalse(TEXT("whole hull is not a breach"), Bar.IsBreach());

	// A hit to 60 percent at t = 1.
	Bar.Step(0.6f, 1.0);
	TestEqual(TEXT("the fill starts where it stood"), Bar.Fill01(1.0), 1.f);
	TestTrue(TEXT("the fill is on its way down at half the lerp"), Bar.Fill01(1.0 + DeepHudStyle::BarLerp * 0.5) < 1.f && Bar.Fill01(1.0 + DeepHudStyle::BarLerp * 0.5) > 0.6f);
	TestEqual(TEXT("the fill arrives after the bar lerp"), Bar.Fill01(1.0 + DeepHudStyle::BarLerp), 0.6f);
	TestEqual(TEXT("the ghost spans up to where it stood"), Bar.GhostTop, 1.f);
	TestEqual(TEXT("the ghost is full at the hit"), Bar.GhostAlpha(1.0), 1.f);
	TestTrue(TEXT("the ghost is fading at 300 ms"), Bar.GhostAlpha(1.3) > 0.f && Bar.GhostAlpha(1.3) < 1.f);
	TestEqual(TEXT("the ghost is gone after its fade"), Bar.GhostAlpha(1.0 + DeepHudStyle::GhostFade), 0.f);
	TestEqual(TEXT("the hairline flashes at the hit"), Bar.FlashAlpha(1.05), 1.f);
	TestEqual(TEXT("the flash is over after 120 ms"), Bar.FlashAlpha(1.0 + DeepHudStyle::HitFlash + 0.001), 0.f);

	// A second hit while the ghost still shows keeps the ghost's top.
	Bar.Step(0.4f, 1.2);
	TestEqual(TEXT("a quick second hit keeps the ghost's top"), Bar.GhostTop, 1.f);
	TestEqual(TEXT("and restarts the ghost"), Bar.GhostAlpha(1.2), 1.f);
	// A later hit after the ghost faded starts from the current value.
	Bar.Step(0.3f, 3.0);
	TestEqual(TEXT("a later hit's ghost starts at the last value"), Bar.GhostTop, 0.4f);

	// A repair eases up with no ghost and no flash.
	Bar.Step(0.8f, 5.0);
	TestEqual(TEXT("a repair has no flash"), Bar.FlashAlpha(5.0), 0.f);
	TestEqual(TEXT("a repair eases up"), Bar.Fill01(5.0 + DeepHudStyle::BarLerp), 0.8f);

	Bar.Step(0.f, 6.0);
	TestTrue(TEXT("zero is a breach"), Bar.IsBreach());
	Bar.Step(-0.5f, 6.1);
	TestTrue(TEXT("below zero clamps to a breach"), Bar.IsBreach());
	return true;
}

// The helm gauge's SUB and LIMIT labels never overlap, wherever the sub is (the bug at the limit, Nov 15).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHudGaugeLabelsTest, "Deep.UI.Hud.GaugeLabels", DEEP_HUD_TEST_FLAGS)
bool FDeepHudGaugeLabelsTest::RunTest(const FString& Parameters)
{
	const FVector2f L(1920.f, 1080.f);
	// The calm HUD (Nov 15) replaced the gauge column with a slim readout; the label rule is kept and tested on the
	// old 400 px column so it stays correct if the gauge comes back.
	const FDeepHudRect Gauge{ L.X - 48.f - 400.f, 48.f, 400.f, L.Y - 96.f };
	const float GX = Gauge.X + 16.f + 120.f;
	const float LimitY = 700.f;
	const FVector2f SubSize(34.f, 22.f);
	const FVector2f LimitSize(96.f, 22.f);
	int32 Shifted = 0;
	for (float SubY = 300.f; SubY <= LimitY + 12.f; SubY += 1.f)
	{
		const DeepHudLayout::FGaugeLabels G = DeepHudLayout::GaugeLabels(GX, SubY, LimitY, SubSize, LimitSize, Gauge.Right());
		if (G.Sub.Overlaps(G.Limit))
		{
			AddError(FString::Printf(TEXT("SUB and LIMIT overlap with the sub at y %.0f"), SubY));
			return false;
		}
		if (G.Limit.Right() > Gauge.Right() - 16.f + 0.01f)
		{
			AddError(FString::Printf(TEXT("LIMIT leaves the panel with the sub at y %.0f"), SubY));
			return false;
		}
		Shifted += G.bShifted ? 1 : 0;
	}
	TestTrue(TEXT("LIMIT moves only near the limit"), Shifted > 0 && Shifted < 80);
	const DeepHudLayout::FGaugeLabels Far = DeepHudLayout::GaugeLabels(GX, 300.f, LimitY, SubSize, LimitSize, Gauge.Right());
	TestFalse(TEXT("far from the limit, LIMIT stays put"), Far.bShifted);
	TestEqual(TEXT("far from the limit, LIMIT sits by the gauge"), Far.Limit.X, GX + 32.f);
	const DeepHudLayout::FGaugeLabels AtLimit = DeepHudLayout::GaugeLabels(GX, LimitY, LimitY, SubSize, LimitSize, Gauge.Right());
	TestTrue(TEXT("at the limit, LIMIT clears SUB by two grid steps"), AtLimit.Limit.X >= AtLimit.Sub.Right() + 16.f - 0.01f);
	TestEqual(TEXT("SUB stays with its arrow"), AtLimit.Sub.X, GX + 24.f);
	return true;
}

// Direction A's water panels and the hull bar sit on screen and clear of each other.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHudLayoutATest, "Deep.UI.Hud.Layout", DEEP_HUD_TEST_FLAGS)
bool FDeepHudLayoutATest::RunTest(const FString& Parameters)
{
	using namespace EDeepHudPanel;
	FDeepHudModeInputs In;
	TestTrue(TEXT("the helm shows the hull bar"), (DeepHudModel::PanelsFor(EDeepHudMode::Helm, In) & Hull) != 0);
	TestFalse(TEXT("the water has no hull bar"), (DeepHudModel::PanelsFor(EDeepHudMode::Water, In) & Hull) != 0);
	TestFalse(TEXT("aboard has no hull bar"), (DeepHudModel::PanelsFor(EDeepHudMode::Aboard, In) & Hull) != 0);
	TestTrue(TEXT("the hull bit is inside the panel count"), static_cast<uint32>(Hull) < (1u << EDeepHudPanel::Count));

	for (const FVector2f L : { FVector2f(1920.f, 1080.f), FVector2f(1440.f, 1080.f), FVector2f(2560.f, 1080.f) })
	{
		const FDeepHudRect Depth = DeepHudLayout::DepthPanel(L);
		const FDeepHudRect Cargo = DeepHudLayout::CargoPanel(L);
		const FDeepHudRect Subtitle = DeepHudLayout::SubtitleMax(L, 3);
		const FString At = FString::Printf(TEXT(" at %.0fx%.0f"), L.X, L.Y);
		// The calm HUD (Nov 15): the water depth is small, a label and the number.
		TestTrue(TEXT("depth is compact") + At, Depth.W == DeepHudStyle::CompactPanelW && Depth.H == DeepHudStyle::CompactPanelH);
		TestTrue(TEXT("depth on screen") + At, Depth.Inside(L.X, L.Y));
		TestTrue(TEXT("cargo on screen") + At, Cargo.Inside(L.X, L.Y));
		TestFalse(TEXT("depth clear of the subtitle") + At, Depth.Overlaps(Subtitle));
		TestFalse(TEXT("cargo clear of the subtitle") + At, Cargo.Overlaps(Subtitle));
		TestFalse(TEXT("depth clear of cargo") + At, Depth.Overlaps(Cargo));
		const FDeepHudRect Hull = DeepHudLayout::HullSlot(L);
		TestTrue(TEXT("the hull bar is 400 x 48, 496 above the bottom margin") + At,
			Hull.W == 400.f && Hull.H == 48.f && FMath::IsNearlyEqual(L.Y - DeepHudStyle::Margin - Hull.Y, 496.f + 48.f));
	}
	return true;
}

#endif
