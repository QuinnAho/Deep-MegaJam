#include "Game/DeepHUD.h"

#include "Camera/PlayerCameraManager.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Diver/DeepTetherComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "Game/DeepGameState.h"
#include "Game/DeepPlayerController.h"
#include "Harpoon/DeepHarpoonHUD.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Sonar/DeepSonarComponent.h"
#include "Story/DeepCaptain.h"
#include "World/DeepSalvage.h"
#include "World/DeepShaft.h"
#include "World/DeepSubmarine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "HAL/IConsoleManager.h"
#include "Harpoon/DeepHarpoonComponent.h"
#include "Harpoon/DeepHarpoonSettings.h"
#include "UI/DeepHudFrame.h"
#include "UI/DeepUserSettings.h"
#include "UI/SDeepHud.h"

static TAutoConsoleVariable<int32> CVarDeepHudLegacy(
	TEXT("Deep.HUD.Legacy"), 0, TEXT("1 = draw the old canvas HUD instead of the Slate HUD (debugging)."), ECVF_Default);

namespace
{
	const FLinearColor TextColor(0.85f, 0.95f, 1.f);
	const FLinearColor DimColor(0.6f, 0.7f, 0.75f, 0.85f);
	const FLinearColor WarnColor(1.f, 0.6f, 0.15f);
	const FLinearColor DangerColor(1.f, 0.2f, 0.15f);
	const FLinearColor GoodColor(0.35f, 1.f, 0.55f);
	const FLinearColor PanelColor(0.f, 0.02f, 0.04f, 0.85f);
	const FLinearColor PatienceColor(0.25f, 0.85f, 0.95f);
	const FLinearColor CaptainColor(1.f, 0.85f, 0.45f);
	constexpr float StatusScale = 0.8f;
	constexpr float RowScale = 0.85f;

	FString Credits(int32 Amount)
	{
		return FString::Printf(TEXT("%d cr"), Amount);
	}
}

ADeepHUD::ADeepHUD()
{
	Sonar = CreateDefaultSubobject<UDeepSonarComponent>(TEXT("Sonar"));
}

UDeepSonarComponent* ADeepHUD::GetSonar() const
{
	return Sonar;
}

void ADeepHUD::DrawHUD()
{
	Super::DrawHUD();

	const bool bLegacy = CVarDeepHudLegacy.GetValueOnGameThread() != 0;
	ShowWidget(!bLegacy);
	if (bLegacy)
	{
		DrawLegacy();
		return;
	}
	if (!Widget.IsValid())
	{
		return;
	}
	const double RealNow = FPlatformTime::Seconds();
	const float Delta = LastRealTime < 0.0 ? 0.f : static_cast<float>(RealNow - LastRealTime);
	LastRealTime = RealNow;
	FDeepHudFrame Frame;
	GatherFrame(Frame);
	Widget->Advance(Frame, RealNow, Delta);
}

void ADeepHUD::ShowWidget(bool bShow)
{
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	APlayerController* Player = GetOwningPlayerController();
	if (bShow && !WidgetRoot.IsValid() && Viewport && Player && Player->GetLocalPlayer())
	{
		Widget = SNew(SDeepHud);
		WidgetRoot = Widget;
		// Under the menus and console; the DPI scaler of the player's layer maps 1080p units to the screen.
		Viewport->AddViewportWidgetForPlayer(Player->GetLocalPlayer(), WidgetRoot.ToSharedRef(), 10);
	}
	if (WidgetRoot.IsValid())
	{
		WidgetRoot->SetVisibility(bShow ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
	}
}

void ADeepHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	APlayerController* Player = GetOwningPlayerController();
	if (WidgetRoot.IsValid() && Viewport)
	{
		if (Player && Player->GetLocalPlayer())
		{
			Viewport->RemoveViewportWidgetForPlayer(Player->GetLocalPlayer(), WidgetRoot.ToSharedRef());
		}
		else
		{
			Viewport->RemoveViewportWidgetContent(WidgetRoot.ToSharedRef());
		}
	}
	WidgetRoot.Reset();
	Widget.Reset();
	Super::EndPlay(EndPlayReason);
}

void ADeepHUD::DrawLegacy()
{
	if (!Canvas || !GEngine)
	{
		return;
	}
	Font = GEngine->GetMediumFont();
	BigFont = GEngine->GetLargeFont();
	// Large on purpose: read at a glance at 1280x720 (playtest: the first HUD was too small).
	S = FMath::Max(1.2f, 1.7f * Canvas->ClipY / 720.f);

	const ADeepGameState* GameState = GetWorld()->GetGameState<ADeepGameState>();
	if (GameState && GameState->bOnTitle)
	{
		DrawTitle(GetWorld()->GetRealTimeSeconds());
		return;
	}
	const ADeepDiver* Diver = Cast<ADeepDiver>(GetOwningPawn());
	if (!Diver)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const UDeepRunComponent* Run = Diver->GetRun();
	const EDeepRunState State = Run ? Run->GetState() : EDeepRunState::Tethered;

	const bool bTethered = State == EDeepRunState::Tethered;
	if (bTethered && !bWasTethered)
	{
		DiveStartTime = Now;
	}
	bWasTethered = bTethered;

	if (Run && Run->IsSeated())
	{
		DrawHelm(Diver);
	}
	else
	{
		DrawStatus(Diver, Now);
	}
	if (State == EDeepRunState::Aboard && Run && !Run->IsSeated())
	{
		DrawAboard(Diver);
	}
	if (State == EDeepRunState::Cut)
	{
		DrawBailout(Diver);
	}
	else if (Run && Run->CanSwim())
	{
		DrawPatience(*Run, Now);
	}
	if (Run && Run->CanSwim())
	{
		DrawThreat(*Run, GetWorld()->GetRealTimeSeconds());
		DrawPrompts(Diver);
		DeepHarpoonHUD::Draw(this, Canvas, Diver, S, Font, BigFont);
		if (Run->IsBiteAlarmOn())
		{
			DrawBiteAlarm(*Run, GetWorld()->GetRealTimeSeconds());
		}
	}

	if (State == EDeepRunState::Dead)
	{
		DrawDead(*Run);
	}
	else if (State == EDeepRunState::Settling)
	{
		DrawSettlement(*Run);
	}
	else if (State == EDeepRunState::Won)
	{
		DrawReview(*Run);
	}

	// The first dive of a run: a big controls card until the first grab (or for ControlsCardSeconds).
	const float CardSeconds = UDeepTuningSettings::Get().Run.ControlsCardSeconds;
	const float DiveSeconds = Now - DiveStartTime;
	if (GameState && State == EDeepRunState::Tethered && GameState->DiverNumber == 1 && GameState->Stop == 1 && GameState->TotalGross == 0
		&& Diver->GetCargo().IsEmpty() && DiveSeconds < CardSeconds)
	{
		DrawControlsCard(FMath::Clamp((CardSeconds - DiveSeconds) / 1.5f, 0.f, 1.f));
	}
	// Last, so the Captain is never hidden behind a panel. The review shows the Captain's win line itself.
	if (State != EDeepRunState::Won)
	{
		DrawCaptain();
	}

	const ADeepPlayerController* Player = Cast<ADeepPlayerController>(GetOwningPlayerController());
	if (Player && Player->IsPauseMenuOpen())
	{
		DrawPauseMenu();
	}
}

float ADeepHUD::DrawMenu(const TArray<FString>& Items, int32 Selected, float Y, bool bEnabled)
{
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		const bool bOn = Index == Selected;
		const FString Label = bOn ? FString::Printf(TEXT(">  %s  <"), *Items[Index]) : Items[Index];
		const FLinearColor Color = !bEnabled ? DimColor : (bOn ? CaptainColor : TextColor);
		Y += TextCentered(Label, Color, Y, BigFont, bOn ? 1.05f : 0.9f) + 8.f * S;
	}
	return Y;
}

void ADeepHUD::DrawTitle(float RealNow)
{
	DrawRect(FLinearColor(0.f, 0.015f, 0.03f, 1.f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	// A thin tether line down the middle of the screen, into the dark.
	DrawRect(FLinearColor(0.45f, 0.36f, 0.08f, 0.8f), Canvas->ClipX * 0.5f - 1.f * S, 0.f, 2.f * S, Canvas->ClipY * 0.16f);

	float W = 0.f, H = 0.f;
	const float TitleScale = 4.2f;
	GetTextSize(TEXT("DEEP"), W, H, BigFont, S * TitleScale);
	DrawText(TEXT("DEEP"), TextColor, (Canvas->ClipX - W) * 0.5f, Canvas->ClipY * 0.17f, BigFont, S * TitleScale);

	const UDeepCaptain* Captain = UDeepCaptain::Get(this);
	FString Line = Captain ? Captain->GetLastText() : FString();
	if (Line.IsEmpty())
	{
		Line = TEXT("Press any key. Any key.");
	}
	TextCentered(FString::Printf(TEXT("CAPTAIN: %s"), *Line), CaptainColor, Canvas->ClipY * 0.60f, nullptr, 1.f);

	FLinearColor Prompt = TextColor;
	Prompt.A = 0.8f + 0.2f * FMath::Sin(RealNow * 3.f);
	TextCentered(TEXT("PRESS ANY KEY OR CLICK TO START"), Prompt, Canvas->ClipY * 0.76f, BigFont, 1.2f);
	TextCentered(TEXT("A jam game. Salvage, a tether, and a Captain who hates you."), DimColor, Canvas->ClipY * 0.90f, nullptr, 0.7f);
}

void ADeepHUD::DrawControlsCard(float Alpha)
{
	if (Alpha <= 0.f)
	{
		return;
	}
	auto Key = [](const TCHAR* Action) { return ADeepPlayerController::KeyHint(Action); };
	const TArray<FString> Lines = {
		TEXT("W A S D  swim        Mouse  look"),
		FString::Printf(TEXT("%s / %s  up / down        %s  sprint"), *Key(TEXT("Up")), *Key(TEXT("Down")), *Key(TEXT("Sprint"))),
		FString::Printf(TEXT("%s  grab        %s  drop        Left click  harpoon (1 per dive)"), *Key(TEXT("Grab")), *Key(TEXT("Drop"))),
		FString::Printf(TEXT("Hold %s  winch home (loud)        %s  lamp (needs the flashlight)"), *Key(TEXT("Winch")), *Key(TEXT("Lamp"))),
		FString::Printf(TEXT("Hold %s  ask to be cut loose        %s  menu"), *Key(TEXT("Cut")), *Key(TEXT("Menu"))),
	};
	const FString GoalA = TEXT("Grab salvage below. Swim back up through");
	const FString GoalB = TEXT("the hatch and bank it at the console.");
	const float Line = 21.f * S;
	float Widest = 0.f;
	for (const FString& Text : Lines)
	{
		float W = 0.f, H = 0.f;
		GetTextSize(Text, W, H, Font, S * 0.85f);
		Widest = FMath::Max(Widest, W);
	}
	for (const FString& Text : { GoalA, GoalB })
	{
		float W = 0.f, H = 0.f;
		GetTextSize(Text, W, H, Font, S * 0.8f);
		Widest = FMath::Max(Widest, W);
	}
	const float W = Widest + 48.f * S;
	const float H = (Lines.Num() + 4.2f) * Line;
	// On the right, so the salvage below the start view stays in sight.
	const float X = Canvas->ClipX - W - 16.f * S;
	const float Top = Canvas->ClipY * 0.2f;
	DrawRect(FLinearColor(0.f, 0.01f, 0.02f, 0.88f * Alpha), X, Top, W, H);
	DrawRect(FLinearColor(CaptainColor.R, CaptainColor.G, CaptainColor.B, Alpha), X, Top, W, 3.f * S);
	float Y = Top + 10.f * S;
	FLinearColor Head = CaptainColor;
	Head.A = Alpha;
	FLinearColor Body = TextColor;
	Body.A = Alpha;
	TextAt(TEXT("CONTROLS"), Head, X + 24.f * S, Y, nullptr, 0.95f);
	Y += Line * 1.3f;
	for (const FString& Text : Lines)
	{
		TextAt(Text, Body, X + 24.f * S, Y, nullptr, 0.85f);
		Y += Line;
	}
	FLinearColor Goal = GoodColor;
	Goal.A = Alpha;
	TextAt(GoalA, Goal, X + 24.f * S, Y + 4.f * S, nullptr, 0.8f);
	TextAt(GoalB, Goal, X + 24.f * S, Y + 4.f * S + Line, nullptr, 0.8f);
}

void ADeepHUD::DrawPauseMenu()
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.7f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	float Y = Canvas->ClipY * 0.3f;
	Y += TextCentered(TEXT("PAUSED"), CaptainColor, Y, BigFont, 1.6f) + 24.f * S;
	TArray<FString> Labels;
	for (const ADeepPlayerController::EMenuItem Item : ADeepPlayerController::PauseItems())
	{
		Labels.Add(ADeepPlayerController::MenuItemLabel(Item));
	}
	const ADeepPlayerController* Player = Cast<ADeepPlayerController>(GetOwningPlayerController());
	Y = DrawMenu(Labels, Player ? Player->GetMenuIndex() : 0, Y);
	TextCentered(TEXT("Up / Down to choose,  Enter to select,  Esc to resume"), DimColor, Y + 16.f * S, nullptr, 0.75f);
}

void ADeepHUD::DrawReview(const UDeepRunComponent& Run)
{
	const ADeepGameState* State = GetWorld()->GetGameState<ADeepGameState>();
	if (!State)
	{
		return;
	}
	const FDeepRunReview Review = State->BuildReview();
	DrawRect(FLinearColor(0.f, 0.01f, 0.02f, 0.94f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	const float W = FMath::Min(Canvas->ClipX * 0.7f, 640.f * S);
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Line = 21.f * S;
	const float Pad = 22.f * S;
	float Y = 14.f * S;
	Y += TextCentered(TEXT("RELIC BANKED.  PERFORMANCE REVIEW"), CaptainColor, Y, BigFont, 1.25f) + 6.f * S;
	if (const UDeepCaptain* Captain = UDeepCaptain::Get(this); Captain && !Captain->GetLastText().IsEmpty())
	{
		Y += TextCentered(FString::Printf(TEXT("CAPTAIN: %s"), *Captain->GetLastText()), CaptainColor, Y, nullptr, 0.75f) + 8.f * S;
	}
	DrawRect(CaptainColor, X, Y, W, 2.f * S);
	Y += 10.f * S;
	auto Row = [&](const FString& Label, const FString& Amount, const FLinearColor& Color)
	{
		TextAt(Label, Color, X + Pad, Y, nullptr, RowScale);
		float AW = 0.f, AH = 0.f;
		GetTextSize(Amount, AW, AH, Font, S * RowScale);
		TextAt(Amount, Color, X + W - Pad - AW, Y, nullptr, RowScale);
		Y += Line;
	};
	const int32 Minutes = FMath::FloorToInt(Review.Seconds / 60.f);
	const int32 Seconds = FMath::FloorToInt(Review.Seconds) % 60;
	Row(TEXT("Divers expended"), FString::Printf(TEXT("%d"), Review.DiversExpended), Review.DiversExpended > 0 ? WarnColor : TextColor);
	Row(TEXT("Total gross"), Credits(Review.Gross), TextColor);
	Row(TEXT("The Captain's cut, in total"), FString::Printf(TEXT("-%d cr"), Review.DirectorCut), WarnColor);
	Row(TEXT("Fees, in total"), FString::Printf(TEXT("-%d cr"), Review.Fees), DimColor);
	DrawRect(DimColor, X + Pad, Y + 2.f * S, W - Pad * 2.f, 1.f * S);
	Y += 8.f * S;
	Row(TEXT("Kept by the diver"), Credits(Review.Kept), Review.Kept > 0 ? GoodColor : DangerColor);
	Row(TEXT("Time taken"), FString::Printf(TEXT("%d:%02d"), Minutes, Seconds), TextColor);
	Y += 10.f * S;
	Y += TextCentered(TEXT("THE CAPTAIN'S RATING"), DimColor, Y, nullptr, 0.8f) + 2.f * S;
	Y += TextCentered(Review.Rating, CaptainColor, Y, BigFont, 2.0f) + 14.f * S;

	TArray<FString> Labels;
	for (const ADeepPlayerController::EMenuItem Item : ADeepPlayerController::WinItems())
	{
		Labels.Add(ADeepPlayerController::MenuItemLabel(Item));
	}
	const ADeepPlayerController* Player = Cast<ADeepPlayerController>(GetOwningPlayerController());
	Y = DrawMenu(Labels, Player ? Player->GetMenuIndex() : 0, Y, Run.CanLeaveReview());
	TextCentered(TEXT("Up / Down to choose,  Enter to select"), DimColor, Y + 4.f * S, nullptr, 0.7f);
}

void ADeepHUD::TextAt(const FString& String, const FLinearColor& Color, float X, float Y, UFont* InFont, float TextScale)
{
	DrawText(String, Color, X, Y, InFont ? InFont : Font.Get(), S * TextScale);
}

float ADeepHUD::TextCentered(const FString& String, const FLinearColor& Color, float Y, UFont* InFont, float TextScale)
{
	UFont* Use = InFont ? InFont : Font.Get();
	float W = 0.f, H = 0.f;
	GetTextSize(String, W, H, Use, S * TextScale);
	// A dark backing so text reads against bright fog and light shafts.
	DrawRect(PanelColor, (Canvas->ClipX - W) * 0.5f - 8.f * S, Y - 1.f * S, W + 16.f * S, H + 2.f * S);
	DrawText(String, Color, (Canvas->ClipX - W) * 0.5f, Y, Use, S * TextScale);
	return H;
}

void ADeepHUD::DrawStatus(const ADeepDiver* Diver, float Now)
{
	const float Line = 18.f * S;
	const float X = 14.f * S;
	TArray<TPair<FString, FLinearColor>> Lines;
	auto Text = [&Lines](const FString& String, const FLinearColor& Color)
	{
		Lines.Emplace(String, Color);
	};
	ON_SCOPE_EXIT
	{
		float Widest = 0.f;
		for (const TPair<FString, FLinearColor>& Entry : Lines)
		{
			float W = 0.f, H = 0.f;
			GetTextSize(Entry.Key, W, H, Font, S * StatusScale);
			Widest = FMath::Max(Widest, W);
		}
		DrawRect(PanelColor, X - 10.f * S, 14.f * S, Widest + 20.f * S, Lines.Num() * Line + 10.f * S);
		float Y = 20.f * S;
		for (const TPair<FString, FLinearColor>& Entry : Lines)
		{
			TextAt(Entry.Key, Entry.Value, X, Y, nullptr, StatusScale);
			Y += Line;
		}
	};

	const UDeepRunComponent* RunForDepth = Diver->GetRun();
	const ADeepGameState* SubState = GetWorld()->GetGameState<ADeepGameState>();
	if (RunForDepth && RunForDepth->IsInside() && SubState)
	{
		Text(FString::Printf(TEXT("ABOARD   SUB AT %.0f m   %s"), SubState->SubDepthM, SubState->bParked ? TEXT("PARKED") : TEXT("BETWEEN STOPS")), TextColor);
	}
	else
	{
		Text(FString::Printf(TEXT("DEPTH  %.0f m   %s"), Diver->GetDepthM(), *Diver->GetZoneName().ToUpper()), TextColor);
	}
	const FDeepWeightResult Weight = Diver->GetWeight();
	Text(FString::Printf(TEXT("CARGO  %.0f kg   %d cr%s"), Weight.CarriedKg, Diver->GetCargoValue(), Weight.bOverloaded ? TEXT("   TOO HEAVY") : TEXT("")), Weight.bOverloaded ? WarnColor : TextColor);

	const UDeepRunComponent* Run = Diver->GetRun();
	const UDeepTetherComponent* Tether = Diver->GetTether();
	if (Run && Run->IsInside())
	{
	}
	else if (Run && !Run->HasTether())
	{
		Text(TEXT("TETHER  CUT"), DangerColor);
	}
	else if (Tether && Tether->HasAnchor())
	{
		const FDeepTetherSolve& Solve = Tether->GetSolve();
		const FLinearColor Color = Solve.bTaut ? DangerColor : (Solve.Strain > 0.75f ? WarnColor : TextColor);
		Text(FString::Printf(TEXT("TETHER %.0f m of %.0f m%s"), Solve.LeftM, Solve.LengthM, Solve.bTaut ? TEXT("  TAUT") : TEXT("")), Color);
	}

	if (const ADeepGameState* State = GetWorld()->GetGameState<ADeepGameState>())
	{
		const FDeepEconomyTuning& Economy = UDeepTuningSettings::Get().Economy;
		Text(FString::Printf(TEXT("CREDITS %d   DIVER #%d"), State->Credits, State->DiverNumber), TextColor);
		Text(State->HasFlashlight() ? FString(TEXT("FLASHLIGHT  yes")) : FString::Printf(TEXT("FLASHLIGHT  no  (%d cr)"), Economy.HullPrices.IsValidIndex(0) ? Economy.HullPrices[0] : 0),
			State->HasFlashlight() ? GoodColor : WarnColor);
		Text(State->IsHullUpgraded() ? FString(TEXT("HULL  upgraded")) : FString::Printf(TEXT("HULL  standard  (upgrade %d cr)"), Economy.HullPrices.IsValidIndex(1) ? Economy.HullPrices[1] : 0),
			State->IsHullUpgraded() ? GoodColor : TextColor);
		Text(FString::Printf(TEXT("SUB LIMIT  %.0f m"), State->DepthLimitM()), TextColor);
	}
	// A fee just taken (re-tether, death): show it next to the account for a moment.
	if (Run && Run->GetLastFee() > 0 && Now - Run->GetLastFeeTime() < 3.f)
	{
		Text(FString::Printf(TEXT("-%d cr FEE"), Run->GetLastFee()), DangerColor);
	}
}

void ADeepHUD::DrawPatience(const UDeepRunComponent& Run, float Now)
{
	const FDeepPatienceTuning& Tuning = UDeepTuningSettings::Get().Patience;
	const float Fraction = Run.GetPatienceFraction();
	const float W = Canvas->ClipX * 0.34f;
	const float H = 16.f * S;
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = 46.f * S;

	FLinearColor Color = PatienceColor;
	if (Fraction <= 0.1f)
	{
		// Last tenth: the bar flashes.
		Color = FMath::Frac(Now * 3.f) < 0.5f ? DangerColor : FLinearColor(0.5f, 0.05f, 0.05f);
	}
	else if (Fraction <= 0.25f)
	{
		Color = DangerColor;
	}
	else if (Fraction <= 0.5f)
	{
		Color = WarnColor;
	}

	const bool bActive = Run.GetState() == EDeepRunState::Tethered;
	TextCentered(FString::Printf(TEXT("CAPTAIN'S PATIENCE  %d%%"), FMath::RoundToInt(Fraction * 100.f)), bActive ? Color : DimColor, 20.f * S, nullptr, 0.9f);
	DrawRect(PanelColor, X - 3.f * S, Y - 3.f * S, W + 6.f * S, H + 6.f * S);
	DrawRect(Color, X, Y, W * Fraction, H);
	for (const float Mark : Tuning.WarningFractions)
	{
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.7f), X + W * Mark - 1.f * S, Y - 3.f * S, 2.f * S, H + 6.f * S);
	}

	// A pickup refilled it: "+10%" beside the bar for a moment.
	const float SinceRefill = Now - Run.GetLastRefillTime();
	if (SinceRefill >= 0.f && SinceRefill < 1.8f && Run.GetLastRefillFraction() > 0.f)
	{
		const float Alpha = 1.f - SinceRefill / 1.8f;
		FLinearColor Flash = GoodColor;
		Flash.A = Alpha;
		TextAt(FString::Printf(TEXT("+%d%%"), FMath::RoundToInt(Run.GetLastRefillFraction() * 100.f)), Flash, X + W + 12.f * S, Y - 6.f * S - SinceRefill * 10.f * S);
	}
}

void ADeepHUD::DrawBailout(const ADeepDiver* Diver)
{
	const UDeepRunComponent* Run = Diver->GetRun();
	const FDeepBailoutTuning& Tuning = UDeepTuningSettings::Get().Bailout;
	const float Air = Run ? Run->GetBailoutAirSeconds() : 0.f;
	const float RealNow = GetWorld()->GetRealTimeSeconds();
	const bool bSlow = Air <= Tuning.SlowWindowSeconds;
	const bool bPulse = Air <= Tuning.WarnSeconds && FMath::Frac(RealNow * (bSlow ? 1.5f : 2.5f)) < 0.5f;

	TextCentered(TEXT("TETHER CUT  -  SWIM UP INTO THE HATCH"), DangerColor, 14.f * S, nullptr, 1.f);
	TextCentered(FString::Printf(TEXT("BAILOUT AIR  %.1f s"), Air), bPulse ? FLinearColor::White : DangerColor, 38.f * S, BigFont, 1.6f);

	const float W = Canvas->ClipX * 0.34f;
	const float H = 12.f * S;
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = 100.f * S;
	const float Fraction = Tuning.AirSeconds > 0.f ? FMath::Clamp(Air / Tuning.AirSeconds, 0.f, 1.f) : 0.f;
	DrawRect(PanelColor, X - 3.f * S, Y - 3.f * S, W + 6.f * S, H + 6.f * S);
	DrawRect(DangerColor, X, Y, W * Fraction, H);

	DrawAirlockMarker(Diver, 124.f * S);
}

void ADeepHUD::DrawAirlockMarker(const ADeepDiver* Diver, float HintY)
{
	// Where is home: a marker on the airlock, or an edge hint when it is off screen.
	const UDeepTetherComponent* Tether = Diver->GetTether();
	const APlayerController* Player = GetOwningPlayerController();
	if (!Tether || !Tether->HasAnchor() || !Player || !Player->PlayerCameraManager)
	{
		return;
	}
	const FVector Anchor = Tether->GetAnchorLocation();
	const float DistanceM = static_cast<float>(FVector::Dist(Anchor, Diver->GetActorLocation())) / UDeepRules::CmPerM;
	const FVector CameraLocation = Player->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = Player->PlayerCameraManager->GetCameraRotation().Vector();
	const FString Label = FString::Printf(TEXT("HATCH  %.0f m"), DistanceM);
	const FVector Screen = Canvas->Project(Anchor);
	const bool bInFront = FVector::DotProduct(Anchor - CameraLocation, CameraForward) > 0.0;
	if (bInFront && Screen.X >= 0.f && Screen.X <= Canvas->ClipX && Screen.Y >= 0.f && Screen.Y <= Canvas->ClipY)
	{
		const float Box = 18.f * S;
		DrawRect(GoodColor, Screen.X - Box, Screen.Y - Box, Box * 2.f, 2.f * S);
		DrawRect(GoodColor, Screen.X - Box, Screen.Y + Box, Box * 2.f, 2.f * S);
		DrawRect(GoodColor, Screen.X - Box, Screen.Y - Box, 2.f * S, Box * 2.f);
		DrawRect(GoodColor, Screen.X + Box, Screen.Y - Box, 2.f * S, Box * 2.f + 2.f * S);
		float TW = 0.f, TH = 0.f;
		GetTextSize(Label, TW, TH, Font, S);
		TextAt(Label, GoodColor, Screen.X - TW * 0.5f, Screen.Y + Box + 4.f * S);
	}
	else
	{
		const FVector Local = Player->PlayerCameraManager->GetCameraRotation().UnrotateVector(Anchor - CameraLocation);
		const TCHAR* Hint = Local.Z > FMath::Abs(Local.Y) ? TEXT("UP") : (Local.Y > 0.0 ? TEXT("RIGHT") : TEXT("LEFT"));
		if (Local.X < 0.0 && FMath::Abs(Local.Z) < FMath::Abs(Local.X))
		{
			Hint = TEXT("BEHIND YOU");
		}
		TextCentered(FString::Printf(TEXT("%s  -  %s"), *Label, Hint), GoodColor, HintY);
	}
}

void ADeepHUD::DrawPrompts(const ADeepDiver* Diver)
{
	TArray<TPair<FString, FLinearColor>> Prompts;
	const UDeepRunComponent* Run = Diver->GetRun();
	const bool bTethered = Run && Run->GetState() == EDeepRunState::Tethered;
	if (const ADeepSalvage* Target = Diver->FindGrabTarget())
	{
		const FDeepSalvageKind& Kind = Target->GetKind();
		Prompts.Emplace(FString::Printf(TEXT("[%s] Grab %s   %.0f kg   %d cr"), *ADeepPlayerController::KeyHint(TEXT("Grab")), *Kind.Name, Kind.Kg, Kind.Value), TextColor);
	}
	const FDeepWeightResult Weight = Diver->GetWeight();
	if (Diver->IsWinching())
	{
		Prompts.Emplace(TEXT("WINCHING"), WarnColor);
	}
	else if (bTethered && Weight.bOverloaded)
	{
		Prompts.Emplace(FString::Printf(TEXT("Too heavy to swim up. Hold [%s] to winch home"), *ADeepPlayerController::KeyHint(TEXT("Winch"))), WarnColor);
	}
	else if (bTethered && Diver->GetTether() && Diver->GetTether()->GetSolve().bTaut)
	{
		Prompts.Emplace(FString::Printf(TEXT("End of the line. Hold [%s] to winch home, hold [%s] to be cut loose"), *ADeepPlayerController::KeyHint(TEXT("Winch")), *ADeepPlayerController::KeyHint(TEXT("Cut"))), WarnColor);
	}
	if (bTethered && Diver->GetCargo().Num() > 0 && !Diver->IsWinching())
	{
		Prompts.Emplace(TEXT("Swim up into the hatch, then bank the cargo at the console"), DimColor);
		// First-time players lose the sub: mark the hatch while carrying (below the Patience bar).
		DrawAirlockMarker(Diver, 76.f * S);
	}

	float PromptY = Canvas->ClipY * 0.58f;
	for (const TPair<FString, FLinearColor>& Prompt : Prompts)
	{
		PromptY += TextCentered(Prompt.Key, Prompt.Value, PromptY) + 2.f * S;
	}

	// Centre dot, and the cut-loose hold progress under it.
	const float Dot = 3.f * S;
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.6f), Canvas->ClipX * 0.5f - Dot * 0.5f, Canvas->ClipY * 0.5f - Dot * 0.5f, Dot, Dot);
	const float Hold = Diver->GetCutHoldFraction();
	if (Hold > 0.f)
	{
		const float W = 220.f * S;
		const float X = (Canvas->ClipX - W) * 0.5f;
		const float Y = Canvas->ClipY * 0.5f + 30.f * S;
		TextCentered(TEXT("ASKING TO BE CUT LOOSE..."), DangerColor, Y - 26.f * S);
		DrawRect(PanelColor, X, Y, W, 10.f * S);
		DrawRect(DangerColor, X, Y, W * Hold, 10.f * S);
	}

	// Unmissable warnings: one big banner, the most serious first.
	FString Banner;
	FLinearColor BannerColor = DangerColor;
	if (bTethered && Diver->GetTether() && Diver->GetTether()->GetSolve().bTaut && !Diver->IsWinching())
	{
		Banner = TEXT("TETHER TAUT");
	}
	else if (bTethered && Run->GetPatienceFraction() <= 0.1f)
	{
		Banner = TEXT("THE CAPTAIN IS ABOUT TO CUT YOU");
	}
	else if (bTethered && Weight.bOverloaded && !Diver->IsWinching())
	{
		Banner = TEXT("TOO HEAVY TO SWIM UP");
		BannerColor = WarnColor;
	}
	if (!Banner.IsEmpty())
	{
		const bool bFlash = FMath::Frac(GetWorld()->GetRealTimeSeconds() * 2.f) < 0.5f;
		TextCentered(Banner, bFlash ? FLinearColor::White : BannerColor, Canvas->ClipY * 0.28f, BigFont, 1.1f);
	}

	// Controls, bottom left, two backed lines.
	const FString ControlsA = FString::Printf(TEXT("WASD swim   Mouse look   %s/%s up/down   %s sprint   %s grab   %s drop"),
		*ADeepPlayerController::KeyHint(TEXT("Up")), *ADeepPlayerController::KeyHint(TEXT("Down")), *ADeepPlayerController::KeyHint(TEXT("Sprint")),
		*ADeepPlayerController::KeyHint(TEXT("Grab")), *ADeepPlayerController::KeyHint(TEXT("Drop")));
	// Only what is possible now: no winch and no cut once the tether is gone.
	const FString LampHint = Diver->HasFlashlight()
		? FString::Printf(TEXT("%s lamp"), *ADeepPlayerController::KeyHint(TEXT("Lamp")))
		: FString(TEXT("no lamp yet (flashlight at the console)"));
	const FString ControlsB = bTethered
		? FString::Printf(TEXT("hold %s winch home   %s   hold %s be cut loose   %s menu"),
			*ADeepPlayerController::KeyHint(TEXT("Winch")), *LampHint, *ADeepPlayerController::KeyHint(TEXT("Cut")),
			*ADeepPlayerController::KeyHint(TEXT("Menu")))
		: FString::Printf(TEXT("no tether: swim up into the hatch   %s   %s menu"),
			*LampHint, *ADeepPlayerController::KeyHint(TEXT("Menu")));
	constexpr float ControlsScale = 0.62f;
	float WA = 0.f, WB = 0.f, HA = 0.f;
	GetTextSize(ControlsA, WA, HA, Font, S * ControlsScale);
	GetTextSize(ControlsB, WB, HA, Font, S * ControlsScale);
	const float CX = 10.f * S;
	const float CY = Canvas->ClipY - HA * 2.f - 12.f * S;
	DrawRect(PanelColor, CX - 6.f * S, CY - 4.f * S, FMath::Max(WA, WB) + 12.f * S, HA * 2.f + 8.f * S);
	const FLinearColor ControlsColor(0.85f, 0.92f, 0.95f);
	TextAt(ControlsA, ControlsColor, CX, CY, nullptr, ControlsScale);
	TextAt(ControlsB, ControlsColor, CX, CY + HA, nullptr, ControlsScale);
}

void ADeepHUD::DrawThreat(const UDeepRunComponent& Run, float RealNow)
{
	const float Threat = FMath::Clamp(Run.GetMonsterThreat01(), 0.f, 1.f);
	if (Threat <= 0.02f)
	{
		return;
	}
	// Edge pulse: faint and slow when it is far, strong and fast when it is on you. Readable with the sound off.
	const float Hz = FMath::Lerp(0.6f, 3.f, Threat);
	const float Pulse = 0.5f + 0.5f * FMath::Sin(RealNow * Hz * 2.f * PI);
	const float Alpha = Threat * Threat * (0.25f + 0.45f * Pulse);
	const float Edge = FMath::Lerp(10.f, 46.f, Threat) * S;
	const FLinearColor Red(0.85f, 0.03f, 0.02f, Alpha);
	DrawRect(Red, 0.f, 0.f, Canvas->ClipX, Edge);
	DrawRect(Red, 0.f, Canvas->ClipY - Edge, Canvas->ClipX, Edge);
	DrawRect(Red, 0.f, Edge, Edge, Canvas->ClipY - Edge * 2.f);
	DrawRect(Red, Canvas->ClipX - Edge, Edge, Edge, Canvas->ClipY - Edge * 2.f);

	// A short bar on the right edge: SOMETHING NEAR, filling as it closes.
	const float W = 150.f * S;
	const float H = 10.f * S;
	const float X = Canvas->ClipX - W - 24.f * S;
	const float Y = 24.f * S;
	const FLinearColor BarColor = Threat >= 0.6f ? DangerColor : WarnColor;
	TextAt(Threat >= 0.6f ? TEXT("IT IS CLOSE") : TEXT("SOMETHING NEAR"), BarColor, X, Y - 18.f * S, nullptr, 0.7f);
	DrawRect(PanelColor, X - 3.f * S, Y - 3.f * S, W + 6.f * S, H + 6.f * S);
	DrawRect(BarColor, X, Y, W * Threat, H);
}

void ADeepHUD::DrawBiteAlarm(const UDeepRunComponent& Run, float RealNow)
{
	// Unmissable: a red wash, a full-width flashing band, the countdown and what to do.
	const bool bFlash = FMath::Frac(RealNow * 3.f) < 0.5f;
	DrawRect(FLinearColor(0.6f, 0.f, 0.f, bFlash ? 0.28f : 0.14f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	const float BandY = Canvas->ClipY * 0.24f;
	const float BandH = 72.f * S;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.85f), 0.f, BandY, Canvas->ClipX, BandH);
	DrawRect(DangerColor, 0.f, BandY, Canvas->ClipX, 4.f * S);
	DrawRect(DangerColor, 0.f, BandY + BandH - 4.f * S, Canvas->ClipX, 4.f * S);
	float Y = BandY + 10.f * S;
	Y += TextCentered(TEXT("IT HAS YOUR TETHER"), bFlash ? FLinearColor::White : DangerColor, Y, BigFont, 1.2f) + 4.f * S;
	TextCentered(FString::Printf(TEXT("LAMP ON IT  [%s]   -   CUT IN %.1f s"), *ADeepPlayerController::KeyHint(TEXT("Lamp")), Run.GetBiteAlarmLeft()), WarnColor, Y, nullptr, 1.1f);
}

void ADeepHUD::DrawCaptain()
{
	UDeepCaptain* Captain = UDeepCaptain::Get(this);
	if (!Captain)
	{
		return;
	}
	const FString Line = Captain->GetLine();
	if (Line.IsEmpty())
	{
		return;
	}
	// Word-wrap into at most 70% of the screen width.
	const float MaxW = Canvas->ClipX * 0.7f;
	TArray<FString> Words;
	Line.ParseIntoArray(Words, TEXT(" "));
	TArray<FString> Lines;
	FString Current;
	for (const FString& Word : Words)
	{
		const FString Try = Current.IsEmpty() ? Word : Current + TEXT(" ") + Word;
		float W = 0.f, H = 0.f;
		GetTextSize(Try, W, H, Font, S);
		if (W > MaxW && !Current.IsEmpty())
		{
			Lines.Add(Current);
			Current = Word;
		}
		else
		{
			Current = Try;
		}
	}
	Lines.Add(Current);

	const float LineH = 26.f * S;
	float WidestW = 0.f;
	for (const FString& Text : Lines)
	{
		float W = 0.f, H = 0.f;
		GetTextSize(Text, W, H, Font, S);
		WidestW = FMath::Max(WidestW, W);
	}
	const float Top = Canvas->ClipY * 0.80f - (Lines.Num() - 1) * LineH;
	DrawRect(PanelColor, (Canvas->ClipX - WidestW) * 0.5f - 14.f * S, Top - 6.f * S, WidestW + 28.f * S, Lines.Num() * LineH + 10.f * S);
	float Y = Top;
	for (const FString& Text : Lines)
	{
		TextCentered(Text, CaptainColor, Y, nullptr, 1.f);
		Y += LineH;
	}
}

void ADeepHUD::DrawDead(const UDeepRunComponent& Run)
{
	DrawRect(FLinearColor(0.12f, 0.f, 0.f, 0.82f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	const ADeepGameState* State = GetWorld()->GetGameState<ADeepGameState>();
	const int32 Number = State ? State->DiverNumber : 1;
	const FDeepEconomyTuning& Economy = UDeepTuningSettings::Get().Economy;
	float Y = Canvas->ClipY * 0.36f;
	Y += TextCentered(FString::Printf(TEXT("DIVER #%d WRITTEN OFF"), Number), DangerColor, Y, BigFont, 1.4f) + 10.f * S;
	Y += TextCentered(Run.WasLastDeathMonster() ? TEXT("Taken by the serpent. Cargo lost.") : TEXT("Bailout air ran out. Cargo lost."), TextColor, Y) + 4.f * S;
	Y += TextCentered(FString::Printf(TEXT("Disposal fee  -%d cr"), Economy.DeathFee), DangerColor, Y) + 18.f * S;
	TextCentered(FString::Printf(TEXT("Replacement diver #%d in %.0f..."), Number + 1, FMath::CeilToFloat(FMath::Max(0.f, Run.GetStateTimeLeft()))), DimColor, Y);
}

void ADeepHUD::DrawSettlement(const UDeepRunComponent& Run)
{
	const FDeepSettlementReport& Report = Run.GetSettlement();
	const FDeepEconomyTuning& Economy = UDeepTuningSettings::Get().Economy;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const float W = FMath::Min(Canvas->ClipX * 0.6f, 620.f * S);
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Line = 20.f * S;
	const int32 Rows = 9 + Report.FeeLabels.Num();
	const float H = Rows * Line + 40.f * S;
	const float Top = 8.f * S;
	DrawRect(FLinearColor(0.f, 0.01f, 0.02f, 0.9f), X, Top, W, H);
	DrawRect(CaptainColor, X, Top, W, 3.f * S);

	const float Pad = 22.f * S;
	float Y = Top + 14.f * S;
	auto Row = [&](const FString& Label, const FString& Amount, const FLinearColor& Color, UFont* RowFont = nullptr)
	{
		TextAt(Label, Color, X + Pad, Y, RowFont, RowScale);
		float AW = 0.f, AH = 0.f;
		GetTextSize(Amount, AW, AH, RowFont ? RowFont : Font.Get(), S * RowScale);
		TextAt(Amount, Color, X + W - Pad - AW, Y, RowFont, RowScale);
		Y += Line;
	};

	TextCentered(TEXT("SETTLEMENT"), CaptainColor, Y, BigFont, 1.2f);
	Y += Line * 1.6f;
	Row(TEXT("Gross haul"), Credits(Report.Gross), TextColor);
	Row(FString::Printf(TEXT("The Captain's cut (%d%%)"), Economy.DirectorCutPercent), FString::Printf(TEXT("-%d cr"), Report.DirectorCut), WarnColor);
	for (int32 Index = 0; Index < Report.FeeLabels.Num(); ++Index)
	{
		const int32 Amount = Report.FeeAmounts.IsValidIndex(Index) ? Report.FeeAmounts[Index] : 0;
		Row(Report.FeeLabels[Index], FString::Printf(TEXT("-%d cr"), Amount), DimColor);
	}
	DrawRect(DimColor, X + Pad, Y + 2.f * S, W - Pad * 2.f, 1.f * S);
	Y += 8.f * S;
	Row(TEXT("NET"), Credits(Report.Net), Report.Net > 0 ? GoodColor : DangerColor);
	Row(TEXT("Credits"), Credits(Report.CreditsAfter), TextColor);
	Y += 6.f * S;
	const auto GateName = [](int32 Rating) { return Rating <= 2 ? FString(TEXT("FLASHLIGHT")) : FString(TEXT("HULL UPGRADE")); };
	if (Report.bHullBought)
	{
		Row(FString::Printf(TEXT("%s BOUGHT"), *GateName(Report.HullRating)), FString::Printf(TEXT("-%d cr"), Report.HullSpent), GoodColor);
	}
	else if (Report.NextHullPrice >= 0)
	{
		Row(GateName(Report.HullRating + 1), FString::Printf(TEXT("needs %d cr"), Report.NextHullPrice), DimColor);
	}
	else
	{
		Row(TEXT("Flashlight and hull upgrade"), TEXT("owned"), DimColor);
	}

	Y += 10.f * S;
	const bool bReady = Run.GetStateTimeLeft() <= 0.f;
	const FString Action = FString::Printf(TEXT("[%s]  BACK TO THE DECK%s"), *ADeepPlayerController::KeyHint(TEXT("Continue")),
		Report.bHullBought ? TEXT("  -  THEN TAKE THE HELM TO GO DEEPER") : TEXT(""));
	TextCentered(Action, bReady ? CaptainColor : DimColor, Y, nullptr, 1.05f);
}

// ---------------------------------------------------------------- aboard the sub

namespace
{
	/** Name and price of the next layer gate, or empty when both are owned. */
	bool NextGate(const ADeepGameState* State, FString& OutName, int32& OutPrice)
	{
		if (!State)
		{
			return false;
		}
		OutPrice = UDeepRules::NextHullPrice(State->HullRating, UDeepTuningSettings::Get().Economy);
		if (OutPrice < 0)
		{
			return false;
		}
		OutName = State->HullRating <= 1 ? TEXT("FLASHLIGHT") : TEXT("HULL UPGRADE");
		return true;
	}

	/** Why the helm stops where it does, in one line. */
	FString LimitReason(const ADeepGameState* State)
	{
		if (!State)
		{
			return FString();
		}
		const FDeepEconomyTuning& Economy = UDeepTuningSettings::Get().Economy;
		const float Limit = State->DepthLimitM();
		if (!State->HasFlashlight())
		{
			return FString::Printf(TEXT("No flashlight: the helm stops at %.0f m. Bank %d cr at the console to buy one."), Limit, Economy.HullPrices.IsValidIndex(0) ? Economy.HullPrices[0] : 0);
		}
		if (!State->IsHullUpgraded())
		{
			return FString::Printf(TEXT("Standard hull: the helm stops at %.0f m. Bank %d cr at the console for the hull upgrade."), Limit, Economy.HullPrices.IsValidIndex(1) ? Economy.HullPrices[1] : 0);
		}
		return FString::Printf(TEXT("Hull upgraded: rated to %.0f m."), Limit);
	}

	/** The deepest stop the helm can reach now that the sub has never parked at, or 0. */
	int32 NewReachableStop(const ADeepGameState* State)
	{
		if (!State)
		{
			return 0;
		}
		const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
		int32 Best = 0;
		for (int32 Stop = 1; Stop <= Tuning.NumStops(); ++Stop)
		{
			if (Tuning.GetStop(Stop).SubDepthM <= State->DepthLimitM() + 0.01f && Stop > State->DeepestStopReached)
			{
				Best = Stop;
			}
		}
		return Best;
	}
}

bool ADeepHUD::DrawMarker(const FVector& World, const FString& Label, const FLinearColor& Color)
{
	const APlayerController* Player = GetOwningPlayerController();
	if (!Player || !Player->PlayerCameraManager)
	{
		return false;
	}
	const FVector CameraLocation = Player->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = Player->PlayerCameraManager->GetCameraRotation().Vector();
	if (FVector::DotProduct(World - CameraLocation, CameraForward) <= 0.0)
	{
		return false;
	}
	const FVector Screen = Canvas->Project(World);
	if (Screen.X < 0.f || Screen.X > Canvas->ClipX || Screen.Y < 0.f || Screen.Y > Canvas->ClipY)
	{
		return false;
	}
	float TW = 0.f, TH = 0.f;
	GetTextSize(Label, TW, TH, Font, S * 0.85f);
	DrawRect(PanelColor, Screen.X - TW * 0.5f - 6.f * S, Screen.Y - TH - 10.f * S, TW + 12.f * S, TH + 4.f * S);
	TextAt(Label, Color, Screen.X - TW * 0.5f, Screen.Y - TH - 8.f * S, nullptr, 0.85f);
	DrawRect(Color, Screen.X - 1.5f * S, Screen.Y - 6.f * S, 3.f * S, 6.f * S);
	return true;
}

void ADeepHUD::DrawAboard(const ADeepDiver* Diver)
{
	const UDeepRunComponent* Run = Diver->GetRun();
	const ADeepGameState* State = GetWorld()->GetGameState<ADeepGameState>();
	const ADeepSubmarine* Sub = Diver->GetTether() ? Diver->GetTether()->GetSubmarine() : nullptr;
	if (!Run || !State || !Sub)
	{
		return;
	}
	const bool bCargo = Diver->GetCargo().Num() > 0;
	const bool bParked = State->bParked;
	const FString UseKey = ADeepPlayerController::KeyHint(TEXT("Use"));
	const int32 NewStop = NewReachableStop(State);

	// Labels on the three things in the room.
	DrawMarker(Sub->GetHatchLocation(), bParked ? TEXT("HATCH - walk in to dive") : TEXT("HATCH - LOCKED"), bParked ? GoodColor : DangerColor);
	DrawMarker(Sub->GetConsoleLocation() + FVector(0.f, 0.f, 40.f), bCargo ? TEXT("CONSOLE - bank here") : TEXT("CONSOLE"), bCargo ? CaptainColor : TextColor);
	DrawMarker(Sub->GetHelmLocation() + FVector(0.f, 0.f, 60.f), TEXT("HELM - drive the sub"), NewStop > 0 || !bParked ? CaptainColor : TextColor);

	// The one thing to do next.
	FString Next;
	FString Why;
	FLinearColor Color = CaptainColor;
	if (Run->IsNearConsole() && bCargo)
	{
		Next = FString::Printf(TEXT("[%s]  BANK %d cr OF CARGO"), *UseKey, Diver->GetCargoValue());
	}
	else if (Run->IsNearConsole())
	{
		Next = FString::Printf(TEXT("[%s]  USE THE CONSOLE"), *UseKey);
		Why = TEXT("Credits and the next purchase. Bring salvage up to bank it here.");
	}
	else if (Run->IsNearHelm())
	{
		Next = FString::Printf(TEXT("[%s]  TAKE THE HELM"), *UseKey);
		Why = NewStop > 0 ? FString::Printf(TEXT("You can now drive down to %.0f m."), UDeepTuningSettings::Get().GetStop(NewStop).SubDepthM) : LimitReason(State);
	}
	else if (bCargo)
	{
		Next = TEXT("BANK YOUR CARGO AT THE CONSOLE");
		Why = TEXT("Walk to the green screen.");
	}
	else if (!bParked)
	{
		Next = TEXT("THE HATCH IS LOCKED BETWEEN STOPS");
		Why = TEXT("Take the helm and stop at 100, 300 or 500 m.");
		Color = WarnColor;
	}
	else if (NewStop > 0)
	{
		Next = TEXT("TAKE THE HELM TO GO DEEPER");
		Why = FString::Printf(TEXT("You can now drive down to %.0f m. The helm is at the front."), UDeepTuningSettings::Get().GetStop(NewStop).SubDepthM);
	}
	else
	{
		Next = TEXT("DROP THROUGH THE HATCH TO DIVE");
		Why = TEXT("Walk into the yellow square in the floor. Bring salvage back up the same way.");
		Color = GoodColor;
	}
	float Y = Canvas->ClipY * 0.62f;
	Y += TextCentered(Next, Color, Y, BigFont, 1.f) + 4.f * S;
	if (!Why.IsEmpty())
	{
		TextCentered(Why, TextColor, Y, nullptr, 0.85f);
	}

	// The console's account panel, while near it or just after using it.
	const float Now = GetWorld()->GetTimeSeconds();
	if (Run->IsNearConsole() || Now - Run->GetLastConsoleTime() < 4.f)
	{
		FString Gate;
		int32 Price = 0;
		const bool bGate = NextGate(State, Gate, Price);
		const float W = 420.f * S;
		const float X = Canvas->ClipX - W - 16.f * S;
		float PY = Canvas->ClipY * 0.2f;
		DrawRect(FLinearColor(0.f, 0.03f, 0.02f, 0.9f), X, PY, W, 116.f * S);
		DrawRect(GoodColor, X, PY, W, 3.f * S);
		PY += 10.f * S;
		TextAt(TEXT("CONSOLE"), GoodColor, X + 16.f * S, PY, nullptr, 0.9f);
		PY += 24.f * S;
		TextAt(FString::Printf(TEXT("Credits   %d cr"), State->Credits), TextColor, X + 16.f * S, PY, nullptr, 0.85f);
		PY += 22.f * S;
		TextAt(bGate ? FString::Printf(TEXT("Next      %s   %d cr"), *Gate, Price) : FString(TEXT("Next      nothing: both gates owned")), bGate && State->Credits >= Price ? GoodColor : TextColor, X + 16.f * S, PY, nullptr, 0.85f);
		PY += 22.f * S;
		TextAt(TEXT("Bought automatically when you can afford it."), DimColor, X + 16.f * S, PY, nullptr, 0.7f);
	}
	if (Now - Run->GetLastGateTime() < 4.f)
	{
		TextCentered(State->IsHullUpgraded() ? TEXT("HULL UPGRADE BOUGHT: THE HELM CAN GO TO 600 m") : TEXT("FLASHLIGHT BOUGHT: THE LAMP WORKS, THE HELM CAN GO TO 400 m"), GoodColor, Canvas->ClipY * 0.3f, BigFont, 0.9f);
	}

	const FString Controls = FString::Printf(TEXT("WASD walk   Mouse look   %s use (console, helm)   walk into the hatch to dive   %s menu"),
		*UseKey, *ADeepPlayerController::KeyHint(TEXT("Menu")));
	float CW = 0.f, CH = 0.f;
	GetTextSize(Controls, CW, CH, Font, S * 0.62f);
	DrawRect(PanelColor, 4.f * S, Canvas->ClipY - CH - 16.f * S, CW + 12.f * S, CH + 8.f * S);
	TextAt(Controls, FLinearColor(0.85f, 0.92f, 0.95f), 10.f * S, Canvas->ClipY - CH - 12.f * S, nullptr, 0.62f);

	const float Dot = 3.f * S;
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.6f), Canvas->ClipX * 0.5f - Dot * 0.5f, Canvas->ClipY * 0.5f - Dot * 0.5f, Dot, Dot);
}

void ADeepHUD::DrawHelm(const ADeepDiver* Diver)
{
	const UDeepRunComponent* Run = Diver->GetRun();
	const ADeepGameState* State = GetWorld()->GetGameState<ADeepGameState>();
	if (!Run || !State)
	{
		return;
	}
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const float Depth = State->SubDepthM;
	const float Limit = State->DepthLimitM();

	// Depth, big, top centre.
	TextCentered(TEXT("HELM"), DimColor, 10.f * S, nullptr, 0.8f);
	TextCentered(FString::Printf(TEXT("%.0f m   %s"), Depth, *UDeepRules::ZoneName(Depth, Tuning.Zones).ToUpper()), TextColor, 30.f * S, BigFont, 1.6f);

	// The gauge: 0 m at the top, the deepest limit at the bottom; stops, the limit and the sub.
	float Deepest = 0.f;
	for (const float L : Tuning.Sub.DepthLimitsM)
	{
		Deepest = FMath::Max(Deepest, L);
	}
	Deepest = FMath::Max(Deepest, 1.f);
	const float GX = Canvas->ClipX - 120.f * S;
	const float GTop = Canvas->ClipY * 0.14f;
	const float GH = Canvas->ClipY * 0.62f;
	auto YAt = [&](float M) { return GTop + GH * FMath::Clamp(M / Deepest, 0.f, 1.f); };
	DrawRect(PanelColor, GX - 70.f * S, GTop - 30.f * S, 180.f * S, GH + 50.f * S);
	DrawRect(FLinearColor(0.3f, 0.5f, 0.6f, 0.8f), GX, GTop, 6.f * S, GH);
	DrawRect(FLinearColor(0.6f, 0.1f, 0.05f, 0.6f), GX, YAt(Limit), 6.f * S, GTop + GH - YAt(Limit));
	TextAt(TEXT("DEPTH"), DimColor, GX - 60.f * S, GTop - 26.f * S, nullptr, 0.7f);
	for (int32 Stop = 1; Stop <= Tuning.NumStops(); ++Stop)
	{
		const float SD = Tuning.GetStop(Stop).SubDepthM;
		const bool bHere = State->ParkedStop() == Stop;
		DrawRect(bHere ? GoodColor : TextColor, GX - 14.f * S, YAt(SD) - 1.f * S, 34.f * S, 3.f * S);
		TextAt(FString::Printf(TEXT("%.0f"), SD), bHere ? GoodColor : TextColor, GX - 62.f * S, YAt(SD) - 10.f * S, nullptr, 0.7f);
	}
	DrawRect(DangerColor, GX - 20.f * S, YAt(Limit) - 1.5f * S, 50.f * S, 3.f * S);
	TextAt(FString::Printf(TEXT("LIMIT %.0f"), Limit), DangerColor, GX + 26.f * S, YAt(Limit) - 10.f * S, nullptr, 0.65f);
	DrawRect(CaptainColor, GX - 8.f * S, YAt(Depth) - 5.f * S, 22.f * S, 10.f * S);
	TextAt(TEXT("SUB"), CaptainColor, GX + 18.f * S, YAt(Depth) - 10.f * S, nullptr, 0.65f);

	// The limit and why, always visible; a banner while the control pushes against it.
	TextCentered(LimitReason(State), State->IsHullUpgraded() ? DimColor : WarnColor, 78.f * S, nullptr, 0.8f);
	if (Run->IsHelmAtLimit())
	{
		const bool bFlash = FMath::Frac(GetWorld()->GetRealTimeSeconds() * 2.f) < 0.5f;
		TextCentered(FString::Printf(TEXT("DEPTH LIMIT %.0f m - THE HELM WILL NOT GO DEEPER"), Limit), bFlash ? FLinearColor::White : DangerColor, Canvas->ClipY * 0.3f, BigFont, 1.f);
	}

	// What to do now.
	const int32 Parked = State->ParkedStop();
	const FString Up = ADeepPlayerController::KeyHint(TEXT("Up"));
	const FString Down = ADeepPlayerController::KeyHint(TEXT("Down"));
	FString Next;
	FString Why;
	if (Parked > 0)
	{
		const int32 NewStop = NewReachableStop(State);
		Next = FString::Printf(TEXT("PARKED AT %.0f m"), Tuning.GetStop(Parked).SubDepthM);
		Why = NewStop > 0
			? FString::Printf(TEXT("Hold [%s] to dive to %.0f m.  [%s] stand up, then drop through the hatch."), *Down, Tuning.GetStop(NewStop).SubDepthM, *ADeepPlayerController::KeyHint(TEXT("Use")))
			: FString::Printf(TEXT("[%s] stand up, then drop through the hatch to dive here."), *ADeepPlayerController::KeyHint(TEXT("Use")));
	}
	else
	{
		Next = TEXT("BETWEEN STOPS: THE HATCH IS LOCKED");
		Why = TEXT("Let go of the control within a few metres of 100, 300 or 500 m to park there.");
	}
	float Y = Canvas->ClipY * 0.62f;
	Y += TextCentered(Next, Parked > 0 ? GoodColor : CaptainColor, Y, BigFont, 0.95f) + 4.f * S;
	TextCentered(Why, TextColor, Y, nullptr, 0.85f);

	const FString Controls = FString::Printf(TEXT("hold %s or S  dive      hold %s or W  rise      %s  stand up      %s menu"),
		*Down, *Up, *ADeepPlayerController::KeyHint(TEXT("Use")), *ADeepPlayerController::KeyHint(TEXT("Menu")));
	float CW = 0.f, CH = 0.f;
	GetTextSize(Controls, CW, CH, Font, S * 0.7f);
	DrawRect(PanelColor, 4.f * S, Canvas->ClipY - CH - 16.f * S, CW + 12.f * S, CH + 8.f * S);
	TextAt(Controls, FLinearColor(0.85f, 0.92f, 0.95f), 10.f * S, Canvas->ClipY - CH - 12.f * S, nullptr, 0.7f);
}

// ---------------------------------------------------------------- the Slate HUD's frame

namespace
{
	FDeepHudPrompt MakePrompt(FName Id, const FString& Key, const FString& Verb, const FString& Detail = FString(), EDeepHudTone Tone = EDeepHudTone::Normal)
	{
		FDeepHudPrompt Prompt;
		Prompt.Id = Id;
		Prompt.Key = Key;
		Prompt.Verb = Verb;
		Prompt.Detail = Detail;
		Prompt.Tone = Tone;
		return Prompt;
	}

	FString Key(const TCHAR* Action)
	{
		return ADeepPlayerController::KeyHint(Action).ToUpper();
	}

	/** The title menu and the screens over the menus (settings, credits, quit). True when it filled the frame. */
	bool DeepHudFillMenuScreens(FDeepHudFrame& Frame, const ADeepPlayerController* Player, bool bOnTitle)
	{
		const int32 Cursor = Player ? Player->GetScreenIndex() : 0;
		switch (Frame.Mode)
		{
		case EDeepHudMode::Title:
			for (const ADeepPlayerController::EMenuItem Item : ADeepPlayerController::TitleItems())
			{
				Frame.MenuItems.Add(ADeepPlayerController::MenuItemLabel(Item));
			}
			Frame.MenuIndex = Player ? Player->GetMenuIndex() : 0;
			Frame.MenuHint = TEXT("UP / DOWN   ENTER TO SELECT");
			return true;
		case EDeepHudMode::Settings:
		{
			const UDeepUserSettings& Settings = UDeepUserSettings::Get();
			Frame.CardTitle = TEXT("Settings");
			Frame.CardLead = TEXT("Changes apply at once and are kept for next time.");
			for (int32 Row = 0; Row < static_cast<int32>(EDeepSettingRow::Count); ++Row)
			{
				const EDeepSettingRow Which = static_cast<EDeepSettingRow>(Row);
				Frame.MenuItems.Add(DeepSettingsRules::RowLabel(Which));
				switch (Which)
				{
				case EDeepSettingRow::Volume:
					Frame.MenuValues.Add(DeepSettingsRules::VolumeLabel(Settings.MasterVolume));
					Frame.MenuBars.Add(Settings.MasterVolume);
					break;
				case EDeepSettingRow::Sensitivity:
					Frame.MenuValues.Add(DeepSettingsRules::SensitivityLabel(Settings.LookSensitivity));
					Frame.MenuBars.Add(DeepSettingsRules::SensitivityBar(Settings.LookSensitivity));
					break;
				case EDeepSettingRow::InvertLook:
					Frame.MenuValues.Add(Settings.bInvertLook ? TEXT("ON") : TEXT("OFF"));
					Frame.MenuBars.Add(-1.f);
					break;
				case EDeepSettingRow::RayTracing:
					Frame.MenuValues.Add(Settings.IsRayTracingOn() ? TEXT("ON") : TEXT("OFF"));
					Frame.MenuBars.Add(-1.f);
					break;
				default:
					Frame.MenuValues.Add(FString());
					Frame.MenuBars.Add(-1.f);
					break;
				}
			}
			Frame.MenuIndex = Cursor;
			Frame.MenuHint = TEXT("UP / DOWN   LEFT / RIGHT TO CHANGE   ESC TO GO BACK");
			return true;
		}
		case EDeepHudMode::Credits:
		{
			Frame.CardTitle = TEXT("Credits");
			Frame.CardLead = TEXT("DEEP. Made for the 2026 Epic AI MegaJam.");
			auto AddRow = [&Frame](const FString& Label, const FString& Text, EDeepHudTone Tone, bool bRule)
			{
				FDeepHudRow Row;
				Row.Label = Label;
				Row.Text = Text.IsEmpty() ? FString(TEXT(" ")) : Text;
				Row.Tone = Tone;
				Row.bRuleAbove = bRule;
				Frame.Rows.Add(Row);
			};
			AddRow(TEXT("Made by"), TEXT("Quinn and Colby"), EDeepHudTone::Normal, false);
			AddRow(TEXT("Built with"), TEXT("Claude agents"), EDeepHudTone::Normal, false);
			bool bFirst = true;
			for (const FDeepCreditRow& Credit : DeepCredits::Get())
			{
				FString Text = Credit.Author;
				if (!Credit.Licence.IsEmpty())
				{
					Text += Text.IsEmpty() ? Credit.Licence : TEXT(", ") + Credit.Licence;
				}
				AddRow(Credit.Item, Text, EDeepHudTone::Normal, bFirst);
				if (!Credit.Note.IsEmpty())
				{
					AddRow(Credit.Note, FString(), EDeepHudTone::Dim, false);
				}
				bFirst = false;
			}
			Frame.MenuItems.Add(TEXT("BACK"));
			Frame.MenuIndex = 0;
			Frame.MenuHint = TEXT("ENTER OR ESC TO GO BACK");
			return true;
		}
		case EDeepHudMode::ConfirmQuit:
			Frame.CardTitle = TEXT("Quit DEEP?");
			Frame.CardLead = bOnTitle ? TEXT("Leave for the desktop?") : TEXT("Leave for the desktop? The run in progress is not kept.");
			Frame.MenuItems = { TEXT("STAY"), TEXT("QUIT TO DESKTOP") };
			Frame.MenuIndex = Cursor;
			Frame.MenuHint = TEXT("ENTER TO SELECT   ESC TO STAY");
			return true;
		default:
			return false;
		}
	}
}

void ADeepHUD::GatherFrame(FDeepHudFrame& Frame)
{
	UWorld* World = GetWorld();
	const ADeepGameState* GameState = World ? World->GetGameState<ADeepGameState>() : nullptr;
	const ADeepDiver* Diver = Cast<ADeepDiver>(GetOwningPawn());
	const UDeepRunComponent* Run = Diver ? Diver->GetRun() : nullptr;
	ADeepPlayerController* Player = Cast<ADeepPlayerController>(GetOwningPlayerController());
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();

	if (Player)
	{
		int32 VX = 0, VY = 0;
		Player->GetViewportSize(VX, VY);
		if (VX > 0 && VY > 0)
		{
			Frame.ViewportPx = FVector2f(static_cast<float>(VX), static_cast<float>(VY));
		}
	}

	FDeepHudModeInputs In;
	In.bOnTitle = GameState && GameState->bOnTitle;
	In.bHasDiver = Diver && Run;
	In.bPauseOpen = Player && Player->IsPauseMenuOpen();
	In.bSeated = Run && Run->IsSeated();
	In.RunState = Run ? static_cast<uint8>(Run->GetState()) : 0;
	In.bBiteAlarm = Run && Run->IsBiteAlarmOn();
	In.bCarryingOrCut = Diver && (Diver->GetCargo().Num() > 0 || (Run && Run->GetState() == EDeepRunState::Cut));
	In.MenuScreen = Player ? static_cast<uint8>(Player->GetMenuScreen()) : 0;
	In.bSonar = Sonar && Sonar->IsSonarEnabled();
	Frame.Mode = DeepHudModel::ChooseMode(In);
	Frame.Panels = DeepHudModel::PanelsFor(Frame.Mode, In);
	Frame.HintSeconds = Tuning.Run.ControlsCardSeconds;

	UDeepCaptain* Captain = UDeepCaptain::Get(this);
	const bool bScreen = DeepHudFillMenuScreens(Frame, Player, In.bOnTitle);
	if (Frame.Mode == EDeepHudMode::Title)
	{
		Frame.CaptainLine = Captain && !Captain->GetLastText().IsEmpty() ? Captain->GetLastText() : FString(TEXT("Press any key. Any key."));
		return;
	}
	if (bScreen)
	{
		return;
	}
	if (!Diver || !Run || !GameState)
	{
		return;
	}
	// The review shows the Captain's win line itself (as in the old HUD, the queue is not read there).
	if (Captain && Run->GetState() != EDeepRunState::Won)
	{
		Frame.CaptainLine = Captain->GetLine();
		// The name tag carries the name; the line is only what he says.
		Frame.CaptainLine.RemoveFromStart(TEXT("CAPTAIN: "));
	}

	const EDeepRunState State = Run->GetState();
	const bool bTethered = State == EDeepRunState::Tethered;
	const float Now = World->GetTimeSeconds();
	const UDeepTetherComponent* TetherComp = Diver->GetTether();
	const ADeepSubmarine* Sub = TetherComp ? TetherComp->GetSubmarine() : nullptr;
	const UDeepHarpoonComponent* Harpoon = Diver->GetHarpoon();
	const UDeepHarpoonSettings& HarpoonSettings = UDeepHarpoonSettings::Get();

	// ---- the water
	const FDeepWeightResult Weight = Diver->GetWeight();
	Frame.DepthM = Diver->GetDepthM();
	Frame.Zone = Diver->GetZoneName();
	Frame.CargoKg = Weight.CarriedKg;
	Frame.CargoValue = Diver->GetCargoValue();
	Frame.bOverloaded = Weight.bOverloaded;
	Frame.bTether = Run->HasTether() && TetherComp && TetherComp->HasAnchor();
	if (Frame.bTether)
	{
		const FDeepTetherSolve& Solve = TetherComp->GetSolve();
		Frame.TetherLeftM = Solve.LeftM;
		Frame.TetherLengthM = Solve.LengthM;
		Frame.TetherStrain = Solve.Strain;
		Frame.bTaut = Solve.bTaut;
	}
	Frame.bHasLamp = Diver->HasFlashlight();
	Frame.bLampOn = Diver->IsLampOn();
	Frame.LampKey = Key(TEXT("Lamp"));
	if (Harpoon)
	{
		const EDeepHarpoonPhase Phase = Harpoon->GetPhase();
		Frame.Harpoon = Phase == EDeepHarpoonPhase::Flying ? 1 : (Phase == EDeepHarpoonPhase::Latched ? 2 : (Harpoon->GetShotsLeft() > 0 ? 0 : 3));
		Frame.HarpoonShots = Harpoon->GetShotsLeft();
		if (Harpoon->IsReady() && Run->CanSwim())
		{
			FVector Origin, Direction;
			if (Harpoon->GetView(Origin, Direction))
			{
				const float RangeCm = HarpoonSettings.RangeM * 100.f;
				const FDeepHarpoonTrace Aim = Harpoon->TraceAim(Origin, Direction, RangeCm * HarpoonSettings.AimLookAheadFactor);
				if (Aim.Target != EDeepHarpoonTarget::None)
				{
					const bool bInRange = Aim.DistanceCm <= RangeCm;
					Frame.AimKind = !bInRange ? 4 : (Aim.Target == EDeepHarpoonTarget::Submarine ? 2 : (Aim.Target == EDeepHarpoonTarget::Monster ? 3 : 1));
					const TCHAR* What = Aim.Target == EDeepHarpoonTarget::Submarine ? TEXT("SUB ") : (Aim.Target == EDeepHarpoonTarget::Monster ? TEXT("SERPENT ") : TEXT(""));
					Frame.AimLabel = bInRange ? FString::Printf(TEXT("%s%.0f m"), What, Aim.DistanceCm / 100.f) : FString::Printf(TEXT("%.0f m  OUT OF RANGE"), Aim.DistanceCm / 100.f);
				}
			}
		}
	}
	Frame.Patience01 = Run->GetPatienceFraction();
	Frame.PatienceMarks = Tuning.Patience.WarningFractions;
	Frame.bPatienceActive = bTethered;
	Frame.RefillFraction = Run->GetLastRefillFraction();
	Frame.SinceRefill = Now - Run->GetLastRefillTime();
	Frame.Threat01 = Run->GetMonsterThreat01();
	Frame.BiteLeft = Run->GetBiteAlarmLeft();
	Frame.AirSeconds = Run->GetBailoutAirSeconds();
	Frame.AirMax = Tuning.Bailout.AirSeconds;
	Frame.CutHold = Diver->GetCutHoldFraction();

	// Where home is.
	if ((Frame.Panels & EDeepHudPanel::HatchMarker) && TetherComp && TetherComp->HasAnchor() && Player && Player->PlayerCameraManager)
	{
		const FVector Anchor = TetherComp->GetAnchorLocation();
		Frame.HatchLabel = FString::Printf(TEXT("HATCH  %.0f m"), FVector::Dist(Anchor, Diver->GetActorLocation()) / UDeepRules::CmPerM);
		FVector2D Screen;
		const FVector CamLoc = Player->PlayerCameraManager->GetCameraLocation();
		const FRotator CamRot = Player->PlayerCameraManager->GetCameraRotation();
		const bool bFront = FVector::DotProduct(Anchor - CamLoc, CamRot.Vector()) > 0.0;
		if (bFront && Player->ProjectWorldLocationToScreen(Anchor, Screen, true)
			&& Screen.X >= 0.0 && Screen.Y >= 0.0 && Screen.X <= Frame.ViewportPx.X && Screen.Y <= Frame.ViewportPx.Y)
		{
			Frame.bHatchOnScreen = true;
			Frame.HatchPx = FVector2f(Screen);
		}
		else
		{
			const FVector Local = CamRot.UnrotateVector(Anchor - CamLoc);
			FVector2f Dir(static_cast<float>(Local.Y), static_cast<float>(-Local.Z));
			if (Dir.IsNearlyZero())
			{
				Dir = FVector2f(0.f, 1.f);
			}
			Frame.HatchDir = Dir.GetSafeNormal();
		}
	}

	// Water prompts under the reticle, and the one warning above it.
	if (Run->CanSwim())
	{
		if (const ADeepSalvage* Target = Diver->FindGrabTarget())
		{
			const FDeepSalvageKind& Kind = Target->GetKind();
			Frame.Prompts.Add(MakePrompt(TEXT("grab"), Key(TEXT("Grab")), TEXT("GRAB"), FString::Printf(TEXT("%s   %.0f kg   %d cr"), *Kind.Name, Kind.Kg, Kind.Value)));
		}
		const bool bTaut = bTethered && Frame.bTaut;
		if (Diver->IsWinching())
		{
			Frame.Prompts.Add(MakePrompt(TEXT("winching"), Key(TEXT("Winch")), TEXT("WINCHING HOME"), TEXT("loud: it can hear the motor"), EDeepHudTone::Warning));
		}
		else if (bTethered && (Weight.bOverloaded || bTaut))
		{
			Frame.Prompts.Add(MakePrompt(TEXT("winch"), Key(TEXT("Winch")), TEXT("HOLD TO WINCH HOME")));
			if (bTaut)
			{
				Frame.Prompts.Add(MakePrompt(TEXT("cut"), Key(TEXT("Cut")), TEXT("HOLD TO BE CUT LOOSE"), FString(), EDeepHudTone::Danger));
			}
		}
		if (bTethered && Diver->GetCargo().Num() > 0 && !Diver->IsWinching())
		{
			Frame.Prompts.Add(MakePrompt(TEXT("home"), FString(), TEXT("TAKE IT HOME"), TEXT("up through the hatch, bank at the console")));
		}
		if (State == EDeepRunState::Cut)
		{
			Frame.Prompts.Add(MakePrompt(TEXT("bailout"), FString(), TEXT("SWIM UP INTO THE HATCH"), FString(), EDeepHudTone::Danger));
		}
		if (bTaut && !Diver->IsWinching())
		{
			Frame.Warning = TEXT("End of the line");
			Frame.WarningTone = EDeepHudTone::Danger;
		}
		else if (bTethered && Frame.Patience01 <= 0.1f)
		{
			Frame.Warning = TEXT("The Captain is about to cut you");
			Frame.WarningTone = EDeepHudTone::Danger;
		}
		else if (bTethered && Weight.bOverloaded && !Diver->IsWinching())
		{
			Frame.Warning = TEXT("Too heavy to swim up");
			Frame.WarningTone = EDeepHudTone::Warning;
		}
	}

	// ---- aboard: prompts on the hatch, the console and the helm, only where they apply
	Frame.Credits = GameState->Credits;
	const int32 NewStop = NewReachableStop(GameState);
	if (Frame.Mode == EDeepHudMode::Aboard && Sub && Player)
	{
		const bool bCargo = Diver->GetCargo().Num() > 0;
		const bool bParked = GameState->bParked;
		auto Anchor = [&](FDeepHudPrompt Prompt, const FVector& WorldAt)
		{
			FVector2D Screen;
			const FVector CamLoc = Player->PlayerCameraManager ? Player->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
			const FVector CamFwd = Player->PlayerCameraManager ? Player->PlayerCameraManager->GetCameraRotation().Vector() : FVector::ForwardVector;
			if (FVector::DotProduct(WorldAt - CamLoc, CamFwd) <= 0.0 || !Player->ProjectWorldLocationToScreen(WorldAt, Screen, true))
			{
				return;
			}
			Prompt.bAnchored = true;
			Prompt.ScreenPx = FVector2f(Screen);
			Frame.Prompts.Add(Prompt);
		};
		const FString UseKey = Key(TEXT("Use"));
		if (Run->IsNearConsole())
		{
			FString Gate;
			int32 Price = 0;
			const bool bGate = NextGate(GameState, Gate, Price);
			FDeepHudPrompt Prompt = bCargo
				? MakePrompt(TEXT("console"), UseKey, TEXT("BANK"), FString::Printf(TEXT("%d cr of cargo"), Diver->GetCargoValue()), EDeepHudTone::Primary)
				: MakePrompt(TEXT("console"), UseKey, TEXT("CONSOLE"), FString::Printf(TEXT("%d cr"), GameState->Credits), EDeepHudTone::Primary);
			Prompt.Detail2 = bGate ? FString::Printf(TEXT("Next: %s, %d cr. Bought when you can afford it."), *Gate.ToLower(), Price) : FString(TEXT("Flashlight and hull upgrade owned."));
			Anchor(Prompt, Sub->GetConsoleLocation() + FVector(0.f, 0.f, 40.f));
		}
		else if (bCargo)
		{
			Anchor(MakePrompt(TEXT("console"), FString(), TEXT("BANK CARGO"), FString::Printf(TEXT("%d cr"), Diver->GetCargoValue()), EDeepHudTone::Primary), Sub->GetConsoleLocation() + FVector(0.f, 0.f, 40.f));
		}
		if (Run->IsNearHelm())
		{
			FDeepHudPrompt Prompt = MakePrompt(TEXT("helm"), UseKey, TEXT("TAKE THE HELM"), FString(), EDeepHudTone::Primary);
			Prompt.Detail2 = NewStop > 0 ? FString::Printf(TEXT("You can now drive down to %.0f m."), Tuning.GetStop(NewStop).SubDepthM) : LimitReason(GameState);
			Anchor(Prompt, Sub->GetHelmLocation() + FVector(0.f, 0.f, 60.f));
		}
		else if (!bCargo && (NewStop > 0 || !bParked))
		{
			Anchor(MakePrompt(TEXT("helm"), FString(), TEXT("HELM"), NewStop > 0 ? FString::Printf(TEXT("down to %.0f m"), Tuning.GetStop(NewStop).SubDepthM) : FString(TEXT("park at a stop")), EDeepHudTone::Primary),
				Sub->GetHelmLocation() + FVector(0.f, 0.f, 60.f));
		}
		if (!bParked)
		{
			Anchor(MakePrompt(TEXT("hatch"), FString(), TEXT("HATCH LOCKED"), TEXT("between stops"), EDeepHudTone::Warning), Sub->GetHatchLocation());
		}
		else if (!bCargo && NewStop == 0)
		{
			Anchor(MakePrompt(TEXT("hatch"), FString(), TEXT("DIVE"), TEXT("drop through the hatch")), Sub->GetHatchLocation());
		}
	}

	// ---- the helm
	if (Frame.Mode == EDeepHudMode::Helm)
	{
		Frame.SubDepthM = GameState->SubDepthM;
		Frame.LimitM = GameState->DepthLimitM();
		float Deepest = 1.f;
		for (const float Limit : Tuning.Sub.DepthLimitsM)
		{
			Deepest = FMath::Max(Deepest, Limit);
		}
		Frame.GaugeMaxM = Deepest;
		for (int32 Stop = 1; Stop <= Tuning.NumStops(); ++Stop)
		{
			Frame.Stops.Add(Tuning.GetStop(Stop).SubDepthM);
		}
		Frame.ParkedStop = GameState->ParkedStop();
		Frame.bAtLimit = Run->IsHelmAtLimit();
		// The hull bar: only in the cave shaft, the one place the hull takes knocks.
		Frame.Hull01 = DeepShaft::IsOn() ? GameState->GetHullIntegrity01() : -1.f;
		Frame.HullTone = Frame.Hull01 < 0.f ? EDeepHudTone::Normal : Frame.Hull01 < Tuning.Captain.ScaredHull01 ? EDeepHudTone::Danger : Frame.Hull01 < Tuning.Captain.TenseHull01 ? EDeepHudTone::Warning : EDeepHudTone::Normal;
		if ((Frame.Panels & EDeepHudPanel::Sonar) && Sonar)
		{
			Sonar->FillHud(Frame.Sonar);
		}
		if (Frame.ParkedStop > 0)
		{
			Frame.HelmStatus = FString::Printf(TEXT("Parked at %.0f m"), Tuning.GetStop(Frame.ParkedStop).SubDepthM);
			Frame.HelmReason = NewStop > 0
				? FString::Printf(TEXT("Dive to %.0f m, or stand up and drop through the hatch."), Tuning.GetStop(NewStop).SubDepthM)
				: LimitReason(GameState);
		}
		else
		{
			Frame.HelmStatus = TEXT("Between stops: hatch locked");
			Frame.HelmReason = TEXT("Let go of the control near 100, 300 or 500 m to park.");
		}
	}

	// ---- cards
	const FDeepEconomyTuning& Economy = Tuning.Economy;
	auto Row = [&Frame](const FString& Label, int32 Amount, bool bMinus, EDeepHudTone Tone, bool bRule = false, bool bBig = false, const FString& Text = FString())
	{
		FDeepHudRow R;
		R.Label = Label;
		R.Amount = Amount;
		R.bMinus = bMinus;
		R.Tone = Tone;
		R.bRuleAbove = bRule;
		R.bBig = bBig;
		R.Text = Text;
		Frame.Rows.Add(R);
	};
	if (Frame.Mode == EDeepHudMode::Dead)
	{
		Frame.CardTitle = FString::Printf(TEXT("Diver #%d written off"), GameState->DiverNumber);
		Frame.CardLead = Run->WasLastDeathMonster() ? TEXT("Taken by the serpent. Cargo lost.") : TEXT("Bailout air ran out. Cargo lost.");
		Row(TEXT("Disposal fee"), Economy.DeathFee, true, EDeepHudTone::Danger);
		Frame.CardFooter = FString::Printf(TEXT("Replacement diver #%d in %.0f"), GameState->DiverNumber + 1, FMath::CeilToFloat(FMath::Max(0.f, Run->GetStateTimeLeft())));
		Frame.bFooterReady = false;
	}
	else if (Frame.Mode == EDeepHudMode::Settlement)
	{
		const FDeepSettlementReport& Report = Run->GetSettlement();
		Frame.CardTitle = TEXT("Settlement");
		Row(TEXT("Gross haul"), Report.Gross, false, EDeepHudTone::Normal);
		Row(FString::Printf(TEXT("The Captain's cut (%d%%)"), Economy.DirectorCutPercent), Report.DirectorCut, true, EDeepHudTone::Normal);
		for (int32 Index = 0; Index < Report.FeeLabels.Num(); ++Index)
		{
			Row(Report.FeeLabels[Index], Report.FeeAmounts.IsValidIndex(Index) ? Report.FeeAmounts[Index] : 0, true, EDeepHudTone::Dim);
		}
		Row(TEXT("Net"), Report.Net, false, Report.Net > 0 ? EDeepHudTone::Normal : EDeepHudTone::Danger, true, true);
		Row(TEXT("Credits"), Report.CreditsAfter, false, EDeepHudTone::Normal);
		const auto GateName = [](int32 Rating) { return Rating <= 2 ? FString(TEXT("Flashlight")) : FString(TEXT("Hull upgrade")); };
		if (Report.bHullBought)
		{
			Row(FString::Printf(TEXT("%s bought"), *GateName(Report.HullRating)), Report.HullSpent, true, EDeepHudTone::Primary, true);
		}
		else if (Report.NextHullPrice >= 0)
		{
			Row(GateName(Report.HullRating + 1), 0, false, EDeepHudTone::Dim, true, false, FString::Printf(TEXT("needs %d cr"), Report.NextHullPrice));
		}
		Frame.CardFooterKey = Key(TEXT("Continue"));
		Frame.CardFooter = Report.bHullBought ? TEXT("Back to the deck, then take the helm") : TEXT("Back to the deck");
		Frame.bFooterReady = Run->GetStateTimeLeft() <= 0.f;
	}
	else if (Frame.Mode == EDeepHudMode::Review)
	{
		const FDeepRunReview Review = GameState->BuildReview();
		Frame.CardTitle = TEXT("Relic banked. Performance review");
		if (Captain && !Captain->GetLastText().IsEmpty())
		{
			Frame.CardLead = FString::Printf(TEXT("Captain: \"%s\""), *Captain->GetLastText());
		}
		Row(TEXT("Divers expended"), 0, false, Review.DiversExpended > 0 ? EDeepHudTone::Warning : EDeepHudTone::Normal, false, false, FString::Printf(TEXT("%d"), Review.DiversExpended));
		Row(TEXT("Total gross"), Review.Gross, false, EDeepHudTone::Normal);
		Row(TEXT("The Captain's cut, in total"), Review.DirectorCut, true, EDeepHudTone::Normal);
		Row(TEXT("Fees, in total"), Review.Fees, true, EDeepHudTone::Dim);
		Row(TEXT("Kept by the diver"), Review.Kept, false, Review.Kept > 0 ? EDeepHudTone::Normal : EDeepHudTone::Danger, true, true);
		Row(TEXT("Time taken"), 0, false, EDeepHudTone::Normal, false, false,
			FString::Printf(TEXT("%d:%02d"), FMath::FloorToInt(Review.Seconds / 60.f), FMath::FloorToInt(Review.Seconds) % 60));
		Frame.CardBigLabel = TEXT("The Captain's rating");
		Frame.CardBig = Review.Rating;
		for (const ADeepPlayerController::EMenuItem Item : ADeepPlayerController::WinItems())
		{
			Frame.MenuItems.Add(ADeepPlayerController::MenuItemLabel(Item));
		}
		Frame.MenuIndex = Player ? Player->GetMenuIndex() : 0;
		Frame.bMenuEnabled = Run->CanLeaveReview();
		Frame.MenuHint = TEXT("UP / DOWN   ENTER TO SELECT");
	}
	else if (Frame.Mode == EDeepHudMode::Pause)
	{
		Frame.CardTitle = TEXT("Paused");
		for (const ADeepPlayerController::EMenuItem Item : ADeepPlayerController::PauseItems())
		{
			Frame.MenuItems.Add(ADeepPlayerController::MenuItemLabel(Item));
		}
		Frame.MenuIndex = Player ? Player->GetMenuIndex() : 0;
		Frame.MenuHint = TEXT("UP / DOWN   ENTER TO SELECT   ESC TO RESUME");
	}

	// ---- the controls hint for this mode
	if (Frame.Mode == EDeepHudMode::Water)
	{
		Frame.Hint = {
			MakePrompt(TEXT("h1"), TEXT("WASD"), TEXT("SWIM"), FString::Printf(TEXT("%s / %s  up and down"), *Key(TEXT("Up")), *Key(TEXT("Down")))),
			MakePrompt(TEXT("h2"), Key(TEXT("Grab")), TEXT("GRAB"), FString::Printf(TEXT("%s  drop"), *Key(TEXT("Drop")))),
			MakePrompt(TEXT("h3"), TEXT("LMB"), TEXT("HARPOON"), TEXT("one per dive")),
			MakePrompt(TEXT("h4"), Key(TEXT("Winch")), TEXT("WINCH HOME"), TEXT("hold, loud")),
			MakePrompt(TEXT("h5"), Key(TEXT("Lamp")), TEXT("LAMP"), Diver->HasFlashlight() ? FString() : FString(TEXT("needs the flashlight"))),
			MakePrompt(TEXT("h6"), Key(TEXT("Cut")), TEXT("CUT LOOSE"), TEXT("hold")),
		};
		Frame.bHintDone = GameState->TotalGross > 0 || Diver->GetCargo().Num() > 0;
	}
	else if (Frame.Mode == EDeepHudMode::Aboard)
	{
		Frame.Hint = {
			MakePrompt(TEXT("h1"), TEXT("WASD"), TEXT("WALK")),
			MakePrompt(TEXT("h2"), Key(TEXT("Use")), TEXT("USE"), TEXT("console, helm")),
			MakePrompt(TEXT("h3"), Key(TEXT("Menu")), TEXT("MENU")),
		};
	}
	else if (Frame.Mode == EDeepHudMode::Helm)
	{
		// In the cave shaft W/S/A/D steer, so only C and Space change depth.
		const bool bShaft = DeepShaft::IsOn();
		Frame.Hint = {
			MakePrompt(TEXT("h1"), bShaft ? Key(TEXT("Down")) : FString::Printf(TEXT("%s / S"), *Key(TEXT("Down"))), TEXT("DIVE"), TEXT("hold")),
			MakePrompt(TEXT("h2"), bShaft ? Key(TEXT("Up")) : FString::Printf(TEXT("%s / W"), *Key(TEXT("Up"))), TEXT("RISE"), TEXT("hold")),
		};
		if (bShaft)
		{
			Frame.Hint.Add(MakePrompt(TEXT("h4"), TEXT("WASD"), TEXT("STEER"), TEXT("off the rock")));
		}
		Frame.Hint.Add(MakePrompt(TEXT("h3"), Key(TEXT("Use")), TEXT("STAND UP")));
	}

	// ---- feedback cues, on the frame the state changes (well inside 100 ms)
	auto Cue = [&Frame](const FString& Label, EDeepHudTone Tone = EDeepHudTone::Primary)
	{
		FDeepHudCue C;
		C.Label = Label;
		C.Tone = Tone;
		Frame.Cues.Add(C);
	};
	const int32 Cargo = Diver->GetCargo().Num();
	const uint8 StateNum = static_cast<uint8>(State);
	if (PrevState != 255)
	{
		if (Cargo > PrevCargo && PrevCargo >= 0 && Cargo > 0)
		{
			Cue(FString::Printf(TEXT("Grabbed   +%.0f kg"), Diver->GetCargo().Last().Kg));
		}
		else if (Cargo < PrevCargo && Run->CanSwim())
		{
			Cue(TEXT("Dropped"), EDeepHudTone::Normal);
		}
		if (Diver->IsWinching() != bPrevWinching)
		{
			Cue(Diver->IsWinching() ? TEXT("Winch on") : TEXT("Winch off"), Diver->IsWinching() ? EDeepHudTone::Warning : EDeepHudTone::Normal);
		}
		if (StateNum != PrevState)
		{
			if (State == EDeepRunState::Cut)
			{
				Cue(TEXT("Tether cut"), EDeepHudTone::Danger);
			}
			else if (State == EDeepRunState::Settling)
			{
				Cue(TEXT("Banked"));
			}
		}
		if (Run->GetLastGateTime() != PrevGateTime && Now - Run->GetLastGateTime() < 1.f)
		{
			Cue(GameState->IsHullUpgraded() ? TEXT("Hull upgrade bought") : TEXT("Flashlight bought"));
		}
		if (Run->GetLastFeeTime() != PrevFeeTime && Run->GetLastFee() > 0 && Now - Run->GetLastFeeTime() < 1.f)
		{
			Cue(FString::Printf(TEXT("Fee   -%d cr"), Run->GetLastFee()), EDeepHudTone::Warning);
		}
		if (GameState->bParked && !bPrevParked && GameState->ParkedStop() > 0)
		{
			Cue(FString::Printf(TEXT("Parked at %.0f m"), Tuning.GetStop(GameState->ParkedStop()).SubDepthM));
		}
	}
	if (Harpoon)
	{
		const uint8 Phase = static_cast<uint8>(Harpoon->GetPhase());
		const float Since = Harpoon->GetSecondsSinceResult();
		if (PrevState != 255)
		{
			if (Phase == static_cast<uint8>(EDeepHarpoonPhase::Flying) && PrevHarpoonPhase != Phase)
			{
				Cue(TEXT("Harpoon away"));
			}
			if (Since < PrevHarpoonSince && Since < HarpoonSettings.MessageSeconds)
			{
				switch (Harpoon->GetLastResult())
				{
				case EDeepHarpoonResult::LatchedSub: Cue(FString::Printf(TEXT("Latched to the sub   repair -%d cr"), FMath::Max(0, HarpoonSettings.RepairFeePerHit)), EDeepHudTone::Warning); break;
				case EDeepHarpoonResult::LatchedTerrain: Cue(TEXT("Latched   release to let go")); break;
				case EDeepHarpoonResult::HitMonster: Cue(TEXT("Hit   it recoils"), EDeepHudTone::Danger); break;
				case EDeepHarpoonResult::Missed: Cue(TEXT("Missed"), EDeepHudTone::Dim); break;
				case EDeepHarpoonResult::OutOfRange: Cue(TEXT("Out of range"), EDeepHudTone::Dim); break;
				case EDeepHarpoonResult::Empty: Cue(TEXT("No harpoon left this dive"), EDeepHudTone::Warning); break;
				default: break;
				}
			}
		}
		PrevHarpoonPhase = Phase;
		PrevHarpoonSince = Since;
	}
	PrevCargo = Cargo;
	bPrevWinching = Diver->IsWinching();
	PrevState = StateNum;
	PrevGateTime = Run->GetLastGateTime();
	PrevFeeTime = Run->GetLastFeeTime();
	bPrevParked = GameState->bParked;
}
