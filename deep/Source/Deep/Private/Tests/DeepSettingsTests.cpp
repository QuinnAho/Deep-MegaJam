// The basics a player expects (Nov 15, settings pass): the settings, credits and quit screens as HUD modes chosen
// in DeepHudModel, the settings' steps and labels, the credits table, the menus driven by real key presses, and
// the settings written to the user's GameUserSettings.ini.

#include "CoreMinimal.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "Game/DeepPlayerController.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeExit.h"
#include "UI/DeepHudModel.h"
#include "UI/DeepUserSettings.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_SETTINGS_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSettingsModesTest, "Deep.UI.Settings.Modes", DEEP_SETTINGS_TEST_FLAGS)
bool FDeepSettingsModesTest::RunTest(const FString& Parameters)
{
	using namespace EDeepHudPanel;
	FDeepHudModeInputs Title;
	Title.bOnTitle = true;
	for (const TPair<uint8, EDeepHudMode>& Pair : TArray<TPair<uint8, EDeepHudMode>>{
		{ 1, EDeepHudMode::Settings }, { 2, EDeepHudMode::Credits }, { 3, EDeepHudMode::ConfirmQuit } })
	{
		Title.MenuScreen = Pair.Key;
		TestEqual(FString::Printf(TEXT("screen %d over the title"), Pair.Key), DeepHudModel::ChooseMode(Title), Pair.Value);
	}
	Title.MenuScreen = 0;
	TestEqual(TEXT("no screen: the title"), DeepHudModel::ChooseMode(Title), EDeepHudMode::Title);

	FDeepHudModeInputs Paused;
	Paused.bHasDiver = true;
	Paused.bPauseOpen = true;
	Paused.MenuScreen = 1;
	TestEqual(TEXT("settings over the pause menu"), DeepHudModel::ChooseMode(Paused), EDeepHudMode::Settings);
	FDeepHudModeInputs Review;
	Review.bHasDiver = true;
	Review.RunState = static_cast<uint8>(EDeepRunState::Won);
	Review.MenuScreen = 2;
	TestEqual(TEXT("credits over the review"), DeepHudModel::ChooseMode(Review), EDeepHudMode::Credits);
	FDeepHudModeInputs Diving;
	Diving.bHasDiver = true;
	Diving.MenuScreen = 1;
	TestEqual(TEXT("a stale screen never covers the water"), DeepHudModel::ChooseMode(Diving), EDeepHudMode::Water);

	// Each screen is its card alone; over the title the black title card stays behind it.
	FDeepHudModeInputs In;
	TestEqual(TEXT("settings: the card only"), DeepHudModel::PanelsFor(EDeepHudMode::Settings, In), static_cast<uint32>(SettingsCard));
	TestEqual(TEXT("credits: the card only"), DeepHudModel::PanelsFor(EDeepHudMode::Credits, In), static_cast<uint32>(CreditsCard));
	TestEqual(TEXT("quit: the card only"), DeepHudModel::PanelsFor(EDeepHudMode::ConfirmQuit, In), static_cast<uint32>(ConfirmCard));
	In.bOnTitle = true;
	TestEqual(TEXT("settings over the title"), DeepHudModel::PanelsFor(EDeepHudMode::Settings, In), static_cast<uint32>(SettingsCard | TitleCard));
	TestEqual(TEXT("quit over the title"), DeepHudModel::PanelsFor(EDeepHudMode::ConfirmQuit, In), static_cast<uint32>(ConfirmCard | TitleCard));
	TestFalse(TEXT("no other mode shows a screen card"),
		(DeepHudModel::PanelsFor(EDeepHudMode::Pause, In) | DeepHudModel::PanelsFor(EDeepHudMode::Water, In) | DeepHudModel::PanelsFor(EDeepHudMode::Title, In))
		& (SettingsCard | CreditsCard | ConfirmCard));
	TestEqual(TEXT("mode name"), FString(DeepHudModel::ModeName(EDeepHudMode::ConfirmQuit)), FString(TEXT("confirm_quit")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSettingsRulesTest, "Deep.UI.Settings.Rules", DEEP_SETTINGS_TEST_FLAGS)
bool FDeepSettingsRulesTest::RunTest(const FString& Parameters)
{
	using namespace DeepSettingsRules;
	TestEqual(TEXT("volume up a step"), StepVolume(0.8f, 1), 0.85f, 1e-4f);
	TestEqual(TEXT("volume stops at 100%"), StepVolume(1.f, 1), 1.f, 1e-4f);
	TestEqual(TEXT("volume stops at 0%"), StepVolume(0.f, -1), 0.f, 1e-4f);
	TestEqual(TEXT("volume snaps to the step"), StepVolume(0.83f, -1), 0.8f, 1e-4f);
	TestEqual(TEXT("volume label"), VolumeLabel(0.85f), FString(TEXT("85%")));
	TestEqual(TEXT("sensitivity up"), StepSensitivity(1.f, 1), 1.1f, 1e-4f);
	TestEqual(TEXT("sensitivity floor"), StepSensitivity(SensitivityMin, -1), SensitivityMin, 1e-4f);
	TestEqual(TEXT("sensitivity ceiling"), StepSensitivity(SensitivityMax, 1), SensitivityMax, 1e-4f);
	TestEqual(TEXT("sensitivity label"), SensitivityLabel(1.f), FString(TEXT("1.0")));
	TestTrue(TEXT("the present sensitivity (1.0) sits inside the range"), SensitivityBar(1.f) > 0.f && SensitivityBar(1.f) < 1.f);
	const FVector2D Look = ScaleLook(FVector2D(1.0, 2.0), 0.5f, 2.f, false);
	TestEqual(TEXT("look scaled: yaw"), Look.X, 1.0, 1e-6);
	TestEqual(TEXT("look scaled: pitch"), Look.Y, 2.0, 1e-6);
	TestEqual(TEXT("invert flips pitch only"), ScaleLook(FVector2D(1.0, 2.0), 1.f, 1.f, true), FVector2D(1.0, -2.0));
	TestEqual(TEXT("five settings rows with back"), static_cast<int32>(EDeepSettingRow::Count), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSettingsCreditsTest, "Deep.UI.Settings.Credits", DEEP_SETTINGS_TEST_FLAGS)
bool FDeepSettingsCreditsTest::RunTest(const FString& Parameters)
{
	const TArray<FDeepCreditRow> Rows = DeepCredits::Parse(TEXT("item,author,licence,note\nTool,Someone,MIT,a note\n,,,\nAsset,Them,CC-BY 4.0,\n"));
	UTEST_EQUAL(TEXT("two rows, the empty one skipped"), Rows.Num(), 2);
	TestEqual(TEXT("item"), Rows[0].Item, FString(TEXT("Tool")));
	TestEqual(TEXT("licence"), Rows[1].Licence, FString(TEXT("CC-BY 4.0")));
	TestEqual(TEXT("note"), Rows[0].Note, FString(TEXT("a note")));
	// The shipped file: the engine and the sound row.
	const TArray<FDeepCreditRow>& Shipped = DeepCredits::Get();
	TestTrue(TEXT("Credits.csv lists Unreal Engine"), Shipped.ContainsByPredicate([](const FDeepCreditRow& R) { return R.Item.Contains(TEXT("Unreal Engine")); }));
	TestTrue(TEXT("Credits.csv says the sound is generated in code"), Shipped.ContainsByPredicate([](const FDeepCreditRow& R) { return R.Author.Contains(TEXT("code")) || R.Note.Contains(TEXT("code")) || R.Note.Contains(TEXT("synthesiser")); }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSettingsSaveTest, "Deep.UI.Settings.Save", DEEP_SETTINGS_TEST_FLAGS)
bool FDeepSettingsSaveTest::RunTest(const FString& Parameters)
{
	UDeepUserSettings& Settings = UDeepUserSettings::Get();
	TestEqual(TEXT("saved to the user's GameUserSettings config"), UDeepUserSettings::StaticClass()->ClassConfigName, FName(TEXT("GameUserSettings")));
	const float OldVolume = Settings.MasterVolume;
	const bool bOldInvert = Settings.bInvertLook;
	Settings.MasterVolume = 0.35f;
	Settings.bInvertLook = !bOldInvert;
	Settings.Save();
	// Read back what is in the user's GameUserSettings.ini, as the next start would.
	const FString Section = UDeepUserSettings::StaticClass()->GetPathName();
	float Saved = -1.f;
	bool bSavedInvert = bOldInvert;
	TestTrue(TEXT("volume is in the file"), GConfig->GetFloat(*Section, TEXT("MasterVolume"), Saved, GGameUserSettingsIni));
	GConfig->GetBool(*Section, TEXT("bInvertLook"), bSavedInvert, GGameUserSettingsIni);
	TestEqual(TEXT("volume round trip"), Saved, 0.35f, 1e-4f);
	TestEqual(TEXT("invert round trip"), bSavedInvert, !bOldInvert);
	UDeepUserSettings* Fresh = NewObject<UDeepUserSettings>();
	Fresh->LoadConfig();
	TestEqual(TEXT("a fresh object loads the saved volume"), Fresh->MasterVolume, 0.35f, 1e-4f);
	Settings.MasterVolume = OldVolume;
	Settings.bInvertLook = bOldInvert;
	Settings.Save();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSettingsMenuKeysTest, "Deep.UI.Settings.MenuKeys", DEEP_SETTINGS_TEST_FLAGS)
bool FDeepSettingsMenuKeysTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
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
	ADeepSubmarine* Sub = GameMode ? GameMode->EnsureArena() : nullptr;
	UTEST_NOT_NULL(TEXT("arena"), Sub);
	GameMode->StartPlay();
	ADeepGameState* State = World->GetGameState<ADeepGameState>();
	ADeepDiver* Diver = World->SpawnActor<ADeepDiver>(Sub->GetDiverStartTransform().GetLocation(), FRotator::ZeroRotator);
	ADeepPlayerController* PC = World->SpawnActor<ADeepPlayerController>();
	UTEST_TRUE(TEXT("state, diver, controller"), State && Diver && PC);
	PC->Possess(Diver);
	auto Press = [PC](const FKey& Key) { return PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key, IE_Pressed, 1.f)); };
	using EScreen = ADeepPlayerController::EMenuScreen;
	UDeepUserSettings& Settings = UDeepUserSettings::Get();
	const float OldVolume = Settings.MasterVolume;
	const float OldSensitivity = Settings.LookSensitivity;
	const bool bOldInvert = Settings.bInvertLook;

	// The title: down to Settings, open it; nothing starts.
	GEngine->Exec(World, TEXT("Deep.Title"));
	UTEST_TRUE(TEXT("on the title"), State->bOnTitle);
	Press(EKeys::Down);
	TestEqual(TEXT("down moves the title cursor"), PC->GetMenuIndex(), 1);
	Press(EKeys::Enter);
	TestEqual(TEXT("enter on SETTINGS opens settings"), PC->GetMenuScreen(), EScreen::Settings);
	TestTrue(TEXT("still on the title"), State->bOnTitle);

	// Volume: right raises it a step at once; sensitivity; invert.
	Settings.MasterVolume = 0.5f;
	Press(EKeys::Right);
	TestEqual(TEXT("right raises the volume"), Settings.MasterVolume, 0.55f, 1e-4f);
	Press(EKeys::Down);
	Settings.LookSensitivity = 1.f;
	Press(EKeys::Left);
	TestEqual(TEXT("left lowers the sensitivity"), Settings.LookSensitivity, 0.9f, 1e-4f);
	Press(EKeys::Down);
	Settings.bInvertLook = false;
	Press(EKeys::Enter);
	TestTrue(TEXT("enter flips invert"), Settings.bInvertLook);
	Press(EKeys::K);
	TestEqual(TEXT("a stray key keeps settings open"), PC->GetMenuScreen(), EScreen::Settings);
	TestTrue(TEXT("and the title"), State->bOnTitle);
	Press(EKeys::Escape);
	TestEqual(TEXT("esc goes back"), PC->GetMenuScreen(), EScreen::None);
	TestTrue(TEXT("back on the title, not diving"), State->bOnTitle);

	// Credits from the title, and back.
	Press(EKeys::Down);
	Press(EKeys::Enter);
	TestEqual(TEXT("credits from the title"), PC->GetMenuScreen(), EScreen::Credits);
	Press(EKeys::Enter);
	TestEqual(TEXT("enter closes credits"), PC->GetMenuScreen(), EScreen::None);

	// Quit asks once: from the title it opens the confirmation on STAY; stay keeps the game.
	Press(EKeys::Down);
	Press(EKeys::Enter);
	TestEqual(TEXT("quit opens the confirmation"), PC->GetMenuScreen(), EScreen::ConfirmQuit);
	TestEqual(TEXT("the confirmation starts on stay"), PC->GetScreenIndex(), 0);
	Press(EKeys::Enter);
	TestEqual(TEXT("stay closes it"), PC->GetMenuScreen(), EScreen::None);
	TestTrue(TEXT("still on the title"), State->bOnTitle);

	// Start from the title menu, then the pause menu: settings and quit from there.
	while (PC->GetMenuIndex() != 0)
	{
		Press(EKeys::Down);
	}
	Press(EKeys::Enter);
	TestFalse(TEXT("START THE DIVE leaves the title"), State->bOnTitle);
	Press(EKeys::Escape);
	UTEST_TRUE(TEXT("esc opens the pause menu"), PC->IsPauseMenuOpen());
	Press(EKeys::Down);
	Press(EKeys::Enter);
	TestEqual(TEXT("settings from the pause menu"), PC->GetMenuScreen(), EScreen::Settings);
	Press(EKeys::Escape);
	TestEqual(TEXT("esc in settings goes back to the pause menu"), PC->GetMenuScreen(), EScreen::None);
	TestTrue(TEXT("still paused"), PC->IsPauseMenuOpen());
	for (int32 I = 0; I < 3; ++I)
	{
		Press(EKeys::Down);
	}
	Press(EKeys::Enter);
	TestEqual(TEXT("quit from the pause menu asks first"), PC->GetMenuScreen(), EScreen::ConfirmQuit);
	Press(EKeys::Escape);
	TestEqual(TEXT("esc on the confirmation stays"), PC->GetMenuScreen(), EScreen::None);
	TestTrue(TEXT("still paused"), PC->IsPauseMenuOpen());
	PC->SetPauseMenu(false);

	Settings.MasterVolume = OldVolume;
	Settings.LookSensitivity = OldSensitivity;
	Settings.bInvertLook = bOldInvert;
	Settings.Save();
	return true;
}

#endif
