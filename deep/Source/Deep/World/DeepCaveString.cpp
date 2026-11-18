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
