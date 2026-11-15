#include "Harpoon/DeepHarpoonHUD.h"

#include "Diver/DeepDiver.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Game/DeepGameState.h"
#include "GameFramework/HUD.h"
#include "Harpoon/DeepHarpoonComponent.h"
#include "Harpoon/DeepHarpoonSettings.h"

namespace
{
	const FLinearColor ReadyColor(0.35f, 1.f, 0.55f);
	const FLinearColor SpentColor(0.55f, 0.6f, 0.62f, 0.9f);
	const FLinearColor SubColor(1.f, 0.75f, 0.2f);
	const FLinearColor TerrainColor(0.9f, 0.95f, 1.f);
	const FLinearColor MonsterColor(1.f, 0.25f, 0.2f);
	const FLinearColor FarColor(0.6f, 0.65f, 0.7f, 0.6f);
	const FLinearColor Panel(0.f, 0.02f, 0.04f, 0.85f);

	float Backed(AHUD* Hud, const FString& Text, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale, bool bCentre)
	{
		float W = 0.f, H = 0.f;
		Hud->GetTextSize(Text, W, H, Font, Scale);
		const float Left = bCentre ? X - W * 0.5f : X;
		Hud->DrawRect(Panel, Left - 6.f, Y - 1.f, W + 12.f, H + 2.f);
		Hud->DrawText(Text, Color, Left, Y, Font, Scale);
		return H;
	}

	void Brackets(AHUD* Hud, float CX, float CY, float Half, float Arm, float Thick, const FLinearColor& Color)
	{
		for (const float SX : { -1.f, 1.f })
		{
			for (const float SY : { -1.f, 1.f })
			{
				const float X = CX + SX * Half;
				const float Y = CY + SY * Half;
				Hud->DrawLine(X, Y, X - SX * Arm, Y, Color, Thick);
				Hud->DrawLine(X, Y, X, Y - SY * Arm, Color, Thick);
			}
		}
	}
}

void DeepHarpoonHUD::Draw(AHUD* Hud, UCanvas* Canvas, const ADeepDiver* Diver, float S, UFont* Font, UFont* BigFont)
{

	const UDeepHarpoonComponent* Harpoon = Diver ? Diver->GetHarpoon() : nullptr;
	if (!Hud || !Canvas || !Harpoon)
	{
		return;
	}
	const UDeepHarpoonSettings& Settings = UDeepHarpoonSettings::Get();
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const EDeepHarpoonPhase Phase = Harpoon->GetPhase();

	// Crosshair: brackets when a ready shot would hit something in range, grey when it is too far.
	if (Harpoon->IsReady())
	{
		FVector Origin, Direction;
		if (Harpoon->GetView(Origin, Direction))
		{
			const float RangeCm = Settings.RangeM * 100.f;
			const FDeepHarpoonTrace Aim = Harpoon->TraceAim(Origin, Direction, RangeCm * Settings.AimLookAheadFactor);
			if (Aim.Target != EDeepHarpoonTarget::None)
			{
				const bool bInRange = Aim.DistanceCm <= RangeCm;
				const FLinearColor Color = !bInRange ? FarColor
					: (Aim.Target == EDeepHarpoonTarget::Submarine ? SubColor : (Aim.Target == EDeepHarpoonTarget::Monster ? MonsterColor : TerrainColor));
				Brackets(Hud, CX, CY, (bInRange ? 14.f : 9.f) * S, 6.f * S, bInRange ? 2.f * S : 1.f * S, Color);
				const FString What = Aim.Target == EDeepHarpoonTarget::Submarine ? TEXT("SUB ") : (Aim.Target == EDeepHarpoonTarget::Monster ? TEXT("SERPENT ") : TEXT(""));
				const FString Label = bInRange ? FString::Printf(TEXT("%s%.0f m"), *What, Aim.DistanceCm / 100.f)
					: FString::Printf(TEXT("%.0f m - out of range"), Aim.DistanceCm / 100.f);
				float W = 0.f, H = 0.f;
				Hud->GetTextSize(Label, W, H, Font, S * 0.55f);
				Hud->DrawText(Label, Color, CX + 20.f * S, CY - H * 0.5f, Font, S * 0.55f);
			}
		}
	}

	// Indicator, bottom right.
	FString Status;
	FLinearColor StatusColor = SpentColor;
	if (Phase == EDeepHarpoonPhase::Flying)
	{
		Status = TEXT("HARPOON  ---->");
		StatusColor = TerrainColor;
	}
	else if (Phase == EDeepHarpoonPhase::Latched)
	{
		Status = Harpoon->GetLatchTarget() == EDeepHarpoonTarget::Submarine ? TEXT("HARPOON  REELING IN") : TEXT("HARPOON  PULLING  (release LMB to let go)");
		StatusColor = SubColor;
	}
	else if (Harpoon->GetShotsLeft() > 0)
	{
		Status = Harpoon->GetShotsLeft() > 1 ? FString::Printf(TEXT("HARPOON  READY x%d  [LMB]"), Harpoon->GetShotsLeft()) : FString(TEXT("HARPOON  READY  [LMB]"));
		StatusColor = ReadyColor;
	}
	else
	{
		Status = TEXT("HARPOON  SPENT  (reloads next dive)");
	}
	const float Scale = S * 0.7f;
	float W = 0.f, H = 0.f;
	Hud->GetTextSize(Status, W, H, Font, Scale);
	const float X = Canvas->ClipX - W - 16.f * S;
	float Y = Canvas->ClipY - H * 2.4f - 12.f * S;
	Backed(Hud, Status, StatusColor, X, Y, Font, Scale, false);
	if (const ADeepGameState* State = Diver->GetWorld() ? Diver->GetWorld()->GetGameState<ADeepGameState>() : nullptr; State && State->HullDamage > 0)
	{
		const FString Hull = State->HullRepairsOwed > 0
			? FString::Printf(TEXT("hull dents %d   repairs owed %d cr"), State->HullDamage, State->HullRepairsOwed * FMath::Max(0, Settings.RepairFeePerHit))
			: FString::Printf(TEXT("hull dents %d"), State->HullDamage);
		Backed(Hud, Hull, SpentColor, X, Y + H * 1.2f, Font, S * 0.55f, false);
	}

	// The message after the last thing the harpoon did.
	if (Harpoon->GetSecondsSinceResult() > Settings.MessageSeconds)
	{
		return;
	}
	FString Message;
	FLinearColor MessageColor = TerrainColor;
	switch (Harpoon->GetLastResult())
	{
	case EDeepHarpoonResult::LatchedSub:
		Message = FString::Printf(TEXT("LATCHED TO THE SUB - REELING IN   (hull repair -%d cr)"), FMath::Max(0, Settings.RepairFeePerHit));
		MessageColor = SubColor;
		break;
	case EDeepHarpoonResult::LatchedTerrain:
		Message = TEXT("LATCHED - RELEASE LMB TO LET GO");
		break;
	case EDeepHarpoonResult::HitMonster:
		Message = TEXT("HIT THE SERPENT - IT RECOILS");
		MessageColor = MonsterColor;
		break;
	case EDeepHarpoonResult::Missed:
		Message = TEXT("MISSED");
		MessageColor = SpentColor;
		break;
	case EDeepHarpoonResult::OutOfRange:
		Message = FString::Printf(TEXT("OUT OF RANGE  (%.0f m, the line is %.0f m)"), Harpoon->GetLastOutOfRangeM(), Settings.RangeM);
		MessageColor = SpentColor;
		break;
	case EDeepHarpoonResult::Empty:
		Message = TEXT("NO HARPOON LEFT THIS DIVE");
		MessageColor = MonsterColor;
		break;
	default:
		break;
	}
	if (!Message.IsEmpty())
	{
		// Just above the crosshair: the prompts sit below it and the warning banner higher up.
		UFont* Use = BigFont ? BigFont : Font;
		float MW = 0.f, MH = 0.f;
		Hud->GetTextSize(Message, MW, MH, Use, S * 0.6f);
		Backed(Hud, Message, MessageColor, CX, CY - 30.f * S - MH, Use, S * 0.6f, true);
	}
}
