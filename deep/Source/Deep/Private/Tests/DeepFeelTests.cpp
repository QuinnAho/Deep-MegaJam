// Feel pass (Nov 15): the title leaves on any key through the real key path, the HUD's one-mode-one-panel-set rule,
// the helm's motion cues, and the state report's tether distance after a cut.

#include "CoreMinimal.h"
#include "Debug/DeepStateReport.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "Game/DeepHudPanels.h"
#include "Game/DeepPlayerController.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sim/DeepRules.h"
#include "World/DeepMotionCues.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_FEEL_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepFeelTest
{
	struct FFeelWorld
	{
		UWorld* World = nullptr;
		ADeepGameMode* GameMode = nullptr;
		ADeepGameState* State = nullptr;
		ADeepSubmarine* Sub = nullptr;
		ADeepDiver* Diver = nullptr;
		UDeepRunComponent* Run = nullptr;

		FFeelWorld()
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
				Diver = World->SpawnActor<ADeepDiver>(Sub->GetDiverStartTransform().GetLocation(), FRotator::ZeroRotator);
			}
			Run = Diver ? Diver->GetRun() : nullptr;
		}

		~FFeelWorld()
		{
			if (World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		bool Valid() const { return GameMode && State && Sub && Diver && Run; }
		void Cmd(const TCHAR* Command) const { GEngine->Exec(World, Command); }
	};

	TSharedPtr<FJsonObject> StateJson(UWorld* World)
	{
		TSharedPtr<FJsonObject> Json;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(DeepStateReport::BuildJson(World));
		FJsonSerializer::Deserialize(Reader, Json);
		return Json;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepTitleKeyTest, "Deep.World.TitleKey", DEEP_FEEL_TEST_FLAGS)
bool FDeepTitleKeyTest::RunTest(const FString& Parameters)
{
	using namespace DeepFeelTest;
	FFeelWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run"), W.Valid());
	ADeepPlayerController* PC = W.World->SpawnActor<ADeepPlayerController>();
	UTEST_NOT_NULL(TEXT("Player controller"), PC);
	PC->Possess(W.Diver);

	// The same call the game viewport makes for a real key press (UGameViewportClient::InputKey -> PC->InputKey).
	auto Press = [PC](const FKey& Key, float Amount = 1.f)
	{
		return PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, Amount));
	};
	for (const FKey& Key : { EKeys::K, EKeys::SpaceBar, EKeys::Enter, EKeys::LeftMouseButton, EKeys::RightMouseButton, EKeys::Escape, EKeys::Gamepad_FaceButton_Bottom })
	{
		W.Cmd(TEXT("Deep.Title"));
		UTEST_TRUE(TEXT("On the title"), W.State->bOnTitle);
		const bool bUsed = Press(Key);
		TestTrue(FString::Printf(TEXT("%s was used by the title"), *Key.ToString()), bUsed);
		TestFalse(FString::Printf(TEXT("%s left the title"), *Key.ToString()), W.State->bOnTitle);
		TestEqual(FString::Printf(TEXT("%s: aboard"), *Key.ToString()), W.Run->GetState(), EDeepRunState::Aboard);
		TestFalse(FString::Printf(TEXT("%s: no pause menu"), *Key.ToString()), PC->IsPauseMenuOpen());
	}

	// Moving the mouse is not a key: the title stays.
	W.Cmd(TEXT("Deep.Title"));
	Press(EKeys::MouseX, 3.f);
	TestTrue(TEXT("Mouse movement keeps the title"), W.State->bOnTitle);

	// The other entry (a key that reaches PlayerInput directly, Enhanced Input AnyKey) ends in the same function.
	TestTrue(TEXT("TryStartFromTitle leaves the title"), PC->TryStartFromTitle());
	TestFalse(TEXT("Title left"), W.State->bOnTitle);
	TestFalse(TEXT("Not on the title: nothing to leave"), PC->TryStartFromTitle());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHudPanelsTest, "Deep.Sim.HudPanels", DEEP_FEEL_TEST_FLAGS)
bool FDeepHudPanelsTest::RunTest(const FString& Parameters)
{
	FDeepHudInput In;
	In.HintSeconds = 6.f;
	In.FadeSeconds = 1.5f;

	In.Mode = EDeepHudMode::Helm;
	In.bConsoleActive = true;
	FDeepHudPanels P = DeepHud::Choose(In);
	TestTrue(TEXT("Helm: helm panel"), P.bHelm);
	TestFalse(TEXT("Helm: no console panel"), P.bConsole);
	TestFalse(TEXT("Helm: no status box"), P.bStatus);
	TestFalse(TEXT("Helm: no aboard labels"), P.bAboard);

	In.Mode = EDeepHudMode::Aboard;
	P = DeepHud::Choose(In);
	TestTrue(TEXT("Aboard near the console: console panel"), P.bConsole);
	TestFalse(TEXT("Aboard: no helm"), P.bHelm);

	In.Mode = EDeepHudMode::Water;
	In.bFirstDive = true;
	In.ModeAgeSeconds = 1.f;
	P = DeepHud::Choose(In);
	TestEqual(TEXT("Water, 1 s in: controls card up"), P.ControlsCardAlpha, 1.f);
	TestEqual(TEXT("Water, 1 s in: key strip up"), P.ControlsAlpha, 1.f);
	TestFalse(TEXT("Water: no console panel"), P.bConsole);
	In.ModeAgeSeconds = 10.f;
	P = DeepHud::Choose(In);
	TestEqual(TEXT("Water, 10 s in: controls card gone"), P.ControlsCardAlpha, 0.f);
	TestEqual(TEXT("Water, 10 s in: key strip gone"), P.ControlsAlpha, 0.f);
	TestTrue(TEXT("Water: Patience"), P.bPatience);
	In.bCut = true;
	P = DeepHud::Choose(In);
	TestTrue(TEXT("Cut: bailout"), P.bBailout);
	TestFalse(TEXT("Cut: no Patience"), P.bPatience);
	TestEqual(TEXT("Fading half way"), DeepHud::HintAlpha(6.75f, 6.f, 1.5f), 0.5f, 0.001f);

	In.Mode = EDeepHudMode::Settling;
	P = DeepHud::Choose(In);
	TestTrue(TEXT("Settling: settlement"), P.bSettlement);
	TestFalse(TEXT("Settling: no status box (it overlapped, and showed the tether)"), P.bStatus);
	TestFalse(TEXT("Settling: no aboard labels"), P.bAboard);

	In.Mode = EDeepHudMode::Won;
	P = DeepHud::Choose(In);
	TestTrue(TEXT("Won: review"), P.bReview);
	TestFalse(TEXT("Won: no status box"), P.bStatus);
	TestFalse(TEXT("Won: no Captain subtitle (the review shows the line)"), P.bCaptain);
	TestEqual(TEXT("Won: no key strip"), P.ControlsAlpha, 0.f);

	In.Mode = EDeepHudMode::Dead;
	P = DeepHud::Choose(In);
	TestTrue(TEXT("Dead: dead panel"), P.bDead);
	TestFalse(TEXT("Dead: no status box"), P.bStatus);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMotionRulesTest, "Deep.Sim.MotionCues", DEEP_FEEL_TEST_FLAGS)
bool FDeepMotionRulesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Wrap inside"), DeepMotion::Wrap(3.f, 10.f), 3.f, 0.001f);
	TestEqual(TEXT("Wrap past the top"), DeepMotion::Wrap(12.f, 10.f), -8.f, 0.001f);
	TestEqual(TEXT("Wrap past the bottom"), DeepMotion::Wrap(-12.f, 10.f), 8.f, 0.001f);
	// Ledges are fixed in the world: 5 m deeper, each ledge is 5 m higher relative to the sub.
	const float A = DeepMotion::LedgeOffsetM(2, 100.f, 12.f, 8);
	const float B = DeepMotion::LedgeOffsetM(2, 105.f, 12.f, 8);
	TestEqual(TEXT("Ledge rises by the depth change"), DeepMotion::Wrap(B - A, 48.f), 5.f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepMotionCuesWorldTest, "Deep.World.MotionCues", DEEP_FEEL_TEST_FLAGS)
bool FDeepMotionCuesWorldTest::RunTest(const FString& Parameters)
{
	using namespace DeepFeelTest;
	FFeelWorld W;
	UTEST_TRUE(TEXT("World"), W.Valid());
	ADeepMotionCues* Cues = nullptr;
	for (TActorIterator<ADeepMotionCues> It(W.World); It; ++It)
	{
		Cues = *It;
	}
	UTEST_NOT_NULL(TEXT("EnsureArena spawned the motion cues"), Cues);

	// Parked: the specks sink slowly.
	const float Z0 = Cues->GetSpeckMeanZM();
	Cues->Tick(0.5f);
	TestTrue(TEXT("Parked: the specks drift"), FMath::Abs(Cues->GetSpeckMeanZM() - Z0) > 0.01f);

	// Driving down: the depth goes up 4 m per 0.5 s, the specks rise, the speed reads positive.
	W.State->bParked = false;
	for (int32 Step = 0; Step < 6; ++Step)
	{
		W.State->SubDepthM += 4.f;
		Cues->Tick(0.5f);
	}
	TestTrue(TEXT("Driving down: speed > 4 m/s"), Cues->GetSpeedMps() > 4.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepTetherDistanceCutTest, "Deep.World.TetherDistanceAfterCut", DEEP_FEEL_TEST_FLAGS)
bool FDeepTetherDistanceCutTest::RunTest(const FString& Parameters)
{
	using namespace DeepFeelTest;
	FFeelWorld W;
	UTEST_TRUE(TEXT("World"), W.Valid());
	W.Cmd(TEXT("Deep.Hatch"));
	UTEST_TRUE(TEXT("Diving"), W.Run->CanSwim());
	W.Diver->SetActorLocation(W.Sub->GetAnchorLocation() + FVector(2000.f, 0.f, -500.f));
	W.Cmd(TEXT("Deep.ForceCut"));
	TestEqual(TEXT("Cut"), W.Run->GetState(), EDeepRunState::Cut);
	const TSharedPtr<FJsonObject> Json = StateJson(W.World);
	UTEST_TRUE(TEXT("State JSON"), Json.IsValid());
	const double Distance = Json->GetNumberField(TEXT("tether_distance_m"));
	TestTrue(FString::Printf(TEXT("tether_distance_m after a cut is the real distance (%.1f m), not 0"), Distance), Distance > 15.0);
	return true;
}

#endif
