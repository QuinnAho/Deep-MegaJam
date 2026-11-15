#include "Diver/DeepRunComponent.h"

#include "Diver/DeepDiver.h"
#include "Diver/DeepTetherComponent.h"
#include "Engine/World.h"
#include "Game/DeepGameState.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepCaptainLines.h"
#include "World/DeepArena.h"

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
	}
	return TEXT("unknown");
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
	if (GetOwner()->HasAuthority() && !bStarted)
	{
		StartDive(EDiveStart::First);
	}
}

void UDeepRunComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	StepRun(DeltaTime);
}

void UDeepRunComponent::SetState(EDeepRunState NewState)
{
	State = NewState;
}

// ---------------------------------------------------------------- the state machine

void UDeepRunComponent::StepRun(float DeltaSeconds)
{
	ADeepDiver* Diver = GetDiver();
	if (!Diver || !Diver->HasAuthority())
	{
		return;
	}
	if (!bStarted)
	{
		StartDive(EDiveStart::First);
	}
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const float Dt = FMath::Max(0.f, DeltaSeconds);

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
			FireChatter(DeepEvent::WinchStart);
		}
		bWasWinching = bWinching;

		const bool bArrived = bHome && !bWasHome;
		bWasHome = bHome;
		if (bArrived && Diver->GetCargo().Num() > 0)
		{
			Bank();
			return;
		}

		const float Before = PatienceSeconds;
		PatienceSeconds = UDeepRules::PatienceStep(Before, Dt, false, Tuning.Patience);
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
	}
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
	ADeepDiver* Diver = GetDiver();
	if (State != EDeepRunState::Tethered || !Diver || !Diver->HasAuthority())
	{
		return;
	}
	Diver->SetWinching(false);
	BailoutAirSeconds = UDeepTuningSettings::Get().Bailout.AirSeconds;
	SetState(EDeepRunState::Cut);
	Fire(bRequested ? DeepEvent::CutRequested : DeepEvent::CutPatience);
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

void UDeepRunComponent::Die()
{
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
	Fire(DeepEvent::DeathAir, Payload);
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
	const TArray<int32> Amounts = UDeepRules::SplitFee(UDeepRules::SettlementFeeTotal(Gross, Economy), LineCount, Rng.GetCurrentSeed());
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

	const FDeepSettlement Settled = UDeepRules::Settle(Gross, Amounts, Economy);
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
		GameState->Credits = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(FMath::Max(0, GameState->Credits)) + Settled.Net));
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

void UDeepRunComponent::ResetRun()
{
	ADeepDiver* Diver = GetDiver();
	if (!Diver || !Diver->HasAuthority())
	{
		return;
	}
	if (ADeepGameState* GameState = GetGameState())
	{
		GameState->Credits = FMath::Max(0, UDeepTuningSettings::Get().Economy.StartingCredits);
		GameState->HullRating = 1;
		GameState->Stop = 1;
		GameState->DiverNumber = 1;
		DeepArena::ApplyStop(GetWorld(), 1);
	}
	Diver->ClearCargo();
	Settlement = FDeepSettlementReport();
	StateTimeLeft = 0.f;
	SetState(EDeepRunState::Tethered);
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
}
