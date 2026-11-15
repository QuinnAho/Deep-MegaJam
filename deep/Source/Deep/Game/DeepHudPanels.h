// One mode, one set of panels: the HUD picks every panel here, from the run state, and nowhere else.
// Pure (no world), so `Deep.Sim.HudPanels` tests it.

#pragma once

#include "CoreMinimal.h"

enum class EDeepHudMode : uint8 { Title, Aboard, Helm, Water, Dead, Settling, Won };

struct FDeepHudPanels
{
	bool bTitle = false;
	bool bStatus = false;     // the account box, top left
	bool bHelm = false;       // depth, gauge, speed, what to do at the helm
	bool bAboard = false;     // labels on hatch, console, helm and the one next step
	bool bConsole = false;    // the console's account panel (aboard, near or just used)
	bool bPatience = false;
	bool bBailout = false;
	bool bSwim = false;       // threat, prompts, harpoon, bite alarm
	bool bDead = false;
	bool bSettlement = false;
	bool bReview = false;
	bool bCaptain = false;
	float ControlsAlpha = 0.f;     // the key strip at the bottom: shown after a mode change, then faded
	float ControlsCardAlpha = 0.f; // the first dive's big controls card, same timing
};

struct FDeepHudInput
{
	EDeepHudMode Mode = EDeepHudMode::Title;
	bool bCut = false;              // in the water without a tether (bailout instead of Patience)
	bool bConsoleActive = false;    // near the console or used it a moment ago
	bool bFirstDive = false;        // the run's first dive: show the big controls card
	float ModeAgeSeconds = 0.f;     // how long this mode has been up
	float HintSeconds = 6.f;
	float FadeSeconds = 1.5f;
};

namespace DeepHud
{
	/** 1 for HintSeconds after the mode changed, then down to 0 over FadeSeconds. */
	inline float HintAlpha(float AgeSeconds, float HintSeconds, float FadeSeconds)
	{
		if (AgeSeconds <= HintSeconds)
		{
			return 1.f;
		}
		return FadeSeconds > 0.f ? FMath::Clamp(1.f - (AgeSeconds - HintSeconds) / FadeSeconds, 0.f, 1.f) : 0.f;
	}

	inline FDeepHudPanels Choose(const FDeepHudInput& In)
	{
		FDeepHudPanels P;
		const float Hint = HintAlpha(In.ModeAgeSeconds, In.HintSeconds, In.FadeSeconds);
		switch (In.Mode)
		{
		case EDeepHudMode::Title:
			P.bTitle = true;
			break;
		case EDeepHudMode::Aboard:
			P.bStatus = true;
			P.bAboard = true;
			P.bConsole = In.bConsoleActive;
			P.bCaptain = true;
			P.ControlsAlpha = Hint;
			break;
		case EDeepHudMode::Helm:
			P.bHelm = true;
			P.bCaptain = true;
			P.ControlsAlpha = Hint;
			break;
		case EDeepHudMode::Water:
			P.bStatus = true;
			P.bPatience = !In.bCut;
			P.bBailout = In.bCut;
			P.bSwim = true;
			P.bCaptain = true;
			P.ControlsAlpha = Hint;
			P.ControlsCardAlpha = In.bFirstDive ? Hint : 0.f;
			break;
		case EDeepHudMode::Dead:
			P.bDead = true;
			P.bCaptain = true;
			break;
		case EDeepHudMode::Settling:
			P.bSettlement = true;
			P.bCaptain = true;
			break;
		case EDeepHudMode::Won:
			P.bReview = true;
			break;
		}
		return P;
	}
}
