#include "UI/SDeepHud.h"

#include "Brushes/SlateColorBrush.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "UI/DeepHudStyle.h"

namespace
{
	using namespace DeepHudStyle;
	using DeepHudModel::EaseOutCubic;

	const FSlateColorBrush& White()
	{
		static const FSlateColorBrush Brush(FLinearColor::White);
		return Brush;
	}

	enum class EFace : uint8 { Label, Body, Readout, Depth, Mono, Heading };

	FSlateFontInfo Font(EFace Face, float Px = 0.f)
	{
		switch (Face)
		{
		case EFace::Label:
		{
			FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(TEXT("BoldCondensed"), (Px > 0.f ? Px : LabelPx) * PxToPt);
			Info.LetterSpacing = LabelLetterSpacing;
			return Info;
		}
		case EFace::Heading:
		{
			FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(TEXT("BoldCondensed"), ReadoutPx * PxToPt);
			Info.LetterSpacing = LabelLetterSpacing;
			return Info;
		}
		case EFace::Body: return FCoreStyle::GetDefaultFontStyle(TEXT("Light"), BodyPx * PxToPt);
		case EFace::Readout: return FCoreStyle::GetDefaultFontStyle(TEXT("Light"), ReadoutPx * PxToPt);
		case EFace::Depth: return FCoreStyle::GetDefaultFontStyle(TEXT("Light"), DepthPx * PxToPt);
		case EFace::Mono: return FCoreStyle::GetDefaultFontStyle(TEXT("Mono"), (Px > 0.f ? Px : LabelPx) * PxToPt);
		}
		return FCoreStyle::GetDefaultFontStyle(TEXT("Light"), BodyPx * PxToPt);
	}

	FLinearColor ToneColor(EDeepHudTone Tone, float A)
	{
		switch (Tone)
		{
		case EDeepHudTone::Primary: return Primary(A);
		case EDeepHudTone::Warning: return Amber(A);
		case EDeepHudTone::Danger: return Alert(A);
		case EDeepHudTone::Dim: return Secondary(0.55f * A);
		default: return Secondary(A);
		}
	}
}

struct SDeepHud::FPainter
{
	const FGeometry& Geo;
	FSlateWindowElementList& Out;
	int32 Layer;
	FVector2f Size;
	/** Viewport pixels -> local units. */
	float PxToLocal = 1.f;
	/** The guide's 1 px hairline: one physical pixel at any resolution (1080p units would vanish below 1080p). */
	float Hair = 1.f;
	TSharedRef<FSlateFontMeasure> Measure;

	FPainter(const FGeometry& InGeo, FSlateWindowElementList& InOut, int32 InLayer)
		: Geo(InGeo), Out(InOut), Layer(InLayer), Size(InGeo.GetLocalSize()),
		  Measure(FSlateApplication::Get().GetRenderer()->GetFontMeasureService())
	{
	}

	void Box(float X, float Y, float W, float H, const FLinearColor& Color) const
	{
		if (Color.A <= 0.001f || W <= 0.f || H <= 0.f)
		{
			return;
		}
		FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X, Y))), &White(), ESlateDrawEffect::None, Color);
	}

	/** Hair outline, 1 px, square corners. */
	void Outline(float X, float Y, float W, float H, const FLinearColor& Color) const
	{
		Box(X, Y, W, Hair, Color);
		Box(X, Y + H - Hair, W, Hair, Color);
		Box(X, Y, Hair, H, Color);
		Box(X + W - Hair, Y, Hair, H, Color);
	}

	/** The one panel: black 55 percent, 1 px hairline in HUD primary. */
	void Panel(float X, float Y, float W, float H, float A) const
	{
		Box(X, Y, W, H, PanelFill(A));
		Outline(X, Y, W, H, Primary(0.6f * A));
	}

	FVector2f TextSize(const FString& Text, const FSlateFontInfo& Info) const
	{
		return FVector2f(Measure->Measure(Text, Info));
	}

	float Text(const FString& Text, float X, float Y, const FSlateFontInfo& Info, const FLinearColor& Color) const
	{
		const FVector2f S = TextSize(Text, Info);
		if (Color.A > 0.001f && !Text.IsEmpty())
		{
			FSlateDrawElement::MakeText(Out, Layer + 1, Geo.ToPaintGeometry(S, FSlateLayoutTransform(FVector2f(X, Y))), Text, Info, ESlateDrawEffect::None, Color);
		}
		return S.X;
	}

	float TextRight(const FString& String, float Right, float Y, const FSlateFontInfo& Info, const FLinearColor& Color) const
	{
		const float W = TextSize(String, Info).X;
		Text(String, Right - W, Y, Info, Color);
		return W;
	}

	float TextCentre(const FString& String, float CX, float Y, const FSlateFontInfo& Info, const FLinearColor& Color) const
	{
		const float W = TextSize(String, Info).X;
		Text(String, CX - W * 0.5f, Y, Info, Color);
		return W;
	}

	void Lines(const TArray<FVector2f>& Points, const FLinearColor& Color, float Thickness = 1.f) const
	{
		if (Color.A <= 0.001f || Points.Num() < 2)
		{
			return;
		}
		FSlateDrawElement::MakeLines(Out, Layer + 1, Geo.ToPaintGeometry(), Points, ESlateDrawEffect::None, Color, true, Thickness);
	}

	/** An arc around a centre: from 12 o'clock clockwise, Fraction of a turn. */
	void Arc(FVector2f C, float R, float Fraction, const FLinearColor& Color, float Thickness) const
	{
		const int32 Segments = FMath::Max(2, FMath::CeilToInt(48.f * FMath::Clamp(Fraction, 0.f, 1.f)));
		TArray<FVector2f> P;
		for (int32 I = 0; I <= Segments; ++I)
		{
			const float Angle = -HALF_PI + 2.f * PI * Fraction * I / Segments;
			P.Add(C + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * R);
		}
		Lines(P, Color, Thickness);
	}

	/** Four corner brackets around a centre. */
	void Brackets(FVector2f C, float Half, float Arm, const FLinearColor& Color) const
	{
		for (const float SX : { -1.f, 1.f })
		{
			for (const float SY : { -1.f, 1.f })
			{
				const FVector2f Corner = C + FVector2f(SX * Half, SY * Half);
				Lines({ Corner - FVector2f(SX * Arm, 0.f), Corner, Corner - FVector2f(0.f, SY * Arm) }, Color, 1.f);
			}
		}
	}

	/** Word-wrap to a width. */
	TArray<FString> Wrap(const FString& Line, const FSlateFontInfo& Info, float MaxW) const
	{
		TArray<FString> Words;
		Line.ParseIntoArray(Words, TEXT(" "));
		TArray<FString> Result;
		FString Current;
		for (const FString& Word : Words)
		{
			const FString Try = Current.IsEmpty() ? Word : Current + TEXT(" ") + Word;
			if (!Current.IsEmpty() && TextSize(Try, Info).X > MaxW)
			{
				Result.Add(Current);
				Current = Word;
			}
			else
			{
				Current = Try;
			}
		}
		if (!Current.IsEmpty())
		{
			Result.Add(Current);
		}
		return Result;
	}

	/** A key cap: the key in a hairline box. Returns its width. */
	float KeyCap(const FString& Key, float X, float Y, float A) const
	{
		const FSlateFontInfo Info = Font(EFace::Label);
		const FVector2f S = TextSize(Key, Info);
		const float W = FMath::Max(28.f, S.X + 12.f);
		const float H = 28.f;
		Outline(X, Y, W, H, Secondary(0.9f * A));
		Text(Key, X + (W - S.X) * 0.5f, Y + (H - S.Y) * 0.5f, Info, Secondary(A));
		return W;
	}

	/** Width of a prompt row: [KEY] VERB  detail. */
	float PromptWidth(const FDeepHudPrompt& P) const
	{
		float W = 0.f;
		if (!P.Key.IsEmpty())
		{
			W += FMath::Max(28.f, TextSize(P.Key, Font(EFace::Label)).X + 12.f) + Grid * 1.5f;
		}
		W += TextSize(P.Verb.ToUpper(), Font(EFace::Label)).X;
		if (!P.Detail.IsEmpty())
		{
			W += Grid * 2.f + TextSize(P.Detail, Font(EFace::Body)).X;
		}
		return W;
	}

	/** One prompt row on a panel fill, left edge X, top Y. Height 44. */
	void PromptRow(const FDeepHudPrompt& P, float X, float Y, float A) const
	{
		const float W = PromptWidth(P) + Pad * 2.f;
		const float H = 44.f;
		Box(X, Y, W, H, PanelFill(A));
		Box(X, Y, 2.f, H, ToneColor(P.Tone == EDeepHudTone::Normal ? EDeepHudTone::Primary : P.Tone, A));
		float CX = X + Pad;
		if (!P.Key.IsEmpty())
		{
			CX += KeyCap(P.Key, CX, Y + 8.f, A) + Grid * 1.5f;
		}
		const FSlateFontInfo Label = Font(EFace::Label);
		const FString Verb = P.Verb.ToUpper();
		const float LabelH = TextSize(TEXT("A"), Label).Y;
		CX += Text(Verb, CX, Y + (H - LabelH) * 0.5f, Label, ToneColor(P.Tone == EDeepHudTone::Normal ? EDeepHudTone::Primary : P.Tone, A));
		if (!P.Detail.IsEmpty())
		{
			const FSlateFontInfo Body = Font(EFace::Body);
			const float BodyH = TextSize(TEXT("A"), Body).Y;
			Text(P.Detail, CX + Grid * 2.f, Y + (H - BodyH) * 0.5f, Body, Secondary(A));
		}
	}
};

void SDeepHud::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);
	SetCanTick(false);
	// Painted fresh every frame: the data changes every frame anyway.
	ForceVolatile(true);
}

void SDeepHud::Advance(const FDeepHudFrame& InFrame, double RealNow, float DeltaSeconds)
{
	Frame = InFrame;
	Now = RealNow;
	DeltaSeconds = FMath::Clamp(DeltaSeconds, 0.f, 0.1f);

	if (Frame.Mode != LastMode)
	{
		LastMode = Frame.Mode;
		ModeStart = Now;
	}
	if (Frame.Mode == EDeepHudMode::Title)
	{
		HintShown.Reset();
	}

	for (int32 Index = 0; Index < EDeepHudPanel::Count; ++Index)
	{
		Fades[Index].Step((Frame.Panels & (1u << Index)) != 0, DeltaSeconds, PanelOpen, PanelClose);
	}

	// Anchored and slot prompts fade on their own, by id; the last copy stays for the fade out.
	TSet<FName> Seen;
	for (const FDeepHudPrompt& Prompt : Frame.Prompts)
	{
		Seen.Add(Prompt.Id);
		LastPrompts.Add(Prompt.Id, Prompt);
	}
	for (auto It = LastPrompts.CreateIterator(); It; ++It)
	{
		DeepHudModel::FFade& Fade = PromptFades.FindOrAdd(It.Key());
		Fade.Step(Seen.Contains(It.Key()), DeltaSeconds, PromptFade, PromptFade);
		if (!Fade.bTarget && Fade.Alpha() <= 0.f)
		{
			PromptFades.Remove(It.Key());
			It.RemoveCurrent();
		}
	}

	// The Captain: one radio line at a time; keep the last text while it fades out.
	if (!Frame.CaptainLine.IsEmpty())
	{
		SubtitleShown = Frame.CaptainLine;
	}
	SubtitleFade.Step(!Frame.CaptainLine.IsEmpty() && (Frame.Panels & EDeepHudPanel::Subtitle), DeltaSeconds, PromptFade, PanelClose);

	Depth.Duration = NumberTween;
	Depth.Set(Frame.DepthM, Now, 5.f);
	SubDepth.Duration = NumberTween;
	SubDepth.Set(Frame.SubDepthM, Now, 5.f);
	CargoKg.Duration = NumberTween;
	CargoKg.Set(Frame.CargoKg, Now);
	CargoValue.Duration = NumberTween;
	CargoValue.Set(static_cast<float>(Frame.CargoValue), Now);
	Patience.Duration = BarLerp;
	Patience.Set(Frame.Patience01, Now, 0.02f);
	Tether.Duration = BarLerp;
	Tether.Set(Frame.TetherLengthM > 0.f ? FMath::Clamp(Frame.TetherLeftM / Frame.TetherLengthM, 0.f, 1.f) : 0.f, Now, 0.02f);
	Air.Duration = BarLerp;
	Air.Set(Frame.AirMax > 0.f ? FMath::Clamp(Frame.AirSeconds / Frame.AirMax, 0.f, 1.f) : 0.f, Now, 0.02f);

	// The controls hint: once per mode per run.
	if ((Frame.Panels & EDeepHudPanel::ControlsHint) && Frame.Hint.Num() > 0 && !HintShown.Contains(Frame.Mode))
	{
		HintShown.Add(Frame.Mode);
		HintMode = Frame.Mode;
		HintStart = Now;
	}
	const bool bHint = HintMode == Frame.Mode && (Frame.Panels & EDeepHudPanel::ControlsHint) && Now - HintStart < Frame.HintSeconds && !Frame.bHintDone;
	if (!bHint && HintMode == Frame.Mode && Frame.bHintDone)
	{
		HintStart = -1000.0;
	}
	HintFade.Step(bHint, DeltaSeconds, PanelOpen, PromptFade);

	// Feedback: the newest cue replaces the last; cues never stack.
	if (Frame.Cues.Num() > 0)
	{
		Cue = Frame.Cues.Last();
		CueStart = Now;
	}
}

int32 SDeepHud::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	FPainter P(AllottedGeometry, OutDrawElements, LayerId);
	const FVector2f L = P.Size;
	if (L.X <= 1.f || L.Y <= 1.f)
	{
		return LayerId;
	}
	P.PxToLocal = Frame.ViewportPx.X > 1.f ? L.X / Frame.ViewportPx.X : 1.f;
	P.Hair = FMath::Max(1.f, P.PxToLocal);
	const FVector2f Centre = L * 0.5f;
	auto A = [this](uint32 Which) { return Fades[FMath::FloorLog2(Which)].Alpha(); };
	auto SlideOf = [](float Alpha) { return (1.f - Alpha) * SlideDistance; };
	const FSlateFontInfo LabelFont = Font(EFace::Label);
	const FSlateFontInfo BodyFont = Font(EFace::Body);
	const FSlateFontInfo ReadoutFont = Font(EFace::Readout);
	const FSlateFontInfo DepthFont = Font(EFace::Depth);
	const FSlateFontInfo MonoFont = Font(EFace::Mono);
	const float LabelH = P.TextSize(TEXT("A"), LabelFont).Y;
	const float BodyH = P.TextSize(TEXT("A"), BodyFont).Y;

	// ------------------------------------------------ threat: the red edge, under everything else
	if (const float TA = A(EDeepHudPanel::Threat); TA > 0.f && Frame.Threat01 > 0.02f)
	{
		const float T = FMath::Clamp(Frame.Threat01, 0.f, 1.f);
		const float Hz = FMath::Lerp(0.5f, 1.6f, T);
		const float Pulse = 0.5f + 0.5f * FMath::Sin(static_cast<float>(Now) * Hz * 2.f * PI);
		const float Base = TA * T * T * (0.30f + 0.20f * Pulse);
		const float Edge = FMath::Lerp(64.f, 240.f, T);
		constexpr int32 Strips = 16;
		const float Step = Edge / Strips;
		for (int32 I = 0; I < Strips; ++I)
		{
			const float Fall = FMath::Square(1.f - static_cast<float>(I) / Strips);
			const FLinearColor C = Alert(Base * Fall * 0.35f);
			const float O = I * Step;
			P.Box(0.f, O, L.X, Step, C);
			P.Box(0.f, L.Y - O - Step, L.X, Step, C);
			P.Box(O, 0.f, Step, L.Y, C);
			P.Box(L.X - O - Step, 0.f, Step, L.Y, C);
		}
	}

	// ------------------------------------------------ title
	if (const float TA = A(EDeepHudPanel::TitleCard); TA > 0.f)
	{
		const float Scene = EaseOutCubic(static_cast<float>((Now - ModeStart) / SceneFade));
		P.Box(0.f, 0.f, L.X, L.Y, FLinearColor(0.f, 0.f, 0.f, TA));
		const float TitleY = L.Y * 0.34f;
		// The tether: a hairline from the top of the screen down to the word.
		P.Box(Centre.X, 0.f, P.Hair, (TitleY - Grid * 3.f) * Scene, Primary(0.5f * TA));
		FSlateFontInfo TitleFont = DepthFont;
		TitleFont.LetterSpacing = 600; // the title only (chosen here): the guide's 72 px, set wide
		P.TextCentre(TEXT("DEEP"), Centre.X + 12.f, TitleY, TitleFont, Secondary(TA * Scene));
		P.TextCentre(TEXT("Salvage, a tether, and a Captain who hates you."), Centre.X, TitleY + 112.f, BodyFont, Secondary(0.6f * TA * Scene));
		const float PromptA = EaseOutCubic(static_cast<float>((Now - ModeStart - SceneFade) / PanelOpen));
		P.TextCentre(TEXT("PRESS ANY KEY"), Centre.X, L.Y * 0.62f, LabelFont, Primary(TA * PromptA));
	}

	// ------------------------------------------------ the water: depth and the tether, bottom left
	if (const float DA = A(EDeepHudPanel::Depth); DA > 0.f)
	{
		const float W = 320.f;
		const float H = 176.f;
		const float X = Margin;
		const float Y = L.Y - Margin - H + SlideOf(DA);
		P.Panel(X, Y, W, H, DA);
		float CY = Y + Pad;
		P.Text(TEXT("DEPTH"), X + Pad, CY, LabelFont, Secondary(DA));
		P.TextRight(Frame.Zone.ToUpper(), X + W - Pad, CY, LabelFont, Secondary(0.6f * DA));
		CY += LabelH;
		const FString Number = FString::Printf(TEXT("%.0f"), Depth.Value(Now));
		const float NW = P.Text(Number, X + Pad - 4.f, CY - 10.f, DepthFont, Primary(DA));
		P.Text(TEXT("m"), X + Pad + NW + Grid, CY + 30.f, ReadoutFont, Primary(0.7f * DA));
		CY = Y + H - Pad - 4.f - Grid - LabelH;
		if (Frame.bTether)
		{
			const EDeepHudTone Tone = Frame.bTaut ? EDeepHudTone::Danger : (Frame.TetherStrain > 0.75f ? EDeepHudTone::Warning : EDeepHudTone::Normal);
			P.Text(Frame.bTaut ? TEXT("TETHER  TAUT") : TEXT("TETHER"), X + Pad, CY, LabelFont, ToneColor(Tone, DA));
			P.TextRight(FString::Printf(TEXT("%.0f / %.0f m"), Frame.TetherLeftM, Frame.TetherLengthM), X + W - Pad, CY, MonoFont, ToneColor(Tone, DA));
			const float BarY = CY + LabelH + Grid;
			const float BarW = W - Pad * 2.f;
			P.Box(X + Pad, BarY, BarW, 2.f, Secondary(0.18f * DA));
			P.Box(X + Pad, BarY, BarW * Tether.Value(Now), 2.f, Tone == EDeepHudTone::Normal ? Primary(DA) : ToneColor(Tone, DA));
		}
		else
		{
			P.Text(TEXT("TETHER  CUT"), X + Pad, CY, LabelFont, Alert(DA));
		}
	}

	// ------------------------------------------------ cargo, lamp and harpoon, bottom right
	if (const float CA = A(EDeepHudPanel::Cargo); CA > 0.f)
	{
		const float W = 320.f;
		const float H = 176.f;
		const float X = L.X - Margin - W;
		const float Y = L.Y - Margin - H + SlideOf(CA);
		P.Panel(X, Y, W, H, CA);
		float CY = Y + Pad;
		P.Text(TEXT("CARGO"), X + Pad, CY, LabelFont, Secondary(CA));
		if (Frame.bOverloaded)
		{
			P.TextRight(TEXT("TOO HEAVY"), X + W - Pad, CY, LabelFont, Amber(CA));
		}
		CY += LabelH + Grid;
		const FLinearColor KgColor = Frame.bOverloaded ? Amber(CA) : Primary(CA);
		const float KW = P.Text(FString::Printf(TEXT("%.0f"), CargoKg.Value(Now)), X + Pad, CY, ReadoutFont, KgColor);
		P.Text(TEXT("kg"), X + Pad + KW + 6.f, CY + 10.f, BodyFont, Secondary(0.7f * CA));
		const FString Value = FString::Printf(TEXT("%.0f"), CargoValue.Value(Now));
		const float CrW = P.TextSize(TEXT("cr"), BodyFont).X;
		P.TextRight(Value, X + W - Pad - CrW - 6.f, CY, ReadoutFont, Primary(CA));
		P.TextRight(TEXT("cr"), X + W - Pad, CY + 10.f, BodyFont, Secondary(0.7f * CA));
		CY = Y + H - Pad - LabelH * 2.f - Grid;
		P.Box(X + Pad, CY - Grid, W - Pad * 2.f, P.Hair, Secondary(0.15f * CA));
		FString Harpoon;
		EDeepHudTone HTone = EDeepHudTone::Normal;
		switch (Frame.Harpoon)
		{
		case 0: Harpoon = TEXT("READY"); HTone = EDeepHudTone::Primary; break;
		case 1: Harpoon = TEXT("IN FLIGHT"); break;
		case 2: Harpoon = TEXT("LATCHED"); HTone = EDeepHudTone::Primary; break;
		default: Harpoon = TEXT("SPENT"); HTone = EDeepHudTone::Dim; break;
		}
		P.Text(TEXT("HARPOON"), X + Pad, CY, LabelFont, Secondary(0.6f * CA));
		P.TextRight(Harpoon, X + W - Pad, CY, LabelFont, ToneColor(HTone, CA));
		CY += LabelH + Grid * 0.5f;
		P.Text(TEXT("LAMP"), X + Pad, CY, LabelFont, Secondary(0.6f * CA));
		P.TextRight(!Frame.bHasLamp ? TEXT("NOT OWNED") : (Frame.bLampOn ? TEXT("ON") : TEXT("OFF")), X + W - Pad, CY, LabelFont,
			ToneColor(!Frame.bHasLamp ? EDeepHudTone::Dim : (Frame.bLampOn ? EDeepHudTone::Primary : EDeepHudTone::Normal), CA));
	}

	// ------------------------------------------------ the Captain's patience, top centre
	if (const float PA = A(EDeepHudPanel::Patience); PA > 0.f)
	{
		const float W = 480.f;
		const float X = Centre.X - W * 0.5f;
		const float Y = Margin - SlideOf(PA);
		const float F = Patience.Value(Now);
		const EDeepHudTone Tone = Frame.Patience01 <= 0.25f ? EDeepHudTone::Danger : (Frame.Patience01 <= 0.5f ? EDeepHudTone::Warning : EDeepHudTone::Primary);
		P.TextCentre(TEXT("THE CAPTAIN'S PATIENCE"), Centre.X, Y, LabelFont, Secondary(PA));
		const float BarY = Y + LabelH + Grid;
		P.Box(X, BarY, W, 4.f, Secondary(0.18f * PA));
		P.Box(X, BarY, W * F, 4.f, ToneColor(Tone, PA));
		for (const float Mark : Frame.PatienceMarks)
		{
			P.Box(X + W * Mark, BarY - 4.f, P.Hair, 12.f, Secondary(0.5f * PA));
		}
		if (Frame.RefillFraction > 0.f && Frame.SinceRefill < 1.2f)
		{
			const float RA = PA * (1.f - EaseOutCubic(Frame.SinceRefill / 1.2f));
			P.Text(FString::Printf(TEXT("+%d%%"), FMath::RoundToInt(Frame.RefillFraction * 100.f)), X + W + Grid * 2.f, BarY - LabelH * 0.5f, MonoFont, Primary(RA));
		}
	}

	// ------------------------------------------------ bailout air after a cut: top centre and a ring at the reticle
	if (const float BA = A(EDeepHudPanel::Bailout); BA > 0.f)
	{
		const float Y = Margin - SlideOf(BA);
		P.TextCentre(TEXT("TETHER CUT   BAILOUT AIR"), Centre.X, Y, LabelFont, Alert(BA));
		const FString Seconds = FString::Printf(TEXT("%.1f"), Frame.AirSeconds);
		const float SW = P.TextSize(Seconds, DepthFont).X;
		P.Text(Seconds, Centre.X - SW * 0.5f, Y + LabelH, DepthFont, Secondary(BA));
		P.Text(TEXT("s"), Centre.X + SW * 0.5f + Grid, Y + LabelH + 40.f, ReadoutFont, Secondary(0.7f * BA));
		P.Arc(Centre, 40.f, 1.f, Alert(0.2f * BA), 2.f);
		P.Arc(Centre, 40.f, Air.Value(Now), Alert(BA), 2.f);
	}

	// ------------------------------------------------ reticle and harpoon aim
	if (const float RA = A(EDeepHudPanel::Reticle); RA > 0.f)
	{
		P.Box(Centre.X - 2.f, Centre.Y - 2.f, 4.f, 4.f, Secondary(0.8f * RA));
		if (Frame.AimKind != 0)
		{
			const bool bFar = Frame.AimKind == 4;
			const FLinearColor C = Frame.AimKind == 3 ? Alert(RA) : (bFar ? Secondary(0.45f * RA) : Primary(RA));
			P.Brackets(Centre, bFar ? 12.f : 18.f, 6.f, C);
			P.Text(Frame.AimLabel, Centre.X + 32.f, Centre.Y - LabelH * 0.5f, MonoFont, C);
		}
		// Cut loose: the hold fills a hairline under the reticle.
		if (Frame.CutHold > 0.f)
		{
			const float W = 160.f;
			P.TextCentre(TEXT("ASKING TO BE CUT LOOSE"), Centre.X, Centre.Y + 56.f, LabelFont, Alert(RA));
			P.Box(Centre.X - W * 0.5f, Centre.Y + 56.f + LabelH + Grid, W, 2.f, Secondary(0.2f * RA));
			P.Box(Centre.X - W * 0.5f, Centre.Y + 56.f + LabelH + Grid, W * Frame.CutHold, 2.f, Alert(RA));
		}
	}

	// ------------------------------------------------ where home is
	if (const float HA = A(EDeepHudPanel::HatchMarker); HA > 0.f && !Frame.HatchLabel.IsEmpty())
	{
		if (Frame.bHatchOnScreen)
		{
			const FVector2f H = Frame.HatchPx * P.PxToLocal;
			P.Brackets(H, 20.f, 8.f, Primary(HA));
			P.TextCentre(Frame.HatchLabel, H.X, H.Y + 28.f, MonoFont, Primary(HA));
		}
		else
		{
			// At the screen edge, in the direction of home.
			const FVector2f D = Frame.HatchDir.GetSafeNormal();
			const FVector2f Half = Centre - FVector2f(Margin * 3.f, Margin * 3.f);
			const float K = FMath::Min(FMath::Abs(D.X) > 0.001f ? Half.X / FMath::Abs(D.X) : 1e6f, FMath::Abs(D.Y) > 0.001f ? Half.Y / FMath::Abs(D.Y) : 1e6f);
			const FVector2f Tip = Centre + D * K;
			const FVector2f Side(-D.Y, D.X);
			P.Lines({ Tip - D * 14.f + Side * 10.f, Tip, Tip - D * 14.f - Side * 10.f }, Primary(HA), 2.f);
			const FVector2f LabelAt = Tip - D * 40.f;
			P.TextCentre(Frame.HatchLabel, LabelAt.X, LabelAt.Y - LabelH * 0.5f, MonoFont, Primary(HA));
		}
	}

	// ------------------------------------------------ the helm: one instrument on the right
	if (const float GA = A(EDeepHudPanel::HelmGauge); GA > 0.f)
	{
		const float W = 400.f;
		const float X = L.X - Margin - W + SlideOf(GA);
		const float Y = Margin;
		const float H = L.Y - Margin * 2.f;
		P.Panel(X, Y, W, H, GA);
		float CY = Y + Pad;
		P.Text(TEXT("HELM"), X + Pad, CY, LabelFont, Amber(GA));
		P.TextRight(TEXT("DEPTH"), X + W - Pad, CY, LabelFont, Secondary(0.6f * GA));
		CY += LabelH;
		const FString Number = FString::Printf(TEXT("%.0f"), SubDepth.Value(Now));
		const float NW = P.TextSize(Number, DepthFont).X;
		const float MW = P.TextSize(TEXT("m"), ReadoutFont).X;
		P.Text(Number, X + W - Pad - MW - Grid - NW, CY - 10.f, DepthFont, Primary(GA));
		P.Text(TEXT("m"), X + W - Pad - MW, CY + 30.f, ReadoutFont, Primary(0.7f * GA));
		CY += 96.f;

		// The gauge: 0 m at the top, the deepest the hull can be rated at the bottom.
		const float GTop = CY + Grid * 2.f;
		const float GBottom = Y + H - Pad - 200.f;
		const float GX = X + Pad + 120.f;
		auto YAt = [&](float M) { return GTop + (GBottom - GTop) * FMath::Clamp(M / FMath::Max(1.f, Frame.GaugeMaxM), 0.f, 1.f); };
		P.Box(GX, GTop, P.Hair, GBottom - GTop, Secondary(0.35f * GA));
		const float LimitY = YAt(Frame.LimitM);
		P.Box(GX - 3.f, LimitY, 7.f, GBottom - LimitY, Alert(0.18f * GA));
		for (int32 Index = 0; Index < Frame.Stops.Num(); ++Index)
		{
			const float SY = YAt(Frame.Stops[Index]);
			const bool bHere = Frame.ParkedStop == Index + 1;
			P.Box(GX - 12.f, SY, 24.f, bHere ? 2.f : P.Hair, bHere ? Primary(GA) : Secondary(0.7f * GA));
			P.TextRight(FString::Printf(TEXT("%.0f"), Frame.Stops[Index]), GX - 24.f, SY - LabelH * 0.5f, MonoFont, bHere ? Primary(GA) : Secondary(0.7f * GA));
		}
		P.Box(GX - 16.f, LimitY, 32.f + 40.f, P.Hair, Alert(GA));
		P.Text(FString::Printf(TEXT("LIMIT %.0f"), Frame.LimitM), GX + 32.f, LimitY - LabelH - 2.f, LabelFont, Alert(GA));
		const float SubY = YAt(SubDepth.Value(Now));
		P.Lines({ FVector2f(GX + 4.f, SubY), FVector2f(GX + 16.f, SubY - 7.f), FVector2f(GX + 16.f, SubY + 7.f), FVector2f(GX + 4.f, SubY) }, Amber(GA), 1.5f);
		P.Text(TEXT("SUB"), GX + 24.f, SubY - LabelH * 0.5f, LabelFont, Amber(GA));

		CY = GBottom + Grid * 3.f;
		P.Box(X + Pad, CY - Grid, W - Pad * 2.f, P.Hair, Secondary(0.15f * GA));
		P.Text(Frame.HelmStatus.ToUpper(), X + Pad, CY, LabelFont, Frame.ParkedStop > 0 ? Primary(GA) : Amber(GA));
		CY += LabelH + Grid;
		for (const FString& Line : P.Wrap(Frame.HelmReason, BodyFont, W - Pad * 2.f))
		{
			P.Text(Line, X + Pad, CY, BodyFont, Secondary(GA));
			CY += BodyH;
		}
		if (Frame.bAtLimit)
		{
			P.Text(TEXT("THE HELM WILL NOT GO DEEPER"), X + Pad, Y + H - Pad - LabelH, LabelFont, Alert(GA));
		}
	}

	// ------------------------------------------------ prompts: aboard on objects, in the water under the reticle
	{
		const float OA = A(EDeepHudPanel::ObjectPrompts);
		const float WA = A(EDeepHudPanel::WaterPrompts);
		float SlotY = Centre.Y + 96.f;
		for (const TPair<FName, FDeepHudPrompt>& Pair : LastPrompts)
		{
			const FDeepHudPrompt& Prompt = Pair.Value;
			const DeepHudModel::FFade* Fade = PromptFades.Find(Pair.Key);
			const float PF = Fade ? Fade->Alpha() : 0.f;
			if (Prompt.bAnchored)
			{
				const float PA = OA * PF * Prompt.Strength;
				if (PA <= 0.f)
				{
					continue;
				}
				const FVector2f At = Prompt.ScreenPx * P.PxToLocal;
				// A hairline stem up from the object, the tag above it.
				P.Box(At.X, At.Y - 24.f, P.Hair, 24.f, ToneColor(Prompt.Tone == EDeepHudTone::Normal ? EDeepHudTone::Primary : Prompt.Tone, PA));
				const float RowW = P.PromptWidth(Prompt) + Pad * 2.f;
				const float BlockW = Prompt.Detail2.IsEmpty() ? RowW : FMath::Max(RowW, P.TextSize(Prompt.Detail2, BodyFont).X + Pad * 2.f);
				const float BlockH = 44.f + (Prompt.Detail2.IsEmpty() ? 0.f : BodyH + Grid);
				// The tag stays whole on screen; the stem still points at the object.
				const float Left = FMath::Clamp(At.X - BlockW * 0.5f, Margin, FMath::Max(Margin, L.X - Margin - BlockW));
				const float TagY = FMath::Clamp(At.Y - 24.f - BlockH, Margin, FMath::Max(Margin, L.Y - Margin - BlockH)) + SlideOf(PF);
				if (!Prompt.Detail2.IsEmpty())
				{
					P.Box(Left, TagY + 44.f, BlockW, BodyH + Grid, PanelFill(PA));
					P.Text(Prompt.Detail2, Left + Pad, TagY + 44.f + Grid * 0.25f, BodyFont, Secondary(0.75f * PA));
				}
				P.PromptRow(Prompt, Left, TagY, PA);
			}
			else
			{
				const float PA = WA * PF;
				if (PA <= 0.f)
				{
					continue;
				}
				const float RowW = P.PromptWidth(Prompt) + Pad * 2.f;
				P.PromptRow(Prompt, Centre.X - RowW * 0.5f, SlotY + SlideOf(PF), PA);
				SlotY += 44.f + Grid;
			}
		}
		// One warning line above the reticle, the most serious only.
		if (WA > 0.f && !Frame.Warning.IsEmpty())
		{
			const FLinearColor C = ToneColor(Frame.WarningTone, WA);
			const float Y = Centre.Y - 160.f;
			const float TW = P.TextCentre(Frame.Warning.ToUpper(), Centre.X, Y, Font(EFace::Heading), C);
			const float RuleY = Y + P.TextSize(TEXT("A"), Font(EFace::Heading)).Y * 0.5f;
			P.Box(Centre.X - TW * 0.5f - Grid * 2.f - 48.f, RuleY, 48.f, P.Hair, C);
			P.Box(Centre.X + TW * 0.5f + Grid * 2.f, RuleY, 48.f, P.Hair, C);
		}
	}

	// ------------------------------------------------ bite: a red band at the top with the countdown
	if (const float BA = A(EDeepHudPanel::BiteAlarm); BA > 0.f)
	{
		const float Y = Margin + 72.f;
		const float H = 104.f;
		const float RuleW = L.X * BA;
		P.Box(0.f, Y, L.X, H, PanelFill(BA));
		P.Box(Centre.X - RuleW * 0.5f, Y, RuleW, P.Hair, Alert(BA));
		P.Box(Centre.X - RuleW * 0.5f, Y + H - P.Hair, RuleW, P.Hair, Alert(BA));
		const FSlateFontInfo Heading = Font(EFace::Heading);
		P.TextCentre(TEXT("IT HAS YOUR TETHER"), Centre.X, Y + Pad, Heading, Alert(BA));
		FDeepHudPrompt Lamp;
		Lamp.Key = Frame.LampKey;
		Lamp.Verb = TEXT("LAMP ON IT");
		Lamp.Detail = FString::Printf(TEXT("cut in %.1f s"), Frame.BiteLeft);
		Lamp.Tone = EDeepHudTone::Danger;
		const float RW = P.PromptWidth(Lamp) + Pad * 2.f;
		P.PromptRow(Lamp, Centre.X - RW * 0.5f, Y + H - 44.f - Grid, BA);
	}

	// ------------------------------------------------ the controls hint, top left, once
	if (const float HA = HintFade.Alpha(); HA > 0.f && Frame.Hint.Num() > 0)
	{
		float Y = (Frame.Panels & EDeepHudPanel::HelmGauge) ? Margin : Margin + 96.f;
		const float X = Margin - SlideOf(HA);
		P.Text(TEXT("CONTROLS"), X, Y, LabelFont, Secondary(0.6f * HA));
		Y += LabelH + Grid;
		for (const FDeepHudPrompt& H : Frame.Hint)
		{
			P.PromptRow(H, X, Y, HA);
			Y += 44.f + Grid * 0.5f;
		}
	}

	// ------------------------------------------------ full-screen cards: dead, settlement, review, pause
	auto Card = [&](uint32 Which, float Width)
	{
		const float CA = A(Which);
		if (CA <= 0.f)
		{
			return;
		}
		const float Since = static_cast<float>(Now - ModeStart);
		const float Count = EaseOutCubic(Since / NumberTween);
		P.Box(0.f, 0.f, L.X, L.Y, PanelFill(CA));
		// Height from the same steps the layout uses.
		const float RowH = 40.f;
		float H = Pad * 2.f + 40.f;
		TArray<FString> Lead = P.Wrap(Frame.CardLead, BodyFont, Width - Pad * 2.f);
		H += Lead.Num() * BodyH + (Lead.Num() > 0 ? Grid * 2.f : 0.f);
		for (const FDeepHudRow& Row : Frame.Rows)
		{
			H += (Row.bBig ? 56.f : RowH) + (Row.bRuleAbove ? Grid * 2.f : 0.f);
		}
		if (!Frame.CardBig.IsEmpty())
		{
			H += 136.f;
		}
		if (!Frame.CardFooter.IsEmpty())
		{
			H += 44.f + Grid * 3.f;
		}
		H += Frame.MenuItems.Num() * 48.f + (Frame.MenuItems.Num() > 0 ? Grid * 2.f : 0.f);
		if (!Frame.MenuHint.IsEmpty())
		{
			H += LabelH + Grid * 2.f;
		}
		const float X = Centre.X - Width * 0.5f;
		float Y = FMath::Max(Margin, Centre.Y - H * 0.5f - 40.f) + SlideOf(CA);
		P.Panel(X, Y, Width, H, CA);
		float CY = Y + Pad;
		P.Text(Frame.CardTitle.ToUpper(), X + Pad, CY, LabelFont, Primary(CA));
		P.Box(X + Pad, CY + LabelH + Grid, (Width - Pad * 2.f) * Count, P.Hair, Primary(0.5f * CA));
		CY += 40.f;
		for (const FString& Line : Lead)
		{
			P.Text(Line, X + Pad, CY, BodyFont, Secondary(CA));
			CY += BodyH;
		}
		if (Lead.Num() > 0)
		{
			CY += Grid * 2.f;
		}
		for (const FDeepHudRow& Row : Frame.Rows)
		{
			if (Row.bRuleAbove)
			{
				P.Box(X + Pad, CY + Grid * 0.5f, Width - Pad * 2.f, P.Hair, Secondary(0.3f * CA));
				CY += Grid * 2.f;
			}
			const FSlateFontInfo& RowFont = Row.bBig ? ReadoutFont : BodyFont;
			const FLinearColor C = Row.bBig && Row.Tone == EDeepHudTone::Normal ? Primary(CA) : ToneColor(Row.Tone, CA);
			const float RH = Row.bBig ? 56.f : RowH;
			const float TH = P.TextSize(TEXT("A"), RowFont).Y;
			P.Text(Row.Label, X + Pad, CY + (RH - BodyH) * 0.5f, BodyFont, Row.bBig ? Secondary(CA) : C);
			const FString Amount = !Row.Text.IsEmpty() ? Row.Text
				: FString::Printf(TEXT("%s%d cr"), Row.bMinus ? TEXT("-") : TEXT(""), FMath::RoundToInt(Row.Amount * Count));
			P.TextRight(Amount, X + Width - Pad, CY + (RH - TH) * 0.5f, RowFont, C);
			CY += RH;
		}
		if (!Frame.CardBig.IsEmpty())
		{
			CY += Grid * 2.f;
			P.TextCentre(Frame.CardBigLabel.ToUpper(), Centre.X, CY, LabelFont, Secondary(0.7f * CA));
			P.TextCentre(Frame.CardBig, Centre.X, CY + LabelH + Grid, DepthFont, Primary(CA * Count));
			CY += 136.f - Grid * 2.f;
		}
		if (!Frame.CardFooter.IsEmpty())
		{
			CY += Grid * 2.f;
			FDeepHudPrompt Footer;
			Footer.Key = Frame.CardFooterKey;
			Footer.Verb = Frame.CardFooter;
			Footer.Tone = Frame.bFooterReady ? EDeepHudTone::Primary : EDeepHudTone::Dim;
			const float FW = P.PromptWidth(Footer) + Pad * 2.f;
			P.PromptRow(Footer, Centre.X - FW * 0.5f, CY, CA * (Frame.bFooterReady ? 1.f : 0.5f));
			CY += 44.f + Grid;
		}
		if (Frame.MenuItems.Num() > 0)
		{
			CY += Grid;
			for (int32 Index = 0; Index < Frame.MenuItems.Num(); ++Index)
			{
				const bool bOn = Index == Frame.MenuIndex;
				const float IA = CA * (Frame.bMenuEnabled ? 1.f : 0.45f);
				if (bOn)
				{
					P.Box(X + Pad, CY, Width - Pad * 2.f, 40.f, Primary(0.08f * IA));
					P.Box(X + Pad, CY, 2.f, 40.f, Primary(IA));
				}
				P.Text(Frame.MenuItems[Index].ToUpper(), X + Pad + Grid * 2.f, CY + (40.f - LabelH) * 0.5f, LabelFont, bOn ? Primary(IA) : Secondary(IA));
				CY += 48.f;
			}
		}
		if (!Frame.MenuHint.IsEmpty())
		{
			CY += Grid;
			P.TextCentre(Frame.MenuHint, Centre.X, CY, LabelFont, Secondary(0.5f * CA));
		}
	};
	Card(EDeepHudPanel::DeadCard, 640.f);
	Card(EDeepHudPanel::SettlementCard, 720.f);
	Card(EDeepHudPanel::ReviewCard, 720.f);
	Card(EDeepHudPanel::PauseCard, 480.f);

	// ------------------------------------------------ feedback: one small cue above the reticle, and a ping
	if (const float FA = A(EDeepHudPanel::Feedback); FA > 0.f && !Cue.Label.IsEmpty())
	{
		const float T = static_cast<float>(Now - CueStart);
		constexpr float Hold = 1.1f;
		if (T < Hold + DeepHudStyle::Slide)
		{
			const float In = EaseOutCubic(T / PromptFade);
			const float OutA = T > Hold ? 1.f - EaseOutCubic((T - Hold) / DeepHudStyle::Slide) : 1.f;
			const float CA = FA * In * OutA;
			const FLinearColor C = ToneColor(Cue.Tone, CA);
			const float Y = Centre.Y - 96.f;
			const float TW = P.TextCentre(Cue.Label.ToUpper(), Centre.X, Y, LabelFont, C);
			const float Rule = 32.f * EaseOutCubic(T / PanelOpen);
			const float RY = Y + LabelH * 0.5f;
			P.Box(Centre.X - TW * 0.5f - Grid - Rule, RY, Rule, P.Hair, C);
			P.Box(Centre.X + TW * 0.5f + Grid, RY, Rule, P.Hair, C);
			if (Frame.Panels & EDeepHudPanel::Reticle)
			{
				const float PT = FMath::Clamp(T / BarLerp, 0.f, 1.f);
				P.Arc(Centre, FMath::Lerp(6.f, 30.f, EaseOutCubic(PT)), 1.f, ToneColor(Cue.Tone, FA * (1.f - PT) * 0.8f), 1.f);
			}
		}
	}

	// ------------------------------------------------ the Captain: one radio line, lower third, last so nothing hides it
	if (const float SA = SubtitleFade.Alpha(); SA > 0.f && !SubtitleShown.IsEmpty())
	{
		const float MaxW = FMath::Min(960.f, L.X - 2.f * (Margin + 320.f + Grid * 3.f));
		const TArray<FString> Lines = P.Wrap(SubtitleShown, BodyFont, MaxW - Pad * 2.f - Grid * 2.f);
		float Widest = P.TextSize(TEXT("CAPTAIN"), LabelFont).X + 40.f;
		for (const FString& Line : Lines)
		{
			Widest = FMath::Max(Widest, P.TextSize(Line, BodyFont).X);
		}
		const float W = Widest + Pad * 2.f + Grid;
		const float H = Pad * 2.f + LabelH + Grid + Lines.Num() * BodyH;
		const float X = Centre.X - W * 0.5f;
		const float Y = L.Y - Margin - H + SlideOf(SA) * 0.5f;
		P.Box(X, Y, W, H, PanelFill(SA));
		P.Box(X, Y, 2.f, H, Amber(SA));
		float CY = Y + Pad;
		// The name tag, and three squelch bars that read as a radio.
		const float TagW = P.Text(TEXT("CAPTAIN"), X + Pad + Grid, CY, LabelFont, Amber(SA));
		for (int32 Bar = 0; Bar < 3; ++Bar)
		{
			const float BH = 4.f + Bar * 4.f;
			P.Box(X + Pad + Grid + TagW + Grid * 1.5f + Bar * 6.f, CY + LabelH - BH - 3.f, 3.f, BH, Amber(0.7f * SA));
		}
		CY += LabelH + Grid;
		for (const FString& Line : Lines)
		{
			P.Text(Line, X + Pad + Grid, CY, BodyFont, Secondary(SA));
			CY += BodyH;
		}
	}

	return LayerId + 2;
}
