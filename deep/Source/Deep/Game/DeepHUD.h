// Plain HUD drawn on the canvas in code: depth, cargo, tether left, and context prompts.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DeepHUD.generated.h"

UCLASS()
class DEEP_API ADeepHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
