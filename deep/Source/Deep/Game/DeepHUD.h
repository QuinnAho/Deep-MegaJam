// HUD drawn on the canvas in code: status lines, the Captain's Patience bar or the bailout timer, the airlock
// marker when cut, prompts, the Captain's subtitle, and full-screen panels for death, settlement and descent.
// Also the run's frame: the title screen, the first dive's controls card, the pause menu and the win review.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DeepHUD.generated.h"

class ADeepDiver;
class UDeepRunComponent;
class UFont;

UCLASS()
class DEEP_API ADeepHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
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
	void DrawDescending(const UDeepRunComponent& Run);
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
};
