#include "World/DeepArena.h"

#include "Sim/DeepPilotRules.h"

#include "Engine/World.h"
#include "Game/DeepGameState.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Sim/DeepCaveDiveRules.h"
#include "Sim/DeepMovingSubRules.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepAbyssFloor.h"
#include "World/DeepArenaRoot.h"
#include "World/DeepCaveString.h"
#include "World/DeepCavern.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "World/DeepDressing.h"
#include "World/DeepEnvironment.h"
#include "World/DeepSalvage.h"
#include "World/DeepShaft.h"
#include "World/DeepSubmarine.h"
#include "World/DeepSubRoom.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepArena, Log, All);

static TAutoConsoleVariable<int32> CVarDeepWorldStill(
	TEXT("Deep.World.Still"),
	1,
	TEXT("1 (default since moving-sub step 4): the arena root stays still and the hull moves through it; 0: D-022, the hull fixed and the root moved. Read when the arena is built, fixed for the world's life."),
	ECVF_Default);

namespace DeepArena
{
	static void RestartForCaves(IConsoleVariable* Var);
}

static TAutoConsoleVariable<int32> CVarDeepWorldCaves(
	TEXT("Deep.World.Caves"),
	0,
	TEXT("1: the dive is the baked cave chunks (World/DeepCaveString; needs Deep.World.Still 1): the run starts parked in S and the helm drives the string. 0 (default): the shaft and the code-built caverns. Latched when the arena is built; set in a game, it restarts the level."),
	FConsoleVariableDelegate::CreateStatic(&DeepArena::RestartForCaves),
	ECVF_Default);

namespace DeepArena
{
	/**
	 * Deep.World.Caves changed in a running game (the console, -ExecCmds, a shot's commands): the arena latched the old
	 * value, so the level restarts and the new arena latches the new one. The server or a lone game only; next tick, not
	 * inside the console call; never in automation tests (they pin the flag before building their world).
	 */
	static void RestartForCaves(IConsoleVariable* Var)
	{
		if (GIsAutomationTesting || !GEngine || !Var)
		{
			return;
		}
		const bool bCaves = Var->GetInt() != 0;
		FTSTicker::GetCoreTicker().AddTicker(TEXT("DeepWorldCavesRestart"), 0.f, [bCaves](float)
		{
			if (!GEngine)
			{
				return false;
			}
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* World = Context.World();
				const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
				if (!World || !(Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE) || !State
					|| !State->bWorldStillLatched || State->bWorldCaves == bCaves || World->GetNetMode() == NM_Client)
				{
					continue;
				}
				UE_LOG(LogDeepArena, Display, TEXT("Deep.World.Caves %d: restarting %s so the arena latches it"), bCaves ? 1 : 0, *World->GetName());
				if (World->GetNetMode() == NM_Standalone)
				{
					if (APlayerController* PC = World->GetFirstPlayerController())
					{
						PC->RestartLevel();
					}
				}
				else
				{
					World->ServerTravel(TEXT("?restart"));
				}
			}
			return false;
		});
	}

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

	bool IsWorldCaves(const UWorld* World)
	{
		const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
		return State && State->bWorldStill && State->bWorldCaves;
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
		// Deep.World.Caves: the game state's depth is the gauge's; the hull is placed at the true depth (D-065).
		return UDeepMovingSubRules::StillMap(Sub->GetHomeTransform(), State->SubOffsetM, DeepCaveString::PhysicalDepthM(World, State->SubDepthM), State->SubYawDeg, DatumCm());
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
		if (IsWorldCaves(World))
		{
			// The caves place the hull where it is (ADeepSubmarine::PlaceStill): its own anchor is the arena's.
			const ADeepSubmarine* Sub = FindSubmarine(World);
			return Sub ? Sub->GetAnchorLocation() : FVector::ZeroVector;
		}
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

	/** A spawned piece's visible and grab shapes' bounds (world), so it can rest on rock by its own bottom. */
	static FBox SalvageBounds(const ADeepSalvage& Salvage)
	{
		FBox Box(ForceInit);
		Salvage.ForEachComponent<UPrimitiveComponent>(false, [&Box](const UPrimitiveComponent* Prim)
		{
			if (Prim->IsRegistered() && (Prim->IsVisible() || Prim->IsCollisionEnabled()))
			{
				Box += Prim->Bounds.GetBox();
			}
		});
		return Box;
	}

	bool SalvageInRock(const ADeepCaveString& String, const ADeepSalvage& Salvage, double LiftCm)
	{
		bool bIn = false;
		Salvage.ForEachComponent<UPrimitiveComponent>(false, [&](const UPrimitiveComponent* Prim)
		{
			if (bIn || !Prim->IsRegistered() || !(Prim->IsVisible() || Prim->IsCollisionEnabled()))
			{
				return;
			}
			const FTransform& ToWorld = Prim->GetComponentTransform();
			const FBox Local = Prim->CalcBounds(FTransform::Identity).GetBox();
			if (!Local.IsValid)
			{
				return;
			}
			const FVector Centre = ToWorld.TransformPosition(Local.GetCenter()) + FVector(0.0, 0.0, LiftCm);
			bIn = String.OverlapsBoxWorld(Centre, Local.GetExtent() * ToWorld.GetScale3D().GetAbs(), ToWorld.GetRotation());
		});
		return bIn;
	}

	bool RestCaveSalvage(const ADeepCaveString& String, ADeepSalvage& Salvage, const FVector& AtCm, const TCHAR* What)
	{
		// Rests on the highest rock under its footprint (so a slope does not swallow its uphill side), a little sunk.
		const FDeepCaveStringTuning& CS = UDeepTuningSettings::Get().CaveString;
		Salvage.SetActorLocation(AtCm);
		const double Search = FMath::Max(0.f, CS.SalvageRestSearchCm);
		const FBox Box = SalvageBounds(Salvage);
		double Highest = 0.0;
		double Lowest = 0.0;
		if (!Box.IsValid || String.FootprintRock(Box, CS.SalvageRestUpCm, float(Search), Highest, Lowest) <= 0)
		{
			UE_LOG(LogDeepArena, Verbose, TEXT("Deep.World.Caves: no rock within %.0f cm under %s"), Search, What);
			return false;
		}
		// Its bounds' bottom on the footprint's highest rock; then by its own shapes (smaller than its bounds): raised
		// a step at a time off a wall or a steeper slope, or lowered until the next step down would touch. Then sunk.
		const double Bottom = AtCm.Z - Box.Min.Z;
		Salvage.SetActorLocation(FVector(AtCm.X, AtCm.Y, Highest + Bottom));
		const double Step = FMath::Max(0.5f, CS.SalvageLiftStepCm);
		double Lifted = 0.0;
		while (SalvageInRock(String, Salvage, 0.0) && Lifted + Step <= CS.SalvageLiftMaxCm)
		{
			Lifted += Step;
			Salvage.AddActorWorldOffset(FVector(0.0, 0.0, Step));
		}
		if (SalvageInRock(String, Salvage, 0.0))
		{
			UE_LOG(LogDeepArena, Verbose, TEXT("Deep.World.Caves: %s still touches rock %.0f cm up"), What, Lifted);
			return false;
		}
		for (double Lowered = 0.0; Lifted <= 0.0 && Lowered + Step <= Search && !SalvageInRock(String, Salvage, -Step); Lowered += Step)
		{
			Salvage.AddActorWorldOffset(FVector(0.0, 0.0, -Step));
		}
		Salvage.AddActorWorldOffset(FVector(0.0, 0.0, -CS.SalvageSinkCm));
		return true;
	}

	int32 SpawnCaveSalvage(UWorld* World, int32 Stop)
	{
		const ADeepCaveString* String = DeepCaveString::Find(World);
		if (!String || !World || World->GetNetMode() == NM_Client)
		{
			return 0;
		}
		// Deep.World.Caves (D-067): a stop's salvage lies in piles at its berth cavern's sockets (CaveString.Salvage). Each
		// piece tries the spots round its socket in turn (DeepCaveDiveRules::PileSpotsCm) and takes the first where it
		// rests on rock, clear of it and of the pile's other pieces, within SalvagePileReachCm of the socket.
		const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
		const FDeepCaveStringTuning& CS = Tuning.CaveString;
		if (!CS.Berths.IsValidIndex(Stop - 1))
		{
			return 0;
		}
		const FName Cavern = CS.Berths[Stop - 1];
		const double Reach = FMath::Max(0.f, CS.SalvagePileReachCm);
		int32 Spawned = 0;
		for (const FDeepCaveSalvageSlot& Slot : CS.Salvage)
		{
			if (Slot.Chunk != Cavern)
			{
				continue;
			}
			FDeepCavePose Pose;
			FVector Socket;
			if (!String->SocketWorld(Slot.Chunk, Slot.Socket, Socket) || !String->SocketPose(Slot.Chunk, Slot.Socket, Pose))
			{
				UE_LOG(LogDeepArena, Warning, TEXT("Deep.World.Caves: no salvage at %s.%s (no socket)"), *Slot.Chunk.ToString(), *Slot.Socket.ToString());
				continue;
			}
			const FString PileName = FString::Printf(TEXT("%s.%s"), *Slot.Chunk.ToString(), *Slot.Socket.ToString());
			const float YawDeg = Pose.YawDeg + String->GetActorRotation().Yaw;
			TArray<FBox> Laid;
			for (int32 Index = 0; Index < Slot.Pile.Num(); ++Index)
			{
				FDeepSalvageKind Kind;
				if (!Tuning.FindSalvageKind(Slot.Pile[Index], Kind))
				{
					UE_LOG(LogDeepArena, Warning, TEXT("Deep.World.Caves: no salvage kind %d for %s"), static_cast<int32>(Slot.Pile[Index]), *PileName);
					continue;
				}
				ADeepSalvage* Salvage = SpawnSalvageAt(World, Kind, FTransform(FQuat::Identity, Socket));
				if (!Salvage)
				{
					continue;
				}
				const FString What = FString::Printf(TEXT("%s piece %d"), *PileName, Index + 1);
				const TArray<FVector2D> Spots = DeepCaveDiveRules::PileSpotsCm(Index, Slot.Pile.Num(), CS.SalvagePileSpreadCm, CS.SalvagePileStepCm, float(Reach), YawDeg);
				bool bPlaced = false;
				for (const FVector2D& Spot : Spots)
				{
					if (!RestCaveSalvage(*String, *Salvage, Socket + FVector(Spot, 0.0), *What))
					{
						continue;
					}
					const FVector At = Salvage->GetActorLocation();
					const FBox Box = SalvageBounds(*Salvage).ExpandBy(0.5 * CS.SalvagePileGapCm);
					const bool bNear = FVector::Dist2D(At, Socket) <= Reach && FMath::Abs(At.Z - Socket.Z) <= Reach;
					const bool bApart = !Laid.ContainsByPredicate([&Box](const FBox& Other) { return Other.Intersect(Box); });
					if (bNear && bApart)
					{
						Laid.Add(Box);
						bPlaced = true;
						break;
					}
				}
				if (!bPlaced)
				{
					UE_LOG(LogDeepArena, Warning, TEXT("Deep.World.Caves: no clear spot for %s within %.0f cm; it lies at the socket"), *What, Reach);
					Salvage->SetActorLocation(Socket);
				}
				++Spawned;
			}
		}
		UE_LOG(LogDeepArena, Log, TEXT("Spawned %d salvage pieces in %s"), Spawned, *Cavern.ToString());
		return Spawned;
	}

	static int32 SpawnSalvageRing(UWorld* World, const ADeepSubmarine* Sub)
	{
		const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
		int32 Spawned = 0;
		if (IsWorldCaves(World))
		{
			// The caves' salvage sits at the berth cavern's sockets; the ring is for the open stops.
			const ADeepGameState* State = World->GetGameState<ADeepGameState>();
			return SpawnCaveSalvage(World, State ? State->Stop : 1);
		}
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

	int32 RelicStop(const UWorld* World)
	{
		const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
		return DeepCaveDiveRules::RelicStop(IsWorldCaves(World), Tuning.Arena.RelicStop, Tuning.CaveString.Berths.Num());
	}

	int32 ShownStops(const UWorld* World)
	{
		const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
		return DeepCaveDiveRules::ShownStops(IsWorldCaves(World), Tuning.NumStops(), Tuning.CaveString.Berths.Num());
	}

	TArray<float> ShownStopDepthsM(const UWorld* World)
	{
		const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
		TArray<float> Out;
		for (int32 Stop = 1; Stop <= ShownStops(World); ++Stop)
		{
			Out.Add(Tuning.GetStop(Stop).SubDepthM);
		}
		return Out;
	}

	ADeepSalvage* EnsureRelic(UWorld* World, int32 Stop)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			return nullptr;
		}
		const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
		ADeepSalvage* Relic = FindRelic(World);
		if (Stop < RelicStop(World))
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
		if (IsWorldCaves(World))
		{
			// D-068: in the caves it waits on the floor of its cavern at its own socket, resting on the rock like salvage.
			const ADeepCaveString* String = DeepCaveString::Find(World);
			const FDeepCaveStringTuning& CS = Tuning.CaveString;
			const FName Chunk = CS.Berths.Num() > 0 ? CS.Berths.Last() : NAME_None;
			FVector At;
			if (!String || !String->SocketWorld(Chunk, CS.RelicSocket, At))
			{
				UE_LOG(LogDeepArena, Warning, TEXT("Deep.World.Caves: no relic (no socket %s.%s)"), *Chunk.ToString(), *CS.RelicSocket.ToString());
				return nullptr;
			}
			Relic = SpawnSalvageAt(World, Kind, FTransform(FQuat::Identity, At));
			if (!Relic)
			{
				return nullptr;
			}
			// At the socket if it rests clear there, else the nearest spot round it that does (as a pile of one).
			bool bPlaced = false;
			for (const FVector2D& Spot : DeepCaveDiveRules::PileSpotsCm(0, 1, 0.f, CS.SalvagePileStepCm, CS.SalvagePileReachCm, 0.f))
			{
				if (RestCaveSalvage(*String, *Relic, At + FVector(Spot, 0.0), TEXT("the relic")))
				{
					bPlaced = true;
					break;
				}
			}
			if (!bPlaced)
			{
				UE_LOG(LogDeepArena, Warning, TEXT("Deep.World.Caves: the relic found no clear rock within %.0f cm of %s.%s; it lies at the socket"), CS.SalvagePileReachCm, *Chunk.ToString(), *CS.RelicSocket.ToString());
				Relic->SetActorLocation(At);
			}
			UE_LOG(LogDeepArena, Log, TEXT("Relic placed at %s.%s at stop %d, %.0f cm across from the socket"), *Chunk.ToString(), *CS.RelicSocket.ToString(), Stop, FVector::Dist2D(Relic->GetActorLocation(), At));
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
		// The caves' salvage is placed at its cavern's rock, which does not move: nothing to carry.
		if (Was <= 0 || Was == Stop || IsWorldCaves(World))
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
			const bool bCavesAsked = CVarDeepWorldCaves.GetValueOnGameThread() != 0;
			State->bWorldCaves = bCavesAsked && State->bWorldStill;
			if (bCavesAsked && !State->bWorldStill)
			{
				UE_LOG(LogDeepArena, Warning, TEXT("Deep.World.Caves needs Deep.World.Still 1: the shaft it is"));
			}
			UE_LOG(LogDeepArena, Log, TEXT("World %s: %s%s"), *World->GetName(), State->bWorldStill ? TEXT("still (the hull moves)") : TEXT("D-022 (the arena root moves)"),
				State->bWorldCaves ? TEXT(", the cave string") : TEXT(""));
		}
		const bool bStill = IsWorldStill(World);
		const bool bCaves = IsWorldCaves(World);
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

		// Deep.World.Caves: the chunks are the rock, the floors and the caverns (no beds, shells or dressing). Built
		// before the salvage, which rests on their rock.
		if (bCaves)
		{
			DeepCaveString::Ensure(World);
		}

		if (!FindFirst<ADeepSalvage>(World))
		{
			SpawnSalvageRing(World, Sub);
			if (ADeepArenaRoot* Root = FindArenaRoot(World))
			{
				Root->StillContentStop = CurrentStop(World);
			}
		}
		if (!bCaves && !FindFirst<ADeepAbyssFloor>(World))
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

		if (!bCaves && !FindFirst<ADeepCavern>(World))
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
		if (!bCaves && !FindFirst<ADeepDressing>(World))
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
		if (State->bWorldCaves)
		{
			// Every machine builds its own string once the hull's home has replicated.
			DeepCaveString::Ensure(World);
		}
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
			// The hull is placed now, so the server's sim reads it this step. The beds, shells, shaft and water follow in
			// the sub's own tick (ADeepSubmarine::Tick -> UpdateStillWorld), once a frame on every machine.
			Sub->PlaceStill();
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
		// Still: the beds, shells and shaft show for the new depth and stop at once, not a frame late (no-op with the flag off).
		UpdateStillWorld(World);
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
