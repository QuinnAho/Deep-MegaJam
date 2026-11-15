// The HUD as one Slate leaf widget added to the player's viewport. It paints FDeepHudFrame in OnPaint with
// Slate draw elements, so text is rasterised by Slate's font cache at the real screen scale (crisp at any
// resolution), and all motion is code: per-panel fades, number tweens and cue timers advanced by ADeepHUD.
// Layout is in 1080p reference pixels; the viewport's DPI scaler maps them to the screen.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "UI/DeepHudFrame.h"
#include "UI/DeepHudModel.h"

class SDeepHud : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SDeepHud) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Called once per frame by ADeepHUD with real (unpaused) time. */
	void Advance(const FDeepHudFrame& InFrame, double RealNow, float DeltaSeconds);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1920.0, 1080.0); }

private:
	struct FPainter;

	FDeepHudFrame Frame;
	double Now = 0.0;

	DeepHudModel::FFade Fades[EDeepHudPanel::Count];
	TMap<FName, DeepHudModel::FFade> PromptFades;
	TMap<FName, FDeepHudPrompt> LastPrompts;
	DeepHudModel::FFade SubtitleFade;
	FString SubtitleShown;

	DeepHudModel::FTween Depth;
	DeepHudModel::FTween SubDepth;
	DeepHudModel::FTween CargoKg;
	DeepHudModel::FTween CargoValue;
	DeepHudModel::FTween Patience;
	DeepHudModel::FTween Tether;
	DeepHudModel::FTween Air;

	EDeepHudMode LastMode = EDeepHudMode::None;
	double ModeStart = 0.0;
	/** Modes whose controls hint has shown this run. */
	TSet<EDeepHudMode> HintShown;
	EDeepHudMode HintMode = EDeepHudMode::None;
	double HintStart = -1000.0;
	DeepHudModel::FFade HintFade;

	FDeepHudCue Cue;
	double CueStart = -1000.0;
};
