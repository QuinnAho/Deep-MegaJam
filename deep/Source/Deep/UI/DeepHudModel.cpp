#include "UI/DeepHudModel.h"

namespace DeepHudModel
{
	EDeepHudMode ChooseMode(const FDeepHudModeInputs& In)
	{
		if (In.bOnTitle)
		{
			return EDeepHudMode::Title;
		}
		if (!In.bHasDiver)
		{
			return EDeepHudMode::None;
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
			return HelmGauge | Subtitle | Feedback | ControlsHint;
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
