#include "Immersion/DeepImmersion.h"

#include "Audio/DeepAudioSubsystem.h"
#include "Debug/DeepStateReport.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Game/DeepGameState.h"
#include "Game/DeepHUD.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Immersion/DeepSilt.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Sonar/DeepSonarComponent.h"
#include "Sonar/DeepSonarRules.h"
#include "Story/DeepEventBus.h"
#include "World/DeepArena.h"
#include "World/DeepSubmarine.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepImmersion, Log, All);

static TAutoConsoleVariable<int32> CVarDeepImmersion(
	TEXT("Deep.Immersion"), 1,
	TEXT("Immersion I1 (D-072): 1 = the hull's creaks, groans and flow, the silt it lifts and the sonar's echo; 0 = none of it."));

namespace
{
	const FName ImmHullCreakRow(DeepAudioImmersion::HullCreak);
	const FName ImmHullGroanRow(DeepAudioImmersion::HullGroan);
	const FName ImmSonarEchoRow(DeepAudioImmersion::SonarEcho);
	/** A change of depth or heading bigger than these in one step is a jump (a cheat, a stop loading), not the hull working. */
	constexpr float ImmJumpM = 50.f;
	constexpr float ImmJumpDeg = 45.f;

	double ImmRound2(float V) { return FMath::RoundToDouble(double(V) * 100.0) / 100.0; }
}

UDeepImmersion* UDeepImmersion::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDeepImmersion>() : nullptr;
}

bool UDeepImmersion::IsOn()
{
	return CVarDeepImmersion.GetValueOnGameThread() != 0;
}

bool UDeepImmersion::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UDeepImmersion::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UDeepEventBus>();
	Collection.InitializeDependency<UDeepAudioSubsystem>();
	if (UDeepEventBus* Bus = UDeepEventBus::Get(this))
	{
		EventHandle = Bus->OnEvent.AddUObject(this, &UDeepImmersion::HandleEvent);
	}
}

void UDeepImmersion::Deinitialize()
{
	if (UDeepEventBus* Bus = UDeepEventBus::Get(this))
	{
		Bus->OnEvent.Remove(EventHandle);
	}
	Super::Deinitialize();
}

TStatId UDeepImmersion::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDeepImmersion, STATGROUP_Tickables);
}

bool UDeepImmersion::IsTickable() const
{
	const UWorld* World = GetWorld();
	return World && World->HasBegunPlay() && World->GetNetMode() != NM_DedicatedServer;
}

void UDeepImmersion::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	// The silt lives where the arena does; it hides itself while Deep.Immersion is 0.
	if (IsOn() && DeepArena::FindSubmarine(World))
	{
		ADeepSilt::Ensure(World);
	}
	StepVoice(DeltaTime);
}

void UDeepImmersion::ResetVoice()
{
	CreakClock = FDeepCreakClock();
	DeepestBand = -1;
	LastDepthM = -1.f;
	bWasHearing = false;
}

bool UDeepImmersion::IsHearingHull() const
{
	UWorld* World = GetWorld();
	const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
	const ADeepDiver* Diver = DeepStateReport::FindDiver(World);
	const UDeepRunComponent* Run = Diver ? Diver->GetRun() : nullptr;
	return IsOn() && State && !State->bOnTitle && Run && Run->IsInside();
}

void UDeepImmersion::StepVoice(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
	if (!State)
	{
		return;
	}
	const FDeepHullVoiceTuning& Voice = UDeepTuningSettings::Get().Immersion.Voice;
	const float Dt = FMath::Max(0.f, DeltaSeconds);
	// A new run (the title, or the run clock starting over) forgets the bands passed.
	if (State->bOnTitle || State->RunSeconds + 0.5f < LastRunSeconds)
	{
		ResetVoice();
	}
	LastRunSeconds = State->RunSeconds;

	const float DepthM = State->SubDepthM;
	float DepthRate = 0.f;
	float YawRate = 0.f;
	if (LastDepthM >= 0.f && Dt > 0.f)
	{
		const float Moved = DepthM - LastDepthM;
		const float Turned = FRotator::NormalizeAxis(State->SubYawDeg - LastYawDeg);
		DepthRate = FMath::Abs(Moved) < ImmJumpM ? Moved / Dt : 0.f;
		YawRate = FMath::Abs(Turned) < ImmJumpDeg ? Turned / Dt : 0.f;
	}
	LastDepthM = DepthM;
	LastYawDeg = State->SubYawDeg;

	const bool bHearing = IsHearingHull();
	// The bands are counted whoever hears them: the sub passed it once, whether or not this machine's diver was aboard.
	if (!State->bOnTitle && DeepImmersionRules::StepGroan(DeepestBand, DepthM, Voice) && bHearing)
	{
		PlayGroanNow();
	}
	if (!bHearing)
	{
		bWasHearing = false;
		return;
	}
	if (!bWasHearing)
	{
		// Just aboard (or the run just began): the clock starts from nothing, so no creak greets the player at once.
		bWasHearing = true;
		CreakClock.Charge = 0.f;
		CreakClock.SinceLastS = 0.f;
	}
	FDeepCreakInputs In;
	In.DepthM = DepthM;
	In.YawRateDegS = YawRate;
	In.DepthRateMps = DepthRate;
	In.bParked = State->bParked;
	const float RandA = Rand.FRand();
	const float RandB = Rand.FRand();
	const FDeepCreak Creak = DeepImmersionRules::StepCreak(CreakClock, In, Dt, RandA, RandB, Voice);
	if (Creak.bFire)
	{
		++Creaks;
		if (UDeepAudioSubsystem* Audio = UDeepAudioSubsystem::Get(this))
		{
			Audio->PlayEventScaled(ImmHullCreakRow, Creak.Volume, Creak.Pitch, true);
		}
		UE_LOG(LogDeepImmersion, Display, TEXT("DEEPIMMERSION {\"what\":\"creak\",\"depth_m\":%.1f,\"pitch\":%.2f,\"volume\":%.2f,\"stress\":%s,\"count\":%d}"),
			DepthM, Creak.Pitch, Creak.Volume, Creak.bStress ? TEXT("true") : TEXT("false"), Creaks);
	}
}

void UDeepImmersion::PlayCreakNow(bool bStress)
{
	const UWorld* World = GetWorld();
	const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
	const FDeepHullVoiceTuning& Voice = UDeepTuningSettings::Get().Immersion.Voice;
	const float DepthM = State ? State->SubDepthM : Voice.ShallowM;
	// The rules' own creak: a clock already full, with a turn when asked for stress.
	FDeepCreakClock Full;
	Full.Charge = 1000.f;
	Full.Threshold = 0.f;
	FDeepCreakInputs In;
	In.DepthM = DepthM;
	In.YawRateDegS = bStress ? Voice.TurnFullDegS : 0.f;
	const FDeepCreak Creak = DeepImmersionRules::StepCreak(Full, In, 0.f, Rand.FRand(), Rand.FRand(), Voice);
	++Creaks;
	if (UDeepAudioSubsystem* Audio = UDeepAudioSubsystem::Get(this))
	{
		Audio->PlayEventScaled(ImmHullCreakRow, Creak.Volume, Creak.Pitch, true);
	}
	UE_LOG(LogDeepImmersion, Display, TEXT("DEEPIMMERSION {\"what\":\"creak\",\"depth_m\":%.1f,\"pitch\":%.2f,\"volume\":%.2f,\"stress\":%s,\"count\":%d,\"forced\":true}"),
		DepthM, Creak.Pitch, Creak.Volume, Creak.bStress ? TEXT("true") : TEXT("false"), Creaks);
}

void UDeepImmersion::PlayGroanNow()
{
	const UWorld* World = GetWorld();
	const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
	const FDeepHullVoiceTuning& Voice = UDeepTuningSettings::Get().Immersion.Voice;
	const float DepthM = State ? State->SubDepthM : Voice.ShallowM;
	++Groans;
	CreakClock.QuietS = FMath::Max(CreakClock.QuietS, Voice.CreakQuietAfterGroanS);
	if (UDeepAudioSubsystem* Audio = UDeepAudioSubsystem::Get(this))
	{
		Audio->PlayEventScaled(ImmHullGroanRow, 1.f, DeepImmersionRules::GroanPitch(DepthM, Voice), true);
	}
	UE_LOG(LogDeepImmersion, Display, TEXT("DEEPIMMERSION {\"what\":\"groan\",\"depth_m\":%.1f,\"band\":%d,\"count\":%d}"), DepthM, DeepestBand, Groans);
}

TArray<FDeepEchoTap> UDeepImmersion::ScheduleEcho(const TArray<float>& RangesM)
{
	const FDeepEchoTuning& Echo = UDeepTuningSettings::Get().Immersion.Echo;
	const TArray<FDeepEchoTap> Taps = DeepImmersionRules::EchoTaps(RangesM, Echo);
	LastEchoRanges = RangesM;
	LastEcho = Taps;
	if (Taps.IsEmpty())
	{
		return Taps;
	}
	++Echoes;
	if (UDeepAudioSubsystem* Audio = UDeepAudioSubsystem::Get(this))
	{
		for (const FDeepEchoTap& Tap : Taps)
		{
			Audio->PlayEventAfter(ImmSonarEchoRow, Tap.DelayS, Tap.Gain, 1.f, Tap.LowPassHz);
		}
	}
	FString List;
	for (const FDeepEchoTap& Tap : Taps)
	{
		List += FString::Printf(TEXT("%s{\"m\":%.1f,\"delay_s\":%.2f,\"gain\":%.2f,\"lp_hz\":%.0f}"), List.IsEmpty() ? TEXT("") : TEXT(","),
			Tap.DistanceM, Tap.DelayS, Tap.Gain, Tap.LowPassHz);
	}
	UE_LOG(LogDeepImmersion, Display, TEXT("DEEPIMMERSION {\"what\":\"echo\",\"beams\":%d,\"taps\":[%s],\"count\":%d}"), RangesM.Num(), *List, Echoes);
	return Taps;
}

TArray<float> UDeepImmersion::HelmSonarRanges() const
{
	TArray<float> Out;
	const UWorld* World = GetWorld();
	const APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
	const ADeepHUD* Hud = Player ? Cast<ADeepHUD>(Player->GetHUD()) : nullptr;
	const UDeepSonarComponent* Sonar = Hud ? Hud->GetSonar() : nullptr;
	if (!Sonar || !Sonar->IsWorking())
	{
		return Out;
	}
	// The sweep's own samples, as it last traced each beam: no new trace for the echo.
	for (int32 Slice = 0; Slice < 2; ++Slice)
	{
		for (const FDeepSonarSample& Sample : Sonar->GetSlice(Slice))
		{
			Out.Add(Sample.RangeM);
		}
	}
	return Out;
}

TArray<float> UDeepImmersion::RingRanges(const FVector& OriginCm, const AActor* Ignore) const
{
	TArray<float> Out;
	UWorld* World = GetWorld();
	if (!World)
	{
		return Out;
	}
	const FDeepEchoTuning& Echo = UDeepTuningSettings::Get().Immersion.Echo;
	const int32 Beams = FMath::Clamp(Echo.DiverBeams, 4, 32);
	const double ReachCm = FMath::Max(5.f, Echo.DiverRangeM) * UDeepRules::CmPerM;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DeepEchoRing), true, Ignore);
	if (const ADeepSubmarine* Sub = DeepArena::FindSubmarine(World))
	{
		Params.AddIgnoredActor(Sub);
	}
	for (int32 Beam = 0; Beam < Beams; ++Beam)
	{
		const double Rad = 2.0 * PI * Beam / Beams;
		const FVector Dir(FMath::Cos(Rad), FMath::Sin(Rad), 0.0);
		FHitResult Hit;
		Out.Add(World->LineTraceSingleByChannel(Hit, OriginCm, OriginCm + Dir * ReachCm, ECC_Visibility, Params)
			? static_cast<float>(Hit.Distance / UDeepRules::CmPerM) : -1.f);
	}
	return Out;
}

void UDeepImmersion::HandleEvent(FName Event, const FDeepEventPayload& Payload)
{
	if (Event != FName(DeepSonarEvent::Ping) || !IsOn())
	{
		return;
	}
	TArray<float> Ranges;
	AActor* Source = Payload.Source.Get();
	if (Cast<ADeepSubmarine>(Source))
	{
		// The helm's ping: only the pilot's machine has a working sweep (others get no ping and no echo).
		Ranges = HelmSonarRanges();
	}
	else if (const APawn* Pawn = Cast<APawn>(Source); Pawn && Pawn->IsLocallyControlled())
	{
		// A diver's own ping (V): one ring of beams, once. Another player's ping is not played here (UDeepAudioSubsystem).
		Ranges = RingRanges(Pawn->GetActorLocation(), Pawn);
	}
	if (!Ranges.IsEmpty())
	{
		ScheduleEcho(Ranges);
	}
}

TSharedRef<FJsonObject> UDeepImmersion::BuildStateJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetBoolField(TEXT("on"), IsOn());
	Json->SetBoolField(TEXT("hearing"), IsHearingHull());
	Json->SetNumberField(TEXT("creaks"), Creaks);
	Json->SetNumberField(TEXT("groans"), Groans);
	Json->SetNumberField(TEXT("band"), DeepestBand);
	Json->SetNumberField(TEXT("echoes"), Echoes);
	TArray<TSharedPtr<FJsonValue>> Taps;
	for (const FDeepEchoTap& Tap : LastEcho)
	{
		TSharedRef<FJsonObject> T = MakeShared<FJsonObject>();
		T->SetNumberField(TEXT("m"), ImmRound2(Tap.DistanceM));
		T->SetNumberField(TEXT("delay_s"), ImmRound2(Tap.DelayS));
		T->SetNumberField(TEXT("gain"), ImmRound2(Tap.Gain));
		T->SetNumberField(TEXT("lp_hz"), FMath::RoundToDouble(Tap.LowPassHz));
		Taps.Add(MakeShared<FJsonValueObject>(T));
	}
	Json->SetArrayField(TEXT("echo"), Taps);
	if (const ADeepSilt* Silt = ADeepSilt::Find(GetWorld()))
	{
		Json->SetObjectField(TEXT("silt"), Silt->BuildStateJson());
	}
	return Json;
}
