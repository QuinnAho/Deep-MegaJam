#include "World/DeepCaveString.h"

#include "Camera/CameraActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#if WITH_EDITOR
#include "StaticMeshCompiler.h"
#endif
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Debug/DeepStateReport.h"
#include "Game/DeepGameState.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepCaptainLines.h"
#include "World/DeepArena.h"
#include "World/DeepShaft.h"
#include "World/DeepSubmarine.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepCaveString, Log, All);

namespace DeepCaveString
{
	ADeepCaveString* Find(const UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		for (TActorIterator<ADeepCaveString> It(const_cast<UWorld*>(World)); It; ++It)
		{
			if (IsValid(*It))
			{
				return *It;
			}
		}
		return nullptr;
	}

	ADeepCaveString* Ensure(UWorld* World)
	{
		if (!World || !DeepArena::IsWorldCaves(World))
		{
			return nullptr;
		}
		// The analytic shaft's rock is not in a cave world (every machine spawns one with its sub).
		for (TActorIterator<ADeepShaft> It(World); It; ++It)
		{
			It->Destroy();
		}
		if (ADeepCaveString* Found = Find(World))
		{
			return Found;
		}
		const ADeepSubmarine* Sub = DeepArena::FindSubmarine(World);
		if (!Sub || !Sub->HasHome())
		{
			return nullptr;
		}
		// S's berth is the string's origin: stop 1's still frame, turned by the hull's home heading (StillHull's frame).
		const FTransform Home = Sub->GetHomeTransform();
		const FVector At = DeepArena::StillStopFrame(World, 1).GetLocation();
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		ADeepCaveString* String = World->SpawnActor<ADeepCaveString>(At, FRotator(0.f, Home.Rotator().Yaw, 0.f), Params);
		if (!String)
		{
			return nullptr;
		}
		FString Error;
		if (!String->Build(Error))
		{
			UE_LOG(LogDeepCaveString, Error, TEXT("Deep.World.Caves: the cave string did not build: %s"), *Error);
		}
		return String;
	}

	FDeepCavePose HullPose(const ADeepGameState& State)
	{
		const TArray<float> Stops = UDeepTuningSettings::Get().StopDepthsM();
		FDeepCavePose Pose;
		Pose.LocationCm = UDeepCaveStringRules::HullCm(State.SubOffsetM, PhysicalDepthM(State.GetWorld(), State.SubDepthM), Stops.IsEmpty() ? 0.f : Stops[0]);
		Pose.YawDeg = State.SubYawDeg;
		return Pose;
	}

	float PhysicalDepthM(const UWorld* World, float ReadM)
	{
		const ADeepCaveString* String = DeepArena::IsWorldCaves(World) ? Find(World) : nullptr;
		return String ? UDeepCaveStringRules::PhysicalDepthM(String->GetDepthMap(), ReadM) : ReadM;
	}

	float ReadDepthM(const UWorld* World, float PhysicalM)
	{
		const ADeepCaveString* String = DeepArena::IsWorldCaves(World) ? Find(World) : nullptr;
		return String ? UDeepCaveStringRules::ReadDepthM(String->GetDepthMap(), PhysicalM) : PhysicalM;
	}
}

ADeepCaveString::ADeepCaveString()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	Frame = CreateDefaultSubobject<USceneComponent>(TEXT("Frame"));
	RootComponent = Frame;
}

bool ADeepCaveString::Build(FString& OutError)
{
	const FDeepCaveStringTuning& T = UDeepTuningSettings::Get().CaveString;
	auto Load = [&OutError](const FString& File, FString& OutText)
	{
		if (!FFileHelper::LoadFileToString(OutText, *DeepData::DataPath(File)))
		{
			OutError = FString::Printf(TEXT("cannot read %s"), *DeepData::DataPath(File));
			return false;
		}
		return true;
	};
	FString ChunksText, SocketsText, PathText;
	TArray<FDeepCaveChunkRow> Rows;
	if (!Load(T.ChunksCsv, ChunksText) || !Load(T.SocketsCsv, SocketsText)
		|| !UDeepCaveStringRules::ParseChunks(ChunksText, Rows, OutError)
		|| !UDeepCaveStringRules::ParseSockets(SocketsText, Sockets, OutError))
	{
		return false;
	}
	const FName Start = T.Berths.IsEmpty() ? (T.String.IsEmpty() ? NAME_None : T.String[0]) : T.Berths[0];
	const FDeepCaveSocketRow* Berth = UDeepCaveStringRules::FindSocket(Sockets, Start, T.ParkSocket);
	if (!Berth)
	{
		OutError = FString::Printf(TEXT("no %s socket in %s"), *T.ParkSocket.ToString(), *Start.ToString());
		return false;
	}
	if (!UDeepCaveStringRules::ComposeString(Rows, T.String, *Berth, Places, OutError))
	{
		return false;
	}
	// D-065: the gauge reads the stops' depths at the berths. The table is what plays; the string says what it should be.
	FString MapText;
	if (!Load(T.DepthMapCsv, MapText) || !UDeepCaveStringRules::ParseDepthMap(MapText, DepthMap, OutError))
	{
		OutError = FString::Printf(TEXT("%s: %s"), *T.DepthMapCsv, *OutError);
		DepthMap.Reset();
		return false;
	}
	TArray<FDeepCaveDepthKnot> Expected;
	FString MapError;
	if (UDeepCaveStringRules::StringDepthMap(Places, Sockets, T.Berths, T.ParkSocket, UDeepTuningSettings::Get().StopDepthsM(), Expected, MapError))
	{
		for (int32 Index = 0; Index < FMath::Max(Expected.Num(), DepthMap.Num()); ++Index)
		{
			const bool bBoth = Expected.IsValidIndex(Index) && DepthMap.IsValidIndex(Index);
			if (!bBoth || !FMath::IsNearlyEqual(Expected[Index].PhysicalM, DepthMap[Index].PhysicalM, 0.05f) || !FMath::IsNearlyEqual(Expected[Index].ReadM, DepthMap[Index].ReadM, 0.05f))
			{
				UE_LOG(LogDeepCaveString, Warning, TEXT("Deep.World.Caves: %s differs from the string at knot %d: re-derive it (UDeepCaveStringRules::StringDepthMap)"), *T.DepthMapCsv, Index);
				break;
			}
		}
	}
	// The path is for cheats and tests: the dive plays without it.
	FString PathError;
	if (Load(T.PathCsv, PathText) && UDeepCaveStringRules::ParsePath(PathText, Path, PathError))
	{
		PathPoses = UDeepCaveStringRules::PathPoses(Places, Path);
		PathMetres = UDeepCaveStringRules::PathMetres(PathPoses);
	}
	else
	{
		UE_LOG(LogDeepCaveString, Warning, TEXT("Deep.World.Caves: no dive path (%s)"), *PathError);
	}

	for (const FDeepCavePlace& Place : Places)
	{
		const FDeepCaveChunkRow* Row = Rows.FindByPredicate([&Place](const FDeepCaveChunkRow& R) { return R.Name == Place.Chunk; });
		UStaticMesh* Mesh = Row ? LoadObject<UStaticMesh>(nullptr, *Row->Mesh, nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
		if (!Mesh)
		{
			OutError = FString::Printf(TEXT("no mesh for chunk %s (%s)"), *Place.Chunk.ToString(), Row ? *Row->Mesh : TEXT("no row"));
			return false;
		}
#if WITH_EDITOR
		// The editor builds a freshly loaded mesh on a worker: wait, or the chunk has no collision until it finishes (the
		// salvage resting on it and the diver's first sweeps would find no rock).
		FStaticMeshCompilingManager::Get().FinishCompilation({ Mesh });
#endif
		UStaticMeshComponent* Chunk = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("Chunk_%s"), *Place.Chunk.ToString()), RF_Transient);
		Chunk->SetupAttachment(Frame);
		Chunk->SetStaticMesh(Mesh);
		Chunk->SetRelativeLocationAndRotation(Place.LocationCm, FRotator(0.f, Place.YawDeg, 0.f));
		// Rock: blocks pawns and Visibility (the sonar sees it), like the shaft's tubes.
		Chunk->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Chunk->RegisterComponent();
		Chunks.Add(Chunk);
	}
	UE_LOG(LogDeepCaveString, Log, TEXT("Deep.World.Caves: %d chunks strung, %d path samples (%.0f m)"), Chunks.Num(), Path.Num(), PathMetres.IsEmpty() ? 0.f : PathMetres.Last());
	return true;
}

bool ADeepCaveString::SocketPose(FName Chunk, FName Socket, FDeepCavePose& OutPose) const
{
	const FDeepCavePlace* Place = UDeepCaveStringRules::FindPlace(Places, Chunk);
	const FDeepCaveSocketRow* Row = UDeepCaveStringRules::FindSocket(Sockets, Chunk, Socket);
	if (!Place || !Row)
	{
		return false;
	}
	OutPose = UDeepCaveStringRules::ToString(*Place, Row->LocalCm, Row->YawDeg);
	return true;
}

FTransform ADeepCaveString::BoxWorld(const FBox& Box, const FVector& AtCm, float YawDeg) const
{
	const FTransform Hull(FRotator(0.f, YawDeg, 0.f), AtCm);
	const FTransform Local(FQuat::Identity, Box.GetCenter());
	return Local * Hull * GetActorTransform();
}

FDeepCaveSweep ADeepCaveString::SweepHull(const FBox& Box, const FVector& FromCm, const FVector& ToCm, float YawDeg) const
{
	FDeepCaveSweep Result;
	if (!Box.IsValid)
	{
		return Result;
	}
	const FTransform Start = BoxWorld(Box, FromCm, YawDeg);
	const FTransform End = BoxWorld(Box, ToCm, YawDeg);
	const FCollisionShape Shape = FCollisionShape::MakeBox(Box.GetExtent());
	// A sweep needs a length: nudge a still one (as cavestring does), so a box at rest still finds rock it sits in.
	FVector EndAt = End.GetLocation();
	if (EndAt.Equals(Start.GetLocation(), 0.01))
	{
		EndAt += Start.GetRotation().GetForwardVector() * 0.5;
	}
	float Best = TNumericLimits<float>::Max();
	for (UStaticMeshComponent* Chunk : Chunks)
	{
		FHitResult Hit;
		if (!Chunk || !Chunk->SweepComponent(Hit, Start.GetLocation(), EndAt, Start.GetRotation(), Shape, true))
		{
			continue;
		}
		const float Key = Hit.bStartPenetrating ? -1.f - Hit.PenetrationDepth : Hit.Time;
		if (Key < Best)
		{
			Best = Key;
			Result.bHit = true;
			Result.bStartPenetrating = Hit.bStartPenetrating;
			Result.Time = Hit.bStartPenetrating ? 0.f : FMath::Clamp(static_cast<float>(Hit.Time), 0.f, 1.f);
			Result.Normal = GetActorTransform().InverseTransformVectorNoScale(Hit.ImpactNormal).GetSafeNormal();
			Result.PenetrationCm = Hit.bStartPenetrating ? Hit.PenetrationDepth : 0.f;
			Result.Chunk = Places.IsValidIndex(Chunks.IndexOfByKey(Chunk)) ? Places[Chunks.IndexOfByKey(Chunk)].Chunk : NAME_None;
		}
	}
	return Result;
}

bool ADeepCaveString::OverlapsHull(const FBox& Box, const FVector& AtCm, float YawDeg, float InflateCm) const
{
	if (!Box.IsValid)
	{
		return false;
	}
	const FTransform At = BoxWorld(Box, AtCm, YawDeg);
	const FCollisionShape Shape = FCollisionShape::MakeBox(Box.GetExtent() + FVector(FMath::Max(0.f, InflateCm)));
	for (const UStaticMeshComponent* Chunk : Chunks)
	{
		if (Chunk && Chunk->OverlapComponent(At.GetLocation(), At.GetRotation(), Shape))
		{
			return true;
		}
	}
	return false;
}

float ADeepCaveString::ReadDepthAtZ(double WorldZCm) const
{
	const TArray<float> Stops = UDeepTuningSettings::Get().StopDepthsM();
	const double Top = Stops.IsEmpty() ? 0.0 : Stops[0];
	return UDeepCaveStringRules::ReadDepthM(DepthMap, float(Top - (WorldZCm - GetActorLocation().Z) / UDeepRules::CmPerM));
}

bool ADeepCaveString::SocketWorld(FName Chunk, FName Socket, FVector& OutWorldCm) const
{
	const FDeepCavePlace* Place = UDeepCaveStringRules::FindPlace(Places, Chunk);
	const FDeepCaveSocketRow* Row = UDeepCaveStringRules::FindSocket(Sockets, Chunk, Socket);
	if (!Place || !Row)
	{
		return false;
	}
	OutWorldCm = GetActorTransform().TransformPosition(UDeepCaveStringRules::ToString(*Place, Row->LocalCm));
	return true;
}

bool ADeepCaveString::RockBelow(const FVector& WorldCm, float MaxDownCm, FHitResult& OutHit) const
{
	const FVector End = WorldCm - FVector(0.0, 0.0, FMath::Max(0.f, MaxDownCm));
	bool bFound = false;
	float Best = TNumericLimits<float>::Max();
	for (UStaticMeshComponent* Chunk : Chunks)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DeepCaveRockBelow), true);
		if (Chunk && Chunk->LineTraceComponent(Hit, WorldCm, End, Params) && Hit.Time < Best)
		{
			Best = Hit.Time;
			OutHit = Hit;
			bFound = true;
		}
	}
	return bFound;
}

int32 ADeepCaveString::FootprintRock(const FBox& WorldBox, float UpCm, float DownCm, double& OutHighestZ, double& OutLowestZ) const
{
	const FVector Centre = WorldBox.GetCenter();
	const FVector Half = WorldBox.GetExtent() * 0.9;
	const double TopZ = WorldBox.Min.Z + FMath::Max(0.f, UpCm);
	const float Reach = float(TopZ - WorldBox.Min.Z) + FMath::Max(0.f, DownCm);
	int32 Hits = 0;
	OutHighestZ = -TNumericLimits<double>::Max();
	OutLowestZ = TNumericLimits<double>::Max();
	for (const FVector2D Corner : { FVector2D(0, 0), FVector2D(1, 1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(-1, -1) })
	{
		FHitResult Hit;
		if (RockBelow(FVector(Centre.X + Corner.X * Half.X, Centre.Y + Corner.Y * Half.Y, TopZ), Reach, Hit))
		{
			++Hits;
			OutHighestZ = FMath::Max(OutHighestZ, Hit.ImpactPoint.Z);
			OutLowestZ = FMath::Min(OutLowestZ, Hit.ImpactPoint.Z);
		}
	}
	return Hits;
}

const FDeepSerpentCave* ADeepCaveString::SerpentCave(int32 Stop) const
{
	if (const TSharedPtr<FDeepSerpentCave>* Found = SerpentCaves.Find(Stop))
	{
		return Found->Get();
	}
	SerpentCaves.Add(Stop);
	const FDeepCaveStringTuning& CS = UDeepTuningSettings::Get().CaveString;
	if (!CS.Berths.IsValidIndex(Stop - 1))
	{
		return nullptr;
	}
	const FName Chunk = CS.Berths[Stop - 1];
	const int32 PlaceIndex = Places.IndexOfByPredicate([Chunk](const FDeepCavePlace& P) { return P.Chunk == Chunk; });
	const UStaticMeshComponent* Component = Chunks.IsValidIndex(PlaceIndex) ? Chunks[PlaceIndex].Get() : nullptr;
	const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
	FVector Socket;
	if (!Mesh || !SocketWorld(Chunk, CS.SerpentSocket, Socket))
	{
		UE_LOG(LogDeepCaveString, Warning, TEXT("Deep.World.Caves: no serpent room at stop %d (%s has no mesh or no %s socket)"), Stop, *Chunk.ToString(), *CS.SerpentSocket.ToString());
		return nullptr;
	}

	const double StartS = FPlatformTime::Seconds();
	TSharedPtr<FDeepSerpentCave> Cave = MakeShared<FDeepSerpentCave>();
	Cave->Stop = Stop;
	Cave->Chunk = Chunk;
	Cave->ToWorld = Component->GetComponentTransform();
	Cave->SocketCm = Socket;
	FDeepCaveRoom& Room = Cave->Room;
	Room.CellCm = FMath::Max(10.f, CS.SerpentRoomCellCm);
	const FBox Bounds = Mesh->GetBoundingBox();
	Room.OriginCm = Bounds.Min;
	const FVector Cells = Bounds.GetSize() / Room.CellCm;
	Room.Dims = FIntVector(FMath::Max(1, FMath::CeilToInt32(Cells.X)), FMath::Max(1, FMath::CeilToInt32(Cells.Y)), FMath::Max(1, FMath::CeilToInt32(Cells.Z)));

	// The rock: the cells the chunks' surfaces pass through, found coarse to fine (a block that touches no rock skips
	// all its cells). Whatever the flood from the socket cannot reach counts as rock too (BuildRoom).
	TArray<uint8> Rock;
	Rock.Init(0, Room.Num());
	const FQuat Rotation = Cave->ToWorld.GetRotation();
	TFunction<void(const FIntVector&, int32)> Mark;
	Mark = [&](const FIntVector& Lo, int32 Span)
	{
		const FIntVector Hi(FMath::Min(Lo.X + Span, Room.Dims.X), FMath::Min(Lo.Y + Span, Room.Dims.Y), FMath::Min(Lo.Z + Span, Room.Dims.Z));
		if (Hi.X <= Lo.X || Hi.Y <= Lo.Y || Hi.Z <= Lo.Z)
		{
			return;
		}
		const FVector MinCm = Room.OriginCm + FVector(Lo) * Room.CellCm;
		const FVector MaxCm = Room.OriginCm + FVector(Hi) * Room.CellCm;
		++Cave->Queries;
		if (!OverlapsBoxWorld(Cave->ToWorld.TransformPosition((MinCm + MaxCm) * 0.5), (MaxCm - MinCm) * 0.5 + FVector(1.0), Rotation))
		{
			return;
		}
		if (Span == 1)
		{
			Rock[Room.Index(Lo)] = 1;
			++Cave->RockCells;
			return;
		}
		const int32 Half = (Span + 1) / 2;
		for (int32 i = 0; i < 8; ++i)
		{
			Mark(Lo + FIntVector((i & 1) ? Half : 0, (i & 2) ? Half : 0, (i & 4) ? Half : 0), Half);
		}
	};
	constexpr int32 Block = 16;
	for (int32 Z = 0; Z < Room.Dims.Z; Z += Block)
	{
		for (int32 Y = 0; Y < Room.Dims.Y; Y += Block)
		{
			for (int32 X = 0; X < Room.Dims.X; X += Block)
			{
				Mark(FIntVector(X, Y, Z), Block);
			}
		}
	}
	const FVector SeedLocal = Cave->ToWorld.InverseTransformPosition(Socket);
	DeepCaveDiveRules::BuildRoom(Room, Rock, Room.CellAt(SeedLocal), CS.SerpentPathClearM * 100.f);
	for (const uint8 R : Room.Reached)
	{
		Cave->ReachedCells += R;
	}
	Cave->BuildSeconds = FPlatformTime::Seconds() - StartS;
	UE_LOG(LogDeepCaveString, Log, TEXT("Deep.World.Caves: serpent room at stop %d (%s): %d x %d x %d cells of %.0f cm, %d rock, %d water, %d free in the largest of %d pools (%.1f m clear), %d queries, %.2f s"),
		Stop, *Chunk.ToString(), Room.Dims.X, Room.Dims.Y, Room.Dims.Z, Room.CellCm, Cave->RockCells, Cave->ReachedCells, Room.FreeCount, Room.PoolCount,
		CS.SerpentPathClearM, Cave->Queries, Cave->BuildSeconds);
	if (Room.FreeCount == 0)
	{
		UE_LOG(LogDeepCaveString, Warning, TEXT("Deep.World.Caves: the serpent room at stop %d has no free cell; it keeps no path there"), Stop);
		return nullptr;
	}
	SerpentCaves[Stop] = Cave;
	return Cave.Get();
}

FVector ADeepCaveString::KeepSerpentCm(int32 Stop, const FVector& WorldCm) const
{
	const FDeepSerpentCave* Cave = SerpentCave(Stop);
	if (!Cave)
	{
		return WorldCm;
	}
	return Cave->ToWorld.TransformPosition(DeepCaveDiveRules::KeepInRoom(Cave->Room, Cave->ToWorld.InverseTransformPosition(WorldCm)));
}

float ADeepCaveString::SerpentLurkRadiusCm(int32 Stop, float MaxRadiusCm, FVector* OutCentreCm) const
{
	// The rooms live in a mutable cache; the fit is cached with them.
	FDeepSerpentCave* Cave = const_cast<FDeepSerpentCave*>(SerpentCave(Stop));
	if (!Cave)
	{
		return MaxRadiusCm;
	}
	if (!FMath::IsNearlyEqual(Cave->LurkForMaxCm, MaxRadiusCm))
	{
		const FDeepCaveStringTuning& CS = UDeepTuningSettings::Get().CaveString;
		const FVector SocketLocal = Cave->ToWorld.InverseTransformPosition(Cave->SocketCm);
		FVector CentreLocal;
		float Radius = 0.f;
		const double StartS = FPlatformTime::Seconds();
		Cave->bLurkFits = DeepCaveDiveRules::FitLurkCircle(Cave->Room, SocketLocal, CS.SerpentLurkCentreReachM * 100.f, CS.SerpentLurkCentreStepM * 100.f,
			FMath::Min(CS.SerpentLurkMinM * 100.f, MaxRadiusCm), MaxRadiusCm, 100.f, CS.SerpentLurkFreeShare, 72, CentreLocal, Radius);
		Cave->LurkForMaxCm = MaxRadiusCm;
		Cave->LurkRadiusCm = Radius;
		Cave->LurkCentreCm = Cave->ToWorld.TransformPosition(CentreLocal);
		UE_LOG(LogDeepCaveString, Log, TEXT("Deep.World.Caves: the serpent lurks %.0f m round a point %.1f m from %s's %s socket (%.0f%% of the circle in its room%s; at most %.0f m; %.2f s)"),
			Radius / 100.f, FVector::Dist(CentreLocal, SocketLocal) / 100.f, *Cave->Chunk.ToString(), *CS.SerpentSocket.ToString(),
			100.f * DeepCaveDiveRules::FreeShareOfCircle(Cave->Room, CentreLocal, Radius, 72), Cave->bLurkFits ? TEXT("") : TEXT(", NO circle fits"),
			MaxRadiusCm / 100.f, FPlatformTime::Seconds() - StartS);
	}
	if (OutCentreCm)
	{
		*OutCentreCm = Cave->LurkCentreCm;
	}
	return Cave->LurkRadiusCm;
}

bool ADeepCaveString::SerpentLurkPoint(int32 Stop, float MaxRadiusCm, float AngleRad, FVector& OutCm, FVector& OutFacing) const
{
	if (!SerpentCave(Stop))
	{
		return false;
	}
	FVector Centre;
	const float Radius = SerpentLurkRadiusCm(Stop, MaxRadiusCm, &Centre);
	const FVector Out(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.0);
	OutCm = KeepSerpentCm(Stop, Centre + Out * Radius);
	OutFacing = FVector(-Out.Y, Out.X, 0.0);
	return true;
}

FVector ADeepCaveString::SerpentRouteCm(int32 Stop, const FVector& FromCm, const FVector& ToCm) const
{
	// The route lives in the room's mutable cache, rebuilt only when its target cell changes.
	FDeepSerpentCave* Cave = const_cast<FDeepSerpentCave*>(SerpentCave(Stop));
	if (!Cave)
	{
		return ToCm;
	}
	const float LookCm = UDeepTuningSettings::Get().CaveString.SerpentRouteLookM * 100.f;
	return Cave->ToWorld.TransformPosition(DeepCaveDiveRules::RouteAim(Cave->Room, Cave->Route, Cave->ToWorld.InverseTransformPosition(FromCm),
		Cave->ToWorld.InverseTransformPosition(ToCm), LookCm));
}

FVector ADeepCaveString::SerpentBitePoint(int32 Stop, const FVector& AnchorCm, const FVector& DiverEndCm, float FromDiverCm, float ToleranceCm) const
{
	const FDeepSerpentCave* Cave = SerpentCave(Stop);
	if (!Cave)
	{
		const double Length = FVector::Dist(AnchorCm, DiverEndCm);
		return Length > 1.0 ? FMath::Lerp(DiverEndCm, AnchorCm, FMath::Clamp(FromDiverCm / Length, 0.0, 1.0)) : DiverEndCm;
	}
	const float StepCm = UDeepTuningSettings::Get().CaveString.SerpentBiteStepM * 100.f;
	return Cave->ToWorld.TransformPosition(DeepCaveDiveRules::ReachableOnLine(Cave->Room, Cave->ToWorld.InverseTransformPosition(AnchorCm),
		Cave->ToWorld.InverseTransformPosition(DiverEndCm), FromDiverCm, ToleranceCm, StepCm));
}

bool ADeepCaveString::OverlapsBoxWorld(const FVector& CentreCm, const FVector& HalfCm, const FQuat& Rotation) const
{
	const FCollisionShape Shape = FCollisionShape::MakeBox(HalfCm);
	for (const UStaticMeshComponent* Chunk : Chunks)
	{
		if (Chunk && Chunk->OverlapComponent(CentreCm, Rotation, Shape))
		{
			return true;
		}
	}
	return false;
}

namespace
{
	/** The outside view's camera (one per world; a debug view for frames, not part of the game). */
	TWeakObjectPtr<ACameraActor> EyeCamera;

	FAutoConsoleCommandWithWorldAndArgs CaveEyeCommand(
		TEXT("Deep.Cave.Eye"),
		TEXT("Deep.Cave.Eye <back m> <up m> [side m] | off: views the hull from outside (behind, above, to starboard of it, looking at it); off = the player's own view."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			const ADeepSubmarine* Sub = World ? DeepArena::FindSubmarine(World) : nullptr;
			if (!Player || !Sub)
			{
				return;
			}
			if (Args.Num() > 0 && Args[0].Equals(TEXT("off"), ESearchCase::IgnoreCase))
			{
				Player->SetViewTarget(Player->GetPawn());
				if (EyeCamera.IsValid())
				{
					EyeCamera->Destroy();
				}
				DeepStateReport::LogState(World);
				return;
			}
			const float BackM = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 18.f;
			const float UpM = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 6.f;
			const float SideM = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 0.f;
			const FTransform Hull = Sub->GetActorTransform();
			const FVector Target = Hull.TransformPosition(Sub->GetSolidBoundsLocal(false).GetCenter());
			const FVector At = Hull.TransformPosition(FVector(-BackM, SideM, UpM) * UDeepRules::CmPerM);
			if (!EyeCamera.IsValid())
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				Params.ObjectFlags |= RF_Transient;
				EyeCamera = World->SpawnActor<ACameraActor>(At, FRotator::ZeroRotator, Params);
				if (EyeCamera.IsValid())
				{
					EyeCamera->Tags.Add(TEXT("DeepEye"));
				}
			}
			if (EyeCamera.IsValid())
			{
				EyeCamera->SetActorLocationAndRotation(At, (Target - At).Rotation());
				Player->SetViewTarget(EyeCamera.Get());
			}
			DeepStateReport::LogState(World);
		}));
}
