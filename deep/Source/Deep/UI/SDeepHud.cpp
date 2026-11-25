#include "UI/SDeepHud.h"

#include "Brushes/SlateColorBrush.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Sonar/DeepSonarRules.h"
#include "UI/DeepHudStyle.h"

namespace
{
	using namespace DeepHudStyle;
	using DeepHudModel::EaseOutCubic;

	const FSlateColorBrush& White()
	{
		static const FSlateColorBrush Brush(BrushBase());
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
		case EDeepHudTone::Warning: return Warn(A);
		case EDeepHudTone::Danger: return Alert(A);
		case EDeepHudTone::Dim: return Secondary(DimAlpha * A);
		default: return Secondary(A);
		}
	}

	/** A bar's fill by the spec's thresholds: primary, amber at half, alert at a quarter. */
	FLinearColor BarColor(float Fraction01, float A)
	{
		switch (DeepHudModel::BarTone(Fraction01))
		{
		case DeepHudModel::EBarTone::Danger: return Alert(A);
		case DeepHudModel::EBarTone::Warn: return Warn(A);
		default: return Primary(A);
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
		Outline(X, Y, W, H, Primary(HairlineAlpha * A));
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

	// Marks with no panel behind them (the reticle, the aim brackets, the hatch marker and their labels) carry a dark
	// edge one pixel wide in Shade at OutlineAlpha, drawn a layer under the mark, so they hold their contrast over the
	// lamp's glow in any palette.
	FLinearColor EdgeOf(const FLinearColor& Color) const { return Shade(OutlineAlpha * Color.A); }

	void MarkBox(float X, float Y, float W, float H, const FLinearColor& Color) const
	{
		Box(X - Hair, Y - Hair, W + Hair * 2.f, H + Hair * 2.f, EdgeOf(Color));
		Box(X, Y, W, H, Color);
	}

	void MarkLines(const TArray<FVector2f>& Points, const FLinearColor& Color, float Thickness) const
	{
		if (Color.A <= 0.001f || Points.Num() < 2)
		{
			return;
		}
		FSlateDrawElement::MakeLines(Out, Layer, Geo.ToPaintGeometry(), Points, ESlateDrawEffect::None, EdgeOf(Color), true, Thickness + Hair * 2.f);
		Lines(Points, Color, Thickness);
	}

	/** Text with the dark edge: the shade one pixel down and right, a layer under the text. */
	float MarkText(const FString& String, float X, float Y, const FSlateFontInfo& Info, const FLinearColor& Color) const
	{
		const FVector2f S = TextSize(String, Info);
		if (Color.A > 0.001f && !String.IsEmpty())
		{
			FSlateDrawElement::MakeText(Out, Layer, Geo.ToPaintGeometry(S, FSlateLayoutTransform(FVector2f(X + Hair, Y + Hair))), String, Info, ESlateDrawEffect::None, EdgeOf(Color));
		}
		return Text(String, X, Y, Info, Color);
	}

	float MarkTextCentre(const FString& String, float CX, float Y, const FSlateFontInfo& Info, const FLinearColor& Color) const
	{
		return MarkText(String, CX - TextSize(String, Info).X * 0.5f, Y, Info, Color);
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

	/** Four corner brackets around a centre, with the dark edge (they have no panel); filled draws each corner as a solid square of the arm's size (latched). */
	void Brackets(FVector2f C, float Half, float Arm, const FLinearColor& Color, bool bFilled = false) const
	{
		for (const float SX : { -1.f, 1.f })
		{
			for (const float SY : { -1.f, 1.f })
			{
				const FVector2f Corner = C + FVector2f(SX * Half, SY * Half);
				if (bFilled)
				{
					MarkBox(SX > 0.f ? Corner.X - Arm : Corner.X, SY > 0.f ? Corner.Y - Arm : Corner.Y, Arm, Arm, Color);
				}
				else
				{
					MarkLines({ Corner - FVector2f(SX * Arm, 0.f), Corner, Corner - FVector2f(0.f, SY * Arm) }, Color, 1.5f);
				}
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
	// The calm HUD: what the player does not need right now never reaches the painter.
	Calm.Tuning.bHelmSonar = DeepHudCalm::HelmSonarOn();
	Calm.Apply(Frame, Now);

	if (Frame.Mode != LastMode)
	{
		LastMode = Frame.Mode;
		ModeStart = Now;
	}

	if ((Frame.Panels & EDeepHudPanel::TitleCard) && !Fades[FMath::FloorLog2(static_cast<uint32>(EDeepHudPanel::TitleCard))].bTarget)
	{
		TitleStart = Now;
	}
	for (int32 Index = 0; Index < EDeepHudPanel::Count; ++Index)
	{
		// The panels that come and go while playing (hull, cargo) ease slower, so they never pop.
		const uint32 Bit = 1u << Index;
		const bool bCalm = Bit == static_cast<uint32>(EDeepHudPanel::Hull) || Bit == static_cast<uint32>(EDeepHudPanel::Cargo);
		const float Open = bCalm ? Calm.Tuning.FadeSeconds : PanelOpen;
		const float Close = bCalm ? Calm.Tuning.FadeSeconds : PanelClose;
		Fades[Index].Step((Frame.Panels & Bit) != 0, DeltaSeconds, Open, Close);
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
	if (Frame.Hull01 >= 0.f)
	{
		HullBar.Step(Frame.Hull01, Now);
	}

	// The controls hint: the calm layer decides; it eases in and out and keeps its rows for the fade.
	const bool bHint = (Frame.Panels & EDeepHudPanel::ControlsHint) && Frame.Hint.Num() > 0;
	if (bHint)
	{
		HintRows = Frame.Hint;
	}
	HintFade.Step(bHint, DeltaSeconds, Calm.Tuning.FadeSeconds, Calm.Tuning.FadeSeconds);

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
		// At the border: ThreatEdgeMin at the least threat up to ThreatEdgeMax at the most, breathing a little; it falls
		// to nothing inward. Strips overlap only in the corners.
		const float Base = TA * FMath::Lerp(ThreatEdgeMin, ThreatEdgeMax, T * T) * (0.8f + 0.2f * Pulse);
		const float Edge = FMath::Lerp(64.f, 240.f, T);
		constexpr int32 Strips = 16;
		const float Step = Edge / Strips;
		for (int32 I = 0; I < Strips; ++I)
		{
			const float Fall = FMath::Square(1.f - static_cast<float>(I) / Strips);
			const FLinearColor C = Alert(Base * Fall);
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
		const float Scene = EaseOutCubic(static_cast<float>((Now - TitleStart) / SceneFade));
		// The veil: bands from near black at the top to thinner at the foot (eased), the room showing through below.
		{
			constexpr int32 Bands = 24;
			const float BandH = L.Y / Bands;
			for (int32 I = 0; I < Bands; ++I)
			{
				const float T = FMath::Pow((I + 0.5f) / Bands, 1.6f);
				const float VeilA = FMath::Lerp(TitleVeilTop, TitleVeilBottom, T);
				// Whole-unit edges, so neighbouring bands neither overlap (a dark line) nor leave a gap.
				const float Y0 = FMath::RoundToFloat(I * BandH);
				const float Y1 = FMath::RoundToFloat((I + 1) * BandH);
				P.Box(0.f, Y0, L.X, Y1 - Y0, Shade(TA * VeilA));
			}
		}
		// A screen over the title (settings, credits, quit): the words step back under its card.
		const float Cover = FMath::Max3(A(EDeepHudPanel::SettingsCard), A(EDeepHudPanel::CreditsCard), A(EDeepHudPanel::ConfirmCard));
		const float WA = TA * (1.f - Cover);
		const float TitleY = L.Y * 0.30f;
		// The tether: a hairline from the top of the screen down to the word.
		P.Box(Centre.X, 0.f, P.Hair, (TitleY - Grid * 3.f) * Scene, Primary(0.5f * WA));
		FSlateFontInfo TitleFont = DepthFont;
		TitleFont.LetterSpacing = 600; // the title only (chosen here): the guide's 72 px, set wide
		P.TextCentre(TEXT("DEEP"), Centre.X + 12.f, TitleY, TitleFont, Secondary(WA * Scene));
		P.TextCentre(TEXT("Salvage, a tether, and a Captain who hates you."), Centre.X, TitleY + 112.f, BodyFont, Secondary(0.6f * WA * Scene));
		const float PromptA = WA * EaseOutCubic(static_cast<float>((Now - TitleStart - SceneFade) / PanelOpen));
		// The title menu: the pause card's rows, centred, no panel.
		const float ItemW = 360.f;
		float MY = L.Y * 0.56f;
		for (int32 Index = 0; Index < Frame.MenuItems.Num(); ++Index)
		{
			const bool bOn = Index == Frame.MenuIndex;
			if (bOn)
			{
				P.Box(Centre.X - ItemW * 0.5f, MY, ItemW, 40.f, Primary(0.08f * PromptA));
				P.Box(Centre.X - ItemW * 0.5f, MY, 2.f, 40.f, Primary(PromptA));
			}
			P.TextCentre(Frame.MenuItems[Index].ToUpper(), Centre.X, MY + (40.f - LabelH) * 0.5f, LabelFont, bOn ? Primary(PromptA) : Secondary(PromptA));
			MY += 48.f;
		}
		if (!Frame.MenuHint.IsEmpty())
		{
			P.TextCentre(Frame.MenuHint, Centre.X, MY + Grid * 2.f, LabelFont, Secondary(0.5f * PromptA));
		}
	}

	// ------------------------------------------------ the water: depth, bottom left, small (the calm HUD: label and number)
	if (const float DA = A(EDeepHudPanel::Depth); DA > 0.f)
	{
		FDeepHudRect R = DeepHudLayout::DepthPanel(L);
		R.Y += SlideOf(DA);
		P.Panel(R.X, R.Y, R.W, R.H, DA);
		const float CY = R.Y + Pad;
		P.Text(TEXT("DEPTH"), R.X + Pad, CY, LabelFont, Secondary(DA));
		const FString Number = FString::Printf(TEXT("%.0f"), Depth.Value(Now));
		const float ReadH = P.TextSize(TEXT("0"), ReadoutFont).Y;
		const float NW = P.Text(Number, R.X + Pad, CY + LabelH, ReadoutFont, Primary(DA));
		P.Text(TEXT("m"), R.X + Pad + NW + 6.f, CY + LabelH + ReadH - BodyH - 3.f, BodyFont, Secondary(UnitAlpha * DA));
		// The tether's strain: a 2 px line along the panel's foot, only once it pulls; alert when taut.
		if (Frame.bTether && Frame.TetherStrain > StrainShowAt)
		{
			const bool bAlert = Frame.bTaut || Frame.TetherStrain > StrainAlertAt;
			const float Show = FMath::Clamp((Frame.TetherStrain - StrainShowAt) / (1.f - StrainShowAt), 0.f, 1.f);
			const float LineA = DA * (bAlert ? 1.f : FMath::Lerp(0.45f, 1.f, Show));
			P.Box(R.X + Pad, R.Bottom() - 6.f, R.W - Pad * 2.f, 2.f, bAlert ? Alert(LineA) : Primary(LineA));
		}
	}

	// ------------------------------------------------ cargo, bottom right (spec A: 280 x 128; kilograms, credits, too heavy)
	if (const float CA = A(EDeepHudPanel::Cargo); CA > 0.f)
	{
		FDeepHudRect R = DeepHudLayout::CargoPanel(L);
		R.Y += SlideOf(CA);
		P.Panel(R.X, R.Y, R.W, R.H, CA);
		float CY = R.Y + Pad;
		P.Text(TEXT("CARGO"), R.X + Pad, CY, LabelFont, Secondary(CA));
		CY += LabelH + Grid * 0.5f;
		const float ReadH = P.TextSize(TEXT("0"), ReadoutFont).Y;
		const float UnitY = CY + ReadH - BodyH - 3.f;
		const float KW = P.Text(FString::Printf(TEXT("%.0f"), CargoKg.Value(Now)), R.X + Pad, CY, ReadoutFont, Frame.bOverloaded ? Warn(CA) : Primary(CA));
		P.Text(TEXT("kg"), R.X + Pad + KW + 6.f, UnitY, BodyFont, Secondary(UnitAlpha * CA));
		const FString Value = FString::Printf(TEXT("%.0f"), CargoValue.Value(Now));
		const float CrW = P.TextSize(TEXT("cr"), BodyFont).X;
		P.TextRight(Value, R.Right() - Pad - CrW - 6.f, CY, ReadoutFont, Secondary(CA));
		P.TextRight(TEXT("cr"), R.Right() - Pad, UnitY, BodyFont, Secondary(UnitAlpha * CA));
		if (Frame.bOverloaded)
		{
			P.Text(TEXT("TOO HEAVY"), R.X + Pad, R.Bottom() - Pad - LabelH, LabelFont, Warn(CA));
		}
	}

	// The Captain's patience bar is gone (the calm HUD, Nov 15): no mode lists EDeepHudPanel::Patience.

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
		P.MarkBox(Centre.X - 2.f, Centre.Y - 2.f, 4.f, 4.f, Secondary(ReticleAlpha * RA));
		// The harpoon has no row of its own in A: its state rides on the aim brackets. Ready: brackets by target.
		// Latched: solid corners. Spent: the brackets dimmed, NO HARPOON.
		const float MonoH = P.TextSize(TEXT("0"), MonoFont).Y;
		if (Frame.Harpoon == 2)
		{
			P.Brackets(Centre, 18.f, 6.f, Primary(RA), true);
			P.MarkText(TEXT("LATCHED"), Centre.X + 32.f, Centre.Y - MonoH * 0.5f, MonoFont, Primary(RA));
		}
		else if (Frame.AimKind != 0)
		{
			const bool bSpent = Frame.Harpoon == 3;
			const bool bFar = Frame.AimKind == 4;
			const FLinearColor C = bSpent || bFar ? Secondary(FarAlpha * RA) : (Frame.AimKind == 3 ? Alert(RA) : Primary(RA));
			P.Brackets(Centre, 18.f, 6.f, C);
			P.MarkText(bSpent ? FString(TEXT("NO HARPOON")) : Frame.AimLabel, Centre.X + 32.f, Centre.Y - MonoH * 0.5f, MonoFont, C);
		}
		// Cut loose: the hold fills a hairline under the reticle.
		if (Frame.CutHold > 0.f)
		{
			const float W = 160.f;
			P.MarkTextCentre(TEXT("ASKING TO BE CUT LOOSE"), Centre.X, Centre.Y + 56.f, LabelFont, Alert(RA));
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
			P.MarkTextCentre(Frame.HatchLabel, H.X, H.Y + 28.f, MonoFont, Primary(HA));
		}
		else
		{
			// At the screen edge, inset by the margin, in the direction of home. Top and bottom keep clear of the bailout
			// readout and the depth and cargo panels, so the chevron never lands on a readout.
			const FVector2f D = Frame.HatchDir.GetSafeNormal();
			const float HalfX = Centre.X - Margin;
			const float HalfY = Centre.Y - Margin - (D.Y < 0.f ? PatienceH : WaterPanelH) - Grid * 2.f;
			const float K = FMath::Min(FMath::Abs(D.X) > 0.001f ? HalfX / FMath::Abs(D.X) : 1e6f, FMath::Abs(D.Y) > 0.001f ? HalfY / FMath::Abs(D.Y) : 1e6f);
			const FVector2f Tip = Centre + D * K;
			const FVector2f Side(-D.Y, D.X);
			// Two 14 px arms at right angles, 2 px.
			const float Arm = 14.f * UE_INV_SQRT_2;
			P.MarkLines({ Tip - D * Arm + Side * Arm, Tip, Tip - D * Arm - Side * Arm }, Primary(HA), 2.f);
			const FVector2f LabelSize = P.TextSize(Frame.HatchLabel, MonoFont);
			const FVector2f LabelAt = Tip - D * 40.f;
			const float LabelX = FMath::Clamp(LabelAt.X, Margin + LabelSize.X * 0.5f, L.X - Margin - LabelSize.X * 0.5f);
			P.MarkTextCentre(Frame.HatchLabel, LabelX, LabelAt.Y - LabelSize.Y * 0.5f, MonoFont, Primary(HA));
		}
	}

	// ------------------------------------------------ the helm: a slim depth readout with the next stop, bottom right
	// The calm HUD (Nov 15) cut the gauge column and the "between stops" text; the sonar is the instrument.
	if (const float GA = A(EDeepHudPanel::HelmGauge); GA > 0.f)
	{
		FDeepHudRect R = DeepHudLayout::HelmGauge(L);
		R.Y += SlideOf(GA);
		P.Panel(R.X, R.Y, R.W, R.H, GA);
		const float CY = R.Y + Pad;
		P.Text(TEXT("DEPTH"), R.X + Pad, CY, LabelFont, Secondary(GA));
		const float NextM = DeepHudCalm::NextStopM(Frame.Stops, Frame.SubDepthM, Frame.ParkedStop);
		const bool bLimit = Frame.bAtLimit || (NextM > 0.f && Frame.LimitM > 0.f && NextM > Frame.LimitM + 0.5f);
		if (bLimit)
		{
			P.TextRight(FString::Printf(TEXT("LIMIT %.0f m"), Frame.LimitM), R.Right() - Pad, CY, LabelFont, Alert(GA));
		}
		else if (NextM > 0.f)
		{
			P.TextRight(FString::Printf(TEXT("NEXT STOP %.0f m"), NextM), R.Right() - Pad, CY, LabelFont, Secondary(GA));
		}
		const FString Number = FString::Printf(TEXT("%.0f"), SubDepth.Value(Now));
		const float ReadH = P.TextSize(TEXT("0"), ReadoutFont).Y;
		const float NW = P.Text(Number, R.X + Pad, CY + LabelH, ReadoutFont, Primary(GA));
		P.Text(TEXT("m"), R.X + Pad + NW + 6.f, CY + LabelH + ReadH - BodyH - 3.f, BodyFont, Secondary(UnitAlpha * GA));

		// ---- the seated readout, stacked on it: the levers the helm camera cannot see (rescope, Nov 15).
		// THROTTLE with the telegraph's notches, BALLAST with a centre-zero bar, HEADING in degrees.
		FDeepHudRect V = DeepHudLayout::HelmLevers(L);
		V.Y += SlideOf(GA);
		P.Panel(V.X, V.Y, V.W, V.H, GA);
		const float VX = V.X + Pad;
		const float VW = V.W - 2.f * Pad;
		const float RowGap = 10.f;
		const float BarGap = 6.f;
		const float TickH = 8.f;
		float RY = V.Y + Pad;

		// A centre-zero bar under a row: the share of full either way.
		auto CentreBar = [&](float Top, float Share)
		{
			const float BY = Top + LabelH + BarGap;
			const float Mid = VX + VW * 0.5f;
			const float Lever = FMath::Clamp(Share, -1.f, 1.f);
			P.Box(VX, BY + 2.f, VW, TickH - 4.f, Primary(0.18f * GA));
			const float FillW = FMath::Abs(Lever) * VW * 0.5f;
			P.Box(Lever < 0.f ? Mid - FillW : Mid, BY + 2.f, FillW, TickH - 4.f, Primary(GA));
			P.Box(Mid - 1.f, BY, 2.f, TickH, Secondary(GA));
		};
		if (Frame.bHelmDirect)
		{
			// D-070's direct drive: no levers to read, so the way the sub is making (ahead or astern, up or down).
			P.Text(TEXT("SPEED"), VX, RY, LabelFont, Secondary(GA));
			P.TextRight(DeepHudCalm::SpeedLabel(Frame.HelmSurgeMps), V.Right() - Pad, RY, LabelFont, Primary(GA));
			CentreBar(RY, Frame.HelmAheadMaxMps > 0.f ? Frame.HelmSurgeMps / Frame.HelmAheadMaxMps : 0.f);
			RY += LabelH + BarGap + TickH + RowGap;
			P.Text(TEXT("UP / DOWN"), VX, RY, LabelFont, Secondary(GA));
			P.TextRight(DeepHudCalm::HeaveLabel(Frame.HelmHeaveMps), V.Right() - Pad, RY, LabelFont, Primary(GA));
			CentreBar(RY, Frame.HelmHeaveMaxMps > 0.f ? Frame.HelmHeaveMps / Frame.HelmHeaveMaxMps : 0.f);
			RY += LabelH + BarGap + TickH + RowGap;
		}
		else
		{
			P.Text(TEXT("THROTTLE"), VX, RY, LabelFont, Secondary(GA));
			P.TextRight(DeepHudCalm::ThrottleLabel(Frame.HelmNotch), V.Right() - Pad, RY, LabelFont, Primary(GA));
			{
				const int32 Lo = FMath::Min(Frame.HelmNotchMin, 0);
				const int32 Hi = FMath::Max(Frame.HelmNotchMax, 0);
				const int32 N = Hi - Lo + 1;
				const float Gap = 4.f;
				const float CellW = (VW - Gap * (N - 1)) / static_cast<float>(N);
				const float TY = RY + LabelH + BarGap;
				for (int32 I = 0; I < N; ++I)
				{
					const int32 Notch = Lo + I;
					const float CX = VX + I * (CellW + Gap);
					const FLinearColor C = Notch == Frame.HelmNotch ? Primary(GA) : Notch == 0 ? Secondary(0.6f * GA) : Primary(0.18f * GA);
					P.Box(CX, TY, CellW, TickH, C);
				}
			}
			RY += LabelH + BarGap + TickH + RowGap;

			P.Text(TEXT("BALLAST"), VX, RY, LabelFont, Secondary(GA));
			P.TextRight(DeepHudCalm::BallastLabel(Frame.HelmBallast), V.Right() - Pad, RY, LabelFont, Primary(GA));
			CentreBar(RY, Frame.HelmBallast);
			RY += LabelH + BarGap + TickH + RowGap;
		}

		P.Text(TEXT("HEADING"), VX, RY, LabelFont, Secondary(GA));
		const float HdgH = P.TextSize(TEXT("0"), MonoFont).Y;
		P.TextRight(DeepHudCalm::HeadingLabel(Frame.HeadingDeg), V.Right() - Pad, RY + (LabelH - HdgH) * 0.5f, MonoFont, Primary(GA));
	}

	// ------------------------------------------------ the hull, at the helm in the cave shaft (spec A, element 11: 400 x 48)
	// HULL, an 8 px bar, the value. A hit eases the bar down, leaves a fading ghost of what was lost and flashes the
	// hairline; at zero the bar gives way to HULL BREACH.
	if (const float HA = A(EDeepHudPanel::Hull); HA > 0.f && Frame.Hull01 >= 0.f)
	{
		FDeepHudRect R = DeepHudLayout::HullSlot(L);
		R.X -= SlideOf(HA);
		const float Flash = HullBar.FlashAlpha(Now);
		P.Box(R.X, R.Y, R.W, R.H, PanelFill(HA));
		P.Outline(R.X, R.Y, R.W, R.H, Flash > 0.f ? Alert(HA) : Primary(HairlineAlpha * HA));
		const float Hull = FMath::Clamp(Frame.Hull01, 0.f, 1.f);
		const FLinearColor Color = BarColor(Hull, HA);
		const float RowY = R.Y + (R.H - LabelH) * 0.5f;
		const float MonoH = P.TextSize(TEXT("0"), MonoFont).Y;
		P.Text(TEXT("HULL"), R.X + Pad, RowY, LabelFont, Secondary(HA));
		P.TextRight(FString::Printf(TEXT("%.0f%%"), Hull * 100.f), R.Right() - Pad, R.Y + (R.H - MonoH) * 0.5f, MonoFont, Color);
		const float BarX = R.X + Pad + P.TextSize(TEXT("HULL"), LabelFont).X + Pad;
		const float BarW = FMath::Max(0.f, R.Right() - Pad - P.TextSize(TEXT("100%"), MonoFont).X - Pad - BarX);
		const float BarY = R.Y + (R.H - HullBarPx) * 0.5f;
		if (HullBar.IsBreach())
		{
			const float Pulse = 0.65f + 0.35f * FMath::Sin(static_cast<float>(Now) * 4.f * PI);
			P.TextCentre(TEXT("HULL BREACH"), BarX + BarW * 0.5f, RowY, LabelFont, Alert(Pulse * HA));
		}
		else
		{
			const float Fill = FMath::Clamp(HullBar.Fill01(Now), 0.f, 1.f);
			P.Box(BarX, BarY, BarW, HullBarPx, Secondary(TrackAlpha * HA));
			if (const float GA = HullBar.GhostAlpha(Now); GA > 0.f && HullBar.GhostTop > Fill)
			{
				P.Box(BarX + BarW * Fill, BarY, BarW * (HullBar.GhostTop - Fill), HullBarPx, Secondary(GhostAlpha * GA * HA));
			}
			P.Box(BarX, BarY, BarW * Fill, HullBarPx, Color);
			// Ticks where the colour changes: amber at half, alert at a quarter.
			for (const float Mark : { BarDangerAt, BarWarnAt })
			{
				P.Box(BarX + BarW * Mark, BarY + HullBarPx * 0.5f - BarTickPx * 0.5f, P.Hair, BarTickPx, Secondary(TickAlpha * HA));
			}
		}
	}

	// ------------------------------------------------ the helm sonar: an amber scope, bottom left (Sonar lane)
	if (const float SA = A(EDeepHudPanel::Sonar); SA > 0.f && Frame.Sonar.RangeM > 0.f)
	{
		const FDeepHudSonar& S = Frame.Sonar;
		FDeepHudRect R = DeepHudLayout::Sonar(L);
		R.X -= SlideOf(SA);
		P.Panel(R.X, R.Y, R.W, R.H, SA);
		FVector2f C;
		float Radius = 0.f;
		DeepHudLayout::SonarScope(R, C, Radius);
		auto At = [&](float BearingDeg, float RangeM)
		{
			const FVector2D V = UDeepSonarRules::PanelPoint(BearingDeg, RangeM, S.RangeM, Radius);
			return C + FVector2f(static_cast<float>(V.X), static_cast<float>(V.Y));
		};

		// Header: the instrument's name in the helm's amber, its range on the right.
		const float HY = R.Y + Pad;
		P.Text(TEXT("SONAR"), R.X + Pad, HY, LabelFont, MachineAmber(SA));
		P.TextRight(FString::Printf(TEXT("RANGE %.0f m"), S.RangeM), R.Right() - Pad, HY, MonoFont, Secondary(0.6f * SA));

		// The scope: rim, range rings with their metres, a faint cross on the bow and beam.
		const float Near = S.Proximity01;
		const float Pulse = 0.5f + 0.5f * FMath::Sin(static_cast<float>(Now) * FMath::Lerp(2.f, 9.f, Near) * 2.f * PI);
		P.Arc(C, Radius, 1.f, Near > 0.f ? FMath::Lerp(MachineAmber(SonarRimAlpha * SA), Alert((0.55f + 0.45f * Pulse) * SA), FMath::Min(1.f, Near * 1.5f)) : MachineAmber(SonarRimAlpha * SA), Near > 0.f ? 2.f : 1.5f);
		for (float Ring = S.RingStepM; Ring < S.RangeM - 0.5f; Ring += FMath::Max(1.f, S.RingStepM))
		{
			const float RR = Radius * Ring / S.RangeM;
			P.Arc(C, RR, 1.f, Secondary(SonarRingAlpha * SA), 1.f);
		}
		for (float Ring = S.RingStepM; Ring <= S.RangeM + 0.5f; Ring += FMath::Max(1.f, S.RingStepM))
		{
			const FVector2f LabelAt = At(135.f, Ring);
			P.Text(FString::Printf(TEXT("%.0f"), Ring), LabelAt.X + 3.f, LabelAt.Y - 2.f, MonoFont, Secondary(TickAlpha * SA));
		}
		P.Box(C.X - Radius, C.Y, Radius * 2.f, P.Hair, Secondary(0.5f * SonarRingAlpha * SA));
		P.Box(C.X, C.Y - Radius, P.Hair, Radius * 2.f, Secondary(0.5f * SonarRingAlpha * SA));
		P.Lines({ C + FVector2f(-5.f, -Radius - 9.f), C + FVector2f(0.f, -Radius - 2.f), C + FVector2f(5.f, -Radius - 9.f) }, MachineAmber(0.7f * SA), 1.5f);

		// The sweep line and its fading trail behind it.
		for (int32 I = SonarTrailSteps; I >= 0; --I)
		{
			const float Back = SonarTrailDeg * I / SonarTrailSteps;
			const float TA = I == 0 ? 0.9f : 0.22f * FMath::Square(1.f - static_cast<float>(I) / SonarTrailSteps);
			P.Lines({ C, At(S.SweepDeg - Back, S.RangeM) }, MachineAmber(TA * SA), I == 0 ? 1.5f : 3.f);
		}

		// Returns: neighbouring beams at about the same range join into a wall; a lone return is a speck.
		auto DrawSlice = [&](const TArray<FDeepHudSonarReturn>& Slice, float SliceAlpha, float Thickness)
		{
			const int32 N = Slice.Num();
			for (int32 I = 0; I < N; ++I)
			{
				const FDeepHudSonarReturn& Here = Slice[I];
				if (Here.RangeM < 0.f || Here.Strength <= 0.01f)
				{
					continue;
				}
				const float Bearing = 360.f * I / N;
				const FVector2f HereAt = At(Bearing, Here.RangeM);
				const FDeepHudSonarReturn& Next = Slice[(I + 1) % N];
				const FDeepHudSonarReturn& Prev = Slice[(I + N - 1) % N];
				const bool bJoinNext = Next.RangeM >= 0.f && Next.Strength > 0.01f && FMath::Abs(Next.RangeM - Here.RangeM) < S.JoinM;
				const bool bJoinPrev = Prev.RangeM >= 0.f && Prev.Strength > 0.01f && FMath::Abs(Prev.RangeM - Here.RangeM) < S.JoinM;
				const FLinearColor Col = MachineAmber(SliceAlpha * FMath::Min(Here.Strength, bJoinNext ? Next.Strength : 1.f) * SA);
				if (bJoinNext)
				{
					P.Lines({ HereAt, At(360.f * (I + 1) / N, Next.RangeM) }, Col, Thickness);
				}
				else if (!bJoinPrev)
				{
					P.Box(HereAt.X - Thickness, HereAt.Y - Thickness, Thickness * 2.f, Thickness * 2.f, MachineAmber(SliceAlpha * Here.Strength * SA));
				}
			}
		};
		DrawSlice(S.Lower, SonarLowerAlpha, SonarLowerPx);
		DrawSlice(S.Upper, 1.f, SonarWallPx);

		// The sub at its true size in the middle, bow up.
		if (S.HullHalfM.X > 0.f && S.HullHalfM.Y > 0.f)
		{
			const float K = Radius / S.RangeM;
			const float HX = FMath::Max(3.f, S.HullHalfM.Y * K);
			const float HY2 = FMath::Max(4.f, S.HullHalfM.X * K);
			P.Lines({ C + FVector2f(-HX, HY2), C + FVector2f(-HX, -HY2 + HX * 0.6f), C + FVector2f(0.f, -HY2), C + FVector2f(HX, -HY2 + HX * 0.6f), C + FVector2f(HX, HY2), C + FVector2f(-HX, HY2) }, Secondary(0.85f * SA), 1.5f);
		}
		else
		{
			P.Box(C.X - 3.f, C.Y - 3.f, 6.f, 6.f, Secondary(0.85f * SA));
		}

		// Contacts: salvage in HUD primary (a relic in a ring), the serpent in red; a caret says above or below.
		for (const FDeepHudSonarBlip& B : S.Blips)
		{
			if (B.Strength <= 0.01f)
			{
				continue;
			}
			const FVector2f BAt = At(B.BearingDeg, B.RangeM);
			const float BA = B.Strength * SA;
			const float H = SonarBlipPx * 0.5f;
			if (B.Kind == 2)
			{
				const float Big = H * 1.8f;
				P.Lines({ BAt + FVector2f(0.f, -Big), BAt + FVector2f(Big, 0.f), BAt + FVector2f(0.f, Big), BAt + FVector2f(-Big, 0.f), BAt + FVector2f(0.f, -Big) }, Alert(BA), 2.f);
				P.Box(BAt.X - H * 0.6f, BAt.Y - H * 0.6f, H * 1.2f, H * 1.2f, Alert(BA));
			}
			else
			{
				P.Box(BAt.X - H * 0.5f, BAt.Y - H * 0.5f, H, H, Primary(BA));
				if (B.Kind == 1)
				{
					P.Arc(BAt, H * 1.6f, 1.f, Primary(BA), 1.f);
				}
			}
			if (FMath::Abs(B.DzM) >= 4.f)
			{
				const float Dir = B.DzM > 0.f ? -1.f : 1.f;
				const FVector2f Tip = BAt + FVector2f(H * 2.f + 4.f, Dir * 5.f);
				P.Lines({ Tip + FVector2f(-4.f, -Dir * 4.f), Tip, Tip + FVector2f(4.f, -Dir * 4.f) }, B.Kind == 2 ? Alert(BA) : Primary(BA), 1.5f);
			}
		}

		// Footer: the nearest wall from the hull, the dim slice's depth, the bottom under the keel.
		const float FY = C.Y + Radius + Grid * 2.f;
		const FString Wall = S.ClearanceM >= 0.f ? FString::Printf(TEXT("WALL %.0f m"), S.ClearanceM) : FString(TEXT("WALL --"));
		const FLinearColor WallColor = Near > 0.5f ? Alert(SA) : (Near > 0.f ? MachineAmber(SA) : Secondary(0.8f * SA));
		P.Text(Wall, R.X + Pad, FY, MonoFont, WallColor);
		P.TextRight(S.BedM >= 0.f ? FString::Printf(TEXT("BED %.0f m"), S.BedM) : FString(TEXT("BED --")), R.Right() - Pad, FY, MonoFont, Secondary(0.8f * SA));
		const float LY = FY + LabelH + Grid;
		P.Box(R.X + Pad, LY + LabelH * 0.5f, 20.f, SonarWallPx, MachineAmber(SA));
		const float LW = P.Text(TEXT("AT DEPTH"), R.X + Pad + 28.f, LY, LabelFont, Secondary(0.6f * SA));
		const float L2 = R.X + Pad + 28.f + LW + Grid * 3.f;
		P.Box(L2, LY + LabelH * 0.5f, 20.f, SonarLowerPx, MachineAmber(SonarLowerAlpha * SA));
		P.Text(FString::Printf(TEXT("%.0f m BELOW"), S.LowerBelowM), L2 + 28.f, LY, LabelFont, Secondary(0.6f * SA));
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

	// ------------------------------------------------ the controls hint, top left, until the controls are used once
	if (const float HA = HintFade.Alpha(); HA > 0.f && HintRows.Num() > 0)
	{
		float Y = Margin;
		const float X = Margin - SlideOf(HA);
		P.Text(TEXT("CONTROLS"), X, Y, LabelFont, Secondary(0.6f * HA));
		Y += LabelH + Grid;
		for (const FDeepHudPrompt& H : HintRows)
		{
			P.PromptRow(H, X, Y, HA);
			Y += 44.f + Grid * 0.5f;
		}
	}

	// ------------------------------------------------ full-screen cards: dead, settlement, review, pause
	// One card at a time: moving between cards (pause -> settings) swaps the card and keeps the dim behind it.
	const uint32 AllCards = EDeepHudPanel::DeadCard | EDeepHudPanel::SettlementCard | EDeepHudPanel::ReviewCard | EDeepHudPanel::PauseCard
		| EDeepHudPanel::SettingsCard | EDeepHudPanel::CreditsCard | EDeepHudPanel::ConfirmCard;
	float LeavingCards = 0.f;
	for (uint32 Bit = EDeepHudPanel::DeadCard; Bit <= EDeepHudPanel::ConfirmCard; Bit <<= 1)
	{
		if ((AllCards & Bit) && !(Frame.Panels & Bit))
		{
			LeavingCards = FMath::Max(LeavingCards, A(Bit));
		}
	}
	auto Card = [&](uint32 Which, float Width)
	{
		const float CA = A(Which);
		if (CA <= 0.f || (!(Frame.Panels & Which) && (Frame.Panels & AllCards)))
		{
			return;
		}
		const float Since = static_cast<float>(Now - ModeStart);
		const float Count = EaseOutCubic(Since / NumberTween);
		P.Box(0.f, 0.f, L.X, L.Y, PanelFill((Frame.Panels & Which) ? FMath::Max(CA, LeavingCards) : CA));
		// Height from the same steps the layout uses.
		const float RowH = 40.f;
		// Spec A: the title in the heading face (wrapped if long); Net is the one 72 px number, unless the card has its own.
		const FSlateFontInfo HeadingFont = Font(EFace::Heading);
		const float HeadH = P.TextSize(TEXT("A"), HeadingFont).Y;
		const TArray<FString> Title = P.Wrap(Frame.CardTitle.ToUpper(), HeadingFont, Width - Pad * 2.f);
		const float TitleH = FMath::Max(1, Title.Num()) * HeadH + Grid * 2.f;
		const FSlateFontInfo& BigFont = Frame.CardBig.IsEmpty() ? DepthFont : ReadoutFont;
		const float BigTH = P.TextSize(TEXT("0"), BigFont).Y;
		const float BigRowH = BigTH + Grid;
		float H = Pad * 2.f + TitleH;
		TArray<FString> Lead = P.Wrap(Frame.CardLead, BodyFont, Width - Pad * 2.f);
		H += Lead.Num() * BodyH + (Lead.Num() > 0 ? Grid * 2.f : 0.f);
		for (const FDeepHudRow& Row : Frame.Rows)
		{
			H += (Row.bBig ? BigRowH : RowH) + (Row.bRuleAbove ? Grid * 2.f : 0.f);
		}
		if (!Frame.CardBig.IsEmpty())
		{
			H += 136.f;
		}
		if (!Frame.CardFooter.IsEmpty())
		{
			H += 44.f + Grid * 3.f;
		}
		// The pause card keeps the full controls list of the mode the run was in (the calm HUD shows it once, then here).
		const TArray<FDeepHudPrompt>& Controls = Which == EDeepHudPanel::PauseCard ? Frame.Hint : TArray<FDeepHudPrompt>();
		const float ControlRowH = 30.f;
		if (Controls.Num() > 0)
		{
			H += LabelH + Grid + Controls.Num() * ControlRowH + Grid * 2.f;
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
		for (const FString& Line : Title)
		{
			P.Text(Line, X + Pad, CY, HeadingFont, Primary(CA));
			CY += HeadH;
		}
		CY = Y + Pad + TitleH;
		P.Box(X + Pad, CY - Grid, (Width - Pad * 2.f) * Count, P.Hair, Primary(HairlineAlpha * CA));
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
				P.Box(X + Pad, CY + Grid * 0.5f, Width - Pad * 2.f, P.Hair, Secondary(0.34f * CA));
				CY += Grid * 2.f;
			}
			const FSlateFontInfo& RowFont = Row.bBig ? BigFont : BodyFont;
			const FLinearColor C = Row.bBig && Row.Tone == EDeepHudTone::Normal ? Primary(CA) : ToneColor(Row.Tone, CA);
			const float RH = Row.bBig ? BigRowH : RowH;
			const float TH = P.TextSize(TEXT("A"), RowFont).Y;
			// A big row's label shares the number's baseline (Roboto's ascent is 0.79 of its line).
			const float LabelY = Row.bBig ? CY + (RH - TH) * 0.5f + 0.79f * (TH - BodyH) : CY + (RH - BodyH) * 0.5f;
			P.Text(Row.Label, X + Pad, LabelY, BodyFont, Row.bBig ? Secondary(CA) : C);
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
		if (Controls.Num() > 0)
		{
			P.Text(TEXT("CONTROLS"), X + Pad, CY, LabelFont, Secondary(0.6f * CA));
			CY += LabelH + Grid;
			for (const FDeepHudPrompt& Row : Controls)
			{
				P.Text(Row.Key, X + Pad, CY, MonoFont, Primary(CA));
				P.Text(Row.Verb, X + Pad + 136.f, CY, LabelFont, Secondary(CA));
				if (!Row.Detail.IsEmpty())
				{
					P.TextRight(Row.Detail, X + Width - Pad, CY, LabelFont, Secondary(0.5f * CA));
				}
				CY += ControlRowH;
			}
			CY += Grid * 2.f;
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
				// Settings: the value at the right, and a bar before it for the sliders.
				if (Frame.MenuValues.IsValidIndex(Index) && !Frame.MenuValues[Index].IsEmpty())
				{
					const float Right = X + Width - Pad - Grid * 2.f;
					P.TextRight(Frame.MenuValues[Index], Right, CY + (40.f - LabelH) * 0.5f, MonoFont, bOn ? Primary(IA) : Secondary(IA));
					if (Frame.MenuBars.IsValidIndex(Index) && Frame.MenuBars[Index] >= 0.f)
					{
						const float BarW = 160.f;
						const float BarX = Right - 64.f - Grid * 2.f - BarW;
						const float BarY = CY + 20.f - 1.f;
						P.Box(BarX, BarY, BarW, 2.f, Secondary(0.18f * IA));
						P.Box(BarX, BarY, BarW * FMath::Clamp(Frame.MenuBars[Index], 0.f, 1.f), 2.f, bOn ? Primary(IA) : Secondary(0.7f * IA));
					}
				}
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
	Card(EDeepHudPanel::PauseCard, Frame.Hint.Num() > 0 ? 600.f : 480.f);
	Card(EDeepHudPanel::SettingsCard, 720.f);
	Card(EDeepHudPanel::CreditsCard, 720.f);
	Card(EDeepHudPanel::ConfirmCard, 480.f);

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
		const float MaxW = DeepHudLayout::SubtitleMax(L, 1).W;
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
		P.Box(X, Y, 2.f, H, MachineAmber(SA));
		float CY = Y + Pad;
		// The name tag, and three squelch bars that read as a radio.
		const float TagW = P.Text(TEXT("CAPTAIN"), X + Pad + Grid, CY, LabelFont, MachineAmber(SA));
		for (int32 Bar = 0; Bar < 3; ++Bar)
		{
			const float BH = 4.f + Bar * 4.f;
			P.Box(X + Pad + Grid + TagW + Grid * 1.5f + Bar * 6.f, CY + LabelH - BH - 3.f, 3.f, BH, MachineAmber(0.7f * SA));
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
