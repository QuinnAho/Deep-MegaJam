#include "World/DeepArena.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepAbyssFloor.h"
#include "World/DeepEnvironment.h"
#include "World/DeepSalvage.h"
#include "World/DeepSubmarine.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepArena, Log, All);

namespace DeepArena
{
	template <typename T>
	static T* FindFirst(const UWorld* World)
	{
		for (TActorIterator<T> It(const_cast<UWorld*>(World)); It; ++It)
		{
			if (IsValid(*It))
			{
				return *It;
			}
		}
		return nullptr;
	}

	static FActorSpawnParameters SpawnParams()
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return Params;
	}

	ADeepSubmarine* FindSubmarine(const UWorld* World)
	{
		return World ? FindFirst<ADeepSubmarine>(World) : nullptr;
	}

	ADeepSalvage* SpawnSalvage(UWorld* World, const FDeepSalvageKind& Kind, const FVector& LocationCm)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			return nullptr;
		}
		ADeepSalvage* Salvage = World->SpawnActor<ADeepSalvage>(LocationCm, FRotator::ZeroRotator, SpawnParams());
		if (Salvage)
		{
			Salvage->SetKind(Kind);
		}
		return Salvage;
	}

	static int32 SpawnSalvageRing(UWorld* World, const ADeepSubmarine* Sub)
	{
		const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
		const FVector Anchor = Sub->GetAnchorLocation();
		int32 Spawned = 0;
		for (const FDeepSalvageSpot& Spot : Tuning.Arena.SalvageSpots)
		{
			FDeepSalvageKind Kind;
			if (!Tuning.FindSalvageKind(Spot.Size, Kind))
			{
				UE_LOG(LogDeepArena, Warning, TEXT("No salvage kind for size %d; spot skipped"), static_cast<int32>(Spot.Size));
				continue;
			}
			if (SpawnSalvage(World, Kind, Anchor + UDeepRules::SalvageSpotOffsetCm(Spot)))
			{
				++Spawned;
			}
		}
		UE_LOG(LogDeepArena, Log, TEXT("Spawned %d salvage pieces"), Spawned);
		return Spawned;
	}

	int32 RespawnSalvage(UWorld* World)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			return 0;
		}
		const ADeepSubmarine* Sub = FindSubmarine(World);
		if (!Sub)
		{
			return 0;
		}
		for (TActorIterator<ADeepSalvage> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				It->Destroy();
			}
		}
		return SpawnSalvageRing(World, Sub);
	}

	ADeepSubmarine* EnsureArena(UWorld* World, int32 Stop)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			return nullptr;
		}
		const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();

		ADeepSubmarine* Sub = FindSubmarine(World);
		if (!Sub)
		{
			Sub = World->SpawnActor<ADeepSubmarine>(Tuning.Arena.SubLocationM * UDeepRules::CmPerM, FRotator::ZeroRotator, SpawnParams());
			UE_LOG(LogDeepArena, Log, TEXT("No submarine in the map; spawned one at %s"), Sub ? *Sub->GetActorLocation().ToString() : TEXT("(failed)"));
		}
		if (!Sub)
		{
			return nullptr;
		}

		if (!FindFirst<ADeepSalvage>(World))
		{
			SpawnSalvageRing(World, Sub);
		}

		if (!FindFirst<ADeepAbyssFloor>(World))
		{
			const FVector FloorAt = Sub->GetActorLocation() - FVector(0.f, 0.f, Tuning.Arena.FloorBelowSubM * UDeepRules::CmPerM);
			if (ADeepAbyssFloor* Floor = World->SpawnActor<ADeepAbyssFloor>(FloorAt, FRotator::ZeroRotator, SpawnParams()))
			{
				Floor->SetSizeM(Tuning.Arena.FloorSizeM);
			}
		}

		if (!FindFirst<ADeepEnvironment>(World))
		{
			World->SpawnActor<ADeepEnvironment>(Sub->GetActorLocation(), FRotator::ZeroRotator, SpawnParams());
		}
		ApplyStop(World, Stop);
		return Sub;
	}

	void ApplyStop(UWorld* World, int32 Stop)
	{
		if (ADeepEnvironment* Environment = World ? FindFirst<ADeepEnvironment>(World) : nullptr)
		{
			Environment->SetStop(Stop);
		}
	}
}
