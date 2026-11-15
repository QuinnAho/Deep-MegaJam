#include "Story/DeepCaptain.h"

#include "Audio/DeepAudioSubsystem.h"
#include "Debug/DeepStateReport.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/World.h"
#include "Game/DeepGameState.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Monster/DeepMonsterBrain.h"
#include "Monster/DeepSerpent.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepEventBus.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepCaptain, Log, All);

namespace
{
	TAutoConsoleVariable<int32> CVarCaptainRadio(
		TEXT("Deep.Captain.Radio"),
		1,
		TEXT("1: the Captain is a voice on the sub's radio (pacing in the water, tone by danger, squelch, voice files, ducking). 0: the Captain as before D-040."),
		ECVF_Default);
}

bool UDeepCaptain::IsRadioOn()
{
	return CVarCaptainRadio.GetValueOnGameThread() != 0;
}

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
		{ DeepEvent::MonsterFirstHeard, 2 }, { DeepEvent::MonsterRepelled, 2 }, { DeepEvent::MonsterNear, 1 },
		{ DeepEvent::RelicSeen, 2 }, { DeepEvent::RelicGrabbed, 2 }, { DeepEvent::RelicWinch, 1 }, { DeepEvent::Title, 0 },
		{ DeepEvent::HelmDepthLimit, 2 }, { DeepEvent::HelmArrive, 2 }, { DeepEvent::EnterSub, 1 }, { DeepEvent::HelmTake, 1 },
		{ DeepEvent::HelmDescend, 1 }, { DeepEvent::IdleInside, 0 },
		// Other lanes' events (rows arrive with them): the sonar's contact, the shaft's hull.
		{ TEXT("sonar_contact"), 2 }, { TEXT("hull_hit"), 1 }, { TEXT("hull_critical"), 2 }, { TEXT("hull_breach"), 3 },
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

EDeepCaptainScene UDeepCaptain::GatherScene() const
{
	UWorld* World = GetWorld();
	const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
	if (State && State->bOnTitle)
	{
		return EDeepCaptainScene::Title;
	}
	const ADeepDiver* Diver = DeepStateReport::FindDiver(World);
	const UDeepRunComponent* Run = Diver ? Diver->GetRun() : nullptr;
	if (!Run)
	{
		return EDeepCaptainScene::Aboard;
	}
	const EDeepRunState RunState = Run->GetState();
	if (RunState == EDeepRunState::Settling || RunState == EDeepRunState::Won)
	{
		return EDeepCaptainScene::Settlement;
	}
	// Driving the sub (seated, not parked) paces like the water, or every wall hit would get a line. Danger stays calm.
	if (Run->IsSeated() && State && !State->bParked)
	{
		return EDeepCaptainScene::Water;
	}
	return Run->IsInside() ? EDeepCaptainScene::Aboard : EDeepCaptainScene::Water;
}

EDeepCaptainDanger UDeepCaptain::GatherDanger() const
{
	UWorld* World = GetWorld();
	ADeepDiver* Diver = DeepStateReport::FindDiver(World);
	const UDeepRunComponent* Run = Diver ? Diver->GetRun() : nullptr;
	FDeepCaptainDangerInputs In;
	if (!Run || Run->IsInside() || Run->IsOnTitle())
	{
		return EDeepCaptainDanger::Calm;
	}
	In.bInWater = true;
	In.bBiteAlarm = Run->IsBiteAlarmOn();
	In.bTethered = Run->HasTether();
	In.PatienceFraction = Run->GetPatienceFraction();
	In.bBailout = Run->GetState() == EDeepRunState::Cut;
	if (const UDeepMonsterBrain* Brain = World->GetSubsystem<UDeepMonsterBrain>())
	{
		if (const ADeepSerpent* Serpent = Brain->GetSerpent())
		{
			In.SerpentDistanceM = static_cast<float>(FVector::Dist(Serpent->GetHeadLocation(), Diver->GetActorLocation())) / UDeepRules::CmPerM;
			if (Brain->GetSerpentController())
			{
				const EDeepMonsterState MonsterState = Brain->GetState();
				const bool bClosing = MonsterState == EDeepMonsterState::Stalk || MonsterState == EDeepMonsterState::Hunt || MonsterState == EDeepMonsterState::Bite;
				In.bSerpentClosing = bClosing && Brain->GetFocusDiver() == Diver;
			}
			else
			{
				// A client does not know its state: near is enough.
				In.bSerpentClosing = true;
			}
		}
	}
	// HOOK(hull): fill In.Hull01 from ADeepGameState::GetHullIntegrity01() once the shaft lane merges it.
	return DeepCaptainPacing::Danger(In, UDeepTuningSettings::Get().Captain);
}

void UDeepCaptain::HandleEventAt(FName Event, const FDeepEventPayload& Payload, double NowSeconds)
{
	const bool bRadio = IsRadioOn();
	HandleEventIn(Event, Payload, NowSeconds, bRadio ? GatherScene() : EDeepCaptainScene::Aboard, bRadio ? GatherDanger() : EDeepCaptainDanger::Calm);
}

void UDeepCaptain::HandleEventIn(FName Event, const FDeepEventPayload& Payload, double NowSeconds, EDeepCaptainScene Scene, EDeepCaptainDanger Danger)
{
	const FDeepCaptainTuning& Tuning = UDeepTuningSettings::Get().Captain;
	const bool bRadio = IsRadioOn();
	if (bRadio && Lines.HasEvent(Event) && !DeepCaptainPacing::Allows(Scene, Danger, Urgency(Event), NowSeconds - LastShownAt, Tuning))
	{
		++HeldBack;
		UE_LOG(LogDeepCaptain, Verbose, TEXT("Held back %s (%s, %s)"), *Event.ToString(), DeepCaptainPacing::SceneName(Scene), *DeepCaptainPacing::ToneName(Danger).ToString());
		return;
	}
	FDeepCaptainLine Line;
	if (!Lines.Pick(Event, Rng, Line, bRadio ? DeepCaptainPacing::ToneName(Danger) : NAME_None))
	{
		return;
	}
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
	float Duration = FMath::Clamp(Tuning.LineMinSeconds + Words * Tuning.SecondsPerWord, Tuning.LineMinSeconds, FMath::Max(Tuning.LineMinSeconds, Tuning.LineMaxSeconds));
	if (bRadio)
	{
		// On the radio: a voice file keeps the subtitle up for as long as he speaks, never shorter than the text.
		Shown.LineId = Line.Id;
		Shown.bRadio = true;
		if (UDeepAudioSubsystem* Audio = UDeepAudioSubsystem::Get(this))
		{
			Duration = FMath::Max(Duration, Audio->GetVoiceHoldSeconds(Line.Id));
		}
	}
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
	LastShownAt = NowSeconds;
	// The radio opens with a squelch, plays the voice file if there is one and closes when the line ends.
	if (Line.bRadio)
	{
		if (UDeepAudioSubsystem* Audio = UDeepAudioSubsystem::Get(this))
		{
			Audio->KeyRadio(Line.LineId, static_cast<float>(Duration));
		}
	}
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
