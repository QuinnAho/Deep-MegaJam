#include "Diver/DeepRunComponent.h"

#include "Diver/DeepDiver.h"
#include "Diver/DeepTetherComponent.h"
#include "Engine/World.h"
#include "Game/DeepGameState.h"
#include "Harpoon/DeepHarpoonComponent.h"
#include "Harpoon/DeepHarpoonRules.h"
#include "GameFramework/PlayerController.h"
#include "Monster/DeepMonsterBrain.h"
#include "Monster/DeepMonsterRules.h"
#include "Monster/DeepMonsterSettings.h"
#include "Monster/DeepSerpent.h"
#include "Net/UnrealNetwork.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepCaptainLines.h"
#include "World/DeepArena.h"
#include "World/DeepSalvage.h"

UDeepRunComponent::UDeepRunComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	PatienceSeconds = FDeepPatienceTuning().MaxSeconds;
}

FString UDeepRunComponent::StateName(EDeepRunState InState)
{
	switch (InState)
	{
	case EDeepRunState::Tethered: return TEXT("tethered");
	case EDeepRunState::Cut: return TEXT("cut");
	case EDeepRunState::Dead: return TEXT("dead");
	case EDeepRunState::Settling: return TEXT("settling");
	case EDeepRunState::Descending: return TEXT("descending");
	case EDeepRunState::Won: return TEXT("won");
	}
	return TEXT("unknown");
}

bool UDeepRunComponent::IsOnTitle() const
{
	const ADeepGameState* GameState = GetGameState();
	return GameState && GameState->bOnTitle;
}

bool UDeepRunComponent::CanSwim() const
{
	return (State == EDeepRunState::Tethered || State == EDeepRunState::Cut) && !IsOnTitle();
}

bool UDeepRunComponent::CarriesRelic() const
{
	const ADeepDiver* Diver = GetDiver();
	return Diver && Diver->GetCargo().ContainsByPredicate([](const FDeepCargoItem& Item) { return Item.Size == EDeepSalvageSize::Relic; });
}

float UDeepRunComponent::GetPatienceFraction() const
{
	const float Max = UDeepTuningSettings::Get().Patience.MaxSeconds;
	return Max > 0.f ? FMath::Clamp(PatienceSeconds / Max, 0.f, 1.f) : 0.f;
}

ADeepDiver* UDeepRunComponent::GetDiver() const
{
	return Cast<ADeepDiver>(GetOwner());
}

ADeepGameState* UDeepRunComponent::GetGameState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<ADeepGameState>() : nullptr;
}

bool UDeepRunComponent::IsDiverHome() const
{
	const ADeepDiver* Diver = GetDiver();
	const UDeepTetherComponent* Tether = Diver ? Diver->GetTether() : nullptr;
	if (!Tether || !Tether->HasAnchor())
	{
		return false;
	}
	return UDeepRules::IsHome(Diver->GetActorLocation(), Tether->GetAnchorLocation(), UDeepTuningSettings::Get().Tether);
}

void UDeepRunComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority() && !bStarted && !IsOnTitle())
	{
		StartDive(EDiveStart::First);
	}
	if (GetOwner()->HasAuthority())
	{
		BindMonster();
	}
}

void UDeepRunComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindMonster();
	Super::EndPlay(EndPlayReason);
}

void UDeepRunComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	StepRun(DeltaTime);
}

void UDeepRunComponent::SetState(EDeepRunState NewState)
{
	State = NewState;
	if (NewState != EDeepRunState::Tethered)
	{
		// Whatever ended the tethered dive (Patience, a request, death, banking) also ends any bite on it.
		ClearBiteAlarm();
	}
	// The serpent's idea of who is tethered follows every cut and every restore.
	ADeepDiver* Diver = GetDiver();
	UDeepMonsterBrain* Brain = GetWorld() ? GetWorld()->GetSubsystem<UDeepMonsterBrain>() : nullptr;
	if (Brain && Diver && Diver->HasAuthority())
	{
		if (NewState == EDeepRunState::Cut || NewState == EDeepRunState::Dead)
		{
			Brain->NotifyTetherCut(Diver);
		}
		else if (NewState == EDeepRunState::Tethered)
		{
			Brain->NotifyRetethered(Diver);
		}
	}
}

// ---------------------------------------------------------------- the serpent

void UDeepRunComponent::BindMonster()
{
	UDeepMonsterBrain* Brain = GetWorld() ? GetWorld()->GetSubsystem<UDeepMonsterBrain>() : nullptr;
	if (!Brain || !MonsterHandles.IsEmpty())
	{
		return;
	}
	MonsterHandles.Add(Brain->OnBiteStarted.AddUObject(this, &UDeepRunComponent::HandleBiteStarted));
	MonsterHandles.Add(Brain->OnBiteBroken.AddUObject(this, &UDeepRunComponent::HandleBiteBroken));
	MonsterHandles.Add(Brain->OnTetherSevered.AddUObject(this, &UDeepRunComponent::HandleTetherSevered));
	MonsterHandles.Add(Brain->OnDiverCaught.AddUObject(this, &UDeepRunComponent::HandleDiverCaught));
	MonsterHandles.Add(Brain->OnStalkStarted.AddUObject(this, &UDeepRunComponent::HandleStalkStarted));
	MonsterHandles.Add(Brain->OnRepelled.AddUObject(this, &UDeepRunComponent::HandleRepelled));
}

void UDeepRunComponent::UnbindMonster()
{
	UDeepMonsterBrain* Brain = GetWorld() ? GetWorld()->GetSubsystem<UDeepMonsterBrain>() : nullptr;
	if (Brain)
	{
		Brain->OnBiteStarted.RemoveAll(this);
		Brain->OnBiteBroken.RemoveAll(this);
		Brain->OnTetherSevered.RemoveAll(this);
		Brain->OnDiverCaught.RemoveAll(this);
		Brain->OnStalkStarted.RemoveAll(this);
		Brain->OnRepelled.RemoveAll(this);
	}
	MonsterHandles.Reset();
}

void UDeepRunComponent::ClearBiteAlarm()
{
	bBiteAlarm = false;
	BiteAlarmLeft = 0.f;
}

void UDeepRunComponent::StepMonsterWires(float DeltaSeconds)
{
	ADeepDiver* Diver = GetDiver();
	UDeepMonsterBrain* Brain = GetWorld() ? GetWorld()->GetSubsystem<UDeepMonsterBrain>() : nullptr;
	if (!Diver || !Brain)
	{
		MonsterThreat01 = 0.f;
		return;
	}
	BindMonster();
	const ADeepGameState* GameState = GetGameState();
	const FDeepMonsterTuning Tuning = UDeepMonsterSettings::Get().GetTuning(GameState ? GameState->Stop : 1);

	// Noise: the winch and sprinting, reported where the loop knows them (grabs report in NotifyPickup).
	if (CanSwim())
	{
		const FDeepSwimTuning& Swim = UDeepTuningSettings::Get().Swim;
		const float SpeedMps = static_cast<float>(Diver->GetVelocity().Size()) / UDeepRules::CmPerM;
		NoiseOwed += UDeepMonsterRules::NoiseFromDiver(Diver->IsWinching(), SpeedMps,
			UDeepMonsterRules::SprintThresholdMps(Swim.SwimSpeedMps, Swim.SprintMultiplier), 0.f, DeltaSeconds, Tuning);
		if (NoiseOwed >= 1.f)
		{
			UDeepMonsterRules::ReportDiverNoise(Diver, Diver->GetActorLocation(), NoiseOwed, Diver);
			NoiseOwed = 0.f;
		}
	}

	// Threat: how close the head is to this diver (full while it bites this diver's line).
	const ADeepSerpent* Serpent = Brain->GetSerpent();
	const bool bFocusMe = Brain->GetFocusDiver() == Diver;
	const EDeepMonsterState MonsterState = Brain->GetState();
	if (Serpent && CanSwim())
	{
		const float DistanceM = static_cast<float>(FVector::Dist(Serpent->GetHeadLocation(), Diver->GetActorLocation())) / UDeepRules::CmPerM;
		MonsterThreat01 = UDeepMonsterRules::Threat01(bFocusMe ? MonsterState : EDeepMonsterState::Lurk, DistanceM, Tuning);
	}
	else
	{
		MonsterThreat01 = 0.f;
	}

	// The Captain notices when it gets close on an approach.
	if (bFocusMe && MonsterState == EDeepMonsterState::Stalk && MonsterThreat01 >= 0.6f && !bSaidNearThisApproach)
	{
		bSaidNearThisApproach = true;
		FireChatter(DeepEvent::MonsterNear);
	}

	// The alarm counts down; it clears if the bite ended some other way (serpent gone, focus changed).
	if (bBiteAlarm)
	{
		BiteAlarmLeft = FMath::Max(0.f, BiteAlarmLeft - DeltaSeconds);
		if (!Serpent || MonsterState != EDeepMonsterState::Bite || !bFocusMe)
		{
			ClearBiteAlarm();
		}
	}
}

void UDeepRunComponent::HandleBiteStarted(ADeepDiver* Who, float AlarmSeconds)
{
	if (Who != GetDiver() || State != EDeepRunState::Tethered)
	{
		return;
	}
	bBiteAlarm = true;
	BiteAlarmLeft = FMath::Max(0.f, AlarmSeconds);
	Fire(DeepEvent::BiteAlarm);
}

void UDeepRunComponent::HandleBiteBroken(ADeepDiver* Who)
{
	if (Who == GetDiver())
	{
		ClearBiteAlarm();
	}
}

void UDeepRunComponent::HandleTetherSevered(ADeepDiver* Who)
{
	ADeepDiver* Diver = GetDiver();
	if (Who != Diver || State != EDeepRunState::Tethered)
	{
		return;
	}
	if (!bBiteAlarm)
	{
		// Fairness: the line never goes without the alarm having been on screen. Tell the serpent it is still there.
		UE_LOG(LogTemp, Warning, TEXT("Serpent severed a tether with no alarm up; ignored"));
		if (UDeepMonsterBrain* Brain = GetWorld()->GetSubsystem<UDeepMonsterBrain>())
		{
			Brain->NotifyRetethered(Diver);
		}
		return;
	}
	ClearBiteAlarm();
	CutTetherFor(DeepEvent::CutBite);
}

void UDeepRunComponent::HandleDiverCaught(ADeepDiver* Who)
{
	if (Who == GetDiver() && State == EDeepRunState::Cut)
	{
		Die(true);
	}
}

void UDeepRunComponent::HandleStalkStarted(ADeepDiver* Who)
{
	if (Who != GetDiver() || State != EDeepRunState::Tethered)
	{
		return;
	}
	bSaidNearThisApproach = false;
	if (!bMonsterHeardThisRun)
	{
		bMonsterHeardThisRun = true;
		Fire(DeepEvent::MonsterFirstHeard);
	}
	else
	{
		bSaidNearThisApproach = FireChatter(DeepEvent::MonsterNear);
	}
}

void UDeepRunComponent::HandleRepelled(ADeepDiver* Who)
{
	if (Who == GetDiver() && CanSwim())
	{
		ClearBiteAlarm();
		Fire(DeepEvent::MonsterRepelled);
	}
}

// ---------------------------------------------------------------- the state machine

void UDeepRunComponent::StepRun(float DeltaSeconds)
{
	ADeepDiver* Diver = GetDiver();
	if (!Diver || !Diver->HasAuthority())
	{
		return;
	}
	if (IsOnTitle())
	{
		// The title screen is up: the first dive waits for a key.
		return;
	}
	if (!bStarted)
	{
		StartDive(EDiveStart::First);
	}
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const float Dt = FMath::Max(0.f, DeltaSeconds);
	StepMonsterWires(Dt);
	if (CanSwim())
	{
		StepRelicSeen();
	}

	switch (State)
	{
	case EDeepRunState::Tethered:
	{
		const bool bOverloaded = Diver->GetWeight().bOverloaded;
		if (bOverloaded && !bWasOverloaded)
		{
			FireChatter(DeepEvent::Overweight);
		}
		bWasOverloaded = bOverloaded;

		const bool bTaut = Diver->GetTether() && Diver->GetTether()->GetSolve().bTaut;
		if (bTaut && !bWasTaut)
		{
			FireChatter(DeepEvent::TetherTaut);
		}
		bWasTaut = bTaut;

		const bool bHome = IsDiverHome();
		const bool bWinching = Diver->IsWinching();
		if (bWinching && !bWasWinching && !bHome)
		{
			// Winching the relic is the loudest thing in the trench; the Captain says so.
			FireChatter(CarriesRelic() ? DeepEvent::RelicWinch : DeepEvent::WinchStart);
		}
		bWasWinching = bWinching;

		const bool bArrived = bHome && !bWasHome;
		bWasHome = bHome;
		if (bArrived && Diver->GetCargo().Num() > 0)
		{
			Bank();
			return;
		}

		// The Captain gets twice as impatient while the serpent closes on (or chews) this diver's line.
		const UDeepMonsterBrain* Brain = GetWorld()->GetSubsystem<UDeepMonsterBrain>();
		const bool bHunted = Brain && Brain->GetSerpent() && Brain->GetFocusDiver() == Diver
			&& (Brain->GetState() == EDeepMonsterState::Stalk || Brain->GetState() == EDeepMonsterState::Bite);
		const float Before = PatienceSeconds;
		PatienceSeconds = UDeepRules::PatienceStep(Before, Dt, bHunted, Tuning.Patience);
		const int32 Warning = UDeepRules::PatienceWarningCrossed(Before, PatienceSeconds, Tuning.Patience);
		if (Tuning.Patience.WarningFractions.IsValidIndex(Warning))
		{
			const int32 Percent = FMath::RoundToInt(Tuning.Patience.WarningFractions[Warning] * 100.f);
			Fire(*FString::Printf(TEXT("patience_%d"), Percent));
		}
		if (PatienceSeconds <= 0.f)
		{
			CutTether(false);
		}
		break;
	}
	case EDeepRunState::Cut:
	{
		const float Before = BailoutAirSeconds;
		BailoutAirSeconds = UDeepRules::BailoutStep(Before, Dt, Tuning.Bailout);
		if (Before > Tuning.Bailout.WarnSeconds && BailoutAirSeconds <= Tuning.Bailout.WarnSeconds)
		{
			Fire(DeepEvent::Bailout10s);
		}
		if (IsDiverHome())
		{
			Retether();
			if (Diver->GetCargo().Num() > 0)
			{
				Bank();
			}
			return;
		}
		if (BailoutAirSeconds <= 0.f)
		{
			Die();
		}
		break;
	}
	case EDeepRunState::Dead:
		StateTimeLeft -= Dt;
		if (StateTimeLeft <= 0.f)
		{
			Respawn();
		}
		break;
	case EDeepRunState::Settling:
	case EDeepRunState::Won:
		StateTimeLeft = FMath::Max(0.f, StateTimeLeft - Dt);
		break;
	case EDeepRunState::Descending:
		StateTimeLeft -= Dt;
		if (!bSwapped && StateTimeLeft <= Tuning.Run.DescendSeconds * 0.5f)
		{
			SwapToNextDive();
		}
		if (StateTimeLeft <= 0.f)
		{
			StateTimeLeft = 0.f;
			SetState(EDeepRunState::Tethered);
		}
		break;
	}
}

// ---------------------------------------------------------------- transitions

void UDeepRunComponent::StartDive(EDiveStart Reason)
{
	bStarted = true;
	ADeepDiver* Diver = GetDiver();
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	PatienceSeconds = Tuning.Patience.MaxSeconds;
	BailoutAirSeconds = 0.f;
	if (Reason == EDiveStart::First || Reason == EDiveStart::Reset)
	{
		bFirstSalvageThisRun = true;
		bMonsterHeardThisRun = false;
		bRelicSeenThisRun = false;
	}
	NoiseOwed = 0.f;
	bSaidNearThisApproach = false;
	if (Diver)
	{
		Diver->SetWinching(false);
		if (Reason != EDiveStart::First)
		{
			// Salvage respawns every dive; the diver starts under the airlock again.
			DeepArena::RespawnSalvage(GetWorld());
			Diver->ResetToStart();
		}
	}
	bWasHome = IsDiverHome();
	bWasOverloaded = Diver && Diver->GetWeight().bOverloaded;
	bWasTaut = false;
	bWasWinching = false;

	const ADeepGameState* GameState = GetGameState();
	const int32 Stop = GameState ? GameState->Stop : 1;
	switch (Reason)
	{
	case EDiveStart::Respawn:
		Fire(DeepEvent::Respawn);
		break;
	case EDiveStart::NewStop:
	{
		FDeepEventPayload Payload;
		Payload.DepthM = Tuning.GetStop(Stop).SubDepthM;
		Fire(Stop >= 3 ? DeepEvent::DescendStop3 : (Stop == 2 ? DeepEvent::DescendStop2 : DeepEvent::DiveStart), Payload);
		break;
	}
	default:
		Fire(DeepEvent::DiveStart);
		break;
	}
}

void UDeepRunComponent::CutTether(bool bRequested)
{
	CutTetherFor(bRequested ? DeepEvent::CutRequested : DeepEvent::CutPatience);
}

void UDeepRunComponent::CutTetherFor(const TCHAR* Event)
{
	ADeepDiver* Diver = GetDiver();
	if (State != EDeepRunState::Tethered || !Diver || !Diver->HasAuthority() || IsOnTitle())
	{
		return;
	}
	Diver->SetWinching(false);
	BailoutAirSeconds = UDeepTuningSettings::Get().Bailout.AirSeconds;
	SetState(EDeepRunState::Cut);
	Fire(Event);
}

void UDeepRunComponent::Retether()
{
	ChargeFee(UDeepTuningSettings::Get().Economy.RetetherFee);
	PatienceSeconds = UDeepTuningSettings::Get().Patience.MaxSeconds;
	BailoutAirSeconds = 0.f;
	bWasHome = true;
	SetState(EDeepRunState::Tethered);
	Fire(DeepEvent::Retether);
}

void UDeepRunComponent::Die(bool bByMonster)
{
	bLastDeathMonster = bByMonster;
	ADeepDiver* Diver = GetDiver();
	FDeepEventPayload Payload;
	Payload.DepthM = Diver ? Diver->GetDepthM() : 0.f;
	if (Diver)
	{
		Diver->SetWinching(false);
		Diver->ClearCargo();
	}
	ChargeFee(UDeepTuningSettings::Get().Economy.DeathFee);
	BailoutAirSeconds = 0.f;
	SetState(EDeepRunState::Dead);
	StateTimeLeft = FMath::Max(0.f, UDeepTuningSettings::Get().Run.DeathScreenSeconds);
	Fire(bByMonster ? DeepEvent::DeathMonster : DeepEvent::DeathAir, Payload);
}

void UDeepRunComponent::Respawn()
{
	if (ADeepGameState* GameState = GetGameState())
	{
		GameState->DiverNumber += 1;
	}
	StateTimeLeft = 0.f;
	SetState(EDeepRunState::Tethered);
	StartDive(EDiveStart::Respawn);
}

void UDeepRunComponent::ChargeFee(int32 Fee)
{
	if (ADeepGameState* GameState = GetGameState())
	{
		GameState->Credits = UDeepRules::ApplyFee(GameState->Credits, Fee);
		GameState->TotalFees += FMath::Max(0, Fee);
	}
	LastFee = Fee;
	LastFeeTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void UDeepRunComponent::Bank()
{
	ADeepDiver* Diver = GetDiver();
	if (!Diver || !Diver->HasAuthority() || !CanSwim())
	{
		return;
	}
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const FDeepEconomyTuning& Economy = Tuning.Economy;
	ADeepGameState* GameState = GetGameState();
	Diver->SetWinching(false);

	// Fees: a rule total, split over a few joke lines drawn from SettlementFees.csv.
	const int32 Gross = Diver->GetCargoValue();
	FRandomStream Rng(FMath::Rand() ^ (++FeeSeed * 7919));
	const int32 MinLines = FMath::Max(1, Economy.FeeLinesMin);
	const int32 LineCount = Rng.RandRange(MinLines, FMath::Max(MinLines, Economy.FeeLinesMax));
	TArray<int32> Amounts = UDeepRules::SplitFee(UDeepRules::SettlementFeeTotal(Gross, Economy), LineCount, Rng.GetCurrentSeed());
	TArray<FString> Pool = DeepData::FeeLabels();
	TArray<FString> Labels;
	for (int32 Index = 0; Index < Amounts.Num(); ++Index)
	{
		if (Pool.IsEmpty())
		{
			Pool = DeepData::FeeLabels();
		}
		Labels.Add(Pool.IsEmpty() ? FString(TEXT("Fees")) : Pool[Rng.RandRange(0, Pool.Num() - 1)]);
		Pool.Remove(Labels.Last());
	}

	const int32 Repair = DeepHarpoon::BillHullRepairs(GameState, Labels, Amounts);
	const FDeepSettlement Settled = UDeepRules::Settle(Gross, Amounts, Economy);
	const bool bRelic = CarriesRelic();
	FDeepSettlementReport Report;
	Report.bValid = true;
	Report.Gross = Settled.Gross;
	Report.DirectorCut = Settled.DirectorCut;
	Report.FeeLabels = Labels;
	Report.FeeAmounts = Amounts;
	Report.Fees = Settled.Fees;
	Report.Net = Settled.Net;
	Report.Mood = UDeepRules::BankMood(Settled.Gross, Settled.Net, Economy);

	if (GameState)
	{
		GameState->TotalGross += Settled.Gross;
		GameState->TotalDirectorCut += Settled.DirectorCut;
		GameState->TotalFees += Settled.Fees;
		GameState->Credits = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(FMath::Max(0, GameState->Credits)) + Settled.Net));
		GameState->Credits = UDeepRules::ApplyFee(GameState->Credits, UDeepHarpoonRules::RepairFromCredits(Settled.Gross, Settled.DirectorCut, Settled.Fees, Repair));
		const FDeepHullPurchase Hull = UDeepRules::TryBuyHull(GameState->Credits, GameState->HullRating, Economy);
		GameState->Credits = Hull.Credits;
		GameState->HullRating = Hull.HullRating;
		Report.bHullBought = Hull.bBought;
		Report.HullSpent = Hull.Spent;
		Report.HullRating = Hull.HullRating;
		Report.NextHullPrice = UDeepRules::NextHullPrice(Hull.HullRating, Economy);
		Report.CreditsAfter = GameState->Credits;
		const int32 Reachable = FMath::Clamp(Hull.HullRating, 1, FMath::Max(1, Tuning.NumStops()));
		Report.NextStop = Reachable > GameState->Stop ? GameState->Stop + 1 : GameState->Stop;
	}
	Settlement = Report;
	Diver->ClearCargo();
	if (bRelic)
	{
		Win();
		return;
	}
	SetState(EDeepRunState::Settling);
	StateTimeLeft = FMath::Max(0.f, Tuning.Run.SettlementMinSeconds);

	FDeepEventPayload Payload;
	Payload.Gross = Report.Gross;
	Payload.Cut = Report.DirectorCut;
	Payload.Net = Report.Net;
	Fire(Report.Mood == EDeepBankMood::Empty ? DeepEvent::BankEmpty : (Report.Mood == EDeepBankMood::Good ? DeepEvent::BankGood : DeepEvent::BankPoor), Payload);
	if (Report.bHullBought)
	{
		Fire(DeepEvent::HullBought, Payload);
	}
}

void UDeepRunComponent::Win()
{
	ADeepGameState* GameState = GetGameState();
	if (GameState)
	{
		GameState->bWon = true;
	}
	SetState(EDeepRunState::Won);
	StateTimeLeft = FMath::Max(0.f, UDeepTuningSettings::Get().Run.ReviewMinSeconds);
	FDeepEventPayload Payload;
	if (GameState)
	{
		const FDeepRunReview Review = GameState->BuildReview();
		Payload.Gross = Review.Gross;
		Payload.Cut = Review.DirectorCut;
		Payload.Net = Review.Kept;
	}
	Fire(DeepEvent::Win, Payload);
}

void UDeepRunComponent::StepRelicSeen()
{
	const ADeepGameState* GameState = GetGameState();
	ADeepDiver* Diver = GetDiver();
	if (bRelicSeenThisRun || !Diver || !GameState || GameState->Stop < UDeepTuningSettings::Get().Arena.RelicStop || CarriesRelic())
	{
		return;
	}
	const ADeepSalvage* Relic = DeepArena::FindRelic(GetWorld());
	if (Relic && UDeepRules::IsRelicSeen(Diver->GetPawnViewLocation(), Diver->GetControlRotation().Vector(), Relic->GetActorLocation(), UDeepTuningSettings::Get().Arena))
	{
		bRelicSeenThisRun = true;
		Fire(DeepEvent::RelicSeen);
	}
}

void UDeepRunComponent::ContinueAfterSettlement()
{
	if (State != EDeepRunState::Settling || StateTimeLeft > 0.f)
	{
		return;
	}
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const ADeepGameState* GameState = GetGameState();
	DescendFromDepthM = Tuning.GetStop(GameState ? GameState->Stop : 1).SubDepthM;
	DescendToDepthM = Tuning.GetStop(Settlement.NextStop).SubDepthM;
	bSwapped = false;
	SetState(EDeepRunState::Descending);
	StateTimeLeft = FMath::Max(0.01f, Tuning.Run.DescendSeconds);
}

void UDeepRunComponent::SwapToNextDive()
{
	bSwapped = true;
	ADeepGameState* GameState = GetGameState();
	const int32 Old = GameState ? GameState->Stop : 1;
	if (GameState)
	{
		GameState->Stop = FMath::Clamp(Settlement.NextStop, 1, FMath::Max(1, UDeepTuningSettings::Get().NumStops()));
		DeepArena::ApplyStop(GetWorld(), GameState->Stop);
	}
	StartDive(GameState && GameState->Stop != Old ? EDiveStart::NewStop : EDiveStart::SameStop);
}

void UDeepRunComponent::SetPatienceFraction(float Fraction)
{
	PatienceSeconds = FMath::Clamp(Fraction, 0.f, 1.f) * UDeepTuningSettings::Get().Patience.MaxSeconds;
}

void UDeepRunComponent::ResetRun(bool bToTitle)
{
	ADeepDiver* Diver = GetDiver();
	if (!Diver || !Diver->HasAuthority())
	{
		return;
	}
	if (ADeepGameState* GameState = GetGameState())
	{
		GameState->ResetRun();
		GameState->bOnTitle = bToTitle;
		DeepArena::ApplyStop(GetWorld(), 1);
	}
	// The serpent goes with the old run (it returns at stop 2).
	if (UDeepMonsterBrain* Brain = GetWorld()->GetSubsystem<UDeepMonsterBrain>())
	{
		Brain->ResetForRun();
	}
	Diver->ClearCargo();
	Diver->SetWinching(false);
	Settlement = FDeepSettlementReport();
	StateTimeLeft = 0.f;
	BailoutAirSeconds = 0.f;
	PatienceSeconds = UDeepTuningSettings::Get().Patience.MaxSeconds;
	LastFee = 0;
	LastFeeTime = -100.f;
	LastRefillFraction = 0.f;
	LastRefillTime = -100.f;
	MonsterThreat01 = 0.f;
	bLastDeathMonster = false;
	bRelicSeenThisRun = false;
	ChatterLastTime.Reset();
	SetState(EDeepRunState::Tethered);
	if (bToTitle)
	{
		// The first dive starts when the title is left (StepRun sees bStarted false).
		bStarted = false;
		DeepArena::RespawnSalvage(GetWorld());
		Diver->ResetToStart();
		return;
	}
	StartDive(EDiveStart::Reset);
}

void UDeepRunComponent::NotifyPickup(const FDeepCargoItem& Item)
{
	if (!CanSwim() || !GetOwner()->HasAuthority())
	{
		return;
	}
	const FDeepPatienceTuning& Patience = UDeepTuningSettings::Get().Patience;
	if (State == EDeepRunState::Tethered)
	{
		PatienceSeconds = UDeepRules::PatienceAfterSalvage(PatienceSeconds, Item.Size, Patience);
	}
	LastRefillFraction = Patience.MaxSeconds > 0.f ? UDeepRules::PatienceRefillSeconds(Item.Size, Patience) / Patience.MaxSeconds : 0.f;
	LastRefillTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	// A grab is loud: one hearing event, sized by the piece.
	if (ADeepDiver* Diver = GetDiver())
	{
		const ADeepGameState* GameState = GetGameState();
		const FDeepMonsterTuning MonsterTuning = UDeepMonsterSettings::Get().GetTuning(GameState ? GameState->Stop : 1);
		const float GrabNoise = UDeepMonsterRules::NoiseFromDiver(false, 0.f, 1.f, FMath::Max(Item.Kg, 0.01f), 0.f, MonsterTuning);
		UDeepMonsterRules::ReportDiverNoise(Diver, Diver->GetActorLocation(), FMath::Max(1.f, GrabNoise), Diver);
	}

	if (Item.Size == EDeepSalvageSize::Relic)
	{
		bFirstSalvageThisRun = false;
		bRelicSeenThisRun = true;
		Fire(DeepEvent::RelicGrabbed);
		return;
	}
	if (bFirstSalvageThisRun)
	{
		bFirstSalvageThisRun = false;
		Fire(DeepEvent::FirstSalvage);
		return;
	}
	switch (Item.Size)
	{
	case EDeepSalvageSize::Small: Fire(DeepEvent::SalvageSmall); break;
	case EDeepSalvageSize::Medium: Fire(DeepEvent::SalvageMedium); break;
	default: Fire(DeepEvent::SalvageLarge); break;
	}
}

// ---------------------------------------------------------------- requests and events

void UDeepRunComponent::RequestCut()
{
	if (GetOwner()->HasAuthority())
	{
		CutTether(true);
	}
	else
	{
		ServerRequestCut();
	}
}

void UDeepRunComponent::ServerRequestCut_Implementation()
{
	CutTether(true);
}

void UDeepRunComponent::RequestContinue()
{
	if (GetOwner()->HasAuthority())
	{
		ContinueAfterSettlement();
	}
	else
	{
		ServerRequestContinue();
	}
}

void UDeepRunComponent::ServerRequestContinue_Implementation()
{
	ContinueAfterSettlement();
}

bool UDeepRunComponent::FireChatter(const TCHAR* Event)
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	const FName Name(Event);
	if (const double* Last = ChatterLastTime.Find(Name); Last && Now - *Last < UDeepTuningSettings::Get().Run.ChatterCooldownSeconds)
	{
		return false;
	}
	ChatterLastTime.Add(Name, Now);
	Fire(Event);
	return true;
}

void UDeepRunComponent::Fire(const TCHAR* Event, FDeepEventPayload Payload)
{
	ADeepDiver* Diver = GetDiver();
	const ADeepGameState* GameState = GetGameState();
	Payload.DiverNumber = GameState ? GameState->DiverNumber : 1;
	if (Payload.DepthM <= 0.f && Diver)
	{
		Payload.DepthM = Diver->GetDepthM();
	}
	Payload.Source = Diver;
	const FName Name(Event);
	if (UDeepEventBus* Bus = UDeepEventBus::Get(this))
	{
		Bus->Broadcast(Name, Payload);
	}
	// A remote player's diver: their machine has its own bus and Captain.
	if (Diver && !Diver->IsLocallyControlled() && Cast<APlayerController>(Diver->GetController()))
	{
		ClientEvent(Name, Payload);
	}
}

void UDeepRunComponent::ClientEvent_Implementation(FName Event, const FDeepEventPayload& Payload)
{
	if (GetOwner()->HasAuthority())
	{
		return;
	}
	if (UDeepEventBus* Bus = UDeepEventBus::Get(this))
	{
		Bus->Broadcast(Event, Payload);
	}
}

void UDeepRunComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDeepRunComponent, State);
	DOREPLIFETIME(UDeepRunComponent, PatienceSeconds);
	DOREPLIFETIME(UDeepRunComponent, BailoutAirSeconds);
	DOREPLIFETIME(UDeepRunComponent, StateTimeLeft);
	DOREPLIFETIME(UDeepRunComponent, Settlement);
	DOREPLIFETIME(UDeepRunComponent, DescendFromDepthM);
	DOREPLIFETIME(UDeepRunComponent, DescendToDepthM);
	DOREPLIFETIME(UDeepRunComponent, LastRefillFraction);
	DOREPLIFETIME(UDeepRunComponent, LastRefillTime);
	DOREPLIFETIME(UDeepRunComponent, LastFee);
	DOREPLIFETIME(UDeepRunComponent, LastFeeTime);
	DOREPLIFETIME(UDeepRunComponent, BiteAlarmLeft);
	DOREPLIFETIME(UDeepRunComponent, bBiteAlarm);
	DOREPLIFETIME(UDeepRunComponent, MonsterThreat01);
	DOREPLIFETIME(UDeepRunComponent, bLastDeathMonster);
}
