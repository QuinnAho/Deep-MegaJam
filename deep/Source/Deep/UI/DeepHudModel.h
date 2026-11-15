// The HUD's pure rules: which mode the run is in, which panels that mode shows, and the easing and tween
// maths. No engine objects, so the automation tests (Deep.UI.Hud.*) can check every state.

#pragma once

#include "CoreMinimal.h"

/** One mode at a time. Nothing from one mode shows in another. */
enum class EDeepHudMode : uint8
{
	None,
	Title,
	Aboard,
	Helm,
	Water,
	Cut,
	Dead,
	Settlement,
	Review,
	Pause,
	Settings,     // over the title, the pause menu or the review
	Credits,
	ConfirmQuit,
};

/** The panels. A mode is a set of these, chosen in one place: DeepHudModel::PanelsFor. */
namespace EDeepHudPanel
{
	enum Type : uint32
	{
		None = 0,
		TitleCard = 1 << 0,
		Subtitle = 1 << 1,
		Feedback = 1 << 2,
		ObjectPrompts = 1 << 3,  // aboard: prompts anchored to the hatch, console and helm
		HelmGauge = 1 << 4,
		Depth = 1 << 5,          // water: depth and the tether under it
		Cargo = 1 << 6,          // water: cargo, lamp and harpoon state
		Patience = 1 << 7,
		Reticle = 1 << 8,
		Threat = 1 << 9,         // the red screen edge
		BiteAlarm = 1 << 10,
		Bailout = 1 << 11,       // air left after a cut, ring around the reticle
		HatchMarker = 1 << 12,
		WaterPrompts = 1 << 13,  // grab, winch, end of the line
		ControlsHint = 1 << 14,  // shows once per mode per run, then fades
		DeadCard = 1 << 15,
		SettlementCard = 1 << 16,
		ReviewCard = 1 << 17,
		PauseCard = 1 << 18,
		SettingsCard = 1 << 19,
		CreditsCard = 1 << 20,
		ConfirmCard = 1 << 21,  // quit asks once
		Sonar = 1 << 22,        // the helm sonar, bottom left (Sonar lane)
		Count = 23,
	};
}

/** What the mode choice reads from the run. Gathered by ADeepHUD. */
struct FDeepHudModeInputs
{
	bool bOnTitle = false;
	bool bHasDiver = false;
	bool bPauseOpen = false;
	bool bSeated = false;
	/** EDeepRunState as a number, so this header needs no game types: Tethered 0, Cut 1, Dead 2, Settling 3, Aboard 4, Won 5. */
	uint8 RunState = 0;
	bool bBiteAlarm = false;
	bool bCarryingOrCut = false;
	/** The screen open over a menu: 0 none, 1 settings, 2 credits, 3 the quit confirmation. */
	uint8 MenuScreen = 0;
	/** The helm sonar is on (Deep.Sonar 1); the helm mode adds its panel. */
	bool bSonar = false;
};

namespace DeepHudModel
{
	DEEP_API EDeepHudMode ChooseMode(const FDeepHudModeInputs& In);
	DEEP_API uint32 PanelsFor(EDeepHudMode Mode, const FDeepHudModeInputs& In);
	DEEP_API const TCHAR* ModeName(EDeepHudMode Mode);

	/** Ease-out cubic, clamped: 1 - (1 - t)^3. */
	DEEP_API float EaseOutCubic(float T);

	/** A panel's visibility: moves toward its target at the guide's open and close times, eased. */
	struct DEEP_API FFade
	{
		float Linear = 0.f;
		bool bTarget = false;
		void Step(bool bShow, float DeltaSeconds, float OpenSeconds, float CloseSeconds);
		/** Eased opacity: fast at the start of both opening and closing. */
		float Alpha() const;
	};

	/** A number that eases to each new target over a fixed time, from wherever it is shown now. */
	struct DEEP_API FTween
	{
		float From = 0.f;
		float To = 0.f;
		double StartTime = -1000.0;
		float Duration = 0.45f;
		bool bInit = false;
		/** Sets a new target; a jump smaller than Snap follows directly (continuous values such as depth). */
		void Set(float Target, double Now, float Snap = 0.f);
		float Value(double Now) const;
	};
}

/** A screen rectangle in the widget's 1080p reference units. */
struct FDeepHudRect
{
	float X = 0.f;
	float Y = 0.f;
	float W = 0.f;
	float H = 0.f;
	float Right() const { return X + W; }
	float Bottom() const { return Y + H; }
	bool Overlaps(const FDeepHudRect& O) const { return X < O.Right() && O.X < Right() && Y < O.Bottom() && O.Y < Bottom(); }
	bool Inside(float LX, float LY) const { return X >= 0.f && Y >= 0.f && Right() <= LX && Bottom() <= LY; }
};

/** Where the helm's panels sit, for a local (DPI-scaled) size. SDeepHud paints with these; Deep.UI.Sonar.Layout checks them. */
namespace DeepHudLayout
{
	/** The helm instrument on the right, full height. */
	DEEP_API FDeepHudRect HelmGauge(FVector2f Local);
	/** The sonar scope, bottom left. */
	DEEP_API FDeepHudRect Sonar(FVector2f Local);
	/** The room kept above the sonar for the hull integrity bar (the Shaft lane's). */
	DEEP_API FDeepHudRect HullSlot(FVector2f Local);
	/** The controls hint at the top left with Rows prompt rows (at the helm it starts at the margin). */
	DEEP_API FDeepHudRect ControlsHint(FVector2f Local, int32 Rows);
	/** The widest the Captain's subtitle can be, bottom centre, with Lines lines of text. */
	DEEP_API FDeepHudRect SubtitleMax(FVector2f Local, int32 Lines);
	/** The sonar scope's centre and radius inside its panel. */
	DEEP_API void SonarScope(const FDeepHudRect& Panel, FVector2f& Centre, float& Radius);
}
