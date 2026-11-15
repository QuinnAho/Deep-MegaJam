#include "Sonar/DeepSonarComponent.h"

#include "Audio/DeepAudioSubsystem.h"
#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/World.h"
#include "Game/DeepGameState.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Harpoon/DeepHarpoonBolt.h"
#include "Misc/App.h"
#include "Monster/DeepSerpent.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Sonar/DeepSonarRules.h"
#include "Story/DeepEventBus.h"
#include "UI/DeepHudFrame.h"
#include "World/DeepAbyssFloor.h"
#include "World/DeepArena.h"
#include "World/DeepSalvage.h"
#include "World/DeepSubmarine.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepSonar, Log, All);

namespace
{
	// The ping on the bus too (camera lane, Nov 15): the camera draws the pulse from it. Source = the sub (the pulse
	// origin), DepthM = the sonar range. The audio repeat guard drops the second play of the sound in the same frame.
	void BroadcastPing(const UObject* Context, AActor* SubActor, float RangeM)
	{
		if (UDeepEventBus* Bus = UDeepEventBus::Get(Context))
		{
			FDeepEventPayload Payload;
			Payload.DepthM = RangeM;
			Payload.Source = SubActor;
			Bus->Broadcast(FName(DeepSonarEvent::Ping), Payload);
		}
	}

	const FDeepSonarTuning& Tune() { return UDeepTuningSettings::Get().Sonar; }

	/** Distance from the centre to the side of a box hull (half sizes in metres, x bow, y beam) along a bearing. */
	float HullEdgeM(const FVector2D& HalfM, float BearingDeg)
	{
		if (HalfM.X <= 0.f || HalfM.Y <= 0.f)
		{
			return 0.f;
		}
		const float Rad = FMath::DegreesToRadians(BearingDeg);
		const float C = FMath::Abs(FMath::Cos(Rad));
		const float S = FMath::Abs(FMath::Sin(Rad));
		return FMath::Min(C > 1e-4f ? HalfM.X / C : 1e6f, S > 1e-4f ? HalfM.Y / S : 1e6f);
	}
}

UDeepSonarComponent::UDeepSonarComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(false);
}

void UDeepSonarComponent::SetSonarEnabled(bool bInEnabled)
{
	bSonarEnabled = bInEnabled;
	if (!bSonarEnabled)
	{
		Proximity01 = 0.f;
	}
}

bool UDeepSonarComponent::IsManned() const
{
	if (bTestFrame)
	{
		return true;
	}
	const AActor* Owner = GetOwner();
	const APawn* Pawn = nullptr;
	if (const AHUD* Hud = Cast<AHUD>(Owner))
	{
		Pawn = Hud->GetOwningPawn();
	}
	else if (const APlayerController* PC = Cast<APlayerController>(Owner))
	{
		Pawn = PC->GetPawn();
	}
	else
	{
		Pawn = Cast<APawn>(Owner);
	}
	const ADeepDiver* Diver = Cast<ADeepDiver>(Pawn);
	const UDeepRunComponent* Run = Diver ? Diver->GetRun() : nullptr;
	return Run && Run->IsSeated();
}

void UDeepSonarComponent::SetTestFrame(const FVector& OriginCm, const FVector& Forward, const FVector2D& InHullHalfM)
{
	bTestFrame = true;
	TestOrigin = OriginCm;
	TestForward = Forward;
	TestHullHalfM = InHullHalfM;
}

bool UDeepSonarComponent::GetFrame(FVector& OriginCm, FVector& Forward, FVector2D& OutHullHalfM) const
{
	if (bTestFrame)
	{
		OriginCm = TestOrigin;
		Forward = TestForward;
		OutHullHalfM = TestHullHalfM;
		return true;
	}
	if (!Sub.IsValid())
	{
		Sub = DeepArena::FindSubmarine(GetWorld());
	}
	const ADeepSubmarine* S = Sub.Get();
	if (!S)
	{
		return false;
	}
	OriginCm = S->GetActorLocation();
	// Up on the scope is where the helm looks; the sub never turns, so this is also its bow.
	const UCameraComponent* Camera = S->GetHelmCamera();
	Forward = Camera ? Camera->GetForwardVector() : S->GetActorForwardVector();
	if (FVector2D(Forward.X, Forward.Y).IsNearlyZero(1e-3))
	{
		Forward = S->GetActorForwardVector();
	}
	const FBox Hull = S->GetSolidBoundsLocal(true);
	OutHullHalfM = Hull.IsValid ? FVector2D(Hull.GetExtent().X, Hull.GetExtent().Y) / UDeepRules::CmPerM : FVector2D::ZeroVector;
	return true;
}

void UDeepSonarComponent::EnsureBeams()
{
	const int32 N = FMath::Max(8, Tune().Beams);
	if (Upper.Num() != N)
	{
		Upper.SetNum(N);
		Lower.SetNum(N);
		Cursor = 0;
		Backlog = 0;
	}
}

void UDeepSonarComponent::BuildQuery(FCollisionQueryParams& Params) const
{
	UWorld* World = GetWorld();
	if (Sub.IsValid())
	{
		Params.AddIgnoredActor(Sub.Get());
	}
	// Divers and the serpent (pawns), salvage (its grab box blocks Visibility) and bolts are contacts, not walls.
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	for (TActorIterator<ADeepSalvage> It(World); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	for (TActorIterator<ADeepHarpoonBolt> It(World); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}
	// A stop's bed is hidden (and still collides) while the sub is below that stop: the sonar sees what the window sees.
	for (TActorIterator<ADeepAbyssFloor> It(World); It; ++It)
	{
		if (It->IsHidden())
		{
			Params.AddIgnoredActor(*It);
		}
	}
}

float UDeepSonarComponent::TraceBeam(const FVector& FromCm, const FVector& Dir) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return -1.f;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DeepSonar), false, GetOwner());
	BuildQuery(Params);
	FHitResult Hit;
	const float ReachCm = Tune().RangeM * UDeepRules::CmPerM;
	if (World->LineTraceSingleByChannel(Hit, FromCm, FromCm + Dir.GetSafeNormal() * ReachCm, ECC_Visibility, Params))
	{
		return static_cast<float>(Hit.Distance / UDeepRules::CmPerM);
	}
	return -1.f;
}

void UDeepSonarComponent::TraceBeamIndex(int32 Beam, const FVector& OriginCm, const FVector& Forward, const FCollisionQueryParams& Params)
{
	UWorld* World = GetWorld();
	const FDeepSonarTuning& T = Tune();
	const FVector Dir = UDeepSonarRules::BeamDirection(Forward, Beam, Upper.Num());
	const float ReachCm = T.RangeM * UDeepRules::CmPerM;
	for (int32 Slice = 0; Slice < 2; ++Slice)
	{
		const FVector From = OriginCm - FVector(0.f, 0.f, Slice == 0 ? 0.f : LowerBelowM * UDeepRules::CmPerM);
		FHitResult Hit;
		FDeepSonarSample& Sample = Slice == 0 ? Upper[Beam] : Lower[Beam];
		Sample.RangeM = World->LineTraceSingleByChannel(Hit, From, From + Dir * ReachCm, ECC_Visibility, Params)
			? static_cast<float>(Hit.Distance / UDeepRules::CmPerM) : -1.f;
		Sample.At = Clock;
		++TracesLastStep;
	}
}

void UDeepSonarComponent::UpdateContacts(const FVector& OriginCm, const FVector& Forward, float PrevDeg, float NewDeg, bool bAll)
{
	UWorld* World = GetWorld();
	const FDeepSonarTuning& T = Tune();
	auto See = [&](AActor* Actor, EDeepSonarContact Kind, const FVector& At, float HeightM)
	{
		const FVector Offset = At - OriginCm;
		const float Range = UDeepSonarRules::PlanRangeM(Offset);
		const float Dz = static_cast<float>(Offset.Z / UDeepRules::CmPerM);
		const float Bearing = UDeepSonarRules::BearingDeg(Forward, Offset);
		if (Range > T.RangeM || FMath::Abs(Dz) > HeightM)
		{
			return;
		}
		if (!bAll && !UDeepSonarRules::SweptOver(Bearing, PrevDeg, NewDeg))
		{
			return;
		}
		FDeepSonarContactSample* Found = Contacts.FindByPredicate([Actor](const FDeepSonarContactSample& C) { return C.Actor.Get() == Actor; });
		if (!Found)
		{
			Found = &Contacts.AddDefaulted_GetRef();
			Found->Actor = Actor;
		}
		Found->Kind = Kind;
		Found->BearingDeg = Bearing;
		Found->RangeM = Range;
		Found->DzM = Dz;
		Found->At = Clock;
	};
	for (TActorIterator<ADeepSalvage> It(World); It; ++It)
	{
		if (!It->IsHidden())
		{
			See(*It, It->GetKind().Size == EDeepSalvageSize::Relic ? EDeepSonarContact::Relic : EDeepSonarContact::Salvage, It->GetActorLocation(), T.ContactHeightM);
		}
	}
	for (TActorIterator<ADeepSerpent> It(World); It; ++It)
	{
		if (!It->IsHidden())
		{
			See(*It, EDeepSonarContact::Serpent, It->GetHeadLocation(), T.SerpentHeightM);
		}
	}
	// The Captain calls the serpent's first contact at each stop (sonar_contact, DepthM = its range). The sonar is the
	// helmsman's own, so this goes on the bus of the world it runs in: the Captain answers the one at the helm.
	const ADeepGameState* Game = World ? World->GetGameState<ADeepGameState>() : nullptr;
	const int32 StopNow = Game ? Game->Stop : 0;
	if (UDeepSonarRules::SerpentCallDue(SerpentCalledStop, StopNow))
	{
		const FDeepSonarContactSample* Serpent = Contacts.FindByPredicate([this](const FDeepSonarContactSample& C)
		{
			return C.Kind == EDeepSonarContact::Serpent && C.At == Clock && C.Actor.IsValid();
		});
		if (Serpent)
		{
			SerpentCalledStop = StopNow;
			++ContactCalls;
			if (UDeepEventBus* Bus = UDeepEventBus::Get(this))
			{
				FDeepEventPayload Payload;
				Payload.DepthM = Serpent->RangeM;
				Payload.Source = Serpent->Actor.Get();
				Bus->Broadcast(DeepSonarEvent::Contact, Payload);
			}
		}
	}
	// A contact the sweep has not refreshed fades with its return; gone ones drop.
	Contacts.RemoveAll([&](const FDeepSonarContactSample& C)
	{
		return !C.Actor.IsValid() || Clock - C.At >= T.PersistS;
	});
}

void UDeepSonarComponent::UpdateNearest()
{
	const FDeepSonarTuning& T = Tune();
	NearestM = -1.f;
	ClearanceM = -1.f;
	for (int32 Beam = 0; Beam < Upper.Num(); ++Beam)
	{
		const FDeepSonarSample& S = Upper[Beam];
		if (S.RangeM < 0.f || Clock - S.At >= T.PersistS)
		{
			continue;
		}
		const float Bearing = 360.f * Beam / Upper.Num();
		const float Clear = FMath::Max(0.f, S.RangeM - HullEdgeM(HullHalfM, Bearing));
		if (NearestM < 0.f || S.RangeM < NearestM)
		{
			NearestM = S.RangeM;
		}
		if (ClearanceM < 0.f || Clear < ClearanceM)
		{
			ClearanceM = Clear;
		}
	}
	Proximity01 = UDeepSonarRules::ProximityLevel(ClearanceM, T);
}

void UDeepSonarComponent::PlaySonarSound(const TCHAR* Event, float VolumeScale, float PitchScale)
{
	if (!bPlaySounds)
	{
		return;
	}
	if (UDeepAudioSubsystem* Audio = UDeepAudioSubsystem::Get(this))
	{
		Audio->PlayEventScaled(FName(Event), VolumeScale, PitchScale);
	}
}

void UDeepSonarComponent::StepSonar(float DeltaSeconds)
{
	TracesLastStep = 0;
	FVector Origin, Forward;
	if (!GetWorld() || !GetFrame(Origin, Forward, HullHalfM))
	{
		return;
	}
	EnsureBeams();
	const FDeepSonarTuning& T = Tune();
	const int32 N = Upper.Num();

	// A fresh helm starts with a full picture: one whole sweep at once, then the turning sweep keeps it current.
	if (!bWasWorking)
	{
		bWasWorking = true;
		SweepNow();
		PlaySonarSound(DeepSonarEvent::Ping, 1.f, 1.f);
		++Pings;
		BroadcastPing(this, Sub.Get(), T.RangeM);
		return;
	}

	Clock += FMath::Max(0.f, DeltaSeconds);
	const float Prev = SweepDegNow;
	SweepDegNow = UDeepSonarRules::SweepDeg(static_cast<float>(FMath::Fmod(Clock, static_cast<double>(FMath::Max(0.01f, T.SweepPeriodS)))), T.SweepPeriodS);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DeepSonar), false, GetOwner());
	BuildQuery(Params);

	// The bottom: one probe straight down, then the dim slice rides above it when it is nearer than SliceBelowM.
	{
		FHitResult Hit;
		const float ReachCm = T.BedProbeM * UDeepRules::CmPerM;
		BedBelowM = GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin - FVector(0.f, 0.f, ReachCm), ECC_Visibility, Params)
			? static_cast<float>(Hit.Distance / UDeepRules::CmPerM) : -1.f;
		LowerBelowM = UDeepSonarRules::LowerSliceBelowM(BedBelowM, T);
		++TracesLastStep;
	}

	// Beams the sweep passed this frame join the backlog; trace from the cursor within the frame's budget.
	const float Step = 360.f / N;
	int32 Crossed = FMath::FloorToInt(SweepDegNow / Step) - FMath::FloorToInt(Prev / Step);
	if (Crossed < 0)
	{
		Crossed += N;
	}
	Backlog = FMath::Min(N, Backlog + Crossed);
	const int32 BeamBudget = FMath::Max(1, T.MaxTracesPerFrame / 2);
	for (int32 I = 0; I < BeamBudget && Backlog > 0; ++I)
	{
		TraceBeamIndex(Cursor, Origin, Forward, Params);
		Cursor = (Cursor + 1) % N;
		--Backlog;
	}

	UpdateContacts(Origin, Forward, Prev, SweepDegNow, false);
	UpdateNearest();

	if (UDeepSonarRules::PingDue(Prev, SweepDegNow))
	{
		PlaySonarSound(DeepSonarEvent::Ping, 1.f, 1.f);
		++Pings;
		BroadcastPing(this, Sub.Get(), T.RangeM);
	}

	// The proximity tone: beeps that quicken and rise as the nearest wall closes on the hull.
	SinceBeep += DeltaSeconds;
	if (Proximity01 > 0.f)
	{
		if (SinceBeep >= UDeepSonarRules::ToneIntervalS(Proximity01, T))
		{
			SinceBeep = 0.f;
			++Beeps;
			PlaySonarSound(DeepSonarEvent::Near, UDeepSonarRules::ToneVolume(Proximity01, T), UDeepSonarRules::TonePitch(Proximity01, T));
		}
	}
	else
	{
		SinceBeep = 1000.f;
	}
}

void UDeepSonarComponent::SweepNow()
{
	FVector Origin, Forward;
	if (!GetWorld() || !GetFrame(Origin, Forward, HullHalfM))
	{
		return;
	}
	EnsureBeams();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DeepSonar), false, GetOwner());
	BuildQuery(Params);
	{
		FHitResult Hit;
		const float ReachCm = Tune().BedProbeM * UDeepRules::CmPerM;
		BedBelowM = GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin - FVector(0.f, 0.f, ReachCm), ECC_Visibility, Params)
			? static_cast<float>(Hit.Distance / UDeepRules::CmPerM) : -1.f;
		LowerBelowM = UDeepSonarRules::LowerSliceBelowM(BedBelowM, Tune());
	}
	for (int32 Beam = 0; Beam < Upper.Num(); ++Beam)
	{
		TraceBeamIndex(Beam, Origin, Forward, Params);
	}
	Backlog = 0;
	UpdateContacts(Origin, Forward, 0.f, 0.f, true);
	UpdateNearest();
}

void UDeepSonarComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const bool bWorking = IsWorking();
	const double Start = FPlatformTime::Seconds();
	if (bWorking)
	{
		StepSonar(DeltaTime);
	}
	else if (bWasWorking)
	{
		// Leaving the helm: the next time starts with a fresh full sweep, and nothing beeps meanwhile.
		bWasWorking = false;
		Proximity01 = 0.f;
		Contacts.Reset();
	}
	const double Cost = FPlatformTime::Seconds() - Start;

	if (BenchLeft > 0.f)
	{
		++BenchFrames;
		BenchFrameS += FApp::GetDeltaTime();
		BenchSonarS += Cost;
		BenchLeft -= static_cast<float>(FApp::GetDeltaTime());
		if (BenchLeft <= 0.f && BenchFrames > 0)
		{
			UE_LOG(LogDeepSonar, Display, TEXT("DEEPSONAR bench {\"on\":%d,\"manned\":%d,\"frames\":%d,\"frame_ms\":%.3f,\"sonar_ms\":%.4f}"),
				bSonarEnabled ? 1 : 0, IsManned() ? 1 : 0, BenchFrames, 1000.0 * BenchFrameS / BenchFrames, 1000.0 * BenchSonarS / BenchFrames);
		}
	}
}

void UDeepSonarComponent::StartBench(float Seconds)
{
	BenchLeft = FMath::Max(0.1f, Seconds);
	BenchFrames = 0;
	BenchFrameS = 0.0;
	BenchSonarS = 0.0;
}

FString UDeepSonarComponent::StateJson() const
{
	int32 Returns[2] = { 0, 0 };
	float LowerMin = -1.f;
	for (int32 Slice = 0; Slice < 2; ++Slice)
	{
		for (const FDeepSonarSample& S : GetSlice(Slice))
		{
			if (S.RangeM >= 0.f && Clock - S.At < Tune().PersistS)
			{
				++Returns[Slice];
				LowerMin = Slice == 1 && (LowerMin < 0.f || S.RangeM < LowerMin) ? S.RangeM : LowerMin;
			}
		}
	}
	// For testers: what is straight below, with nothing ignored (the bed probe ignores the sub, pawns and salvage).
	FString Below = TEXT("none");
	FVector Origin = FVector::ZeroVector;
	FVector Forward;
	FVector2D HullHalf;
	if (GetWorld() && GetFrame(Origin, Forward, HullHalf))
	{
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin - FVector(0.f, 0.f, 200.f * UDeepRules::CmPerM), ECC_Visibility))
		{
			Below = FString::Printf(TEXT("%s@%.1f"), Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("?"), Hit.Distance / UDeepRules::CmPerM);
		}
	}
	return FString::Printf(TEXT("{\"on\":%d,\"manned\":%d,\"sweep_deg\":%.0f,\"nearest_m\":%.1f,\"clearance_m\":%.1f,\"bed_m\":%.1f,\"lower_m\":%.1f,\"returns\":%d,\"lower_returns\":%d,\"lower_min_m\":%.1f,\"below\":\"%s\",\"origin_cm\":\"%.0f,%.0f,%.0f\",\"contacts\":%d,\"pings\":%d,\"beeps\":%d,\"contact_calls\":%d}"),
		bSonarEnabled ? 1 : 0, IsManned() ? 1 : 0, SweepDegNow, NearestM, ClearanceM, BedBelowM, LowerBelowM, Returns[0], Returns[1], LowerMin, *Below,
		Origin.X, Origin.Y, Origin.Z, Contacts.Num(), Pings, Beeps, ContactCalls);
}

void UDeepSonarComponent::FillHud(FDeepHudSonar& Out) const
{
	const FDeepSonarTuning& T = Tune();
	Out.SweepDeg = SweepDegNow;
	Out.RangeM = T.RangeM;
	Out.RingStepM = T.RingStepM;
	Out.JoinM = T.JoinM;
	Out.LowerBelowM = LowerBelowM;
	Out.BedM = BedBelowM;
	Out.ClearanceM = ClearanceM;
	Out.Proximity01 = Proximity01;
	Out.HullHalfM = FVector2f(HullHalfM);
	auto Fill = [&](const TArray<FDeepSonarSample>& In, TArray<FDeepHudSonarReturn>& To)
	{
		To.SetNum(In.Num());
		for (int32 I = 0; I < In.Num(); ++I)
		{
			To[I].RangeM = In[I].RangeM;
			To[I].Strength = In[I].RangeM >= 0.f ? UDeepSonarRules::ReturnStrength(static_cast<float>(Clock - In[I].At), In[I].RangeM, T) : 0.f;
		}
	};
	Fill(Upper, Out.Upper);
	Fill(Lower, Out.Lower);
	Out.Blips.Reset(Contacts.Num());
	for (const FDeepSonarContactSample& C : Contacts)
	{
		FDeepHudSonarBlip& B = Out.Blips.AddDefaulted_GetRef();
		B.BearingDeg = C.BearingDeg;
		B.RangeM = C.RangeM;
		B.DzM = C.DzM;
		// Contacts fade like walls, without the range falloff: a far serpent is as bright as a near one.
		B.Strength = UDeepSonarRules::ReturnStrength(static_cast<float>(Clock - C.At), 0.f, T);
		B.Kind = C.Kind == EDeepSonarContact::Serpent ? 2 : (C.Kind == EDeepSonarContact::Relic ? 1 : 0);
	}
}
