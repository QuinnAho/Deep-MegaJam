// Harpoon tests: the pure rules (range, reel step, bolt step, sphere hit, firing permission, repair fees), and world
// tests that a cut diver harpooning the sub is reeled home and re-tethered, the repair fee lands in the next
// settlement with the damage count up, a second shot in a dive is refused, and a bolt in the serpent makes it recoil.

#include "CoreMinimal.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "Harpoon/DeepHarpoonComponent.h"
#include "Harpoon/DeepHarpoonRules.h"
#include "Harpoon/DeepHarpoonSettings.h"
#include "Misc/AutomationTest.h"
#include "Monster/DeepMonsterBrain.h"
#include "Monster/DeepSerpent.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepEventBus.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_HARPOON_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepHarpoonTest
{
	struct FHarpoonWorld
	{
		UWorld* World = nullptr;
		ADeepGameMode* GameMode = nullptr;
		ADeepSubmarine* Sub = nullptr;
		ADeepGameState* State = nullptr;
		ADeepDiver* Diver = nullptr;
		UDeepRunComponent* Run = nullptr;
		UDeepHarpoonComponent* Harpoon = nullptr;
		UDeepEventBus* Bus = nullptr;

		FHarpoonWorld()
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
				Diver = World->SpawnActor<ADeepDiver>(Sub->GetDiverStartTransform().GetLocation(), FRotator::ZeroRotator);
			}
			Run = Diver ? Diver->GetRun() : nullptr;
			Harpoon = Diver ? Diver->GetHarpoon() : nullptr;
		}

		~FHarpoonWorld()
		{
			if (World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		bool Valid() const { return Sub && State && Diver && Run && Harpoon && Bus; }

		/** Steps the harpoon and the run together, like a frame, until the harpoon is idle or time runs out. */
		float StepUntilIdle(float MaxSeconds, float Dt = 0.05f)
		{
			float Elapsed = 0.f;
			while (Elapsed < MaxSeconds)
			{
				Harpoon->StepHarpoon(Dt);
				Run->StepRun(Dt);
				Elapsed += Dt;
				if (Harpoon->GetPhase() == EDeepHarpoonPhase::Idle)
				{
					break;
				}
			}
			return Elapsed;
		}

		/** Puts the diver DistanceM out from the hull's middle along Offset and fires at the hull. */
		void FireAtSubFrom(const FVector& Offset, float DistanceM)
		{
			const FVector Hull = Sub->GetActorLocation();
			const FVector From = Hull + Offset.GetSafeNormal() * DistanceM * UDeepRules::CmPerM;
			Diver->TeleportDiver(From);
			Harpoon->FireAlong(From, (Hull - From).GetSafeNormal());
		}
	};

	FDeepCargoItem Item(float Kg, int32 Value)
	{
		FDeepCargoItem Out;
		Out.Size = EDeepSalvageSize::Medium;
		Out.Kg = Kg;
		Out.Value = Value;
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHarpoonRulesTest, "Deep.Sim.Harpoon.Rules", DEEP_HARPOON_TEST_FLAGS)
bool FDeepHarpoonRulesTest::RunTest(const FString& Parameters)
{
	// Range.
	TestTrue(TEXT("24 m is in a 25 m range"), UDeepHarpoonRules::InRange(FVector::ZeroVector, FVector(2400, 0, 0), 25.f));
	TestTrue(TEXT("Exactly 25 m is in range"), UDeepHarpoonRules::InRange(FVector::ZeroVector, FVector(0, 1500, 2000), 25.f));
	TestFalse(TEXT("26 m is out of range"), UDeepHarpoonRules::InRange(FVector::ZeroVector, FVector(0, 0, -2600), 25.f));
	TestFalse(TEXT("Zero range reaches nothing away"), UDeepHarpoonRules::InRange(FVector::ZeroVector, FVector(1, 0, 0), 0.f));

	// Firing permission: one shot, in the water, nothing out.
	TestTrue(TEXT("A loaded diver in the water may fire"), UDeepHarpoonRules::CanFire(1, true, EDeepHarpoonPhase::Idle));
	TestFalse(TEXT("No shots left: refused"), UDeepHarpoonRules::CanFire(0, true, EDeepHarpoonPhase::Idle));
	TestFalse(TEXT("Not in the water: refused"), UDeepHarpoonRules::CanFire(1, false, EDeepHarpoonPhase::Idle));
	TestFalse(TEXT("A bolt already out: refused"), UDeepHarpoonRules::CanFire(1, true, EDeepHarpoonPhase::Flying));
	TestFalse(TEXT("Latched: refused"), UDeepHarpoonRules::CanFire(1, true, EDeepHarpoonPhase::Latched));

	// Reel step: speed * dt toward the target, never past it.
	const FVector Reeled = UDeepHarpoonRules::ReelStep(FVector::ZeroVector, FVector(1000, 0, 0), 10.f, 0.5f);
	TestEqual(TEXT("10 m/s for 0.5 s moves 5 m"), Reeled.X, 500.0, 0.01);
	TestEqual(TEXT("Straight at the target"), Reeled.Y + Reeled.Z, 0.0, 0.01);
	TestEqual(TEXT("Never overshoots"), UDeepHarpoonRules::ReelStep(FVector::ZeroVector, FVector(0, 300, 0), 10.f, 1.f), FVector(0, 300, 0));
	TestEqual(TEXT("Zero dt stays put"), UDeepHarpoonRules::ReelStep(FVector(5, 5, 5), FVector(0, 300, 0), 10.f, 0.f), FVector(5, 5, 5));
	{
		FVector At(0, 0, -2500);
		float T = 0.f;
		while (At != FVector::ZeroVector && T < 10.f)
		{
			At = UDeepHarpoonRules::ReelStep(At, FVector::ZeroVector, 10.f, 1.f / 30.f);
			T += 1.f / 30.f;
		}
		TestTrue(TEXT("25 m at 10 m/s arrives in about 2.5 s"), At == FVector::ZeroVector && T > 2.45f && T < 2.6f);
	}

	// Bolt step: quick, but capped by the range.
	TestEqual(TEXT("60 m/s for 0.1 s is 6 m"), UDeepHarpoonRules::BoltStepCm(0.f, 25.f, 60.f, 0.1f), 600.f, 0.01f);
	TestEqual(TEXT("The last step stops at the range"), UDeepHarpoonRules::BoltStepCm(2300.f, 25.f, 60.f, 0.1f), 200.f, 0.01f);
	TestEqual(TEXT("Nothing past the range"), UDeepHarpoonRules::BoltStepCm(2600.f, 25.f, 60.f, 0.1f), 0.f, 0.01f);

	// Sphere hit (the serpent has no collision).
	TestEqual(TEXT("Head-on hit at the near surface"), UDeepHarpoonRules::SegmentSphereHit(FVector::ZeroVector, FVector(1000, 0, 0), FVector(500, 0, 0), 100.f), 0.4f, 0.001f);
	TestEqual(TEXT("Passing wide misses"), UDeepHarpoonRules::SegmentSphereHit(FVector::ZeroVector, FVector(1000, 0, 0), FVector(500, 200, 0), 100.f), -1.f, 0.001f);
	TestEqual(TEXT("Short of it misses"), UDeepHarpoonRules::SegmentSphereHit(FVector::ZeroVector, FVector(300, 0, 0), FVector(500, 0, 0), 100.f), -1.f, 0.001f);
	TestEqual(TEXT("Starting inside is an immediate hit"), UDeepHarpoonRules::SegmentSphereHit(FVector(450, 0, 0), FVector(1000, 0, 0), FVector(500, 0, 0), 100.f), 0.f, 0.001f);

	// Fees from hits.
	TestEqual(TEXT("Three hits at 40"), UDeepHarpoonRules::RepairFee(3, 40), 120);
	TestEqual(TEXT("No hits, no fee"), UDeepHarpoonRules::RepairFee(0, 40), 0);
	TestEqual(TEXT("Negative inputs count as zero"), UDeepHarpoonRules::RepairFee(-2, 40) + UDeepHarpoonRules::RepairFee(2, -40), 0);
	TestEqual(TEXT("No overflow"), UDeepHarpoonRules::RepairFee(MAX_int32, 40), MAX_int32);

	// The part of the repair the haul cannot cover comes from the account.
	TestEqual(TEXT("A rich haul pays the repair itself"), UDeepHarpoonRules::RepairFromCredits(200, 80, 60, 40), 0);
	TestEqual(TEXT("An empty haul: the whole repair from credits"), UDeepHarpoonRules::RepairFromCredits(0, 0, 45, 40), 40);
	TestEqual(TEXT("A thin haul: only the shortfall"), UDeepHarpoonRules::RepairFromCredits(100, 40, 80, 40), 20);
	TestEqual(TEXT("Joke fees over gross never come from credits"), UDeepHarpoonRules::RepairFromCredits(10, 4, 50, 0), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHarpoonSubTest, "Deep.World.Harpoon.SubReelAndFee", DEEP_HARPOON_TEST_FLAGS)
bool FDeepHarpoonSubTest::RunTest(const FString& Parameters)
{
	using namespace DeepHarpoonTest;
	const UDeepHarpoonSettings& Settings = UDeepHarpoonSettings::Get();
	FHarpoonWorld W;
	UTEST_TRUE(TEXT("World, sub, diver, run and harpoon"), W.Valid());
	TestEqual(TEXT("Starts loaded"), W.Harpoon->GetShotsLeft(), Settings.ShotsPerDive);
	TestEqual(TEXT("No hull damage yet"), W.State->HullDamage, 0);

	// Cut loose, out below the sub with cargo; harpoon the hull from 15 m below.
	W.Diver->AddCargo(Item(6.f, 300));
	W.Run->CutTether(true);
	UTEST_EQUAL(TEXT("Cut"), W.Run->GetState(), EDeepRunState::Cut);
	W.State->Credits = 500;
	W.FireAtSubFrom(FVector(0.2f, 0.f, -1.f), 15.f);
	TestEqual(TEXT("Flying"), W.Harpoon->GetPhase(), EDeepHarpoonPhase::Flying);
	TestEqual(TEXT("harpoon_fired"), W.Bus->CountOf(DeepHarpoonEvent::Fired), 1);
	TestEqual(TEXT("The shot is used"), W.Harpoon->GetShotsLeft(), Settings.ShotsPerDive - 1);
	TestNotNull(TEXT("A visible bolt"), W.Harpoon->GetBolt());

	// Quick but not instant: the bolt needs more than one short step to cover 13 m.
	W.Harpoon->StepHarpoon(0.05f);
	TestEqual(TEXT("Still in flight after one 50 ms step"), W.Harpoon->GetPhase(), EDeepHarpoonPhase::Flying);
	for (int32 Step = 0; Step < 20 && W.Harpoon->GetPhase() == EDeepHarpoonPhase::Flying; ++Step)
	{
		W.Harpoon->StepHarpoon(0.05f);
	}
	UTEST_EQUAL(TEXT("Latched"), W.Harpoon->GetPhase(), EDeepHarpoonPhase::Latched);
	TestEqual(TEXT("On the submarine"), W.Harpoon->GetLatchTarget(), EDeepHarpoonTarget::Submarine);
	TestEqual(TEXT("harpoon_hit_sub"), W.Bus->CountOf(DeepHarpoonEvent::HitSub), 1);
	TestEqual(TEXT("The hull is damaged"), W.State->HullDamage, 1);
	TestEqual(TEXT("One repair owed"), W.State->HullRepairsOwed, 1);

	// Reeled home fast: up into the hatch, re-tether fee, aboard; the cargo is banked at the console.
	const float Took = W.StepUntilIdle(8.f);
	TestTrue(FString::Printf(TEXT("Reeled in within the latch limit (%.2f s)"), Took), Took < Settings.MaxLatchSeconds);
	W.Run->StepRun(0.05f);
	TestEqual(TEXT("retether fired"), W.Bus->CountOf(DeepEvent::Retether), 1);
	UTEST_EQUAL(TEXT("Reeled up into the hatch: aboard"), W.Run->GetState(), EDeepRunState::Aboard);
	W.Run->UseConsole();
	UTEST_EQUAL(TEXT("Banked at the console"), W.Run->GetState(), EDeepRunState::Settling);

	// The fee is itemised on that settlement, and the damage stays.
	const FDeepSettlementReport& Report = W.Run->GetSettlement();
	const int32 Line = Report.FeeLabels.IndexOfByKey(Settings.RepairFeeLabel);
	TestTrue(TEXT("Repair line on the settlement"), Line != INDEX_NONE);
	TestEqual(TEXT("At the repair price"), Report.FeeAmounts.IsValidIndex(Line) ? Report.FeeAmounts[Line] : -1, Settings.RepairFeePerHit);
	TestEqual(TEXT("Billed once"), W.State->HullRepairsOwed, 0);
	TestEqual(TEXT("Damage remains until repaired"), W.State->HullDamage, 1);
	TestTrue(TEXT("Credits never below zero"), W.State->Credits >= 0);

	// No haul to pay with: the repair comes out of the account, never below zero.
	W.State->HullRepairsOwed = 2;
	W.State->Credits = 50;
	W.Run->ResetRun();
	TestEqual(TEXT("A new run clears the damage"), W.State->HullDamage + W.State->HullRepairsOwed, 0);
	W.State->HullRepairsOwed = 2;
	W.State->Credits = 50;
	W.Diver->AddCargo(Item(1.f, 1));
	W.Run->Bank();
	TestEqual(TEXT("80 cr of repairs on 50 cr leaves zero"), W.State->Credits, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHarpoonOneShotTest, "Deep.World.Harpoon.OneShotPerDive", DEEP_HARPOON_TEST_FLAGS)
bool FDeepHarpoonOneShotTest::RunTest(const FString& Parameters)
{
	using namespace DeepHarpoonTest;
	const UDeepHarpoonSettings& Settings = UDeepHarpoonSettings::Get();
	FHarpoonWorld W;
	UTEST_TRUE(TEXT("World"), W.Valid());

	// Fire into open water sideways: a miss after the full range.
	const FVector From = W.Sub->GetDiverStartTransform().GetLocation() + FVector(0.f, 0.f, -500.f);
	W.Diver->TeleportDiver(From);
	for (int32 Shot = 0; Shot < Settings.ShotsPerDive; ++Shot)
	{
		W.Harpoon->FireAlong(From, FVector(0.f, 1.f, 0.f));
		W.StepUntilIdle(3.f);
	}
	TestEqual(TEXT("harpoon_missed"), W.Bus->CountOf(DeepHarpoonEvent::Missed), Settings.ShotsPerDive);
	TestEqual(TEXT("Spent"), W.Harpoon->GetShotsLeft(), 0);

	// A second shot in the same dive is refused.
	W.Harpoon->FireAlong(From, FVector(0.f, 1.f, 0.f));
	TestEqual(TEXT("Nothing flies"), W.Harpoon->GetPhase(), EDeepHarpoonPhase::Idle);
	TestEqual(TEXT("harpoon_empty"), W.Bus->CountOf(DeepHarpoonEvent::Empty), 1);
	TestEqual(TEXT("Still only the first shot fired"), W.Bus->CountOf(DeepHarpoonEvent::Fired), Settings.ShotsPerDive);
	TestEqual(TEXT("The HUD hears about it"), W.Harpoon->GetLastResult(), EDeepHarpoonResult::Empty);

	// Re-tethering (home at the hatch, aboard) is the same dive; the next drop through the hatch reloads.
	W.Run->CutTether(true);
	W.Diver->TeleportDiver(W.Sub->GetAnchorLocation() - FVector(0.f, 0.f, 150.f));
	W.Run->StepRun(0.05f);
	TestEqual(TEXT("Re-tethered: aboard"), W.Run->GetState(), EDeepRunState::Aboard);
	TestEqual(TEXT("A re-tether does not reload"), W.Harpoon->GetShotsLeft(), 0);
	W.Run->LeaveSub();
	TestEqual(TEXT("A new dive reloads"), W.Harpoon->GetShotsLeft(), Settings.ShotsPerDive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHarpoonSerpentTest, "Deep.World.Harpoon.SerpentRecoils", DEEP_HARPOON_TEST_FLAGS)
bool FDeepHarpoonSerpentTest::RunTest(const FString& Parameters)
{
	using namespace DeepHarpoonTest;
	FHarpoonWorld W;
	UTEST_TRUE(TEXT("World"), W.Valid());
	UDeepMonsterBrain* Brain = W.World->GetSubsystem<UDeepMonsterBrain>();
	UTEST_NOT_NULL(TEXT("Monster brain"), Brain);
	ADeepSerpent* Serpent = Brain->SpawnSerpent(true);
	UTEST_NOT_NULL(TEXT("Serpent"), Serpent);

	// Out in open water below the sub; the serpent 12 m ahead, stalking.
	const FVector From = W.Sub->GetDiverStartTransform().GetLocation() + FVector(0.f, 0.f, -800.f);
	W.Diver->TeleportDiver(From);
	Serpent->PlaceHead(From + FVector(1200.f, 0.f, 0.f), FVector(0.f, 1.f, 0.f));
	Brain->ForceState(EDeepMonsterState::Stalk);
	UTEST_EQUAL(TEXT("Stalking"), Brain->GetState(), EDeepMonsterState::Stalk);

	W.Harpoon->FireAlong(From, FVector(1.f, 0.f, 0.f));
	W.StepUntilIdle(2.f);
	TestEqual(TEXT("harpoon_hit_monster"), W.Bus->CountOf(DeepHarpoonEvent::HitMonster), 1);
	TestEqual(TEXT("It recoils"), Brain->GetState(), EDeepMonsterState::Recoil);
	TestEqual(TEXT("Nothing to reel"), W.Harpoon->GetPhase(), EDeepHarpoonPhase::Idle);
	TestEqual(TEXT("No hull damage"), W.State->HullDamage, 0);
	return true;
}

#endif
