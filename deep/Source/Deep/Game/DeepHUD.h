// The HUD. By default a Slate widget (UI/SDeepHud) painted from one FDeepHudFrame gathered here each frame,
// with the panels chosen in one place, DeepHudModel::PanelsFor (see UI/). The old canvas HUD stays behind
// the console variable Deep.HUD.Legacy 1, off by default, for debugging.
//
// Legacy: HUD drawn on the canvas in code: status lines, the Captain's Patience bar or the bailout timer, the airlock
// marker when cut, prompts, the Captain's subtitle, and full-screen panels for death, settlement and descent.
// Also the run's frame: the title screen, the first dive's controls card, the pause menu and the win review.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DeepHUD.generated.h"

class ADeepDiver;
class UDeepRunComponent;
class UFont;
class SDeepHud;
class SWidget;
struct FDeepHudFrame;

UCLASS()
class DEEP_API ADeepHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** The old canvas HUD (Deep.HUD.Legacy 1). */
	void DrawLegacy();
	/** Fills the frame the widget paints, and raises feedback cues on the frame a state changes. */
	void GatherFrame(FDeepHudFrame& Frame);
	void ShowWidget(bool bShow);

	TSharedPtr<SDeepHud> Widget;
	TSharedPtr<SWidget> WidgetRoot;
	double LastRealTime = -1.0;

	// Last frame's state, for the cues.
	int32 PrevCargo = -1;
	bool bPrevWinching = false;
	uint8 PrevState = 255;
	uint8 PrevHarpoonPhase = 0;
	float PrevHarpoonSince = 1e6f;
	float PrevGateTime = -100.f;
	float PrevFeeTime = -100.f;
	bool bPrevParked = true;

	void TextAt(const FString& String, const FLinearColor& Color, float X, float Y, UFont* InFont = nullptr, float TextScale = 1.f);
	/** Draws centred horizontally; returns the text height. */
	float TextCentered(const FString& String, const FLinearColor& Color, float Y, UFont* InFont = nullptr, float TextScale = 1.f);

	void DrawStatus(const ADeepDiver* Diver, float Now);
	void DrawPatience(const UDeepRunComponent& Run, float Now);
	void DrawBailout(const ADeepDiver* Diver);
	void DrawPrompts(const ADeepDiver* Diver);
	void DrawCaptain();
	/** The serpent: a red edge pulse that quickens as it closes, plus a small proximity bar. */
	void DrawThreat(const UDeepRunComponent& Run, float RealNow);
	/** The serpent has the tether: full-width flashing banner and a countdown. */
	void DrawBiteAlarm(const UDeepRunComponent& Run, float RealNow);
	void DrawDead(const UDeepRunComponent& Run);
	void DrawSettlement(const UDeepRunComponent& Run);
	/** Aboard and standing: labels on the hatch, console and helm, the one next step, the console's account panel. */
	void DrawAboard(const ADeepDiver* Diver);
	/** Seated at the helm: depth, a gauge with the stops and the limit, why the limit is there, what to do. */
	void DrawHelm(const ADeepDiver* Diver);
	/** A label over a world point if it is on screen. */
	bool DrawMarker(const FVector& World, const FString& Label, const FLinearColor& Color);
	/** Where home is: a box on the airlock, or an edge hint when it is off screen. */
	void DrawAirlockMarker(const ADeepDiver* Diver, float Y);
	void DrawTitle(float RealNow);
	void DrawControlsCard(float Alpha);
	void DrawPauseMenu();
	void DrawReview(const UDeepRunComponent& Run);
	/** A vertical list of menu items, the selected one marked. Returns the y below it. */
	float DrawMenu(const TArray<FString>& Items, int32 Selected, float Y, bool bEnabled = true);

	UPROPERTY(Transient)
	TObjectPtr<UFont> Font;

	UPROPERTY(Transient)
	TObjectPtr<UFont> BigFont;

	float S = 1.f;
	/** World time the current tethered dive began (the first dive's controls card). */
	float DiveStartTime = -1.f;
	bool bWasTethered = false;
};
