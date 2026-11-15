#include "Monster/DeepMonsterBrain.h"

#include "Diver/DeepDiver.h"
#include "Diver/DeepTetherComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameState.h"
#include "Monster/DeepMonsterRules.h"
#include "Monster/DeepMonsterSettings.h"
#include "Monster/DeepSerpent.h"
#include "Monster/DeepSerpentController.h"
#include "Sim/DeepTuningSettings.h"
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
	Step(DeltaTime);
}

void UDeepMonsterBrain::Deinitialize()
{
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
		TEXT("{\"present\":%s,\"state\":\"%s\",\"noise\":%.2f,\"heard\":%.2f,\"lit_s\":%.2f,\"cooldown_s\":%.2f,\"tether_distance_m\":%.2f,\"diver_distance_m\":%.2f,\"threat\":%.3f,\"stop\":%d}"),
		Mind ? TEXT("true") : TEXT("false"),
		*UDeepMonsterRules::StateName(GetState()),
		GetNoise(),
		Mind ? Mind->GetHeardTotal() : 0.f,
		Mind ? Mind->GetLitSeconds() : 0.f,
		Mind ? Mind->GetCooldownSeconds() : 0.f,
		GetTetherDistanceM(), GetDiverDistanceM(), GetThreat01(), CurrentStop());
}

int32 UDeepMonsterBrain::CurrentStop() const
{
	const UWorld* World = GetWorld();
	const ADeepGameState* GameState = World ? World->GetGameState<ADeepGameState>() : nullptr;
	return GameState ? GameState->Stop : 1;
}

void UDeepMonsterBrain::GatherDivers(TArray<ADeepDiver*>& Out) const
{
	Out.Reset();
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<ADeepDiver> It(World); It; ++It)
		{
			if (IsValid(*It))
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
	const UDeepTetherComponent* Tether = Diver->GetTether();
	return Tether && Tether->HasAnchor();
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
	const FVector Location = Centre + Out * LurkRadiusCm(Target, Tuning);
	const FVector Facing(-Out.Y, Out.X, 0.f);

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

void UDeepMonsterBrain::ReportDiverNoise(const TArray<ADeepDiver*>& Divers, float DeltaSeconds)
{
	const FDeepSwimTuning& Swim = UDeepTuningSettings::Get().Swim;
	const FDeepMonsterTuning Tuning = UDeepMonsterSettings::Get().GetTuning(CurrentStop());
	const float SprintMps = UDeepMonsterRules::SprintThresholdMps(Swim.SwimSpeedMps, Swim.SprintMultiplier);
	for (ADeepDiver* Diver : Divers)
	{
		const float CargoKg = Diver->GetCargoKg();
		const float* LastKg = LastCargoKg.Find(Diver);
		const float GainKg = LastKg ? FMath::Max(0.f, CargoKg - *LastKg) : 0.f;
		LastCargoKg.Add(Diver, CargoKg);

		const float SpeedMps = static_cast<float>(Diver->GetVelocity().Size()) / CmPerM;
		float& Owed = NoiseOwed.FindOrAdd(Diver);
		Owed += UDeepMonsterRules::NoiseFromDiver(Diver->IsWinching(), SpeedMps, SprintMps, GainKg, DeltaSeconds, Tuning);
		if (Owed >= 1.f)
		{
			UDeepMonsterRules::ReportDiverNoise(Diver, Diver->GetActorLocation(), Owed, Diver);
			Owed = 0.f;
		}
	}
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

	// The diver makes noise whether or not anything is listening yet.
	ReportDiverNoise(Divers, FMath::Max(0.f, DeltaSeconds));
}
