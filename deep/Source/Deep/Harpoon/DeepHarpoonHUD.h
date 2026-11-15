// The harpoon's HUD: ready/spent indicator (bottom right), crosshair brackets when something would be hit in range,
// and a short message after a latch, miss, serpent hit or refusal. ADeepHUD calls Draw once per frame.

#pragma once

#include "CoreMinimal.h"

class AHUD;
class ADeepDiver;
class UCanvas;
class UFont;

namespace DeepHarpoonHUD
{
	/** S is the HUD's text scale. Draws nothing without a harpoon. */
	DEEP_API void Draw(AHUD* Hud, UCanvas* Canvas, const ADeepDiver* Diver, float S, UFont* Font, UFont* BigFont);
}
