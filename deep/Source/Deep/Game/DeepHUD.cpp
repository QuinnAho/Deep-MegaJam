#include "Game/DeepHUD.h"

#include "Diver/DeepDiver.h"
#include "Diver/DeepTetherComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Game/DeepGameState.h"
#include "Game/DeepPlayerController.h"
#include "World/DeepSalvage.h"

namespace
{
	const FLinearColor TextColor(0.85f, 0.95f, 1.f);
	const FLinearColor WarnColor(1.f, 0.6f, 0.15f);
	const FLinearColor DangerColor(1.f, 0.2f, 0.15f);
}

void ADeepHUD::DrawHUD()
{
	Super::DrawHUD();

	const ADeepDiver* Diver = Cast<ADeepDiver>(GetOwningPawn());
	if (!Canvas || !Diver || !GEngine)
	{
		return;
	}
	UFont* Font = GEngine->GetMediumFont();
	const float Scale = FMath::Max(1.f, Canvas->ClipY / 900.f);
	const float Line = 26.f * Scale;
	const float X = 32.f * Scale;
	float Y = 28.f * Scale;

	auto Text = [&](const FString& String, const FLinearColor& Color)
	{
		DrawText(String, Color, X, Y, Font, Scale);
		Y += Line;
	};

	Text(FString::Printf(TEXT("DEPTH  %.0f m   %s"), Diver->GetDepthM(), *Diver->GetZoneName().ToUpper()), TextColor);

	const FDeepWeightResult Weight = Diver->GetWeight();
	Text(FString::Printf(TEXT("CARGO  %.0f kg   %d cr"), Weight.CarriedKg, Diver->GetCargoValue()), Weight.bOverloaded ? WarnColor : TextColor);

	if (const UDeepTetherComponent* Tether = Diver->GetTether(); Tether && Tether->HasAnchor())
	{
		const FDeepTetherSolve& Solve = Tether->GetSolve();
		const FLinearColor Color = Solve.bTaut ? DangerColor : (Solve.Strain > 0.75f ? WarnColor : TextColor);
		Text(FString::Printf(TEXT("TETHER %.1f m left of %.0f m%s"), Solve.LeftM, Solve.LengthM, Solve.bTaut ? TEXT("   TAUT") : TEXT("")), Color);
	}

	if (const ADeepGameState* State = GetWorld()->GetGameState<ADeepGameState>())
	{
		Text(FString::Printf(TEXT("CREDITS %d   HULL %d   STOP %d   DIVER #%d"), State->Credits, State->HullRating, State->Stop, State->DiverNumber), TextColor);
	}

	// Context prompts, bottom centre.
	TArray<TPair<FString, FLinearColor>> Prompts;
	if (const ADeepSalvage* Target = Diver->FindGrabTarget())
	{
		const FDeepSalvageKind& Kind = Target->GetKind();
		Prompts.Emplace(FString::Printf(TEXT("[%s] Grab %s   %.0f kg   %d cr"), *ADeepPlayerController::KeyHint(TEXT("Grab")), *Kind.Name, Kind.Kg, Kind.Value), TextColor);
	}
	if (Diver->IsWinching())
	{
		Prompts.Emplace(TEXT("WINCHING"), WarnColor);
	}
	else if (Weight.bOverloaded)
	{
		Prompts.Emplace(FString::Printf(TEXT("Too heavy to swim up. Hold [%s] to winch home"), *ADeepPlayerController::KeyHint(TEXT("Winch"))), WarnColor);
	}
	else if (Diver->GetTether() && Diver->GetTether()->GetSolve().bTaut)
	{
		Prompts.Emplace(FString::Printf(TEXT("End of the line. Hold [%s] to winch home"), *ADeepPlayerController::KeyHint(TEXT("Winch"))), WarnColor);
	}

	float PromptY = Canvas->ClipY * 0.72f;
	for (const TPair<FString, FLinearColor>& Prompt : Prompts)
	{
		float W = 0.f, H = 0.f;
		GetTextSize(Prompt.Key, W, H, Font, Scale);
		DrawText(Prompt.Key, Prompt.Value, (Canvas->ClipX - W) * 0.5f, PromptY, Font, Scale);
		PromptY += Line;
	}

	// Centre dot.
	const float Dot = 3.f * Scale;
	DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.6f), Canvas->ClipX * 0.5f - Dot * 0.5f, Canvas->ClipY * 0.5f - Dot * 0.5f, Dot, Dot);

	// Controls, bottom left, small.
	const FString Controls = FString::Printf(TEXT("WASD swim   Mouse look   %s/%s up/down   %s sprint   %s grab   %s drop   hold %s winch   %s lamp"),
		*ADeepPlayerController::KeyHint(TEXT("Up")), *ADeepPlayerController::KeyHint(TEXT("Down")), *ADeepPlayerController::KeyHint(TEXT("Sprint")),
		*ADeepPlayerController::KeyHint(TEXT("Grab")), *ADeepPlayerController::KeyHint(TEXT("Drop")), *ADeepPlayerController::KeyHint(TEXT("Winch")),
		*ADeepPlayerController::KeyHint(TEXT("Lamp")));
	DrawText(Controls, FLinearColor(0.6f, 0.7f, 0.75f, 0.8f), X, Canvas->ClipY - 40.f * Scale, Font, Scale * 0.8f);
}
