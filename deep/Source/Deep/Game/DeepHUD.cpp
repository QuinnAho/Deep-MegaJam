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
#include "Story/DeepCaptain.h"
#include "World/DeepSalvage.h"

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

void ADeepHUD::DrawHUD()
{
	Super::DrawHUD();

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

	DrawStatus(Diver, Now);
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
	else if (State == EDeepRunState::Descending)
	{
		DrawDescending(*Run);
	}
	else if (State == EDeepRunState::Won)
	{
		DrawReview(*Run);
	}

	// The first dive of a run: a big controls card until the first grab (or for ControlsCardSeconds).
	const float CardSeconds = UDeepTuningSettings::Get().Run.ControlsCardSeconds;
	if (GameState && State == EDeepRunState::Tethered && GameState->DiverNumber == 1 && GameState->Stop == 1 && GameState->TotalGross == 0
		&& Diver->GetCargo().IsEmpty() && GameState->RunSeconds < CardSeconds)
	{
		DrawControlsCard(FMath::Clamp((CardSeconds - GameState->RunSeconds) / 1.5f, 0.f, 1.f));
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
	TextCentered(TEXT("PRESS ANY KEY"), Prompt, Canvas->ClipY * 0.76f, BigFont, 1.2f);
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
		FString::Printf(TEXT("Hold %s  winch home (loud)        %s  lamp"), *Key(TEXT("Winch")), *Key(TEXT("Lamp"))),
		FString::Printf(TEXT("Hold %s  ask to be cut loose        %s  menu"), *Key(TEXT("Cut")), *Key(TEXT("Menu"))),
	};
	const FString GoalA = TEXT("Grab salvage below. Bring it back");
	const FString GoalB = TEXT("to the airlock under the sub.");
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

	Text(FString::Printf(TEXT("DEPTH  %.0f m   %s"), Diver->GetDepthM(), *Diver->GetZoneName().ToUpper()), TextColor);
	const FDeepWeightResult Weight = Diver->GetWeight();
	Text(FString::Printf(TEXT("CARGO  %.0f kg   %d cr%s"), Weight.CarriedKg, Diver->GetCargoValue(), Weight.bOverloaded ? TEXT("   TOO HEAVY") : TEXT("")), Weight.bOverloaded ? WarnColor : TextColor);

	const UDeepRunComponent* Run = Diver->GetRun();
	const UDeepTetherComponent* Tether = Diver->GetTether();
	if (Run && !Run->HasTether())
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
		Text(FString::Printf(TEXT("CREDITS %d   HULL %d"), State->Credits, State->HullRating), TextColor);
		Text(FString::Printf(TEXT("STOP %d   DIVER #%d"), State->Stop, State->DiverNumber), TextColor);
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

	TextCentered(TEXT("TETHER CUT  -  SWIM TO THE AIRLOCK"), DangerColor, 14.f * S, nullptr, 1.f);
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
	const FString Label = FString::Printf(TEXT("AIRLOCK  %.0f m"), DistanceM);
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
		Prompts.Emplace(TEXT("Bring the cargo to the airlock to bank it"), DimColor);
		// First-time players lose the sub: mark the airlock while carrying (below the Patience bar).
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
	const FString ControlsB = bTethered
		? FString::Printf(TEXT("hold %s winch home   %s lamp   hold %s be cut loose   %s menu"),
			*ADeepPlayerController::KeyHint(TEXT("Winch")), *ADeepPlayerController::KeyHint(TEXT("Lamp")), *ADeepPlayerController::KeyHint(TEXT("Cut")),
			*ADeepPlayerController::KeyHint(TEXT("Menu")))
		: FString::Printf(TEXT("no tether: swim to the airlock   %s lamp   %s menu"),
			*ADeepPlayerController::KeyHint(TEXT("Lamp")), *ADeepPlayerController::KeyHint(TEXT("Menu")));
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
	if (Report.bHullBought)
	{
		Row(FString::Printf(TEXT("HULL RATING %d BOUGHT"), Report.HullRating), FString::Printf(TEXT("-%d cr"), Report.HullSpent), GoodColor);
	}
	else if (Report.NextHullPrice >= 0)
	{
		Row(FString::Printf(TEXT("Hull rating %d"), Report.HullRating + 1), FString::Printf(TEXT("needs %d cr"), Report.NextHullPrice), DimColor);
	}
	else
	{
		Row(TEXT("Hull rating"), TEXT("maxed"), DimColor);
	}

	const ADeepGameState* State = GetWorld()->GetGameState<ADeepGameState>();
	const bool bDeeper = State && Report.NextStop > State->Stop;
	const FDeepStopTuning Next = Tuning.GetStop(Report.NextStop);
	const FString NextZone = UDeepRules::ZoneName(Next.SubDepthM, Tuning.Zones).ToUpper();
	Y += 10.f * S;
	const bool bReady = Run.GetStateTimeLeft() <= 0.f;
	const FString Action = bDeeper
		? FString::Printf(TEXT("[%s]  DESCEND TO %.0f m  %s"), *ADeepPlayerController::KeyHint(TEXT("Continue")), Next.SubDepthM, *NextZone)
		: FString::Printf(TEXT("[%s]  DIVE AGAIN"), *ADeepPlayerController::KeyHint(TEXT("Continue")));
	TextCentered(Action, bReady ? CaptainColor : DimColor, Y, nullptr, 1.1f);
}

void ADeepHUD::DrawDescending(const UDeepRunComponent& Run)
{
	const float Total = FMath::Max(0.01f, UDeepTuningSettings::Get().Run.DescendSeconds);
	const float T = FMath::Clamp(1.f - Run.GetStateTimeLeft() / Total, 0.f, 1.f);
	// Fade to black, hold, fade back in; the depth counter rolls meanwhile.
	const float Alpha = FMath::Clamp(1.f - FMath::Abs(T - 0.5f) * 2.f, 0.f, 1.f);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, FMath::Clamp(Alpha * 1.4f, 0.f, 1.f)), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	const float Depth = FMath::Lerp(Run.GetDescendFromDepthM(), Run.GetDescendToDepthM(), FMath::SmoothStep(0.f, 1.f, T));
	FLinearColor Color = TextColor;
	Color.A = FMath::Clamp(Alpha * 2.f, 0.f, 1.f);
	TextCentered(FString::Printf(TEXT("%.0f m"), Depth), Color, Canvas->ClipY * 0.45f, BigFont, 1.8f);
}
