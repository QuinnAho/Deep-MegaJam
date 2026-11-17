// Tests for Deep.World.Still (moving-sub step 3): the arena root stays at identity and the hull moves through a still
// world. The same route flown with the flag off and on must leave the hull in the same place relative to everything
// outside it; each stop has its own bed; the shaft is placed once; the airlock, resets and the room set still work.

#include "CoreMinimal.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Diver/DeepTetherComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Private/Tests/DeepTestCVar.h"
#include "ProceduralMeshComponent.h"
#include "Sim/DeepCaveRules.h"
#include "Sim/DeepMovingSubRules.h"
#include "Sim/DeepPilotRules.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepAbyssFloor.h"
#include "World/DeepArena.h"
#include "World/DeepArenaRoot.h"
#include "World/DeepCavern.h"
#include "Camera/CameraComponent.h"
#include "World/DeepDressing.h"
#include "World/DeepEnvironment.h"
#include "World/DeepSalvage.h"
#include "World/DeepShaft.h"
#include "World/DeepSubmarine.h"
#include "World/DeepSubRoom.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_STILL_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepStillTest
{
	using DeepTest::FCVarScope;

	/** A game world with the arena and one diver in the room, as a run starts (as Deep.World.Pilot's). */
	struct FStillWorld
	{
		UWorld* World = nullptr;
		ADeepGameMode* GameMode = nullptr;
		ADeepGameState* State = nullptr;
		ADeepSubmarine* Sub = nullptr;
		ADeepDiver* Diver = nullptr;
		UDeepRunComponent* Run = nullptr;

		FStillWorld()
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

		~FStillWorld()
		{
			if (World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		bool Valid() const { return GameMode && State && Sub && Diver && Run; }

		ADeepArenaRoot* Root() const { return DeepArena::FindArenaRoot(World); }

		template <typename T>
		T* First() const
		{
			for (TActorIterator<T> It(World); It; ++It)
			{
				return *It;
			}
			return nullptr;
		}

		/** The first ring piece hung from the root (not the relic): spawned in the same order with the flag off and on. */
		ADeepSalvage* RingPiece() const
		{
			const ADeepSalvage* Relic = DeepArena::FindRelic(World);
			for (TActorIterator<ADeepSalvage> It(World); It; ++It)
			{
				if (*It != Relic && It->GetAttachParentActor() == Root())
				{
					return *It;
				}
			}
			return nullptr;
		}

		TArray<ADeepAbyssFloor*> Floors() const
		{
			TArray<ADeepAbyssFloor*> Out;
			for (TActorIterator<ADeepAbyssFloor> It(World); It; ++It)
			{
				Out.Add(*It);
			}
			return Out;
		}

		/** Seated, unparked at a depth on the passage's centre, heading as parked, levers home (as Deep.World.Pilot's). */
		void HoldAt(float DepthM) const
		{
			if (!Run->IsSeated())
			{
				Run->TakeHelm();
			}
			const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
			Run->AllStop();
			Run->SetPilotHold(0.f, 0.f);
			State->SubDepthM = DepthM;
			State->SubOffsetM = UDeepCaveRules::CentreM(DepthM, Tuning.StopDepthsM(), Tuning.Cave);
			State->SubYawDeg = 0.f;
			State->bParked = false;
			DeepArena::ApplySubDepth(World);
		}

		/** Runs the helm and the park glide in 0.05 s steps. */
		void Fly(float Seconds) const
		{
			for (float T = 0.f; T < Seconds; T += 0.05f)
			{
				Run->StepRun(0.05f);
				State->StepParkGlide(0.05f);
			}
		}

		/** One frame of the sub's tick (what places the hull and the still world on every machine). */
		void TickSub() const
		{
			static_cast<AActor*>(Sub)->Tick(1.f / 30.f);
		}

		/** Out through the airlock by the valve (as Deep.World.SubHatch's). */
		bool OutByValve(float MaxSeconds = 4.f) const
		{
			Diver->TeleportDiver(DeepArena::EnsureRoom(World)->GetValveLocation() + FVector(-50.f, 0.f, 100.f));
			Run->Use();
			for (float T = 0.f; T < MaxSeconds && Run->GetState() == EDeepRunState::Aboard; T += 0.05f)
			{
				Run->StepRun(0.05f);
			}
			return Run->GetState() == EDeepRunState::Tethered;
		}

		void FinishAirlock() const
		{
			for (int32 Step = 0; Step < 100 && Run->IsInAirlock(); ++Step)
			{
				Run->StepRun(0.05f);
			}
		}
	};

	/** What the route checks at each point: the hull against the world outside it, and the sim's own numbers. */
	struct FCheck
	{
		FTransform HullToBed;
		FTransform HullToSalvage;
		FTransform HullToShaft;
		float DepthM = 0.f;
		FVector2D OffsetM = FVector2D::ZeroVector;
		float YawDeg = 0.f;
		float PenetrationM = 0.f;
		bool bTouching = false;
		FTransform Root;
		FTransform Hull;
		bool bComplete = false;
	};

	static FCheck Capture(const FStillWorld& W)
	{
		const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
		FCheck C;
		W.TickSub();
		const ADeepAbyssFloor* Bed = DeepArena::FindLoadedFloor(W.World);
		const ADeepSalvage* Piece = W.RingPiece();
		const ADeepShaft* Shaft = W.First<ADeepShaft>();
		C.Hull = W.Sub->GetActorTransform();
		C.Root = W.Root() ? W.Root()->GetActorTransform() : FTransform::Identity;
		C.HullToBed = Bed ? C.Hull.GetRelativeTransform(Bed->GetActorTransform()) : FTransform::Identity;
		C.HullToSalvage = Piece ? C.Hull.GetRelativeTransform(Piece->GetActorTransform()) : FTransform::Identity;
		C.HullToShaft = Shaft ? C.Hull.GetRelativeTransform(Shaft->GetActorTransform()) : FTransform::Identity;
		C.DepthM = W.State->SubDepthM;
		C.OffsetM = W.State->SubOffsetM;
		C.YawDeg = W.State->SubYawDeg;
		FVector RealMin, RealMax, BoxMin, BoxMax;
		DeepShaft::SubBoxM(W.Sub, RealMin, RealMax);
		UDeepPilotRules::TurnFreeBoxM(RealMin, RealMax, BoxMin, BoxMax);
		const FDeepCaveContact Contact = UDeepCaveRules::Contact(C.OffsetM, C.DepthM, BoxMin, BoxMax, Tuning.StopDepthsM(), UDeepCaveRules::MinRadiusM(RealMin, RealMax, Tuning.Cave), Tuning.Cave);
		C.PenetrationM = Contact.PenetrationM;
		C.bTouching = Contact.bTouching;
		C.bComplete = Bed && Piece && Shaft;
		return C;
	}

	/** Stop 1, then flown down the shaft with the throttle and rudder, then the Deep.GotoStop jump to stop 2. */
	static TArray<FCheck> FlyRoute(const FStillWorld& W)
	{
		const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
		TArray<FCheck> Out;
		Out.Add(Capture(W));
		W.HoldAt(Tuning.GetStop(1).SubDepthM + 50.f);
		W.Run->StepThrottle(1);
		W.Run->StepThrottle(1);
		W.Run->SetPilotHold(0.6f, 0.5f);
		W.Fly(3.f);
		Out.Add(Capture(W));
		W.GameMode->GotoStop(2);
		Out.Add(Capture(W));
		return Out;
	}

	static bool Near(const FTransform& A, const FTransform& B, double Cm = 1.0, double Deg = 0.1)
	{
		return FVector::Dist(A.GetLocation(), B.GetLocation()) <= Cm
			&& FMath::RadiansToDegrees(A.GetRotation().AngularDistance(B.GetRotation())) <= Deg;
	}

	static FString Describe(const FTransform& T)
	{
		return FString::Printf(TEXT("%s %s"), *T.GetLocation().ToString(), *T.Rotator().ToString());
	}
}

// ---------------------------------------------------------------- Deep.World.Still

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepStillRulesTest, "Deep.World.Still.Rules", DEEP_STILL_TEST_FLAGS)
bool FDeepStillRulesTest::RunTest(const FString& Parameters)
{
	using namespace DeepStillTest;
	const FDeepMovingSubTuning MovingSub;
	const FTransform Home(FRotator(0.f, 30.f, 0.f), FVector(1200.0, -800.0, 500.0));
	const double DatumCm = 0.0;

	// Parked on a stop's centre: the hull straight down at the stop's depth, its own heading.
	const FTransform Parked = UDeepMovingSubRules::StillHull(Home, FVector2D::ZeroVector, 100.f, 0.f, DatumCm);
	TestTrue(TEXT("Parked: over home at the stop's depth"), Parked.GetLocation().Equals(FVector(1200.0, -800.0, -10000.0), 0.01));
	TestTrue(TEXT("Parked: home's heading"), Parked.GetRotation().Equals(Home.GetRotation(), 1.e-5));
	TestTrue(TEXT("The stop frame is where the hull parks"), UDeepMovingSubRules::StillStopFrame(Home, 100.f, DatumCm).GetLocation().Equals(Parked.GetLocation(), 0.01));
	const FTransform Shaft = UDeepMovingSubRules::StillShaft(Home, DatumCm);
	TestTrue(TEXT("The shaft hangs from the datum over home"), Shaft.GetLocation().Equals(FVector(1200.0, -800.0, 0.0), 0.01) && Shaft.GetRotation().Equals(Home.GetRotation(), 1.e-5));

	// Anywhere: the hull sits against the stop's content exactly as D-022's moved root puts the content round the hull.
	const FTransform Content(FRotator(0.f, -15.f, 0.f), FVector(900.0, -300.0, -650.0));
	const float StopM = 300.f;
	const float DepthM = 337.5f;
	const FVector2D OffsetM(4.f, -2.5f);
	const float YawDeg = 27.f;
	FQuat Turn;
	const FVector RootAt = UDeepPilotRules::HangLocationCm(Home.GetLocation(), Home.GetRotation(), OffsetM, DepthM - StopM, YawDeg, Turn);
	const FTransform MovingContent = Content * FTransform(Turn, RootAt);
	const FTransform StillContent = Content * UDeepMovingSubRules::StillStopFrame(Home, StopM, DatumCm);
	const FTransform StillHull = UDeepMovingSubRules::StillHull(Home, OffsetM, DepthM, YawDeg, DatumCm);
	const FTransform Moving = Home.GetRelativeTransform(MovingContent);
	const FTransform Still = StillHull.GetRelativeTransform(StillContent);
	TestTrue(FString::Printf(TEXT("Hull against the content: D-022 %s, still %s"), *Describe(Moving), *Describe(Still)), Near(Moving, Still, 0.01, 0.001));
	TestTrue(TEXT("The still hull is at the absolute depth"), FMath::IsNearlyEqual(UDeepMovingSubRules::DepthFromZ(StillHull.GetLocation().Z, DatumCm), DepthM, 0.001f));

	// A stop's bed shows parked there and above it, never once the hull is through it.
	TestTrue(TEXT("Above the bed: shows"), UDeepMovingSubRules::StillBedShows(250.f, 300.f, MovingSub));
	TestTrue(TEXT("Parked at it: shows"), UDeepMovingSubRules::StillBedShows(300.f, 300.f, MovingSub));
	TestTrue(TEXT("Within BedShowsBelowStopM: shows"), UDeepMovingSubRules::StillBedShows(300.f + MovingSub.BedShowsBelowStopM * 0.5f, 300.f, MovingSub));
	TestFalse(TEXT("Below it: hidden"), UDeepMovingSubRules::StillBedShows(300.f + MovingSub.BedShowsBelowStopM + 0.5f, 300.f, MovingSub));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepStillSameGeometryTest, "Deep.World.Still.SameGeometry", DEEP_STILL_TEST_FLAGS)
bool FDeepStillSameGeometryTest::RunTest(const FString& Parameters)
{
	using namespace DeepStillTest;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	UTEST_TRUE(TEXT("At least two stops"), Tuning.NumStops() >= 2);
	FCVarScope Shaft(TEXT("Deep.Cave.Shaft"), 1);

	TArray<FCheck> Off;
	{
		FCVarScope Flag(TEXT("Deep.World.Still"), 0);
		FStillWorld W;
		UTEST_TRUE(TEXT("Flag off: world, game mode, state, sub, diver, run"), W.Valid());
		TestFalse(TEXT("Flag off: not still"), DeepArena::IsWorldStill(W.World));
		Off = FlyRoute(W);
	}
	TArray<FCheck> On;
	{
		FCVarScope Flag(TEXT("Deep.World.Still"), 1);
		FStillWorld W;
		UTEST_TRUE(TEXT("Flag on: world, game mode, state, sub, diver, run"), W.Valid());
		UTEST_TRUE(TEXT("Flag on: still"), DeepArena::IsWorldStill(W.World));
		On = FlyRoute(W);
	}
	UTEST_EQUAL(TEXT("Same number of checks"), On.Num(), Off.Num());

	const TCHAR* Names[] = { TEXT("stop 1"), TEXT("mid-descent"), TEXT("stop 2") };
	for (int32 I = 0; I < On.Num(); ++I)
	{
		const FCheck& A = Off[I];
		const FCheck& B = On[I];
		const TCHAR* At = Names[FMath::Min(I, 2)];
		TestTrue(FString::Printf(TEXT("%s: bed, ring piece and shaft found both ways"), At), A.bComplete && B.bComplete);
		TestTrue(FString::Printf(TEXT("%s: hull against the bed (off %s, on %s)"), At, *Describe(A.HullToBed), *Describe(B.HullToBed)), Near(A.HullToBed, B.HullToBed));
		TestTrue(FString::Printf(TEXT("%s: hull against a salvage piece (off %s, on %s)"), At, *Describe(A.HullToSalvage), *Describe(B.HullToSalvage)), Near(A.HullToSalvage, B.HullToSalvage));
		TestTrue(FString::Printf(TEXT("%s: hull against the shaft (off %s, on %s)"), At, *Describe(A.HullToShaft), *Describe(B.HullToShaft)), Near(A.HullToShaft, B.HullToShaft));
		TestEqual(FString::Printf(TEXT("%s: SubDepthM"), At), B.DepthM, A.DepthM, 0.01f);
		TestTrue(FString::Printf(TEXT("%s: SubOffsetM"), At), B.OffsetM.Equals(A.OffsetM, 0.01f));
		TestEqual(FString::Printf(TEXT("%s: SubYawDeg"), At), B.YawDeg, A.YawDeg, 0.1f);
		TestEqual(FString::Printf(TEXT("%s: wall contact"), At), B.bTouching, A.bTouching);
		TestEqual(FString::Printf(TEXT("%s: wall penetration"), At), B.PenetrationM, A.PenetrationM, 0.01f);
		TestTrue(FString::Printf(TEXT("%s: flag on, the root stays at identity"), At), Near(B.Root, FTransform::Identity, 0.01, 0.001));
		TestTrue(FString::Printf(TEXT("%s: flag on, the hull is at the absolute depth"), At),
			FMath::IsNearlyEqual(UDeepMovingSubRules::DepthFromZ(B.Hull.GetLocation().Z, Tuning.MovingSub.DepthDatumZM * UDeepRules::CmPerM), B.DepthM, 0.01f));
	}
	AddInfo(FString::Printf(TEXT("Mid-descent: depth %.2f m, offset %s m, yaw %.2f deg"), On[1].DepthM, *On[1].OffsetM.ToString(), On[1].YawDeg));
	TestTrue(TEXT("The flight turned and moved the sub (the route is not trivial)"), FMath::Abs(On[1].YawDeg) > 1.f && !On[1].OffsetM.IsNearlyZero(0.01f));
	TestTrue(TEXT("Flag on: the hull moved down between the stops"), On[0].Hull.GetLocation().Z - On[2].Hull.GetLocation().Z > 1000.0);
	TestTrue(TEXT("Flag off: the hull never moved"), Near(Off[0].Hull, Off[2].Hull, 0.01, 0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepStillAirlockTest, "Deep.World.Still.Airlock", DEEP_STILL_TEST_FLAGS)
bool FDeepStillAirlockTest::RunTest(const FString& Parameters)
{
	using namespace DeepStillTest;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FCVarScope Flag(TEXT("Deep.World.Still"), 1);
	FStillWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run"), W.Valid());
	UTEST_TRUE(TEXT("Still"), DeepArena::IsWorldStill(W.World));
	const FVector HullAtStop1 = W.Sub->GetActorLocation();

	W.GameMode->GotoStop(2);
	W.TickSub();
	const float StopM = Tuning.GetStop(2).SubDepthM;
	TestTrue(TEXT("The hull went down to stop 2"), HullAtStop1.Z - W.Sub->GetActorLocation().Z > (StopM - Tuning.GetStop(1).SubDepthM) * 100.f - 1.f);
	TestTrue(TEXT("Parked at stop 2: the hatch is open"), W.Sub->IsHatchOpen());

	TestTrue(TEXT("Out by the valve: tethered"), W.OutByValve());
	W.FinishAirlock();
	const FVector Hatch = W.Sub->GetHatchLocation();
	const FVector At = W.Diver->GetActorLocation();
	AddInfo(FString::Printf(TEXT("Hatch %s, diver %s"), *Hatch.ToString(), *At.ToString()));
	TestTrue(TEXT("Under the moved hull's hatch"), FVector::Dist2D(At, Hatch) < 300.0 && At.Z < Hatch.Z && At.Z > Hatch.Z - 1500.0);
	TestTrue(TEXT("Tethered to the moved hull's anchor"), W.Diver->GetTether()->GetAnchorLocation().Equals(W.Sub->GetAnchorLocation(), 1.0));
	TestTrue(TEXT("The diver's depth is the stop's"), W.Diver->GetDepthM() >= StopM - 1.f && W.Diver->GetDepthM() < StopM + 20.f);
	const ADeepAbyssFloor* Bed = DeepArena::FindLoadedFloor(W.World);
	TestTrue(TEXT("Stop 2's bed is loaded, shows and blocks"), Bed && Bed->BedStop == 2 && !Bed->IsHidden() && Bed->GetActorEnableCollision());

	W.Diver->TeleportDiver(Hatch - FVector(0.f, 0.f, 20.f));
	W.Run->StepRun(0.05f);
	TestEqual(TEXT("Up through the moved hatch: aboard"), W.Run->GetState(), EDeepRunState::Aboard);
	TestTrue(TEXT("Back in the room set"), DeepArena::EnsureRoom(W.World)->IsInRoom(W.Diver->GetActorLocation(), 100.0));
	W.FinishAirlock();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepStillBedsTest, "Deep.World.Still.BedsAndShaft", DEEP_STILL_TEST_FLAGS)
bool FDeepStillBedsTest::RunTest(const FString& Parameters)
{
	using namespace DeepStillTest;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FCVarScope Shaft(TEXT("Deep.Cave.Shaft"), 1);
	FCVarScope Flag(TEXT("Deep.World.Still"), 1);
	FStillWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run"), W.Valid());
	UTEST_TRUE(TEXT("Still"), DeepArena::IsWorldStill(W.World));
	const TArray<float> Stops = Tuning.StopDepthsM();
	const double DatumCm = Tuning.MovingSub.DepthDatumZM * UDeepRules::CmPerM;
	const FVector AnchorDrop = W.Sub->GetAnchorLocation() - W.Sub->GetActorLocation();

	// One bed per stop, each at its own stop's anchor.
	const TArray<ADeepAbyssFloor*> Floors = W.Floors();
	TestEqual(TEXT("One bed per stop"), Floors.Num(), Stops.Num());
	for (const ADeepAbyssFloor* Floor : Floors)
	{
		const FVector Want = UDeepMovingSubRules::StillStopFrame(W.Sub->GetHomeTransform(), Tuning.GetStop(Floor->BedStop).SubDepthM, DatumCm).GetLocation() + AnchorDrop;
		TestTrue(FString::Printf(TEXT("Bed %d at its stop's anchor (%s, want %s)"), Floor->BedStop, *Floor->GetActorLocation().ToString(), *Want.ToString()), Floor->GetActorLocation().Equals(Want, 1.0));
	}
	ADeepShaft* ShaftActor = W.First<ADeepShaft>();
	ADeepCavern* Cavern = W.First<ADeepCavern>();
	UTEST_NOT_NULL(TEXT("The shaft"), ShaftActor);
	const FTransform ShaftAt = UDeepMovingSubRules::StillShaft(W.Sub->GetHomeTransform(), DatumCm);

	// Each check: every bed, cave shell and shaft stretch shows by the rule for the hull's depth; the shaft never moved.
	auto CheckAt = [&](const TCHAR* Where)
	{
		W.TickSub();
		const float DepthM = W.State->SubDepthM;
		for (const ADeepAbyssFloor* Floor : Floors)
		{
			const bool bShow = UDeepMovingSubRules::StillBedShows(DepthM, Tuning.GetStop(Floor->BedStop).SubDepthM, Tuning.MovingSub);
			TestEqual(FString::Printf(TEXT("%s: bed %d shows"), Where, Floor->BedStop), !Floor->IsHidden(), bShow);
			TestEqual(FString::Printf(TEXT("%s: bed %d blocks"), Where, Floor->BedStop), Floor->GetActorEnableCollision(), bShow);
		}
		for (int32 Index = 0; Index < ShaftActor->GetTubes().Num(); ++Index)
		{
			const int32 K = ShaftActor->GetTubeStretch(Index);
			const bool bShow = !(Stops.IsValidIndex(K) && UDeepMovingSubRules::StillBedShows(DepthM, Stops[K], Tuning.MovingSub));
			if (const UProceduralMeshComponent* Tube = ShaftActor->GetTubes()[Index])
			{
				TestEqual(FString::Printf(TEXT("%s: shaft stretch %d shows"), Where, K), Tube->IsVisible(), bShow);
			}
		}
		if (Cavern)
		{
			for (int32 Stop = 1; Stop <= Stops.Num(); ++Stop)
			{
				const UProceduralMeshComponent* Shell = Cavern->GetShell(Stop);
				if (Shell && Shell->GetNumSections() > 0)
				{
					const double WantZ = UDeepMovingSubRules::ZFromDepth(Stops[Stop - 1], DatumCm) + AnchorDrop.Z;
					TestEqual(FString::Printf(TEXT("%s: cave shell %d at its stop"), Where, Stop), Shell->GetComponentLocation().Z, WantZ, 1.0);
				}
			}
		}
		TestTrue(FString::Printf(TEXT("%s: the shaft is where it was placed"), Where), Near(ShaftActor->GetActorTransform(), ShaftAt, 0.01, 0.001));
		TestTrue(FString::Printf(TEXT("%s: the root stays at identity"), Where), Near(W.Root()->GetActorTransform(), FTransform::Identity, 0.01, 0.001));
		TestFalse(FString::Printf(TEXT("%s: no trench"), Where), W.Sub->GetTrench() && W.Sub->GetTrench()->IsVisible());
		if (const ADeepDressing* Dressing = W.First<ADeepDressing>())
		{
			TestTrue(FString::Printf(TEXT("%s: dressing in the loaded stop's frame"), Where),
				Dressing->GetActorLocation().Equals(DeepArena::StillStopFrame(W.World, W.State->Stop).GetLocation(), 1.0));
		}
	};

	CheckAt(TEXT("Stop 1"));
	TestFalse(TEXT("Stop 1: its bed shows"), DeepArena::FindLoadedFloor(W.World)->IsHidden());
	W.HoldAt(Stops[0] + 20.f);
	CheckAt(TEXT("Through bed 1"));
	TestTrue(TEXT("Through bed 1: it is hidden and lets the hull by"), DeepArena::FindLoadedFloor(W.World)->IsHidden() && !DeepArena::FindLoadedFloor(W.World)->GetActorEnableCollision());
	W.HoldAt((Stops[0] + Stops[1]) * 0.5f + 5.f);
	CheckAt(TEXT("Nearer stop 2"));
	W.GameMode->GotoStop(2);
	CheckAt(TEXT("Stop 2"));
	TestEqual(TEXT("Stop 2: its bed is the loaded one"), DeepArena::FindLoadedFloor(W.World)->BedStop, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepStillResetTest, "Deep.World.Still.Reset", DEEP_STILL_TEST_FLAGS)
bool FDeepStillResetTest::RunTest(const FString& Parameters)
{
	using namespace DeepStillTest;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FCVarScope Flag(TEXT("Deep.World.Still"), 1);
	FStillWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run"), W.Valid());
	UTEST_TRUE(TEXT("Still"), DeepArena::IsWorldStill(W.World));
	W.TickSub();
	const FTransform AtStop1 = W.Sub->GetActorTransform();
	const double DatumCm = Tuning.MovingSub.DepthDatumZM * UDeepRules::CmPerM;
	TestTrue(TEXT("Starts at stop 1"), Near(AtStop1, UDeepMovingSubRules::StillHull(W.Sub->GetHomeTransform(), FVector2D::ZeroVector, Tuning.GetStop(1).SubDepthM, 0.f, DatumCm)));

	W.GameMode->GotoStop(2);
	W.TickSub();
	TestFalse(TEXT("Moved to stop 2"), Near(W.Sub->GetActorTransform(), AtStop1));
	W.GameMode->RestartRun();
	W.TickSub();
	TestTrue(TEXT("RestartRun: the hull is back at stop 1"), Near(W.Sub->GetActorTransform(), AtStop1));
	TestEqual(TEXT("RestartRun: stop 1 loaded"), DeepArena::FindLoadedFloor(W.World)->BedStop, 1);

	W.HoldAt(Tuning.GetStop(1).SubDepthM + 30.f);
	W.State->SubYawDeg = 40.f;
	DeepArena::ApplySubDepth(W.World);
	W.TickSub();
	TestFalse(TEXT("Flown off and turned"), Near(W.Sub->GetActorTransform(), AtStop1));
	W.GameMode->EnterTitle();
	W.TickSub();
	TestTrue(TEXT("Back to the title: the hull is back at stop 1, home's heading"), Near(W.Sub->GetActorTransform(), AtStop1));

	// The game state reset alone (nothing re-applied): the sub's own tick puts the hull back.
	W.GameMode->GotoStop(2);
	W.State->ResetRun();
	W.TickSub();
	TestTrue(TEXT("ResetRun alone: the next tick puts the hull at stop 1"), Near(W.Sub->GetActorTransform(), AtStop1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepStillRoomClearTest, "Deep.World.Still.RoomClear", DEEP_STILL_TEST_FLAGS)
bool FDeepStillRoomClearTest::RunTest(const FString& Parameters)
{
	using namespace DeepStillTest;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FCVarScope Shaft(TEXT("Deep.Cave.Shaft"), 1);
	FCVarScope Flag(TEXT("Deep.World.Still"), 1);
	FStillWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run"), W.Valid());
	const ADeepSubRoom* Room = DeepArena::EnsureRoom(W.World);
	UTEST_NOT_NULL(TEXT("The room set"), Room);
	const FBox RoomBox = Room->GetComponentsBoundingBox(true);
	UTEST_TRUE(TEXT("The room set has bounds"), RoomBox.IsValid != 0);

	auto CheckClear = [&](const TCHAR* Where)
	{
		W.TickSub();
		int32 Checked = 0;
		for (TActorIterator<AActor> It(W.World); It; ++It)
		{
			AActor* Actor = *It;
			const bool bArena = Actor->IsA<ADeepSubmarine>() || Actor->IsA<ADeepAbyssFloor>() || Actor->IsA<ADeepCavern>() || Actor->IsA<ADeepSalvage>()
				|| Actor->IsA<ADeepShaft>() || Actor->IsA<ADeepDressing>();
			if (!bArena)
			{
				continue;
			}
			const FBox Box = Actor->GetComponentsBoundingBox(true);
			if (!Box.IsValid)
			{
				continue;
			}
			++Checked;
			TestFalse(FString::Printf(TEXT("%s: %s (%s) stands clear of the room set (%s)"), Where, *Actor->GetName(), *Box.ToString(), *RoomBox.ToString()), Box.Intersect(RoomBox));
		}
		TestTrue(FString::Printf(TEXT("%s: arena content checked"), Where), Checked > 3);
	};
	CheckClear(TEXT("Stop 1"));
	W.HoldAt(Tuning.GetStop(1).SubDepthM + 50.f);
	CheckClear(TEXT("Mid-descent"));
	W.GameMode->GotoStop(2);
	CheckClear(TEXT("Stop 2"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepStillRoomLookTest, "Deep.World.Still.RoomLook", DEEP_STILL_TEST_FLAGS)
bool FDeepStillRoomLookTest::RunTest(const FString& Parameters)
{
	using namespace DeepStillTest;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const FDeepMovingSubTuning& MovingSub = Tuning.MovingSub;
	UTEST_FALSE(TEXT("Not the grey box (it has no water look)"), Tuning.Arena.bGreyBox);
	// The ease fits inside the airlock's held black, so the fade hides the switch.
	TestTrue(TEXT("The Room look's ease is no longer than the airlock's black"), MovingSub.RoomLookEaseSeconds > 0.f && MovingSub.RoomLookEaseSeconds <= Tuning.Airlock.BlackSeconds);

	FStillWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run"), W.Valid());
	UTEST_TRUE(TEXT("Still by default"), DeepArena::IsWorldStill(W.World));
	ADeepEnvironment* Environment = W.First<ADeepEnvironment>();
	const ADeepSubRoom* Room = DeepArena::EnsureRoom(W.World);
	UTEST_NOT_NULL(TEXT("The environment"), Environment);
	UTEST_NOT_NULL(TEXT("The room set"), Room);
	W.TickSub();
	const FVector InRoom = Room->GetActorLocation();
	const FVector AtHelm = W.Sub->GetHelmCamera()->GetComponentLocation();
	UTEST_TRUE(TEXT("The room's centre is in the room"), Room->IsInRoom(InRoom, 100.0));
	UTEST_FALSE(TEXT("The helm camera is not in the room"), Room->IsInRoom(AtHelm, 100.0));
	const float Step = 0.05f;
	auto Hold = [&](const FVector& View, float Seconds)
	{
		for (float T = 0.f; T < Seconds; T += Step)
		{
			Environment->StepRoomLook(Step, View);
		}
	};
	auto CheckCave = [&](const TCHAR* Where)
	{
		const FDeepLookPreset Cave = ADeepEnvironment::BlendLook(Tuning.Look, W.State->SubDepthM);
		const FDeepLookPreset& Got = Environment->GetAppliedLook();
		TestEqual(FString::Printf(TEXT("%s: no Room look"), Where), Environment->GetRoomBlend(), 0.f);
		TestEqual(FString::Printf(TEXT("%s: the cave's fog density"), Where), Got.FogDensity, Cave.FogDensity, 1.e-4f);
		TestTrue(FString::Printf(TEXT("%s: the cave's fog colour"), Where), Got.FogColor.Equals(Cave.FogColor, 1.e-4f));
		TestEqual(FString::Printf(TEXT("%s: the cave's volumetric light"), Where), Got.DownlightVolumetric, Cave.DownlightVolumetric, 1.e-4f);
		TestTrue(FString::Printf(TEXT("%s: volumetric fog on"), Where), Environment->IsVolumetricFogOn());
	};

	// Outside (the seat looks through the hull's helm camera): the cave look at the hull's depth.
	Hold(AtHelm, 1.f);
	CheckCave(TEXT("At the helm"));

	// In the room: one step is partway (an ease, not a snap), then the Room look in full.
	Environment->StepRoomLook(Step, InRoom);
	const float Partway = Environment->GetRoomBlend();
	TestTrue(FString::Printf(TEXT("Into the room: eases (%.2f after one step)"), Partway), Partway > 0.f && Partway < 1.f);
	Hold(InRoom, MovingSub.RoomLookEaseSeconds + 0.2f);
	const FDeepLookPreset& InLook = Environment->GetAppliedLook();
	TestEqual(TEXT("In the room: the Room look"), Environment->GetRoomBlend(), 1.f);
	TestEqual(TEXT("In the room: thin fog"), InLook.FogDensity, MovingSub.RoomFogDensity, 1.e-4f);
	TestTrue(TEXT("In the room: the room's haze colour"), InLook.FogColor.Equals(MovingSub.RoomFogColor, 1.e-4f));
	TestEqual(TEXT("In the room: no light in the fog"), InLook.DownlightVolumetric, 0.f, 1.e-4f);
	TestFalse(TEXT("In the room: no volumetric fog"), Environment->IsVolumetricFogOn());
	TestTrue(TEXT("The Room look's fog is thinner than the cave's"), MovingSub.RoomFogDensity < ADeepEnvironment::BlendLook(Tuning.Look, W.State->SubDepthM).FogDensity);

	// Out through the airlock at stop 2: under the hull, the cave look at stop 2's depth again.
	W.GameMode->GotoStop(2);
	W.TickSub();
	TestTrue(TEXT("Out by the valve: tethered"), W.OutByValve());
	W.FinishAirlock();
	Hold(W.Diver->GetActorLocation(), MovingSub.RoomLookEaseSeconds + 0.2f);
	CheckCave(TEXT("Outside at stop 2"));
	return true;
}

#endif
