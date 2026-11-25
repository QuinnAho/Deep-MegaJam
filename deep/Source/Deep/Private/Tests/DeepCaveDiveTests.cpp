// Tests for Deep.World.Caves (cave-dive M1): the baked chunk string S, A, T1, C1, B, E is the rock. A run starts parked at
// S's berth, the real hull box flies the exported path clean, the helm's sweep stops it on rock with a dent and a scrape,
// it parks on C1's berth and stays there, and with the switch off the shaft world is as before.

#include "CoreMinimal.h"
#include "AIController.h"
#include "AI/AISystemBase.h"
#include "Monster/DeepMonsterBrain.h"
#include "Monster/DeepMonsterRules.h"
#include "Monster/DeepMonsterSettings.h"
#include "Monster/DeepSerpent.h"
#include "Monster/DeepSerpentController.h"
#include "Perception/AIPerceptionSystem.h"
#include "Diver/DeepTetherComponent.h"
#include "Story/DeepEventBus.h"
#include "Components/StaticMeshComponent.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "Misc/AutomationTest.h"
#include "Private/Tests/DeepTestCVar.h"
#include "Sim/DeepCaveDiveRules.h"
#include "Sim/DeepCaveStringRules.h"
#include "Sim/DeepPilotRules.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepArena.h"
#include "World/DeepCaveString.h"
#include "World/DeepSalvage.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "World/DeepShaft.h"
#include "World/DeepSubmarine.h"
#include "World/DeepSubRoom.h"
#include "UI/DeepHudCalm.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_CAVEDIVE_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepCaveDiveTest
{
	using DeepTest::FCVarScope;

	/** A game world with the arena and one diver in the room, as a run starts (as Deep.World.Still's). */
	struct FDiveWorld
	{
		/** The helm's mode for the test: M1 and M2b's tests fly the levers (D-048), D-070's the direct drive. */
		FCVarScope Levers;
		UWorld* World = nullptr;
		ADeepGameMode* GameMode = nullptr;
		ADeepGameState* State = nullptr;
		ADeepSubmarine* Sub = nullptr;
		ADeepDiver* Diver = nullptr;
		UDeepRunComponent* Run = nullptr;

		/** bAI: the world has an AI system (perception), for the serpent. bLevers: the lever mode, else direct drive. */
		explicit FDiveWorld(bool bAI = false, bool bLevers = true)
			: Levers(TEXT("Deep.Pilot.Levers"), bLevers ? 1 : 0)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			if (bAI)
			{
				World->CreateAISystem();
			}
			World->InitializeActorsForPlay(FURL());
			GameMode = World->SpawnActor<ADeepGameMode>();
			Sub = GameMode ? GameMode->EnsureArena() : nullptr;
			if (GameMode)
			{
				GameMode->StartPlay();
			}
			if (UAISystemBase* AISystem = bAI ? World->GetAISystem() : nullptr)
			{
				AISystem->StartPlay();
			}
			State = World->GetGameState<ADeepGameState>();
			if (Sub)
			{
				Diver = World->SpawnActor<ADeepDiver>(DeepArena::EnsureRoom(World)->GetRoomStartTransform().GetLocation(), FRotator::ZeroRotator);
			}
			Run = Diver ? Diver->GetRun() : nullptr;
			if (Run)
			{
				Run->StepRun(0.f);
			}
		}

		~FDiveWorld()
		{
			if (World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		bool Valid() const { return GameMode && State && Sub && Diver && Run; }

		ADeepCaveString* String() const { return DeepCaveString::Find(World); }

		FDeepCavePose Pose() const { return DeepCaveString::HullPose(*State); }

		void TickSub() const
		{
			static_cast<AActor*>(Sub)->Tick(1.f / 30.f);
		}

		/** Seated at the helm, levers home, the hull placed (as Deep.Cave.Goto). */
		void PlaceAt(const FDeepCavePose& Pose, bool bPark = false) const
		{
			if (!Run->IsSeated())
			{
				Run->TakeHelm();
			}
			Run->AllStop();
			Run->SetPilotHold(0.f, 0.f);
			Run->PlaceInCave(Pose.LocationCm, Pose.YawDeg, bPark);
		}

		/** Runs the helm (and the shaft's park glide, which must leave a cave hull alone) in 0.05 s steps. */
		void Fly(float Seconds) const
		{
			for (float T = 0.f; T < Seconds; T += 0.05f)
			{
				Run->StepRun(0.05f);
				State->StepParkGlide(0.05f);
			}
		}

		/** The path sample nearest to a point (string frame, cm). */
		int32 NearestSample(const FVector& Cm) const
		{
			const TArray<FDeepCavePose>& Poses = String()->GetPathPoses();
			int32 Best = INDEX_NONE;
			double BestCm = TNumericLimits<double>::Max();
			for (int32 Index = 0; Index < Poses.Num(); ++Index)
			{
				const double D = FVector::Dist(Poses[Index].LocationCm, Cm);
				if (D < BestCm)
				{
					BestCm = D;
					Best = Index;
				}
			}
			return Best;
		}
	};

	static FString Describe(const FDeepCavePose& Pose)
	{
		return FString::Printf(TEXT("(%.0f, %.0f, %.0f) yaw %.1f"), Pose.LocationCm.X, Pose.LocationCm.Y, Pose.LocationCm.Z, Pose.YawDeg);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDiveStartTest, "Deep.World.CaveDive.StartsAtS", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDiveStartTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W;
	if (!TestTrue(TEXT("the world is up"), W.Valid()))
	{
		return false;
	}
	TestTrue(TEXT("the game state latched caves"), W.State->bWorldCaves);
	TestTrue(TEXT("IsWorldCaves"), DeepArena::IsWorldCaves(W.World));
	const ADeepCaveString* String = W.String();
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	TestEqual(TEXT("six chunks"), String->GetChunks().Num(), 6);
	TestEqual(TEXT("six places"), String->GetPlaces().Num(), 6);
	TestTrue(TEXT("the path is loaded"), String->GetPathPoses().Num() > 1000);
	int32 Shafts = 0;
	for (TActorIterator<ADeepShaft> It(W.World); It; ++It)
	{
		++Shafts;
	}
	TestEqual(TEXT("no shaft rock in caves"), Shafts, 0);
	TestFalse(TEXT("the shaft is off in this world"), DeepShaft::IsOn(W.World));
	TestTrue(TEXT("there is rock to hit"), DeepShaft::HasRock(W.World));

	FDeepCavePose Berth;
	TestTrue(TEXT("S has a berth"), String->SocketPose(TEXT("S"), UDeepTuningSettings::Get().CaveString.ParkSocket, Berth));
	const FDeepCavePose Pose = W.Pose();
	AddInfo(FString::Printf(TEXT("hull %s, S berth %s"), *Describe(Pose), *Describe(Berth)));
	TestTrue(TEXT("the run starts on S's berth"), FVector::Dist(Pose.LocationCm, Berth.LocationCm) < 1.0);
	TestTrue(TEXT("the run starts parked"), W.State->bParked);
	TestFalse(TEXT("the hull starts clear of rock"), String->OverlapsHull(W.Sub->GetSolidBoundsLocal(false), Pose.LocationCm, Pose.YawDeg));

	// The chunks stand where the string's frame says: the hull's world pose is the frame's transform of its pose.
	W.TickSub();
	const FVector Expected = String->GetActorTransform().TransformPosition(Pose.LocationCm);
	AddInfo(FString::Printf(TEXT("hull world %s, expected %s"), *W.Sub->GetActorLocation().ToString(), *Expected.ToString()));
	TestTrue(TEXT("the hull stands at its pose in the string"), FVector::Dist(W.Sub->GetActorLocation(), Expected) < 1.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDivePathTest, "Deep.World.CaveDive.PathClear", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDivePathTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W;
	const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	// The real hull box (the model's solid bounds), against cavestring's (615, 165, 250) half size, 94 cm aft.
	const FBox Box = W.Sub->GetSolidBoundsLocal(false);
	AddInfo(FString::Printf(TEXT("real hull box half %s, centre %s (cavestring: half (615, 165, 250), centre 94 aft)"), *Box.GetExtent().ToString(), *Box.GetCenter().ToString()));

	const TArray<FDeepCavePose>& Poses = String->GetPathPoses();
	int32 Contacts = 0;
	int32 Steps = 0;
	for (int32 Index = 0; Index + 1 < Poses.Num(); ++Index)
	{
		const FDeepCavePose& A = Poses[Index];
		const FDeepCavePose& B = Poses[Index + 1];
		++Steps;
		const FDeepCaveSweep Hit = String->SweepHull(Box, A.LocationCm, B.LocationCm, A.YawDeg);
		const bool bTurn = !FMath::IsNearlyEqual(A.YawDeg, B.YawDeg) && String->OverlapsHull(Box, B.LocationCm, B.YawDeg);
		if (Hit.bHit || bTurn)
		{
			if (++Contacts <= 8)
			{
				AddWarning(FString::Printf(TEXT("contact at step %d (%s, %s): %s %s, %s, pen %.1f cm, normal %s"), Index,
					*String->GetPath()[Index].Chunk.ToString(), *String->GetPath()[Index].Region.ToString(), *Describe(A),
					bTurn ? TEXT("turning") : TEXT("moving"), *Hit.Chunk.ToString(), Hit.PenetrationCm, *Hit.Normal.ToString()));
			}
		}
	}
	AddInfo(FString::Printf(TEXT("DEEPCAVEDIVE path sweep: %d contacts in %d steps, %.0f m"), Contacts, Steps, String->GetPathMetres().IsEmpty() ? 0.f : String->GetPathMetres().Last()));
	TestEqual(TEXT("the real hull flies the exported path S to E without touching"), Contacts, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDiveDriveTest, "Deep.World.CaveDive.DrivesOutOfS", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDiveDriveTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W;
	const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	W.Run->TakeHelm();
	const FDeepCavePose Start = W.Pose();
	// Ahead two notches: the berth lets go, the hull drives out along S toward A, never into rock.
	W.Run->StepThrottle(1);
	W.Run->StepThrottle(1);
	W.Fly(8.f);
	const FDeepCavePose Pose = W.Pose();
	AddInfo(FString::Printf(TEXT("from %s to %s, damage %d"), *Describe(Start), *Describe(Pose), W.State->HullDamage));
	TestFalse(TEXT("unparked by the throttle"), W.State->bParked);
	TestTrue(TEXT("driven ahead (+X) at least 10 m"), Pose.LocationCm.X - Start.LocationCm.X > 1000.0);
	TestFalse(TEXT("never inside rock"), String->OverlapsHull(W.Sub->GetSolidBoundsLocal(false), Pose.LocationCm, Pose.YawDeg));
	TestTrue(TEXT("the helm reports speed"), W.State->HelmSpeedMps > 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDiveKnockTest, "Deep.World.CaveDive.KnockAndScrape", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDiveKnockTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W;
	const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	const FBox Box = W.Sub->GetSolidBoundsLocal(false);
	// In A's cavern, flooded hard: the hull sinks onto the floor fast enough to knock, then grinds on it.
	const int32 Sample = UDeepCaveStringRules::FindPathSample(String->GetPath(), String->GetPathMetres(), TEXT("A"), 10.f);
	if (!TestTrue(TEXT("a sample in A"), String->GetPathPoses().IsValidIndex(Sample)))
	{
		return false;
	}
	W.PlaceAt(String->GetPathPoses()[Sample]);
	const FDeepCavePose Start = W.Pose();
	W.Run->SetPilotHold(1.f, 0.f);
	float LowestCm = Start.LocationCm.Z;
	float FastestMps = 0.f;
	for (float T = 0.f; T < 30.f && W.State->HullDamage == 0; T += 0.05f)
	{
		W.Fly(0.05f);
		LowestCm = FMath::Min(LowestCm, float(W.Pose().LocationCm.Z));
		FastestMps = FMath::Max(FastestMps, W.Run->GetPilotMotion().HeaveMps);
	}
	const FDeepCavePose Hit = W.Pose();
	AddInfo(FString::Printf(TEXT("from %s down to %s at up to %.2f m/s, damage %d"), *Describe(Start), *Describe(Hit), FastestMps, W.State->HullDamage));
	TestTrue(TEXT("the knock dents the hull"), W.State->HullDamage > 0);
	TestTrue(TEXT("the knock stops the sink (or bounces it)"), W.Run->GetPilotMotion().HeaveMps < FastestMps * 0.5f);
	TestFalse(TEXT("stopped on the rock, not in it"), String->OverlapsHull(Box, Hit.LocationCm, Hit.YawDeg));

	// Held down on the floor: a scrape (contact) every frame, a dent per ScrapeSecondsPerDamage, and never through.
	const int32 AfterKnock = W.State->HullDamage;
	W.Fly(UDeepTuningSettings::Get().Cave.ScrapeSecondsPerDamage * 2.f + 1.f);
	const FDeepCavePose Held = W.Pose();
	AddInfo(FString::Printf(TEXT("held at %s, damage %d"), *Describe(Held), W.State->HullDamage));
	TestTrue(TEXT("grinding on the floor scrapes more dents"), W.State->HullDamage > AfterKnock);
	TestTrue(TEXT("held down, the hull does not sink through"), Held.LocationCm.Z > Hit.LocationCm.Z - 10.0);
	TestFalse(TEXT("held down, not in the rock"), String->OverlapsHull(Box, Held.LocationCm, Held.YawDeg));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDiveParkTest, "Deep.World.CaveDive.ParksAtC1", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDiveParkTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W;
	const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	FDeepCavePose Berth;
	if (!TestTrue(TEXT("C1 has a berth"), String->SocketPose(TEXT("C1"), UDeepTuningSettings::Get().CaveString.ParkSocket, Berth)))
	{
		return false;
	}
	AddInfo(FString::Printf(TEXT("C1 berth %s"), *Describe(Berth)));
	TestTrue(TEXT("C1's berth is (12500, 7800, -3250) yaw 90"), FVector::Dist(Berth.LocationCm, FVector(12500.0, 7800.0, -3250.0)) < 1.0 && FMath::IsNearlyEqual(Berth.YawDeg, 90.f, 0.1f));

	// Three metres short on the path, slow, levers home: it glides onto the berth and parks.
	const TArray<FDeepCavePose>& Poses = String->GetPathPoses();
	int32 Near = INDEX_NONE;
	for (int32 Index = 0; Index < Poses.Num(); ++Index)
	{
		const double D = FVector::Dist(Poses[Index].LocationCm, Berth.LocationCm);
		if (D > 250.0 && D < 400.0)
		{
			Near = Index;
			break;
		}
	}
	if (!TestTrue(TEXT("a path sample 2.5 to 4 m from C1's berth"), Poses.IsValidIndex(Near)))
	{
		return false;
	}
	W.PlaceAt(Poses[Near]);
	TestFalse(TEXT("placed unparked"), W.State->bParked);
	W.Fly(8.f);
	const FDeepCavePose Parked = W.Pose();
	AddInfo(FString::Printf(TEXT("from %s parked at %s"), *Describe(Poses[Near]), *Describe(Parked)));
	TestTrue(TEXT("parked at C1"), W.State->bParked);
	TestTrue(TEXT("on the berth"), FVector::Dist(Parked.LocationCm, Berth.LocationCm) < 2.0);
	TestTrue(TEXT("at the berth's heading"), FMath::Abs(FMath::FindDeltaAngleDegrees(Parked.YawDeg, Berth.YawDeg)) < 0.5f);

	// No depth rule drags it: the shaft's park glide, the sub's tick and more helm time leave it on the berth.
	for (int32 Step = 0; Step < 60; ++Step)
	{
		W.State->StepParkGlide(0.05f);
		W.TickSub();
	}
	W.Fly(3.f);
	const FDeepCavePose After = W.Pose();
	TestTrue(TEXT("still on the berth"), FVector::Dist(After.LocationCm, Berth.LocationCm) < 2.0);
	TestTrue(TEXT("still parked"), W.State->bParked);
	TestFalse(TEXT("parked clear of rock"), String->OverlapsHull(W.Sub->GetSolidBoundsLocal(false), After.LocationCm, After.YawDeg));
	return true;
}

namespace DeepCaveDiveTest
{
	/** C1's first sample in its sink (the path drops straight down onto the berth), FromTopCm below the sink's top. */
	static int32 SinkSample(const ADeepCaveString& String, double FromTopCm)
	{
		const TArray<FDeepCavePathPoint>& Path = String.GetPath();
		for (int32 Index = 0; Index < Path.Num(); ++Index)
		{
			if (Path[Index].Chunk == TEXT("C1") && Path[Index].LocalCm.Z <= -FromTopCm)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	/** Floods (ballast full) for up to Seconds or until the gauge reads StopAtM. */
	static void Flood(const FDiveWorld& W, float Seconds, float StopAtM = TNumericLimits<float>::Max())
	{
		W.Run->SetPilotHold(1.f, 0.f);
		for (float T = 0.f; T < Seconds && W.State->SubDepthM < StopAtM; T += 0.05f)
		{
			W.Fly(0.05f);
		}
	}

	/** Each salvage piece in the world, with its bounds. */
	static TArray<ADeepSalvage*> Salvage(UWorld* World)
	{
		TArray<ADeepSalvage*> Out;
		for (TActorIterator<ADeepSalvage> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				Out.Add(*It);
			}
		}
		return Out;
	}

	static FBox PieceBounds(const ADeepSalvage& Piece)
	{
		FBox Box(ForceInit);
		Piece.ForEachComponent<UPrimitiveComponent>(false, [&Box](const UPrimitiveComponent* Prim)
		{
			if (Prim->IsRegistered() && (Prim->IsVisible() || Prim->IsCollisionEnabled()))
			{
				Box += Prim->Bounds.GetBox();
			}
		});
		return Box;
	}

	/**
	 * D-067: the stop's salvage lies in piles at its cavern's sockets. Each piece belongs to its nearest socket (by X and
	 * Y); each pile is its slot's kinds, within SalvagePileReachCm of its socket, its pieces apart from each other. Every
	 * piece rests on rock (rock within a few cm under its bottom), is clear of it (its bounds, lifted by the sink, overlap
	 * none), and is within the tether's reach of the anchor. The stop's kinds are the shaft's (Arena.SalvageSpots).
	 */
	static void CheckCavernSalvage(FAutomationTestBase& Test, const FDiveWorld& W, int32 Stop)
	{
		const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
		const FDeepCaveStringTuning& CS = Tuning.CaveString;
		const ADeepCaveString* String = W.String();
		const FName Cavern = CS.Berths[Stop - 1];
		TArray<ADeepSalvage*> Pieces = Salvage(W.World);
		Pieces.RemoveAll([](const ADeepSalvage* Piece) { return Piece->GetKind().Size == EDeepSalvageSize::Relic; });
		const FVector Anchor = W.Sub->GetAnchorLocation();
		const float TetherCm = UDeepRules::TetherLengthM(Stop, Tuning.Tether) * UDeepRules::CmPerM;
		const double Reach = CS.SalvagePileReachCm + 1.0;

		// The piles: this cavern's slots and their sockets.
		TArray<const FDeepCaveSalvageSlot*> Slots;
		TArray<FVector> Sockets;
		for (const FDeepCaveSalvageSlot& Slot : CS.Salvage)
		{
			FVector Socket;
			if (Slot.Chunk == Cavern && Test.TestTrue(FString::Printf(TEXT("%s.%s exists"), *Slot.Chunk.ToString(), *Slot.Socket.ToString()), String->SocketWorld(Slot.Chunk, Slot.Socket, Socket)))
			{
				Slots.Add(&Slot);
				Sockets.Add(Socket);
			}
		}
		if (Slots.Num() == 0)
		{
			Test.AddError(FString::Printf(TEXT("stop %d: %s has no salvage slots"), Stop, *Cavern.ToString()));
			return;
		}

		// The stop's kinds are the shaft's ten.
		TArray<int32> Want;
		for (const FDeepSalvageSpot& Spot : Tuning.Arena.SalvageSpots)
		{
			Want.Add(static_cast<int32>(Spot.Size));
		}
		TArray<int32> Have;
		for (const ADeepSalvage* Piece : Pieces)
		{
			Have.Add(static_cast<int32>(Piece->GetKind().Size));
		}
		Want.Sort();
		Have.Sort();
		Test.TestEqual(FString::Printf(TEXT("stop %d: the shaft's %d pieces"), Stop, Want.Num()), Have.Num(), Want.Num());
		Test.TestTrue(FString::Printf(TEXT("stop %d: the shaft's kinds"), Stop), Have == Want);

		// Each piece to its nearest socket.
		TArray<TArray<const ADeepSalvage*>> Piles;
		Piles.SetNum(Slots.Num());
		for (const ADeepSalvage* Piece : Pieces)
		{
			int32 Best = 0;
			for (int32 i = 1; i < Sockets.Num(); ++i)
			{
				if (FVector::Dist2D(Piece->GetActorLocation(), Sockets[i]) < FVector::Dist2D(Piece->GetActorLocation(), Sockets[Best]))
				{
					Best = i;
				}
			}
			Piles[Best].Add(Piece);
		}

		for (int32 p = 0; p < Slots.Num(); ++p)
		{
			const FDeepCaveSalvageSlot& Slot = *Slots[p];
			const FVector& Socket = Sockets[p];
			const FString Name = FString::Printf(TEXT("%s.%s"), *Slot.Chunk.ToString(), *Slot.Socket.ToString());
			TArray<int32> PileWant;
			for (const EDeepSalvageSize Size : Slot.Pile)
			{
				PileWant.Add(static_cast<int32>(Size));
			}
			TArray<int32> PileHave;
			for (const ADeepSalvage* Piece : Piles[p])
			{
				PileHave.Add(static_cast<int32>(Piece->GetKind().Size));
			}
			PileWant.Sort();
			PileHave.Sort();
			Test.TestTrue(FString::Printf(TEXT("%s: a pile of %d, its slot's kinds (has %d)"), *Name, PileWant.Num(), PileHave.Num()), PileHave == PileWant);

			for (int32 i = 0; i < Piles[p].Num(); ++i)
			{
				const ADeepSalvage& Piece = *Piles[p][i];
				const FString What = FString::Printf(TEXT("%s piece %d"), *Name, i + 1);
				const FVector At = Piece.GetActorLocation();
				const FBox Box = PieceBounds(Piece);
				double Highest = 0.0;
				double Lowest = 0.0;
				const bool bRock = String->FootprintRock(Box, float(Box.GetSize().Z), 30.f, Highest, Lowest) > 0;
				const double Gap = bRock ? Box.Min.Z - Highest : -1.0;
				const bool bClear = !DeepArena::SalvageInRock(*String, Piece, CS.SalvageSinkCm + 1.0);
				const double FromAnchor = FVector::Dist(Anchor, At);
				Test.AddInfo(FString::Printf(TEXT("DEEPCAVESALVAGE %s (%s): %.0f cm across and %.0f cm up from the socket, bottom %.1f cm above rock, %s, %.1f m from the anchor (tether %.0f m); at %s"),
					*What, *UEnum::GetValueAsString(Piece.GetKind().Size), FVector::Dist2D(At, Socket), At.Z - Socket.Z, Gap, bClear ? TEXT("clear") : TEXT("IN ROCK"),
					FromAnchor / UDeepRules::CmPerM, TetherCm / UDeepRules::CmPerM, *At.ToCompactString()));
				// Not floating: 10 cm lower, its own shapes would be in rock.
				Test.TestTrue(FString::Printf(TEXT("%s rests on rock (10 cm lower is in it)"), *What), bRock && DeepArena::SalvageInRock(*String, Piece, -10.0));
				Test.TestTrue(FString::Printf(TEXT("%s is clear of rock"), *What), bClear);
				Test.TestTrue(FString::Printf(TEXT("%s is within the tether from the anchor"), *What), FromAnchor < TetherCm);
				Test.TestTrue(FString::Printf(TEXT("%s is within %.0f cm of its socket"), *What, Reach), FVector::Dist2D(At, Socket) <= Reach && FMath::Abs(At.Z - Socket.Z) <= Reach);
				for (int32 j = i + 1; j < Piles[p].Num(); ++j)
				{
					Test.TestFalse(FString::Printf(TEXT("%s is apart from piece %d"), *What, j + 1), Box.Intersect(PieceBounds(*Piles[p][j])));
				}
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDiveReadDepthTest, "Deep.World.CaveDive.ReadsStops", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDiveReadDepthTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W;
	const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	TestEqual(TEXT("the string's map is CaveDepthMap.csv's three knots"), String->GetDepthMap().Num(), 3);
	TestEqual(TEXT("S's berth reads stop 1"), W.State->SubDepthM, 100.f, 0.01f);
	TestEqual(TEXT("stop 1"), W.State->Stop, 1);
	TestEqual(TEXT("the hull's height reads 100 m"), String->ReadDepthAtZ(W.Sub->GetActorLocation().Z), 100.f, 0.05f);
	TestTrue(TEXT("the hatch is open at S"), W.Sub->IsHatchOpen());

	// C1's berth: 32.5 m below S, read as 300 m; NearestStop, the hatch and the diver's gauge agree.
	FDeepCavePose Berth;
	String->SocketPose(TEXT("C1"), UDeepTuningSettings::Get().CaveString.ParkSocket, Berth);
	W.PlaceAt(Berth, true);
	W.TickSub();
	AddInfo(FString::Printf(TEXT("at C1: reads %.2f m, stop %d, hull z %.0f"), W.State->SubDepthM, W.State->Stop, W.Sub->GetActorLocation().Z));
	TestEqual(TEXT("C1's berth reads stop 2"), W.State->SubDepthM, 300.f, 0.01f);
	TestEqual(TEXT("stop 2"), W.State->Stop, 2);
	TestTrue(TEXT("settled at the stop"), W.State->IsSettledAtStop());
	TestTrue(TEXT("the hatch is open at C1"), W.Sub->IsHatchOpen());
	TestTrue(TEXT("the hull stands at its true pose"), FVector::Dist(W.Sub->GetActorLocation(), String->GetActorTransform().TransformPosition(Berth.LocationCm)) < 1.0);
	TestEqual(TEXT("the hull's height reads 300 m"), String->ReadDepthAtZ(W.Sub->GetActorLocation().Z), 300.f, 0.05f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDiveLimitTest, "Deep.World.CaveDive.LimitHoldsInSink", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDiveLimitTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W;
	const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	const int32 Sample = SinkSample(*String, 150.0);
	if (!TestTrue(TEXT("a sample at the top of C1's sink"), String->GetPathPoses().IsValidIndex(Sample)))
	{
		return false;
	}
	// No gates: flooded hard down the sink, the hull stops where the gauge reads the limit, well above the berth.
	W.PlaceAt(String->GetPathPoses()[Sample]);
	TestEqual(TEXT("no gates bought"), W.State->GatesBought(), 0);
	const float Limit = W.State->DepthLimitM();
	Flood(W, 40.f);
	const FDeepCavePose Held = W.Pose();
	const float Physical = DeepCaveString::PhysicalDepthM(W.World, W.State->SubDepthM);
	AddInfo(FString::Printf(TEXT("held at %s: reads %.2f m (limit %.0f), true %.2f m, stop %d"), *Describe(Held), W.State->SubDepthM, Limit, Physical, W.State->Stop));
	TestEqual(TEXT("the gauge reads the limit"), W.State->SubDepthM, Limit, 0.05f);
	TestEqual(TEXT("about 120 m true"), Physical, 120.f, 0.5f);
	TestTrue(TEXT("the helm reports the limit"), W.Run->IsHelmAtLimit());
	TestEqual(TEXT("still stop 1"), W.State->Stop, 1);
	TestTrue(TEXT("in C1's sink, above its berth"), Held.LocationCm.Z > -3250.0 + 500.0 && Held.LocationCm.Z < -850.0);
	TestFalse(TEXT("not parked"), W.State->bParked);
	TestEqual(TEXT("no dents: nothing touched"), W.State->HullDamage, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDiveGateTest, "Deep.World.CaveDive.GateSinksToC1", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDiveGateTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W;
	const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	const int32 Sample = SinkSample(*String, 150.0);
	if (!TestTrue(TEXT("a sample at the top of C1's sink"), String->GetPathPoses().IsValidIndex(Sample)))
	{
		return false;
	}
	// One gate bought: the limit reads 400, below C1's berth. Flooded until the gauge nears 300, then levers home.
	W.State->HullRating = 2;
	W.PlaceAt(String->GetPathPoses()[Sample]);
	Flood(W, 60.f, 285.f);
	W.Run->AllStop();
	W.Run->SetPilotHold(0.f, 0.f);
	W.Fly(20.f);
	const FDeepCavePose Parked = W.Pose();
	FDeepCavePose Berth;
	String->SocketPose(TEXT("C1"), UDeepTuningSettings::Get().CaveString.ParkSocket, Berth);
	AddInfo(FString::Printf(TEXT("parked at %s (berth %s): reads %.2f m, stop %d, deepest %d, damage %d"), *Describe(Parked), *Describe(Berth),
		W.State->SubDepthM, W.State->Stop, W.State->DeepestStopReached, W.State->HullDamage));
	TestTrue(TEXT("parked"), W.State->bParked);
	TestTrue(TEXT("on C1's berth"), FVector::Dist(Parked.LocationCm, Berth.LocationCm) < 2.0);
	TestEqual(TEXT("reads 300 m"), W.State->SubDepthM, 300.f, 0.01f);
	TestEqual(TEXT("stop 2"), W.State->Stop, 2);
	TestEqual(TEXT("the deepest stop reached is 2"), W.State->DeepestStopReached, 2);
	W.TickSub();
	TestTrue(TEXT("the hatch opens"), W.Sub->IsHatchOpen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDiveSalvageTest, "Deep.World.CaveDive.SalvageAtSockets", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDiveSalvageTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W;
	const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	// Stop 1: S's sockets, from S's berth. Then parked at C1: stop 2's pieces at C1's sockets.
	CheckCavernSalvage(*this, W, 1);
	FDeepCavePose Berth;
	String->SocketPose(TEXT("C1"), UDeepTuningSettings::Get().CaveString.ParkSocket, Berth);
	W.PlaceAt(Berth, true);
	W.TickSub();
	TestEqual(TEXT("stop 2"), W.State->Stop, 2);
	CheckCavernSalvage(*this, W, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDiveRelicTest, "Deep.World.CaveDive.RelicAtLastBerth", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDiveRelicTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W;
	const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const FDeepCaveStringTuning& CS = Tuning.CaveString;
	UDeepEventBus* Bus = UDeepEventBus::Get(W.World);
	UTEST_NOT_NULL(TEXT("the event bus"), Bus);

	// D-068: the relic waits at the last berth (today C1, stop 2), not at the shaft's stop 3.
	const int32 LastStop = CS.Berths.Num();
	const FName Last = CS.Berths.Last();
	TestEqual(TEXT("the caves' relic stop is the last berth's"), DeepArena::RelicStop(W.World), LastStop);
	TestNull(TEXT("no relic at stop 1"), DeepArena::FindRelic(W.World));

	FDeepCavePose Berth;
	String->SocketPose(Last, CS.ParkSocket, Berth);
	W.PlaceAt(Berth, true);
	W.TickSub();
	TestEqual(TEXT("parked at the last berth"), W.State->Stop, LastStop);
	const ADeepSalvage* Relic = DeepArena::FindRelic(W.World);
	UTEST_NOT_NULL(TEXT("the relic lies at the last berth"), Relic);

	// At its socket, on the floor and clear of the rock, inside the tether, apart from the piles, in sight of the anchor.
	FVector Socket;
	UTEST_TRUE(TEXT("the last berth has a Relic socket"), String->SocketWorld(Last, CS.RelicSocket, Socket));
	const FVector At = Relic->GetActorLocation();
	const FVector Anchor = W.Sub->GetAnchorLocation();
	const float TetherCm = UDeepRules::TetherLengthM(LastStop, Tuning.Tether) * UDeepRules::CmPerM;
	double NearestPileCm = TNumericLimits<double>::Max();
	for (const ADeepSalvage* Piece : Salvage(W.World))
	{
		if (Piece != Relic)
		{
			NearestPileCm = FMath::Min(NearestPileCm, FVector::Dist(Piece->GetActorLocation(), At));
		}
	}
	bool bSeen = true;
	for (const UStaticMeshComponent* Chunk : String->GetChunks())
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DeepCaveRelicSight), true);
		const FVector Above = At + FVector(0.0, 0.0, 30.0);
		bSeen &= !(Chunk && const_cast<UStaticMeshComponent*>(Chunk)->LineTraceComponent(Hit, Anchor, Above, Params));
	}
	FVector Serpent;
	String->SocketWorld(Last, TEXT("Serpent"), Serpent);
	AddInfo(FString::Printf(TEXT("DEEPCAVERELIC at %s: %.0f cm across and %.0f cm down from %s.%s, %.1f m from the anchor (tether %.0f m), %.1f m below it, %.1f m from the nearest salvage, %.1f m from the Serpent socket, %s"),
		*At.ToCompactString(), FVector::Dist2D(At, Socket), Socket.Z - At.Z, *Last.ToString(), *CS.RelicSocket.ToString(), FVector::Dist(At, Anchor) / UDeepRules::CmPerM,
		TetherCm / UDeepRules::CmPerM, (Anchor.Z - At.Z) / UDeepRules::CmPerM, NearestPileCm / UDeepRules::CmPerM, FVector::Dist(At, Serpent) / UDeepRules::CmPerM,
		bSeen ? TEXT("in sight of the anchor") : TEXT("HIDDEN from the anchor")));
	TestTrue(TEXT("at its socket (within a step)"), FVector::Dist2D(At, Socket) <= CS.SalvagePileStepCm + 1.0 && FMath::Abs(At.Z - Socket.Z) < CS.SalvageRestSearchCm);
	TestTrue(TEXT("rests on rock (10 cm lower is in it)"), DeepArena::SalvageInRock(*String, *Relic, -10.0));
	TestFalse(TEXT("clear of rock"), DeepArena::SalvageInRock(*String, *Relic, CS.SalvageSinkCm + 1.0));
	TestTrue(TEXT("inside the tether from the anchor"), FVector::Dist(At, Anchor) < TetherCm);
	TestTrue(TEXT("apart from the piles (over 5 m)"), NearestPileCm > 500.0);
	TestTrue(TEXT("in sight of the anchor: the line runs clear to it"), bSeen);

	// Out of the hatch, to the relic, grabbed, winched up through the hatch, banked at the console: won.
	AAIController* Controller = W.World->SpawnActor<AAIController>();
	UTEST_NOT_NULL(TEXT("a controller"), Controller);
	Controller->Possess(W.Diver);
	W.Run->LeaveHelm();
	GEngine->Exec(W.World, TEXT("Deep.Hatch"));
	UTEST_EQUAL(TEXT("diving"), W.Run->GetState(), EDeepRunState::Tethered);
	W.Diver->GetTether()->UpdateTether(0.f);
	TestEqual(TEXT("the last berth's tether"), W.Diver->GetTether()->GetSolve().LengthM, UDeepRules::TetherLengthM(LastStop, Tuning.Tether), 0.01f);
	GEngine->Exec(W.World, TEXT("Deep.Relic"));
	const UCapsuleComponent* Capsule = W.Diver->GetCapsuleComponent();
	const FVector Half(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
	TestFalse(TEXT("Deep.Relic leaves the diver clear of rock"), String->OverlapsBoxWorld(W.Diver->GetActorLocation(), Half, FQuat::Identity));
	W.Run->StepRun(0.05f);
	TestEqual(TEXT("relic_seen fired"), Bus->CountOf(DeepEvent::RelicSeen), 1);
	W.Diver->Grab();
	UTEST_TRUE(TEXT("carrying the relic"), W.Run->CarriesRelic());
	W.Diver->SetWinching(true);
	for (float T = 0.f; T < 60.f && W.Run->GetState() == EDeepRunState::Tethered; T += 0.1f)
	{
		W.Diver->GetTether()->UpdateTether(0.1f);
		W.Run->StepRun(0.1f);
	}
	TestEqual(TEXT("relic_winch fired"), Bus->CountOf(DeepEvent::RelicWinch), 1);
	TestEqual(TEXT("the winch brings it aboard"), W.Run->GetState(), EDeepRunState::Aboard);
	TestTrue(TEXT("still carrying it aboard"), W.Run->CarriesRelic());
	GEngine->Exec(W.World, TEXT("Deep.Console"));
	TestEqual(TEXT("banking the relic wins"), W.Run->GetState(), EDeepRunState::Won);
	TestTrue(TEXT("the game state says won"), W.State->bWon);
	TestEqual(TEXT("win fired"), Bus->CountOf(DeepEvent::Win), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDiveDiverTest, "Deep.World.CaveDive.DiverOutOfHatch", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDiveDiverTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W;
	const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	const UCapsuleComponent* Capsule = W.Diver->GetCapsuleComponent();
	const FVector Half(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
	for (int32 Stop = 1; Stop <= 2; ++Stop)
	{
		const FName Cavern = UDeepTuningSettings::Get().CaveString.Berths[Stop - 1];
		FDeepCavePose Berth;
		String->SocketPose(Cavern, UDeepTuningSettings::Get().CaveString.ParkSocket, Berth);
		W.PlaceAt(Berth, true);
		W.TickSub();
		TestTrue(FString::Printf(TEXT("%s: the hatch is open"), *Cavern.ToString()), W.Sub->IsHatchOpen());
		// Out of the hatch: the diver's start is in open water, and the gauge reads the stop there or a little below.
		const FTransform Start = W.Sub->GetDiverStartTransform();
		TestFalse(FString::Printf(TEXT("%s: the diver starts clear of rock"), *Cavern.ToString()), String->OverlapsBoxWorld(Start.GetLocation(), Half, FQuat::Identity));
		W.Diver->SetActorLocation(Start.GetLocation(), false, nullptr, ETeleportType::TeleportPhysics);
		const float Reads = String->ReadDepthAtZ(Start.GetLocation().Z);
		AddInfo(FString::Printf(TEXT("%s: diver start reads %.1f m"), *Cavern.ToString(), Reads));
		TestTrue(FString::Printf(TEXT("%s: the diver's gauge reads near the stop"), *Cavern.ToString()), Reads >= W.State->SubDepthM && Reads < W.State->SubDepthM + 40.f);

		// The rock stops the diver: swept from above each piece straight down through the floor, it lands on it.
		for (const ADeepSalvage* Piece : Salvage(W.World))
		{
			const FVector Above = Piece->GetActorLocation() + FVector(0.0, 0.0, Half.Z + 120.0);
			W.Diver->SetActorLocation(Above, false, nullptr, ETeleportType::TeleportPhysics);
			if (String->OverlapsBoxWorld(Above, Half, FQuat::Identity))
			{
				AddWarning(FString::Printf(TEXT("%s: no room for the diver above %s"), *Cavern.ToString(), *Piece->GetActorLocation().ToString()));
				continue;
			}
			W.Diver->SetActorLocation(Above - FVector(0.0, 0.0, 800.0), true);
			TestTrue(FString::Printf(TEXT("%s: the floor under %s stops the diver"), *Cavern.ToString(), *Piece->GetName()), W.Diver->GetActorLocation().Z > Piece->GetActorLocation().Z - 200.0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDiveHudNextTest, "Deep.World.CaveDive.HudNextStop", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDiveHudNextTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W;
	const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	// D-068: the HUD names only the cave string's berths; the next stop is the next berth, and past the last there is none.
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const TArray<FName>& Berths = Tuning.CaveString.Berths;
	const TArray<float> Shown = DeepArena::ShownStopDepthsM(W.World);
	TestEqual(TEXT("the HUD names the berths' stops"), Shown.Num(), Berths.Num());
	for (int32 Stop = 1; Stop <= Berths.Num(); ++Stop)
	{
		FDeepCavePose Berth;
		String->SocketPose(Berths[Stop - 1], Tuning.CaveString.ParkSocket, Berth);
		W.PlaceAt(Berth, true);
		W.TickSub();
		const float NextM = DeepHudCalm::NextStopM(DeepArena::ShownStopDepthsM(W.World), W.State->SubDepthM, W.State->ParkedStop());
		AddInfo(FString::Printf(TEXT("parked at %s (stop %d, %.0f m): next stop %.0f m"), *Berths[Stop - 1].ToString(), W.State->ParkedStop(), W.State->SubDepthM, NextM));
		TestEqual(FString::Printf(TEXT("parked at %s"), *Berths[Stop - 1].ToString()), W.State->ParkedStop(), Stop);
		if (Stop < Berths.Num())
		{
			TestEqual(FString::Printf(TEXT("%s: the next stop is the next berth's"), *Berths[Stop - 1].ToString()), NextM, Tuning.GetStop(Stop + 1).SubDepthM, 0.01f);
		}
		else
		{
			TestTrue(FString::Printf(TEXT("%s, the last berth: no next stop"), *Berths[Stop - 1].ToString()), NextM < 0.f);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDiveOffTest, "Deep.World.CaveDive.OffIsUnchanged", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDiveOffTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Shaft(TEXT("Deep.Cave.Shaft"), 1);
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 0);
	const FDiveWorld W;
	if (!TestTrue(TEXT("the world is up"), W.Valid()))
	{
		return false;
	}
	TestFalse(TEXT("no caves latched"), W.State->bWorldCaves);
	TestNull(TEXT("no chunk string"), W.String());
	int32 Shafts = 0;
	for (TActorIterator<ADeepShaft> It(W.World); It; ++It)
	{
		++Shafts;
	}
	TestTrue(TEXT("the shaft's rock is there"), Shafts > 0);
	TestTrue(TEXT("the shaft is on"), DeepShaft::IsOn(W.World));
	TestEqual(TEXT("the HUD names all the shaft's stops"), DeepArena::ShownStopDepthsM(W.World).Num(), UDeepTuningSettings::Get().NumStops());
	TestEqual(TEXT("the relic waits at the shaft's stop"), DeepArena::RelicStop(W.World), UDeepTuningSettings::Get().Arena.RelicStop);
	return true;
}

// ---- Deep.Sim.CaveDive: the pure rules (Sim/DeepCaveDiveRules) ----

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDivePileSpotsTest, "Deep.Sim.CaveDive.PileSpots", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDivePileSpotsTest::RunTest(const FString& Parameters)
{
	// A pile of three, 70 cm out, 25 cm steps, 200 cm reach, facing +X.
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const TArray<FVector2D> Spots = DeepCaveDiveRules::PileSpotsCm(Index, 3, 70.f, 25.f, 200.f, 0.f);
		if (!TestTrue(FString::Printf(TEXT("piece %d has spots"), Index), Spots.Num() > 3))
		{
			return false;
		}
		const double Want = FMath::DegreesToRadians(120.0 * Index);
		TestEqual(FString::Printf(TEXT("piece %d first: 70 cm out"), Index), Spots[0].Size(), 70.0, 0.01);
		TestTrue(FString::Printf(TEXT("piece %d first: at its share"), Index), Spots[0].Equals(FVector2D(FMath::Cos(Want), FMath::Sin(Want)) * 70.0, 0.01));
		for (const FVector2D& Spot : Spots)
		{
			TestTrue(FString::Printf(TEXT("piece %d: within the reach"), Index), Spot.Size() <= 200.01);
			if (Spot.Size() > 1.0)
			{
				const double Off = FMath::Abs(FMath::FindDeltaAngleDegrees(FMath::RadiansToDegrees(FMath::Atan2(Spot.Y, Spot.X)), 120.0 * Index));
				TestTrue(FString::Printf(TEXT("piece %d: keeps its side (%.0f deg off)"), Index, Off), Off < 60.0);
			}
		}
	}
	// A pile of one starts at the socket; yaw turns the pile.
	TestTrue(TEXT("a pile of one starts at the socket"), DeepCaveDiveRules::PileSpotsCm(0, 1, 70.f, 25.f, 200.f, 0.f)[0].IsNearlyZero());
	TestTrue(TEXT("the socket's yaw turns the pile"), DeepCaveDiveRules::PileSpotsCm(0, 2, 70.f, 25.f, 200.f, 90.f)[0].Equals(FVector2D(0.0, 70.0), 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDiveStopsTest, "Deep.Sim.CaveDive.RelicAndShownStops", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDiveStopsTest::RunTest(const FString& Parameters)
{
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	TestEqual(TEXT("the shaft's relic stop"), DeepCaveDiveRules::RelicStop(false, 3, 2), 3);
	TestEqual(TEXT("the caves' relic stop"), DeepCaveDiveRules::RelicStop(true, 3, 2), 2);
	TestEqual(TEXT("a third berth moves it to stop 3"), DeepCaveDiveRules::RelicStop(true, 3, 3), 3);
	TestEqual(TEXT("tuning: the shaft's relic stays at 3"), Tuning.Arena.RelicStop, 3);
	TestEqual(TEXT("the shaft names all its stops"), DeepCaveDiveRules::ShownStops(false, 3, 2), 3);
	TestEqual(TEXT("the caves name their berths"), DeepCaveDiveRules::ShownStops(true, 3, 2), 2);
	TestEqual(TEXT("never more than the shaft has"), DeepCaveDiveRules::ShownStops(true, 3, 5), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDiveRoomTest, "Deep.Sim.CaveDive.Room", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDiveRoomTest::RunTest(const FString& Parameters)
{
	// A 40 x 20 x 20 grid of 50 cm cells: a box room (cells 2..17 in Y and Z) with a wall at x = 20, open below its
	// middle (z 2..5) so the two halves join, and a shelf at x 28..29 that seals the pocket past it (x 30..37) off.
	FDeepCaveRoom Room;
	Room.CellCm = 50.f;
	Room.Dims = FIntVector(40, 20, 20);
	Room.OriginCm = FVector(-1000.0, -500.0, -500.0);
	TArray<uint8> Rock;
	Rock.Init(1, Room.Num());
	for (int32 i = 0; i < Room.Num(); ++i)
	{
		const FIntVector C = Room.CellOf(i);
		const bool bBox = C.X >= 2 && C.X <= 37 && C.Y >= 2 && C.Y <= 17 && C.Z >= 2 && C.Z <= 17;
		const bool bWall = C.X == 20 && C.Z > 5;
		const bool bShelf = C.X >= 28 && C.X <= 29;
		Rock[i] = (bBox && !bWall && !bShelf) ? 0 : 1;
	}
	DeepCaveDiveRules::BuildRoom(Room, Rock, FIntVector(10, 10, 10), 100.f);
	UTEST_TRUE(TEXT("the room is built"), Room.IsValid());
	TestTrue(TEXT("the room has free cells"), Room.FreeCount > 0);
	TestTrue(TEXT("the seed is reached"), Room.Reached[Room.Index(FIntVector(10, 10, 10))] != 0);
	TestTrue(TEXT("the far side of the wall is reached, under it"), Room.Reached[Room.Index(FIntVector(25, 10, 10))] != 0);
	TestFalse(TEXT("the pocket past the shelf is not"), Room.Reached[Room.Index(FIntVector(33, 10, 10))] != 0);

	// Every free cell is ClearCm plus a diagonal off rock (here: the box's faces and the wall).
	int32 Bad = 0;
	for (int32 i = 0; i < Room.Num(); ++i)
	{
		if (!Room.Free[i])
		{
			continue;
		}
		const FIntVector C = Room.CellOf(i);
		const FVector P = Room.CentreCm(C);
		for (int32 j = 0; j < Room.Num(); ++j)
		{
			if (!Room.Reached[j] && FVector::Dist(P, Room.CentreCm(Room.CellOf(j))) < 100.0 + 50.0 * UE_SQRT_3 - 0.01)
			{
				++Bad;
				break;
			}
		}
	}
	TestEqual(TEXT("free cells stand clear of every solid cell"), Bad, 0);

	// Kept: a free point is unchanged; a point in the wall, in the rock or in the pocket comes back into a free cell.
	const FVector Middle = Room.CentreCm(FIntVector(10, 10, 10));
	TestTrue(TEXT("a free point is unchanged"), DeepCaveDiveRules::KeepInRoom(Room, Middle).Equals(Middle, 0.01));
	for (const FIntVector& From : { FIntVector(20, 10, 12), FIntVector(0, 0, 0), FIntVector(33, 10, 10), FIntVector(39, 19, 19) })
	{
		const FVector P = Room.CentreCm(From);
		const FVector Kept = DeepCaveDiveRules::KeepInRoom(Room, P);
		const int32 At = Room.Index(Room.CellAt(Kept));
		TestTrue(FString::Printf(TEXT("%s is kept in a free cell"), *From.ToString()), Room.Free[At] != 0);
		TestTrue(FString::Printf(TEXT("%s is kept nearby (%.0f cm)"), *From.ToString(), FVector::Dist(P, Kept)), FVector::Dist(P, Kept) < 1500.0);
	}
	// The circle's free share: all of a small one round the middle, none of one in the pocket.
	TestEqual(TEXT("a circle in the open is all free"), DeepCaveDiveRules::FreeShareOfCircle(Room, Middle, 200.f, 24), 1.f, 0.001f);
	TestEqual(TEXT("a circle in the pocket is not"), DeepCaveDiveRules::FreeShareOfCircle(Room, Room.CentreCm(FIntVector(33, 10, 10)), 100.f, 24), 0.f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDiveSerpentRouteTest, "Deep.Sim.CaveDive.SerpentRoute", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDiveSerpentRouteTest::RunTest(const FString& Parameters)
{
	// A long room split by a rib of rock from one side wall, open past its end: the way from one half to the other goes
	// round the rib. A straight swim into the rib stalls on it; the route goes round.
	FDeepCaveRoom Room;
	Room.CellCm = 50.f;
	Room.Dims = FIntVector(60, 30, 12);
	Room.OriginCm = FVector::ZeroVector;
	TArray<uint8> Rock;
	Rock.Init(1, Room.Num());
	for (int32 i = 0; i < Room.Num(); ++i)
	{
		const FIntVector C = Room.CellOf(i);
		const bool bBox = C.X >= 1 && C.X <= 58 && C.Y >= 1 && C.Y <= 28 && C.Z >= 1 && C.Z <= 10;
		const bool bRib = C.X >= 29 && C.X <= 30 && C.Y <= 18;
		Rock[i] = (bBox && !bRib) ? 0 : 1;
	}
	DeepCaveDiveRules::BuildRoom(Room, Rock, FIntVector(10, 5, 6), 10.f);
	UTEST_TRUE(TEXT("the room is built"), Room.IsValid() && Room.FreeCount > 0);
	const FVector From = Room.CentreCm(FIntVector(12, 6, 6));
	const FVector To = Room.CentreCm(FIntVector(48, 6, 6));
	UTEST_TRUE(TEXT("both ends are free"), Room.Free[Room.Index(Room.CellAt(From))] && Room.Free[Room.Index(Room.CellAt(To))]);
	TestFalse(TEXT("the straight way crosses the rib"), DeepCaveDiveRules::LineFree(Room, From, To));
	TestTrue(TEXT("a way along one half is free"), DeepCaveDiveRules::LineFree(Room, From, Room.CentreCm(FIntVector(20, 10, 6))));

	FDeepCaveRoute Route;
	const FVector First = DeepCaveDiveRules::RouteAim(Room, Route, From, To, 800.f);
	TestEqual(TEXT("the route is built to the target's cell"), Route.Target, Room.Index(Room.CellAt(To)));
	TestTrue(TEXT("the first aim is in sight"), DeepCaveDiveRules::LineFree(Room, From, First));
	TestTrue(TEXT("and on the way"), Route.Steps[Room.Index(Room.CellAt(First))] < Route.Steps[Room.Index(Room.CellAt(From))]);
	TestTrue(TEXT("in sight of the goal, the goal"), DeepCaveDiveRules::RouteAim(Room, Route, Room.CentreCm(FIntVector(40, 6, 6)), To, 800.f).Equals(To));

	// Swim 0.5 m a step toward the aim, kept in the room: routed it arrives, straight it stalls on the rib.
	auto Swim = [&](bool bRouted, int32& OutSteps)
	{
		FVector At = From;
		for (OutSteps = 0; OutSteps < 400; ++OutSteps)
		{
			if (FVector::Dist(At, To) < 50.0)
			{
				return true;
			}
			const FVector Aim = bRouted ? DeepCaveDiveRules::RouteAim(Room, Route, At, To, 800.f) : To;
			const FVector Move = (Aim - At).GetClampedToMaxSize(50.0);
			At = DeepCaveDiveRules::KeepInRoom(Room, At + Move);
		}
		return false;
	};
	int32 RoutedSteps = 0;
	int32 StraightSteps = 0;
	const bool bRouted = Swim(true, RoutedSteps);
	const bool bStraight = Swim(false, StraightSteps);
	AddInfo(FString::Printf(TEXT("routed: %s in %d steps of 0.5 m (straight line %.1f m); straight: %s"), bRouted ? TEXT("arrives") : TEXT("stalls"),
		RoutedSteps, FVector::Dist(From, To) / 100.0, bStraight ? TEXT("arrives") : TEXT("stalls")));
	TestTrue(TEXT("the routed swim arrives"), bRouted);
	TestFalse(TEXT("the straight swim stalls on the rib"), bStraight);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDiveSerpentFitTest, "Deep.Sim.CaveDive.SerpentFit", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDiveSerpentFitTest::RunTest(const FString& Parameters)
{
	// The room of Deep.Sim.CaveDive.Room: a box split by a wall at x = 20, open under it (z 2..5) too low for a free cell.
	FDeepCaveRoom Room;
	Room.CellCm = 50.f;
	Room.Dims = FIntVector(40, 20, 20);
	Room.OriginCm = FVector(-1000.0, -500.0, -500.0);
	TArray<uint8> Rock;
	Rock.Init(1, Room.Num());
	for (int32 i = 0; i < Room.Num(); ++i)
	{
		const FIntVector C = Room.CellOf(i);
		const bool bBox = C.X >= 2 && C.X <= 37 && C.Y >= 2 && C.Y <= 17 && C.Z >= 2 && C.Z <= 17;
		const bool bWall = C.X == 20 && C.Z > 5;
		const bool bShelf = C.X >= 28 && C.X <= 29;
		Rock[i] = (bBox && !bWall && !bShelf) ? 0 : 1;
	}
	DeepCaveDiveRules::BuildRoom(Room, Rock, FIntVector(10, 10, 10), 100.f);
	UTEST_TRUE(TEXT("the room is built"), Room.IsValid() && Room.FreeCount > 0);

	// One pool: the far side of the wall is water, but its free cells join the seed's only through the gap, which has
	// none, and they are fewer, so they are dropped and a point there is kept on the seed's side.
	const FIntVector Far(24, 10, 10);
	TestTrue(TEXT("the far side is water"), Room.Reached[Room.Index(Far)] != 0);
	TestFalse(TEXT("the far side's free cells, a smaller pool, are dropped"), Room.Free[Room.Index(Far)] != 0);
	int32 PastWall = 0;
	for (int32 i = 0; i < Room.Num(); ++i)
	{
		PastWall += (Room.Free[i] && Room.CellOf(i).X >= 20) ? 1 : 0;
	}
	TestEqual(TEXT("no free cell past the wall"), PastWall, 0);
	const FVector FarKept = DeepCaveDiveRules::KeepInRoom(Room, Room.CentreCm(Far));
	TestTrue(TEXT("a point past the wall is kept on the seed's side"), Room.CellAt(FarKept).X < 20 && Room.Free[Room.Index(Room.CellAt(FarKept))] != 0);

	// The lurk fit: the widest circle (from 10 m down in 0.5 m steps) with 60% in the room; the next one up has less.
	const FVector Middle = Room.CentreCm(FIntVector(10, 10, 10));
	const float Fit = DeepCaveDiveRules::FitLurkRadiusCm(Room, Middle, 100.f, 1000.f, 50.f, 0.6f, 72);
	const float FitShare = DeepCaveDiveRules::FreeShareOfCircle(Room, Middle, Fit, 72);
	AddInfo(FString::Printf(TEXT("lurk fit %.0f cm, %.0f%% free; %.0f cm: %.0f%%"), Fit, 100.f * FitShare, Fit + 50.f,
		100.f * DeepCaveDiveRules::FreeShareOfCircle(Room, Middle, Fit + 50.f, 72)));
	TestTrue(TEXT("the fitted circle is mostly in the room"), FitShare >= 0.6f);
	TestTrue(TEXT("it is narrower than the largest"), Fit < 1000.f);
	TestTrue(TEXT("the next one out is not"), DeepCaveDiveRules::FreeShareOfCircle(Room, Middle, Fit + 50.f, 72) < 0.6f);
	TestTrue(TEXT("a small circle fits whole"), FitShare > 0.f && DeepCaveDiveRules::FitLurkRadiusCm(Room, Middle, 100.f, 150.f, 50.f, 0.6f, 72) == 150.f);
	TestEqual(TEXT("no circle fits in the sealed pocket: the smallest"), DeepCaveDiveRules::FitLurkRadiusCm(Room, Room.CentreCm(FIntVector(33, 10, 10)), 100.f, 400.f, 50.f, 0.6f, 72), 100.f);

	// The circle near a socket by the wall: a centre within reach in the open gives a wider circle than the socket's own.
	const FVector Socket = Room.CentreCm(FIntVector(16, 6, 10));
	FVector Centre;
	float Radius = 0.f;
	const float AtSocket = DeepCaveDiveRules::FitLurkRadiusCm(Room, Socket, 100.f, 1000.f, 50.f, 0.9f, 72);
	const bool bFits = DeepCaveDiveRules::FitLurkCircle(Room, Socket, 300.f, 50.f, 100.f, 1000.f, 50.f, 0.9f, 72, Centre, Radius);
	AddInfo(FString::Printf(TEXT("lurk circle near the wall: %.0f cm round a point %.0f cm from the socket (%.0f%% free); round the socket %.0f cm (%.0f%%)"),
		Radius, FVector::Dist(Centre, Socket), 100.f * DeepCaveDiveRules::FreeShareOfCircle(Room, Centre, Radius, 72), AtSocket,
		100.f * DeepCaveDiveRules::FreeShareOfCircle(Room, Socket, AtSocket, 72)));
	TestTrue(TEXT("a circle fits near the socket"), bFits);
	TestTrue(TEXT("its centre is within reach of the socket"), FVector::Dist(Centre, Socket) <= 300.0 + 0.01);
	TestTrue(TEXT("its centre is free"), Room.Free[Room.Index(Room.CellAt(Centre))] != 0);
	TestTrue(TEXT("it is at least 90% free"), DeepCaveDiveRules::FreeShareOfCircle(Room, Centre, Radius, 72) >= 0.9f);
	TestTrue(TEXT("it is wider than the socket's own"), Radius > AtSocket);
	FVector PocketCentre;
	float PocketRadius = 0.f;
	TestFalse(TEXT("none fits by the sealed pocket"), DeepCaveDiveRules::FitLurkCircle(Room, Room.CentreCm(FIntVector(33, 10, 10)), 100.f, 50.f, 100.f, 400.f, 50.f, 0.9f, 72, PocketCentre, PocketRadius));
	TestTrue(TEXT("then it is the socket's"), PocketCentre.Equals(Room.CentreCm(FIntVector(33, 10, 10))) && PocketRadius == 100.f);

	// The bite point: up the line from a diver in the rock under the floor, the first point within tolerance of the room.
	const FVector Anchor = Middle;
	const FVector DiverEnd = Room.CentreCm(FIntVector(10, 10, 0));
	const FVector Bite = DeepCaveDiveRules::ReachableOnLine(Room, Anchor, DiverEnd, 0.f, 50.f, 25.f);
	const double Gap = FVector::Dist(DeepCaveDiveRules::KeepInRoom(Room, Bite), Bite);
	const FVector Before = Bite - (Anchor - DiverEnd).GetSafeNormal() * 25.0;
	AddInfo(FString::Printf(TEXT("bite at z %.0f cm (diver end %.0f, anchor %.0f), %.0f cm from the room"), Bite.Z, DiverEnd.Z, Anchor.Z, Gap));
	TestTrue(TEXT("the bite point is on the line"), FMath::Abs(Bite.X - Anchor.X) < 0.01 && FMath::Abs(Bite.Y - Anchor.Y) < 0.01 && Bite.Z >= DiverEnd.Z && Bite.Z <= Anchor.Z);
	TestTrue(TEXT("within tolerance of the room"), Gap <= 50.0);
	TestTrue(TEXT("and the first such point from the diver"), FVector::Dist(DeepCaveDiveRules::KeepInRoom(Room, Before), Before) > 50.0);
	// A diver in the open: BiteFromDiver up the line, as in the shaft.
	const FVector OpenEnd = Room.CentreCm(FIntVector(14, 10, 10));
	TestTrue(TEXT("in the open the bite is FromDiver up the line"),
		DeepCaveDiveRules::ReachableOnLine(Room, Middle, OpenEnd, 100.f, 50.f, 25.f).Equals(OpenEnd + (Middle - OpenEnd).GetSafeNormal() * 100.0, 0.01));
	// A line wholly in the sealed pocket: the point that comes closest.
	const FVector PocketBite = DeepCaveDiveRules::ReachableOnLine(Room, Room.CentreCm(FIntVector(31, 10, 10)), Room.CentreCm(FIntVector(35, 10, 10)), 0.f, 50.f, 25.f);
	TestTrue(TEXT("a line out of reach gives its nearest point"), FMath::Abs(PocketBite.X - Room.CentreCm(FIntVector(31, 10, 10)).X) < 0.01);
	return true;
}

namespace DeepCaveDiveTest
{
	/** A sphere (world) touches a chunk's rock (complex collision). */
	static bool SphereInRock(const ADeepCaveString& String, const FVector& CentreCm, float RadiusCm)
	{
		const FCollisionShape Ball = FCollisionShape::MakeSphere(RadiusCm);
		for (const UStaticMeshComponent* Chunk : String.GetChunks())
		{
			if (Chunk && const_cast<UStaticMeshComponent*>(Chunk)->OverlapComponent(CentreCm, FQuat::Identity, Ball))
			{
				return true;
			}
		}
		return false;
	}

	/** A world point lies in the room's water (reached from the socket). */
	static bool InWater(const FDeepSerpentCave& Cave, const FVector& WorldCm)
	{
		const FVector Local = Cave.ToWorld.InverseTransformPosition(WorldCm);
		const FVector Size = FVector(Cave.Room.Dims) * Cave.Room.CellCm;
		const FBox Box(Cave.Room.OriginCm, Cave.Room.OriginCm + Size);
		return !Box.IsInside(Local) || Cave.Room.Reached[Cave.Room.Index(Cave.Room.CellAt(Local))] != 0;
	}

	/** Parked at a berth's Park socket, out of the hatch and on the line. */
	static bool DiveAt(FAutomationTestBase& Test, const FDiveWorld& W, FName Chunk)
	{
		FDeepCavePose Berth;
		if (!W.String()->SocketPose(Chunk, UDeepTuningSettings::Get().CaveString.ParkSocket, Berth))
		{
			return false;
		}
		if (W.Run->CanSwim())
		{
			W.Run->EnterSub();
		}
		W.PlaceAt(Berth, true);
		W.TickSub();
		W.Run->LeaveHelm();
		GEngine->Exec(W.World, TEXT("Deep.Hatch"));
		W.Diver->GetTether()->UpdateTether(0.f);
		return Test.TestEqual(FString::Printf(TEXT("diving at %s"), *Chunk.ToString()), W.Run->GetState(), EDeepRunState::Tethered);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDiveSerpentRoomTest, "Deep.World.CaveDive.SerpentRoom", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDiveSerpentRoomTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W;
	const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const FDeepCaveStringTuning& CS = Tuning.CaveString;
	TestNull(TEXT("no room past the last berth"), String->SerpentCave(CS.Berths.Num() + 1));
	for (int32 Stop = 1; Stop <= CS.Berths.Num(); ++Stop)
	{
		const FName Chunk = CS.Berths[Stop - 1];
		FDeepCavePose Berth;
		UTEST_TRUE(TEXT("the berth has a Park socket"), String->SocketPose(Chunk, CS.ParkSocket, Berth));
		W.PlaceAt(Berth, true);
		W.TickSub();
		UTEST_EQUAL(TEXT("parked at the berth"), W.State->Stop, Stop);
		const FDeepMonsterTuning Monster = UDeepMonsterSettings::Get().GetTuning(Stop);

		const FDeepSerpentCave* Cave = String->SerpentCave(Stop);
		if (!TestNotNull(FString::Printf(TEXT("%s has a serpent room"), *Chunk.ToString()), Cave))
		{
			continue;
		}
		const FDeepCaveRoom& Room = Cave->Room;
		const FVector SocketLocal = Cave->ToWorld.InverseTransformPosition(Cave->SocketCm);
		AddInfo(FString::Printf(TEXT("DEEPSERPENTROOM %s (stop %d): %d x %d x %d cells of %.0f cm, %d rock, %d water, %d free in the largest of %d pools (%.1f m clear), %d queries, %.2f s"),
			*Chunk.ToString(), Stop, Room.Dims.X, Room.Dims.Y, Room.Dims.Z, Room.CellCm, Cave->RockCells, Cave->ReachedCells, Room.FreeCount, Room.PoolCount,
			Room.ClearCm / 100.f, Cave->Queries, Cave->BuildSeconds));
		TestTrue(TEXT("the socket is in the water"), Room.Reached[Room.Index(Room.CellAt(SocketLocal))] != 0);
		TestTrue(TEXT("the room has free cells"), Room.FreeCount > 0);
		TestTrue(TEXT("built in under 10 s"), Cave->BuildSeconds < 10.0);

		// The lurk circle round the socket: fitted, mostly in the room; every kept point and step a path ball clear of rock.
		FVector Centre;
		const float LurkCm = String->SerpentLurkRadiusCm(Stop, Monster.LurkRadiusM * 100.f, &Centre);
		const float Share = DeepCaveDiveRules::FreeShareOfCircle(Room, Cave->ToWorld.InverseTransformPosition(Centre), LurkCm, 72);
		int32 Moved = 0;
		int32 Fails = 0;
		int32 Dry = 0;
		double FarthestCm = 0.0;
		FVector Prev;
		for (int32 i = 0; i <= 120; ++i)
		{
			FVector At, Facing;
			String->SerpentLurkPoint(Stop, Monster.LurkRadiusM * 100.f, 2.f * PI * i / 120.f, At, Facing);
			const FVector Raw = Centre + FVector(FMath::Cos(2.f * PI * i / 120.f), FMath::Sin(2.f * PI * i / 120.f), 0.0) * LurkCm;
			Moved += At.Equals(Raw, 1.0) ? 0 : 1;
			FarthestCm = FMath::Max(FarthestCm, FVector::Dist(At, Raw));
			for (const FVector& P : { At, i > 0 ? String->KeepSerpentCm(Stop, (Prev + At) * 0.5) : At })
			{
				Fails += SphereInRock(*String, P, Room.ClearCm - 5.f) ? 1 : 0;
				Dry += InWater(*Cave, P) ? 0 : 1;
			}
			Prev = At;
		}
		AddInfo(FString::Printf(TEXT("DEEPSERPENTLURK %s: %.1f m round a point %.1f m from the Serpent socket (%.1f m up; shaft %.0f m), %.0f%% of the circle free, %d of 121 points moved (at most %.1f m), %d in rock, %d out of the water"),
			*Chunk.ToString(), LurkCm / 100.f, FVector::Dist(Centre, Cave->SocketCm) / 100.f, (Centre.Z - Cave->SocketCm.Z) / 100.f, Monster.LurkRadiusM, 100.f * Share, Moved,
			FarthestCm / 100.f, Fails, Dry));
		TestTrue(TEXT("a lurk circle fits"), Cave->bLurkFits);
		TestTrue(TEXT("its centre is near the socket"), FVector::Dist(Centre, Cave->SocketCm) <= CS.SerpentLurkCentreReachM * 100.f + 1.f);
		TestTrue(TEXT("the lurk circle is at least SerpentLurkMinM"), LurkCm >= CS.SerpentLurkMinM * 100.f - 1.f);
		TestTrue(TEXT("most of it is in the room"), Share >= CS.SerpentLurkFreeShare);
		TestEqual(TEXT("lurk points within the path clearance of rock"), Fails, 0);
		TestEqual(TEXT("lurk points out of the water"), Dry, 0);

		// The reach: a diver at each salvage piece (and the relic), and in sampled water cells within the tether: the head
		// kept at the bite point is within BiteReachM of the line.
		const FVector Anchor = W.Sub->GetAnchorLocation();
		const float TetherCm = UDeepRules::TetherLengthM(Stop, Tuning.Tether) * UDeepRules::CmPerM;
		auto Reach = [&](const FVector& DiverCm, double& OutLineCm, double& OutDiverCm)
		{
			const FVector Bite = String->SerpentBitePoint(Stop, Anchor, DiverCm, Monster.BiteFromDiverM * 100.f, 0.5f * Monster.BiteReachM * 100.f);
			const FVector Head = String->KeepSerpentCm(Stop, Bite);
			OutLineCm = FMath::PointDistToSegment(Head, Anchor, DiverCm);
			OutDiverCm = FVector::Dist(Head, DiverCm);
			return OutLineCm <= Monster.BiteReachM * 100.f;
		};
		int32 Pieces = 0;
		int32 PieceMisses = 0;
		double WorstPieceCm = 0.0;
		for (const ADeepSalvage* Piece : Salvage(W.World))
		{
			double LineCm, DiverCm;
			++Pieces;
			PieceMisses += Reach(Piece->GetActorLocation() + FVector(0.0, 0.0, 60.0), LineCm, DiverCm) ? 0 : 1;
			WorstPieceCm = FMath::Max(WorstPieceCm, LineCm);
		}
		int32 Samples = 0;
		int32 Misses = 0;
		int32 Caught = 0;
		double WorstCm = 0.0;
		FVector WorstAt = FVector::ZeroVector;
		const int32 Stride = FMath::Max(1, Room.Num() / 4000);
		for (int32 i = 0; i < Room.Num(); i += Stride)
		{
			if (!Room.Reached[i])
			{
				continue;
			}
			const FVector P = Cave->ToWorld.TransformPosition(Room.CentreCm(Room.CellOf(i)));
			if (FVector::Dist(P, Anchor) > TetherCm)
			{
				continue;
			}
			double LineCm, DiverCm;
			++Samples;
			if (!Reach(P, LineCm, DiverCm))
			{
				++Misses;
			}
			Caught += DiverCm <= Monster.CatchReachM * 100.f ? 1 : 0;
			if (LineCm > WorstCm)
			{
				WorstCm = LineCm;
				WorstAt = P;
			}
		}
		AddInfo(FString::Printf(TEXT("DEEPSERPENTREACH %s: %d salvage pieces, %d out of reach (worst %.1f m off the line); %d water cells within the %.0f m tether, %d out of reach (worst %.1f m off the line at %s), the head within catch reach (%.1f m) of the diver at %d"),
			*Chunk.ToString(), Pieces, PieceMisses, WorstPieceCm / 100.0, Samples, TetherCm / 100.f, Misses, WorstCm / 100.0, *WorstAt.ToCompactString(), Monster.CatchReachM, Caught));
		TestTrue(TEXT("there is salvage to reach"), Pieces > 0);
		TestEqual(TEXT("salvage pieces where it cannot bite the line"), PieceMisses, 0);
		TestTrue(TEXT("water sampled"), Samples > 100);
		TestEqual(TEXT("water cells within the tether where it cannot bite the line"), Misses, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDiveSerpentLivesTest, "Deep.World.CaveDive.SerpentLives", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDiveSerpentLivesTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	constexpr float Dt = 0.1f;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDeepCaveStringTuning& CS = UDeepTuningSettings::Get().CaveString;

	// Each berth in a fresh world, so one cavern's noise, bite and cut line never carry into the next.
	for (int32 Stop = 1; Stop <= CS.Berths.Num(); ++Stop)
	{
		const FName Chunk = CS.Berths[Stop - 1];
		const FDiveWorld W(/*bAI*/ true);
		const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
		if (!TestNotNull(TEXT("the chunk string is built"), String))
		{
			return false;
		}
		UDeepMonsterBrain* Brain = W.World->GetSubsystem<UDeepMonsterBrain>();
		UTEST_NOT_NULL(TEXT("the monster brain"), Brain);
		UAIPerceptionSystem* Perception = UAIPerceptionSystem::GetCurrent(*W.World);
		AAIController* Controller = W.World->SpawnActor<AAIController>();
		UTEST_NOT_NULL(TEXT("a controller"), Controller);
		Controller->Possess(W.Diver);
		if (!DiveAt(*this, W, Chunk))
		{
			continue;
		}
		UTEST_EQUAL(TEXT("at the berth's stop"), W.State->Stop, Stop);
		if (W.Diver->IsLampOn())
		{
			W.Diver->ToggleLamp();
		}
		const FDeepSerpentCave* Cave = String->SerpentCave(Stop);
		UTEST_NOT_NULL(TEXT("the berth's room"), Cave);
		const FDeepMonsterTuning Monster = UDeepMonsterSettings::Get().GetTuning(Stop);

		// The diver over a pile's first piece (the relic at the last berth), held there.
		FVector DiverAt = W.Sub->GetAnchorLocation();
		for (const ADeepSalvage* Piece : Salvage(W.World))
		{
			const bool bRelic = Piece->GetKind().Size == EDeepSalvageSize::Relic;
			if (bRelic == (Stop == CS.Berths.Num()))
			{
				DiverAt = Piece->GetActorLocation() + FVector(0.0, 0.0, 120.0);
				break;
			}
		}
		// The serpent: a cheat's where it has none yet (it comes from stop 2 on its own).
		if (!Brain->GetSerpent())
		{
			Brain->SpawnSerpent(/*bForced*/ true);
		}
		ADeepSerpent* Serpent = Brain->GetSerpent();
		ADeepSerpentController* Mind = Brain->GetSerpentController();
		UTEST_TRUE(TEXT("the serpent and its mind"), Serpent && Mind);
		Mind->ForceState(EDeepMonsterState::Lurk);

		struct FPhase
		{
			int32 Steps = 0;
			int32 Spheres = 0;
			int32 InRock = 0;
			int32 Near = 0;
			int32 Dry = 0;
			double SocketSumCm = 0.0;
			double SweptRad = 0.0;
			FString FirstFail;
		};
		FVector LurkCentre;
		const float LurkCm = String->SerpentLurkRadiusCm(Stop, Monster.LurkRadiusM * 100.f, &LurkCentre);
		auto Run = [&](FPhase& Phase, float Seconds, TFunctionRef<bool()> Done, float Noise)
		{
			double LastAz = 0.0;
			for (int32 Step = 0; Step < FMath::CeilToInt(Seconds / Dt); ++Step)
			{
				W.Diver->SetActorLocation(DiverAt, false, nullptr, ETeleportType::TeleportPhysics);
				W.World->TimeSeconds += Dt;
				if (Noise > 0.f)
				{
					Brain->AddNoise(Noise);
				}
				Brain->Step(Dt);
				if (Perception)
				{
					Perception->Tick(Dt);
				}
				Mind->StepMonster(Dt);
				W.Run->StepRun(Dt);
				W.Diver->GetTether()->UpdateTether(Dt);
				Serpent->Tick(Dt);
				++Phase.Steps;
				const FVector Head = Serpent->GetHeadLocation();
				const FVector FromSocket = Head - LurkCentre;
				Phase.SocketSumCm += FVector2D(FromSocket.X, FromSocket.Y).Size();
				const double Az = FMath::Atan2(FromSocket.Y, FromSocket.X);
				Phase.SweptRad += Step > 0 ? FMath::Abs(FMath::UnwindRadians(Az - LastAz)) : 0.0;
				LastAz = Az;
				Phase.Dry += InWater(*Cave, Serpent->GetActorLocation()) ? 0 : 1;
				if (Step % 3 == 0)
				{
					TArray<FSphere> Spheres;
					Serpent->GetDrawnSpheres(Spheres);
					for (const FSphere& S : Spheres)
					{
						++Phase.Spheres;
						if (SphereInRock(*String, S.Center, float(S.W)))
						{
							++Phase.InRock;
							if (Phase.FirstFail.IsEmpty())
							{
								Phase.FirstFail = FString::Printf(TEXT(", first in rock: %s at %.1f s, a %.2f m sphere at %s"),
									*UDeepMonsterRules::StateName(Mind->GetState()), Step * Dt, S.W / 100.0, *S.Center.ToCompactString());
							}
						}
						else if (SphereInRock(*String, S.Center, float(S.W) + 100.f))
						{
							++Phase.Near;
						}
					}
				}
				if (Done())
				{
					break;
				}
			}
		};
		auto Report = [&](const TCHAR* Name, const FPhase& Phase)
		{
			AddInfo(FString::Printf(TEXT("DEEPSERPENTCAVE %s %s: %.1f s, %.1f m from the lurk centre on average (lurk %.1f m), %.0f deg round it; %d body spheres, %d in rock, %d within 1 m of it; head out of the water %d%s"),
				*Chunk.ToString(), Name, Phase.Steps * Dt, Phase.Steps > 0 ? Phase.SocketSumCm / Phase.Steps / 100.0 : 0.0, LurkCm / 100.f,
				FMath::RadiansToDegrees(Phase.SweptRad), Phase.Spheres, Phase.InRock, Phase.Near, Phase.Dry, *Phase.FirstFail));
			TestTrue(FString::Printf(TEXT("%s %s: body sampled"), *Chunk.ToString(), Name), Phase.Spheres > 0);
			TestEqual(FString::Printf(TEXT("%s %s: drawn body spheres in rock"), *Chunk.ToString(), Name), Phase.InRock, 0);
			TestEqual(FString::Printf(TEXT("%s %s: path points out of the water"), *Chunk.ToString(), Name), Phase.Dry, 0);
		};

		// Lurk: round its circle by the cavern's Serpent socket, not round the line.
		FPhase Lurk;
		Run(Lurk, 40.f, [] { return false; }, 0.f);
		Report(TEXT("lurk"), Lurk);
		const double MeanCm = Lurk.SocketSumCm / FMath::Max(1, Lurk.Steps);
		TestEqual(TEXT("it stayed in Lurk"), Mind->GetState(), EDeepMonsterState::Lurk);
		TestTrue(FString::Printf(TEXT("%s: it circles by the socket (%.1f m from the lurk centre on average, lurk %.1f m)"), *Chunk.ToString(), MeanCm / 100.0, LurkCm / 100.f),
			MeanCm < LurkCm + 500.0);
		TestTrue(FString::Printf(TEXT("%s: it goes round (%.0f deg)"), *Chunk.ToString(), FMath::RadiansToDegrees(Lurk.SweptRad)), Lurk.SweptRad > 0.5 * PI);

		// The pass: the first sight, laid in the cavern.
		FPhase Pass;
		Mind->ForceState(EDeepMonsterState::Lurk);
		UTEST_TRUE(TEXT("the pass starts"), Mind->StartFirstSight());
		Run(Pass, Monster.PassMaxS + 2.f, [&] { return Mind->GetState() != EDeepMonsterState::Pass; }, 0.f);
		Report(TEXT("pass"), Pass);
		TestTrue(FString::Printf(TEXT("%s: the pass ends before its time-out (%.1f s of %.0f)"), *Chunk.ToString(), Pass.Steps * Dt, Monster.PassMaxS),
			Pass.Steps * Dt < Monster.PassMaxS - 0.05f);
		Mind->ForceState(EDeepMonsterState::Lurk);
		// Stalk to the bite: noisy, it comes in on the line and bites it.
		FPhase Stalk;
		Mind->ForceState(EDeepMonsterState::Stalk);
		Run(Stalk, 120.f, [&] { return Mind->GetState() == EDeepMonsterState::Bite; }, Monster.StalkNoise);
		Report(TEXT("stalk"), Stalk);
		TestEqual(FString::Printf(TEXT("%s: it reaches the line and bites (%.1f m off it at the end)"), *Chunk.ToString(), Mind->GetTetherDistanceM()), Mind->GetState(), EDeepMonsterState::Bite);

		Mind->ForceState(EDeepMonsterState::Lurk);
	}
	return true;
}


// ---------------------------------------------------------------- D-070: the direct drive and the pilot's view in the caves

namespace DeepCaveDiveTest
{
	/** Holds the direct drive's keys (ahead, turn, down, strafe) as the pilot would. */
	static void Hold(const FDiveWorld& W, float Surge, float Turn = 0.f, float Heave = 0.f, float Strafe = 0.f)
	{
		FDeepDriveInput In;
		In.Surge = Surge;
		In.Turn = Turn;
		In.Heave = Heave;
		In.Strafe = Strafe;
		W.Run->SetDriveHold(In);
	}

	/** The hull's speed along its heading, from the server's motion (m/s). */
	static float SurgeMps(const FDiveWorld& W)
	{
		const FDeepPilotMotion& M = W.Run->GetPilotMotion();
		return FVector2D::DotProduct(M.VelocityMps, UDeepPilotRules::Ahead(M.YawDeg));
	}

	/** Places the hull on a path sample, heading along the path, unparked, drive keys let go. */
	static void PlaceDirect(const FDiveWorld& W, const FDeepCavePose& Pose)
	{
		W.PlaceAt(Pose);
		Hold(W, 0.f);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDirectDriveTest, "Deep.World.CaveDive.Direct.DrivesOutOfS", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDirectDriveTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W(false, false);
	const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	W.Run->TakeHelm();
	TestTrue(TEXT("the helm drives direct by default"), W.Run->IsDirectDrive());
	const FDeepCavePose Start = W.Pose();
	// W held: the berth lets go and the hull drives out along S toward A, never into rock.
	Hold(W, 1.f);
	W.Fly(5.f);
	const FDeepCavePose Out = W.Pose();
	AddInfo(FString::Printf(TEXT("W held 5 s: from %s to %s at %.2f m/s, damage %d"), *Describe(Start), *Describe(Out), W.State->HelmSpeedMps, W.State->HullDamage));
	TestFalse(TEXT("unparked by W"), W.State->bParked);
	TestTrue(TEXT("driven ahead (+X) at least 12 m"), Out.LocationCm.X - Start.LocationCm.X > 1200.0);
	TestTrue(TEXT("at full ahead"), FMath::IsNearlyEqual(W.State->HelmSpeedMps, UDeepTuningSettings::Get().Drive.AheadSpeedMps, 0.1f));
	TestFalse(TEXT("never inside rock"), String->OverlapsHull(W.Sub->GetSolidBoundsLocal(false), Out.LocationCm, Out.YawDeg));
	// Let go: nothing latches, the hull is at rest within 1.5 s and a few metres.
	Hold(W, 0.f);
	W.Fly(1.5f);
	const FDeepCavePose Rest = W.Pose();
	AddInfo(FString::Printf(TEXT("let go: coasted %.2f m, speed %.3f m/s"), FVector::Dist(Rest.LocationCm, Out.LocationCm) / 100.0, W.State->HelmSpeedMps));
	TestTrue(TEXT("at rest 1.5 s after letting go"), W.State->HelmSpeedMps < 0.05f);
	TestTrue(TEXT("coasted under 3.5 m"), FVector::Dist(Rest.LocationCm, Out.LocationCm) < 350.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDirectSlideTest, "Deep.World.CaveDive.Direct.SlidesAlongRock", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDirectSlideTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W(false, false);
	const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	// In A (where the knock test finds its floor), half ahead and C held: the hull reaches the floor and slides along
	// it for a second, keeping most of its way.
	const int32 Sample = UDeepCaveStringRules::FindPathSample(String->GetPath(), String->GetPathMetres(), TEXT("A"), 10.f);
	if (!TestTrue(TEXT("a sample in A"), String->GetPathPoses().IsValidIndex(Sample)))
	{
		return false;
	}
	PlaceDirect(W, String->GetPathPoses()[Sample]);
	const float Want = UDeepTuningSettings::Get().Drive.AheadSpeedMps * 0.5f;
	Hold(W, 0.5f, 0.f, 1.f);
	float FirstTouchS = -1.f;
	float SlowestOnRock = 1000.f;
	int32 Touching = 0;
	for (float T = 0.f; T < 30.f && Touching < 20; T += 0.05f)
	{
		const float ZBefore = float(W.Pose().LocationCm.Z);
		W.Fly(0.05f);
		const bool bOnRock = String->OverlapsHull(W.Sub->GetSolidBoundsLocal(false), W.Pose().LocationCm, W.Pose().YawDeg, 5.f) && FMath::Abs(float(W.Pose().LocationCm.Z) - ZBefore) < 2.f;
		if (bOnRock)
		{
			FirstTouchS = FirstTouchS < 0.f ? T : FirstTouchS;
			// Give the drive a moment after the first touch, then watch the slide.
			if (T > FirstTouchS + 0.5f)
			{
				SlowestOnRock = FMath::Min(SlowestOnRock, SurgeMps(W));
				++Touching;
			}
		}
	}
	AddInfo(FString::Printf(TEXT("on the floor from %.2f s: %d steps sliding, slowest %.2f m/s of %.2f, damage %d"), FirstTouchS, Touching, SlowestOnRock, Want, W.State->HullDamage));
	TestTrue(TEXT("the hull reached the floor"), FirstTouchS >= 0.f && Touching > 10);
	TestTrue(TEXT("sliding keeps most of the speed along the rock"), SlowestOnRock >= Want * 0.7f);
	TestFalse(TEXT("on the rock, not in it"), String->OverlapsHull(W.Sub->GetSolidBoundsLocal(false), W.Pose().LocationCm, W.Pose().YawDeg));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDirectDamageTest, "Deep.World.CaveDive.Direct.DamageBySpeed", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDirectDamageTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W(false, false);
	const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	const int32 Sample = UDeepCaveStringRules::FindPathSample(String->GetPath(), String->GetPathMetres(), TEXT("A"), 10.f);
	if (!TestTrue(TEXT("a sample in A"), String->GetPathPoses().IsValidIndex(Sample)))
	{
		return false;
	}
	// The same drop onto A's floor at a slow and at a full sink: the dent on touching grows with the speed into the rock.
	const auto DentOnTouch = [&](float Heave, float& OutImpactMps)
	{
		PlaceDirect(W, String->GetPathPoses()[Sample]);
		W.State->HullDamage = 0;
		Hold(W, 0.f, 0.f, Heave);
		OutImpactMps = 0.f;
		for (float T = 0.f; T < 30.f; T += 0.05f)
		{
			const float Before = W.Run->GetPilotMotion().HeaveMps;
			W.Fly(0.05f);
			if (W.Run->GetPilotMotion().HeaveMps < Before - 0.3f || W.State->HullDamage > 0)
			{
				OutImpactMps = Before;
				break;
			}
		}
		W.Fly(0.3f);
		const int32 Dent = W.State->HullDamage;
		Hold(W, 0.f);
		return Dent;
	};
	float SlowMps = 0.f;
	float FastMps = 0.f;
	const int32 Slow = DentOnTouch(0.5f, SlowMps);
	const int32 Fast = DentOnTouch(1.f, FastMps);
	AddInfo(FString::Printf(TEXT("onto the floor at %.2f m/s: %d dents; at %.2f m/s: %d dents"), SlowMps, Slow, FastMps, Fast));
	TestTrue(TEXT("a slow touch is a scrape: no dent at once"), Slow == 0);
	TestTrue(TEXT("a full sink into the rock dents"), Fast > 0);
	TestTrue(TEXT("the dent grows with the speed into the rock"), Fast > Slow);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDirectKnockTest, "Deep.World.CaveDive.Direct.KnockKeepsDriving", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDirectKnockTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W(false, false);
	const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	const int32 Sample = UDeepCaveStringRules::FindPathSample(String->GetPath(), String->GetPathMetres(), TEXT("A"), 10.f);
	if (!TestTrue(TEXT("a sample in A"), String->GetPathPoses().IsValidIndex(Sample)))
	{
		return false;
	}
	// A full sink onto the floor with W held: the knock dents, the drive goes on along the floor.
	PlaceDirect(W, String->GetPathPoses()[Sample]);
	Hold(W, 1.f, 0.f, 1.f);
	for (float T = 0.f; T < 30.f && W.State->HullDamage == 0; T += 0.05f)
	{
		W.Fly(0.05f);
	}
	TestTrue(TEXT("the knock dents"), W.State->HullDamage > 0);
	W.Fly(1.f);
	AddInfo(FString::Printf(TEXT("after the knock: hold %.1f, ahead %.2f m/s, damage %d"), W.Run->GetDriveHold().Surge, SurgeMps(W), W.State->HullDamage));
	TestEqual(TEXT("the throttle is not tripped: W is still held"), W.Run->GetDriveHold().Surge, 1.f);
	TestTrue(TEXT("still driving ahead after the knock"), SurgeMps(W) > 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDirectTurnTest, "Deep.World.CaveDive.Direct.TurnIntoRock", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDirectTurnTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W(false, false);
	const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	const FBox Box = W.Sub->GetSolidBoundsLocal(false);
	// In S's cavern, strafed hard against the port wall, then turned to port: the bow swings into the rock.
	const int32 Sample = UDeepCaveStringRules::FindPathSample(String->GetPath(), String->GetPathMetres(), TEXT("S"), 6.f);
	if (!TestTrue(TEXT("a sample in S"), String->GetPathPoses().IsValidIndex(Sample)))
	{
		return false;
	}
	PlaceDirect(W, String->GetPathPoses()[Sample]);
	Hold(W, 0.f, 0.f, 0.f, -1.f);
	bool bTouched = false;
	for (float T = 0.f; T < 30.f && !bTouched; T += 0.05f)
	{
		W.Fly(0.05f);
		bTouched = String->OverlapsHull(Box, W.Pose().LocationCm, W.Pose().YawDeg, 5.f);
	}
	if (!TestTrue(TEXT("strafed to the wall"), bTouched))
	{
		return false;
	}
	Hold(W, 0.f, -1.f, 0.f, -0.5f);
	const FDeepCavePose Before = W.Pose();
	bool bEverInRock = false;
	for (float T = 0.f; T < 1.5f; T += 0.05f)
	{
		W.Fly(0.05f);
		bEverInRock = bEverInRock || String->OverlapsHull(Box, W.Pose().LocationCm, W.Pose().YawDeg);
	}
	const FDeepCavePose After = W.Pose();
	const float Turned = FMath::Abs(FRotator::NormalizeAxis(After.YawDeg - Before.YawDeg));
	AddInfo(FString::Printf(TEXT("against the wall at %s, turned %.1f deg to %s, pushed %.0f cm"), *Describe(Before), Turned, *Describe(After), FVector::Dist2D(Before.LocationCm, After.LocationCm)));
	TestTrue(TEXT("the turn into rock is not refused (15 deg or more in 1.5 s)"), Turned >= 15.f);
	TestFalse(TEXT("pushed clear, never in the rock"), bEverInRock);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveDirectSinkTest, "Deep.World.CaveDive.Direct.SinksToC1", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveDirectSinkTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W(false, false);
	const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	// As S13-caves: one gate, at the top of C1's sink, C held in 1 s runs until the gauge reads 285, let go, 20 s.
	W.State->HullRating = 2;
	const int32 Sample = UDeepCaveStringRules::FindPathSample(String->GetPath(), String->GetPathMetres(), TEXT("C1"), 31.5f);
	if (!TestTrue(TEXT("a sample at the top of C1's sink"), String->GetPathPoses().IsValidIndex(Sample)))
	{
		return false;
	}
	PlaceDirect(W, String->GetPathPoses()[Sample]);
	Hold(W, 0.f, 0.f, 1.f);
	for (int32 Run = 0; Run < 120 && W.State->SubDepthM < 285.f; ++Run)
	{
		for (int32 Step = 0; Step < 10; ++Step)
		{
			W.Run->StepRun(0.1f);
		}
	}
	Hold(W, 0.f);
	for (int32 Step = 0; Step < 200; ++Step)
	{
		W.Run->StepRun(0.1f);
	}
	AddInfo(FString::Printf(TEXT("C1: reads %.2f m, parked %d, stop %d, damage %d"), W.State->SubDepthM, W.State->bParked, W.State->Stop, W.State->HullDamage));
	TestTrue(TEXT("parked"), W.State->bParked);
	TestEqual(TEXT("at C1, stop 2"), W.State->Stop, 2);
	TestEqual(TEXT("reads 300 m"), W.State->SubDepthM, 300.f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveChaseViewTest, "Deep.World.CaveDive.View.ChaseClear", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveChaseViewTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W(false, false);
	const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	W.Run->TakeHelm();
	W.Sub->SetHelmCockpit(false);
	const FBox Hull = W.Sub->GetSolidBoundsLocal(false);
	const TArray<FDeepCavePose>& Poses = String->GetPathPoses();
	UCameraComponent* Camera = W.Sub->GetHelmCamera();
	// Every 10 m from S to E: the hull driven the last 12 m along the path to the sample (its trail), then held there
	// for 1.5 s while the view settles. The chase view is out of the rock and out of the hull, and sees the hull.
	int32 Checks = 0;
	int32 InRock = 0;
	int32 InHull = 0;
	int32 Blind = 0;
	int32 Astern = 0;
	double NearestM = 1000.0;
	for (int32 Index = 48; Index < Poses.Num(); Index += 40)
	{
		W.Sub->ResetHelmView();
		for (int32 Back = Index - 48; Back <= Index; Back += 4)
		{
			W.PlaceAt(Poses[Back]);
			W.TickSub();
		}
		for (int32 Settle = 0; Settle < 45; ++Settle)
		{
			W.TickSub();
		}
		++Checks;
		const FVector Cam = Camera->GetComponentLocation();
		const FVector HullCentre = W.Sub->GetHullCentreWorld();
		const bool bInRock = String->OverlapsBoxWorld(Cam, FVector(20.0), FQuat::Identity);
		const bool bInHull = Hull.ExpandBy(10.0).IsInside(W.Sub->GetActorTransform().InverseTransformPosition(Cam));
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DeepTestChaseSight), true, W.Sub);
		const FVector HullTop = W.Sub->GetActorTransform().TransformPosition(FVector(Hull.GetCenter().X, Hull.GetCenter().Y, Hull.Max.Z));
		const bool bBlind = W.World->LineTraceTestByObjectType(Cam, HullTop, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
		const bool bAstern = FVector::DotProduct((Cam - HullCentre).GetSafeNormal2D(), W.Sub->GetActorForwardVector().GetSafeNormal2D()) < 0.0;
		InRock += bInRock ? 1 : 0;
		InHull += bInHull ? 1 : 0;
		Blind += bBlind ? 1 : 0;
		Astern += bAstern ? 1 : 0;
		NearestM = FMath::Min(NearestM, FVector::Dist(Cam, HullCentre) / 100.0);
		if ((bInRock || bInHull || bBlind) && InRock + InHull + Blind <= 6)
		{
			AddWarning(FString::Printf(TEXT("sample %d (%s %s): camera %s, rock %d hull %d blind %d"), Index, *String->GetPath()[Index].Chunk.ToString(),
				*String->GetPath()[Index].Region.ToString(), *Cam.ToString(), bInRock, bInHull, bBlind));
		}
	}
	AddInfo(FString::Printf(TEXT("DEEPVIEW chase: %d samples, in rock %d, in hull %d, hull hidden %d, astern %d, nearest %.1f m"), Checks, InRock, InHull, Blind, Astern, NearestM));
	TestTrue(TEXT("sampled the whole path"), Checks > 20);
	TestEqual(TEXT("the chase view is never in rock"), InRock, 0);
	TestEqual(TEXT("the chase view is never in the hull"), InHull, 0);
	TestTrue(TEXT("the chase view sees the hull (at least 95% of samples)"), Blind * 20 <= Checks);
	TestTrue(TEXT("the chase view sits astern (at least 80% of samples)"), Astern * 5 >= Checks * 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveCockpitViewTest, "Deep.World.CaveDive.View.CockpitLevel", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveCockpitViewTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W(false, false);
	const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	W.Run->TakeHelm();
	const int32 Sample = UDeepCaveStringRules::FindPathSample(String->GetPath(), String->GetPathMetres(), TEXT("A"), 10.f);
	PlaceDirect(W, String->GetPathPoses()[Sample]);
	UCameraComponent* Camera = W.Sub->GetHelmCamera();
	// The chase view first (the default), looking ahead along the heading at rest.
	TestFalse(TEXT("the helm starts in the chase view"), W.Sub->IsHelmCockpit());
	for (int32 Tick = 0; Tick < 30; ++Tick)
	{
		W.TickSub();
	}
	const FVector Ahead = W.Sub->GetActorForwardVector();
	TestTrue(TEXT("the chase view looks ahead"), FVector::DotProduct(Camera->GetForwardVector(), Ahead) > 0.7);
	// V: the cockpit, level (0 to -5 deg) and looking ahead.
	W.Sub->ToggleHelmView();
	W.TickSub();
	const FRotator Level = Camera->GetComponentRotation();
	AddInfo(FString::Printf(TEXT("cockpit: pitch %.2f, yaw %.2f (hull yaw %.2f)"), Level.Pitch, Level.Yaw, W.Sub->GetActorRotation().Yaw));
	TestTrue(TEXT("the cockpit view is level (0 to -5 deg)"), Level.Pitch <= 0.5f && Level.Pitch >= -5.5f);
	TestTrue(TEXT("the cockpit view looks ahead"), FVector::DotProduct(Camera->GetForwardVector(), Ahead) > 0.99);
	// The mouse looks round within limits and the view eases back after a moment.
	W.Sub->AddHelmLook(500.f, 0.f);
	W.TickSub();
	const float Looked = FRotator::NormalizeAxis(Camera->GetComponentRotation().Yaw - W.Sub->GetActorRotation().Yaw);
	TestEqual(TEXT("the look stops at the cockpit's limit"), Looked, UDeepTuningSettings::Get().HelmView.CockpitLookYawDeg, 1.f);
	for (int32 Tick = 0; Tick < 150; ++Tick)
	{
		W.TickSub();
	}
	const float Back = FRotator::NormalizeAxis(Camera->GetComponentRotation().Yaw - W.Sub->GetActorRotation().Yaw);
	AddInfo(FString::Printf(TEXT("looked %.1f deg, 5 s later %.1f deg"), Looked, Back));
	TestTrue(TEXT("the view eases back ahead"), FMath::Abs(Back) < 2.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveIdleTest, "Deep.World.CaveDive.Idle.HoldsStation", DEEP_CAVEDIVE_TEST_FLAGS)

bool FDeepCaveIdleTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveDiveTest;
	FCVarScope Still(TEXT("Deep.World.Still"), 1);
	FCVarScope Caves(TEXT("Deep.World.Caves"), 1);
	const FDiveWorld W(false, false);
	const ADeepCaveString* String = W.Valid() ? W.String() : nullptr;
	if (!TestNotNull(TEXT("the chunk string is built"), String))
	{
		return false;
	}
	// D-070, Quinn's idle: driven out of S and let go, the sub comes to rest and holds station there for 30 s.
	W.Run->TakeHelm();
	Hold(W, 1.f);
	W.Fly(3.f);
	Hold(W, 0.f);
	W.Fly(2.f);
	const FDeepCavePose Stopped = W.Pose();
	const FBox BoxBefore = W.Sub->GetSolidBoundsLocal(false);
	float MostIdle = 0.f;
	FVector MostVisualCm = FVector::ZeroVector;
	for (float T = 0.f; T < 30.f; T += 0.05f)
	{
		W.Fly(0.05f);
		static_cast<AActor*>(W.Sub)->Tick(0.05f);
		if (W.Sub->GetIdleAmount() > MostIdle)
		{
			MostIdle = W.Sub->GetIdleAmount();
		}
		if (W.Sub->GetHullVisual()->GetRelativeLocation().Size() > MostVisualCm.Size())
		{
			MostVisualCm = W.Sub->GetHullVisual()->GetRelativeLocation();
		}
	}
	const FDeepCavePose Held = W.Pose();
	const double DriftCm = FVector::Dist(Held.LocationCm, Stopped.LocationCm);
	AddInfo(FString::Printf(TEXT("held 30 s: drifted %.2f cm, %.2f deg; idle amount up to %.2f, visual hull moved up to %s cm"), DriftCm,
		FMath::Abs(FRotator::NormalizeAxis(Held.YawDeg - Stopped.YawDeg)), MostIdle, *MostVisualCm.ToString()));
	TestTrue(TEXT("holds station: within 3 cm after 30 s"), DriftCm < 3.0);
	TestTrue(TEXT("holds its heading"), FMath::Abs(FRotator::NormalizeAxis(Held.YawDeg - Stopped.YawDeg)) < 0.1f);
	TestTrue(TEXT("at rest the idle comes in"), MostIdle > 0.5f);
	// Visual only: the sweep box and the hull's pose never see the bob.
	TestTrue(TEXT("the visual hull moves"), !MostVisualCm.IsNearlyZero(0.01) || !W.Sub->GetHullVisual()->GetRelativeRotation().IsNearlyZero(0.001f));
	TestTrue(TEXT("the sweep box is unchanged by the idle"), W.Sub->GetSolidBoundsLocal(false).Equals(BoxBefore, 0.01));
	const FVector Expected = String->GetActorTransform().TransformPosition(Held.LocationCm);
	TestTrue(TEXT("the hull actor (the sweep's pose) stays on its pose"), FVector::Dist(W.Sub->GetActorLocation(), Expected) < 1.0);
	return true;
}

#endif
