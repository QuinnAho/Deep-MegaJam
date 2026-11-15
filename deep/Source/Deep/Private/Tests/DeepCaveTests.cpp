// Tests for the cave shaft between stops (Sim/DeepCaveRules, ADeepShaft, UDeepRunComponent::StepShaft): the passage is
// centred and 14 m wide at every stop, never narrower than the sub, the contact maths, the autopilot's clean drive,
// a steered knock that dents the hull, and a breach that drowns the diver at the helm.

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "ProceduralMeshComponent.h"
#include "Sim/DeepCaveRules.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepEventBus.h"
#include "World/DeepArena.h"
#include "World/DeepShaft.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_CAVE_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepCaveTest
{
	/** Deep.Cave.Shaft set for the test's lifetime. */
	struct FShaftScope
	{
		IConsoleVariable* Var = nullptr;
		int32 Before = -1;

		explicit FShaftScope(int32 Value)
		{
			Var = IConsoleManager::Get().FindConsoleVariable(TEXT("Deep.Cave.Shaft"));
			if (Var)
			{
				Before = Var->GetInt();
				Var->Set(Value, ECVF_SetByCode);
			}
		}

		~FShaftScope()
		{
			if (Var)
			{
				Var->Set(Before, ECVF_SetByCode);
			}
		}
	};

	/** A game world with the arena and one diver in the room, as a run starts (the same as Deep.World.Sub's). */
	struct FCaveWorld
	{
		UWorld* World = nullptr;
		ADeepGameMode* GameMode = nullptr;
		ADeepGameState* State = nullptr;
		ADeepSubmarine* Sub = nullptr;
		ADeepDiver* Diver = nullptr;
		UDeepRunComponent* Run = nullptr;
		UDeepEventBus* Bus = nullptr;

		FCaveWorld()
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
			Bus = UDeepEventBus::Get(World);
			if (Sub)
			{
				Diver = World->SpawnActor<ADeepDiver>(Sub->GetRoomStartTransform().GetLocation(), FRotator::ZeroRotator);
			}
			Run = Diver ? Diver->GetRun() : nullptr;
			if (Run)
			{
				Run->StepRun(0.f);
			}
		}

		~FCaveWorld()
		{
			if (World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		bool Valid() const { return GameMode && State && Sub && Diver && Run && Bus; }

		ADeepShaft* Shaft() const
		{
			for (TActorIterator<ADeepShaft> It(World); It; ++It)
			{
				return *It;
			}
			return nullptr;
		}

		/** Seated, unparked at a depth between stops on the passage's centre (as if driven there), nothing pressed. */
		void HoldAt(float DepthM) const
		{
			if (!Run->IsSeated())
			{
				Run->TakeHelm();
			}
			const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
			State->SubDepthM = DepthM;
			State->SubOffsetM = UDeepCaveRules::CentreM(DepthM, Tuning.StopDepthsM(), Tuning.Cave);
			State->bParked = false;
			DeepArena::ApplySubDepth(World);
		}

		/** Steer one way in small steps until the bus has seen Event (or the time runs out). Seconds taken. */
		float SteerUntil(const FVector2D& Steer, FName Event, float MaxSeconds) const
		{
			Run->SetHelmSteer(Steer);
			const int32 Before = Bus->CountOf(Event);
			float T = 0.f;
			while (T < MaxSeconds && Bus->CountOf(Event) == Before)
			{
				Run->StepRun(0.05f);
				T += 0.05f;
			}
			Run->SetHelmSteer(FVector2D::ZeroVector);
			return T;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveStopsTest, "Deep.Sim.Cave.Stops", DEEP_CAVE_TEST_FLAGS)
bool FDeepCaveStopsTest::RunTest(const FString& Parameters)
{
	const FDeepCaveTuning Cave;
	const TArray<float> Stops = { 100.f, 300.f, 500.f };
	const float Min = 8.f;
	for (const float Stop : Stops)
	{
		// At the stop, and the straight stretch just under and over it (the parked sub's room to swim and glide).
		for (const float D : { Stop - 10.f, Stop - 0.5f, Stop, Stop + 0.5f, Stop + 10.f })
		{
			const FVector2D C = UDeepCaveRules::CentreM(D, Stops, Cave);
			TestTrue(FString::Printf(TEXT("%.1f m: centred on the stop's axis"), D), C.IsNearlyZero(0.001f));
			TestEqual(FString::Printf(TEXT("%.1f m: 14 m radius"), D), UDeepCaveRules::RadiusM(D, Stops, Min, Cave), Cave.StopRadiusM, 0.001f);
		}
	}
	TestEqual(TEXT("Stop radius is 14 m"), Cave.StopRadiusM, 14.f);
	TestEqual(TEXT("Above the first stop: segment -1"), UDeepCaveRules::Segment(50.f, Stops), -1);
	TestEqual(TEXT("Between 1 and 2: segment 0"), UDeepCaveRules::Segment(200.f, Stops), 0);
	TestEqual(TEXT("Under the last: segment 2"), UDeepCaveRules::Segment(560.f, Stops), 2);

	// Between stops it really winds: the centre leaves the axis by metres somewhere in every stretch.
	for (int32 K = 0; K + 1 < Stops.Num(); ++K)
	{
		float Far = 0.f;
		for (float D = Stops[K]; D <= Stops[K + 1]; D += 1.f)
		{
			Far = FMath::Max(Far, float(UDeepCaveRules::CentreM(D, Stops, Cave).Size()));
		}
		TestTrue(FString::Printf(TEXT("Stretch %d bends (%.1f m off the axis)"), K, Far), Far > 5.f);
	}
	// The same shaft every time (every machine builds its own).
	TestTrue(TEXT("Deterministic"), UDeepCaveRules::CentreM(213.f, Stops, Cave).Equals(UDeepCaveRules::CentreM(213.f, Stops, Cave)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveRadiusTest, "Deep.Sim.Cave.Radius", DEEP_CAVE_TEST_FLAGS)
bool FDeepCaveRadiusTest::RunTest(const FString& Parameters)
{
	const FDeepCaveTuning Cave;
	const TArray<float> Stops = { 100.f, 300.f, 500.f };
	const float Min = UDeepCaveRules::MinRadiusM(FVector(-5.f, -3.f, -2.f), FVector(5.f, 3.f, 2.f), Cave);
	TestEqual(TEXT("Min radius: the padded half-diagonal plus the margin"), Min, FMath::Sqrt(5.6f * 5.6f + 3.6f * 3.6f) + Cave.SqueezeMarginM, 0.001f);
	float Lowest = UE_BIG_NUMBER;
	float Highest = 0.f;
	for (float D = 0.f; D <= 700.f; D += 0.25f)
	{
		const float R = UDeepCaveRules::RadiusM(D, Stops, Min, Cave);
		Lowest = FMath::Min(Lowest, R);
		Highest = FMath::Max(Highest, R);
		if (R < Min - 0.001f)
		{
			AddError(FString::Printf(TEXT("%.2f m: radius %.2f under the minimum %.2f"), D, R, Min));
			break;
		}
	}
	AddInfo(FString::Printf(TEXT("Radius over the shaft: %.1f to %.1f m (min %.1f)"), Lowest, Highest, Min));
	TestTrue(TEXT("A cavern opens somewhere (over 25 m)"), Highest > 25.f);
	TestTrue(TEXT("Never wider than a cavern"), Highest <= Cave.CavernRadiusM + 0.001f);
	TestTrue(TEXT("A squeeze somewhere (within 1.5 m of the minimum)"), Lowest < Min + 1.5f);
	// A squeeze that would not fit the sub is widened to fit.
	TestEqual(TEXT("A huge sub: the radius is its minimum"), UDeepCaveRules::RadiusM(200.f, Stops, 40.f, Cave), 40.f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveContactTest, "Deep.Sim.Cave.Contact", DEEP_CAVE_TEST_FLAGS)
bool FDeepCaveContactTest::RunTest(const FString& Parameters)
{
	FDeepCaveTuning Cave;
	Cave.HullPadM = 0.f;
	Cave.SqueezeMarginM = 0.f;
	const TArray<float> Stops = { 100.f, 300.f, 500.f };
	const FVector BoxMin(-1.f, -1.f, -1.f);
	const FVector BoxMax(1.f, 1.f, 1.f);
	const float Min = UDeepCaveRules::MinRadiusM(BoxMin, BoxMax, Cave);
	TestEqual(TEXT("A 2 m cube's minimum is its half-diagonal"), Min, UE_SQRT_2, 0.001f);

	// Above the first stop: a straight 14 m tube on the axis.
	const FDeepCaveContact Clear = UDeepCaveRules::Contact(FVector2D(12.f, 0.f), 50.f, BoxMin, BoxMax, Stops, Min, Cave);
	TestFalse(TEXT("12 m off: clear"), Clear.bTouching);
	TestEqual(TEXT("12 m off: the far corner is 0.96 m from the rock"), Clear.PenetrationM, FMath::Sqrt(13.f * 13.f + 1.f) - 14.f, 0.001f);
	const FDeepCaveContact Hit = UDeepCaveRules::Contact(FVector2D(13.5f, 0.f), 50.f, BoxMin, BoxMax, Stops, Min, Cave);
	TestTrue(TEXT("13.5 m off: touching"), Hit.bTouching);
	TestEqual(TEXT("13.5 m off: into the rock by the corner's overshoot"), Hit.PenetrationM, FMath::Sqrt(14.5f * 14.5f + 1.f) - 14.f, 0.001f);
	TestTrue(TEXT("The normal points out, to the rock"), Hit.Normal.X > 0.99f && FMath::IsNearlyEqual(float(Hit.Normal.Size()), 1.f, 0.001f));
	const FDeepCaveContact Port = UDeepCaveRules::Contact(FVector2D(0.f, -14.f), 50.f, BoxMin, BoxMax, Stops, Min, Cave);
	TestTrue(TEXT("To port: touching, normal to port"), Port.bTouching && Port.Normal.Y < -0.99f);
	const FVector2D Out = FVector2D(13.5f, 0.f) - Hit.Normal * Hit.PenetrationM;
	// Pushed out by the penetration along the normal: the far corner leaves the rock (the next corner may graze it by mm; the substeps take that).
	TestTrue(TEXT("Pushed out along the normal: out by all but a few mm"), UDeepCaveRules::Contact(Out, 50.f, BoxMin, BoxMax, Stops, Min, Cave).PenetrationM < 0.02f);

	// Steering: inertia, then the top speed.
	const FVector2D V1 = UDeepCaveRules::SteerVelocity(FVector2D::ZeroVector, FVector2D(1.f, 0.f), 0.5f, Cave);
	TestEqual(TEXT("Half a second of thrust"), float(V1.X), Cave.SteerAccelMps2 * 0.5f, 0.001f);
	TestEqual(TEXT("Top speed"), float(UDeepCaveRules::SteerVelocity(FVector2D::ZeroVector, FVector2D(1.f, 1.f), 10.f, Cave).Size()), Cave.SteerSpeedMps, 0.001f);
	TestTrue(TEXT("Let go: it slows"), UDeepCaveRules::SteerVelocity(FVector2D(5.f, 0.f), FVector2D::ZeroVector, 0.25f, Cave).X < 5.f);

	// Damage by impact speed, capped; the hull's integrity.
	TestEqual(TEXT("A nudge: no dent"), UDeepCaveRules::HitDamage(Cave.HitMinSpeedMps * 0.5f, Cave), 0);
	TestEqual(TEXT("Just over the threshold: one dent"), UDeepCaveRules::HitDamage(Cave.HitMinSpeedMps + 0.01f, Cave), 1);
	TestEqual(TEXT("Full speed: the cap"), UDeepCaveRules::HitDamage(Cave.SteerSpeedMps, Cave), FMath::Min(Cave.MaxHitDamage, FMath::RoundToInt(Cave.SteerSpeedMps * Cave.DamagePerMps)));
	TestEqual(TEXT("Never past the cap"), UDeepCaveRules::HitDamage(100.f, Cave), Cave.MaxHitDamage);
	TestEqual(TEXT("Whole hull"), UDeepCaveRules::HullIntegrity01(0, Cave), 1.f);
	TestEqual(TEXT("Half the hull"), UDeepCaveRules::HullIntegrity01(Cave.HullBreachDamage / 2, Cave), 1.f - float(Cave.HullBreachDamage / 2) / Cave.HullBreachDamage, 0.001f);
	TestFalse(TEXT("One short of a breach"), UDeepCaveRules::IsBreached(Cave.HullBreachDamage - 1, Cave));
	TestTrue(TEXT("Breached"), UDeepCaveRules::IsBreached(Cave.HullBreachDamage, Cave));
	// hull_critical: once, on the dent that takes the integrity under a quarter (10 hull: 7 -> 8 dents is 0.3 -> 0.2).
	FDeepCaveTuning Ten = Cave;
	Ten.HullBreachDamage = 10;
	Ten.HullCriticalIntegrity01 = 0.25f;
	TestFalse(TEXT("6 -> 7 dents: still over a quarter"), UDeepCaveRules::CrossesCritical(6, 7, Ten));
	TestTrue(TEXT("7 -> 8 dents: critical"), UDeepCaveRules::CrossesCritical(7, 8, Ten));
	TestTrue(TEXT("5 -> 8 dents in one knock: critical"), UDeepCaveRules::CrossesCritical(5, 8, Ten));
	TestFalse(TEXT("8 -> 9 dents: already critical, not again"), UDeepCaveRules::CrossesCritical(8, 9, Ten));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveTubeTest, "Deep.World.Cave.Tube", DEEP_CAVE_TEST_FLAGS)
bool FDeepCaveTubeTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveTest;
	FShaftScope On(1);
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FCaveWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus"), W.Valid());
	ADeepShaft* Shaft = W.Shaft();
	UTEST_NOT_NULL(TEXT("The sub spawned the shaft"), Shaft);
	AddInfo(FString::Printf(TEXT("Shaft built in %.0f ms, %d pieces, %d triangles"), Shaft->GetBuildSeconds() * 1000.0, Shaft->GetTubes().Num(), Shaft->GetTriangleCount()));
	TSet<int32> Stretches;
	TArray<UProceduralMeshComponent*> FirstStretch;
	for (int32 Index = 0; Index < Shaft->GetTubes().Num(); ++Index)
	{
		Stretches.Add(Shaft->GetTubeStretch(Index));
		if (Shaft->GetTubeStretch(Index) == 0 && Shaft->GetTubes()[Index])
		{
			FirstStretch.Add(Shaft->GetTubes()[Index].Get());
		}
	}
	TestEqual(TEXT("Rock in every stretch"), Stretches.Num(), Tuning.NumStops());
	UTEST_TRUE(TEXT("The first stretch has rock"), FirstStretch.Num() > 0);
	for (UProceduralMeshComponent* Piece : FirstStretch)
	{
		TestFalse(TEXT("Parked at stop 1: its stretch is hidden (the sea bed is there)"), Piece->IsVisible());
		TestEqual(TEXT("Blocks the sonar's traces"), int32(Piece->GetCollisionResponseToChannel(ECC_Visibility)), int32(ECR_Block));
		TestEqual(TEXT("Blocks pawns"), int32(Piece->GetCollisionResponseToChannel(ECC_Pawn)), int32(ECR_Block));
	}

	// Half way down to stop 2: the rock shows and stands all round the sub, where the rule says.
	const float Depth = 0.5f * (Tuning.GetStop(1).SubDepthM + Tuning.GetStop(2).SubDepthM);
	W.HoldAt(Depth);
	for (UProceduralMeshComponent* Piece : FirstStretch)
	{
		TestTrue(TEXT("Between stops: the stretch shows"), Piece->IsVisible());
	}
	const TArray<float> Stops = Tuning.StopDepthsM();
	FVector BoxMin, BoxMax;
	DeepShaft::SubBoxM(W.Sub, BoxMin, BoxMax);
	const float R = UDeepCaveRules::RadiusM(Depth, Stops, UDeepCaveRules::MinRadiusM(BoxMin, BoxMax, Tuning.Cave), Tuning.Cave);
	AddInfo(FString::Printf(TEXT("Half way down: the passage is %.1f m off the stops' axis, %.1f m wide"), W.State->SubOffsetM.Size(), R));
	const FVector From = W.Sub->GetActorLocation();
	int32 Hits = 0;
	int32 WorldHits = 0;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		const float A = UE_TWO_PI * Index / 8.f;
		const FVector Dir = W.Sub->GetActorRotation().RotateVector(FVector(FMath::Cos(A), FMath::Sin(A), 0.f));
		const FVector To = From + Dir * (Tuning.Cave.CavernRadiusM + 30.f) * UDeepRules::CmPerM;
		FCollisionQueryParams Params(TEXT("DeepCaveTest"), true);
		Params.AddIgnoredActor(W.Sub);
		Params.AddIgnoredActor(W.Diver);
		// The sonar's trace: the world, on the Visibility channel.
		FHitResult WorldHit;
		if (W.World->LineTraceSingleByChannel(WorldHit, From, To, ECC_Visibility, Params) && WorldHit.GetActor() == Shaft)
		{
			++WorldHits;
		}
		// The shaft's own rock: the nearest piece the ray meets.
		float Nearest = TNumericLimits<float>::Max();
		for (UProceduralMeshComponent* Piece : FirstStretch)
		{
			FHitResult Hit;
			if (Piece->LineTraceComponent(Hit, From, To, Params))
			{
				Nearest = FMath::Min(Nearest, float(Hit.Distance));
			}
		}
		if (Nearest < TNumericLimits<float>::Max())
		{
			++Hits;
			// The sub sits on the passage's centre: the rock is the rule's radius away (give or take the relief).
			const float Got = Nearest / UDeepRules::CmPerM;
			TestTrue(FString::Printf(TEXT("Ray %d: rock at %.1f m, the rule says %.1f m"), Index, Got, R), FMath::Abs(Got - R) < 4.f);
		}
	}
	TestEqual(TEXT("Rock all round"), Hits, 8);
	TestEqual(TEXT("The world's Visibility trace meets the shaft first, all round"), WorldHits, 8);

	// Shaft off: no rock at all.
	FShaftScope Off(0);
	Shaft->Follow();
	for (UProceduralMeshComponent* Piece : FirstStretch)
	{
		TestFalse(TEXT("Shaft off: hidden"), Piece->IsVisible());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveAutopilotTest, "Deep.World.Cave.Autopilot", DEEP_CAVE_TEST_FLAGS)
bool FDeepCaveAutopilotTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveTest;
	FShaftScope On(1);
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FCaveWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus"), W.Valid());
	const TArray<float> Stops = Tuning.StopDepthsM();
	FVector BoxMin, BoxMax;
	DeepShaft::SubBoxM(W.Sub, BoxMin, BoxMax);
	const float Min = UDeepCaveRules::MinRadiusM(BoxMin, BoxMax, Tuning.Cave);

	// Deep.Drive 2: stop 1 to stop 2 on the autopilot, in the game's own frame steps.
	W.Run->BuyNextGate(true);
	W.Run->SetAutopilot(Tuning.GetStop(2).SubDepthM);
	int32 Touches = 0;
	float FarOff = 0.f;
	for (int32 Step = 0; Step < 4000 && W.Run->IsAutopiloting(); ++Step)
	{
		W.Run->StepRun(1.f / 30.f);
		FarOff = FMath::Max(FarOff, float(W.State->SubOffsetM.Size()));
		if (UDeepCaveRules::Contact(W.State->SubOffsetM, W.State->SubDepthM, BoxMin, BoxMax, Stops, Min, Tuning.Cave).PenetrationM > 0.f)
		{
			++Touches;
		}
	}
	W.Run->StepRun(1.f / 30.f);
	AddInfo(FString::Printf(TEXT("The autopilot went up to %.1f m off the axis"), FarOff));
	TestTrue(TEXT("It followed the bends (off the axis by metres)"), FarOff > 3.f);
	TestEqual(TEXT("Never in the rock"), Touches, 0);
	TestEqual(TEXT("No dents"), W.State->HullDamage, 0);
	TestEqual(TEXT("No hull_hit"), W.Bus->CountOf(DeepEvent::HullHit), 0);
	TestEqual(TEXT("No hull_damage"), W.Bus->CountOf(DeepEvent::HullDamage), 0);
	TestEqual(TEXT("Parked at stop 2"), W.State->ParkedStop(), 2);
	TestTrue(TEXT("On the stop's centre"), W.State->SubOffsetM.IsNearlyZero(0.01f));
	TestTrue(TEXT("Hatch open"), W.Sub->IsHatchOpen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveHitTest, "Deep.World.Cave.Hit", DEEP_CAVE_TEST_FLAGS)
bool FDeepCaveHitTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveTest;
	FShaftScope On(1);
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const FDeepCaveTuning& Cave = Tuning.Cave;
	FCaveWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus"), W.Valid());
	W.HoldAt(0.5f * (Tuning.GetStop(1).SubDepthM + Tuning.GetStop(2).SubDepthM));
	const FVector2D Start = W.State->SubOffsetM;

	// Steering moves the sub across the passage (the world slides the other way).
	W.Run->SetHelmSteer(FVector2D(0.f, 1.f));
	W.Run->StepRun(1.f);
	TestTrue(TEXT("A second to starboard: it moved starboard"), W.State->SubOffsetM.Y > Start.Y + 1.f);
	TestTrue(TEXT("With inertia, not at once"), W.Run->GetSteerVelocityMps().Y > 0.f && W.Run->GetSteerVelocityMps().Y <= Cave.SteerSpeedMps);

	// Held into the rock: a knock, a dent by speed, the Captain's line, the bill.
	const float Took = W.SteerUntil(FVector2D(0.f, 1.f), DeepEvent::HullHit, 30.f);
	AddInfo(FString::Printf(TEXT("Hit the wall after %.1f s more"), Took));
	TestEqual(TEXT("hull_hit fired"), W.Bus->CountOf(DeepEvent::HullHit), 1);
	TestTrue(TEXT("Dented by impact speed"), W.State->HullDamage >= 1 && W.State->HullDamage <= Cave.MaxHitDamage);
	TestEqual(TEXT("Billed at the console"), W.State->HullRepairsOwed, W.State->HullDamage);
	TestTrue(TEXT("Integrity below whole"), W.State->GetHullIntegrity01() < 1.f);
	AddInfo(FString::Printf(TEXT("After the knock: steer velocity (%.2f, %.2f) m/s"), W.Run->GetSteerVelocityMps().X, W.Run->GetSteerVelocityMps().Y));
	TestTrue(TEXT("The knock took the speed into the rock"), W.Run->GetSteerVelocityMps().Y < Cave.SteerSpeedMps * 0.5f);
	TestTrue(TEXT("Pushed out: not in the rock"), [&W, &Tuning]()
	{
		FVector BoxMin, BoxMax;
		DeepShaft::SubBoxM(W.Sub, BoxMin, BoxMax);
		return UDeepCaveRules::Contact(W.State->SubOffsetM, W.State->SubDepthM, BoxMin, BoxMax, Tuning.StopDepthsM(), UDeepCaveRules::MinRadiusM(BoxMin, BoxMax, Tuning.Cave), Tuning.Cave).PenetrationM <= 0.01f;
	}());

	// Parking glides the sub home to the stop's centre before the hatch opens.
	W.Run->SetAutopilot(Tuning.GetStop(1).SubDepthM);
	for (int32 Step = 0; Step < 2000 && W.Run->IsAutopiloting(); ++Step)
	{
		W.Run->StepRun(0.1f);
	}
	W.Run->StepRun(0.1f);
	TestEqual(TEXT("Parked at stop 1"), W.State->ParkedStop(), 1);
	TestTrue(TEXT("On the stop's centre"), W.State->SubOffsetM.IsNearlyZero(0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveBreachTest, "Deep.World.Cave.Breach", DEEP_CAVE_TEST_FLAGS)
bool FDeepCaveBreachTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveTest;
	FShaftScope On(1);
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const FDeepCaveTuning& Cave = Tuning.Cave;
	if (!Cave.bHullBreachKills)
	{
		AddInfo(TEXT("Cave.bHullBreachKills is off: no breach to test"));
		return true;
	}
	FCaveWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus"), W.Valid());
	W.HoldAt(0.5f * (Tuning.GetStop(1).SubDepthM + Tuning.GetStop(2).SubDepthM));
	const int32 DiverBefore = W.State->DiverNumber;
	W.State->HullDamage = Cave.HullBreachDamage - 1;
	W.State->HullRepairsOwed = Cave.HullBreachDamage - 1;
	W.SteerUntil(FVector2D(1.f, 0.f), DeepEvent::HullBreach, 30.f);
	TestEqual(TEXT("hull_breach fired"), W.Bus->CountOf(DeepEvent::HullBreach), 1);
	TestEqual(TEXT("The diver at the helm drowned"), W.Run->GetState(), EDeepRunState::Dead);
	TestFalse(TEXT("Not seated"), W.Run->IsSeated());
	TestEqual(TEXT("The hull is patched for the next diver"), W.State->HullDamage, 0);
	TestTrue(TEXT("The repairs are still owed"), W.State->HullRepairsOwed >= Cave.HullBreachDamage);
	for (int32 Step = 0; Step < 200 && W.Run->GetState() == EDeepRunState::Dead; ++Step)
	{
		W.Run->StepRun(0.25f);
	}
	TestEqual(TEXT("The next diver starts aboard"), W.Run->GetState(), EDeepRunState::Aboard);
	TestEqual(TEXT("A new diver number"), W.State->DiverNumber, DiverBefore + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveOffTest, "Deep.World.Cave.Off", DEEP_CAVE_TEST_FLAGS)
bool FDeepCaveOffTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveTest;
	FShaftScope Off(0);
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FCaveWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus"), W.Valid());
	W.Run->BuyNextGate(true);
	W.Run->TakeHelm();
	W.Run->SetHelmSteer(FVector2D(1.f, 1.f));
	W.Run->SetHelmInput(1.f);
	for (int32 Step = 0; Step < 200; ++Step)
	{
		W.Run->StepRun(0.1f);
	}
	TestEqual(TEXT("The straight drive: 20 s at the helm's speed"), W.State->SubDepthM, FMath::Min(Tuning.GetStop(1).SubDepthM + Tuning.Sub.DriveSpeedMps * 20.f, W.State->DepthLimitM()), 0.05f);
	TestTrue(TEXT("No sideways offset"), W.State->SubOffsetM.IsZero());
	TestEqual(TEXT("No dents"), W.State->HullDamage, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
