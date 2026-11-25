// Tests for immersion I1 (D-072): the hull's creaks (more often deeper, never closer than the minimum gap), the groan
// once per 100 m band, the flow by speed, silt that lifts only near the floor or on a scrape and never inside the hull's
// box or under the rock, a bounded count, the echo's delay growing with the traced distance, the sounds' rows, and the
// shaft game unchanged with all of it running.

#include "CoreMinimal.h"
#include "Audio/DeepAudioRules.h"
#include "Audio/DeepAudioSettings.h"
#include "Audio/DeepAudioSubsystem.h"
#include "Audio/DeepSynth.h"
#include "Components/StaticMeshComponent.h"
#include "DeepTestCVar.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "Immersion/DeepImmersion.h"
#include "Immersion/DeepSilt.h"
#include "Misc/AutomationTest.h"
#include "Sim/DeepCaveRules.h"
#include "Sim/DeepImmersionRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepEventBus.h"
#include "World/DeepArena.h"
#include "World/DeepShapes.h"
#include "World/DeepSubmarine.h"
#include "World/DeepSubRoom.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_IMMERSION_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepImmersionTest
{
	struct FCreakRun
	{
		int32 Count = 0;
		float MinGapS = 1.0e6f;
		float MeanPitch = 0.f;
	};

	/** Seconds of creak clock at a fixed state, stepped at Dt with a seeded stream. */
	FCreakRun RunCreaks(const FDeepCreakInputs& In, float Seconds, float Dt, const FDeepHullVoiceTuning& T, int32 Seed = 7)
	{
		FCreakRun Out;
		FDeepCreakClock Clock;
		FRandomStream Rand(Seed);
		float Since = -1.f;
		float PitchSum = 0.f;
		for (float Time = 0.f; Time < Seconds; Time += Dt)
		{
			const FDeepCreak Creak = DeepImmersionRules::StepCreak(Clock, In, Dt, Rand.FRand(), Rand.FRand(), T);
			if (Since >= 0.f)
			{
				Since += Dt;
			}
			if (Creak.bFire)
			{
				if (Since >= 0.f)
				{
					Out.MinGapS = FMath::Min(Out.MinGapS, Since);
				}
				Since = 0.f;
				++Out.Count;
				PitchSum += Creak.Pitch;
			}
		}
		Out.MeanPitch = Out.Count > 0 ? PitchSum / Out.Count : 0.f;
		return Out;
	}

	/** A game world with the arena and one diver aboard (as Deep.World.Cave's), off the title. */
	struct FWorld
	{
		UWorld* World = nullptr;
		ADeepGameMode* GameMode = nullptr;
		ADeepGameState* State = nullptr;
		ADeepSubmarine* Sub = nullptr;
		ADeepDiver* Diver = nullptr;
		UDeepRunComponent* Run = nullptr;
		UDeepEventBus* Bus = nullptr;

		FWorld()
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
			if (State)
			{
				State->bOnTitle = false;
			}
			Bus = UDeepEventBus::Get(World);
			if (Sub)
			{
				Diver = World->SpawnActor<ADeepDiver>(DeepArena::EnsureRoom(World)->GetRoomStartTransform().GetLocation(), FRotator::ZeroRotator);
			}
			Run = Diver ? Diver->GetRun() : nullptr;
			if (Run)
			{
				Run->StepRun(0.f);
			}
		}

		~FWorld()
		{
			if (World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		bool Valid() const { return GameMode && State && Sub && Diver && Run && Bus; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepImmersionCreakTest, "Deep.Sim.Immersion.Creak", DEEP_IMMERSION_TEST_FLAGS)
bool FDeepImmersionCreakTest::RunTest(const FString& Parameters)
{
	using namespace DeepImmersionTest;
	const FDeepHullVoiceTuning& T = UDeepTuningSettings::Get().Immersion.Voice;
	FDeepCreakInputs Shallow;
	Shallow.DepthM = T.ShallowM;
	FDeepCreakInputs Deep = Shallow;
	Deep.DepthM = T.DeepM;

	const FCreakRun Up = RunCreaks(Shallow, 1200.f, 0.05f, T);
	const FCreakRun Down = RunCreaks(Deep, 1200.f, 0.05f, T);
	AddInfo(FString::Printf(TEXT("20 min still: %d creaks at %.0f m (pitch %.2f), %d at %.0f m (pitch %.2f)"), Up.Count, Shallow.DepthM, Up.MeanPitch, Down.Count, Deep.DepthM, Down.MeanPitch));
	TestTrue(TEXT("Some creaks even up top"), Up.Count > 10);
	TestTrue(TEXT("More often deeper"), Down.Count > 2 * Up.Count);
	TestTrue(TEXT("About the tuned mean deep down (x0.5 to x1.6)"), Down.Count > 0.5f * 1200.f / T.DeepIntervalS && Down.Count < 1.6f * 1200.f / T.DeepIntervalS);
	TestTrue(TEXT("Lower deeper"), Down.MeanPitch < Up.MeanPitch);
	TestTrue(TEXT("Never closer than the minimum gap (still)"), Down.MinGapS >= T.MinGapS - 0.051f);

	// The hull working: a hard turn while sinking creaks far more, but still never closer than the gap.
	FDeepCreakInputs Working = Deep;
	Working.YawRateDegS = 2.f * T.TurnFullDegS;
	Working.DepthRateMps = -2.f * T.DepthChangeFullMps;
	const FCreakRun Work = RunCreaks(Working, 600.f, 0.05f, T);
	AddInfo(FString::Printf(TEXT("10 min turning and sinking: %d creaks, closest %.2f s apart"), Work.Count, Work.MinGapS));
	TestTrue(TEXT("Turns and depth changes bring more creaks"), Work.Count > RunCreaks(Deep, 600.f, 0.05f, T).Count);
	TestTrue(TEXT("Never closer than the minimum gap (working)"), Work.MinGapS >= T.MinGapS - 0.051f);
	TestTrue(TEXT("The rate counts the turn"), DeepImmersionRules::CreakRatePerS(Working, T) > DeepImmersionRules::CreakRatePerS(Deep, T));

	// The room at rest at the surface stop: rarer than driving at the same depth.
	FDeepCreakInputs Room = Shallow;
	Room.bParked = true;
	const FCreakRun Rest = RunCreaks(Room, 1200.f, 0.05f, T);
	AddInfo(FString::Printf(TEXT("20 min parked at the surface stop: %d creaks"), Rest.Count));
	TestTrue(TEXT("Rarer parked at the surface stop"), Rest.Count < Up.Count);
	FDeepCreakInputs DeepRoom = Deep;
	DeepRoom.bParked = true;
	TestEqual(TEXT("Parked deep keeps its rate"), DeepImmersionRules::CreakRatePerS(DeepRoom, T), DeepImmersionRules::CreakRatePerS(Deep, T));

	// A groan silences the creaks for a while.
	FDeepCreakClock Quiet;
	Quiet.Charge = 100.f;
	Quiet.Threshold = 0.5f;
	Quiet.QuietS = 3.f;
	TestFalse(TEXT("No creak while the groan rings"), DeepImmersionRules::StepCreak(Quiet, Working, 1.f, 0.5f, 0.5f, T).bFire);
	TestTrue(TEXT("Then it may"), DeepImmersionRules::StepCreak(Quiet, Working, 2.5f, 0.5f, 0.5f, T).bFire);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepImmersionGroanTest, "Deep.Sim.Immersion.Groan", DEEP_IMMERSION_TEST_FLAGS)
bool FDeepImmersionGroanTest::RunTest(const FString& Parameters)
{
	const FDeepHullVoiceTuning& T = UDeepTuningSettings::Get().Immersion.Voice;
	int32 Band = -1;
	TestFalse(TEXT("Starting at 100 m: no groan"), DeepImmersionRules::StepGroan(Band, 100.f, T));
	TestEqual(TEXT("Band 1 recorded"), Band, 1);
	TestFalse(TEXT("150 m: same band"), DeepImmersionRules::StepGroan(Band, 150.f, T));
	TestTrue(TEXT("200 m: a groan"), DeepImmersionRules::StepGroan(Band, 200.f, T));
	TestFalse(TEXT("Still 200 m: no second"), DeepImmersionRules::StepGroan(Band, 200.1f, T));
	TestFalse(TEXT("Back up to 190 m"), DeepImmersionRules::StepGroan(Band, 190.f, T));
	TestFalse(TEXT("Down past 200 m again: once per band"), DeepImmersionRules::StepGroan(Band, 230.f, T));
	TestTrue(TEXT("300 m"), DeepImmersionRules::StepGroan(Band, 300.f, T));
	TestTrue(TEXT("A jump to 550 m: one groan"), DeepImmersionRules::StepGroan(Band, 550.f, T));
	TestFalse(TEXT("No more until 600 m"), DeepImmersionRules::StepGroan(Band, 599.f, T));
	TestTrue(TEXT("600 m"), DeepImmersionRules::StepGroan(Band, 600.f, T));

	// A full descent from 100 m to just short of 600 m in small steps: 200, 300, 400 and 500 m.
	int32 Count = 0;
	int32 Fresh = -1;
	for (float Depth = 100.f; Depth <= 600.f; Depth += 0.37f)
	{
		Count += DeepImmersionRules::StepGroan(Fresh, Depth, T) ? 1 : 0;
	}
	TestEqual(TEXT("100 to 600 m: one groan per band"), Count, 4);
	TestTrue(TEXT("Each band heavier"), DeepImmersionRules::GroanPitch(500.f, T) < DeepImmersionRules::GroanPitch(200.f, T));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepImmersionFlowTest, "Deep.Sim.Immersion.Flow", DEEP_IMMERSION_TEST_FLAGS)
bool FDeepImmersionFlowTest::RunTest(const FString& Parameters)
{
	const FDeepHullVoiceTuning& T = UDeepTuningSettings::Get().Immersion.Voice;
	TestEqual(TEXT("Silent at rest"), DeepImmersionRules::FlowLevel(0.f, T).Volume01, 0.f);
	TestEqual(TEXT("Silent at a drift"), DeepImmersionRules::FlowLevel(0.5f * T.FlowStartMps, T).Volume01, 0.f);
	TestEqual(TEXT("Full at top speed"), DeepImmersionRules::FlowLevel(T.FlowFullMps, T).Volume01, 1.f, 1e-4f);
	float Last = -1.f;
	float LastHz = 0.f;
	bool bRises = true;
	for (float Speed = 0.f; Speed <= T.FlowFullMps; Speed += 0.25f)
	{
		const FDeepFlowLevel Level = DeepImmersionRules::FlowLevel(Speed, T);
		bRises = bRises && Level.Volume01 >= Last && Level.LowPassHz >= LastHz;
		Last = Level.Volume01;
		LastHz = Level.LowPassHz;
	}
	TestTrue(TEXT("Louder and brighter with speed"), bRises);
	TestTrue(TEXT("Astern counts too"), DeepImmersionRules::FlowLevel(-2.f, T).Volume01 > 0.f);

	// The mix: the flow loop is one of the loops, aboard only.
	TestTrue(TEXT("loop_flow is a loop"), DeepAudioLoop::All().Contains(FName(DeepAudioLoop::Flow)));
	FDeepAudioMixInputs In;
	In.bHasDiver = true;
	In.bInside = true;
	In.Flow.Volume01 = 0.7f;
	TestEqual(TEXT("Aboard: the flow plays"), DeepAudioRules::LoopTargets(In, UDeepAudioSettings::Get())[FName(DeepAudioLoop::Flow)].Volume01, 0.7f);
	In.bInside = false;
	TestEqual(TEXT("Swimming: no flow"), DeepAudioRules::LoopTargets(In, UDeepAudioSettings::Get())[FName(DeepAudioLoop::Flow)].Volume01, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepImmersionSiltTest, "Deep.Sim.Immersion.Silt", DEEP_IMMERSION_TEST_FLAGS)
bool FDeepImmersionSiltTest::RunTest(const FString& Parameters)
{
	const FDeepSiltTuning& T = UDeepTuningSettings::Get().Immersion.Silt;
	// Lift: only near the floor, only moving, more the nearer.
	TestEqual(TEXT("No floor found: nothing"), DeepImmersionRules::LiftPuffsPerS(-1.f, 3.f, T), 0.f);
	TestEqual(TEXT("Above the lift height: nothing"), DeepImmersionRules::LiftPuffsPerS(T.LiftHeightM + 0.1f, 3.f, T), 0.f);
	TestEqual(TEXT("At rest over the floor: nothing"), DeepImmersionRules::LiftPuffsPerS(0.2f, 0.f, T), 0.f);
	TestTrue(TEXT("Near the floor and moving: puffs"), DeepImmersionRules::LiftPuffsPerS(0.3f, 2.f, T) > 0.f);
	TestTrue(TEXT("Nearer lifts more"), DeepImmersionRules::LiftPuffsPerS(0.2f, 2.f, T) > DeepImmersionRules::LiftPuffsPerS(1.2f, 2.f, T));
	TestTrue(TEXT("Faster lifts more"), DeepImmersionRules::LiftPuffsPerS(0.5f, T.LiftFullMps, T) > DeepImmersionRules::LiftPuffsPerS(0.5f, 0.5f * T.LiftFullMps, T));
	TestTrue(TEXT("A scrape is rock within the gap"), DeepImmersionRules::IsScrape(0.1f, T) && !DeepImmersionRules::IsScrape(T.ScrapeGapM + 0.2f, T));
	TestEqual(TEXT("Moving with no thrust: its own speed"), DeepImmersionRules::WashSpeedMps(1.f, 0.f, T), 1.f);
	TestEqual(TEXT("Pinned on rock, thrusters full: full wash"), DeepImmersionRules::WashSpeedMps(0.f, 1.f, T), T.LiftFullMps);
	TestTrue(TEXT("Pinned near the floor and pushing: silt lifts"), DeepImmersionRules::LiftPuffsPerS(0.5f, DeepImmersionRules::WashSpeedMps(0.f, 0.8f, T), T) > 0.f);
	TestEqual(TEXT("No knock, no burst"), DeepImmersionRules::KnockPuffs(0.f, T), 0);
	TestTrue(TEXT("A harder knock bursts more, within the cap"), DeepImmersionRules::KnockPuffs(4.f, T) > DeepImmersionRules::KnockPuffs(1.f, T)
		&& DeepImmersionRules::KnockPuffs(100.f, T) == T.KnockPuffsMax);
	TestTrue(TEXT("A diver kicking near the floor lifts"), DeepImmersionRules::DiverLifts(0.5f, 1.5f, T) && !DeepImmersionRules::DiverLifts(3.f, 1.5f, T) && !DeepImmersionRules::DiverLifts(0.5f, 0.1f, T));

	// A puff: born on the open side of the surface; rises, spreads, hangs, fades out at the end of a 10 to 20 s life.
	FRandomStream Rand(3);
	const FVector Wall(500.0, 0.0, 0.0);
	const FVector Normal(-1.0, 0.0, 0.0);
	bool bOpenSide = true;
	bool bLife = true;
	for (int32 I = 0; I < 200; ++I)
	{
		const FDeepSiltPuff Puff = DeepImmersionRules::MakePuff(Wall, Normal, 1.f, Rand, T);
		bOpenSide = bOpenSide && FVector::DotProduct(Puff.BirthCm - Wall, Normal) > Puff.StartRadiusM * 100.f * 0.5f;
		bLife = bLife && Puff.LifeS >= FMath::Min(T.LifeS.X, T.LifeS.Y) && Puff.LifeS <= FMath::Max(T.LifeS.X, T.LifeS.Y);
	}
	TestTrue(TEXT("Born on the open side, off the rock by half its start radius"), bOpenSide);
	TestTrue(TEXT("Lives 10 to 20 s"), bLife);

	FDeepSiltPuff Puff = DeepImmersionRules::MakePuff(FVector::ZeroVector, FVector::UpVector, 1.f, Rand, T);
	Puff.LifeS = 15.f;
	const FDeepSiltPuffNow Born = DeepImmersionRules::PuffAt(Puff, T);
	Puff.AgeS = 4.f;
	const FDeepSiltPuffNow Hanging = DeepImmersionRules::PuffAt(Puff, T);
	Puff.AgeS = 15.f;
	const FDeepSiltPuffNow Gone = DeepImmersionRules::PuffAt(Puff, T);
	TestEqual(TEXT("Fades in from nothing"), Born.Opacity01, 0.f);
	TestTrue(TEXT("Hangs visible"), Hanging.Opacity01 > 0.25f * T.PeakOpacity);
	TestEqual(TEXT("Gone at the end of its life"), Gone.Opacity01, 0.f);
	TestTrue(TEXT("Spreads"), Hanging.RadiusCm > 1.5f * Born.RadiusCm);
	TestTrue(TEXT("Rises off the floor"), Hanging.LocationCm.Z > Born.LocationCm.Z + 30.0);

	// Bounded: never more than MaxPuffs alive; the dead are dropped.
	FDeepSiltPool Pool;
	for (int32 I = 0; I < 5 * T.MaxPuffs; ++I)
	{
		FDeepSiltPuff P = DeepImmersionRules::MakePuff(FVector::ZeroVector, FVector::UpVector, 1.f, Rand, T);
		P.AgeS = I * 0.01f;
		Pool.Add(P, T.MaxPuffs);
		if (Pool.Num() > T.MaxPuffs)
		{
			break;
		}
	}
	TestEqual(TEXT("Never more than MaxPuffs"), Pool.Num(), T.MaxPuffs);
	Pool.Step(FMath::Max(T.LifeS.X, T.LifeS.Y) + 1.f);
	TestEqual(TEXT("All gone after the longest life"), Pool.Num(), 0);

	// The hull's box: a puff reaching into it is shoved out, never back into the rock it rose from.
	const FBox Hull(FVector(-400.0, -150.0, -150.0), FVector(400.0, 150.0, 150.0));
	const FVector Floor(0.0, 0.0, -400.0);
	const FVector Up(0.0, 0.0, 1.0);
	const FVector Under(0.0, 20.0, -200.0);
	const FVector Kept = DeepImmersionRules::KeepOffBox(Under, 100.f, 1.f, Hull, Floor, Up, 30.f);
	TestTrue(TEXT("Under the hull and touching it: shoved"), !Kept.Equals(Under));
	TestFalse(TEXT("Then clear of the box"), DeepImmersionRules::TouchesBox(Kept, 99.f, Hull));
	TestTrue(TEXT("Shoved the short way: down, toward the floor but not into it"), Kept.Z < Under.Z && Kept.Z >= Floor.Z + 30.f);
	// The floor 1 m under the hull: no room below; it is shoved the next shortest way, never into the rock.
	const FVector LowFloor(0.0, 0.0, -250.0);
	const FVector Tight = DeepImmersionRules::KeepOffBox(FVector(0.0, 20.0, -200.0), 100.f, 1.f, Hull, LowFloor, Up, 30.f);
	TestTrue(TEXT("Tight under the hull: never into the floor"), Tight.Z >= LowFloor.Z + 30.f - 0.1f);
	TestFalse(TEXT("Tight under the hull: clear of the box"), DeepImmersionRules::TouchesBox(Tight, 99.f, Hull));
	const FVector Inside(10.0, 0.0, 0.0);
	TestFalse(TEXT("From inside the box: out"), DeepImmersionRules::TouchesBox(DeepImmersionRules::KeepOffBox(Inside, 80.f, 1.f, Hull, Floor, Up, 30.f), 79.f, Hull));
	const FVector Clear(0.0, 600.0, 0.0);
	TestTrue(TEXT("A clear puff stays"), DeepImmersionRules::KeepOffBox(Clear, 100.f, 1.f, Hull, Floor, Up, 30.f).Equals(Clear));
	TestEqual(TEXT("Squeezed to the gap"), DeepImmersionRules::FitRadiusCm(FVector(0.0, 0.0, -200.0), 1.f, Hull), 50.f, 0.01f);
	TestEqual(TEXT("Inside the box: no room at all"), DeepImmersionRules::FitRadiusCm(Inside, 1.f, Hull), 0.f);
	FDeepSiltPuff Squeezed = DeepImmersionRules::MakePuff(FVector::ZeroVector, FVector::UpVector, 1.f, Rand, T);
	Squeezed.AgeS = 5.f;
	Squeezed.FitRadiusCm = 20.f;
	TestEqual(TEXT("A squeezed puff draws no bigger than its fit"), DeepImmersionRules::PuffAt(Squeezed, T).RadiusCm, 20.f);

	// Drift never carries a puff into the wall it rose from.
	FDeepSiltPuff OffWall = DeepImmersionRules::MakePuff(Wall, Normal, 1.f, Rand, T);
	bool bOffWall = true;
	for (float Age = 0.f; Age < OffWall.LifeS; Age += 0.5f)
	{
		OffWall.AgeS = Age;
		bOffWall = bOffWall && FVector::DotProduct(DeepImmersionRules::PuffAt(OffWall, T).LocationCm - Wall, Normal) > 0.0;
	}
	TestTrue(TEXT("Never back through its wall"), bOffWall);

	// The wash: a puff lifted under a hull running ahead (+X) is thrown aft, along the floor, harder at speed.
	FDeepSiltPuff Washed = DeepImmersionRules::MakePuff(FVector::ZeroVector, FVector::UpVector, 1.f, Rand, T);
	DeepImmersionRules::Wash(Washed, FVector(T.LiftFullMps, 0.0, 0.0), T);
	TestTrue(TEXT("Washed aft"), Washed.Out.X < -0.5);
	TestTrue(TEXT("Along the floor"), FMath::Abs(Washed.Out.Z) < 1e-3);
	TestTrue(TEXT("Harder than a still lift"), Washed.OutMps > T.OutwardMps);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepImmersionEchoTest, "Deep.Sim.Immersion.Echo", DEEP_IMMERSION_TEST_FLAGS)
bool FDeepImmersionEchoTest::RunTest(const FString& Parameters)
{
	const FDeepEchoTuning& T = UDeepTuningSettings::Get().Immersion.Echo;
	TestEqual(TEXT("Nothing came back: no echo"), DeepImmersionRules::EchoTaps({ -1.f, -1.f, -1.f }, T).Num(), 0);
	TestEqual(TEXT("No beams: no echo"), DeepImmersionRules::EchoTaps({}, T).Num(), 0);

	// The delay grows with the distance; the level and the brightness fall.
	float LastDelay = -1.f;
	float LastGain = 2.f;
	float LastHz = 1.0e6f;
	bool bOrdered = true;
	for (float M = 1.f; M <= 90.f; M += 4.f)
	{
		const FDeepEchoTap Tap = DeepImmersionRules::EchoTap(M, 1.f, T);
		bOrdered = bOrdered && Tap.DelayS > LastDelay && Tap.Gain <= LastGain && Tap.LowPassHz <= LastHz;
		LastDelay = Tap.DelayS;
		LastGain = Tap.Gain;
		LastHz = Tap.LowPassHz;
	}
	TestTrue(TEXT("Farther: later, fainter, darker"), bOrdered);

	// A tunnel (walls 2 to 4 m round, its length out of range) against a cavern (walls 35 to 50 m round).
	TArray<float> Tunnel;
	TArray<float> Cavern;
	for (int32 Beam = 0; Beam < 32; ++Beam)
	{
		const float A = 2.f * PI * Beam / 32.f;
		const float Side = FMath::Abs(FMath::Sin(A));
		Tunnel.Add(Side > 0.3f ? 2.5f / Side : -1.f);
		Cavern.Add(35.f + 15.f * FMath::Abs(FMath::Cos(A)));
	}
	const TArray<FDeepEchoTap> Short = DeepImmersionRules::EchoTaps(Tunnel, T);
	const TArray<FDeepEchoTap> Long = DeepImmersionRules::EchoTaps(Cavern, T);
	UTEST_TRUE(TEXT("Both answer"), Short.Num() > 0 && Long.Num() > 0);
	AddInfo(FString::Printf(TEXT("Tunnel: %.2f s x%.2f %.0f Hz; cavern: %.2f s x%.2f %.0f Hz"), Short[0].DelayS, Short[0].Gain, Short[0].LowPassHz, Long[0].DelayS, Long[0].Gain, Long[0].LowPassHz));
	TestTrue(TEXT("Tunnel: short"), Short[0].DelayS < 0.15f);
	TestTrue(TEXT("Cavern: long (a cavern sounds big)"), Long[0].DelayS > 0.4f);
	TestTrue(TEXT("Tunnel louder than the cavern"), Short[0].Gain > 2.f * Long[0].Gain);
	TestTrue(TEXT("Tunnel brighter (dry), cavern darker"), Short[0].LowPassHz > Long[0].LowPassHz);
	for (const FDeepEchoTap& Tap : Long)
	{
		TestTrue(TEXT("Every return later than the near one"), Tap.DelayS >= Long[0].DelayS);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepImmersionSoundsTest, "Deep.Sim.Immersion.Sounds", DEEP_IMMERSION_TEST_FLAGS)
bool FDeepImmersionSoundsTest::RunTest(const FString& Parameters)
{
	const TArray<FName> Creaks = DeepSynth::Variants(FName(TEXT("hull_creak")));
	TestEqual(TEXT("Three creak variations"), Creaks.Num(), 3);
	TestTrue(TEXT("The family is a built-in"), DeepSynth::IsBuiltin(FName(TEXT("hull_creak"))));
	TestEqual(TEXT("A plain built-in has no family"), DeepSynth::Variants(FName(TEXT("clank"))).Num(), 0);
	TestEqual(TEXT("The family's length is its first's"), DeepSynth::ExpectedSamples(FName(TEXT("hull_creak"))), DeepSynth::ExpectedSamples(Creaks[0]));
	for (int32 A = 0; A < Creaks.Num(); ++A)
	{
		for (int32 B = A + 1; B < Creaks.Num(); ++B)
		{
			TestFalse(TEXT("Variations differ"), DeepSynth::Generate(Creaks[A]).Samples == DeepSynth::Generate(Creaks[B]).Samples);
		}
	}
	TestTrue(TEXT("The groan is long"), DeepSynth::ExpectedSamples(FName(TEXT("hull_groan"))) >= 4 * DeepSynth::SampleRate);
	TestTrue(TEXT("The flow loops"), DeepSynth::IsLoopBuiltin(FName(TEXT("flow"))));

	FDeepSoundTable Table;
	Table.LoadDefault();
	for (const TCHAR* Row : { TEXT("hull_creak"), TEXT("hull_groan"), TEXT("sonar_echo"), TEXT("loop_flow") })
	{
		const FDeepSoundRow* Found = Table.Find(FName(Row));
		TestTrue(FString::Printf(TEXT("%s: a Sounds.csv row with a built-in"), Row), Found && DeepSynth::IsBuiltin(Found->Builtin));
	}
	const FDeepSoundRow* Ping = Table.Find(FName(TEXT("sonar_ping")));
	TestTrue(TEXT("The ping is dry: its returns come from the walls"), Ping && Ping->Builtin == FName(TEXT("sonar_ping_dry")));

	// The ping and its echo render: a tunnel's ends sooner than a cavern's.
	const TArray<int16> Near = DeepSynth::RenderEcho(FName(TEXT("sonar_ping_dry")), FName(TEXT("sonar_echo")), { { 0.1f, 0.9f, 5000.f } }, 1.f);
	const TArray<int16> Far = DeepSynth::RenderEcho(FName(TEXT("sonar_ping_dry")), FName(TEXT("sonar_echo")), { { 0.7f, 0.2f, 1200.f } }, 1.f);
	TestTrue(TEXT("A far echo runs longer"), Far.Num() > Near.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepImmersionVoiceWorldTest, "Deep.World.Immersion.Voice", DEEP_IMMERSION_TEST_FLAGS)
bool FDeepImmersionVoiceWorldTest::RunTest(const FString& Parameters)
{
	using namespace DeepImmersionTest;
	DeepTest::FCVarScope On(TEXT("Deep.Immersion"), 1);
	FWorld W;
	UTEST_TRUE(TEXT("World, state, sub, diver, run, bus"), W.Valid());
	UDeepImmersion* Immersion = UDeepImmersion::Get(W.World);
	UDeepAudioSubsystem* Audio = UDeepAudioSubsystem::Get(W.World);
	UTEST_TRUE(TEXT("Immersion and audio"), Immersion && Audio);
	TestTrue(TEXT("Aboard: the hull is heard"), Immersion->IsHearingHull());
	Immersion->ResetVoice();
	Audio->ClearHistory();

	// Descend from 100 to 320 m over a minute and a half: the bands groan once each, creaks come.
	W.State->bParked = false;
	for (float Time = 0.f; Time < 90.f; Time += 0.1f)
	{
		W.State->SubDepthM = 100.f + 220.f * Time / 90.f;
		Immersion->StepVoice(0.1f);
	}
	TestEqual(TEXT("200 and 300 m: two groans"), Immersion->GetGroans(), 2);
	TestTrue(TEXT("Creaks on the way down"), Immersion->GetCreaks() >= 3);
	TestTrue(TEXT("The audio heard them"), Audio->CountOf(FName(TEXT("hull_groan"))) == 2 && Audio->CountOf(FName(TEXT("hull_creak"))) >= 3);

	// Back up and down again: no new groan for bands already passed.
	for (float Depth = 320.f; Depth > 150.f; Depth -= 2.f)
	{
		W.State->SubDepthM = Depth;
		Immersion->StepVoice(0.1f);
	}
	for (float Depth = 150.f; Depth < 330.f; Depth += 2.f)
	{
		W.State->SubDepthM = Depth;
		Immersion->StepVoice(0.1f);
	}
	TestEqual(TEXT("Once per band"), Immersion->GetGroans(), 2);

	// Off: nothing.
	{
		DeepTest::FCVarScope Off(TEXT("Deep.Immersion"), 0);
		const int32 Before = Immersion->GetCreaks();
		for (float Time = 0.f; Time < 60.f; Time += 0.1f)
		{
			W.State->SubDepthM = 450.f + Time;
			Immersion->StepVoice(0.1f);
		}
		TestEqual(TEXT("Deep.Immersion 0: no creaks"), Immersion->GetCreaks(), Before);
		TestFalse(TEXT("Deep.Immersion 0: not heard"), Immersion->IsHearingHull());
	}

	// The echo: each return plays after its delay.
	Audio->ClearHistory();
	const TArray<FDeepEchoTap> Taps = Immersion->ScheduleEcho({ 3.f, 3.5f, 40.f, 45.f });
	UTEST_TRUE(TEXT("An echo"), Taps.Num() > 0);
	TestEqual(TEXT("Waiting"), Audio->GetPendingCount(), Taps.Num());
	Audio->StepAudio(0.01f);
	TestEqual(TEXT("Not before its delay"), Audio->CountOf(FName(TEXT("sonar_echo"))), 0);
	for (int32 Step = 0; Step < 30; ++Step)
	{
		Audio->StepAudio(0.05f);
	}
	TestEqual(TEXT("Every return played"), Audio->CountOf(FName(TEXT("sonar_echo"))), Taps.Num());
	TestEqual(TEXT("Nothing left waiting"), Audio->GetPendingCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepImmersionSiltWorldTest, "Deep.World.Immersion.Silt", DEEP_IMMERSION_TEST_FLAGS)
bool FDeepImmersionSiltWorldTest::RunTest(const FString& Parameters)
{
	using namespace DeepImmersionTest;
	DeepTest::FCVarScope On(TEXT("Deep.Immersion"), 1);
	FWorld W;
	UTEST_TRUE(TEXT("World, state, sub, diver, run, bus"), W.Valid());
	const FDeepSiltTuning& T = UDeepTuningSettings::Get().Immersion.Silt;
	ADeepSilt* Silt = ADeepSilt::Ensure(W.World);
	UTEST_TRUE(TEXT("The silt actor"), Silt != nullptr);
	TestTrue(TEXT("One per world"), ADeepSilt::Ensure(W.World) == Silt);

	// A flat floor of rock (world static) 0.8 m under the hull's solid box, wide enough for a run along it.
	const FBox Box = W.Sub->GetSolidBoundsLocal(false);
	UTEST_TRUE(TEXT("The hull has a box"), Box.IsValid != 0);
	const FVector Start = W.Sub->GetActorLocation() + FVector(0.0, 0.0, 3000.0);
	W.Sub->SetActorLocation(Start);
	const double BottomZ = W.Sub->GetActorTransform().TransformPosition(Box.GetCenter()).Z - Box.GetExtent().Z;
	const double FloorTopZ = BottomZ - 80.0;
	AStaticMeshActor* Floor = W.World->SpawnActor<AStaticMeshActor>(FVector(Start.X, Start.Y, FloorTopZ - 50.0), FRotator::ZeroRotator);
	UTEST_TRUE(TEXT("A floor"), Floor != nullptr);
	Floor->SetMobility(EComponentMobility::Movable);
	Floor->GetStaticMeshComponent()->SetStaticMesh(DeepShapes::LoadMesh(DeepShapes::Cube));
	Floor->SetActorScale3D(FVector(200.0, 200.0, 1.0));
	Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	Floor->GetStaticMeshComponent()->SetCollisionObjectType(ECC_WorldStatic);

	auto Drive = [&](const FVector& From, const FVector& VelocityCmS, float Seconds, bool& bEverInHull, bool& bEverUnderRock)
	{
		W.Sub->SetActorLocation(From);
		for (float Time = 0.f; Time < Seconds; Time += 0.1f)
		{
			W.Sub->SetActorLocation(From + VelocityCmS * Time);
			Silt->StepSilt(0.1f);
			bEverInHull = bEverInHull || Silt->AnyPuffInHull();
			for (int32 I = 0; I < Silt->GetPuffCount(); ++I)
			{
				bEverUnderRock = bEverUnderRock || Silt->PuffNowWorld(I).LocationCm.Z < FloorTopZ;
			}
		}
	};

	// Driving 0.8 m over the floor at 2.5 m/s: silt lifts, never inside the hull's box, never under the rock.
	bool bInHull = false;
	bool bUnderRock = false;
	Silt->Clear();
	Silt->ProbeNextStep();
	Drive(Start, FVector(250.0, 0.0, 0.0), 4.f, bInHull, bUnderRock);
	AddInfo(FString::Printf(TEXT("0.8 m over the floor at 2.5 m/s for 4 s: %d puffs lifted, clearance %.2f m"), Silt->GetLifted(), Silt->GetHullClearanceM()));
	TestTrue(TEXT("Clearance read off the floor"), FMath::IsNearlyEqual(Silt->GetHullClearanceM(), 0.8f, 0.1f));
	TestTrue(TEXT("Silt lifts near the floor"), Silt->GetLifted() > 3);
	TestFalse(TEXT("Never inside the hull's box"), bInHull);
	TestFalse(TEXT("Never under the floor's top"), bUnderRock);

	// The same run 5 m up: nothing lifts.
	const int32 LiftedLow = Silt->GetLifted();
	Drive(Start + FVector(0.0, 0.0, 400.0), FVector(250.0, 0.0, 0.0), 4.f, bInHull, bUnderRock);
	TestEqual(TEXT("5 m up: nothing lifts"), Silt->GetLifted(), LiftedLow);

	// At rest just over the floor: nothing lifts either.
	Drive(Start, FVector::ZeroVector, 3.f, bInHull, bUnderRock);
	TestEqual(TEXT("At rest: nothing lifts"), Silt->GetLifted(), LiftedLow);

	// A knock bursts silt off the nearest rock.
	FDeepEventPayload Knock;
	Knock.ImpactMps = 3.f;
	W.Bus->Broadcast(FName(DeepEvent::HullHit), Knock);
	TestTrue(TEXT("A knock bursts silt"), Silt->GetKnocked() >= T.KnockPuffs);

	// Puffs dropped right into the hull are shoved out of it on the next step.
	Silt->Clear();
	const FVector Centre = W.Sub->GetActorTransform().TransformPosition(Box.GetCenter());
	Silt->Burst(Centre, FVector::UpVector, 6);
	Silt->StepSilt(0.1f);
	TestFalse(TEXT("Shoved out of the hull"), Silt->AnyPuffInHull());

	// Bounded however much is thrown at it.
	Silt->Burst(Centre + FVector(0.0, 0.0, -100.0), FVector::UpVector, 10 * T.MaxPuffs);
	Silt->StepSilt(0.1f);
	TestTrue(TEXT("Bounded"), Silt->GetPuffCount() <= T.MaxPuffs);

	// Off: none.
	{
		DeepTest::FCVarScope Off(TEXT("Deep.Immersion"), 0);
		Silt->StepSilt(0.1f);
		TestEqual(TEXT("Deep.Immersion 0: no silt"), Silt->GetPuffCount(), 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepImmersionShaftTest, "Deep.World.Immersion.ShaftUnchanged", DEEP_IMMERSION_TEST_FLAGS)
bool FDeepImmersionShaftTest::RunTest(const FString& Parameters)
{
	using namespace DeepImmersionTest;
	// The caves off, the shaft on (the game today): the same steering gives the same sub with I1 running as without it.
	DeepTest::FCVarScope Caves(TEXT("Deep.World.Caves"), 0);
	DeepTest::FCVarScope Shaft(TEXT("Deep.Cave.Shaft"), 1);
	struct FResult
	{
		FVector2D Offset = FVector2D::ZeroVector;
		float Depth = 0.f;
		int32 Damage = 0;
		int32 Hits = 0;
		bool bValid = false;
	};
	auto RunShaft = [this](bool bImmersion) -> FResult
	{
		DeepTest::FCVarScope Switch(TEXT("Deep.Immersion"), bImmersion ? 1 : 0);
		FResult Out;
		FWorld W;
		if (!W.Valid())
		{
			return Out;
		}
		const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
		W.Run->TakeHelm();
		const float Depth = 0.5f * (Tuning.GetStop(1).SubDepthM + Tuning.GetStop(2).SubDepthM);
		W.State->SubDepthM = Depth;
		W.State->SubOffsetM = UDeepCaveRules::CentreM(Depth, Tuning.StopDepthsM(), Tuning.Cave);
		W.State->bParked = false;
		DeepArena::ApplySubDepth(W.World);
		UDeepImmersion* Immersion = UDeepImmersion::Get(W.World);
		ADeepSilt* Silt = bImmersion ? ADeepSilt::Ensure(W.World) : nullptr;
		// Steer into the wall and along it: knocks, scrapes, silt, creaks.
		for (int32 Step = 0; Step < 200; ++Step)
		{
			W.Run->SetHelmSteer(Step < 120 ? FVector2D(0.f, 1.f) : FVector2D(1.f, 0.f));
			W.Run->StepRun(0.05f);
			if (Immersion)
			{
				Immersion->StepVoice(0.05f);
			}
			if (Silt)
			{
				Silt->StepSilt(0.05f);
			}
		}
		Out.Offset = W.State->SubOffsetM;
		Out.Depth = W.State->SubDepthM;
		Out.Damage = W.State->HullDamage;
		Out.Hits = W.Bus->CountOf(DeepEvent::HullHit);
		Out.bValid = true;
		AddInfo(FString::Printf(TEXT("Immersion %d: offset (%.3f, %.3f), depth %.2f, damage %d, hits %d, silt %d"), bImmersion ? 1 : 0,
			Out.Offset.X, Out.Offset.Y, Out.Depth, Out.Damage, Out.Hits, Silt ? Silt->GetPuffCount() : -1));
		return Out;
	};
	const FResult With = RunShaft(true);
	const FResult Without = RunShaft(false);
	UTEST_TRUE(TEXT("Both runs"), With.bValid && Without.bValid);
	TestTrue(TEXT("The run touched the rock (the test means something)"), Without.Hits > 0 || Without.Damage > 0);
	TestTrue(TEXT("Same place"), With.Offset.Equals(Without.Offset, 1e-4f) && FMath::IsNearlyEqual(With.Depth, Without.Depth, 1e-4f));
	TestEqual(TEXT("Same hull"), With.Damage, Without.Damage);
	TestEqual(TEXT("Same knocks"), With.Hits, Without.Hits);
	return true;
}

#endif
