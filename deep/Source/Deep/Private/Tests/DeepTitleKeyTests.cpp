// The title leaves on any key or mouse button through the real key path (Nov 15), and the state report
// measures the tether distance after a cut.

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
#include "Game/DeepPlayerController.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sim/DeepRules.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_TITLE_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepTitleTest
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepTitleKeyTest, "Deep.World.TitleKey", DEEP_TITLE_TEST_FLAGS)
bool FDeepTitleKeyTest::RunTest(const FString& Parameters)
{
	using namespace DeepTitleTest;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepTetherDistanceCutTest, "Deep.World.TetherDistanceAfterCut", DEEP_TITLE_TEST_FLAGS)
bool FDeepTetherDistanceCutTest::RunTest(const FString& Parameters)
{
	using namespace DeepTitleTest;
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
