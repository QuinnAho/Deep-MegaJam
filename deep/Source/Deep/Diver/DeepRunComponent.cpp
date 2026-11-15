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
#include "Sim/DeepAirLineRules.h"
#include "Sim/DeepCaveRules.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepCaptainLines.h"
#include "World/DeepArena.h"
#include "World/DeepSalvage.h"
#include "World/DeepShaft.h"
#include "World/DeepSubmarine.h"

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
	case EDeepRunState::Aboard: return TEXT("aboard");
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
	if (GetOwner()->HasAuthority())
	{
		// A diver placed in the room is aboard from the start (also behind the title screen).
		ADeepDiver* Diver = GetDiver();
		const ADeepSubmarine* Sub = Diver && Diver->GetTether() ? Diver->GetTether()->GetSubmarine() : nullptr;
		if (Diver && Sub && Sub->IsInRoom(Diver->GetActorLocation(), 100.0))
		{
			SetState(EDeepRunState::Aboard);
			Diver->SetAboard(true);
		}
		if (!bStarted && !IsOnTitle())
		{
			BeginRun();
		}
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
	if (NewState != EDeepRunState::Cut)
	{
		// Only a Cut can be an unclip; aboard, dead or a new dive clears it.
		bUnclipped = false;
	}
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
		// The title screen is up: the run waits for a key.
		return;
	}
	if (!bStarted)
	{
		BeginRun();
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

		// Up through the hatch, or arriving at the anchor under it (the winch ends there): aboard.
		if (ReachedHatch(true))
		{
			bWasHome = bHome;
			EnterSub();
			return;
		}
		bWasHome = bHome;

		// The line is the air line (rescope): Patience no longer drains unless the tuning brings the old rule back
		// (then twice as fast while the serpent closes on this diver's line). Zero still cuts: Deep.SetPatience 0.
		const UDeepMonsterBrain* Brain = GetWorld()->GetSubsystem<UDeepMonsterBrain>();
		const bool bHunted = Brain && Brain->GetSerpent() && Brain->GetFocusDiver() == Diver
			&& (Brain->GetState() == EDeepMonsterState::Stalk || Brain->GetState() == EDeepMonsterState::Bite);
		const float Before = PatienceSeconds;
		PatienceSeconds = DeepAirLine::PatienceAfterStep(Before, Dt, bHunted, Tuning.Patience, Tuning.AirLine);
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
		if (ReachedHatch(false))
		{
			// Home on bailout air: re-tether fee, aboard.
			EnterSub();
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
	case EDeepRunState::Aboard:
		StepAboard(Dt);
		break;
	}
}

void UDeepRunComponent::StepAboard(float DeltaSeconds)
{
	ADeepDiver* Diver = GetDiver();
	const ADeepSubmarine* Sub = Diver && Diver->GetTether() ? Diver->GetTether()->GetSubmarine() : nullptr;
	if (bSeated)
	{
		IdleSeconds = 0.f;
		StepHelm(DeltaSeconds);
		return;
	}
	// Fell through the open hatch: into the water, and the dive starts.
	const FDeepSubTuning& SubTuning = UDeepTuningSettings::Get().Sub;
	if (Sub && Sub->IsHatchOpen() && Diver->GetActorLocation().Z < Sub->GetDeckTopZ() - SubTuning.DropBelowDeckM * UDeepRules::CmPerM)
	{
		LeaveSub();
		return;
	}
	IdleSeconds += DeltaSeconds;
	if (SubTuning.IdleInsideSeconds > 0.f && IdleSeconds >= SubTuning.IdleInsideSeconds)
	{
		IdleSeconds = 0.f;
		FireChatter(DeepEvent::IdleInside);
	}
}

void UDeepRunComponent::StepHelm(float DeltaSeconds)
{
	const bool bAutopilot = AutopilotDepthM >= 0.f;
	StepHelmDepth(DeltaSeconds);
	if (bSeated)
	{
		StepShaft(DeltaSeconds, bAutopilot);
	}
}

void UDeepRunComponent::StepShaft(float DeltaSeconds, bool bAutopilot)
{
	ADeepGameState* GameState = GetGameState();
	if (!GameState || DeltaSeconds <= 0.f)
	{
		return;
	}
	// Off, or parked (the game state glides the offset home): no steering, no rock.
	if (!DeepShaft::IsOn() || GameState->bParked)
	{
		SteerVelMps = FVector2D::ZeroVector;
		ScrapeSeconds = 0.f;
		return;
	}
	ShaftClock += DeltaSeconds;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const FDeepCaveTuning& Cave = Tuning.Cave;
	const TArray<float> Stops = Tuning.StopDepthsM();
	FVector BoxMin, BoxMax;
	DeepShaft::SubBoxM(DeepArena::FindSubmarine(GetWorld()), BoxMin, BoxMax);
	const float MinR = UDeepCaveRules::MinRadiusM(BoxMin, BoxMax, Cave);
	const float DepthM = GameState->SubDepthM;

	FVector2D Offset = GameState->SubOffsetM;
	if (bAutopilot)
	{
		// The autopilot holds the passage's centre (a quick ease from wherever the pilot left it).
		const FVector2D Centre = UDeepCaveRules::CentreM(DepthM, Stops, Cave);
		Offset = Centre + (Offset - Centre) * FMath::Exp(-6.f * DeltaSeconds);
		// Within a centimetre: exactly on it, so a parked sub sits exactly where it did before the shaft.
		if (FVector2D::DistSquared(Offset, Centre) < FMath::Square(0.01f))
		{
			Offset = Centre;
		}
		SteerVelMps = FVector2D::ZeroVector;
	}
	// Substeps: a fast sub never passes through the rock between two ticks.
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(DeltaSeconds / 0.05f), 1, 40);
	const float Dt = DeltaSeconds / Steps;
	bool bTouching = false;
	float ImpactMps = 0.f;
	for (int32 Index = 0; Index < Steps; ++Index)
	{
		if (!bAutopilot)
		{
			SteerVelMps = UDeepCaveRules::SteerVelocity(SteerVelMps, HelmSteer, Dt, Cave);
			Offset += SteerVelMps * Dt;
		}
		const FDeepCaveContact Hit = UDeepCaveRules::Contact(Offset, DepthM, BoxMin, BoxMax, Stops, MinR, Cave);
		if (!Hit.bTouching)
		{
			continue;
		}
		bTouching = true;
		ImpactMps = FMath::Max(ImpactMps, Hit.PenetrationM / Dt);
		Offset -= Hit.Normal * Hit.PenetrationM;
		const float Into = FVector2D::DotProduct(SteerVelMps, Hit.Normal);
		if (Into > 0.f)
		{
			SteerVelMps -= Hit.Normal * Into * (1.f + FMath::Clamp(Cave.Bounce, 0.f, 1.f));
		}
	}
	if (!Offset.Equals(GameState->SubOffsetM, 0.0001f))
	{
		GameState->SubOffsetM = Offset;
		DeepArena::MoveRoot(GetWorld());
	}
	if (bAutopilot)
	{
		ScrapeSeconds = 0.f;
		return;
	}

	// A knock (fast into the rock): dents by impact speed, then a cooldown. Holding against it: a scrape, a dent a while.
	const int32 Damage = UDeepCaveRules::HitDamage(ImpactMps, Cave);
	if (Damage > 0 && ShaftClock - LastHitClock >= Cave.HitCooldownSeconds)
	{
		LastHitClock = ShaftClock;
		LastScrapeClock = ShaftClock;
		ScrapeSeconds = 0.f;
		DentHull(Damage, DeepEvent::HullHit, ImpactMps);
		return;
	}
	if (!bTouching)
	{
		return;
	}
	ScrapeSeconds += DeltaSeconds;
	int32 Scraped = 0;
	if (Cave.ScrapeSecondsPerDamage > 0.f && ScrapeSeconds >= Cave.ScrapeSecondsPerDamage)
	{
		ScrapeSeconds -= Cave.ScrapeSecondsPerDamage;
		Scraped = 1;
	}
	if (ShaftClock - LastScrapeClock >= Cave.ScrapeCooldownSeconds)
	{
		LastScrapeClock = ShaftClock;
		DentHull(Scraped, DeepEvent::HullDamage, ImpactMps);
	}
	else if (Scraped > 0)
	{
		DentHull(Scraped, nullptr, ImpactMps);
	}
}

void UDeepRunComponent::DentHull(int32 Points, const TCHAR* Event, float ImpactMps)
{
	ADeepGameState* GameState = GetGameState();
	if (!GameState)
	{
		return;
	}
	const FDeepCaveTuning& Cave = UDeepTuningSettings::Get().Cave;
	const int32 DamageBefore = GameState->HullDamage;
	if (Points > 0)
	{
		GameState->HullDamage += Points;
		// Billed at the console with the harpoon's dents (DeepHarpoon::BillHullRepairs).
		GameState->HullRepairsOwed += Points;
	}
	UE_LOG(LogTemp, Log, TEXT("DEEPCAVE %s +%d hull (%d), impact %.1f m/s at %.1f m"), Event ? Event : TEXT("scrape"), Points, GameState->HullDamage, ImpactMps, GameState->SubDepthM);
	if (Cave.bHullBreachKills && UDeepCaveRules::IsBreached(GameState->HullDamage, Cave))
	{
		// Breached: the room floods, the diver at the helm drowns. The next diver gets a patched hull and the bill.
		GameState->HullDamage = 0;
		HelmInput = 0.f;
		HelmSteer = FVector2D::ZeroVector;
		SteerVelMps = FVector2D::ZeroVector;
		AutopilotDepthM = -1.f;
		bSeated = false;
		bHelmAtLimit = false;
		bWasDriving = false;
		ScrapeSeconds = 0.f;
		DieFor(DeepEvent::HullBreach, false);
		return;
	}
	FDeepEventPayload Payload;
	Payload.DepthM = GameState->SubDepthM;
	Payload.HullIntegrity01 = UDeepCaveRules::HullIntegrity01(GameState->HullDamage, Cave);
	Payload.ImpactMps = ImpactMps;
	if (Event)
	{
		Fire(Event, Payload);
	}
	if (UDeepCaveRules::CrossesCritical(DamageBefore, GameState->HullDamage, Cave))
	{
		Fire(DeepEvent::HullCritical, Payload);
	}
}

void UDeepRunComponent::StepHelmDepth(float DeltaSeconds)
{
	ADeepGameState* GameState = GetGameState();
	if (!GameState)
	{
		return;
	}
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const FDeepSubTuning& Sub = Tuning.Sub;
	const TArray<float> Stops = Tuning.StopDepthsM();
	const float Top = Stops.IsEmpty() ? 0.f : Stops[0];
	const float Limit = GameState->DepthLimitM();

	float Input = FMath::Clamp(HelmInput, -1.f, 1.f);
	bool bJustBlocked = false;
	if (AutopilotDepthM >= 0.f)
	{
		const float Target = FMath::Clamp(AutopilotDepthM, Top, FMath::Max(Top, Limit));
		const float Step = FMath::Max(0.f, Sub.DriveSpeedMps) * DeltaSeconds;
		const float Gap = Target - GameState->SubDepthM;
		if (FMath::Abs(Gap) <= Step || Step <= 0.f)
		{
			// Arrived (or the limit stopped it short): the last bit, then release the control.
			GameState->SubDepthM = Target;
			GameState->bParked = false;
			DeepArena::ApplySubDepth(GetWorld());
			const bool bBlocked = AutopilotDepthM > Limit + 0.01f;
			AutopilotDepthM = -1.f;
			Input = 0.f;
			if (bBlocked)
			{
				bJustBlocked = true;
				FireChatter(DeepEvent::HelmDepthLimit);
			}
			bWasDriving = true;
		}
		else
		{
			Input = Gap > 0.f ? 1.f : -1.f;
		}
	}

	if (!FMath::IsNearlyZero(Input))
	{
		if (Input > 0.f && !bWasDriving)
		{
			FireChatter(DeepEvent::HelmDescend);
		}
		const FDeepHelmStep Step = UDeepRules::HelmStep(GameState->SubDepthM, Input, DeltaSeconds, Top, Limit, Sub);
		if (Step.bAtLimit && !bHelmAtLimit)
		{
			FireChatter(DeepEvent::HelmDepthLimit);
		}
		bHelmAtLimit = Step.bAtLimit;
		bWasDriving = true;
		if (!FMath::IsNearlyEqual(Step.DepthM, GameState->SubDepthM) || GameState->bParked)
		{
			GameState->SubDepthM = Step.DepthM;
			GameState->bParked = false;
			DeepArena::ApplySubDepth(GetWorld());
		}
		return;
	}
	bHelmAtLimit = bJustBlocked;
	// Released: park if a stop is close enough.
	if (!GameState->bParked)
	{
		if (const int32 Stop = UDeepRules::HelmParkStop(GameState->SubDepthM, Stops, Sub.ParkSnapM))
		{
			Park(Stop);
		}
	}
	bWasDriving = false;
}

void UDeepRunComponent::Park(int32 Stop)
{
	ADeepGameState* GameState = GetGameState();
	if (!GameState)
	{
		return;
	}
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	GameState->SubDepthM = Tuning.GetStop(Stop).SubDepthM;
	GameState->bParked = true;
	DeepArena::ApplySubDepth(GetWorld());
	FDeepEventPayload Payload;
	Payload.DepthM = GameState->SubDepthM;
	if (Stop > GameState->DeepestStopReached && Stop >= 2)
	{
		Fire(Stop >= 3 ? DeepEvent::DescendStop3 : DeepEvent::DescendStop2, Payload);
	}
	else
	{
		Fire(DeepEvent::HelmArrive, Payload);
	}
	GameState->DeepestStopReached = FMath::Max(GameState->DeepestStopReached, Stop);
}

// ---------------------------------------------------------------- transitions

void UDeepRunComponent::BeginRun()
{
	bStarted = true;
	ADeepDiver* Diver = GetDiver();
	const ADeepSubmarine* Sub = Diver && Diver->GetTether() ? Diver->GetTether()->GetSubmarine() : nullptr;
	if (Diver && Sub && Sub->IsInRoom(Diver->GetActorLocation(), 100.0))
	{
		PatienceSeconds = UDeepTuningSettings::Get().Patience.MaxSeconds;
		bFirstSalvageThisRun = true;
		bMonsterHeardThisRun = false;
		bRelicSeenThisRun = false;
		bSeated = false;
		IdleSeconds = 0.f;
		SetState(EDeepRunState::Aboard);
		// The first view of a run: the helm ahead along the +Y side, the console ahead right, the hatch ahead left (off every straight walk).
		Diver->ResetToRoom();
		Diver->SetAboard(true);
		return;
	}
	StartDive(EDiveStart::First);
}

bool UDeepRunComponent::ReachedHatch(bool bArrivalOnly) const
{
	const ADeepDiver* Diver = GetDiver();
	const ADeepSubmarine* Sub = Diver && Diver->GetTether() ? Diver->GetTether()->GetSubmarine() : nullptr;
	if (!Sub)
	{
		return false;
	}
	const FVector At = Diver->GetActorLocation();
	const FDeepSubTuning& Tuning = UDeepTuningSettings::Get().Sub;
	const double Deck = Sub->GetDeckTopZ();
	if (Sub->IsInHatchColumn(At) && At.Z >= Deck - Tuning.EnterBelowDeckM * UDeepRules::CmPerM && At.Z <= Deck + Tuning.RoomHeightM * UDeepRules::CmPerM)
	{
		return true;
	}
	const bool bHome = IsDiverHome();
	return bArrivalOnly ? bHome && !bWasHome : bHome;
}

void UDeepRunComponent::StartDive(EDiveStart Reason)
{
	bStarted = true;
	ADeepDiver* Diver = GetDiver();
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	PatienceSeconds = Tuning.Patience.MaxSeconds;
	BailoutAirSeconds = 0.f;
	if (Reason == EDiveStart::First)
	{
		bFirstSalvageThisRun = true;
		bMonsterHeardThisRun = false;
		bRelicSeenThisRun = false;
	}
	NoiseOwed = 0.f;
	bSaidNearThisApproach = false;
	bSeated = false;
	if (Diver)
	{
		Diver->SetWinching(false);
		Diver->SetAboard(false);
		if (Reason == EDiveStart::Hatch)
		{
			// Salvage respawns every dive.
			DeepArena::RespawnSalvage(GetWorld());
			// Look only: through the hatch the diver faces at least this far down, at the bed and (at 500 m) the well's
			// lip and its light, not at black water level with the eye. Never raises a view already lower.
			const float DropPitch = Tuning.Arena.HatchDropPitchDeg;
			if (AController* Controller = Diver->GetController(); Controller && DropPitch < 0.f)
			{
				FRotator View = Controller->GetControlRotation();
				View.Pitch = FMath::Min(FRotator::NormalizeAxis(View.Pitch), DropPitch);
				View.Roll = 0.f;
				Controller->SetControlRotation(View);
				if (APlayerController* Player = Cast<APlayerController>(Controller); Player && !Player->IsLocalController())
				{
					Player->ClientSetRotation(View);
				}
			}
		}
	}
	bWasHome = IsDiverHome();
	bWasOverloaded = Diver && Diver->GetWeight().bOverloaded;
	bWasTaut = false;
	bWasWinching = false;
	SetState(EDeepRunState::Tethered);
	Fire(DeepEvent::DiveStart);
}

void UDeepRunComponent::EnterSub()
{
	ADeepDiver* Diver = GetDiver();
	if (!Diver || !Diver->HasAuthority() || !CanSwim())
	{
		return;
	}
	const bool bWasCut = State == EDeepRunState::Cut;
	const bool bWasUnclipped = bWasCut && bUnclipped;
	Diver->SetWinching(false);
	if (bWasCut)
	{
		// Clipped back on at the hatch: the re-tether fee after a cut, free after a voluntary unclip.
		const int32 Fee = DeepAirLine::ReclipFee(bWasUnclipped, UDeepTuningSettings::Get().Economy, UDeepTuningSettings::Get().AirLine);
		if (Fee > 0 || !bWasUnclipped)
		{
			ChargeFee(Fee);
		}
	}
	BailoutAirSeconds = 0.f;
	bSeated = false;
	IdleSeconds = 0.f;
	SetState(EDeepRunState::Aboard);
	if (const ADeepSubmarine* Sub = Diver->GetTether() ? Diver->GetTether()->GetSubmarine() : nullptr)
	{
		Diver->TeleportDiver(Sub->GetEnterStandLocation());
	}
	// Look only: level the view so the first frame aboard is the room, not the ceiling the diver swam up at.
	if (AController* Controller = Diver->GetController())
	{
		FRotator View = Controller->GetControlRotation();
		View.Pitch = UDeepTuningSettings::Get().Arena.AboardPitchDeg;
		View.Roll = 0.f;
		Controller->SetControlRotation(View);
		if (APlayerController* Player = Cast<APlayerController>(Controller); Player && !Player->IsLocalController())
		{
			Player->ClientSetRotation(View);
		}
	}
	Diver->SetAboard(true);
	Fire(bWasCut && !bWasUnclipped ? DeepEvent::Retether : DeepEvent::EnterSub);
}

bool UDeepRunComponent::LeaveSub()
{
	ADeepDiver* Diver = GetDiver();
	const ADeepGameState* GameState = GetGameState();
	const ADeepSubmarine* Sub = Diver && Diver->GetTether() ? Diver->GetTether()->GetSubmarine() : nullptr;
	if (!Diver || !Diver->HasAuthority() || State != EDeepRunState::Aboard || bSeated || IsOnTitle() || !Sub || (GameState && !GameState->bParked))
	{
		return false;
	}
	// Just under the hatch, clear of the deck and of the way back in.
	const FVector Below = Sub->GetAnchorLocation() - FVector(0.f, 0.f, 60.f);
	if (Diver->GetActorLocation().Z > Below.Z || !Sub->IsInHatchColumn(Diver->GetActorLocation()))
	{
		Diver->TeleportDiver(Below);
	}
	StartDive(EDiveStart::Hatch);
	return true;
}

void UDeepRunComponent::Use()
{
	if (!GetOwner()->HasAuthority() || State != EDeepRunState::Aboard || IsOnTitle())
	{
		return;
	}
	IdleSeconds = 0.f;
	if (bSeated)
	{
		LeaveHelm();
	}
	else if (IsNearHelm())
	{
		TakeHelm();
	}
	else if (IsNearConsole())
	{
		UseConsole();
	}
}

bool UDeepRunComponent::IsNearConsole() const
{
	const ADeepDiver* Diver = GetDiver();
	const ADeepSubmarine* Sub = Diver && Diver->GetTether() ? Diver->GetTether()->GetSubmarine() : nullptr;
	return Sub && State == EDeepRunState::Aboard && !bSeated && Sub->IsWithinUseRange(Diver->GetActorLocation(), Sub->GetConsoleLocation());
}

bool UDeepRunComponent::IsNearHelm() const
{
	const ADeepDiver* Diver = GetDiver();
	const ADeepSubmarine* Sub = Diver && Diver->GetTether() ? Diver->GetTether()->GetSubmarine() : nullptr;
	return Sub && State == EDeepRunState::Aboard && !bSeated && Sub->IsWithinUseRange(Diver->GetActorLocation(), Sub->GetHelmLocation());
}

void UDeepRunComponent::UseConsole()
{
	ADeepDiver* Diver = GetDiver();
	if (!Diver || !Diver->HasAuthority() || State != EDeepRunState::Aboard)
	{
		return;
	}
	bSeated = false;
	IdleSeconds = 0.f;
	LastConsoleTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (Diver->GetCargo().Num() > 0)
	{
		Bank();
		return;
	}
	BuyNextGate(false);
}

bool UDeepRunComponent::BuyNextGate(bool bFree)
{
	ADeepGameState* GameState = GetGameState();
	if (!GameState)
	{
		return false;
	}
	const FDeepEconomyTuning& Economy = UDeepTuningSettings::Get().Economy;
	if (UDeepRules::NextHullPrice(GameState->HullRating, Economy) < 0)
	{
		return false;
	}
	bool bBought = false;
	if (bFree)
	{
		GameState->HullRating += 1;
		bBought = true;
	}
	else
	{
		const FDeepHullPurchase Hull = UDeepRules::TryBuyHull(GameState->Credits, GameState->HullRating, Economy);
		GameState->Credits = Hull.Credits;
		GameState->HullRating = Hull.HullRating;
		bBought = Hull.bBought;
	}
	if (bBought)
	{
		LastGateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		Fire(DeepEvent::HullBought);
	}
	return bBought;
}

void UDeepRunComponent::TakeHelm()
{
	ADeepDiver* Diver = GetDiver();
	if (!Diver || State != EDeepRunState::Aboard || bSeated)
	{
		return;
	}
	bSeated = true;
	HelmInput = 0.f;
	HelmSteer = FVector2D::ZeroVector;
	AutopilotDepthM = -1.f;
	bWasDriving = false;
	bHelmAtLimit = false;
	Diver->SetMoveInput(FVector2D::ZeroVector);
	FireChatter(DeepEvent::HelmTake);
}

void UDeepRunComponent::LeaveHelm()
{
	if (!bSeated)
	{
		return;
	}
	// Standing up releases the control: park if close enough to a stop. In the shaft the sub drifts to a stop sideways.
	HelmInput = 0.f;
	HelmSteer = FVector2D::ZeroVector;
	SteerVelMps = FVector2D::ZeroVector;
	AutopilotDepthM = -1.f;
	StepHelm(0.f);
	bSeated = false;
	bHelmAtLimit = false;
}

void UDeepRunComponent::SetAutopilot(float TargetDepthM)
{
	AutopilotDepthM = TargetDepthM;
	if (TargetDepthM >= 0.f && !bSeated)
	{
		TakeHelm();
		AutopilotDepthM = TargetDepthM;
	}
}

void UDeepRunComponent::SetHelmInput(float Input)
{
	if (GetOwner()->HasAuthority())
	{
		HelmInput = FMath::Clamp(Input, -1.f, 1.f);
		if (!FMath::IsNearlyZero(HelmInput))
		{
			AutopilotDepthM = -1.f;
		}
		else
		{
			bHelmAtLimit = false;
		}
	}
	else
	{
		ServerSetHelmInput(Input);
	}
}

void UDeepRunComponent::ServerSetHelmInput_Implementation(float Input)
{
	SetHelmInput(Input);
}

void UDeepRunComponent::SetHelmSteer(FVector2D Steer)
{
	Steer = Steer.GetClampedToMaxSize(1.f);
	if (GetOwner()->HasAuthority())
	{
		HelmSteer = Steer;
		// Steering takes the helm back from the autopilot, as the depth keys do.
		if (!Steer.IsNearlyZero())
		{
			AutopilotDepthM = -1.f;
		}
	}
	else
	{
		ServerSetHelmSteer(Steer);
	}
}

void UDeepRunComponent::ServerSetHelmSteer_Implementation(FVector2D Steer)
{
	SetHelmSteer(Steer);
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

void UDeepRunComponent::Unclip()
{
	if (State != EDeepRunState::Tethered)
	{
		return;
	}
	// Set first so the Cut it makes is already an unclip when its event goes out; SetState(Cut) keeps it.
	bUnclipped = true;
	CutTetherFor(DeepAirLine::Unclip);
	if (State != EDeepRunState::Cut)
	{
		bUnclipped = false;
	}
}

void UDeepRunComponent::Die(bool bByMonster)
{
	DieFor(bByMonster ? DeepEvent::DeathMonster : DeepEvent::DeathAir, bByMonster);
}

void UDeepRunComponent::DieFor(const TCHAR* Event, bool bByMonster)
{
	bLastDeathMonster = bByMonster;
	ADeepDiver* Diver = GetDiver();
	FDeepEventPayload Payload;
	Payload.DepthM = Diver ? Diver->GetDepthM() : 0.f;
	if (FCString::Strcmp(Event, DeepEvent::HullBreach) == 0)
	{
		Payload.HullIntegrity01 = 0.f;
	}
	if (Diver)
	{
		Diver->SetWinching(false);
		Diver->ClearCargo();
	}
	ChargeFee(UDeepTuningSettings::Get().Economy.DeathFee);
	BailoutAirSeconds = 0.f;
	SetState(EDeepRunState::Dead);
	StateTimeLeft = FMath::Max(0.f, UDeepTuningSettings::Get().Run.DeathScreenSeconds);
	Fire(Event, Payload);
}

void UDeepRunComponent::Respawn()
{
	if (ADeepGameState* GameState = GetGameState())
	{
		GameState->DiverNumber += 1;
	}
	StateTimeLeft = 0.f;
	PatienceSeconds = UDeepTuningSettings::Get().Patience.MaxSeconds;
	BailoutAirSeconds = 0.f;
	bSeated = false;
	IdleSeconds = 0.f;
	// The replacement diver starts aboard, on the deck.
	SetState(EDeepRunState::Aboard);
	if (ADeepDiver* Diver = GetDiver())
	{
		Diver->ResetToRoom();
		Diver->SetAboard(true);
	}
	Fire(DeepEvent::Respawn);
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
	if (!Diver || !Diver->HasAuthority() || IsOnTitle())
	{
		return;
	}
	if (CanSwim())
	{
		// Cheats and tests bank from the water: come aboard first (a cut diver pays the re-tether fee).
		EnterSub();
	}
	if (State != EDeepRunState::Aboard)
	{
		return;
	}
	bSeated = false;
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
		// The next layer gate (flashlight, then hull upgrade) is bought automatically when affordable.
		const FDeepHullPurchase Hull = UDeepRules::TryBuyHull(GameState->Credits, GameState->HullRating, Economy);
		GameState->Credits = Hull.Credits;
		GameState->HullRating = Hull.HullRating;
		Report.bHullBought = Hull.bBought;
		Report.HullSpent = Hull.Spent;
		Report.HullRating = Hull.HullRating;
		Report.NextHullPrice = UDeepRules::NextHullPrice(Hull.HullRating, Economy);
		Report.CreditsAfter = GameState->Credits;
		Report.NextStop = GameState->Stop;
		if (Hull.bBought)
		{
			LastGateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		}
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
	// Back on the deck: the helm, the hatch and the console are all there.
	IdleSeconds = 0.f;
	SetState(EDeepRunState::Aboard);
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
		DeepArena::ApplySubDepth(GetWorld());
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
	LastConsoleTime = -100.f;
	LastGateTime = -100.f;
	MonsterThreat01 = 0.f;
	bLastDeathMonster = false;
	bRelicSeenThisRun = false;
	bFirstSalvageThisRun = true;
	bMonsterHeardThisRun = false;
	bSeated = false;
	bHelmAtLimit = false;
	HelmInput = 0.f;
	AutopilotDepthM = -1.f;
	IdleSeconds = 0.f;
	HelmSteer = FVector2D::ZeroVector;
	SteerVelMps = FVector2D::ZeroVector;
	ShaftClock = 0.0;
	LastHitClock = -1000.0;
	LastScrapeClock = -1000.0;
	ScrapeSeconds = 0.f;
	ChatterLastTime.Reset();
	// Every run starts aboard, on the deck; on the title it waits there for a key.
	DeepArena::RespawnSalvage(GetWorld());
	SetState(EDeepRunState::Aboard);
	Diver->ResetToRoom();
	Diver->SetAboard(true);
	bStarted = !bToTitle;
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

void UDeepRunComponent::RequestUnclip()
{
	if (GetOwner()->HasAuthority())
	{
		Unclip();
	}
	else
	{
		ServerRequestUnclip();
	}
}

void UDeepRunComponent::ServerRequestUnclip_Implementation()
{
	Unclip();
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
	DOREPLIFETIME(UDeepRunComponent, bUnclipped);
	DOREPLIFETIME(UDeepRunComponent, StateTimeLeft);
	DOREPLIFETIME(UDeepRunComponent, Settlement);
	DOREPLIFETIME(UDeepRunComponent, bSeated);
	DOREPLIFETIME(UDeepRunComponent, bHelmAtLimit);
	DOREPLIFETIME(UDeepRunComponent, LastConsoleTime);
	DOREPLIFETIME(UDeepRunComponent, LastGateTime);
	DOREPLIFETIME(UDeepRunComponent, LastRefillFraction);
	DOREPLIFETIME(UDeepRunComponent, LastRefillTime);
	DOREPLIFETIME(UDeepRunComponent, LastFee);
	DOREPLIFETIME(UDeepRunComponent, LastFeeTime);
	DOREPLIFETIME(UDeepRunComponent, BiteAlarmLeft);
	DOREPLIFETIME(UDeepRunComponent, bBiteAlarm);
	DOREPLIFETIME(UDeepRunComponent, MonsterThreat01);
	DOREPLIFETIME(UDeepRunComponent, bLastDeathMonster);
}
