#include "Game/DeepHUD.h"

#include "Camera/PlayerCameraManager.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Diver/DeepTetherComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Game/DeepGameState.h"
#include "Game/DeepPlayerController.h"
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
	const FLinearColor PanelColor(0.f, 0.02f, 0.04f, 0.78f);
	const FLinearColor PatienceColor(0.25f, 0.85f, 0.95f);
	const FLinearColor CaptainColor(1.f, 0.85f, 0.45f);

	FString Credits(int32 Amount)
	{
		return FString::Printf(TEXT("%d cr"), Amount);
	}
}

void ADeepHUD::DrawHUD()
{
	Super::DrawHUD();

	const ADeepDiver* Diver = Cast<ADeepDiver>(GetOwningPawn());
	if (!Canvas || !Diver || !GEngine)
	{
		return;
	}
	Font = GEngine->GetMediumFont();
	BigFont = GEngine->GetLargeFont();
	S = FMath::Max(1.f, Canvas->ClipY / 720.f);
	const float Now = GetWorld()->GetTimeSeconds();
	const UDeepRunComponent* Run = Diver->GetRun();
	const EDeepRunState State = Run ? Run->GetState() : EDeepRunState::Tethered;

	DrawStatus(Diver, Now);
	if (State == EDeepRunState::Cut)
	{
		DrawBailout(Diver);
	}
	else if (Run)
	{
		DrawPatience(*Run, Now);
	}
	if (Run && Run->CanSwim())
	{
		DrawPrompts(Diver);
	}
	DrawCaptain();

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
	DrawText(String, Color, (Canvas->ClipX - W) * 0.5f, Y, Use, S * TextScale);
	return H;
}

void ADeepHUD::DrawStatus(const ADeepDiver* Diver, float Now)
{
	const float Line = 24.f * S;
	const float X = 24.f * S;
	float Y = 20.f * S;
	auto Text = [&](const FString& String, const FLinearColor& Color)
	{
		TextAt(String, Color, X, Y);
		Y += Line;
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
		Text(FString::Printf(TEXT("TETHER %.1f m left of %.0f m%s"), Solve.LeftM, Solve.LengthM, Solve.bTaut ? TEXT("   TAUT") : TEXT("")), Color);
	}

	if (const ADeepGameState* State = GetWorld()->GetGameState<ADeepGameState>())
	{
		Text(FString::Printf(TEXT("CREDITS %d   HULL %d   STOP %d   DIVER #%d"), State->Credits, State->HullRating, State->Stop, State->DiverNumber), TextColor);
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
	const float W = Canvas->ClipX * 0.4f;
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
	TextCentered(FString::Printf(TEXT("BAILOUT AIR  %.1f s"), Air), bPulse ? FLinearColor::White : DangerColor, 38.f * S, BigFont, 1.f);

	const float W = Canvas->ClipX * 0.4f;
	const float H = 12.f * S;
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = 84.f * S;
	const float Fraction = Tuning.AirSeconds > 0.f ? FMath::Clamp(Air / Tuning.AirSeconds, 0.f, 1.f) : 0.f;
	DrawRect(PanelColor, X - 3.f * S, Y - 3.f * S, W + 6.f * S, H + 6.f * S);
	DrawRect(DangerColor, X, Y, W * Fraction, H);

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
		TextCentered(FString::Printf(TEXT("%s  -  %s"), *Label, Hint), GoodColor, 108.f * S);
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
	}

	float PromptY = Canvas->ClipY * 0.66f;
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

	const FString Controls = FString::Printf(TEXT("WASD swim   Mouse look   %s/%s up/down   %s sprint   %s grab   %s drop   hold %s winch   %s lamp   hold %s cut loose"),
		*ADeepPlayerController::KeyHint(TEXT("Up")), *ADeepPlayerController::KeyHint(TEXT("Down")), *ADeepPlayerController::KeyHint(TEXT("Sprint")),
		*ADeepPlayerController::KeyHint(TEXT("Grab")), *ADeepPlayerController::KeyHint(TEXT("Drop")), *ADeepPlayerController::KeyHint(TEXT("Winch")),
		*ADeepPlayerController::KeyHint(TEXT("Lamp")), *ADeepPlayerController::KeyHint(TEXT("Cut")));
	TextAt(Controls, DimColor, 24.f * S, Canvas->ClipY - 28.f * S, nullptr, 0.75f);
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
		GetTextSize(Try, W, H, Font, S * 1.1f);
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
		GetTextSize(Text, W, H, Font, S * 1.1f);
		WidestW = FMath::Max(WidestW, W);
	}
	const float Top = Canvas->ClipY * 0.80f - (Lines.Num() - 1) * LineH;
	DrawRect(PanelColor, (Canvas->ClipX - WidestW) * 0.5f - 14.f * S, Top - 6.f * S, WidestW + 28.f * S, Lines.Num() * LineH + 10.f * S);
	float Y = Top;
	for (const FString& Text : Lines)
	{
		TextCentered(Text, CaptainColor, Y, nullptr, 1.1f);
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
	Y += TextCentered(FString::Printf(TEXT("DIVER #%d WRITTEN OFF"), Number), DangerColor, Y, BigFont, 1.f) + 10.f * S;
	Y += TextCentered(TEXT("Bailout air ran out. Cargo lost."), TextColor, Y) + 4.f * S;
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
	const float Line = 26.f * S;
	const int32 Rows = 9 + Report.FeeLabels.Num();
	const float H = Rows * Line + 40.f * S;
	const float Top = FMath::Max(10.f * S, (Canvas->ClipY - H) * 0.42f);
	DrawRect(FLinearColor(0.f, 0.01f, 0.02f, 0.9f), X, Top, W, H);
	DrawRect(CaptainColor, X, Top, W, 3.f * S);

	const float Pad = 22.f * S;
	float Y = Top + 14.f * S;
	auto Row = [&](const FString& Label, const FString& Amount, const FLinearColor& Color, UFont* RowFont = nullptr)
	{
		TextAt(Label, Color, X + Pad, Y, RowFont);
		float AW = 0.f, AH = 0.f;
		GetTextSize(Amount, AW, AH, RowFont ? RowFont : Font.Get(), S);
		TextAt(Amount, Color, X + W - Pad - AW, Y, RowFont);
		Y += Line;
	};

	TextCentered(TEXT("SETTLEMENT"), CaptainColor, Y, BigFont, 1.f);
	Y += Line * 1.6f;
	Row(TEXT("Gross haul"), Credits(Report.Gross), TextColor);
	Row(FString::Printf(TEXT("The Director's cut (%d%%)"), Economy.DirectorCutPercent), FString::Printf(TEXT("-%d cr"), Report.DirectorCut), WarnColor);
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
	TextCentered(FString::Printf(TEXT("%.0f m"), Depth), Color, Canvas->ClipY * 0.45f, BigFont, 1.f);
}
