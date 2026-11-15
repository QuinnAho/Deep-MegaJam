// Tests for the sea serpent: the pure rules in Monster/DeepMonsterRules (each test builds its own tuning),
// the shipped monster settings, and one world test driving UDeepMonsterBrain through lurk, stalk, recoil and a bite.

#include "CoreMinimal.h"
#include "Components/SpotLightComponent.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Diver/DeepTetherComponent.h"
#include "Story/DeepEventBus.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "Misc/AutomationTest.h"
#include "Monster/DeepMonsterBrain.h"
#include "Monster/DeepMonsterRules.h"
#include "Monster/DeepMonsterSettings.h"
#include "Monster/DeepSerpent.h"
#include "Monster/DeepSerpentController.h"
#include "Perception/AIPerceptionSystem.h"
#include "AI/AISystemBase.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_MONSTER_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepMonsterTest
{
	using EState = EDeepMonsterState;

	/** Fixed numbers so tuning changes never break the rule tests. */
	FDeepMonsterTuning Tuning()
	{
		FDeepMonsterTuning T;
		T.NoiseMax = 100.f;
		T.NoiseDecayPerS = 10.f;
		T.WinchNoisePerS = 30.f;
		T.SprintNoisePerS = 15.f;
		T.GrabNoise = 30.f;
		T.GrabNoisePerKg = 1.f;
		T.StalkNoise = 60.f;
		T.CalmNoise = 25.f;
		T.LitToRecoilS = 0.75f;
		T.LitDecayPerS = 1.5f;
		T.LurkRadiusM = 45.f;
		T.RecoilMaxS = 8.f;
		T.BiteReachM = 2.f;
		T.BiteAlarmS = 5.f;
		T.CatchReachM = 1.5f;
		T.ThreatFarM = 70.f;
		return T;
	}

	/** A calm, tethered, far-away starting point; each check changes one thing. */
	FDeepMonsterInputs In(EState State)
	{
		FDeepMonsterInputs I;
		I.State = State;
		I.Noise = 0.f;
		I.LitSeconds = 0.f;
		I.bLitNow = false;
		I.StateSeconds = 1.f;
		I.CooldownS = 0.f;
		I.TetherDistanceM = 30.f;
		I.DiverDistanceM = 35.f;
		I.bDiverTethered = true;
		I.bHasDiver = true;
		return I;
	}

	FDeepMonsterStep Next(const FDeepMonsterInputs& I)
	{
		return UDeepMonsterRules::NextState(I, Tuning());
	}

	bool NoEvents(const FDeepMonsterStep& S)
	{
		return !S.bBiteStarted && !S.bBiteBroken && !S.bSevered && !S.bCaught && !S.bStartCooldown && !S.bRepelled;
	}

	/** A bare game world, destroyed when this goes out of scope. */
	struct FScopedWorld
	{
		UWorld* World = nullptr;

		FScopedWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			// A bare world gets no AI system by default; the serpent's perception needs one.
			World->CreateAISystem();
			World->InitializeActorsForPlay(FURL());
		}

		~FScopedWorld()
		{
			if (World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};
}

// ---------------------------------------------------------------- noise

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMonsterNoiseTest, "Deep.Sim.Monster.Noise", DEEP_MONSTER_TEST_FLAGS)
bool FDeepMonsterNoiseTest::RunTest(const FString& Parameters)
{
	using namespace DeepMonsterTest;
	const FDeepMonsterTuning T = Tuning();

	TestEqual(TEXT("Sprint threshold is halfway between swim and sprint"), UDeepMonsterRules::SprintThresholdMps(4.f, 1.6f), 5.2f, 0.001f);

	TestEqual(TEXT("Swimming quietly makes no noise"), UDeepMonsterRules::NoiseFromDiver(false, 4.f, 5.2f, 0.f, 1.f, T), 0.f, 0.001f);
	TestEqual(TEXT("Sprinting is loud"), UDeepMonsterRules::NoiseFromDiver(false, 6.f, 5.2f, 0.f, 1.f, T), 15.f, 0.001f);
	TestEqual(TEXT("Winching is loud"), UDeepMonsterRules::NoiseFromDiver(true, 0.f, 5.2f, 0.f, 0.5f, T), 15.f, 0.001f);
	TestEqual(TEXT("Winch speed is not counted again as sprinting"), UDeepMonsterRules::NoiseFromDiver(true, 7.f, 5.2f, 0.f, 1.f, T), 30.f, 0.001f);
	TestEqual(TEXT("A cargo rise is a grab"), UDeepMonsterRules::NoiseFromDiver(false, 0.f, 5.2f, 6.f, 0.1f, T), 36.f, 0.001f);
	TestEqual(TEXT("Dropping cargo is not a grab"), UDeepMonsterRules::NoiseFromDiver(false, 0.f, 5.2f, -6.f, 0.1f, T), 0.f, 0.001f);

	TestEqual(TEXT("Noise decays"), UDeepMonsterRules::NoiseStep(50.f, 0.f, 1.f, T), 40.f, 0.001f);
	TestEqual(TEXT("Noise never goes below zero"), UDeepMonsterRules::NoiseStep(5.f, 0.f, 1.f, T), 0.f, 0.001f);
	TestEqual(TEXT("Noise is capped"), UDeepMonsterRules::NoiseStep(90.f, 50.f, 0.f, T), 100.f, 0.001f);
	TestEqual(TEXT("Added noise and decay in one step"), UDeepMonsterRules::NoiseStep(20.f, 30.f, 0.5f, T), 45.f, 0.001f);

	// Silence always calms it: from full, quiet for (Max - Calm) / Decay seconds drops it below Calm.
	float Noise = T.NoiseMax;
	const int32 Steps = FMath::CeilToInt((T.NoiseMax - T.CalmNoise) / T.NoiseDecayPerS / 0.1f) + 1;
	for (int32 i = 0; i < Steps; ++i)
	{
		Noise = UDeepMonsterRules::NoiseStep(Noise, 0.f, 0.1f, T);
	}
	TestTrue(TEXT("Silence calms it within (Max - Calm) / Decay seconds"), Noise < T.CalmNoise);

	// Winching home for 3 s from silence brings it in.
	Noise = 0.f;
	for (int32 i = 0; i < 30; ++i)
	{
		Noise = UDeepMonsterRules::NoiseStep(Noise, UDeepMonsterRules::NoiseFromDiver(true, 7.f, 5.2f, 0.f, 0.1f, T), 0.1f, T);
	}
	TestEqual(TEXT("3 s of winching nets (30 - 10) * 3"), Noise, 60.f, 0.01f);
	return true;
}

// ---------------------------------------------------------------- lamp

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMonsterBeamTest, "Deep.Sim.Monster.Beam", DEEP_MONSTER_TEST_FLAGS)
bool FDeepMonsterBeamTest::RunTest(const FString& Parameters)
{
	using namespace DeepMonsterTest;
	const FVector Lamp(100.f, 200.f, 300.f);
	const FVector Fwd = FVector::ForwardVector;
	const float Half = 24.f;
	const float Range = 35.f;
	auto At = [&Lamp](float DistM, float AngleDeg)
	{
		return Lamp + FVector::ForwardVector.RotateAngleAxis(AngleDeg, FVector::UpVector) * (DistM * 100.f);
	};

	TestTrue(TEXT("On the axis, in range"), UDeepMonsterRules::IsInBeam(Lamp, Fwd, Half, Range, At(10.f, 0.f), 0.f));
	TestTrue(TEXT("At exactly the range"), UDeepMonsterRules::IsInBeam(Lamp, Fwd, Half, Range, At(Range, 0.f), 0.f));
	TestFalse(TEXT("Just beyond the range"), UDeepMonsterRules::IsInBeam(Lamp, Fwd, Half, Range, At(Range + 0.05f, 0.f), 0.f));
	TestTrue(TEXT("Just inside the cone edge"), UDeepMonsterRules::IsInBeam(Lamp, Fwd, Half, Range, At(20.f, Half - 0.2f), 0.f));
	TestFalse(TEXT("Just outside the cone edge"), UDeepMonsterRules::IsInBeam(Lamp, Fwd, Half, Range, At(20.f, Half + 0.2f), 0.f));
	TestTrue(TEXT("Cone edge holds in the vertical too"), UDeepMonsterRules::IsInBeam(Lamp, Fwd, Half, Range,
		Lamp + FVector::ForwardVector.RotateAngleAxis(-(Half - 0.2f), FVector::RightVector) * 2000.f, 0.f));
	TestFalse(TEXT("Behind the lamp is never lit"), UDeepMonsterRules::IsInBeam(Lamp, Fwd, Half, Range, At(5.f, 180.f), 5.f));
	TestFalse(TEXT("Sideways is never lit"), UDeepMonsterRules::IsInBeam(Lamp, Fwd, Half, Range, At(5.f, 90.f), 0.5f));
	TestTrue(TEXT("Leniency: a 1 m head just outside the cone at 10 m still counts"), UDeepMonsterRules::IsInBeam(Lamp, Fwd, Half, Range, At(10.f, Half + 3.f), 1.f));
	TestFalse(TEXT("Leniency does not reach far off axis"), UDeepMonsterRules::IsInBeam(Lamp, Fwd, Half, Range, At(10.f, Half + 10.f), 1.f));
	TestTrue(TEXT("Direction need not be normalised"), UDeepMonsterRules::IsInBeam(Lamp, Fwd * 7.f, Half, Range, At(10.f, 0.f), 0.f));
	TestFalse(TEXT("A zero direction lights nothing"), UDeepMonsterRules::IsInBeam(Lamp, FVector::ZeroVector, Half, Range, At(10.f, 0.f), 0.f));

	const FDeepMonsterTuning T = Tuning();
	TestEqual(TEXT("Lit time grows while lit"), UDeepMonsterRules::LitStep(0.5f, true, 0.25f, T), 0.75f, 0.001f);
	TestEqual(TEXT("Lit time falls out of the beam"), UDeepMonsterRules::LitStep(0.5f, false, 0.2f, T), 0.2f, 0.001f);
	TestEqual(TEXT("Lit time never below zero"), UDeepMonsterRules::LitStep(0.1f, false, 1.f, T), 0.f, 0.001f);
	return true;
}

// ---------------------------------------------------------------- states

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMonsterStatesTest, "Deep.Sim.Monster.States", DEEP_MONSTER_TEST_FLAGS)
bool FDeepMonsterStatesTest::RunTest(const FString& Parameters)
{
	using namespace DeepMonsterTest;
	FDeepMonsterInputs I;
	FDeepMonsterStep S;

	// ---- Lurk
	S = Next(In(EState::Lurk));
	TestTrue(TEXT("Lurk: quiet stays Lurk"), S.Next == EState::Lurk && NoEvents(S));
	I = In(EState::Lurk); I.Noise = 59.9f;
	TestTrue(TEXT("Lurk: just under the threshold stays"), Next(I).Next == EState::Lurk);
	I.Noise = 60.f;
	TestTrue(TEXT("Lurk -> Stalk at the noise threshold"), Next(I).Next == EState::Stalk);
	I.CooldownS = 0.1f;
	TestTrue(TEXT("Lurk: cooldown blocks Stalk"), Next(I).Next == EState::Lurk);
	I = In(EState::Lurk); I.bDiverTethered = false;
	TestTrue(TEXT("Lurk -> Hunt when the diver has no tether"), Next(I).Next == EState::Hunt);
	I.CooldownS = 2.f;
	TestTrue(TEXT("Lurk: cooldown blocks Hunt"), Next(I).Next == EState::Lurk);
	I = In(EState::Lurk); I.LitSeconds = 0.75f; I.CooldownS = 3.f;
	TestTrue(TEXT("Lurk -> Recoil when held in the beam"), Next(I).Next == EState::Recoil);
	TestFalse(TEXT("Lurk: lit while not approaching is not 'repelled'"), Next(I).bRepelled);
	I = In(EState::Lurk); I.Noise = 100.f; I.bHasDiver = false;
	TestTrue(TEXT("No diver: nothing changes"), Next(I).Next == EState::Lurk);

	// ---- Stalk
	I = In(EState::Stalk); I.Noise = 40.f;
	TestTrue(TEXT("Stalk: between calm and threshold keeps stalking"), Next(I).Next == EState::Stalk);
	I.Noise = 24.9f;
	S = Next(I);
	TestTrue(TEXT("Stalk -> Lurk when it goes quiet"), S.Next == EState::Lurk && NoEvents(S));
	I = In(EState::Stalk); I.Noise = 80.f; I.TetherDistanceM = 2.f;
	S = Next(I);
	TestTrue(TEXT("Stalk -> Bite on reaching the tether"), S.Next == EState::Bite && S.bBiteStarted);
	I.Noise = 0.f;
	TestTrue(TEXT("Stalk: reaching the tether bites even if it just went quiet"), Next(I).Next == EState::Bite);
	I = In(EState::Stalk); I.Noise = 80.f; I.LitSeconds = 0.74f;
	TestTrue(TEXT("Stalk: 0.74 s in the beam is not enough"), Next(I).Next == EState::Stalk);
	I.LitSeconds = 0.75f; I.TetherDistanceM = 1.f;
	S = Next(I);
	TestTrue(TEXT("Stalk -> Recoil at 0.75 s in the beam, even at the tether"), S.Next == EState::Recoil && !S.bBiteStarted);
	TestTrue(TEXT("Stalk: the lamp counts as repelled"), S.bRepelled);
	I = In(EState::Stalk); I.Noise = 80.f; I.bDiverTethered = false;
	TestTrue(TEXT("Stalk -> Hunt when the tether is gone"), Next(I).Next == EState::Hunt);

	// ---- Bite
	I = In(EState::Bite); I.StateSeconds = 4.99f; I.TetherDistanceM = 0.5f;
	S = Next(I);
	TestTrue(TEXT("Bite: holds through the alarm"), S.Next == EState::Bite && NoEvents(S));
	I.StateSeconds = 5.f;
	S = Next(I);
	TestTrue(TEXT("Bite -> Recoil and severed when the alarm runs out"), S.Next == EState::Recoil && S.bSevered && !S.bBiteBroken && !S.bRepelled);
	I.StateSeconds = 4.99f; I.bLitNow = true;
	S = Next(I);
	TestTrue(TEXT("Bite -> Recoil at once when the beam hits it"), S.Next == EState::Recoil && S.bBiteBroken && !S.bSevered && S.bRepelled);
	I.StateSeconds = 5.f;
	S = Next(I);
	TestTrue(TEXT("Bite: the beam wins a tie with the alarm"), S.bBiteBroken && !S.bSevered);
	I = In(EState::Bite); I.StateSeconds = 2.f; I.bDiverTethered = false;
	S = Next(I);
	TestTrue(TEXT("Bite -> Recoil quietly if something else cut the line"), S.Next == EState::Recoil && NoEvents(S));

	// ---- Recoil
	I = In(EState::Recoil); I.TetherDistanceM = 20.f; I.StateSeconds = 2.f; I.LitSeconds = 3.f; I.Noise = 100.f;
	TestTrue(TEXT("Recoil: keeps fleeing inside the lurk radius"), Next(I).Next == EState::Recoil);
	I.TetherDistanceM = 45.f;
	S = Next(I);
	TestTrue(TEXT("Recoil -> Lurk at the lurk radius, cooldown starts"), S.Next == EState::Lurk && S.bStartCooldown);
	I.TetherDistanceM = 10.f; I.StateSeconds = 8.f;
	S = Next(I);
	TestTrue(TEXT("Recoil -> Lurk after RecoilMaxS even if not far enough"), S.Next == EState::Lurk && S.bStartCooldown);

	// ---- Hunt
	I = In(EState::Hunt); I.bDiverTethered = false; I.DiverDistanceM = 10.f;
	TestTrue(TEXT("Hunt: chases"), Next(I).Next == EState::Hunt);
	I.DiverDistanceM = 1.5f;
	S = Next(I);
	TestTrue(TEXT("Hunt -> Recoil and caught on contact"), S.Next == EState::Recoil && S.bCaught && !S.bRepelled);
	I.DiverDistanceM = 10.f; I.LitSeconds = 0.75f;
	S = Next(I);
	TestTrue(TEXT("Hunt -> Recoil when held in the beam"), S.Next == EState::Recoil && !S.bCaught);
	I = In(EState::Hunt); I.bDiverTethered = true;
	TestTrue(TEXT("Hunt -> Lurk once the diver is tethered again"), Next(I).Next == EState::Lurk);

	// ---- Threat
	const FDeepMonsterTuning T = Tuning();
	TestEqual(TEXT("Threat 0 far away"), UDeepMonsterRules::Threat01(EState::Lurk, 100.f, T), 0.f, 0.001f);
	TestEqual(TEXT("Threat 1 at contact"), UDeepMonsterRules::Threat01(EState::Hunt, 1.f, T), 1.f, 0.001f);
	TestEqual(TEXT("Threat 1 while biting"), UDeepMonsterRules::Threat01(EState::Bite, 30.f, T), 1.f, 0.001f);
	const float Mid = UDeepMonsterRules::Threat01(EState::Stalk, 30.f, T);
	TestTrue(TEXT("Threat in between at mid range"), Mid > 0.f && Mid < 1.f);
	return true;
}

// ---------------------------------------------------------------- steering, body and settings

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMonsterSteeringTest, "Deep.Sim.Monster.Steering", DEEP_MONSTER_TEST_FLAGS)
bool FDeepMonsterSteeringTest::RunTest(const FString& Parameters)
{
	const FVector Turned = UDeepMonsterRules::TurnToward(FVector::ForwardVector, FVector::RightVector, 30.f);
	TestEqual(TEXT("Turn is limited to the max angle"), FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Turned, FVector::ForwardVector))), 30.0, 0.01);
	TestTrue(TEXT("Turn goes toward the wanted side"), Turned.Y > 0.f);
	TestTrue(TEXT("Small turns arrive exactly"), UDeepMonsterRules::TurnToward(FVector::ForwardVector, FVector(1.f, 0.1f, 0.f), 30.f).Equals(FVector(1.f, 0.1f, 0.f).GetSafeNormal(), 0.001));
	TestTrue(TEXT("Turning fully around still turns"), !UDeepMonsterRules::TurnToward(FVector::ForwardVector, -FVector::ForwardVector, 10.f).Equals(FVector::ForwardVector, 0.001));

	TArray<FVector> Trail = { FVector(0, 0, 0), FVector(-100, 0, 0), FVector(-100, -100, 0) };
	TestTrue(TEXT("Trail: start is the head"), UDeepMonsterRules::PointAlongTrail(Trail, 0.f).Equals(FVector::ZeroVector, 0.01));
	TestTrue(TEXT("Trail: halfway along the first piece"), UDeepMonsterRules::PointAlongTrail(Trail, 50.f).Equals(FVector(-50, 0, 0), 0.01));
	TestTrue(TEXT("Trail: follows the bend"), UDeepMonsterRules::PointAlongTrail(Trail, 150.f).Equals(FVector(-100, -50, 0), 0.01));
	TestTrue(TEXT("Trail: extends straight past the end"), UDeepMonsterRules::PointAlongTrail(Trail, 250.f).Equals(FVector(-100, -150, 0), 0.01));

	const FVector Target(0, 0, 0);
	const FVector Pos(4000, 0, 0);
	const FVector Aim = UDeepMonsterRules::SpiralAimPoint(Target, Pos, 35.f, 0.75f, 10.f);
	TestTrue(TEXT("Stalk aim is closer than where it is"), Aim.Size() < Pos.Size());
	TestTrue(TEXT("Stalk aim is off the straight line (an arc)"), FMath::Abs(Aim.Y) > 500.f);
	TestTrue(TEXT("Stalk goes straight in when close"), UDeepMonsterRules::SpiralAimPoint(Target, FVector(900, 0, 0), 35.f, 0.75f, 10.f).Equals(Target, 0.01));

	const FVector Orbit = UDeepMonsterRules::OrbitAimPoint(FVector(0, 0, 500), FVector(3000, 0, 0), 45.f, 25.f);
	TestEqual(TEXT("Lurk aim is on the circle"), FVector(Orbit.X, Orbit.Y, 0).Size(), 4500.0, 0.5);
	TestEqual(TEXT("Lurk aim is at the centre's height"), Orbit.Z, 500.0, 0.01);
	TestTrue(TEXT("Lurk aim leads around the circle"), Orbit.Y > 0.f);

	TestEqual(TEXT("Distance to a segment"), UDeepMonsterRules::DistanceToSegmentM(FVector(0, 300, 500), FVector(0, 0, 0), FVector(0, 0, 1000)), 3.f, 0.001f);
	TestEqual(TEXT("Alpha along a segment"), UDeepMonsterRules::SegmentAlpha(FVector(0, 300, 250), FVector(0, 0, 0), FVector(0, 0, 1000)), 0.25f, 0.001f);

	// The shipped settings: fair and readable.
	const UDeepMonsterSettings& Settings = UDeepMonsterSettings::Get();
	const FDeepMonsterTuning Stop2 = Settings.GetTuning(2);
	const FDeepMonsterTuning Stop3 = Settings.GetTuning(3);
	TestTrue(TEXT("Stop 3 comes back sooner"), Stop3.RecoilCooldownS < Stop2.RecoilCooldownS);
	TestTrue(TEXT("Stop 3 closes faster"), Stop3.StalkSpeedMps > Stop2.StalkSpeedMps);
	TestTrue(TEXT("It lurks beyond lamp range"), Stop2.LurkRadiusM > UDeepTuningSettings::Get().Lamp.RangeM);
	TestTrue(TEXT("Calm is below the stalk threshold"), Stop2.CalmNoise < Stop2.StalkNoise);
	TestTrue(TEXT("The bite alarm is at least 5 s"), Stop2.BiteAlarmS >= 5.f && Stop3.BiteAlarmS >= 5.f);
	TestTrue(TEXT("Noise always decays"), Stop2.NoiseDecayPerS > 0.f);
	TestTrue(TEXT("The monster starts at stop 2"), Settings.FirstStop == 2);
	return true;
}

// ---------------------------------------------------------------- world

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMonsterWorldTest, "Deep.World.Monster",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepMonsterWorldTest::RunTest(const FString& Parameters)
{
	using namespace DeepMonsterTest;
	constexpr float Dt = 0.1f;

	FScopedWorld Scoped;
	UWorld* World = Scoped.World;
	UTEST_NOT_NULL(TEXT("World created"), World);

	ADeepGameMode* GameMode = World->SpawnActor<ADeepGameMode>();
	UTEST_NOT_NULL(TEXT("Game mode spawned"), GameMode);
	ADeepSubmarine* Sub = GameMode->EnsureArena();
	UTEST_NOT_NULL(TEXT("Submarine spawned"), Sub);
	GameMode->StartPlay();
	if (UAISystemBase* AISystem = World->GetAISystem())
	{
		AISystem->StartPlay();
	}

	ADeepDiver* Diver = World->SpawnActor<ADeepDiver>(Sub->GetDiverStartTransform().GetLocation(), FRotator::ZeroRotator);
	UTEST_NOT_NULL(TEXT("Diver spawned"), Diver);

	UDeepMonsterBrain* Brain = World->GetSubsystem<UDeepMonsterBrain>();
	UTEST_NOT_NULL(TEXT("The brain exists in a game world"), Brain);

	int32 BitesStarted = 0;
	int32 Severed = 0;
	float AlarmSeconds = 0.f;
	Brain->OnBiteStarted.AddLambda([&](ADeepDiver* Who, float Seconds) { ++BitesStarted; AlarmSeconds = Seconds; });
	Brain->OnTetherSevered.AddLambda([&](ADeepDiver* Who) { ++Severed; TestTrue(TEXT("Severed names the diver"), Who == Diver); });

	// The test world is not ticked, so step what the engine would: the brain, AI perception, the serpent's controller.
	UAIPerceptionSystem* Perception = UAIPerceptionSystem::GetCurrent(*World);
	TestNotNull(TEXT("The world has Unreal's AI perception system"), Perception);
	auto StepAll = [&](float Seconds)
	{
		// Perception delivers a stimulus only once world time has moved past it, so advance time as a tick would.
		World->TimeSeconds += Seconds;
		Brain->Step(Seconds);
		if (Perception)
		{
			Perception->Tick(Seconds);
		}
		if (ADeepSerpentController* Mind = Brain->GetSerpentController())
		{
			Mind->StepMonster(Seconds);
		}
		// The run loop reports the diver's noise (winch, sprint, grabs) and carries out bites.
		Diver->GetRun()->StepRun(Seconds);
	};

	// Stop 1: no monster.
	StepAll(Dt);
	TestNull(TEXT("No serpent at stop 1"), Brain->GetSerpent());

	// Stop 2: it appears by itself, in the dark beyond the lamp.
	GameMode->GotoStop(2);
	ADeepGameState* GameState = World->GetGameState<ADeepGameState>();
	UTEST_NOT_NULL(TEXT("Game state"), GameState);
	TestEqual(TEXT("At stop 2"), GameState->Stop, 2);
	StepAll(Dt);
	ADeepSerpent* Serpent = Brain->GetSerpent();
	UTEST_NOT_NULL(TEXT("Serpent spawned at stop 2"), Serpent);
	ADeepSerpentController* Mind = Brain->GetSerpentController();
	UTEST_NOT_NULL(TEXT("An AI controller possesses the serpent"), Mind);
	TestTrue(TEXT("The controller has a perception component"), Mind->GetAIPerceptionComponent() != nullptr);
	TestTrue(TEXT("Spawned lurking"), Brain->GetState() == EDeepMonsterState::Lurk);
	TestTrue(TEXT("Spawned beyond lamp range"), Brain->GetDiverDistanceM() > UDeepTuningSettings::Get().Lamp.RangeM);
	TestTrue(TEXT("Threat is low while it lurks"), Brain->GetThreat01() < 0.5f);
	TestEqual(TEXT("A body of segments"), Serpent->GetSegmentTransforms().Num(), UDeepMonsterSettings::Get().Body.Segments);

	// Quiet: it stays out there.
	for (int32 i = 0; i < 20; ++i)
	{
		StepAll(Dt);
	}
	TestTrue(TEXT("Silence keeps it lurking"), Brain->GetState() == EDeepMonsterState::Lurk);

	// It hears the winch through the engine hearing sense (the run loop reports the winch as noise events).
	Diver->SetWinching(true);
	for (int32 i = 0; i < 3; ++i)
	{
		StepAll(Dt);
	}
	Diver->SetWinching(false);
	TestTrue(TEXT("Hearing: the winch was heard through AI perception"), Mind->GetHeardTotal() > 0.f);
	TestTrue(TEXT("A little winching is not enough to bring it in"), Brain->GetState() == EDeepMonsterState::Lurk);

	// A loud noise event brings it in.
	UDeepMonsterRules::ReportDiverNoise(World, Diver->GetActorLocation(), 70.f, Diver);
	for (int32 i = 0; i < 3 && Brain->GetState() == EDeepMonsterState::Lurk; ++i)
	{
		StepAll(Dt);
	}
	TestTrue(TEXT("Noise: it leaves Lurk"), Brain->GetState() == EDeepMonsterState::Stalk);
	const float StartDistance = Brain->GetTetherDistanceM();
	for (int32 i = 0; i < 10; ++i)
	{
		StepAll(Dt);
	}
	TestTrue(TEXT("Stalking closes on the tether"), Brain->GetTetherDistanceM() < StartDistance);

	// The real beam: put its head 20 m down the lamp's axis and hold it there.
	USpotLightComponent* Lamp = Diver->FindComponentByClass<USpotLightComponent>();
	UTEST_NOT_NULL(TEXT("Diver has a lamp"), Lamp);
	TestTrue(TEXT("Lamp is on"), Diver->IsLampOn());
	Serpent->PlaceHead(Lamp->GetComponentLocation() + Lamp->GetForwardVector() * 2000.f, -Lamp->GetForwardVector());
	bool bRecoiled = false;
	int32 StepsToRecoil = 0;
	for (int32 i = 0; i < 20 && !bRecoiled; ++i)
	{
		StepAll(Dt);
		++StepsToRecoil;
		bRecoiled = Brain->GetState() == EDeepMonsterState::Recoil;
	}
	TestTrue(TEXT("Beam held on its head: it recoils"), bRecoiled);
	TestTrue(TEXT("It takes about 0.75 s, not an instant"), StepsToRecoil >= 7 && StepsToRecoil <= 9);

	// A bite: lamp off, line hanging straight down, head on the middle of the line, stalking.
	if (Diver->IsLampOn())
	{
		Diver->ToggleLamp();
	}
	UDeepTetherComponent* Tether = Diver->GetTether();
	const FVector Anchor = Tether->GetAnchorLocation();
	Diver->TeleportDiver(Anchor - FVector(0.f, 0.f, 3000.f));
	const FVector Middle = (Anchor + Diver->GetTetherAttachLocation()) * 0.5f;
	Serpent->PlaceHead(Middle + FVector(50.f, 0.f, 0.f), -FVector::ForwardVector);
	Brain->ForceState(EDeepMonsterState::Stalk);
	Brain->AddNoise(100.f);
	StepAll(Dt);
	TestTrue(TEXT("Reaching the tether starts a bite"), Brain->GetState() == EDeepMonsterState::Bite);
	TestEqual(TEXT("One bite started"), BitesStarted, 1);
	TestEqual(TEXT("The alarm is the tuned length"), AlarmSeconds, UDeepMonsterSettings::Get().GetTuning(2).BiteAlarmS, 0.001f);
	TestTrue(TEXT("Threat is full during a bite"), Brain->GetThreat01() >= 0.999f);

	int32 StepsToSever = 0;
	for (int32 i = 0; i < 100 && Severed == 0; ++i)
	{
		StepAll(Dt);
		++StepsToSever;
		if (Severed == 0)
		{
			TestTrue(TEXT("It holds on to the line during the alarm"), Brain->GetTetherDistanceM() < 3.f);
		}
	}
	TestEqual(TEXT("A bite left alone severs the tether"), Severed, 1);
	TestTrue(TEXT("Never before the 5 s alarm"), StepsToSever * Dt >= 4.95f);

	for (int32 i = 0; i < 300; ++i)
	{
		StepAll(Dt);
	}
	TestEqual(TEXT("OnTetherSevered fires once"), Severed, 1);
	return true;
}

// ---------------------------------------------------------------- the serpent wired into the run loop

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMonsterLoopTest, "Deep.World.MonsterLoop",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepMonsterLoopTest::RunTest(const FString& Parameters)
{
	using namespace DeepMonsterTest;
	constexpr float Dt = 0.1f;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();

	FScopedWorld Scoped;
	UWorld* World = Scoped.World;
	ADeepGameMode* GameMode = World->SpawnActor<ADeepGameMode>();
	UTEST_NOT_NULL(TEXT("Game mode"), GameMode);
	ADeepSubmarine* Sub = GameMode->EnsureArena();
	UTEST_NOT_NULL(TEXT("Submarine"), Sub);
	GameMode->StartPlay();
	if (UAISystemBase* AISystem = World->GetAISystem())
	{
		AISystem->StartPlay();
	}
	ADeepDiver* Diver = World->SpawnActor<ADeepDiver>(Sub->GetDiverStartTransform().GetLocation(), FRotator::ZeroRotator);
	UTEST_NOT_NULL(TEXT("Diver"), Diver);
	UDeepRunComponent* Run = Diver->GetRun();
	UDeepMonsterBrain* Brain = World->GetSubsystem<UDeepMonsterBrain>();
	UDeepEventBus* Bus = UDeepEventBus::Get(World);
	ADeepGameState* GameState = World->GetGameState<ADeepGameState>();
	UTEST_TRUE(TEXT("Run, brain, bus and game state"), Run && Brain && Bus && GameState);

	UAIPerceptionSystem* Perception = UAIPerceptionSystem::GetCurrent(*World);
	auto StepAll = [&](float Seconds)
	{
		World->TimeSeconds += Seconds;
		Brain->Step(Seconds);
		if (Perception)
		{
			Perception->Tick(Seconds);
		}
		if (ADeepSerpentController* Mind = Brain->GetSerpentController())
		{
			Mind->StepMonster(Seconds);
		}
		Run->StepRun(Seconds);
	};
	auto LampOn = [&](bool bOn)
	{
		if (Diver->IsLampOn() != bOn)
		{
			Diver->ToggleLamp();
		}
	};
	// Lamp off, line hanging straight down, head beside the middle of the line, stalking: the next step bites.
	auto StartBite = [&]()
	{
		LampOn(false);
		const FVector Anchor = Diver->GetTether()->GetAnchorLocation();
		Diver->TeleportDiver(Anchor - FVector(0.f, 0.f, 3000.f));
		const FVector Middle = (Anchor + Diver->GetTetherAttachLocation()) * 0.5f;
		Brain->GetSerpent()->PlaceHead(Middle + FVector(50.f, 0.f, 0.f), -FVector::ForwardVector);
		Brain->ForceState(EDeepMonsterState::Stalk);
		Brain->AddNoise(100.f);
		StepAll(Dt);
	};

	// ---- Never at stop 1.
	for (int32 i = 0; i < 10; ++i)
	{
		StepAll(Dt);
	}
	TestNull(TEXT("No serpent at stop 1"), Brain->GetSerpent());
	TestEqual(TEXT("No threat at stop 1"), Run->GetMonsterThreat01(), 0.f);

	GameMode->GotoStop(2);
	StepAll(Dt);
	UTEST_NOT_NULL(TEXT("Serpent at stop 2"), Brain->GetSerpent());
	ADeepSerpentController* Mind = Brain->GetSerpentController();
	UTEST_NOT_NULL(TEXT("Controller"), Mind);

	// ---- Noise is counted once: one second of winching is the winch rate, not double.
	const float HeardBefore = Mind->GetHeardTotal();
	Diver->SetWinching(true);
	for (int32 i = 0; i < 10; ++i)
	{
		StepAll(Dt);
	}
	Diver->SetWinching(false);
	StepAll(Dt);
	const float WinchHeard = Mind->GetHeardTotal() - HeardBefore;
	const float WinchRate = UDeepMonsterSettings::Get().GetTuning(2).WinchNoisePerS;
	TestTrue(FString::Printf(TEXT("One second of winch is heard once (%.1f of %.1f)"), WinchHeard, WinchRate), WinchHeard >= WinchRate * 0.7f && WinchHeard <= WinchRate * 1.3f);

	// ---- Noise brings it in: the Captain says "did you hear that?" once.
	Brain->AddNoise(90.f);
	StepAll(Dt);
	TestTrue(TEXT("Noise: stalking"), Brain->GetState() == EDeepMonsterState::Stalk);
	TestEqual(TEXT("monster_first_heard fired"), Bus->CountOf(DeepEvent::MonsterFirstHeard), 1);

	// ---- A bite left alone ends in the Cut state exactly once, through the loop's own Cut.
	StartBite();
	UTEST_TRUE(TEXT("Biting"), Brain->GetState() == EDeepMonsterState::Bite);
	TestTrue(TEXT("The alarm is up"), Run->IsBiteAlarmOn());
	TestEqual(TEXT("The alarm counts from 5 s"), Run->GetBiteAlarmLeft(), UDeepMonsterSettings::Get().GetTuning(2).BiteAlarmS, 0.11f);
	TestEqual(TEXT("bite_alarm fired"), Bus->CountOf(DeepEvent::BiteAlarm), 1);
	TestTrue(TEXT("Threat is full during the bite"), Run->GetMonsterThreat01() >= 0.999f);
	const float PatienceBefore = Run->GetPatienceSeconds();
	StepAll(1.f);
	// The air line (rescope, Nov 15): Patience no longer drains, not even during a bite; the bite itself is the threat.
	TestEqual(TEXT("Patience holds during a bite"), PatienceBefore - Run->GetPatienceSeconds(), 0.f, 0.01f);
	int32 CutSteps = 0;
	while (Run->GetState() == EDeepRunState::Tethered && CutSteps < 100)
	{
		TestTrue(TEXT("The alarm stays up until the line goes"), Run->IsBiteAlarmOn());
		StepAll(Dt);
		++CutSteps;
	}
	TestEqual(TEXT("An unbroken bite ends in Cut"), Run->GetState(), EDeepRunState::Cut);
	TestTrue(TEXT("Not before the alarm ran out"), 1.f + CutSteps * Dt >= 4.9f);
	TestEqual(TEXT("cut_bite fired once"), Bus->CountOf(DeepEvent::CutBite), 1);
	TestEqual(TEXT("Not a Patience cut"), Bus->CountOf(DeepEvent::CutPatience), 0);
	TestFalse(TEXT("The alarm is down after the cut"), Run->IsBiteAlarmOn());
	TestFalse(TEXT("The serpent knows the diver is loose"), Brain->IsTethered(Diver));
	for (int32 i = 0; i < 5; ++i)
	{
		StepAll(Dt);
	}
	TestEqual(TEXT("Still exactly one cut_bite"), Bus->CountOf(DeepEvent::CutBite), 1);

	// ---- Caught while untethered: death by the serpent, then the next diver.
	const int32 DiverBefore = GameState->DiverNumber;
	Brain->GetSerpent()->PlaceHead(Diver->GetActorLocation() + FVector(50.f, 0.f, 0.f), -FVector::ForwardVector);
	Brain->ForceState(EDeepMonsterState::Hunt);
	StepAll(Dt);
	TestEqual(TEXT("Caught untethered: Dead"), Run->GetState(), EDeepRunState::Dead);
	TestEqual(TEXT("death_monster fired"), Bus->CountOf(DeepEvent::DeathMonster), 1);
	TestEqual(TEXT("Not an air death"), Bus->CountOf(DeepEvent::DeathAir), 0);
	TestTrue(TEXT("The death screen names the serpent"), Run->WasLastDeathMonster());
	Run->StepRun(Tuning.Run.DeathScreenSeconds + 0.1f);
	// The replacement diver starts aboard (D-022) and drops through the hatch to dive again.
	TestEqual(TEXT("Respawned aboard"), Run->GetState(), EDeepRunState::Aboard);
	TestTrue(TEXT("Drops through the hatch"), Run->LeaveSub());
	TestEqual(TEXT("Tethered again"), Run->GetState(), EDeepRunState::Tethered);
	TestEqual(TEXT("Diver number goes up"), GameState->DiverNumber, DiverBefore + 1);
	TestTrue(TEXT("The serpent knows the new diver is tethered"), Brain->IsTethered(Diver));

	// ---- The lamp during a bite releases it and the tether stays.
	StartBite();
	UTEST_TRUE(TEXT("Biting again"), Brain->GetState() == EDeepMonsterState::Bite);
	TestTrue(TEXT("Alarm up again"), Run->IsBiteAlarmOn());
	LampOn(true);
	USpotLightComponent* Lamp = Diver->FindComponentByClass<USpotLightComponent>();
	UTEST_NOT_NULL(TEXT("Lamp"), Lamp);
	Brain->GetSerpent()->PlaceHead(Lamp->GetComponentLocation() + Lamp->GetForwardVector() * 1500.f, -Lamp->GetForwardVector());
	StepAll(Dt);
	TestTrue(TEXT("The beam breaks the bite at once"), Brain->GetState() == EDeepMonsterState::Recoil);
	TestFalse(TEXT("Alarm down"), Run->IsBiteAlarmOn());
	TestEqual(TEXT("monster_repelled fired"), Bus->CountOf(DeepEvent::MonsterRepelled), 1);
	for (int32 i = 0; i < 80; ++i)
	{
		StepAll(Dt);
	}
	TestEqual(TEXT("The tether stays"), Run->GetState(), EDeepRunState::Tethered);
	TestEqual(TEXT("No second cut_bite"), Bus->CountOf(DeepEvent::CutBite), 1);

	// ---- A new run goes back to stop 1, and the serpent goes with it.
	Run->ResetRun();
	StepAll(Dt);
	TestNull(TEXT("No serpent after a reset to stop 1"), Brain->GetSerpent());
	return true;
}

// The last ascent's budget, from the shipped numbers (the ending is winnable only while these hold):
// winching the relic home from the sinkhole takes less time than the serpent needs to come back after one lamp repel
// (turn, flee to its lurk ring, cooldown, close in again), so one well-aimed repel brings a careful player home;
// and a cut diver who drops the relic can swim home from the bottom of the hole on bailout air.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMonsterLastAscentTest, "Deep.Sim.Monster.LastAscent", DEEP_MONSTER_TEST_FLAGS)
bool FDeepMonsterLastAscentTest::RunTest(const FString& Parameters)
{
	const UDeepTuningSettings& Game = UDeepTuningSettings::Get();
	const int32 Stop = Game.Arena.RelicStop;
	const FDeepMonsterTuning Serpent = UDeepMonsterSettings::Get().GetTuning(Stop);
	const float LineM = static_cast<float>(UDeepRules::RelicOffsetCm(Game.Arena, Game.GetStop(Stop).SubDepthM,
		UDeepRules::TetherLengthM(Stop, Game.Tether)).Size()) / UDeepRules::CmPerM;
	const float RelicWinchMps = UDeepRules::WinchSpeedMps(true, Game.Tether);
	const float WinchS = LineM / FMath::Max(0.1f, RelicWinchMps);
	const float ApproachS = (Serpent.LurkRadiusM - Serpent.BiteReachM) / FMath::Max(0.1f, Serpent.StalkSpeedMps);
	const float FleeS = FMath::Min(Serpent.RecoilMaxS, Serpent.RecoilTurnS + (Serpent.LurkRadiusM - Serpent.BiteReachM) / FMath::Max(0.1f, Serpent.RecoilSpeedMps));
	const float BackS = FleeS + Serpent.RecoilCooldownS + ApproachS;
	AddInfo(FString::Printf(TEXT("Relic line %.1f m, winch %.1f m/s = %.1f s; serpent bites %.1f s after stalking, back %.1f s after a repel"),
		LineM, RelicWinchMps, WinchS, ApproachS, BackS));
	TestTrue(TEXT("The relic winches slower than the serpent stalks (it can catch the line)"), RelicWinchMps < Serpent.StalkSpeedMps);
	TestTrue(TEXT("A player who ignores the serpent is bitten and cut before the winch is home"), WinchS > ApproachS + Serpent.BiteAlarmS);
	TestTrue(TEXT("Two lamp repels are enough: the winch is home before the serpent's third approach"), WinchS < 2.f * BackS);
	TestTrue(TEXT("The bite alarm leaves time to turn and aim (at least 3 s)"), Serpent.BiteAlarmS >= 3.f);
	// A cut diver with the relic is overloaded and cannot swim up; without it, a plain swim home beats the air.
	FDeepSalvageKind RelicKind;
	TestTrue(TEXT("The relic is a salvage kind"), Game.FindSalvageKind(EDeepSalvageSize::Relic, RelicKind));
	TestTrue(TEXT("The relic overloads the diver (winch only)"), UDeepRules::SolveWeight(RelicKind.Kg, Game.Weight).bOverloaded);
	const float SwimHomeS = LineM / FMath::Max(0.1f, Game.Swim.SwimSpeedMps);
	TestTrue(TEXT("Dropped, a cut diver swims home from the hole on bailout air with 5 s to spare"), SwimHomeS + 5.f <= Game.Bailout.AirSeconds);
	return true;
}

// The last ascent's well: whatever the state, it steers for a place inside the shaft; it comes up from below; it
// flees down; and it lurks at a depth from which a stalk takes as long as one from the lurk radius in open water.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMonsterWellTest, "Deep.Sim.Monster.Well", DEEP_MONSTER_TEST_FLAGS)
bool FDeepMonsterWellTest::RunTest(const FString& Parameters)
{
	const FDeepMonsterTuning Tuning = UDeepMonsterSettings::Get().GetTuning(UDeepTuningSettings::Get().Arena.RelicStop);
	FDeepWell Well;
	Well.AxisCm = FVector2D(1000.0, -500.0);
	Well.FloorZCm = -10000.0;
	Well.TopZCm = -3500.0;
	Well.RadiusCm = 450.0;
	Well.bValid = true;
	const double InnerCm = Well.RadiusCm - Tuning.WellWallClearM * 100.0;
	TestTrue(TEXT("The path keeps clear of the wall by less than the radius"), InnerCm > 100.0);

	// A diver rising up the axis, 30 m above the floor; the open-water aims point far out into the rock.
	const FVector Diver(1000.0, -500.0, -7000.0);
	const FVector OpenWater(1000.0 + 4500.0, -500.0, -6000.0);
	const FVector2D Side(0.0, 1.0);
	const EDeepMonsterState States[] = { EDeepMonsterState::Lurk, EDeepMonsterState::Stalk, EDeepMonsterState::Recoil,
		EDeepMonsterState::Bite, EDeepMonsterState::Hunt, EDeepMonsterState::Pass };
	const FVector Heads[] = { FVector(1100.0, -500.0, -9000.0), FVector(1000.0, -300.0, -7300.0), FVector(5000.0, 3000.0, -12000.0) };
	for (const EDeepMonsterState State : States)
	{
		for (const FVector& Head : Heads)
		{
			const FVector Aim = UDeepMonsterRules::WellAimPoint(State, Well, Head, Diver, OpenWater, Side, 4.f, Tuning);
			const double Off = FVector2D::Distance(FVector2D(Aim.X, Aim.Y), Well.AxisCm);
			TestTrue(FString::Printf(TEXT("%s: the aim is inside the shaft (%.0f cm off the axis)"), *UDeepMonsterRules::StateName(State), Off), Off <= InnerCm + 1.0);
			TestTrue(FString::Printf(TEXT("%s: the aim is above the floor"), *UDeepMonsterRules::StateName(State)), Aim.Z >= Well.FloorZCm + Tuning.WellFloorClearM * 100.0 - 1.0);
		}
	}
	const FVector Below(1000.0, -500.0, -9000.0);
	const FVector StalkAim = UDeepMonsterRules::WellAimPoint(EDeepMonsterState::Stalk, Well, Below, Diver, OpenWater, Side, 4.f, Tuning);
	TestTrue(TEXT("The stalk comes up to the diver from below"), StalkAim.Z < Diver.Z && StalkAim.Z > Below.Z);
	const FVector RecoilAim = UDeepMonsterRules::WellAimPoint(EDeepMonsterState::Recoil, Well, FVector(1000.0, -400.0, -7300.0), Diver, OpenWater, Side, 4.f, Tuning);
	TestTrue(TEXT("The recoil goes down the shaft"), RecoilAim.Z < -7300.0 - 1000.0);
	const FVector LurkAim = UDeepMonsterRules::WellAimPoint(EDeepMonsterState::Lurk, Well, Below, Diver, OpenWater, Side, 4.f, Tuning);
	TestTrue(TEXT("It lurks below the diver"), LurkAim.Z < Diver.Z - 2.f * Tuning.BiteReachM * 100.0);
	TestTrue(TEXT("A point outside is moved in"), UDeepMonsterRules::IsInWell(Well, UDeepMonsterRules::KeepInWell(Well, OpenWater, Tuning)));
	const FVector PathKept = UDeepMonsterRules::KeepInWell(Well, OpenWater, Tuning, Tuning.WellPathWallClearM);
	TestTrue(TEXT("The path is held off the wall by its own clearance"), FMath::IsNearlyEqual(FVector2D::Distance(FVector2D(PathKept.X, PathKept.Y), Well.AxisCm), Well.RadiusCm - Tuning.WellPathWallClearM * 100.0, 1.0));

	// Timing kept: from the hold depth, closing at stalk speed less the rise, the stalk takes the open-water time.
	const float OpenS = (Tuning.LurkRadiusM - Tuning.BiteReachM) / Tuning.StalkSpeedMps;
	for (const float Rise : { 0.f, 2.f, 4.f })
	{
		const float HoldM = UDeepMonsterRules::WellHoldBelowM(Rise, Tuning);
		const float Closing = Tuning.StalkSpeedMps - Rise;
		AddInfo(FString::Printf(TEXT("Rise %.0f m/s: lurks %.1f m below, stalk %.1f s (open water %.1f s)"), Rise, HoldM, (HoldM - Tuning.BiteReachM) / Closing, OpenS));
		TestTrue(TEXT("Never deeper than the lurk radius"), HoldM <= Tuning.LurkRadiusM + 0.01f);
		if (HoldM > 2.f * Tuning.BiteReachM + 0.01f)
		{
			TestTrue(TEXT("The stalk from the hold takes the open-water time"), FMath::IsNearlyEqual((HoldM - Tuning.BiteReachM) / Closing, OpenS, 0.05f));
		}
	}
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS
namespace DeepChainTest
{
	// A body like the shipped one: 20 segments 70 cm apart, half a spacing at the neck and the tail; the neck 80 cm
	// behind the head. The head swims; the test keeps its trail as ADeepSerpent does (a point every 10 cm).
	constexpr int32 Segs = 20;
	constexpr float SpacingCm = 70.f;
	constexpr float HeadBackCm = 80.f;

	struct FSwim
	{
		FDeepSerpentChain Chain;
		FDeepSerpentChainTuning Tuning;
		FVector Head = FVector::ZeroVector;
		FVector Forward = FVector::ForwardVector;
		TArray<FVector> Path;

		FSwim()
		{
			Chain.RestCm.SetNum(Segs + 1);
			for (int32 i = 0; i <= Segs; ++i)
			{
				Chain.RestCm[i] = (i == 0 || i == Segs) ? SpacingCm * 0.5f : SpacingCm;
			}
			Path = { Head, Head - Forward * 2000.f };
			UDeepMonsterRules::LayChain(Chain, Path, HeadBackCm);
		}

		/** The head moves by Step along Forward (after turning Forward by YawDeg), then one frame of Dt. */
		int32 Frame(float StepCm, float YawDeg, float Dt)
		{
			Forward = FRotator(0.f, YawDeg, 0.f).RotateVector(Forward).GetSafeNormal();
			Head += Forward * StepCm;
			if (FVector::Dist(Head, Path[0]) >= 10.f)
			{
				Path.Insert(Head, 0);
			}
			TArray<FVector> Full = Path;
			Full.Insert(Head, 0);
			return UDeepMonsterRules::StepChain(Chain, Head - Forward * HeadBackCm, Forward, Full, HeadBackCm, Tuning, Dt);
		}

		float WorstSpacing01() const
		{
			float Worst = 0.f;
			for (int32 i = 0; i + 1 < Chain.Pos.Num(); ++i)
			{
				const float Len = static_cast<float>(FVector::Dist(Chain.Pos[i], Chain.Pos[i + 1]));
				Worst = FMath::Max(Worst, FMath::Abs(Len - Chain.RestCm[i]) / Chain.RestCm[i]);
			}
			return Worst;
		}

		float WorstBendDeg() const
		{
			float Worst = 0.f;
			for (int32 j = 0; j + 1 < Chain.Pos.Num(); ++j)
			{
				Worst = FMath::Max(Worst, UDeepMonsterRules::ChainBendDeg(Chain, j, Forward));
			}
			return Worst;
		}

		bool AnyNaN() const
		{
			for (int32 i = 0; i < Chain.Pos.Num(); ++i)
			{
				if (Chain.Pos[i].ContainsNaN() || Chain.Old[i].ContainsNaN())
				{
					return true;
				}
			}
			return false;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMonsterChainStraightTest, "Deep.Sim.Monster.Chain.Straight", DEEP_MONSTER_TEST_FLAGS)

bool FDeepMonsterChainStraightTest::RunTest(const FString& Parameters)
{
	using namespace DeepChainTest;
	FSwim Swim;
	// 3 m/s straight ahead for 3 s at 60 fps.
	float WorstOff = 0.f;
	for (int32 f = 0; f < 180; ++f)
	{
		TestTrue(TEXT("Substeps run at 60 fps"), Swim.Frame(5.f, 0.f, 1.f / 60.f) >= 0);
	}
	for (const FVector& P : Swim.Chain.Pos)
	{
		WorstOff = FMath::Max(WorstOff, static_cast<float>(FMath::Abs(P.Y) + FMath::Abs(P.Z)));
	}
	AddInfo(FString::Printf(TEXT("Straight swim: worst off the line %.2f cm, spacing off by %.2f%%"), WorstOff, Swim.WorstSpacing01() * 100.f));
	TestTrue(TEXT("A head moving straight leaves a straight body (within 1 cm)"), WorstOff < 1.f);
	TestTrue(TEXT("Spacing holds within 2%"), Swim.WorstSpacing01() < 0.02f);
	TestTrue(TEXT("The body moved with the head"), Swim.Chain.Pos.Last().X > 800.f - (HeadBackCm + Segs * SpacingCm) - 50.f);
	// The drawn body passes through every particle: the smooth curve meets each joint.
	float CurveMiss = 0.f;
	float AlongCm = 0.f;
	for (int32 i = 0; i < Swim.Chain.Pos.Num(); ++i)
	{
		AlongCm += i > 0 ? Swim.Chain.RestCm[i - 1] : 0.f;
		CurveMiss = FMath::Max(CurveMiss, static_cast<float>(FVector::Dist(UDeepMonsterRules::ChainPoint(Swim.Chain, AlongCm), Swim.Chain.Pos[i])));
	}
	TestTrue(TEXT("The smooth body curve passes through every joint (within 1 cm)"), CurveMiss < 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMonsterChainTurnTest, "Deep.Sim.Monster.Chain.Turn", DEEP_MONSTER_TEST_FLAGS)

bool FDeepMonsterChainTurnTest::RunTest(const FString& Parameters)
{
	using namespace DeepChainTest;
	FSwim Swim;
	const float Dt = 1.f / 60.f;
	float WorstSpacing = 0.f;
	float WorstBend = 0.f;
	auto Track = [&]()
	{
		WorstSpacing = FMath::Max(WorstSpacing, Swim.WorstSpacing01());
		WorstBend = FMath::Max(WorstBend, Swim.WorstBendDeg());
	};
	// 4 m straight at 3 m/s, then a 90 degree turn to the left at 90 deg/s (a 1.9 m radius), then on along +Y.
	for (int32 f = 0; f < 80; ++f)
	{
		Swim.Frame(5.f, 0.f, Dt);
		Track();
	}
	const FVector TailBefore = Swim.Chain.Pos.Last();
	for (int32 f = 0; f < 60; ++f)
	{
		Swim.Frame(5.f, -1.5f, Dt);
		Track();
	}
	TestTrue(TEXT("The head now swims along -Y (a left turn)"), FVector::DotProduct(Swim.Forward, FVector(0.f, -1.f, 0.f)) > 0.99f);
	const FVector TailAtTurn = Swim.Chain.Pos.Last();
	const FVector TailDirAtTurn = (Swim.Chain.Pos[Swim.Chain.Pos.Num() - 2] - TailAtTurn).GetSafeNormal();
	AddInfo(FString::Printf(TEXT("As the head finishes the turn the tail points (%.2f, %.2f) and has moved %.0f cm in X, %.0f in Y"),
		TailDirAtTurn.X, TailDirAtTurn.Y, TailAtTurn.X - TailBefore.X, TailAtTurn.Y - TailBefore.Y));
	TestTrue(TEXT("The tail lags: as the head finishes turning, the tail still swims along +X"), TailDirAtTurn.X > 0.9f);
	TestTrue(TEXT("The tail lags: it has not swung sideways yet"), FMath::Abs(TailAtTurn.Y - TailBefore.Y) < 50.f);
	// 18 m on: the whole 14.8 m body has come round the corner.
	for (int32 f = 0; f < 360; ++f)
	{
		Swim.Frame(5.f, 0.f, Dt);
		Track();
	}
	const FVector Tail = Swim.Chain.Pos.Last();
	const FVector TailDir = (Swim.Chain.Pos[Swim.Chain.Pos.Num() - 2] - Tail).GetSafeNormal();
	const float CornerX = 400.f + 190.f;
	AddInfo(FString::Printf(TEXT("After the turn the tail points (%.2f, %.2f) at x %.0f (corner about %.0f); worst spacing %.2f%%, worst bend %.1f deg"),
		TailDir.X, TailDir.Y, Tail.X, CornerX, WorstSpacing * 100.f, WorstBend));
	TestTrue(TEXT("Then the tail follows the turn: it swims along -Y"), TailDir.Y < -0.9f);
	TestTrue(TEXT("It went round where the head went (within 1.5 m of the corner's x), not across"), FMath::Abs(Tail.X - CornerX) < 150.f);
	TestTrue(TEXT("Spacing holds within 2% throughout"), WorstSpacing < 0.02f);
	TestTrue(TEXT("No joint bends past the limit"), WorstBend <= Swim.Tuning.BendLimitDeg + 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMonsterChainBendTest, "Deep.Sim.Monster.Chain.BendLimit", DEEP_MONSTER_TEST_FLAGS)

bool FDeepMonsterChainBendTest::RunTest(const FString& Parameters)
{
	using namespace DeepChainTest;
	// The head whips round on the spot (180 deg/s while barely moving) and with no path to follow: the body still never
	// folds past the limit at any joint, the neck included.
	FSwim Swim;
	Swim.Path.Reset();
	float WorstBend = 0.f;
	for (int32 f = 0; f < 120; ++f)
	{
		Swim.Forward = FRotator(0.f, 3.f, 0.f).RotateVector(Swim.Forward).GetSafeNormal();
		Swim.Head += Swim.Forward * 1.f;
		UDeepMonsterRules::StepChain(Swim.Chain, Swim.Head - Swim.Forward * HeadBackCm, Swim.Forward, TArray<FVector>(), HeadBackCm, Swim.Tuning, 1.f / 60.f);
		WorstBend = FMath::Max(WorstBend, Swim.WorstBendDeg());
	}
	AddInfo(FString::Printf(TEXT("Spinning head: worst bend %.1f deg (limit %.0f), spacing off by %.2f%%"), WorstBend, Swim.Tuning.BendLimitDeg, Swim.WorstSpacing01() * 100.f));
	TestTrue(TEXT("No joint bends past the limit"), WorstBend <= Swim.Tuning.BendLimitDeg + 0.5f);
	TestTrue(TEXT("Spacing holds within 2%"), Swim.WorstSpacing01() < 0.02f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMonsterChainStableTest, "Deep.Sim.Monster.Chain.Stable", DEEP_MONSTER_TEST_FLAGS)

bool FDeepMonsterChainStableTest::RunTest(const FString& Parameters)
{
	using namespace DeepChainTest;
	FSwim Swim;
	const TArray<FVector> Before = Swim.Chain.Pos;
	TestEqual(TEXT("A frame of 0 s takes no substep"), Swim.Frame(0.f, 0.f, 0.f), 0);
	TestFalse(TEXT("A frame of 0 s leaves no NaN"), Swim.AnyNaN());
	TestTrue(TEXT("A frame of 0 s leaves the body where it was"), Swim.Chain.Pos.Last().Equals(Before.Last(), 0.01));

	// Half a second in one frame, with the head jumping 10 m sideways: clamped, no NaN, the body still in one piece.
	const int32 Steps = Swim.Frame(0.f, 0.f, 0.f);
	Swim.Head += FVector(0.f, 1000.f, 0.f);
	const int32 BigSteps = Swim.Frame(10.f, 40.f, 0.5f);
	AddInfo(FString::Printf(TEXT("dt 0.5 s ran %d substeps (clamped to %.2f s)"), BigSteps, Swim.Tuning.MaxFrameS));
	TestTrue(TEXT("A 0.5 s frame is clamped"), BigSteps <= FMath::CeilToInt(Swim.Tuning.MaxFrameS / Swim.Tuning.SubstepS) + 1 && Steps == 0);
	TestFalse(TEXT("A 0.5 s frame leaves no NaN"), Swim.AnyNaN());
	TestTrue(TEXT("Spacing holds after a 0.5 s frame"), Swim.WorstSpacing01() < 0.02f);
	for (int32 f = 0; f < 10; ++f)
	{
		Swim.Frame(30.f, 0.f, 0.5f);
	}
	TestFalse(TEXT("Ten 0.5 s frames leave no NaN"), Swim.AnyNaN());

	// The flinch's kick: the tail moves sideways, the neck stays pinned, nothing breaks.
	FSwim Kicked;
	UDeepMonsterRules::KickChain(Kicked.Chain, FVector::UpVector, 400.f, 700.f, Kicked.Tuning.SubstepS);
	// The pull onto the trail swings it back, so look for the widest swing, not the last frame.
	float MostSideways = 0.f;
	for (int32 f = 0; f < 30; ++f)
	{
		Kicked.Frame(0.f, 0.f, 1.f / 60.f);
		for (const FVector& P : Kicked.Chain.Pos)
		{
			MostSideways = FMath::Max(MostSideways, static_cast<float>(FMath::Abs(P.Y)));
		}
	}
	AddInfo(FString::Printf(TEXT("Kick: the body swung at most %.0f cm sideways in 0.5 s"), MostSideways));
	TestTrue(TEXT("The flinch's kick throws the body sideways"), MostSideways > 10.f);
	TestFalse(TEXT("The kick leaves no NaN"), Kicked.AnyNaN());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMonsterBodyShippedTest, "Deep.Sim.Monster.Chain.Shipped", DEEP_MONSTER_TEST_FLAGS)

bool FDeepMonsterBodyShippedTest::RunTest(const FString& Parameters)
{
	const FDeepSerpentBodyTuning& Body = UDeepMonsterSettings::Get().Body;
	TestTrue(TEXT("No swim stroke by default: the body moves by the head's path and the physics only"), Body.Chain.StrokeM == 0.f && Body.Chain.StrokeDeg == 0.f);
	TestTrue(TEXT("Water resists sideways motion more than motion along the body"), Body.Chain.SideDragPerS > 0.f);
	TestTrue(TEXT("Substeps of at most 1/60 s"), Body.Chain.SubstepS > 0.f && Body.Chain.SubstepS <= 1.f / 60.f);
	TestTrue(TEXT("The bend limit lets it curl but not fold"), Body.Chain.BendLimitDeg > 5.f && Body.Chain.BendLimitDeg < 60.f);
	TestTrue(TEXT("No pack head by default"), Body.MeshHead.Mesh.IsNull());
	return true;
}
#endif
