// The calm HUD (rescope, Nov 15, Quinn: "the UI is still too cluttered and really takes away from the immersion").
// The player reads the world, the sonar and the sub's levers; text shows only while it is needed, then goes.
// FDeepHudCalm takes the frame ADeepHUD gathered and thins it before SDeepHud paints it:
//   - the controls hint for a mode shows until those controls have been used once, then never again this run;
//   - one prompt at a time, the nearest usable thing in reach;
//   - feedback cues only when critical (hull hit or critical, the line cut or bitten, air low, a bite);
//   - the hull shows for a few seconds when it changes and stays below a threshold; cargo shows when it changes.
// Pure: no engine objects, so Deep.UI.HudCalm.* drives it with plain frames.

#pragma once

#include "CoreMinimal.h"
#include "UI/DeepHudFrame.h"

/** The calm HUD's numbers. Seconds and 0..1 fractions. */
struct FDeepHudCalmTuning
{
	/** The hull bar shows this long after its value changes. */
	float HullShowSeconds = 3.f;
	/** Below this hull fraction the bar stays up. */
	float HullStayBelow01 = 0.3f;
	/** Below this the hull is critical (the style's danger band, the shaft's hull_critical). */
	float HullCritical01 = 0.25f;
	/** The cargo panel shows this long after the load changes. */
	float CargoShowSeconds = 3.f;
	/** Bailout air below this share of the full tank raises AIR LOW once. */
	float AirLow01 = 0.3f;
	/** The calm fades: the hint, the hull and the cargo ease in and out over this. */
	float FadeSeconds = 0.4f;
	/** The water hint counts as used once the diver has moved this far (depth or line paid out). */
	float WaterUseM = 3.f;
	/** The helm hint counts as used once the sub's depth has changed this much. */
	float HelmUseM = 1.f;
	/** A hint stays at least this long, so it can be read even when the controls are in use at once. */
	float HintMinSeconds = 2.5f;
};

namespace DeepHudCalm
{
	/** The helm's controls (the helm lane's levers): W/S throttle, A/D rudder, SPACE/C ballast, X all stop, E stand up. */
	DEEP_API TArray<FDeepHudPrompt> HelmHint();

	/** The air line's row for the water hint: T unclips (bailout air, re-clip free at the hatch). */
	DEEP_API FDeepHudPrompt UnclipHint();

	/** A cue worth the screen: Warning or Danger, and about the hull, the line, the air or a bite. */
	DEEP_API bool IsCriticalCue(const FDeepHudCue& Cue);

	/** The one prompt to show: in reach (it has a key), nearest the screen centre if anchored, else the first. INDEX_NONE for none. */
	DEEP_API int32 PickPrompt(const TArray<FDeepHudPrompt>& Prompts, FVector2f ViewportPx);

	/** The next stop below the sub (the one after the parked stop when parked); negative when there is none. */
	DEEP_API float NextStopM(const TArray<float>& Stops, float SubDepthM, int32 ParkedStop);

	/** A value that shows for a while after it changes. The first value seen is not a change. */
	struct DEEP_API FShowOnChange
	{
		float Last = 0.f;
		bool bInit = false;
		double ChangedAt = -1000.0;
		void Step(float Value, double Now, float Epsilon);
		bool Showing(double Now, float HoldSeconds) const { return Now - ChangedAt < HoldSeconds; }
	};

	/** Whether a mode's controls hint shows: until the controls have been used once, then never again this run. */
	struct DEEP_API FHintTracker
	{
		/** Modes whose hint is done this run. */
		TSet<EDeepHudMode> Done;
		/** The play mode being watched, and its values when it began. */
		EDeepHudMode Mode = EDeepHudMode::None;
		float StartDepthM = 0.f;
		float StartTetherLeftM = 0.f;
		float StartSubDepthM = 0.f;
		int32 StartParkedStop = 0;
		double StartTime = 0.0;

		void Reset();
		/** True while the hint for this frame's mode should show. */
		bool Step(const FDeepHudFrame& Frame, const FDeepHudCalmTuning& Tuning, double Now);
	};

	/** True for a menu over the run (pause and its screens): the run's hints wait under it. */
	DEEP_API bool IsOverlay(EDeepHudMode Mode);
}

/** The calm layer's state for one player's HUD. Apply once per frame, before painting. */
struct DEEP_API FDeepHudCalm
{
	FDeepHudCalmTuning Tuning;
	DeepHudCalm::FHintTracker Hint;
	DeepHudCalm::FShowOnChange Hull;
	DeepHudCalm::FShowOnChange Cargo;
	/** The last hull seen (negative = none yet), for the hit and critical cues. */
	float LastHull01 = -1.f;
	bool bAirLowSaid = false;
	/** The last play mode's full controls list, for the pause card. */
	TArray<FDeepHudPrompt> LastPlayHint;

	/** Thins the frame: panels, prompts, cues, the warning line and the hint rows. */
	void Apply(FDeepHudFrame& Frame, double Now);
};
