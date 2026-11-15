// The HUD's style tokens: every colour, size and time the HUD uses, copied from
// deep-claude-brain/02 - Design/Style Guide.md (sections 3 and 8). Nothing in UI/ may pick its own.

#pragma once

#include "CoreMinimal.h"

namespace DeepHudStyle
{
	// Section 3, sRGB hex. FColor::FromHex keeps them exactly as written in the guide.
	inline FLinearColor Hex(const TCHAR* InHex, float Alpha = 1.f)
	{
		FLinearColor Color = FLinearColor(FColor::FromHex(InHex));
		Color.A = Alpha;
		return Color;
	}
	inline FLinearColor Primary(float A = 1.f) { return Hex(TEXT("58E1D6"), A); }          // numbers, hairlines
	inline FLinearColor Secondary(float A = 1.f) { return Hex(TEXT("E0E6E6"), 0.88f * A); }  // body text, labels
	inline FLinearColor Alert(float A = 1.f) { return Hex(TEXT("E5484D"), A); }             // danger only: cut, bite, threat edge
	inline FLinearColor Amber(float A = 1.f) { return Hex(TEXT("FFB45A"), A); }             // the Captain's name tag, warnings, helm screen
	inline FLinearColor PanelFill(float A = 1.f) { return FLinearColor(0.f, 0.f, 0.f, 0.55f * A); }

	// Section 8 sizes at the 1080p reference, in pixels. The viewport's DPI curve (shortest side, 1.0 at 1080)
	// scales them for other resolutions. Slate font sizes are points at 96 DPI, so px * 0.75 = pt.
	constexpr float LabelPx = 18.f;
	constexpr float BodyPx = 24.f;
	constexpr float ReadoutPx = 36.f;
	constexpr float DepthPx = 72.f;
	constexpr float PxToPt = 0.75f;
	constexpr int32 LabelLetterSpacing = 120; // 1/1000 em, as the guide writes it

	// Panels: 1 px hairline, 8 px grid, 16 px padding, square corners, no shadow.
	constexpr float Hairline = 1.f;
	constexpr float Grid = 8.f;
	constexpr float Pad = 16.f;
	constexpr float Margin = 48.f; // six grid steps from the screen edge (chosen here, on the grid)

	// The title's veil over the room (opening pass, Nov 15, chosen here): near black behind the word, thinner at the
	// foot of the screen, so the first frame shows where you are (the hatch's red, the console's amber) and not a void.
	constexpr float TitleVeilTop = 0.95f;
	constexpr float TitleVeilBottom = 0.62f;

	// Motion, ease-out cubic, no bounce. Seconds.
	constexpr float PanelOpen = 0.200f;
	constexpr float PanelClose = 0.120f;
	constexpr float Slide = 0.200f;
	constexpr float PromptFade = 0.150f;
	constexpr float BarLerp = 0.250f;
	constexpr float NumberTween = 0.450f;
	constexpr float SceneFade = 0.600f;
	constexpr float SlideDistance = Grid * 2.f;

	// The helm sonar (Sonar lane, Nov 15): an amber phosphor scope like the room's helm screens. Walls at the sub's
	// depth are full amber, the slice below is the same amber at a third, so the two read as one instrument.
	// Salvage is HUD primary (what you came for), the serpent Alert (danger only). No new hue.
	constexpr float SonarLowerAlpha = 0.38f;
	constexpr float SonarRingAlpha = 0.16f;
	constexpr float SonarRimAlpha = 0.45f;
	constexpr float SonarWallPx = 2.f;
	constexpr float SonarLowerPx = 1.5f;
	constexpr float SonarBlipPx = 10.f;
	constexpr int32 SonarTrailSteps = 10;
	constexpr float SonarTrailDeg = 36.f;
}
