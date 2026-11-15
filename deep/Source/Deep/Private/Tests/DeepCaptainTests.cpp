// The Captain on the radio (D-040): pacing in the water, tone by danger, the tone column, and the radio switch.

#include "CoreMinimal.h"
#include "Audio/DeepAudioRules.h"
#include "Audio/DeepAudioSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepCaptain.h"
#include "Story/DeepCaptainLines.h"
#include "Story/DeepCaptainPacing.h"
#include "Story/DeepEventBus.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_CAPTAIN_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepCaptainTest
{
	struct FWorld
	{
		UWorld* World = nullptr;

		FWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}

		~FWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};

	/** Sets Deep.Captain.Radio for one scope and puts it back. */
	struct FRadio
	{
		IConsoleVariable* Var = nullptr;
		int32 Old = 1;

		explicit FRadio(int32 Value)
		{
			Var = IConsoleManager::Get().FindConsoleVariable(TEXT("Deep.Captain.Radio"));
			if (Var)
			{
				Old = Var->GetInt();
				Var->Set(Value, ECVF_SetByCode);
			}
		}

		~FRadio()
		{
			if (Var)
			{
				Var->Set(Old, ECVF_SetByCode);
			}
		}
	};

	// salvage_small = urgency 0, overweight = 1, patience_25 = 2, cut_patience = 3.
	const TCHAR* Csv = TEXT(
		"\"id\",\"event\",\"variant\",\"text\",\"voice_note\",\"tone\"\n"
		"\"s_01\",\"salvage_small\",1,\"Small.\",\"bored\",\"any\"\n"
		"\"o_01\",\"overweight\",1,\"Heavy.\",\"bored\",\"\"\n"
		"\"p_01\",\"patience_25\",1,\"Calm quarter.\",\"bored\",\"calm\"\n"
		"\"p_02\",\"patience_25\",2,\"Tense quarter.\",\"curt\",\"tense\"\n"
		"\"p_03\",\"patience_25\",3,\"Any quarter.\",\"curt\",\"any\"\n"
		"\"c_01\",\"cut_patience\",1,\"Cut.\",\"gleeful\",\"SCARED\"\n");

	/** A table written before the tone column existed. */
	const TCHAR* OldCsv = TEXT(
		"\"id\",\"event\",\"variant\",\"text\",\"voice_note\"\n"
		"\"a_01\",\"alpha\",1,\"One.\",\"bored\"\n"
		"\"a_02\",\"alpha\",2,\"Two.\",\"bored\"\n");

	FString Strip(const FString& Line)
	{
		return Line.Replace(TEXT("CAPTAIN: "), TEXT(""));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaptainPacingTest, "Deep.Story.CaptainPacing", DEEP_CAPTAIN_TEST_FLAGS)
bool FDeepCaptainPacingTest::RunTest(const FString& Parameters)
{
	FDeepCaptainTuning T;
	T.WaterNotableGapSeconds = 30.f;
	T.ScaredSerpentM = 25.f;
	T.TensePatienceFraction = 0.25f;
	T.ScaredMinUrgency = 3;

	// Danger.
	FDeepCaptainDangerInputs In;
	TestEqual(TEXT("Aboard is calm"), DeepCaptainPacing::Danger(In, T), EDeepCaptainDanger::Calm);
	In.bBiteAlarm = true;
	TestEqual(TEXT("Out of the water is calm even with stale flags"), DeepCaptainPacing::Danger(In, T), EDeepCaptainDanger::Calm);
	// Aboard, at the helm: the hull is the danger.
	T.TenseHull01 = 0.5f;
	T.ScaredHull01 = 0.25f;
	In = FDeepCaptainDangerInputs();
	In.Hull01 = 0.8f;
	TestEqual(TEXT("Helm, hull 0.8: calm"), DeepCaptainPacing::Danger(In, T), EDeepCaptainDanger::Calm);
	In.Hull01 = 0.4f;
	TestEqual(TEXT("Helm, hull under half: tense"), DeepCaptainPacing::Danger(In, T), EDeepCaptainDanger::Tense);
	In.Hull01 = 0.2f;
	TestEqual(TEXT("Helm, hull under a quarter: scared"), DeepCaptainPacing::Danger(In, T), EDeepCaptainDanger::Scared);
	In.bInWater = true;
	In.Hull01 = 0.2f;
	TestEqual(TEXT("In the water the hull is not the diver's danger"), DeepCaptainPacing::Danger(In, T), EDeepCaptainDanger::Calm);
	In = FDeepCaptainDangerInputs();
	In.bInWater = true;
	In.bTethered = true;
	In.PatienceFraction = 0.8f;
	TestEqual(TEXT("Water, full Patience: calm"), DeepCaptainPacing::Danger(In, T), EDeepCaptainDanger::Calm);
	In.PatienceFraction = 0.2f;
	TestEqual(TEXT("Patience under a quarter: tense"), DeepCaptainPacing::Danger(In, T), EDeepCaptainDanger::Tense);
	In.PatienceFraction = 0.8f;
	In.bTethered = false;
	In.bBailout = true;
	TestEqual(TEXT("Bailout air: tense"), DeepCaptainPacing::Danger(In, T), EDeepCaptainDanger::Tense);
	In.bSerpentClosing = true;
	In.SerpentDistanceM = 30.f;
	TestEqual(TEXT("Serpent closing at 30 m: still tense"), DeepCaptainPacing::Danger(In, T), EDeepCaptainDanger::Tense);
	In.SerpentDistanceM = 20.f;
	TestEqual(TEXT("Serpent closing at 20 m: scared"), DeepCaptainPacing::Danger(In, T), EDeepCaptainDanger::Scared);
	In.bSerpentClosing = false;
	TestEqual(TEXT("Serpent near but lurking: not scared"), DeepCaptainPacing::Danger(In, T), EDeepCaptainDanger::Tense);
	In.bBiteAlarm = true;
	TestEqual(TEXT("Bite alarm: scared"), DeepCaptainPacing::Danger(In, T), EDeepCaptainDanger::Scared);
	TestEqual(TEXT("Tone names"), DeepCaptainPacing::ToneName(EDeepCaptainDanger::Tense), FName(TEXT("tense")));

	// Allows.
	using S = EDeepCaptainScene;
	using D = EDeepCaptainDanger;
	for (const S Scene : { S::Title, S::Aboard, S::Settlement })
	{
		for (int32 U = 0; U <= 3; ++U)
		{
			TestTrue(FString::Printf(TEXT("%s shows urgency %d"), DeepCaptainPacing::SceneName(Scene), U), DeepCaptainPacing::Allows(Scene, D::Calm, U, 0.0, T));
		}
	}
	TestFalse(TEXT("Water: urgency 0 never"), DeepCaptainPacing::Allows(S::Water, D::Calm, 0, 1000.0, T));
	TestFalse(TEXT("Water: urgency 1 inside the gap"), DeepCaptainPacing::Allows(S::Water, D::Calm, 1, 29.0, T));
	TestTrue(TEXT("Water: urgency 1 after the gap"), DeepCaptainPacing::Allows(S::Water, D::Calm, 1, 30.0, T));
	TestTrue(TEXT("Water: urgency 2 always"), DeepCaptainPacing::Allows(S::Water, D::Tense, 2, 0.0, T));
	TestFalse(TEXT("Water, scared: urgency 2 is silent"), DeepCaptainPacing::Allows(S::Water, D::Scared, 2, 1000.0, T));
	TestTrue(TEXT("Water, scared: urgency 3 shows"), DeepCaptainPacing::Allows(S::Water, D::Scared, 3, 0.0, T));

	// The tone column.
	FDeepCaptainLines Lines;
	TestEqual(TEXT("Toned table loads"), Lines.LoadFromString(DeepCaptainTest::Csv), 6);
	FRandomStream Rng(7);
	FDeepCaptainLine Line;
	TSet<FString> Seen;
	for (int32 I = 0; I < 20; ++I)
	{
		Lines.Pick(TEXT("patience_25"), Rng, Line, TEXT("tense"));
		Seen.Add(Line.Text);
	}
	TestTrue(TEXT("Tense picks the tense row"), Seen.Contains(TEXT("Tense quarter.")));
	TestFalse(TEXT("Tense never picks the calm row"), Seen.Contains(TEXT("Calm quarter.")));
	Lines.Pick(TEXT("patience_25"), Rng, Line, TEXT("scared"));
	TestEqual(TEXT("No scared row: falls back to any"), Line.Text, FString(TEXT("Any quarter.")));
	Lines.Pick(TEXT("cut_patience"), Rng, Line, TEXT("calm"));
	TestEqual(TEXT("Only a scared row: still says it"), Line.Text, FString(TEXT("Cut.")));
	TestEqual(TEXT("Tone cells are case-blind"), Line.Tone, FName(TEXT("scared")));
	Lines.Pick(TEXT("overweight"), Rng, Line, TEXT("calm"));
	TestTrue(TEXT("An empty cell is any"), Line.Tone.IsNone());
	Seen.Reset();
	for (int32 I = 0; I < 30; ++I)
	{
		Lines.Pick(TEXT("patience_25"), Rng, Line);
		Seen.Add(Line.Text);
	}
	TestEqual(TEXT("No tone: every row, as before"), Seen.Num(), 3);

	FDeepCaptainLines Old;
	TestEqual(TEXT("A table without the tone column loads"), Old.LoadFromString(DeepCaptainTest::OldCsv), 2);
	TestTrue(TEXT("And picks with a tone"), Old.Pick(TEXT("alpha"), Rng, Line, TEXT("scared")) && Line.Tone.IsNone());

	FDeepCaptainLines Shipped;
	Shipped.LoadDefault();
	TestTrue(TEXT("The shipped table still loads with the tone column"), Shipped.Num() > 50);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaptainRadioTest, "Deep.Story.CaptainRadio", DEEP_CAPTAIN_TEST_FLAGS)
bool FDeepCaptainRadioTest::RunTest(const FString& Parameters)
{
	using S = EDeepCaptainScene;
	using D = EDeepCaptainDanger;
	using DeepCaptainTest::Strip;
	const float Gap = UDeepTuningSettings::Get().Captain.WaterNotableGapSeconds;
	const FDeepEventPayload Payload;

	{
		DeepCaptainTest::FRadio Radio(1);
		DeepCaptainTest::FWorld Scoped;
		UDeepCaptain* Captain = UDeepCaptain::Get(Scoped.World);
		UTEST_NOT_NULL(TEXT("Captain"), Captain);
		Captain->GetLines().LoadFromString(DeepCaptainTest::Csv);
		TestEqual(TEXT("No diver: aboard"), Captain->GatherScene(), S::Aboard);
		TestEqual(TEXT("No diver: calm"), Captain->GatherDanger(), D::Calm);

		Captain->HandleEventIn(TEXT("salvage_small"), Payload, 1.0, S::Water, D::Calm);
		TestTrue(TEXT("Water: chatter is held back"), Captain->GetLineAt(1.1).IsEmpty());
		Captain->HandleEventIn(TEXT("overweight"), Payload, 2.0, S::Water, D::Calm);
		TestEqual(TEXT("Water: the first notable line shows"), Strip(Captain->GetLineAt(2.1)), FString(TEXT("Heavy.")));
		Captain->HandleEventIn(TEXT("overweight"), Payload, 20.0, S::Water, D::Calm);
		TestTrue(TEXT("Water: a notable line inside the gap is held back"), Captain->GetLineAt(20.1).IsEmpty());
		Captain->HandleEventIn(TEXT("overweight"), Payload, 2.0 + Gap + 0.5, S::Water, D::Calm);
		TestEqual(TEXT("Water: after the gap it shows"), Strip(Captain->GetLineAt(2.0 + Gap + 0.6)), FString(TEXT("Heavy.")));
		Captain->HandleEventIn(TEXT("patience_25"), Payload, 100.0, S::Water, D::Tense);
		TestEqual(TEXT("Tense: the tense line"), Strip(Captain->GetLineAt(100.1)), FString(TEXT("Tense quarter.")));
		Captain->HandleEventIn(TEXT("patience_25"), Payload, 200.0, S::Water, D::Scared);
		TestTrue(TEXT("Scared: urgency 2 is silent"), Captain->GetLineAt(200.1).IsEmpty());
		Captain->HandleEventIn(TEXT("cut_patience"), Payload, 201.0, S::Water, D::Scared);
		TestEqual(TEXT("Scared: urgency 3 shows"), Strip(Captain->GetLineAt(201.1)), FString(TEXT("Cut.")));
		Captain->HandleEventIn(TEXT("salvage_small"), Payload, 300.0, S::Aboard, D::Calm);
		TestEqual(TEXT("Aboard: chatter shows"), Strip(Captain->GetLineAt(300.1)), FString(TEXT("Small.")));
		Captain->HandleEventIn(TEXT("salvage_small"), Payload, 400.0, S::Settlement, D::Calm);
		TestEqual(TEXT("Settlement: chatter shows"), Strip(Captain->GetLineAt(400.1)), FString(TEXT("Small.")));
		TestEqual(TEXT("Three lines held back"), Captain->GetHeldBackCount(), 3);
	}
	{
		DeepCaptainTest::FRadio Radio(0);
		DeepCaptainTest::FWorld Scoped;
		UDeepCaptain* Captain = UDeepCaptain::Get(Scoped.World);
		UTEST_NOT_NULL(TEXT("Captain"), Captain);
		Captain->GetLines().LoadFromString(DeepCaptainTest::Csv);
		Captain->HandleEventIn(TEXT("salvage_small"), Payload, 1.0, S::Water, D::Scared);
		TestEqual(TEXT("Radio off: chatter in the water shows, as before"), Strip(Captain->GetLineAt(1.1)), FString(TEXT("Small.")));
		TestEqual(TEXT("Radio off: nothing held back"), Captain->GetHeldBackCount(), 0);
		if (UDeepAudioSubsystem* Audio = UDeepAudioSubsystem::Get(Scoped.World))
		{
			TestEqual(TEXT("Radio off: no squelch"), Audio->CountOf(FName(DeepAudioRadio::On)), 0);
			TestFalse(TEXT("Radio off: not keyed"), Audio->IsRadioKeyed());
		}
	}
	{
		// Each shown line keys the radio: squelch on, (no voice file here: text only, no duck), squelch off at its end.
		DeepCaptainTest::FRadio Radio(1);
		DeepCaptainTest::FWorld Scoped;
		UDeepCaptain* Captain = UDeepCaptain::Get(Scoped.World);
		UDeepAudioSubsystem* Audio = UDeepAudioSubsystem::Get(Scoped.World);
		UTEST_TRUE(TEXT("Captain and audio"), Captain && Audio);
		Captain->GetLines().LoadFromString(DeepCaptainTest::Csv);
		TestEqual(TEXT("No voice file: no voice hold"), Audio->GetVoiceHoldSeconds(TEXT("o_01")), 0.f);
		TestNull(TEXT("No voice file found"), Audio->FindVoice(TEXT("o_01")));
		Captain->HandleEventIn(TEXT("overweight"), Payload, 1.0, S::Aboard, D::Calm);
		TestEqual(TEXT("The line shows"), Strip(Captain->GetLineAt(1.1)), FString(TEXT("Heavy.")));
		TestTrue(TEXT("Radio keyed"), Audio->IsRadioKeyed());
		TestEqual(TEXT("Squelch on once"), Audio->CountOf(FName(DeepAudioRadio::On)), 1);
		Captain->HandleEventIn(TEXT("cut_patience"), Payload, 1.5, S::Aboard, D::Calm);
		TestEqual(TEXT("A more urgent line while keyed: no second squelch"), Audio->CountOf(FName(DeepAudioRadio::On)), 1);
		for (int32 I = 0; I < 40; ++I)
		{
			Audio->StepAudio(0.5f);
		}
		TestFalse(TEXT("Radio closed after the line"), Audio->IsRadioKeyed());
		TestEqual(TEXT("Squelch off once"), Audio->CountOf(FName(DeepAudioRadio::Off)), 1);
		TestEqual(TEXT("Text only: no duck"), Audio->GetDuck01(), 0.f);
	}
	return true;
}

#endif
