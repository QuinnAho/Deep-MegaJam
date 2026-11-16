// The first sight (state Pass): once per run, before its first hunt, a sound, then one slow pass across the diver's
// lamp at the edge of its reach, then it leaves and hunts as before. The old rules are untouched when it is not
// pending (Deep.Sim.Monster.States still covers them).

#include "CoreMinimal.h"
#include "AI/AISystemBase.h"
#include "Components/SpotLightComponent.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/DeepGameMode.h"
#include "Misc/AutomationTest.h"
#include "Monster/DeepMonsterBrain.h"
#include "Monster/DeepMonsterRules.h"
#include "Monster/DeepMonsterSettings.h"
#include "Monster/DeepSerpent.h"
#include "Monster/DeepSerpentController.h"
#include "Perception/AIPerceptionSystem.h"
#include "Story/DeepEventBus.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DeepFirstSightTest
{
	using EState = EDeepMonsterState;

	FDeepMonsterTuning Tuning()
	{
		FDeepMonsterTuning T;
		T.StalkNoise = 60.f;
		T.CalmNoise = 25.f;
		T.LitToRecoilS = 0.75f;
		T.FirstSightAfterS = 20.f;
		T.PassMaxS = 25.f;
		return T;
	}

	FDeepMonsterInputs In(EState State, bool bPending)
	{
		FDeepMonsterInputs I;
		I.State = State;
		I.StateSeconds = 1.f;
		I.TetherDistanceM = 30.f;
		I.DiverDistanceM = 35.f;
		I.bDiverTethered = true;
		I.bHasDiver = true;
		I.bFirstSightPending = bPending;
		return I;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMonsterFirstSightRulesTest, "Deep.Sim.Monster.FirstSight",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepMonsterFirstSightRulesTest::RunTest(const FString& Parameters)
{
	using namespace DeepFirstSightTest;
	const FDeepMonsterTuning T = Tuning();

	// Lurk, pending: what would bring it in brings the pass instead.
	FDeepMonsterInputs I = In(EState::Lurk, true);
	I.Noise = 60.f;
	FDeepMonsterStep S = UDeepMonsterRules::NextState(I, T);
	TestEqual(TEXT("Pending + noise: the pass, not a stalk"), S.Next, EState::Pass);
	TestTrue(TEXT("Pass started event"), S.bPassStarted);
	TestFalse(TEXT("No bite or repel on the way in"), S.bBiteStarted || S.bRepelled || S.bStartCooldown);

	I = In(EState::Lurk, true);
	I.bDiverTethered = false;
	TestEqual(TEXT("Pending + untethered: the pass before the first hunt"), UDeepMonsterRules::NextState(I, T).Next, EState::Pass);

	I = In(EState::Lurk, true);
	I.StateSeconds = 19.f;
	TestEqual(TEXT("Pending, quiet, early: still lurking"), UDeepMonsterRules::NextState(I, T).Next, EState::Lurk);
	I.StateSeconds = 20.f;
	TestEqual(TEXT("Pending, quiet, FirstSightAfterS: the pass"), UDeepMonsterRules::NextState(I, T).Next, EState::Pass);

	I = In(EState::Lurk, true);
	I.Noise = 100.f;
	I.CooldownS = 3.f;
	TestEqual(TEXT("Pending but shaken (cooldown): nothing"), UDeepMonsterRules::NextState(I, T).Next, EState::Lurk);

	I = In(EState::Lurk, true);
	I.LitSeconds = 1.f;
	TestEqual(TEXT("Pending but lit: it recoils"), UDeepMonsterRules::NextState(I, T).Next, EState::Recoil);

	// Not pending: exactly the old rules.
	I = In(EState::Lurk, false);
	I.Noise = 60.f;
	S = UDeepMonsterRules::NextState(I, T);
	TestEqual(TEXT("Not pending + noise: stalk as before"), S.Next, EState::Stalk);
	TestFalse(TEXT("Not pending: no pass event"), S.bPassStarted);
	I = In(EState::Lurk, false);
	I.StateSeconds = 100.f;
	TestEqual(TEXT("Not pending, quiet, late: lurk as before"), UDeepMonsterRules::NextState(I, T).Next, EState::Lurk);

	// Pass: noise and the tether do not interrupt it; the lamp does; it ends when out in the dark or on time.
	I = In(EState::Pass, false);
	I.Noise = 100.f;
	I.bDiverTethered = false;
	I.TetherDistanceM = 1.f;
	I.DiverDistanceM = 1.f;
	S = UDeepMonsterRules::NextState(I, T);
	TestEqual(TEXT("Pass ignores noise, the tether and distance"), S.Next, EState::Pass);
	TestFalse(TEXT("Pass never bites or catches"), S.bBiteStarted || S.bCaught || S.bSevered);

	I = In(EState::Pass, false);
	I.LitSeconds = 0.75f;
	I.bLitNow = true;
	TestEqual(TEXT("A head crossing the beam does not end the pass"), UDeepMonsterRules::NextState(I, T).Next, EState::Pass);
	I.LitSeconds = T.PassLitToRecoilS;
	S = UDeepMonsterRules::NextState(I, T);
	TestEqual(TEXT("Beam held on its head in the pass: recoil"), S.Next, EState::Recoil);
	TestTrue(TEXT("...and that counts as the lamp driving it off"), S.bRepelled);

	I = In(EState::Pass, false);
	I.bPassComplete = true;
	S = UDeepMonsterRules::NextState(I, T);
	TestEqual(TEXT("Pass complete: lurk (normal hunting from here)"), S.Next, EState::Lurk);
	TestFalse(TEXT("No cooldown after a pass"), S.bStartCooldown);

	I = In(EState::Pass, false);
	I.StateSeconds = 25.f;
	TestEqual(TEXT("Pass times out: lurk"), UDeepMonsterRules::NextState(I, T).Next, EState::Lurk);

	TestEqual(TEXT("State name"), UDeepMonsterRules::StateName(EState::Pass), FString(TEXT("Pass")));
	TestTrue(TEXT("Shipped: the first sight is on"), UDeepMonsterSettings::Get().Behaviour.bFirstSight);
	const FDeepMonsterTuning Shipped = UDeepMonsterSettings::Get().GetTuning(2);
	TestTrue(TEXT("Shipped: the pass is at the edge of the lamp's reach (6 to 8 m)"), Shipped.PassDistanceM >= 6.f && Shipped.PassDistanceM <= 8.f);
	TestTrue(TEXT("Shipped: the pass is slow"), Shipped.PassSpeedMps <= Shipped.LurkSpeedMps);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMonsterFirstSightWorldTest, "Deep.World.Monster.FirstSight",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepMonsterFirstSightWorldTest::RunTest(const FString& Parameters)
{
	constexpr float Dt = 0.1f;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->CreateAISystem();
	World->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT
	{
		if (World->HasBegunPlay())
		{
			World->EndPlay(EEndPlayReason::Quit);
		}
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};

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
	UTEST_TRUE(TEXT("Run, brain and bus"), Run && Brain && Bus);

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

	GameMode->GotoStop(2);
	StepAll(Dt);
	ADeepSerpent* Serpent = Brain->GetSerpent();
	ADeepSerpentController* Mind = Brain->GetSerpentController();
	UTEST_TRUE(TEXT("The serpent is there at stop 2"), Serpent && Mind);
	TestFalse(TEXT("No first sight yet"), Mind->IsFirstSightDone());
	if (!Diver->IsLampOn())
	{
		Diver->ToggleLamp();
	}
	USpotLightComponent* Lamp = Diver->FindComponentByClass<USpotLightComponent>();
	UTEST_NOT_NULL(TEXT("Lamp"), Lamp);
	const FDeepMonsterTuning Tuning = UDeepMonsterSettings::Get().GetTuning(2);

	int32 Stalks = 0;
	Brain->OnStalkStarted.AddLambda([&](ADeepDiver*) { ++Stalks; });

	// The console command's path: a sound, then the head placed out of view.
	UTEST_TRUE(TEXT("The first sight starts"), Brain->StartFirstSight());
	TestEqual(TEXT("State Pass"), Brain->GetState(), EDeepMonsterState::Pass);
	TestTrue(TEXT("It counts as this run's first sight"), Mind->IsFirstSightDone());
	TestEqual(TEXT("The sound comes first (one first-heard broadcast)"), Stalks, 1);
	TestEqual(TEXT("monster_first_heard fired once"), Bus->CountOf(DeepEvent::MonsterFirstHeard), 1);
	const FVector LampAt = Lamp->GetComponentLocation();
	const FVector ToHead = (Serpent->GetHeadLocation() - LampAt).GetSafeNormal();
	const float StartAngle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(ToHead, Lamp->GetForwardVector())));
	TestTrue(FString::Printf(TEXT("It starts out of view (%.0f deg off the lamp axis)"), StartAngle), StartAngle > 50.f);

	// Slow pass: part of it crosses the beam at the edge of its reach, noise does not turn it into a hunt.
	bool bBodyInBeam = false;
	bool bAnyOtherState = false;
	int32 Steps = 0;
	const int32 MaxSteps = FMath::CeilToInt(Tuning.PassMaxS / Dt) + 5;
	for (; Steps < MaxSteps && Brain->GetState() == EDeepMonsterState::Pass; ++Steps)
	{
		if (Steps == 30)
		{
			Brain->AddNoise(100.f);
		}
		StepAll(Dt);
		// The body follows the head's path exactly, so the head's path through the beam is where the body crosses it.
		bBodyInBeam |= UDeepMonsterRules::IsInBeam(Lamp->GetComponentLocation(), Lamp->GetForwardVector(), UDeepMonsterSettings::Get().Behaviour.BeamHalfAngleDeg,
			Lamp->AttenuationRadius / 100.f, Serpent->GetHeadLocation(), 0.f);
	}
	const float Closest = Mind->GetPassClosestM();
	TestTrue(TEXT("Part of the body crosses the beam"), bBodyInBeam);
	TestTrue(FString::Printf(TEXT("Closest pass %.1f m: within the lamp's reach, never on top of the diver"), Closest), Closest >= 5.f && Closest <= 9.f);
	TestTrue(FString::Printf(TEXT("The pass ends by itself (%.1f s)"), Steps * Dt), Steps * Dt <= Tuning.PassMaxS + 0.5f);
	TestTrue(TEXT("Noise during the pass did not start a stalk"), Stalks == 1);
	TestEqual(TEXT("The pass ends in Lurk (the beam crossing its head did not drive it off)"), UDeepMonsterRules::StateName(Brain->GetState()), FString(TEXT("Lurk")));

	// Afterwards: normal hunting (noise is still high, so it comes in), and the pass never comes back.
	for (int32 i = 0; i < 5 && Brain->GetState() != EDeepMonsterState::Stalk; ++i)
	{
		Brain->AddNoise(100.f);
		StepAll(Dt);
	}
	TestEqual(TEXT("After the pass, noise brings a stalk"), Brain->GetState(), EDeepMonsterState::Stalk);
	TestEqual(TEXT("...and the stalk is announced"), Stalks, 2);
	TestEqual(TEXT("monster_first_heard still once per run"), Bus->CountOf(DeepEvent::MonsterFirstHeard), 1);

	// A new run brings a new serpent with its first sight still to come.
	Brain->ResetForRun();
	GameMode->GotoStop(2);
	StepAll(Dt);
	TestTrue(TEXT("New run: a new serpent, first sight to come"), Brain->GetSerpentController() && !Brain->GetSerpentController()->IsFirstSightDone());
	return true;
}

#endif
