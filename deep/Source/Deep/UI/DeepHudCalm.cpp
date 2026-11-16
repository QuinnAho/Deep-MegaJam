#include "UI/DeepHudCalm.h"

#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarDeepHudHelmSonar(
	TEXT("Deep.Hud.HelmSonar"),
	0,
	TEXT("0 hides the helm sonar panel (default, Quinn: sonar off; the diver's ping on V stays), 1 shows it."),
	ECVF_Default);

namespace DeepHudCalm
{
	namespace
	{
		FDeepHudPrompt Row(const TCHAR* Id, const TCHAR* Key, const TCHAR* Verb, const TCHAR* Detail = TEXT(""))
		{
			FDeepHudPrompt P;
			P.Id = FName(Id);
			P.Key = Key;
			P.Verb = Verb;
			P.Detail = Detail;
			return P;
		}

		bool IsPlay(EDeepHudMode Mode)
		{
			return Mode == EDeepHudMode::Aboard || Mode == EDeepHudMode::Helm || Mode == EDeepHudMode::Water;
		}

		FDeepHudCue MakeCue(const TCHAR* Label, EDeepHudTone Tone)
		{
			FDeepHudCue C;
			C.Label = Label;
			C.Tone = Tone;
			return C;
		}
	}

	TArray<FDeepHudPrompt> HelmHint()
	{
		return {
			Row(TEXT("hh1"), TEXT("W / S"), TEXT("THROTTLE"), TEXT("in notches")),
			Row(TEXT("hh2"), TEXT("A / D"), TEXT("RUDDER")),
			Row(TEXT("hh3"), TEXT("C / SPACE"), TEXT("BALLAST"), TEXT("C floods, SPACE blows; it stays")),
			Row(TEXT("hh4"), TEXT("X"), TEXT("ALL STOP"), TEXT("throttle and ballast")),
			Row(TEXT("hh5"), TEXT("E"), TEXT("STAND UP")),
		};
	}

	FString ThrottleLabel(int32 Notch)
	{
		switch (FMath::Clamp(Notch, -2, 3))
		{
		case -2: return TEXT("FULL ASTERN");
		case -1: return TEXT("HALF ASTERN");
		case 1: return TEXT("SLOW");
		case 2: return TEXT("HALF");
		case 3: return TEXT("FULL AHEAD");
		default: return TEXT("STOP");
		}
	}

	FString BallastLabel(float Lever)
	{
		const float L = FMath::Clamp(Lever, -1.f, 1.f);
		const int32 Share = FMath::RoundToInt(FMath::Abs(L) * 100.f);
		if (Share == 0)
		{
			return TEXT("NEUTRAL");
		}
		if (Share >= 100)
		{
			return L < 0.f ? TEXT("BLOWN") : TEXT("FLOODED");
		}
		return FString::Printf(TEXT("%s %d%%"), L < 0.f ? TEXT("BLOW") : TEXT("FLOOD"), Share);
	}

	FString HeadingLabel(float YawDeg)
	{
		const int32 Deg = FMath::RoundToInt(FRotator::ClampAxis(YawDeg)) % 360;
		return FString::Printf(TEXT("%03d%s"), Deg, TEXT("\u00B0"));
	}

	bool HelmSonarOn()
	{
		return CVarDeepHudHelmSonar.GetValueOnGameThread() != 0;
	}

	FDeepHudPrompt UnclipHint()
	{
		return Row(TEXT("h8"), TEXT("T"), TEXT("UNCLIP"), TEXT("bailout air; re-clip at the hatch"));
	}

	bool IsCriticalCue(const FDeepHudCue& Cue)
	{
		if (Cue.Tone != EDeepHudTone::Warning && Cue.Tone != EDeepHudTone::Danger)
		{
			return false;
		}
		static const TCHAR* const Critical[] = {
			TEXT("tether cut"), TEXT("line cut"), TEXT("tether bitten"), TEXT("hull"), TEXT("air low"), TEXT("bite"), TEXT("bitten"),
		};
		for (const TCHAR* Prefix : Critical)
		{
			if (Cue.Label.StartsWith(Prefix, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	int32 PickPrompt(const TArray<FDeepHudPrompt>& Prompts, FVector2f ViewportPx)
	{
		const FVector2f Centre = ViewportPx * 0.5f;
		int32 Best = INDEX_NONE;
		float BestScore = TNumericLimits<float>::Max();
		for (int32 Index = 0; Index < Prompts.Num(); ++Index)
		{
			const FDeepHudPrompt& P = Prompts[Index];
			// In reach = it has a key to press now. Far tags (HELM, BANK CARGO, HATCH LOCKED) and advice rows have none.
			if (P.Key.IsEmpty())
			{
				continue;
			}
			// Anchored: nearest the centre of the view. Under the reticle: the gatherer's order is the priority.
			const float Score = P.bAnchored ? FVector2f::Distance(P.ScreenPx, Centre) : -1.e6f + Index;
			if (Score < BestScore)
			{
				BestScore = Score;
				Best = Index;
			}
		}
		return Best;
	}

	float NextStopM(const TArray<float>& Stops, float SubDepthM, int32 ParkedStop)
	{
		if (ParkedStop > 0)
		{
			return Stops.IsValidIndex(ParkedStop) ? Stops[ParkedStop] : -1.f;
		}
		for (const float Stop : Stops)
		{
			if (Stop > SubDepthM + 0.5f)
			{
				return Stop;
			}
		}
		return -1.f;
	}

	void FShowOnChange::Step(float Value, double Now, float Epsilon)
	{
		if (!bInit)
		{
			Last = Value;
			bInit = true;
			return;
		}
		if (FMath::Abs(Value - Last) > Epsilon)
		{
			ChangedAt = Now;
		}
		Last = Value;
	}

	bool IsOverlay(EDeepHudMode Mode)
	{
		return Mode == EDeepHudMode::Pause || Mode == EDeepHudMode::Settings || Mode == EDeepHudMode::Credits || Mode == EDeepHudMode::ConfirmQuit;
	}

	void FHintTracker::Reset()
	{
		*this = FHintTracker();
	}

	bool FHintTracker::Step(const FDeepHudFrame& Frame, const FDeepHudCalmTuning& Tuning, double Now)
	{
		if (IsOverlay(Frame.Mode))
		{
			return false;
		}
		if (Frame.Mode != Mode)
		{
			// Leaving a mode takes its controls (E to stand up or use, the hatch, the winch home): its hint is done.
			if (IsPlay(Mode))
			{
				Done.Add(Mode);
			}
			Mode = Frame.Mode;
			StartDepthM = Frame.DepthM;
			StartTetherLeftM = Frame.TetherLeftM;
			StartSubDepthM = Frame.SubDepthM;
			StartParkedStop = Frame.ParkedStop;
			StartTime = Now;
		}
		if (!IsPlay(Mode) || Frame.Hint.Num() == 0 || Done.Contains(Mode))
		{
			return false;
		}
		bool bUsed = false;
		if (Mode == EDeepHudMode::Helm)
		{
			bUsed = FMath::Abs(Frame.SubDepthM - StartSubDepthM) >= Tuning.HelmUseM || Frame.ParkedStop != StartParkedStop;
		}
		else if (Mode == EDeepHudMode::Water)
		{
			bUsed = Frame.bHintDone || Frame.CargoKg > 0.f || FMath::Abs(Frame.DepthM - StartDepthM) >= Tuning.WaterUseM
				|| (Frame.bTether && FMath::Abs(Frame.TetherLeftM - StartTetherLeftM) >= Tuning.WaterUseM);
		}
		if (bUsed && Now - StartTime >= Tuning.HintMinSeconds)
		{
			Done.Add(Mode);
			return false;
		}
		return true;
	}
}

void FDeepHudCalm::Apply(FDeepHudFrame& Frame, double Now)
{
	using namespace EDeepHudPanel;
	if (Frame.Mode == EDeepHudMode::Title)
	{
		// A new run: every hint comes back once.
		const FDeepHudCalmTuning Keep = Tuning;
		*this = FDeepHudCalm();
		Tuning = Keep;
	}

	// ---- the controls hint: until used, then gone for the run; the pause card keeps the full list
	if (Frame.Mode == EDeepHudMode::Helm)
	{
		Frame.Hint = DeepHudCalm::HelmHint();
		if (!Tuning.bHelmSonar)
		{
			Frame.Panels &= ~static_cast<uint32>(EDeepHudPanel::Sonar);
		}
	}
	if (Frame.Mode == EDeepHudMode::Water)
	{
		// T UNCLIP leaves the line for free; the Q hold to be cut loose still works but is no longer taught.
		Frame.Hint.RemoveAll([](const FDeepHudPrompt& P) { return P.Id == FName(TEXT("h6")) || P.Verb == TEXT("CUT LOOSE"); });
	}
	if (Frame.Mode == EDeepHudMode::Water && Frame.Hint.Num() > 0
		&& !Frame.Hint.ContainsByPredicate([](const FDeepHudPrompt& P) { return P.Verb == TEXT("UNCLIP"); }))
	{
		// The air line (lane D): T unclips, the diver goes on bailout air and re-clips free at the hatch.
		Frame.Hint.Add(DeepHudCalm::UnclipHint());
	}
	if ((Frame.Mode == EDeepHudMode::Aboard || Frame.Mode == EDeepHudMode::Helm || Frame.Mode == EDeepHudMode::Water) && Frame.Hint.Num() > 0)
	{
		LastPlayHint = Frame.Hint;
	}
	if (!Hint.Step(Frame, Tuning, Now))
	{
		Frame.Panels &= ~static_cast<uint32>(ControlsHint);
	}
	if (Frame.Mode == EDeepHudMode::Pause)
	{
		Frame.Hint = LastPlayHint;
	}

	// ---- one prompt, the nearest usable thing in reach
	const int32 Pick = DeepHudCalm::PickPrompt(Frame.Prompts, Frame.ViewportPx);
	if (Pick == INDEX_NONE)
	{
		Frame.Prompts.Reset();
	}
	else
	{
		const FDeepHudPrompt Keep = Frame.Prompts[Pick];
		Frame.Prompts = { Keep };
	}
	// The warning line above the reticle: danger only (the end of the line).
	if (Frame.WarningTone != EDeepHudTone::Danger)
	{
		Frame.Warning.Reset();
	}

	// ---- feedback: critical cues only
	TArray<FDeepHudCue> Cues;
	for (const FDeepHudCue& Cue : Frame.Cues)
	{
		if (DeepHudCalm::IsCriticalCue(Cue))
		{
			Cues.Add(Cue);
		}
	}

	// ---- the hull: a few seconds after it changes, and always when low; a hit raises its own cue
	if (Frame.Hull01 >= 0.f)
	{
		Hull.Step(Frame.Hull01, Now, 0.001f);
		if (LastHull01 >= 0.f && Frame.Hull01 < LastHull01 - 0.001f)
		{
			if (Frame.Hull01 <= KINDA_SMALL_NUMBER)
			{
				Cues.Add(DeepHudCalm::MakeCue(TEXT("Hull breach"), EDeepHudTone::Danger));
			}
			else if (Frame.Hull01 < Tuning.HullCritical01 && LastHull01 >= Tuning.HullCritical01)
			{
				Cues.Add(DeepHudCalm::MakeCue(TEXT("Hull critical"), EDeepHudTone::Danger));
			}
			else
			{
				Cues.Add(DeepHudCalm::MakeCue(TEXT("Hull hit"), EDeepHudTone::Warning));
			}
		}
		LastHull01 = Frame.Hull01;
	}
	if (Frame.Hull01 < 0.f || !(Hull.Showing(Now, Tuning.HullShowSeconds) || Frame.Hull01 < Tuning.HullStayBelow01))
	{
		Frame.Panels &= ~static_cast<uint32>(EDeepHudPanel::Hull);
	}

	// ---- cargo: when the load changes, and while it is too heavy to swim up
	Cargo.Step(Frame.CargoKg, Now, 0.05f);
	if (!(Cargo.Showing(Now, Tuning.CargoShowSeconds) || Frame.bOverloaded))
	{
		Frame.Panels &= ~static_cast<uint32>(EDeepHudPanel::Cargo);
	}

	// ---- bailout air: one AIR LOW as it runs short
	if (Frame.Mode == EDeepHudMode::Cut)
	{
		if (!bAirLowSaid && Frame.AirMax > 0.f && Frame.AirSeconds / Frame.AirMax < Tuning.AirLow01)
		{
			Cues.Add(DeepHudCalm::MakeCue(TEXT("Air low"), EDeepHudTone::Danger));
			bAirLowSaid = true;
		}
	}
	else if (!DeepHudCalm::IsOverlay(Frame.Mode))
	{
		bAirLowSaid = false;
	}
	Frame.Cues = MoveTemp(Cues);
}
