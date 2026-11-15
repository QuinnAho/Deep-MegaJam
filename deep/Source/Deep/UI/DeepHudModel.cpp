#include "UI/DeepHudModel.h"

namespace DeepHudModel
{
	/** The screen open over a menu, or None. */
	static EDeepHudMode DeepHudScreenMode(uint8 MenuScreen)
	{
		switch (MenuScreen)
		{
		case 1: return EDeepHudMode::Settings;
		case 2: return EDeepHudMode::Credits;
		case 3: return EDeepHudMode::ConfirmQuit;
		default: return EDeepHudMode::None;
		}
	}

	EDeepHudMode ChooseMode(const FDeepHudModeInputs& In)
	{
		const EDeepHudMode Screen = DeepHudScreenMode(In.MenuScreen);
		if (In.bOnTitle)
		{
			return Screen != EDeepHudMode::None ? Screen : EDeepHudMode::Title;
		}
		if (!In.bHasDiver)
		{
			return EDeepHudMode::None;
		}
		// The controller opens a screen only over the pause menu or the review.
		if (Screen != EDeepHudMode::None && (In.bPauseOpen || In.RunState == 5))
		{
			return Screen;
		}
		if (In.bPauseOpen)
		{
			return EDeepHudMode::Pause;
		}
		switch (In.RunState)
		{
		case 0: return EDeepHudMode::Water;
		case 1: return EDeepHudMode::Cut;
		case 2: return EDeepHudMode::Dead;
		case 3: return EDeepHudMode::Settlement;
		case 4: return In.bSeated ? EDeepHudMode::Helm : EDeepHudMode::Aboard;
		case 5: return EDeepHudMode::Review;
		default: return EDeepHudMode::None;
		}
	}

	uint32 PanelsFor(EDeepHudMode Mode, const FDeepHudModeInputs& In)
	{
		using namespace EDeepHudPanel;
		switch (Mode)
		{
		case EDeepHudMode::Title:
			return TitleCard | Subtitle;
		case EDeepHudMode::Aboard:
			return ObjectPrompts | Subtitle | Feedback | ControlsHint;
		case EDeepHudMode::Helm:
			return HelmGauge | Subtitle | Feedback | ControlsHint | (In.bSonar ? Sonar : None);
		case EDeepHudMode::Water:
			return Depth | Cargo | Patience | Reticle | Threat | WaterPrompts | Subtitle | Feedback | ControlsHint
				| (In.bBiteAlarm ? BiteAlarm : None) | (In.bCarryingOrCut ? HatchMarker : None);
		case EDeepHudMode::Cut:
			return Depth | Cargo | Bailout | Reticle | Threat | HatchMarker | WaterPrompts | Subtitle | Feedback
				| (In.bBiteAlarm ? BiteAlarm : None);
		case EDeepHudMode::Dead:
			return DeadCard | Subtitle;
		case EDeepHudMode::Settlement:
			return SettlementCard | Subtitle | Feedback;
		case EDeepHudMode::Review:
			return ReviewCard;
		case EDeepHudMode::Pause:
			return PauseCard;
		// Over the title the black title card stays behind the screen (its words fade under any card).
		case EDeepHudMode::Settings:
			return SettingsCard | (In.bOnTitle ? TitleCard : None);
		case EDeepHudMode::Credits:
			return CreditsCard | (In.bOnTitle ? TitleCard : None);
		case EDeepHudMode::ConfirmQuit:
			return ConfirmCard | (In.bOnTitle ? TitleCard : None);
		default:
			return None;
		}
	}

	const TCHAR* ModeName(EDeepHudMode Mode)
	{
		switch (Mode)
		{
		case EDeepHudMode::Title: return TEXT("title");
		case EDeepHudMode::Aboard: return TEXT("aboard");
		case EDeepHudMode::Helm: return TEXT("helm");
		case EDeepHudMode::Water: return TEXT("water");
		case EDeepHudMode::Cut: return TEXT("cut");
		case EDeepHudMode::Dead: return TEXT("dead");
		case EDeepHudMode::Settlement: return TEXT("settlement");
		case EDeepHudMode::Review: return TEXT("review");
		case EDeepHudMode::Pause: return TEXT("pause");
		case EDeepHudMode::Settings: return TEXT("settings");
		case EDeepHudMode::Credits: return TEXT("credits");
		case EDeepHudMode::ConfirmQuit: return TEXT("confirm_quit");
		default: return TEXT("none");
		}
	}

	float EaseOutCubic(float T)
	{
		const float U = 1.f - FMath::Clamp(T, 0.f, 1.f);
		return 1.f - U * U * U;
	}

	void FFade::Step(bool bShow, float DeltaSeconds, float OpenSeconds, float CloseSeconds)
	{
		if (bShow != bTarget)
		{
			// Keep the shown opacity continuous when the direction flips mid-motion.
			const float Shown = Alpha();
			bTarget = bShow;
			// Invert the easing for the new direction: open shows Ease(L), close shows 1 - Ease(1 - L).
			const float Root = 1.f - FMath::Pow(FMath::Clamp(1.f - Shown, 0.f, 1.f), 1.f / 3.f); // Ease^-1(Shown)
			const float RootClose = FMath::Pow(FMath::Clamp(Shown, 0.f, 1.f), 1.f / 3.f);      // 1 - Ease^-1(1 - Shown)
			Linear = bShow ? Root : RootClose;
		}
		const float Rate = bShow ? (OpenSeconds > 0.f ? DeltaSeconds / OpenSeconds : 1.f) : (CloseSeconds > 0.f ? -DeltaSeconds / CloseSeconds : -1.f);
		Linear = FMath::Clamp(Linear + Rate, 0.f, 1.f);
	}

	float FFade::Alpha() const
	{
		return bTarget ? EaseOutCubic(Linear) : 1.f - EaseOutCubic(1.f - Linear);
	}

	void FTween::Set(float Target, double Now, float Snap)
	{
		if (!bInit)
		{
			From = To = Target;
			StartTime = Now - Duration;
			bInit = true;
			return;
		}
		if (FMath::IsNearlyEqual(Target, To))
		{
			return;
		}
		const bool bSettled = Now - StartTime >= Duration;
		if (Snap > 0.f && bSettled && FMath::Abs(Target - To) < Snap)
		{
			From = To = Target;
			return;
		}
		From = Value(Now);
		To = Target;
		StartTime = Now;
	}

	float FTween::Value(double Now) const
	{
		const float T = Duration > 0.f ? static_cast<float>((Now - StartTime) / Duration) : 1.f;
		return FMath::Lerp(From, To, EaseOutCubic(T));
	}
}

// The helm layout. Numbers are the style guide's grid (DeepHudStyle: Margin 48, Pad 16, Grid 8) and the sizes the
// existing panels already paint at; SDeepHud reads the same numbers so the tests check what is drawn.
namespace DeepHudLayout
{
	namespace
	{
		constexpr float Margin = 48.f;
		constexpr float Pad = 16.f;
		constexpr float Grid = 8.f;
		constexpr float SideW = 400.f;
		constexpr float LabelH = 24.f;
		constexpr float BodyH = 32.f;
		constexpr float SonarH = 488.f;
		constexpr float HullH = 48.f;
	}

	FDeepHudRect HelmGauge(FVector2f Local)
	{
		return { Local.X - Margin - SideW, Margin, SideW, Local.Y - Margin * 2.f };
	}

	FDeepHudRect Sonar(FVector2f Local)
	{
		return { Margin, Local.Y - Margin - SonarH, SideW, SonarH };
	}

	FDeepHudRect HullSlot(FVector2f Local)
	{
		const FDeepHudRect S = Sonar(Local);
		return { S.X, S.Y - Grid - HullH, SideW, HullH };
	}

	FDeepHudRect ControlsHint(FVector2f Local, int32 Rows)
	{
		// The label, then rows of 44 with half a grid between (SDeepHud's hint). Width: the widest prompt row seen.
		return { Margin, Margin, SideW, LabelH + Grid + FMath::Max(0, Rows) * (44.f + Grid * 0.5f) };
	}

	FDeepHudRect SubtitleMax(FVector2f Local, int32 Lines)
	{
		const float W = FMath::Min(960.f, Local.X - 2.f * (Margin + 320.f + Grid * 3.f));
		const float H = Pad * 2.f + LabelH + Grid + FMath::Max(1, Lines) * BodyH;
		return { Local.X * 0.5f - W * 0.5f, Local.Y - Margin - H, W, H };
	}

	void SonarScope(const FDeepHudRect& Panel, FVector2f& Centre, float& Radius)
	{
		// Header row on top, footer row below, a grid of air round the rim for the ring labels.
		Radius = Panel.W * 0.5f - Pad - Grid * 2.f;
		Centre = FVector2f(Panel.X + Panel.W * 0.5f, Panel.Y + Pad + LabelH + Grid * 2.f + Radius);
	}
}
