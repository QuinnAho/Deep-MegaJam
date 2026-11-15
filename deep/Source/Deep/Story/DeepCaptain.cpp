#include "Story/DeepCaptain.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepEventBus.h"

UDeepCaptain* UDeepCaptain::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDeepCaptain>() : nullptr;
}

void UDeepCaptain::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Rng.Initialize(static_cast<int32>(FPlatformTime::Cycles()));
	Lines.LoadDefault();
	if (UDeepEventBus* Bus = Collection.InitializeDependency<UDeepEventBus>())
	{
		BusHandle = Bus->OnEvent.AddUObject(this, &UDeepCaptain::OnBusEvent);
	}
}

void UDeepCaptain::Deinitialize()
{
	if (UDeepEventBus* Bus = UDeepEventBus::Get(this))
	{
		Bus->OnEvent.Remove(BusHandle);
	}
	Super::Deinitialize();
}

int32 UDeepCaptain::Urgency(FName Event)
{
	static const TMap<FName, int32> Table = {
		{ DeepEvent::CutPatience, 3 }, { DeepEvent::CutRequested, 3 }, { TEXT("cut_bite"), 3 },
		{ DeepEvent::DeathAir, 3 }, { TEXT("death_monster"), 3 }, { DeepEvent::Bailout10s, 3 },
		{ DeepEvent::Retether, 2 }, { DeepEvent::Respawn, 2 }, { DeepEvent::BankGood, 2 }, { DeepEvent::BankPoor, 2 },
		{ DeepEvent::BankEmpty, 2 }, { DeepEvent::HullBought, 2 }, { DeepEvent::DescendStop2, 2 }, { DeepEvent::DescendStop3, 2 },
		{ DeepEvent::Patience25, 2 }, { DeepEvent::Patience10, 2 }, { TEXT("bite_alarm"), 3 }, { TEXT("win"), 3 },
		{ DeepEvent::Patience50, 1 }, { DeepEvent::Overweight, 1 }, { DeepEvent::FirstSalvage, 1 }, { DeepEvent::DiveStart, 1 },
		{ DeepEvent::TetherTaut, 1 }, { DeepEvent::WinchStart, 1 },
	};
	const int32* Found = Table.Find(Event);
	return Found ? *Found : 0;
}

double UDeepCaptain::Now() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetRealTimeSeconds() : 0.0;
}

void UDeepCaptain::OnBusEvent(FName Event, const FDeepEventPayload& Payload)
{
	// On a listen server the bus also carries remote players' divers; only speak to the local one.
	if (const APawn* Pawn = Cast<APawn>(Payload.Source))
	{
		if (Pawn->GetController() && !Pawn->IsLocallyControlled())
		{
			return;
		}
	}
	HandleEventAt(Event, Payload, Now());
}

void UDeepCaptain::HandleEventAt(FName Event, const FDeepEventPayload& Payload, double NowSeconds)
{
	FDeepCaptainLine Line;
	if (!Lines.Pick(Event, Rng, Line))
	{
		return;
	}
	const FDeepCaptainTuning& Tuning = UDeepTuningSettings::Get().Captain;
	FShown Shown;
	Shown.Text = FDeepCaptainLines::FillTokens(Line.Text, Payload);
	Shown.Event = Event;
	Shown.Urgency = Urgency(Event);
	Shown.Start = NowSeconds;
	int32 Words = 1;
	for (const TCHAR Char : Shown.Text)
	{
		Words += Char == TEXT(' ') ? 1 : 0;
	}
	const float Duration = FMath::Clamp(Tuning.LineMinSeconds + Words * Tuning.SecondsPerWord, Tuning.LineMinSeconds, FMath::Max(Tuning.LineMinSeconds, Tuning.LineMaxSeconds));
	Shown.End = NowSeconds + Duration;
	LastText = Shown.Text;

	const bool bCurrentShowing = Current.IsValid() && NowSeconds < Current.End;
	if (!bCurrentShowing || Shown.Urgency > Current.Urgency)
	{
		Show(Shown, NowSeconds);
	}
	else if (!Pending.IsValid() || Shown.Urgency >= Pending.Urgency)
	{
		Pending = Shown;
	}
}

void UDeepCaptain::Show(const FShown& Line, double NowSeconds)
{
	const double Duration = Line.End - Line.Start;
	Current = Line;
	Current.Start = NowSeconds;
	Current.End = NowSeconds + Duration;
}

FString UDeepCaptain::GetLineAt(double NowSeconds)
{
	if (Current.IsValid() && NowSeconds >= Current.End)
	{
		Current = FShown();
		const float MaxAge = UDeepTuningSettings::Get().Captain.PendingMaxAgeSeconds;
		if (Pending.IsValid() && NowSeconds - Pending.Start <= MaxAge)
		{
			Show(Pending, NowSeconds);
		}
		Pending = FShown();
	}
	return Current.IsValid() ? FString::Printf(TEXT("CAPTAIN: %s"), *Current.Text) : FString();
}

FString UDeepCaptain::GetLine()
{
	return GetLineAt(Now());
}

float UDeepCaptain::GetLineProgressAt(double NowSeconds) const
{
	if (!Current.IsValid() || Current.End <= Current.Start)
	{
		return 1.f;
	}
	return FMath::Clamp(static_cast<float>((NowSeconds - Current.Start) / (Current.End - Current.Start)), 0.f, 1.f);
}
