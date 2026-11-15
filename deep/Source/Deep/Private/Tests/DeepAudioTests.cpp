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
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sonar/DeepSonarRules.h"
#include "Sound/SoundBase.h"
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
			DeepHarpoonEvent::HitMonster, DeepHarpoonEvent::Missed, DeepHarpoonEvent::Empty, DeepSonarEvent::Ping, DeepSonarEvent::Near })
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
		TestTrue(FString::Printf(TEXT("%s: asset under /Game/ or empty"), *N), Row.Asset.IsEmpty() || Row.Asset.StartsWith(TEXT("/Game/")));
		TestTrue(FString::Printf(TEXT("%s: volume 0..1"), *N), Row.Volume >= 0.f && Row.Volume <= 1.f);
		TestTrue(FString::Printf(TEXT("%s: known group"), *N), UDeepAudioSettings::Get().GroupVolume(Row.Group) >= 0.f
			&& (Row.Group == TEXT("sfx") || Row.Group == TEXT("ui") || Row.Group == TEXT("ambience") || Row.Group == TEXT("creature")
				|| Row.Group == DeepAudioRadio::Group || Row.Group == DeepAudioMusic::Group));
		if (Row.Group == DeepAudioMusic::Group)
		{
			TestFalse(FString::Printf(TEXT("%s: music has no generated fallback"), *N), DeepSynth::IsBuiltin(Row.Builtin));
			TestEqual(FString::Printf(TEXT("%s: beds loop, cues do not"), *N), Row.bLoop, DeepAudioMusic::Beds().Contains(Row.Event));
			continue;
		}
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
	// Every row belongs to something that exists: a fired event, a loop state, the radio or music (catches typos in the CSV).
	const TArray<FName> Loops = DeepAudioLoop::All();
	const TArray<FName> Radio = DeepAudioRadio::Rows();
	const TArray<FName> Music = DeepAudioMusic::All();
	for (const FDeepSoundRow& Row : Shipped.GetRows())
	{
		TestTrue(FString::Printf(TEXT("%s: is a game event, a loop state, the radio or music"), *Row.Event.ToString()),
			Events.Contains(Row.Event) || Loops.Contains(Row.Event) || Radio.Contains(Row.Event) || Music.Contains(Row.Event));
	}
	for (const FName& Name : Music)
	{
		const FDeepSoundRow* Found = Shipped.Find(Name);
		TestTrue(FString::Printf(TEXT("%s: a music row under /Game/DeepAudio/Music/"), *Name.ToString()),
			Found && Found->Group == DeepAudioMusic::Group && Found->Asset.StartsWith(TEXT("/Game/DeepAudio/Music/")));
	}
	for (const FName& Row : Radio)
	{
		const FDeepSoundRow* Found = Shipped.Find(Row);
		TestTrue(FString::Printf(TEXT("%s: a one-shot row with a built-in fallback"), *Row.ToString()), Found && !Found->bLoop && DeepSynth::IsBuiltin(Found->Builtin));
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

	// Breathing by danger (Deep.Music). Off: the danger tier is ignored, as before D-040.
	FDeepAudioMixInputs Scared = Shallow;
	Scared.Danger = 2;
	TestEqual(TEXT("Music off: scared still breathes calm"), Vol(DeepAudioRules::LoopTargets(Scared, *Settings), DeepAudioLoop::Breath), 1.f);
	TestEqual(TEXT("Music off: no heartbeat"), Vol(DeepAudioRules::LoopTargets(Scared, *Settings), DeepAudioLoop::Heartbeat), 0.f);
	Scared.bMusic = true;
	const TMap<FName, FDeepLoopTarget> Sc = DeepAudioRules::LoopTargets(Scared, *Settings);
	TestEqual(TEXT("Scared: panic breathing"), Vol(Sc, DeepAudioLoop::BreathPanic), 1.f);
	TestEqual(TEXT("Scared: heartbeat"), Vol(Sc, DeepAudioLoop::Heartbeat), 1.f);
	TestEqual(TEXT("Scared: not calm"), Vol(Sc, DeepAudioLoop::Breath), 0.f);
	TestEqual(TEXT("Scared: not fast"), Vol(Sc, DeepAudioLoop::BreathFast), 0.f);
	FDeepAudioMixInputs Tense = Scared;
	Tense.Danger = 1;
	const TMap<FName, FDeepLoopTarget> Te = DeepAudioRules::LoopTargets(Tense, *Settings);
	TestEqual(TEXT("Tense: fast breathing"), Vol(Te, DeepAudioLoop::BreathFast), 1.f);
	TestEqual(TEXT("Tense: no heartbeat"), Vol(Te, DeepAudioLoop::Heartbeat), 0.f);
	FDeepAudioMixInputs CalmLowAir = Cut;
	CalmLowAir.bMusic = true;
	TestEqual(TEXT("Calm but low air: still fast"), Vol(DeepAudioRules::LoopTargets(CalmLowAir, *Settings), DeepAudioLoop::BreathFast), 1.f);
	FDeepAudioMixInputs ScaredAboard = Aboard;
	ScaredAboard.bMusic = true;
	ScaredAboard.Danger = 2;
	TestEqual(TEXT("Aboard: no heartbeat"), Vol(DeepAudioRules::LoopTargets(ScaredAboard, *Settings), DeepAudioLoop::Heartbeat), 0.f);

	// Fades: never a jump.
	TestEqual(TEXT("Fade out step"), DeepAudioRules::FadeToward(1.f, 0.f, 0.1f, 0.8f, 1.2f), 1.f - 0.1f / 1.2f, 1e-4f);
	TestEqual(TEXT("Fade in step"), DeepAudioRules::FadeToward(0.f, 1.f, 0.1f, 0.8f, 1.2f), 0.1f / 0.8f, 1e-4f);
	TestEqual(TEXT("Fade stops at the target"), DeepAudioRules::FadeToward(0.95f, 1.f, 1.f, 0.8f, 1.2f), 1.f, 1e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepAudioMusicTest, "Deep.Sim.Audio.Music", DEEP_AUDIO_TEST_FLAGS)
bool FDeepAudioMusicTest::RunTest(const FString& Parameters)
{
	UDeepAudioSettings* Settings = NewObject<UDeepAudioSettings>();
	Settings->MusicZone2M = 200.f;
	Settings->MusicZone3M = 400.f;
	Settings->MusicCrossfadeSeconds = 3.f;
	Settings->StingSerpentM = 30.f;
	auto Playing = [](const TMap<FName, FDeepLoopTarget>& T)
	{
		TArray<FString> Out;
		for (const TPair<FName, FDeepLoopTarget>& Pair : T)
		{
			if (Pair.Value.Volume01 > 0.f)
			{
				Out.Add(Pair.Key.ToString());
			}
		}
		return FString::Join(Out, TEXT(","));
	};
	auto Only = [](const TCHAR* Bed) { return FString(Bed); };

	FDeepAudioMixInputs Water;
	Water.bHasDiver = true;
	Water.bInside = false;
	Water.bSwimming = true;
	Water.DepthM = 100.f;
	TestEqual(TEXT("Music off: silent"), Playing(DeepAudioRules::MusicTargets(Water, *Settings)), FString());
	TestEqual(TEXT("Every bed has an entry"), DeepAudioRules::MusicTargets(Water, *Settings).Num(), DeepAudioMusic::Beds().Num());

	Water.bMusic = true;
	TestEqual(TEXT("100 m: the first bed"), Playing(DeepAudioRules::MusicTargets(Water, *Settings)), Only(DeepAudioMusic::Depth1));
	Water.DepthM = 300.f;
	TestEqual(TEXT("300 m: the second bed"), Playing(DeepAudioRules::MusicTargets(Water, *Settings)), Only(DeepAudioMusic::Depth2));
	Water.DepthM = 500.f;
	const TMap<FName, FDeepLoopTarget> DeepBeds = DeepAudioRules::MusicTargets(Water, *Settings);
	TestEqual(TEXT("500 m: the third bed"), Playing(DeepBeds), Only(DeepAudioMusic::Depth3));
	TestEqual(TEXT("Beds crossfade over 3 s (in)"), DeepBeds[FName(DeepAudioMusic::Depth3)].FadeInS, 3.f);
	TestEqual(TEXT("Beds crossfade over 3 s (out)"), DeepBeds[FName(DeepAudioMusic::Depth1)].FadeOutS, 3.f);
	TestEqual(TEXT("Zone edges"), DeepAudioRules::MusicZone(199.f, *Settings) * 10 + DeepAudioRules::MusicZone(200.f, *Settings), 12);

	FDeepAudioMixInputs Cut = Water;
	Cut.bCut = true;
	const FString CutBeds = Playing(DeepAudioRules::MusicTargets(Cut, *Settings));
	TestTrue(TEXT("Bailout: the tension layer"), CutBeds.Contains(DeepAudioMusic::Tension));
	TestTrue(TEXT("Bailout: over the depth bed"), CutBeds.Contains(DeepAudioMusic::Depth3));

	FDeepAudioMixInputs Title = Water;
	Title.bOnTitle = true;
	Title.bHasDiver = false;
	TestEqual(TEXT("Title: the title bed, no diver needed"), Playing(DeepAudioRules::MusicTargets(Title, *Settings)), Only(DeepAudioMusic::Title));
	FDeepAudioMixInputs Settle = Water;
	Settle.bInside = true;
	Settle.bSettling = true;
	TestEqual(TEXT("Settlement: its own bed"), Playing(DeepAudioRules::MusicTargets(Settle, *Settings)), Only(DeepAudioMusic::Settlement));
	FDeepAudioMixInputs Won = Settle;
	Won.bWon = true;
	TestEqual(TEXT("Won: the beds give way to the win cue"), Playing(DeepAudioRules::MusicTargets(Won, *Settings)), FString());

	TestTrue(TEXT("Sting: the first sight"), DeepAudioRules::ShouldSting(false, true, -1.f, *Settings));
	TestTrue(TEXT("Sting: within 30 m"), DeepAudioRules::ShouldSting(false, false, 25.f, *Settings));
	TestFalse(TEXT("No sting: far and unseen"), DeepAudioRules::ShouldSting(false, false, 80.f, *Settings));
	TestFalse(TEXT("No sting: no serpent"), DeepAudioRules::ShouldSting(false, false, -1.f, *Settings));
	TestFalse(TEXT("Sting once"), DeepAudioRules::ShouldSting(true, true, 5.f, *Settings));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepAudioFilesTest, "Deep.Sim.Audio.Files", DEEP_AUDIO_TEST_FLAGS)
bool FDeepAudioFilesTest::RunTest(const FString& Parameters)
{
	UDeepAudioSettings* Settings = NewObject<UDeepAudioSettings>();
	Settings->PitchJitter = 0.03f;

	TestEqual(TEXT("Object path from a package"), DeepAudioRules::AssetObjectPath(TEXT("/Game/DeepAudio/Serpent/roar")), FString(TEXT("/Game/DeepAudio/Serpent/roar.roar")));
	TestEqual(TEXT("Object path kept"), DeepAudioRules::AssetObjectPath(TEXT("/Game/A/B.B")), FString(TEXT("/Game/A/B.B")));
	TestEqual(TEXT("Package from an object path"), DeepAudioRules::AssetPackage(TEXT("/Game/A/B.B")), FString(TEXT("/Game/A/B")));
	TestEqual(TEXT("Empty stays empty"), DeepAudioRules::AssetObjectPath(TEXT("")), FString());
	const TArray<FString> Variants = DeepAudioRules::VariantPackages(TEXT("/Game/DeepAudio/Serpent/roar"), 3);
	TestEqual(TEXT("Three variations"), Variants.Num(), 3);
	TestEqual(TEXT("Named _01"), Variants.Num() > 0 ? Variants[0] : FString(), FString(TEXT("/Game/DeepAudio/Serpent/roar_01")));
	TestEqual(TEXT("Named _03"), Variants.Num() > 2 ? Variants[2] : FString(), FString(TEXT("/Game/DeepAudio/Serpent/roar_03")));

	// Picks cover every variation and never repeat the last one.
	TSet<int32> Seen;
	int32 Last = INDEX_NONE;
	bool bRepeated = false;
	for (int32 I = 0; I < 60; ++I)
	{
		const int32 Pick = DeepAudioRules::PickVariant(3, Last, float((I * 37) % 100) / 100.f);
		bRepeated |= Pick == Last;
		TestTrue(TEXT("Pick in range"), Pick >= 0 && Pick < 3);
		Seen.Add(Pick);
		Last = Pick;
	}
	TestFalse(TEXT("Never the same variation twice"), bRepeated);
	TestEqual(TEXT("Every variation plays"), Seen.Num(), 3);
	TestEqual(TEXT("One file: always it"), DeepAudioRules::PickVariant(1, 0, 0.9f), 0);

	const FName Sfx(TEXT("sfx"));
	TestEqual(TEXT("Jitter low end"), DeepAudioRules::JitterPitch(1.f, Sfx, 0.f, *Settings), 0.97f, 1.e-4f);
	TestEqual(TEXT("Jitter high end"), DeepAudioRules::JitterPitch(1.f, Sfx, 1.f, *Settings), 1.03f, 1.e-4f);
	TestEqual(TEXT("Jitter middle"), DeepAudioRules::JitterPitch(1.2f, Sfx, 0.5f, *Settings), 1.2f, 1.e-4f);
	TestEqual(TEXT("Music is never jittered"), DeepAudioRules::JitterPitch(1.f, FName(DeepAudioMusic::Group), 0.f, *Settings), 1.f);
	TestEqual(TEXT("The Captain is never jittered"), DeepAudioRules::JitterPitch(1.f, FName(DeepAudioRadio::Group), 0.f, *Settings), 1.f);

	// The cook adds pack files by reference (with the variations that exist), never /Game/DeepAudio/ (cooked whole).
	FDeepSoundTable Table;
	Table.LoadFromString(TEXT(
		"\"event\",\"group\",\"asset\",\"builtin\",\"volume\",\"pitch\",\"loop\",\"note\"\n"
		"\"a\",\"creature\",\"/Game/Pack/Roar\",\"\",1,1,0,\"\"\n"
		"\"b\",\"music\",\"/Game/DeepAudio/Music/music_depth1\",\"\",1,1,1,\"\"\n"
		"\"c\",\"sfx\",\"/Game/Pack/Gone\",\"\",1,1,0,\"\"\n"
		"\"d\",\"sfx\",\"\",\"click\",1,1,0,\"\"\n"));
	const TSet<FString> OnDisk = { TEXT("/Game/Pack/Roar"), TEXT("/Game/Pack/Roar_02"), TEXT("/Game/DeepAudio/Music/music_depth1") };
	const TArray<FName> Cook = DeepAudioRules::PackagesToCook(Table, 3, [&OnDisk](const FString& P) { return OnDisk.Contains(P); });
	TestEqual(TEXT("Cook: the pack file and its one variation"), Cook.Num(), 2);
	TestTrue(TEXT("Cook: base"), Cook.Contains(FName(TEXT("/Game/Pack/Roar"))));
	TestTrue(TEXT("Cook: _02"), Cook.Contains(FName(TEXT("/Game/Pack/Roar_02"))));
	return true;
}

// Every file Sounds.csv names loads as a sound (the base or a variation), checked only where its folder exists: music,
// serpent and pack folders arrive late and are outside git, so a fresh checkout skips them.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepAudioAssetsResolveTest, "Deep.Sim.Audio.AssetsResolve", DEEP_AUDIO_TEST_FLAGS)
bool FDeepAudioAssetsResolveTest::RunTest(const FString& Parameters)
{
	FDeepSoundTable Shipped;
	UTEST_TRUE(TEXT("Sounds.csv loads"), Shipped.LoadDefault() > 0);
	const int32 MaxVariants = UDeepAudioSettings::Get().MaxVariants;
	int32 Checked = 0;
	int32 Skipped = 0;
	for (const FDeepSoundRow& Row : Shipped.GetRows())
	{
		const FString Package = DeepAudioRules::AssetPackage(Row.Asset);
		if (Package.IsEmpty())
		{
			continue;
		}
		FString Folder;
		const bool bFolder = FPackageName::TryConvertLongPackageNameToFilename(FPackageName::GetLongPackagePath(Package), Folder)
			&& IFileManager::Get().DirectoryExists(*Folder);
		if (!bFolder)
		{
			++Skipped;
			continue;
		}
		TArray<FString> Candidates = { Package };
		Candidates.Append(DeepAudioRules::VariantPackages(Package, MaxVariants));
		bool bLoaded = false;
		for (const FString& Candidate : Candidates)
		{
			if (FPackageName::DoesPackageExist(Candidate))
			{
				const FString ObjectPath = DeepAudioRules::AssetObjectPath(Candidate);
				const bool bSound = LoadObject<USoundBase>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet) != nullptr;
				TestTrue(FString::Printf(TEXT("%s: %s is a sound"), *Row.Event.ToString(), *ObjectPath), bSound);
				bLoaded |= bSound;
			}
		}
		TestTrue(FString::Printf(TEXT("%s: %s (or a _01.. variation) is in its folder"), *Row.Event.ToString(), *Package), bLoaded);
		++Checked;
	}
	AddInfo(FString::Printf(TEXT("%d asset rows checked, %d skipped (folder not delivered)"), Checked, Skipped));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepAudioDuckTest, "Deep.Sim.Audio.Duck", DEEP_AUDIO_TEST_FLAGS)
bool FDeepAudioDuckTest::RunTest(const FString& Parameters)
{
	UDeepAudioSettings* Settings = NewObject<UDeepAudioSettings>();
	Settings->DuckMusicDb = -9.f;
	Settings->DuckAmbienceDb = -6.f;
	Settings->DuckEffectsDb = -3.f;
	auto Gain = [Settings](const TCHAR* Group, float Duck01) { return DeepAudioRules::DuckGain(FName(Group), Duck01, *Settings); };
	TestEqual(TEXT("No duck: unity"), Gain(TEXT("music"), 0.f), 1.f, 1e-4f);
	TestEqual(TEXT("Music -9 dB"), Gain(TEXT("music"), 1.f), FMath::Pow(10.f, -9.f / 20.f), 1e-4f);
	TestEqual(TEXT("Ambience -6 dB"), Gain(TEXT("ambience"), 1.f), FMath::Pow(10.f, -6.f / 20.f), 1e-4f);
	TestEqual(TEXT("Effects (sfx) -3 dB"), Gain(TEXT("sfx"), 1.f), FMath::Pow(10.f, -3.f / 20.f), 1e-4f);
	TestEqual(TEXT("Effects (creature) -3 dB"), Gain(TEXT("creature"), 1.f), FMath::Pow(10.f, -3.f / 20.f), 1e-4f);
	TestEqual(TEXT("The voice is never ducked"), Gain(DeepAudioRadio::Group, 1.f), 1.f, 1e-4f);
	TestTrue(TEXT("Half way is between"), Gain(TEXT("music"), 0.5f) < 1.f && Gain(TEXT("music"), 0.5f) > Gain(TEXT("music"), 1.f));
	Settings->DuckMusicDb = 3.f;
	TestEqual(TEXT("A positive setting never boosts"), Gain(TEXT("music"), 1.f), 1.f, 1e-4f);
	// Attack 50 ms, release 500 ms through FadeToward.
	TestEqual(TEXT("Attack: full in 50 ms"), DeepAudioRules::FadeToward(0.f, 1.f, 0.05f, 0.05f, 0.5f), 1.f, 1e-4f);
	TestEqual(TEXT("Release: half in 250 ms"), DeepAudioRules::FadeToward(1.f, 0.f, 0.25f, 0.05f, 0.5f), 0.5f, 1e-4f);
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
