// HUD drawn on the canvas in code: status lines, the Captain's Patience bar or the bailout timer, the airlock
// marker when cut, prompts, the Captain's subtitle, and full-screen panels for death, settlement and descent.

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
	void DrawDead(const UDeepRunComponent& Run);
	void DrawSettlement(const UDeepRunComponent& Run);
	void DrawDescending(const UDeepRunComponent& Run);

	UPROPERTY(Transient)
	TObjectPtr<UFont> Font;

	UPROPERTY(Transient)
	TObjectPtr<UFont> BigFont;

	float S = 1.f;
};
