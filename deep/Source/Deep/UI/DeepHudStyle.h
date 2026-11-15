// The HUD's style tokens: every colour, size and time the HUD uses, copied from
// deep-claude-brain/02 - Design/Style Guide.md (sections 3 and 8) and the HUD spec, direction A "Instrument".
// Nothing else may pick a colour: SDeepHud, the legacy canvas HUD (ADeepHUD::DrawLegacy) and the harpoon's canvas
// overlay all take theirs from the roles below. A new palette is an edit to the hex values here and nowhere else.

#pragma once

#include "CoreMinimal.h"

namespace DeepHudStyle
{
	// ---------------------------------------------------------------- the palette (sRGB hex), by role
	// Tonal water (the recommended palette on the HUD mockup page, Nov 15). Each role is its own constant, so a palette
	// change can move one without the other even where two share a value today (Warn and MachineAmber).
	// FColor::FromHex keeps them exactly as written.
	inline FLinearColor Hex(const TCHAR* InHex, float Alpha = 1.f)
	{
		FLinearColor Color = FLinearColor(FColor::FromHex(InHex));
		Color.A = Alpha;
		return Color;
	}
	inline const TCHAR* PrimaryHex() { return TEXT("A9C5D0"); }      // numbers, hairlines, what you came for
	inline const TCHAR* SecondaryHex() { return TEXT("7F9AA4"); }    // body text, labels, tracks, ticks, rings, the reticle
	inline const TCHAR* WarnHex() { return TEXT("FFB45A"); }         // warnings: patience at half or below, TOO HEAVY, fees
	inline const TCHAR* AlertHex() { return TEXT("EC5650"); }        // danger only: cut, bite, threat edge, limit, breach
	inline const TCHAR* MachineAmberHex() { return TEXT("FFB45A"); } // the machine's voice: sonar, helm header, SUB, the Captain
	inline const TCHAR* PanelHex() { return TEXT("04121A"); }        // panel fill and the card dim
	inline const TCHAR* ShadeHex() { return TEXT("000000"); }        // the dark edge under marks with no panel, the title veil
	/** Secondary text is drawn at this share of full opacity. */
	constexpr float SecondaryAlpha = 1.f;
	/** Panel fill opacity. */
	constexpr float PanelAlpha = 0.6f;

	inline FLinearColor Primary(float A = 1.f) { return Hex(PrimaryHex(), A); }
	inline FLinearColor Secondary(float A = 1.f) { return Hex(SecondaryHex(), SecondaryAlpha * A); }
	inline FLinearColor Warn(float A = 1.f) { return Hex(WarnHex(), A); }
	/** Danger. */
	inline FLinearColor Alert(float A = 1.f) { return Hex(AlertHex(), A); }
	inline FLinearColor MachineAmber(float A = 1.f) { return Hex(MachineAmberHex(), A); }
	/** The one panel fill, also the card dim. A is the panel's fade (1 = PanelAlpha). */
	inline FLinearColor PanelFill(float A = 1.f) { return Hex(PanelHex(), PanelAlpha * A); }
	/** The shade colour at an exact opacity (the title veil, outlines, the legacy HUD's full-screen washes). */
	inline FLinearColor Shade(float A = 1.f) { return Hex(ShadeHex(), A); }
	/** Slate's brush base: untinted, so every draw's colour is exactly the role it passes. Not a HUD colour. */
	inline FLinearColor BrushBase() { return Hex(TEXT("FFFFFF")); }

	// Opacities that recur, as shares of a role (so a palette change keeps the hierarchy).
	constexpr float HairlineAlpha = 0.6f;   // panel hairline in primary
	constexpr float TrackAlpha = 0.18f;     // a bar's empty track in secondary
	constexpr float TickAlpha = 0.45f;      // bar ticks in secondary
	constexpr float GhostAlpha = 0.35f;     // the hull bar's just-lost segment in secondary
	constexpr float DimAlpha = 0.6f;        // dim rows (fees) in secondary
	constexpr float UnitAlpha = 0.7f;       // "m", "kg", "cr" after a number
	constexpr float FarAlpha = 0.45f;       // the harpoon aim out of range, or spent
	constexpr float ReticleAlpha = 0.9f;    // the reticle dot in secondary
	/** Marks with no panel behind them (reticle, aim brackets, hatch marker, their labels) get a 1 px dark edge in
	 *  Shade at this opacity, so they keep their contrast over the lamp's glow. */
	constexpr float OutlineAlpha = 0.6f;
	/** The threat's red edge (Alert) at the screen border: this opacity at the least threat, ThreatEdgeMax at the most. */
	constexpr float ThreatEdgeMin = 0.03f;
	constexpr float ThreatEdgeMax = 0.11f;

	// Bars (patience, hull): primary above WarnAt, amber at WarnAt or below, alert at DangerAt or below.
	constexpr float BarWarnAt = 0.5f;
	constexpr float BarDangerAt = 0.25f;

	// ---------------------------------------------------------------- type and layout
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

	// Direction A panel sizes (HUD spec, at 1080p).
	constexpr float WaterPanelW = 280.f;  // depth (bottom left) and cargo (bottom right)
	constexpr float WaterPanelH = 128.f;
	constexpr float PatienceW = 480.f;
	constexpr float PatienceH = 44.f;
	constexpr float StrainLineY = 122.f;  // the tether strain line inside the depth panel
	constexpr float StrainShowAt = 0.5f;  // the strain line shows only above this strain
	constexpr float StrainAlertAt = 0.75f;
	constexpr float HullBarPx = 8.f;
	constexpr float BarTickPx = 12.f;

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
	constexpr float GhostFade = 0.600f;   // the hull bar's lost segment
	constexpr float HitFlash = 0.120f;    // the hull panel's hairline turns alert on a hit

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
