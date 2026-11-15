// Tests for the sound layer. Nobody on the agent side can listen, so these check what can be measured: every
// generated buffer has the right length, stays under the peak limit and starts and ends at silence (loops: a seam
// no rougher than the rest of the buffer); Sounds.csv parses and covers every event the game fires; the loop mix
// follows the diver; and a world run (no audio device under -NoSound) logs the right sounds without crashing.

#include "CoreMinimal.h"
#include "Audio/DeepAudioRules.h"
#include "Audio/DeepAudioSettings.h"
#include "Audio/DeepAudioSubsystem.h"
#include "Audio/DeepSynth.h"
#include "Debug/DeepStateReport.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "Harpoon/DeepHarpoonRules.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Story/DeepEventBus.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_AUDIO_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepAudioTest
{
	/** Events the game fires that deliberately make no sound (the Captain speaks, or nothing happens). */
	TArray<FName> DeliberatelySilent()
	{
		return { DeepEvent::Title, DeepEvent::IdleInside, DeepHarpoonEvent::Missed };
	}

	/** Every event name the game can fire: the bus list plus the harpoon's own events. */
	TArray<FName> AllGameEvents()
	{
		TArray<FName> Out = DeepEvent::AllFired();
		for (const TCHAR* Name : { DeepHarpoonEvent::Fired, DeepHarpoonEvent::HitSub, DeepHarpoonEvent::HitTerrain,
			DeepHarpoonEvent::HitMonster, DeepHarpoonEvent::Missed, DeepHarpoonEvent::Empty })
		{
			Out.AddUnique(FName(Name));
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepAudioGeneratorsTest, "Deep.Sim.Audio.Generators", DEEP_AUDIO_TEST_FLAGS)
bool FDeepAudioGeneratorsTest::RunTest(const FString& Parameters)
{
	const int32 Limit = FMath::FloorToInt(DeepSynth::PeakLimit * 32767.f);
	const int32 NearZero = FMath::CeilToInt(0.01f * 32767.f);
	const TArray<FName> Names = DeepSynth::BuiltinNames();
	TestTrue(TEXT("At least eight built-in sounds"), Names.Num() >= 8);
	for (const FName& Name : Names)
	{
		const FString N = Name.ToString();
		const DeepSynth::FBuffer Buffer = DeepSynth::Generate(Name);
		const TArray<int16>& S = Buffer.Samples;
		TestEqual(FString::Printf(TEXT("%s: expected length"), *N), S.Num(), DeepSynth::ExpectedSamples(Name));
		TestEqual(FString::Printf(TEXT("%s: loop flag"), *N), Buffer.bLoop, DeepSynth::IsLoopBuiltin(Name));
		if (S.Num() < 2)
		{
			AddError(FString::Printf(TEXT("%s: empty"), *N));
			continue;
		}
		int32 Peak = 0;
		int32 MaxStep = 0;
		for (int32 I = 0; I < S.Num(); ++I)
		{
			Peak = FMath::Max(Peak, FMath::Abs(int32(S[I])));
			if (I > 0)
			{
				MaxStep = FMath::Max(MaxStep, FMath::Abs(int32(S[I]) - int32(S[I - 1])));
			}
		}
		TestTrue(FString::Printf(TEXT("%s: peak %d under %d"), *N, Peak, Limit), Peak <= Limit);
		TestTrue(FString::Printf(TEXT("%s: not silent (peak %d)"), *N, Peak), Peak > 32767 / 10);
		if (Buffer.bLoop)
		{
			const int32 Seam = FMath::Abs(int32(S[0]) - int32(S.Last()));
			TestTrue(FString::Printf(TEXT("%s: loop seam %d no rougher than the buffer (%d)"), *N, Seam, MaxStep), Seam <= MaxStep);
		}
		else
		{
			TestTrue(FString::Printf(TEXT("%s: starts near zero"), *N), FMath::Abs(int32(S[0])) <= NearZero);
			TestTrue(FString::Printf(TEXT("%s: ends near zero"), *N), FMath::Abs(int32(S.Last())) <= NearZero);
		}
	}
	TestEqual(TEXT("Unknown name: empty"), DeepSynth::Generate(FName(TEXT("no_such_sound"))).Samples.Num(), 0);

	// The safety pass: NaN and infinity become silence, the peak is scaled down, the ends fade.
	TArray<float> Raw;
	Raw.Init(2.f, 4800);
	Raw[10] = NAN;
	Raw[20] = INFINITY;
	const TArray<int16> Safe = DeepSynth::Finish(Raw, 0.45f, true);
	TestEqual(TEXT("NaN to silence"), int32(Safe[10]), 0);
	TestEqual(TEXT("Infinity to silence"), int32(Safe[20]), 0);
	TestEqual(TEXT("Scaled to the peak"), int32(Safe[2400]), FMath::RoundToInt(0.45f * 32767.f));
	TestEqual(TEXT("Fades in from zero"), int32(Safe[0]), 0);
	TestEqual(TEXT("Fades out to zero"), int32(Safe.Last()), 0);
	TArray<float> Loud;
	Loud.Init(5.f, 100);
	const TArray<int16> Capped = DeepSynth::Finish(Loud, 2.f, false);
	TestTrue(TEXT("Never over the limit even if asked"), Capped[50] <= Limit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepAudioTableTest, "Deep.Sim.Audio.Table", DEEP_AUDIO_TEST_FLAGS)
bool FDeepAudioTableTest::RunTest(const FString& Parameters)
{
	FDeepSoundTable Inline;
	const int32 Count = Inline.LoadFromString(TEXT(
		"\"event\",\"group\",\"asset\",\"builtin\",\"volume\",\"pitch\",\"loop\",\"note\"\n"
		"\"a\",\"ui\",\"\",\"click\",0.5,1.2,0,\"note, with a comma\"\n"
		"\"b\",\"sfx\",\"/Game/Audio/S_B\",\"\",3,0.1,1,\"\"\n"
		",,,,,,,\n"));
	TestEqual(TEXT("Two rows, the blank one skipped"), Count, 2);
	const FDeepSoundRow* A = Inline.Find(FName(TEXT("a")));
	const FDeepSoundRow* B = Inline.Find(FName(TEXT("b")));
	UTEST_TRUE(TEXT("Both rows found"), A && B);
	TestEqual(TEXT("Builtin"), A->Builtin, FName(TEXT("click")));
	TestEqual(TEXT("Volume"), A->Volume, 0.5f, 1e-4f);
	TestEqual(TEXT("Pitch"), A->Pitch, 1.2f, 1e-4f);
	TestFalse(TEXT("Not a loop"), A->bLoop);
	TestEqual(TEXT("Asset path"), B->Asset, FString(TEXT("/Game/Audio/S_B")));
	TestEqual(TEXT("Volume clamped to 1"), B->Volume, 1.f, 1e-4f);
	TestEqual(TEXT("Pitch clamped to 0.5"), B->Pitch, 0.5f, 1e-4f);
	TestTrue(TEXT("Loop"), B->bLoop);

	FDeepSoundTable Shipped;
	UTEST_TRUE(TEXT("Sounds.csv loads"), Shipped.LoadDefault() > 0);
	for (const FDeepSoundRow& Row : Shipped.GetRows())
	{
		const FString N = Row.Event.ToString();
		TestTrue(FString::Printf(TEXT("%s: has a built-in or an asset"), *N), DeepSynth::IsBuiltin(Row.Builtin) || !Row.Asset.IsEmpty());
		TestTrue(FString::Printf(TEXT("%s: asset under /Game/Audio/ or empty"), *N), Row.Asset.IsEmpty() || Row.Asset.StartsWith(TEXT("/Game/Audio/")));
		TestTrue(FString::Printf(TEXT("%s: volume 0..1"), *N), Row.Volume >= 0.f && Row.Volume <= 1.f);
		TestTrue(FString::Printf(TEXT("%s: known group"), *N), UDeepAudioSettings::Get().GroupVolume(Row.Group) >= 0.f
			&& (Row.Group == TEXT("sfx") || Row.Group == TEXT("ui") || Row.Group == TEXT("ambience") || Row.Group == TEXT("creature")));
		TestEqual(FString::Printf(TEXT("%s: a loop row uses a loop built-in"), *N), Row.bLoop, DeepSynth::IsLoopBuiltin(Row.Builtin));
	}
	for (const FName& Loop : DeepAudioLoop::All())
	{
		const FDeepSoundRow* Row = Shipped.Find(Loop);
		TestTrue(FString::Printf(TEXT("%s: has a loop row"), *Loop.ToString()), Row && Row->bLoop);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepAudioCoverageTest, "Deep.Sim.Audio.Coverage", DEEP_AUDIO_TEST_FLAGS)
bool FDeepAudioCoverageTest::RunTest(const FString& Parameters)
{
	FDeepSoundTable Shipped;
	UTEST_TRUE(TEXT("Sounds.csv loads"), Shipped.LoadDefault() > 0);
	const TArray<FName> Silent = DeepAudioTest::DeliberatelySilent();
	const TArray<FName> Events = DeepAudioTest::AllGameEvents();
	for (const FName& Event : Events)
	{
		const FDeepSoundRow* Row = Shipped.Find(Event);
		TestTrue(FString::Printf(TEXT("%s: has a one-shot row or is deliberately silent"), *Event.ToString()),
			(Row && !Row->bLoop) || Silent.Contains(Event));
	}
	// Every row belongs to something that exists: a fired event or a loop state (catches typos in the CSV).
	const TArray<FName> Loops = DeepAudioLoop::All();
	for (const FDeepSoundRow& Row : Shipped.GetRows())
	{
		TestTrue(FString::Printf(TEXT("%s: is a game event or a loop state"), *Row.Event.ToString()), Events.Contains(Row.Event) || Loops.Contains(Row.Event));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepAudioMixTest, "Deep.Sim.Audio.Mix", DEEP_AUDIO_TEST_FLAGS)
bool FDeepAudioMixTest::RunTest(const FString& Parameters)
{
	UDeepAudioSettings* Settings = NewObject<UDeepAudioSettings>();
	Settings->DeepestM = 700.f;
	Settings->FastBreathAirS = 12.f;
	Settings->SerpentThreatFloor = 0.05f;
	auto Vol = [](const TMap<FName, FDeepLoopTarget>& T, const TCHAR* Loop) { return T.FindChecked(FName(Loop)).Volume01; };

	FDeepAudioMixInputs None;
	for (const TPair<FName, FDeepLoopTarget>& Pair : DeepAudioRules::LoopTargets(None, *Settings))
	{
		TestEqual(FString::Printf(TEXT("No diver: %s silent"), *Pair.Key.ToString()), Pair.Value.Volume01, 0.f);
	}

	FDeepAudioMixInputs Aboard;
	Aboard.bHasDiver = true;
	Aboard.bInside = true;
	Aboard.DepthM = 100.f;
	const TMap<FName, FDeepLoopTarget> A = DeepAudioRules::LoopTargets(Aboard, *Settings);
	TestEqual(TEXT("Aboard: inside drone"), Vol(A, DeepAudioLoop::DroneIn), 1.f);
	TestEqual(TEXT("Aboard: no outside drone"), Vol(A, DeepAudioLoop::DroneOut), 0.f);
	TestEqual(TEXT("Aboard: no breathing"), Vol(A, DeepAudioLoop::Breath), 0.f);

	FDeepAudioMixInputs Shallow;
	Shallow.bHasDiver = true;
	Shallow.bInside = false;
	Shallow.bSwimming = true;
	Shallow.DepthM = 100.f;
	Shallow.BailoutAirS = 60.f;
	FDeepAudioMixInputs Deep = Shallow;
	Deep.DepthM = 600.f;
	const TMap<FName, FDeepLoopTarget> S = DeepAudioRules::LoopTargets(Shallow, *Settings);
	const TMap<FName, FDeepLoopTarget> D = DeepAudioRules::LoopTargets(Deep, *Settings);
	TestEqual(TEXT("Water: outside drone"), Vol(S, DeepAudioLoop::DroneOut), 1.f);
	TestEqual(TEXT("Water: no inside drone"), Vol(S, DeepAudioLoop::DroneIn), 0.f);
	TestEqual(TEXT("Water: calm breathing"), Vol(S, DeepAudioLoop::Breath), 1.f);
	TestEqual(TEXT("Water: no fast breathing"), Vol(S, DeepAudioLoop::BreathFast), 0.f);
	TestTrue(TEXT("Deeper is lower"), D[FName(DeepAudioLoop::DroneOut)].Pitch < S[FName(DeepAudioLoop::DroneOut)].Pitch);
	TestTrue(TEXT("Deeper is darker"), D[FName(DeepAudioLoop::DroneOut)].LowPassHz < S[FName(DeepAudioLoop::DroneOut)].LowPassHz);
	TestTrue(TEXT("Pitch stays in range"), D[FName(DeepAudioLoop::DroneOut)].Pitch >= 0.5f);

	FDeepAudioMixInputs Cut = Shallow;
	Cut.bCut = true;
	Cut.BailoutAirS = 30.f;
	TestEqual(TEXT("Cut with air: calm breathing"), Vol(DeepAudioRules::LoopTargets(Cut, *Settings), DeepAudioLoop::Breath), 1.f);
	Cut.BailoutAirS = 8.f;
	const TMap<FName, FDeepLoopTarget> C = DeepAudioRules::LoopTargets(Cut, *Settings);
	TestEqual(TEXT("Low air: fast breathing"), Vol(C, DeepAudioLoop::BreathFast), 1.f);
	TestEqual(TEXT("Low air: not calm"), Vol(C, DeepAudioLoop::Breath), 0.f);

	FDeepAudioMixInputs Busy = Shallow;
	Busy.bWinching = true;
	Busy.Threat01 = 0.8f;
	Busy.bHelmMoving = true;
	const TMap<FName, FDeepLoopTarget> B = DeepAudioRules::LoopTargets(Busy, *Settings);
	TestEqual(TEXT("Winch loop"), Vol(B, DeepAudioLoop::Winch), 1.f);
	TestTrue(TEXT("Serpent loud at threat 0.8"), Vol(B, DeepAudioLoop::Serpent) > 0.5f);
	TestEqual(TEXT("Serpent silent at threat 0"), Vol(S, DeepAudioLoop::Serpent), 0.f);
	TestEqual(TEXT("Helm hum"), Vol(B, DeepAudioLoop::Helm), 1.f);

	// Fades: never a jump.
	TestEqual(TEXT("Fade out step"), DeepAudioRules::FadeToward(1.f, 0.f, 0.1f, 0.8f, 1.2f), 1.f - 0.1f / 1.2f, 1e-4f);
	TestEqual(TEXT("Fade in step"), DeepAudioRules::FadeToward(0.f, 1.f, 0.1f, 0.8f, 1.2f), 0.1f / 0.8f, 1e-4f);
	TestEqual(TEXT("Fade stops at the target"), DeepAudioRules::FadeToward(0.95f, 1.f, 1.f, 0.8f, 1.2f), 1.f, 1e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepAudioWorldTest, "Deep.World.Audio", DEEP_AUDIO_TEST_FLAGS)
bool FDeepAudioWorldTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	ADeepGameMode* GameMode = World->SpawnActor<ADeepGameMode>();
	ADeepSubmarine* Sub = GameMode ? GameMode->EnsureArena() : nullptr;
	if (GameMode)
	{
		GameMode->StartPlay();
	}
	ADeepDiver* Diver = Sub ? World->SpawnActor<ADeepDiver>(Sub->GetDiverStartTransform().GetLocation(), FRotator::ZeroRotator) : nullptr;
	UDeepRunComponent* Run = Diver ? Diver->GetRun() : nullptr;
	UDeepEventBus* Bus = UDeepEventBus::Get(World);
	UDeepAudioSubsystem* Audio = UDeepAudioSubsystem::Get(World);
	auto Cmd = [World](const TCHAR* Command) { GEngine->Exec(World, Command); };
	auto Step = [Audio](float Seconds) { for (float T = 0.f; T < Seconds - 1e-4f; T += 0.1f) { Audio->StepAudio(0.1f); } };
	auto Playing = [Audio](const TCHAR* Loop) { return Audio->GetPlayingLoops().Contains(FName(Loop)); };

	if (GameMode && Sub && Diver && Run && Bus && Audio)
	{
		Cmd(TEXT("Deep.Title"));
		Cmd(TEXT("Deep.Start"));
		TestEqual(TEXT("Aboard"), Run->GetState(), EDeepRunState::Aboard);
		Step(2.f);
		TestTrue(TEXT("Aboard: inside drone"), Playing(DeepAudioLoop::DroneIn));
		TestFalse(TEXT("Aboard: no outside drone"), Playing(DeepAudioLoop::DroneOut));
		TestFalse(TEXT("Aboard: no breathing"), Playing(DeepAudioLoop::Breath));

		Audio->ClearHistory();
		Cmd(TEXT("Deep.Hatch"));
		TestTrue(TEXT("Diving"), Run->CanSwim());
		TestEqual(TEXT("dive_start made a sound"), Audio->CountOf(FName(DeepEvent::DiveStart)), 1);
		TestTrue(TEXT("It resolved to a generated sound"), !Audio->GetHistory().IsEmpty() && Audio->GetHistory().Last().Source == EDeepSoundSource::Generated);

		// The loops cross over with fades, never at once.
		Step(0.1f);
		const float Rising = Audio->GetLoopVolume(FName(DeepAudioLoop::DroneOut));
		const float Falling = Audio->GetLoopVolume(FName(DeepAudioLoop::DroneIn));
		TestTrue(FString::Printf(TEXT("Outside drone fades in (%.2f)"), Rising), Rising > 0.f && Rising < 0.5f);
		TestTrue(FString::Printf(TEXT("Inside drone fades out (%.2f)"), Falling), Falling > 0.5f && Falling < 1.f);
		Step(2.f);
		TestTrue(TEXT("Water: outside drone"), Playing(DeepAudioLoop::DroneOut));
		TestTrue(TEXT("Water: breathing"), Playing(DeepAudioLoop::Breath));
		TestFalse(TEXT("Water: inside drone gone"), Playing(DeepAudioLoop::DroneIn));

		Cmd(TEXT("Deep.Winch 1"));
		Step(1.f);
		TestTrue(TEXT("Winch loop while winching"), Playing(DeepAudioLoop::Winch));
		Cmd(TEXT("Deep.Winch 0"));
		Diver->SetWinching(false);
		Step(2.f);
		TestFalse(TEXT("Winch loop fades away"), Playing(DeepAudioLoop::Winch));

		// The event bus drives one-shots; Deep.Audio.Play forces one.
		Bus->Broadcast(FName(DeepEvent::BankGood), FDeepEventPayload());
		TestEqual(TEXT("bank_good chimed"), Audio->CountOf(FName(DeepEvent::BankGood)), 1);
		Bus->Broadcast(FName(DeepEvent::Title), FDeepEventPayload());
		TestEqual(TEXT("title is deliberately silent"), Audio->CountOf(FName(DeepEvent::Title)), 0);
		Cmd(TEXT("Deep.Audio.Play harpoon_fired"));
		TestEqual(TEXT("Deep.Audio.Play"), Audio->CountOf(FName(DeepHarpoonEvent::Fired)), 1);
		Cmd(TEXT("Deep.Audio.List"));

		// DEEPSTATE reports the loops.
		TSharedPtr<FJsonObject> Json;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(DeepStateReport::BuildJson(World));
		const bool bParsed = FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid();
		TestTrue(TEXT("DEEPSTATE parses"), bParsed);
		const TSharedPtr<FJsonObject>* AudioJson = nullptr;
		if (bParsed && Json->TryGetObjectField(TEXT("audio"), AudioJson))
		{
			const TArray<TSharedPtr<FJsonValue>>* Loops = nullptr;
			bool bBreath = false;
			if ((*AudioJson)->TryGetArrayField(TEXT("loops"), Loops))
			{
				for (const TSharedPtr<FJsonValue>& V : *Loops)
				{
					bBreath |= V->AsString() == DeepAudioLoop::Breath;
				}
			}
			TestTrue(TEXT("DEEPSTATE audio.loops has the breathing"), bBreath);
		}
		else
		{
			AddError(TEXT("DEEPSTATE has no audio object"));
		}

		Cmd(TEXT("Deep.Audio.Mute 1"));
		TestTrue(TEXT("Muted"), Audio->IsMuted());
		Cmd(TEXT("Deep.Audio.Mute 0"));
		TestFalse(TEXT("Unmuted"), Audio->IsMuted());
	}
	else
	{
		AddError(TEXT("World, game mode, sub, diver, run, bus and audio subsystem"));
	}

	if (World->HasBegunPlay())
	{
		World->EndPlay(EEndPlayReason::Quit);
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
