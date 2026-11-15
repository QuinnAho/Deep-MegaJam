#include "World/DeepArena.h"

#include "Engine/World.h"
#include "Game/DeepGameState.h"
#include "EngineUtils.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepAbyssFloor.h"
#include "World/DeepArenaRoot.h"
#include "World/DeepCavern.h"
#include "World/DeepDressing.h"
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

	ADeepArenaRoot* FindArenaRoot(const UWorld* World)
	{
		return World ? FindFirst<ADeepArenaRoot>(World) : nullptr;
	}

	FVector ArenaAnchor(const UWorld* World)
	{
		const ADeepSubmarine* Sub = FindSubmarine(World);
		if (!Sub)
		{
			return FVector::ZeroVector;
		}
		const ADeepArenaRoot* Root = FindArenaRoot(World);
		return Sub->GetAnchorLocation() + (Root ? Root->GetActorLocation() - Sub->GetActorLocation() : FVector::ZeroVector);
	}

	static void HangFromRoot(UWorld* World, AActor* Actor)
	{
		if (ADeepArenaRoot* Root = Actor ? FindArenaRoot(World) : nullptr)
		{
			Actor->AttachToActor(Root, FAttachmentTransformRules::KeepWorldTransform);
		}
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
			HangFromRoot(World, Salvage);
		}
		return Salvage;
	}

	static int32 SpawnSalvageRing(UWorld* World, const ADeepSubmarine* Sub)
	{
		const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
		const FVector Anchor = ArenaAnchor(World);
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

	static int32 CurrentStop(const UWorld* World)
	{
		const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
		return State ? State->Stop : 1;
	}

	ADeepSalvage* FindRelic(const UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		for (TActorIterator<ADeepSalvage> It(const_cast<UWorld*>(World)); It; ++It)
		{
			if (IsValid(*It) && It->GetKind().Size == EDeepSalvageSize::Relic)
			{
				return *It;
			}
		}
		return nullptr;
	}

	ADeepSalvage* EnsureRelic(UWorld* World, int32 Stop)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			return nullptr;
		}
		const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
		ADeepSalvage* Relic = FindRelic(World);
		if (Stop < Tuning.Arena.RelicStop)
		{
			if (Relic)
			{
				Relic->Destroy();
			}
			return nullptr;
		}
		const ADeepSubmarine* Sub = FindSubmarine(World);
		FDeepSalvageKind Kind;
		if (Relic || !Sub || !Tuning.FindSalvageKind(EDeepSalvageSize::Relic, Kind))
		{
			return Relic;
		}
		const FVector Offset = UDeepRules::RelicOffsetCm(Tuning.Arena, Tuning.GetStop(Stop).SubDepthM, UDeepRules::TetherLengthM(Stop, Tuning.Tether));
		Relic = SpawnSalvage(World, Kind, ArenaAnchor(World) + Offset);
		UE_LOG(LogDeepArena, Log, TEXT("Relic placed %.1f m below the anchor at stop %d"), -Offset.Z / UDeepRules::CmPerM, Stop);
		return Relic;
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
		const int32 Spawned = SpawnSalvageRing(World, Sub);
		return Spawned + (EnsureRelic(World, CurrentStop(World)) ? 1 : 0);
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
		if (!FindArenaRoot(World))
		{
			World->SpawnActor<ADeepArenaRoot>(Sub->GetActorLocation(), FRotator::ZeroRotator, SpawnParams());
		}

		if (!FindFirst<ADeepSalvage>(World))
		{
			SpawnSalvageRing(World, Sub);
		}

		if (!FindFirst<ADeepAbyssFloor>(World))
		{
			// The sea bed is built around the anchor, through every salvage spot's resting point (ADeepAbyssFloor).
			if (ADeepAbyssFloor* Floor = World->SpawnActor<ADeepAbyssFloor>(ArenaAnchor(World), FRotator::ZeroRotator, SpawnParams()))
			{
				HangFromRoot(World, Floor);
			}
		}

		if (!FindFirst<ADeepCavern>(World))
		{
			// Rock walls and a roof round the arena, built round the same anchor and hung the same way (ADeepCavern).
			if (ADeepCavern* Cavern = World->SpawnActor<ADeepCavern>(ArenaAnchor(World), FRotator::ZeroRotator, SpawnParams()))
			{
				HangFromRoot(World, Cavern);
			}
		}

		if (!FindFirst<ADeepEnvironment>(World))
		{
			World->SpawnActor<ADeepEnvironment>(Sub->GetActorLocation(), FRotator::ZeroRotator, SpawnParams());
		}

		// Rock and ruin from the packs (Content/Data/Dressing.csv), hung from the root; every machine reads the rows itself.
		if (!FindFirst<ADeepDressing>(World))
		{
			World->SpawnActor<ADeepDressing>(FindArenaRoot(World)->GetActorLocation(), FRotator::ZeroRotator, SpawnParams());
		}
		ApplyStop(World, Stop);
		return Sub;
	}

	static void PositionRoot(UWorld* World)
	{
		const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
		const ADeepSubmarine* Sub = FindSubmarine(World);
		ADeepArenaRoot* Root = FindArenaRoot(World);
		if (!State || !Sub || !Root)
		{
			return;
		}
		// Deeper than the loaded stop: the world is above us, so the root goes up.
		const float OffsetM = State->SubDepthM - UDeepTuningSettings::Get().GetStop(State->Stop).SubDepthM;
		Root->SetActorLocation(Sub->GetActorLocation() + FVector(0.f, 0.f, OffsetM * UDeepRules::CmPerM));
	}

	void ApplyStop(UWorld* World, int32 Stop)
	{
		if (ADeepEnvironment* Environment = World ? FindFirst<ADeepEnvironment>(World) : nullptr)
		{
			Environment->SetStop(Stop);
		}
		PositionRoot(World);
		EnsureRelic(World, Stop);
		// The root may have re-based: dressing placed by absolute depth follows in the same frame.
		if (ADeepDressing* Dressing = World ? FindFirst<ADeepDressing>(World) : nullptr)
		{
			Dressing->Refresh();
		}
		if (ADeepSubmarine* Sub = FindSubmarine(World))
		{
			const ADeepGameState* State = World->GetGameState<ADeepGameState>();
			Sub->SetHatchOpen(!State || State->bParked);
		}
	}

	void ApplySubDepth(UWorld* World)
	{
		ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
		if (!State || World->GetNetMode() == NM_Client)
		{
			return;
		}
		const int32 Nearest = UDeepRules::NearestStop(State->SubDepthM, UDeepTuningSettings::Get().StopDepthsM());
		const bool bNewArena = Nearest != State->Stop;
		State->Stop = Nearest;
		ApplyStop(World, Nearest);
		if (bNewArena)
		{
			RespawnSalvage(World);
		}
	}
}
