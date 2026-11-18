#include "Monster/DeepMonsterBrain.h"

#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Diver/DeepTetherComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameState.h"
#include "Monster/DeepMonsterRules.h"
#include "Monster/DeepMonsterSettings.h"
#include "Monster/DeepSerpent.h"
#include "Monster/DeepSerpentController.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepEventBus.h"
#include "Sim/DeepRules.h"
#include "World/DeepAbyssFloor.h"
#include "World/DeepArena.h"
#include "World/DeepCaveString.h"
#include "World/DeepSubmarine.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepMonster, Log, All);

namespace
{
	using DeepMonster::CmPerM;
}

// ---------------------------------------------------------------- subsystem

bool UDeepMonsterBrain::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UDeepMonsterBrain::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDeepMonsterBrain, STATGROUP_Tickables);
}

bool UDeepMonsterBrain::IsTickable() const
{
	const UWorld* World = GetWorld();
	return World && World->HasBegunPlay();
}

void UDeepMonsterBrain::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	if (!BusHandle.IsValid())
	{
		if (UDeepEventBus* Bus = UDeepEventBus::Get(this))
		{
			BusHandle = Bus->OnEvent.AddUObject(this, &UDeepMonsterBrain::HandleBusEvent);
		}
	}
	Step(DeltaTime);
	HearHelm(DeltaTime);
}

void UDeepMonsterBrain::Deinitialize()
{
	if (BusHandle.IsValid())
	{
		if (UDeepEventBus* Bus = UDeepEventBus::Get(this))
		{
			Bus->OnEvent.Remove(BusHandle);
		}
		BusHandle.Reset();
	}
	Serpent.Reset();
	Super::Deinitialize();
}

// ---------------------------------------------------------------- reads

ADeepSerpentController* UDeepMonsterBrain::GetSerpentController() const
{
	return Serpent.IsValid() ? Cast<ADeepSerpentController>(Serpent->GetController()) : nullptr;
}

float UDeepMonsterBrain::GetThreat01() const
{
	const ADeepSerpentController* Mind = GetSerpentController();
	return Mind ? Mind->GetThreat01() : 0.f;
}

EDeepMonsterState UDeepMonsterBrain::GetState() const
{
	const ADeepSerpentController* Mind = GetSerpentController();
	return Mind ? Mind->GetState() : EDeepMonsterState::Lurk;
}

float UDeepMonsterBrain::GetNoise() const
{
	const ADeepSerpentController* Mind = GetSerpentController();
	return Mind ? Mind->GetNoise() : 0.f;
}

float UDeepMonsterBrain::GetTetherDistanceM() const
{
	const ADeepSerpentController* Mind = GetSerpentController();
	return Mind ? Mind->GetTetherDistanceM() : 0.f;
}

float UDeepMonsterBrain::GetDiverDistanceM() const
{
	const ADeepSerpentController* Mind = GetSerpentController();
	return Mind ? Mind->GetDiverDistanceM() : 0.f;
}

FString UDeepMonsterBrain::BuildStateJson() const
{
	const ADeepSerpentController* Mind = GetSerpentController();
	return FString::Printf(
		TEXT("{\"present\":%s,\"state\":\"%s\",\"noise\":%.2f,\"heard\":%.2f,\"lit_s\":%.2f,\"cooldown_s\":%.2f,\"tether_distance_m\":%.2f,\"diver_distance_m\":%.2f,\"threat\":%.3f,\"stop\":%d,\"first_sight_done\":%s}"),
		Mind ? TEXT("true") : TEXT("false"),
		*UDeepMonsterRules::StateName(GetState()),
		GetNoise(),
		Mind ? Mind->GetHeardTotal() : 0.f,
		Mind ? Mind->GetLitSeconds() : 0.f,
		Mind ? Mind->GetCooldownSeconds() : 0.f,
		GetTetherDistanceM(), GetDiverDistanceM(), GetThreat01(), CurrentStop(),
		Mind && Mind->IsFirstSightDone() ? TEXT("true") : TEXT("false"));
}

int32 UDeepMonsterBrain::CurrentStop() const
{
	const UWorld* World = GetWorld();
	const ADeepGameState* GameState = World ? World->GetGameState<ADeepGameState>() : nullptr;
	return GameState ? GameState->Stop : 1;
}

ADeepDiver* UDeepMonsterBrain::GetFocusDiver() const
{
	const ADeepSerpentController* Mind = GetSerpentController();
	return Mind ? Mind->GetFocusDiver() : nullptr;
}

void UDeepMonsterBrain::GatherDivers(TArray<ADeepDiver*>& Out) const
{
	Out.Reset();
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ADeepDiver> It(World); It; ++It)
		{
			const UDeepRunComponent* Run = IsValid(*It) ? It->GetRun() : nullptr;
			if (IsValid(*It) && (!Run || Run->CanSwim()))
			{
				Out.Add(*It);
			}
		}
	}
}

// ---------------------------------------------------------------- tethers

void UDeepMonsterBrain::NotifyTetherCut(ADeepDiver* Diver)
{
	if (Diver)
	{
		CutDivers.Add(Diver);
	}
}

void UDeepMonsterBrain::NotifyRetethered(ADeepDiver* Diver)
{
	CutDivers.Remove(Diver);
}

bool UDeepMonsterBrain::IsTethered(const ADeepDiver* Diver) const
{
	if (!Diver || CutDivers.Contains(Diver))
	{
		return false;
	}
	const UDeepRunComponent* Run = Diver->GetRun();
	const UDeepTetherComponent* Tether = Diver->GetTether();
	return Tether && Tether->HasAnchor() && (!Run || Run->HasTether());
}

bool UDeepMonsterBrain::TetherEnds(const ADeepDiver* Diver, FVector& OutAnchor, FVector& OutDiverEnd) const
{
	if (!IsTethered(Diver))
	{
		return false;
	}
	OutAnchor = Diver->GetTether()->GetAnchorLocation();
	OutDiverEnd = Diver->GetTetherAttachLocation();
	return true;
}

float UDeepMonsterBrain::LurkRadiusCm(const ADeepDiver* Diver, const FDeepMonsterTuning& Tuning) const
{
	// Circle the middle of the tether far enough out that no point of the line (or the diver) is within LurkRadiusM.
	float HalfSpanCm = 0.f;
	FVector A, B;
	if (TetherEnds(Diver, A, B))
	{
		HalfSpanCm = 0.5f * static_cast<float>(FVector(B.X - A.X, B.Y - A.Y, 0.f).Size());
	}
	return Tuning.LurkRadiusM * CmPerM + HalfSpanCm;
}

// ---------------------------------------------------------------- cheats and tests

void UDeepMonsterBrain::AddNoise(float Amount)
{
	if (ADeepSerpentController* Mind = GetSerpentController())
	{
		Mind->AddNoise(Amount);
	}
}

void UDeepMonsterBrain::SetDebugLit(float Seconds)
{
	if (ADeepSerpentController* Mind = GetSerpentController())
	{
		Mind->SetDebugLit(Seconds);
	}
}

void UDeepMonsterBrain::ForceState(EDeepMonsterState NewState)
{
	if (ADeepSerpentController* Mind = GetSerpentController())
	{
		Mind->ForceState(NewState);
	}
}

bool UDeepMonsterBrain::StartFirstSight()
{
	ADeepSerpentController* Mind = GetSerpentController();
	return Mind && Mind->StartFirstSight();
}

ADeepSerpent* UDeepMonsterBrain::SpawnSerpent(bool bForced)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	bForcedSpawn = bForcedSpawn || bForced;
	if (Serpent.IsValid())
	{
		return Serpent.Get();
	}

	// Centre on the nearest tether middle (any diver will do), else the sub's airlock.
	TArray<ADeepDiver*> Divers;
	GatherDivers(Divers);
	const ADeepDiver* Target = Divers.Num() > 0 ? Divers[0] : nullptr;
	const FDeepMonsterTuning Tuning = UDeepMonsterSettings::Get().GetTuning(CurrentStop());

	FVector Centre = FVector::ZeroVector;
	FVector A, B;
	if (TetherEnds(Target, A, B))
	{
		Centre = (A + B) * 0.5f;
	}
	else if (Target)
	{
		Centre = Target->GetActorLocation();
	}
	else
	{
		for (TActorIterator<ADeepSubmarine> It(World); It; ++It)
		{
			Centre = It->GetAnchorLocation();
			break;
		}
	}

	// Somewhere on the lurk circle, swimming around it: heard and glimpsed, never on top of you.
	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
	FVector Location = Centre + Out * LurkRadiusCm(Target, Tuning);
	FVector Facing(-Out.Y, Out.X, 0.f);
	// The caves (M2b): on the lurk circle round the berth cavern's Serpent socket, kept in its room.
	if (DeepArena::IsWorldCaves(World))
	{
		const ADeepCaveString* CaveString = DeepCaveString::Find(World);
		FVector At, Way;
		if (CaveString && CaveString->SerpentLurkPoint(CurrentStop(), Tuning.LurkRadiusM * CmPerM, Angle, At, Way))
		{
			Location = At;
			Facing = Way;
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADeepSerpent* Spawned = World->SpawnActor<ADeepSerpent>(Location, Facing.Rotation(), Params);
	if (!Spawned)
	{
		return nullptr;
	}
	if (!Spawned->GetController())
	{
		// Auto-possession runs at spawn on a server; make sure of it in odd worlds (tests).
		Spawned->SpawnDefaultController();
	}
	Spawned->PlaceHead(Location, Facing);
	Serpent = Spawned;
	UE_LOG(LogDeepMonster, Log, TEXT("Serpent spawned at stop %d, %.0f m from the tether, controller %s"),
		CurrentStop(), LurkRadiusCm(Target, Tuning) / CmPerM, GetSerpentController() ? TEXT("possessed") : TEXT("MISSING"));
	return Spawned;
}

void UDeepMonsterBrain::Step(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const UDeepMonsterSettings& Settings = UDeepMonsterSettings::Get();
	const int32 Stop = CurrentStop();

	TArray<ADeepDiver*> Divers;
	GatherDivers(Divers);

	// Spawn at FirstStop and deeper; remove it again above (a new run) unless a cheat put it there.
	if (Serpent.IsValid() && !bForcedSpawn && Stop < Settings.FirstStop)
	{
		if (AController* Mind = Serpent->GetController())
		{
			Mind->Destroy();
		}
		Serpent->Destroy();
		Serpent.Reset();
	}
	if (!Serpent.IsValid() && Stop >= Settings.FirstStop && Divers.Num() > 0)
	{
		SpawnSerpent(false);
	}
}

void UDeepMonsterBrain::ResetForRun()
{
	if (Serpent.IsValid())
	{
		if (AController* Mind = Serpent->GetController())
		{
			Mind->Destroy();
		}
		Serpent->Destroy();
	}
	Serpent.Reset();
	CutDivers.Reset();
	bForcedSpawn = false;
}

FDeepWell UDeepMonsterBrain::GetWell() const
{
	FDeepWell Well;
	const UWorld* World = GetWorld();
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	// The well is the shaft's (cut in the abyss floor); the caves' relic lies on a cavern floor (D-068).
	if (!World || DeepArena::IsWorldCaves(World) || CurrentStop() != Tuning.Arena.RelicStop)
	{
		return Well;
	}
	// The shaft is cut round the relic's spot (ADeepAbyssFloor::RestPointsFromTuning, negative radius); its floor is that
	// point's depth below the anchor, its rim the bed just outside it.
	const FVector Anchor = DeepArena::ArenaAnchor(World);
	const TArray<FVector4> Rest = ADeepAbyssFloor::RestPointsFromTuning();
	const FVector4* Pit = Rest.FindByPredicate([](const FVector4& R) { return R.W < 0.f; });
	if (!Pit)
	{
		return Well;
	}
	const double RadiusM = -Pit->W;
	const FVector2D RimM(Pit->X + RadiusM * 1.6, Pit->Y);
	const double RimBelowM = ADeepAbyssFloor::SeabedBelowM(RimM, Rest, Tuning.Look);
	Well.AxisCm = FVector2D(Anchor.X + Pit->X * CmPerM, Anchor.Y + Pit->Y * CmPerM);
	Well.FloorZCm = Anchor.Z - Pit->Z * CmPerM;
	Well.TopZCm = Anchor.Z - RimBelowM * CmPerM;
	Well.RadiusCm = RadiusM * CmPerM;
	Well.bValid = Well.TopZCm > Well.FloorZCm + 5.0 * CmPerM;
	return Well;
}

// ---------------------------------------------------------------- what it hears beyond the divers (lane G)

void UDeepMonsterBrain::HandleBusEvent(FName Event, const FDeepEventPayload& Payload)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !Serpent.IsValid())
	{
		return;
	}
	const float Loudness = UDeepMonsterRules::NoiseForEvent(Event, Payload.ImpactMps, UDeepMonsterSettings::Get().GetTuning(CurrentStop()));
	if (Loudness <= 0.f)
	{
		return;
	}
	// Where it happened: the actor it is about, else the sub (a hull knock, the helm's sonar).
	AActor* Where = Payload.Source.Get();
	if (!Where)
	{
		Where = DeepArena::FindSubmarine(World);
	}
	if (Where)
	{
		UDeepMonsterRules::ReportDiverNoise(World, Where->GetActorLocation(), Loudness, Where);
	}
}

void UDeepMonsterBrain::HearHelm(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	ADeepSubmarine* Sub = World ? DeepArena::FindSubmarine(World) : nullptr;
	if (!Sub || !Serpent.IsValid())
	{
		return;
	}
	HelmNoiseAccum += UDeepMonsterRules::HelmNoise(Sub->GetDriveSpeedMps(), UDeepTuningSettings::Get().Sub.DriveSpeedMps, DeltaSeconds,
		UDeepMonsterSettings::Get().GetTuning(CurrentStop()));
	// Reported in lumps once a second: the hearing sense hears events, not a trickle.
	HelmClockS += DeltaSeconds;
	if (HelmClockS >= 1.f)
	{
		if (HelmNoiseAccum > 0.f)
		{
			UDeepMonsterRules::ReportDiverNoise(World, Sub->GetActorLocation(), HelmNoiseAccum, Sub);
		}
		HelmNoiseAccum = 0.f;
		HelmClockS = 0.f;
	}
}
