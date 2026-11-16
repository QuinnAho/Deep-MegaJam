#include "World/DeepArena.h"

#include "Sim/DeepPilotRules.h"

#include "Engine/World.h"
#include "Game/DeepGameState.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Sim/DeepMovingSubRules.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepAbyssFloor.h"
#include "World/DeepArenaRoot.h"
#include "World/DeepCavern.h"
#include "World/DeepDressing.h"
#include "World/DeepEnvironment.h"
#include "World/DeepSalvage.h"
#include "World/DeepShaft.h"
#include "World/DeepSubmarine.h"
#include "World/DeepSubRoom.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepArena, Log, All);

static TAutoConsoleVariable<int32> CVarDeepWorldStill(
	TEXT("Deep.World.Still"),
	0,
	TEXT("1: the arena root stays still and the hull moves through it (moving-sub step 3); 0: D-022, the hull fixed and the root moved. Read when the arena is built, fixed for the world's life."),
	ECVF_Default);

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

	ADeepSubRoom* FindRoom(const UWorld* World)
	{
		return World ? FindFirst<ADeepSubRoom>(World) : nullptr;
	}

	ADeepSubRoom* EnsureRoom(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		if (ADeepSubRoom* Room = FindRoom(World))
		{
			return Room;
		}
		const FVector At = UDeepTuningSettings::Get().Sub.RoomSetLocationM * UDeepRules::CmPerM;
		return World->SpawnActor<ADeepSubRoom>(At, FRotator::ZeroRotator, SpawnParams());
	}

	ADeepArenaRoot* FindArenaRoot(const UWorld* World)
	{
		return World ? FindFirst<ADeepArenaRoot>(World) : nullptr;
	}

	bool IsWorldStill(const UWorld* World)
	{
		const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
		return State && State->bWorldStill;
	}

	static double DatumCm()
	{
		return UDeepTuningSettings::Get().MovingSub.DepthDatumZM * UDeepRules::CmPerM;
	}

	FTransform StillStopFrame(const UWorld* World, int32 Stop)
	{
		const ADeepSubmarine* Sub = FindSubmarine(World);
		return UDeepMovingSubRules::StillStopFrame(Sub ? Sub->GetHomeTransform() : FTransform::Identity, UDeepTuningSettings::Get().GetStop(Stop).SubDepthM, DatumCm());
	}

	/** Deep.World.Still: the map from where D-022 would place the loaded stop's content now to where it goes (identity with the flag off). */
	static FTransform StillMapNow(const UWorld* World)
	{
		const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
		const ADeepSubmarine* Sub = FindSubmarine(World);
		if (!State || !Sub || !State->bWorldStill)
		{
			return FTransform::Identity;
		}
		return UDeepMovingSubRules::StillMap(Sub->GetHomeTransform(), State->SubOffsetM, State->SubDepthM, State->SubYawDeg, DatumCm());
	}

	/** Where D-022 puts the arena root for the game state: by the depth below the loaded stop, the offset and the yaw, round the hull at Home. */
	static FTransform MovingRootTransform(const ADeepGameState& State, const FTransform& Home)
	{
		const float OffsetM = State.SubDepthM - UDeepTuningSettings::Get().GetStop(State.Stop).SubDepthM;
		FQuat Turn;
		const FVector At = UDeepPilotRules::HangLocationCm(Home.GetLocation(), Home.GetRotation(), State.SubOffsetM, OffsetM, State.SubYawDeg, Turn);
		return FTransform(Turn, At);
	}

	/** The anchor where D-022 has it now: the hull's anchor shifted by the root's offset from the hull. */
	static FVector MovingAnchor(const UWorld* World)
	{
		const ADeepSubmarine* Sub = FindSubmarine(World);
		if (!Sub)
		{
			return FVector::ZeroVector;
		}
		const ADeepGameState* State = World->GetGameState<ADeepGameState>();
		if (State && State->bWorldStill)
		{
			// The anchor hangs straight down from the hull, so its offset is the same wherever the hull is.
			return MovingRootTransform(*State, Sub->GetHomeTransform()).GetLocation() + (Sub->GetAnchorLocation() - Sub->GetActorLocation());
		}
		const ADeepArenaRoot* Root = FindArenaRoot(World);
		return Sub->GetAnchorLocation() + (Root ? Root->GetActorLocation() - Sub->GetActorLocation() : FVector::ZeroVector);
	}

	FVector ArenaAnchor(const UWorld* World)
	{
		if (!IsWorldStill(World))
		{
			return MovingAnchor(World);
		}
		return StillMapNow(World).TransformPosition(MovingAnchor(World));
	}

	/** Where a piece OffsetCm from the arena's anchor goes: as D-022 places it, carried into the still world by the map. */
	static FTransform ArenaPlace(const UWorld* World, const FVector& OffsetCm)
	{
		if (!IsWorldStill(World))
		{
			return FTransform(FQuat::Identity, MovingAnchor(World) + OffsetCm);
		}
		const FTransform Map = StillMapNow(World);
		return FTransform(Map.GetRotation(), Map.TransformPosition(MovingAnchor(World) + OffsetCm));
	}

	/** Deep.World.Still: a stop's anchor, where its bed and cave are built (as D-022 builds them parked at that stop). */
	static FVector StillAnchor(const UWorld* World, const ADeepSubmarine& Sub, int32 Stop)
	{
		return StillStopFrame(World, Stop).GetLocation() + (Sub.GetAnchorLocation() - Sub.GetActorLocation());
	}

	static void HangFromRoot(UWorld* World, AActor* Actor)
	{
		if (ADeepArenaRoot* Root = Actor ? FindArenaRoot(World) : nullptr)
		{
			Actor->AttachToActor(Root, FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	static ADeepSalvage* SpawnSalvageAt(UWorld* World, const FDeepSalvageKind& Kind, const FTransform& Where)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			return nullptr;
		}
		ADeepSalvage* Salvage = World->SpawnActor<ADeepSalvage>(Where.GetLocation(), Where.Rotator(), SpawnParams());
		if (Salvage)
		{
			Salvage->SetKind(Kind);
			HangFromRoot(World, Salvage);
		}
		return Salvage;
	}

	ADeepSalvage* SpawnSalvage(UWorld* World, const FDeepSalvageKind& Kind, const FVector& LocationCm)
	{
		return SpawnSalvageAt(World, Kind, FTransform(FQuat::Identity, LocationCm));
	}

	static int32 SpawnSalvageRing(UWorld* World, const ADeepSubmarine* Sub)
	{
		const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
		int32 Spawned = 0;
		for (const FDeepSalvageSpot& Spot : Tuning.Arena.SalvageSpots)
		{
			FDeepSalvageKind Kind;
			if (!Tuning.FindSalvageKind(Spot.Size, Kind))
			{
				UE_LOG(LogDeepArena, Warning, TEXT("No salvage kind for size %d; spot skipped"), static_cast<int32>(Spot.Size));
				continue;
			}
			if (SpawnSalvageAt(World, Kind, ArenaPlace(World, UDeepRules::SalvageSpotOffsetCm(Spot))))
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
		Relic = SpawnSalvageAt(World, Kind, ArenaPlace(World, Offset));
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
		if (ADeepArenaRoot* Root = FindArenaRoot(World))
		{
			Root->StillContentStop = CurrentStop(World);
		}
		return Spawned + (EnsureRelic(World, CurrentStop(World)) ? 1 : 0);
	}

	/** Deep.World.Still (server): salvage placed for one stop moves to the loaded stop's frame (D-022: it rides the re-based root). */
	static void CarryStillContent(UWorld* World, int32 Stop)
	{
		ADeepArenaRoot* Root = FindArenaRoot(World);
		if (!Root || !IsWorldStill(World) || World->GetNetMode() == NM_Client)
		{
			return;
		}
		const int32 Was = Root->StillContentStop;
		Root->StillContentStop = Stop;
		if (Was <= 0 || Was == Stop)
		{
			return;
		}
		const FVector Shift = StillStopFrame(World, Stop).GetLocation() - StillStopFrame(World, Was).GetLocation();
		for (TActorIterator<ADeepSalvage> It(World); It; ++It)
		{
			if (IsValid(*It) && It->GetAttachParentActor() == Root)
			{
				It->AddActorWorldOffset(Shift);
			}
		}
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
		// Deep.World.Still is read once, here, and fixed for the world's life; the hull's spot now is its home.
		ADeepGameState* State = World->GetGameState<ADeepGameState>();
		if (State && !State->bWorldStillLatched)
		{
			State->bWorldStill = CVarDeepWorldStill.GetValueOnGameThread() != 0;
			State->bWorldStillLatched = true;
			UE_LOG(LogDeepArena, Log, TEXT("World %s: %s"), *World->GetName(), State->bWorldStill ? TEXT("still (the hull moves)") : TEXT("D-022 (the arena root moves)"));
		}
		const bool bStill = IsWorldStill(World);
		if (!bStill || !FindArenaRoot(World))
		{
			// Still: the hull is about to leave its spot, so its home is kept now (before any root or bed is placed from it).
			Sub->SetHome(Sub->GetHomeTransform());
		}
		EnsureRoom(World);
		if (!FindArenaRoot(World))
		{
			// Still: the root stays at identity for good.
			World->SpawnActor<ADeepArenaRoot>(bStill ? FVector::ZeroVector : Sub->GetActorLocation(), FRotator::ZeroRotator, SpawnParams());
		}

		if (!FindFirst<ADeepSalvage>(World))
		{
			SpawnSalvageRing(World, Sub);
			if (ADeepArenaRoot* Root = FindArenaRoot(World))
			{
				Root->StillContentStop = CurrentStop(World);
			}
		}

		if (!FindFirst<ADeepAbyssFloor>(World))
		{
			if (bStill)
			{
				// Still: every stop has its own bed at its own depth, shown while the hull is at or above it (UpdateStillWorld).
				for (int32 BedStop = 1; BedStop <= FMath::Max(1, Tuning.NumStops()); ++BedStop)
				{
					if (ADeepAbyssFloor* Floor = World->SpawnActor<ADeepAbyssFloor>(StillAnchor(World, *Sub, BedStop), FRotator::ZeroRotator, SpawnParams()))
					{
						Floor->BedStop = BedStop;
						HangFromRoot(World, Floor);
					}
				}
			}
			// The sea bed is built around the anchor, through every salvage spot's resting point (ADeepAbyssFloor).
			else if (ADeepAbyssFloor* Floor = World->SpawnActor<ADeepAbyssFloor>(ArenaAnchor(World), FRotator::ZeroRotator, SpawnParams()))
			{
				HangFromRoot(World, Floor);
			}
		}

		if (!FindFirst<ADeepCavern>(World))
		{
			// Rock walls and a roof round the arena, built round the same anchor and hung the same way (ADeepCavern).
			// Still: it stands at stop 1's anchor and lays each stop's shell at that stop's depth (ADeepCavern::UpdateShellsStill).
			const FVector CaveAt = bStill ? StillAnchor(World, *Sub, 1) : ArenaAnchor(World);
			if (ADeepCavern* Cavern = World->SpawnActor<ADeepCavern>(CaveAt, FRotator::ZeroRotator, SpawnParams()))
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

	ADeepAbyssFloor* FindLoadedFloor(const UWorld* World)
	{
		ADeepAbyssFloor* First = FindFirst<ADeepAbyssFloor>(World);
		const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
		if (!State || !State->bWorldStill)
		{
			return First;
		}
		for (TActorIterator<ADeepAbyssFloor> It(const_cast<UWorld*>(World)); It; ++It)
		{
			if (IsValid(*It) && It->BedStop == State->Stop)
			{
				return *It;
			}
		}
		return First;
	}

	void UpdateStillWorld(UWorld* World)
	{
		const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
		if (!State || !State->bWorldStill)
		{
			return;
		}
		const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
		for (TActorIterator<ADeepAbyssFloor> It(World); It; ++It)
		{
			// A stop's bed shows, and blocks, while the hull is parked there or still above it: never where it passes through.
			const bool bShow = UDeepMovingSubRules::StillBedShows(State->SubDepthM, Tuning.GetStop(It->BedStop).SubDepthM, Tuning.MovingSub);
			if (It->IsHidden() == bShow)
			{
				It->SetActorHiddenInGame(!bShow);
			}
			if (It->GetActorEnableCollision() != bShow)
			{
				It->SetActorEnableCollision(bShow);
			}
		}
		if (ADeepCavern* Cavern = FindFirst<ADeepCavern>(World))
		{
			Cavern->UpdateShellsStill(State->SubDepthM);
		}
		if (ADeepShaft* Shaft = FindFirst<ADeepShaft>(World))
		{
			Shaft->Follow();
		}
		if (ADeepEnvironment* Environment = FindFirst<ADeepEnvironment>(World))
		{
			Environment->FollowStillHull();
		}
	}

	static void PositionRoot(UWorld* World)
	{
		const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
		ADeepSubmarine* Sub = FindSubmarine(World);
		ADeepArenaRoot* Root = FindArenaRoot(World);
		if (!State || !Sub || !Root)
		{
			return;
		}
		if (State->bWorldStill)
		{
			// Still: the world stays put and the hull takes the inverse of where D-022 would have put the root.
			if (!Root->GetActorTransform().Equals(FTransform::Identity, 0.001))
			{
				Root->SetActorTransform(FTransform::Identity);
			}
			Sub->PlaceStill();
			UpdateStillWorld(World);
			return;
		}
		// Deeper than the loaded stop: the world is above us, so the root goes up. Steered to starboard in the cave
		// shaft: the world slides to port (SubOffsetM, in the sub's frame).
		const float OffsetM = State->SubDepthM - UDeepTuningSettings::Get().GetStop(State->Stop).SubDepthM;
		// Turned by the helm's rudder: the world turns the other way about the vertical through the sub (identity at yaw 0).
		FQuat Turn;
		const FVector At = UDeepPilotRules::HangLocationCm(Sub->GetActorLocation(), Sub->GetActorQuat(), State->SubOffsetM, OffsetM, State->SubYawDeg, Turn);
		Root->SetActorLocationAndRotation(At, Turn);
		if (ADeepShaft* Shaft = FindFirst<ADeepShaft>(World))
		{
			Shaft->Follow();
		}
	}

	void MoveRoot(UWorld* World)
	{
		if (World && World->GetNetMode() != NM_Client)
		{
			PositionRoot(World);
		}
	}

	void ApplyStop(UWorld* World, int32 Stop)
	{
		if (ADeepEnvironment* Environment = World ? FindFirst<ADeepEnvironment>(World) : nullptr)
		{
			Environment->SetStop(Stop);
		}
		PositionRoot(World);
		// Still: salvage placed for another stop moves to this stop's frame first, so the relic below is not carried.
		CarryStillContent(World, Stop);
		EnsureRelic(World, Stop);
		// The root may have re-based: dressing placed by absolute depth follows in the same frame.
		if (ADeepDressing* Dressing = World ? FindFirst<ADeepDressing>(World) : nullptr)
		{
			Dressing->Refresh();
		}
		if (ADeepSubmarine* Sub = FindSubmarine(World))
		{
			const ADeepGameState* State = World->GetGameState<ADeepGameState>();
			// Parked off the stop's centre (the cave shaft), the sub glides home first; the hatch opens once it is there.
			Sub->SetHatchOpen(!State || State->IsSettledAtStop());
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
