// Tests for Deep.World.Caves (cave-dive M1): the baked chunk string S, A, T1, C1, B, E is the rock. A run starts parked at
// S's berth, the real hull box flies the exported path clean, the helm's sweep stops it on rock with a dent and a scrape,
// it parks on C1's berth and stays there, and with the switch off the shaft world is as before.

#include "CoreMinimal.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "Misc/AutomationTest.h"
#include "Private/Tests/DeepTestCVar.h"
#include "Sim/DeepCaveStringRules.h"
#include "Sim/DeepPilotRules.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepArena.h"
#include "World/DeepCaveString.h"
#include "World/DeepSalvage.h"
#include "Components/CapsuleComponent.h"
#include "World/DeepShaft.h"
#include "World/DeepSubmarine.h"
#include "World/DeepSubRoom.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_CAVEDIVE_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepCaveDiveTest
{
	using DeepTest::FCVarScope;

	/** A game world with the arena and one diver in the room, as a run starts (as Deep.World.Still's). */
	struct FDiveWorld
	{
		UWorld* World = nullptr;
		ADeepGameMode* GameMode = nullptr;
		ADeepGameState* State = nullptr;
		ADeepSubmarine* Sub = nullptr;
		ADeepDiver* Diver = nullptr;
		UDeepRunComponent* Run = nullptr;

		FDiveWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			GameMode = World->SpawnActor<ADeepGameMode>();
			Sub = GameMode ? GameMode->EnsureArena() : nullptr;
			if (GameMode)
			{
				GameMode->StartPlay();
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
	 * The stop's salvage sits at its cavern's sockets (by X and Y), rests on rock (rock within a few cm under its bottom)
	 * and is clear of it (its bounds, lifted by the sink, overlap none). Each within the tether's reach of the anchor.
	 */
	static void CheckCavernSalvage(FAutomationTestBase& Test, const FDiveWorld& W, int32 Stop)
	{
		const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
		const FDeepCaveStringTuning& CS = Tuning.CaveString;
		const ADeepCaveString* String = W.String();
		const FName Cavern = CS.Berths[Stop - 1];
		const TArray<ADeepSalvage*> Pieces = Salvage(W.World);
		int32 Slots = 0;
		const FVector Anchor = W.Sub->GetAnchorLocation();
		const float TetherCm = UDeepRules::TetherLengthM(Stop, Tuning.Tether) * UDeepRules::CmPerM;
		for (const FDeepCaveSalvageSlot& Slot : CS.Salvage)
		{
			if (Slot.Chunk != Cavern)
			{
				continue;
			}
			++Slots;
			FVector Socket;
			if (!Test.TestTrue(FString::Printf(TEXT("%s.%s exists"), *Slot.Chunk.ToString(), *Slot.Socket.ToString()), String->SocketWorld(Slot.Chunk, Slot.Socket, Socket)))
			{
				continue;
			}
			const ADeepSalvage* const* Found = Pieces.FindByPredicate([&Socket](const ADeepSalvage* Piece)
			{
				return FVector::Dist2D(Piece->GetActorLocation(), Socket) < 1.0;
			});
			const FString Name = FString::Printf(TEXT("%s.%s"), *Slot.Chunk.ToString(), *Slot.Socket.ToString());
			if (!Test.TestNotNull(FString::Printf(TEXT("a piece at %s"), *Name), Found))
			{
				continue;
			}
			const ADeepSalvage& Piece = **Found;
			Test.TestEqual(FString::Printf(TEXT("%s is its slot's kind"), *Name), static_cast<int32>(Piece.GetKind().Size), static_cast<int32>(Slot.Size));
			const FBox Box = PieceBounds(Piece);
			double Highest = 0.0;
			double Lowest = 0.0;
			const bool bRock = String->FootprintRock(Box, float(Box.GetSize().Z), 30.f, Highest, Lowest) > 0;
			const double Gap = bRock ? Box.Min.Z - Highest : -1.0;
			const bool bClear = !DeepArena::SalvageInRock(*String, Piece, CS.SalvageSinkCm + 1.0);
			const double Reach = FVector::Dist(Anchor, Piece.GetActorLocation());
			Test.AddInfo(FString::Printf(TEXT("DEEPCAVESALVAGE %s: socket z %.0f, piece z %.0f, bottom %.1f cm above rock (footprint spans %.0f cm), %s, %.1f m from the anchor (tether %.0f m); piece at %s, anchor at %s, string yaw %.1f"),
				*Name, Socket.Z, Piece.GetActorLocation().Z, Gap, bRock ? Highest - Lowest : 0.0, bClear ? TEXT("clear") : TEXT("IN ROCK"), Reach / UDeepRules::CmPerM, TetherCm / UDeepRules::CmPerM,
				*Piece.GetActorLocation().ToCompactString(), *Anchor.ToCompactString(), String->GetActorRotation().Yaw));
			// Not floating: 10 cm lower, its own shapes would be in rock.
			Test.TestTrue(FString::Printf(TEXT("%s rests on rock (10 cm lower is in it)"), *Name), bRock && DeepArena::SalvageInRock(*String, Piece, -10.0));
			Test.TestTrue(FString::Printf(TEXT("%s is clear of rock"), *Name), bClear);
			Test.TestTrue(FString::Printf(TEXT("%s is within the tether from the anchor"), *Name), Reach < TetherCm);
			Test.TestTrue(FString::Printf(TEXT("%s is within 3 m of its socket"), *Name), FMath::Abs(Piece.GetActorLocation().Z - Socket.Z) < 300.0);
		}
		Test.TestEqual(FString::Printf(TEXT("stop %d: one piece per %s slot, and no others"), Stop, *Cavern.ToString()), Pieces.Num(), Slots);
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
	return true;
}

#endif
