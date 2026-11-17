// Sonar tests (Sonar lane). Deep.UI.Sonar.*: the pure rules (bearing and range to the scope, the sweep and its
// ping, the phosphor fade, the lower slice, the proximity tone), the helm panel set and the helm layout at 16:9.
// Deep.World.Sonar.*: a test world with blocks round a point gives the right ranges and bearings on both slices and
// the bottom under it; salvage and the serpent show as blips from the actors themselves; the sweep keeps to its
// trace budget, pings once a turn, and a wall close to the hull sets the proximity tone going.

#include "CoreMinimal.h"
#include "Camera/DeepCameraSubsystem.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Monster/DeepSerpent.h"
#include "Sim/DeepTuningSettings.h"
#include "Sonar/DeepSonarComponent.h"
#include "Sonar/DeepSonarRules.h"
#include "Story/DeepEventBus.h"
#include "UI/DeepHudFrame.h"
#include "UI/DeepHudModel.h"
#include "World/DeepSalvage.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_SONAR_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepSonarTest
{
	const FDeepSonarTuning& Tune() { return UDeepTuningSettings::Get().Sonar; }

	/** A bare game world with a sonar on a holder actor at the origin, bow along +X. */
	struct FSonarWorld
	{
		UWorld* World = nullptr;
		UDeepSonarComponent* Sonar = nullptr;

		FSonarWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			AActor* Holder = World->SpawnActor<AActor>();
			Sonar = NewObject<UDeepSonarComponent>(Holder);
			Sonar->SetPlaySounds(false);
			Sonar->RegisterComponent();
			Sonar->SetTestFrame(FVector::ZeroVector, FVector::ForwardVector);
		}

		~FSonarWorld()
		{
			if (World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		/** A solid box (blocks every channel) centred at Centre, half size Extent, all in centimetres. */
		AActor* Block(const FVector& Centre, const FVector& Extent)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
			Box->SetBoxExtent(Extent);
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetCollisionResponseToAllChannels(ECR_Block);
			Actor->SetRootComponent(Box);
			Box->RegisterComponent();
			Box->SetWorldLocation(Centre);
			return Actor;
		}

		float Range(int32 Slice, float BearingDeg) const
		{
			const TArray<FDeepSonarSample>& S = Sonar->GetSlice(Slice);
			return S.Num() > 0 ? S[UDeepSonarRules::BeamAt(BearingDeg, S.Num())].RangeM : -2.f;
		}
	};
}

// ------------------------------------------------------------------------------------------------ pure rules

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSonarMappingTest, "Deep.UI.Sonar.Mapping", DEEP_SONAR_TEST_FLAGS)
bool FDeepSonarMappingTest::RunTest(const FString& Parameters)
{
	// The scope: bow up, starboard right, ranges scale to the radius and stop at the rim.
	TestTrue(TEXT("Bow is straight up"), UDeepSonarRules::PanelPoint(0.f, 30.f, 60.f, 200.f).Equals(FVector2D(0.0, -100.0), 0.01));
	TestTrue(TEXT("Starboard is right"), UDeepSonarRules::PanelPoint(90.f, 60.f, 60.f, 200.f).Equals(FVector2D(200.0, 0.0), 0.01));
	TestTrue(TEXT("Astern is down"), UDeepSonarRules::PanelPoint(180.f, 15.f, 60.f, 200.f).Equals(FVector2D(0.0, 50.0), 0.01));
	TestTrue(TEXT("Port is left"), UDeepSonarRules::PanelPoint(270.f, 30.f, 60.f, 200.f).Equals(FVector2D(-100.0, 0.0), 0.01));
	TestTrue(TEXT("Past the range sits on the rim"), UDeepSonarRules::PanelPoint(0.f, 500.f, 60.f, 200.f).Equals(FVector2D(0.0, -200.0), 0.01));

	// Bearings from the bow, clockwise seen from above (+Y is to the right of +X in Unreal).
	const FVector Bow = FVector::ForwardVector;
	TestEqual(TEXT("Ahead is 0"), UDeepSonarRules::BearingDeg(Bow, FVector(500, 0, 0)), 0.f, 0.01f);
	TestEqual(TEXT("+Y is 90"), UDeepSonarRules::BearingDeg(Bow, FVector(0, 500, 0)), 90.f, 0.01f);
	TestEqual(TEXT("Behind is 180"), UDeepSonarRules::BearingDeg(Bow, FVector(-500, 0, 0)), 180.f, 0.01f);
	TestEqual(TEXT("-Y is 270"), UDeepSonarRules::BearingDeg(Bow, FVector(0, -500, 0)), 270.f, 0.01f);
	TestEqual(TEXT("Height does not change a bearing"), UDeepSonarRules::BearingDeg(Bow, FVector(500, 500, -4000)), 45.f, 0.01f);
	TestEqual(TEXT("A turned bow turns the bearings"), UDeepSonarRules::BearingDeg(FVector(0, 1, 0), FVector(500, 0, 0)), 270.f, 0.01f);
	TestEqual(TEXT("Plan range ignores height"), UDeepSonarRules::PlanRangeM(FVector(300, 400, 9000)), 5.f, 0.001f);

	// Beams: beam 0 on the bow, and every beam's direction reads back as its own bearing.
	const int32 N = 90;
	TestTrue(TEXT("Beam 0 is the bow"), UDeepSonarRules::BeamDirection(Bow, 0, N).Equals(Bow, 1e-4));
	TestTrue(TEXT("A quarter turn of beams is +Y"), UDeepSonarRules::BeamDirection(Bow, 0, 4).Equals(Bow, 1e-4)
		&& UDeepSonarRules::BeamDirection(Bow, 1, 4).Equals(FVector(0, 1, 0), 1e-4));
	for (int32 I = 0; I < N; I += 7)
	{
		const FVector Dir = UDeepSonarRules::BeamDirection(FVector(1, 1, 0), I, N);
		TestEqual(FString::Printf(TEXT("Beam %d reads back"), I), UDeepSonarRules::BeamAt(UDeepSonarRules::BearingDeg(FVector(1, 1, 0), Dir), N), I);
		TestEqual(FString::Printf(TEXT("Beam %d is flat and unit"), I), static_cast<float>(Dir.Size()), 1.f, 1e-4f);
	}
	TestEqual(TEXT("Just short of a full turn rounds to beam 0"), UDeepSonarRules::BeamAt(359.f, N), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSonarTimingTest, "Deep.UI.Sonar.Timing", DEEP_SONAR_TEST_FLAGS)
bool FDeepSonarTimingTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("The sweep starts on the bow"), UDeepSonarRules::SweepDeg(0.f, 2.5f), 0.f, 0.01f);
	TestEqual(TEXT("A quarter period is starboard"), UDeepSonarRules::SweepDeg(0.625f, 2.5f), 90.f, 0.01f);
	TestEqual(TEXT("A full period is back on the bow"), UDeepSonarRules::SweepDeg(2.5f, 2.5f), 0.f, 0.01f);
	TestTrue(TEXT("Passing the bow pings"), UDeepSonarRules::PingDue(350.f, 5.f));
	TestFalse(TEXT("Mid-turn is quiet"), UDeepSonarRules::PingDue(5.f, 10.f));
	TestTrue(TEXT("A bearing inside the swept arc is swept"), UDeepSonarRules::SweptOver(20.f, 10.f, 30.f));
	TestFalse(TEXT("Behind the arc is not"), UDeepSonarRules::SweptOver(5.f, 10.f, 30.f));
	TestFalse(TEXT("The arc's start was swept last frame"), UDeepSonarRules::SweptOver(10.f, 10.f, 30.f));
	TestTrue(TEXT("The arc's end is swept"), UDeepSonarRules::SweptOver(30.f, 10.f, 30.f));
	TestTrue(TEXT("An arc over the bow wraps"), UDeepSonarRules::SweptOver(2.f, 350.f, 10.f));
	// The Captain's serpent call: once per stop.
	TestTrue(TEXT("The first contact calls"), UDeepSonarRules::SerpentCallDue(-1, 2));
	TestFalse(TEXT("Not twice at a stop"), UDeepSonarRules::SerpentCallDue(2, 2));
	TestTrue(TEXT("A new stop calls again"), UDeepSonarRules::SerpentCallDue(2, 3));
	TestFalse(TEXT("A still sweep sweeps nothing"), UDeepSonarRules::SweptOver(10.f, 10.f, 10.f));

	// One turn in frames: exactly one ping, and every beam bearing is swept exactly once.
	int32 Pings = 0;
	TArray<int32> Hits;
	Hits.SetNumZeroed(90);
	float Prev = UDeepSonarRules::SweepDeg(0.01f, 2.5f);
	for (float T = 0.01f + 1.f / 60.f; T <= 2.51f + 1.f / 60.f; T += 1.f / 60.f)
	{
		const float Next = UDeepSonarRules::SweepDeg(T, 2.5f);
		Pings += UDeepSonarRules::PingDue(Prev, Next) ? 1 : 0;
		for (int32 B = 0; B < 90; ++B)
		{
			Hits[B] += UDeepSonarRules::SweptOver(4.f * B + 0.5f, Prev, Next) ? 1 : 0;
		}
		Prev = Next;
	}
	TestEqual(TEXT("One ping a turn"), Pings, 1);
	for (int32 B = 0; B < 90; B += 11)
	{
		TestEqual(FString::Printf(TEXT("Bearing %d swept once"), 4 * B), Hits[B], 1);
	}

	// The ping's own cadence (Nov 17): the jam style pings once a sweep; the soft style once every PingPeriodS while
	// the sweep still turns every SweepPeriodS. Over 18 s at 60 fps that is 7 jam pings and 3 soft ones.
	const FDeepSonarTuning Cadence;
	TestEqual(TEXT("Jam style: a ping a sweep"), UDeepSonarRules::PingPeriodFor(0, Cadence), Cadence.SweepPeriodS);
	TestEqual(TEXT("Soft style: a ping every PingPeriodS"), UDeepSonarRules::PingPeriodFor(1, Cadence), Cadence.PingPeriodS);
	TestTrue(TEXT("About one ping every 6 s (brief)"), Cadence.PingPeriodS >= 5.f && Cadence.PingPeriodS <= 7.f && Cadence.PingPeriodS > Cadence.SweepPeriodS);
	TestTrue(TEXT("Crossing a period pings"), UDeepSonarRules::PingDueAt(5.99, 6.01, 6.f));
	TestFalse(TEXT("Inside a period is quiet"), UDeepSonarRules::PingDueAt(6.01, 6.03, 6.f));
	TestFalse(TEXT("A still clock never pings"), UDeepSonarRules::PingDueAt(6.0, 6.0, 6.f));
	int32 JamPings = 0;
	int32 SoftPings = 0;
	int32 SweepPings = 0;
	double Clock = 0.0;
	float SweepPrev = 0.f;
	for (int32 F = 0; F < 18 * 60 + 1; ++F)
	{
		const double Next = Clock + 1.0 / 60.0;
		const float SweepNext = UDeepSonarRules::SweepDeg(static_cast<float>(FMath::Fmod(Next, 2.5)), 2.5f);
		JamPings += UDeepSonarRules::PingDueAt(Clock, Next, UDeepSonarRules::PingPeriodFor(0, Cadence)) ? 1 : 0;
		SoftPings += UDeepSonarRules::PingDueAt(Clock, Next, 6.f) ? 1 : 0;
		SweepPings += UDeepSonarRules::PingDue(SweepPrev, SweepNext) ? 1 : 0;
		Clock = Next;
		SweepPrev = SweepNext;
	}
	TestEqual(TEXT("18 s, jam: 7 pings"), JamPings, 7);
	TestEqual(TEXT("The jam cadence matches the sweep's own bow crossings"), JamPings, SweepPings);
	TestEqual(TEXT("18 s, soft: 3 pings"), SoftPings, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSonarFalloffTest, "Deep.UI.Sonar.Falloff", DEEP_SONAR_TEST_FLAGS)
bool FDeepSonarFalloffTest::RunTest(const FString& Parameters)
{
	FDeepSonarTuning T;
	T.RangeM = 60.f;
	T.PersistS = 3.f;
	T.RangeFalloff = 0.4f;
	T.SliceBelowM = 20.f;
	T.BedClearanceM = 2.f;
	TestEqual(TEXT("Fresh and near is full"), UDeepSonarRules::ReturnStrength(0.f, 0.f, T), 1.f, 1e-4f);
	TestEqual(TEXT("Fresh at full range is dimmer by the falloff"), UDeepSonarRules::ReturnStrength(0.f, 60.f, T), 0.6f, 1e-4f);
	TestEqual(TEXT("Gone after the persistence"), UDeepSonarRules::ReturnStrength(3.f, 10.f, T), 0.f, 1e-4f);
	TestEqual(TEXT("Not before it was swept"), UDeepSonarRules::ReturnStrength(-0.1f, 10.f, T), 0.f, 1e-4f);
	float Last = 2.f;
	bool bFalls = true;
	for (float Age = 0.f; Age < 3.f; Age += 0.25f)
	{
		const float S = UDeepSonarRules::ReturnStrength(Age, 20.f, T);
		bFalls &= S < Last;
		Last = S;
	}
	TestTrue(TEXT("A return only fades"), bFalls);

	TestEqual(TEXT("No bottom: the full slice"), UDeepSonarRules::LowerSliceBelowM(-1.f, T), 20.f, 1e-4f);
	TestEqual(TEXT("A far bottom: the full slice"), UDeepSonarRules::LowerSliceBelowM(100.f, T), 20.f, 1e-4f);
	TestEqual(TEXT("A near bottom: the slice rides above it"), UDeepSonarRules::LowerSliceBelowM(10.f, T), 8.f, 1e-4f);
	TestEqual(TEXT("On the bottom: the slice is the sub's"), UDeepSonarRules::LowerSliceBelowM(1.f, T), 0.f, 1e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSonarProximityTest, "Deep.UI.Sonar.Proximity", DEEP_SONAR_TEST_FLAGS)
bool FDeepSonarProximityTest::RunTest(const FString& Parameters)
{
	const FDeepSonarTuning T = DeepSonarTest::Tune();
	TestEqual(TEXT("No wall, no tone"), UDeepSonarRules::ProximityLevel(-1.f, T), 0.f);
	TestEqual(TEXT("At the start distance, silent"), UDeepSonarRules::ProximityLevel(T.ToneStartM, T), 0.f);
	TestTrue(TEXT("Just inside it, audible"), UDeepSonarRules::ProximityLevel(T.ToneStartM - 0.01f, T) > 0.f);
	TestEqual(TEXT("At the full distance, full"), UDeepSonarRules::ProximityLevel(T.ToneFullM, T), 1.f, 1e-4f);
	TestEqual(TEXT("Touching, full"), UDeepSonarRules::ProximityLevel(0.f, T), 1.f, 1e-4f);
	float Last = -1.f;
	bool bRises = true;
	for (float C = T.ToneStartM - 0.5f; C >= T.ToneFullM; C -= 0.5f)
	{
		const float Level = UDeepSonarRules::ProximityLevel(C, T);
		bRises &= Level >= Last;
		Last = Level;
	}
	TestTrue(TEXT("The level rises as the wall closes"), bRises);

	TestEqual(TEXT("Silent has no beeps"), UDeepSonarRules::ToneIntervalS(0.f, T), 0.f);
	TestEqual(TEXT("Full beeps fastest"), UDeepSonarRules::ToneIntervalS(1.f, T), T.ToneFastS, 1e-4f);
	TestTrue(TEXT("Closer beeps quicker"), UDeepSonarRules::ToneIntervalS(0.8f, T) < UDeepSonarRules::ToneIntervalS(0.2f, T));
	TestTrue(TEXT("Closer beeps higher"), UDeepSonarRules::TonePitch(0.8f, T) > UDeepSonarRules::TonePitch(0.2f, T));
	TestTrue(TEXT("Closer beeps louder"), UDeepSonarRules::ToneVolume(0.8f, T) > UDeepSonarRules::ToneVolume(0.2f, T));
	TestEqual(TEXT("Silent is silent"), UDeepSonarRules::ToneVolume(0.f, T), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSonarPanelsTest, "Deep.UI.Sonar.Panels", DEEP_SONAR_TEST_FLAGS)
bool FDeepSonarPanelsTest::RunTest(const FString& Parameters)
{
	FDeepHudModeInputs In;
	In.bHasDiver = true;
	In.RunState = 4;
	In.bSeated = true;
	TestTrue(TEXT("Seated is the helm"), DeepHudModel::ChooseMode(In) == EDeepHudMode::Helm);
	TestFalse(TEXT("Sonar off: today's helm"), (DeepHudModel::PanelsFor(EDeepHudMode::Helm, In) & EDeepHudPanel::Sonar) != 0);
	In.bSonar = true;
	const uint32 Helm = DeepHudModel::PanelsFor(EDeepHudMode::Helm, In);
	TestTrue(TEXT("Sonar on: the helm shows it"), (Helm & EDeepHudPanel::Sonar) != 0);
	TestTrue(TEXT("Next to the helm gauge"), (Helm & EDeepHudPanel::HelmGauge) != 0);
	for (const EDeepHudMode Mode : { EDeepHudMode::Title, EDeepHudMode::Aboard, EDeepHudMode::Water, EDeepHudMode::Cut, EDeepHudMode::Dead,
		EDeepHudMode::Settlement, EDeepHudMode::Review, EDeepHudMode::Pause, EDeepHudMode::Settings, EDeepHudMode::Credits, EDeepHudMode::ConfirmQuit })
	{
		TestFalse(FString::Printf(TEXT("%s has no sonar"), DeepHudModel::ModeName(Mode)), (DeepHudModel::PanelsFor(Mode, In) & EDeepHudPanel::Sonar) != 0);
	}
	TestTrue(TEXT("The sonar bit is inside the panel count"), static_cast<uint32>(EDeepHudPanel::Sonar) < (1u << EDeepHudPanel::Count));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSonarLayoutTest, "Deep.UI.Sonar.Layout", DEEP_SONAR_TEST_FLAGS)
bool FDeepSonarLayoutTest::RunTest(const FString& Parameters)
{
	// 1280x720 and 1920x1080 both lay out at 1920x1080 under the DPI curve (shortest side, 1.0 at 1080).
	const FVector2f L(1920.f, 1080.f);
	const FDeepHudRect Sonar = DeepHudLayout::Sonar(L);
	const FDeepHudRect Gauge = DeepHudLayout::HelmGauge(L);
	const FDeepHudRect Hull = DeepHudLayout::HullSlot(L);
	const FDeepHudRect Hint = DeepHudLayout::ControlsHint(L, 3);
	const FDeepHudRect Subtitle = DeepHudLayout::SubtitleMax(L, 3);
	TestTrue(TEXT("The sonar is on screen"), Sonar.Inside(L.X, L.Y));
	TestTrue(TEXT("The hull slot is on screen"), Hull.Inside(L.X, L.Y));
	TestFalse(TEXT("Sonar clear of the helm gauge"), Sonar.Overlaps(Gauge));
	TestFalse(TEXT("Sonar clear of the controls hint"), Sonar.Overlaps(Hint));
	TestFalse(TEXT("Sonar clear of the widest subtitle"), Sonar.Overlaps(Subtitle));
	TestFalse(TEXT("Sonar clear of the hull slot"), Sonar.Overlaps(Hull));
	TestFalse(TEXT("Hull slot clear of the hint"), Hull.Overlaps(Hint));
	TestFalse(TEXT("Hull slot clear of the gauge"), Hull.Overlaps(Gauge));
	TestFalse(TEXT("Hull slot clear of the subtitle"), Hull.Overlaps(Subtitle));

	FVector2f C;
	float R = 0.f;
	DeepHudLayout::SonarScope(Sonar, C, R);
	TestTrue(TEXT("The scope is a readable size"), R >= 150.f);
	TestTrue(TEXT("The scope sits inside its panel"), C.X - R > Sonar.X && C.X + R < Sonar.Right() && C.Y - R > Sonar.Y && C.Y + R < Sonar.Bottom() - 64.f);
	return true;
}

// ------------------------------------------------------------------------------------------------ world

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSonarRangesTest, "Deep.World.Sonar.Ranges", DEEP_SONAR_TEST_FLAGS)
bool FDeepSonarRangesTest::RunTest(const FString& Parameters)
{
	DeepSonarTest::FSonarWorld W;
	// Walls at the sub's depth: ahead at 20 m, starboard at 35 m, astern at 50 m, nothing to port.
	W.Block(FVector(2050, 0, 0), FVector(50, 600, 400));
	W.Block(FVector(0, 3550, 0), FVector(600, 50, 400));
	W.Block(FVector(-5050, 0, 0), FVector(50, 600, 400));
	// Only on the dim slice (20 m down): a rock 10 m to port.
	W.Block(FVector(0, -1050, -2000), FVector(600, 50, 400));
	// The bottom 29.5 m under the sub: further than the slice, so the slice stays at 20 m.
	W.Block(FVector(0, 0, -3000), FVector(9000, 9000, 50));
	// A piece of salvage in front of the bow wall must not hide it (salvage blocks Visibility for grabbing).
	ADeepSalvage* Salvage = W.World->SpawnActor<ADeepSalvage>(FVector(1000, 0, 0), FRotator::ZeroRotator);

	W.Sonar->SweepNow();
	const float Tol = 0.35f;
	TestEqual(TEXT("Bottom under the sub"), W.Sonar->GetBedBelowM(), 29.5f, Tol);
	TestEqual(TEXT("The slice stays at its depth over a far bottom"), W.Sonar->GetLowerBelowM(), DeepSonarTest::Tune().SliceBelowM, 0.01f);
	TestEqual(TEXT("Ahead at 20 m (through the salvage)"), W.Range(0, 0.f), 20.f, Tol);
	TestEqual(TEXT("Starboard at 35 m"), W.Range(0, 90.f), 35.f, Tol);
	TestEqual(TEXT("Astern at 50 m"), W.Range(0, 180.f), 50.f, Tol);
	TestTrue(TEXT("Nothing to port at depth"), W.Range(0, 270.f) < 0.f);
	TestEqual(TEXT("The rock to port on the slice below"), W.Range(1, 270.f), 10.f, Tol);
	TestTrue(TEXT("Nothing ahead on the slice below"), W.Range(1, 0.f) < 0.f);
	TestTrue(TEXT("Open water off the bow quarter"), W.Range(0, 45.f) < 0.f);
	TestEqual(TEXT("Nearest at depth is the bow wall"), W.Sonar->GetNearestM(), 20.f, Tol);

	// A bottom nearer than the slice lifts it to just above the bottom.
	W.Block(FVector(0, 0, -1050), FVector(400, 400, 50));
	W.Sonar->SweepNow();
	TestEqual(TEXT("A near bottom"), W.Sonar->GetBedBelowM(), 10.f, Tol);
	TestEqual(TEXT("The slice rides above it"), W.Sonar->GetLowerBelowM(), 10.f - DeepSonarTest::Tune().BedClearanceM, Tol);

	// The frame for the HUD carries the same picture, every fresh return at full phosphor less its range falloff.
	FDeepHudSonar Hud;
	W.Sonar->FillHud(Hud);
	TestEqual(TEXT("A return per beam"), Hud.Upper.Num(), W.Sonar->GetSlice(0).Num());
	const FDeepHudSonarReturn& Bow = Hud.Upper[0];
	TestEqual(TEXT("Bow return in the frame"), Bow.RangeM, 20.f, Tol);
	TestEqual(TEXT("Fresh return strength"), Bow.Strength, UDeepSonarRules::ReturnStrength(0.f, Bow.RangeM, DeepSonarTest::Tune()), 1e-3f);
	if (Salvage)
	{
		Salvage->Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSonarBlipsTest, "Deep.World.Sonar.Blips", DEEP_SONAR_TEST_FLAGS)
bool FDeepSonarBlipsTest::RunTest(const FString& Parameters)
{
	DeepSonarTest::FSonarWorld W;
	const FDeepSonarTuning& T = DeepSonarTest::Tune();
	ADeepSalvage* Scrap = W.World->SpawnActor<ADeepSalvage>(FVector(0, 1500, 0), FRotator::ZeroRotator);
	ADeepSalvage* Relic = W.World->SpawnActor<ADeepSalvage>(FVector(-2500, 0, -800), FRotator::ZeroRotator);
	FDeepSalvageKind RelicKind;
	if (Relic && UDeepTuningSettings::Get().FindSalvageKind(EDeepSalvageSize::Relic, RelicKind))
	{
		Relic->SetKind(RelicKind);
	}
	ADeepSalvage* Far = W.World->SpawnActor<ADeepSalvage>(FVector((T.RangeM + 20.f) * 100.f, 0, 0), FRotator::ZeroRotator);
	ADeepSalvage* Hidden = W.World->SpawnActor<ADeepSalvage>(FVector(1000, 1000, 0), FRotator::ZeroRotator);
	if (Hidden)
	{
		Hidden->SetActorHiddenInGame(true);
	}
	// The serpent has no collision: the sonar reads its head's position. Down and to port astern.
	ADeepSerpent* Serpent = W.World->SpawnActor<ADeepSerpent>(FVector(-2000, -2000, -1000), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("Salvage spawned"), Scrap) || !TestNotNull(TEXT("Relic spawned"), Relic) || !TestNotNull(TEXT("Serpent spawned"), Serpent))
	{
		return false;
	}

	UDeepEventBus* Bus = UDeepEventBus::Get(W.World);
	float CalledM = -1.f;
	FDelegateHandle Listen;
	if (Bus)
	{
		Listen = Bus->OnEvent.AddLambda([&CalledM](FName Event, const FDeepEventPayload& Payload)
		{
			CalledM = Event == FName(DeepSonarEvent::Contact) ? Payload.DepthM : CalledM;
		});
	}
	W.Sonar->SweepNow();
	const TArray<FDeepSonarContactSample>& Contacts = W.Sonar->GetContacts();
	auto Find = [&Contacts](const AActor* Actor) { return Contacts.FindByPredicate([Actor](const FDeepSonarContactSample& C) { return C.Actor.Get() == Actor; }); };
	TestEqual(TEXT("Three contacts: scrap, relic, serpent"), Contacts.Num(), 3);
	if (const FDeepSonarContactSample* C = Find(Scrap))
	{
		TestTrue(TEXT("Scrap is salvage"), C->Kind == EDeepSonarContact::Salvage);
		TestEqual(TEXT("Scrap to starboard"), C->BearingDeg, 90.f, 0.5f);
		TestEqual(TEXT("Scrap at 15 m"), C->RangeM, 15.f, 0.05f);
	}
	else
	{
		AddError(TEXT("The scrap is not on the sonar"));
	}
	if (const FDeepSonarContactSample* C = Find(Relic))
	{
		TestTrue(TEXT("The relic is a relic"), C->Kind == (RelicKind.Size == EDeepSalvageSize::Relic ? EDeepSonarContact::Relic : EDeepSonarContact::Salvage));
		TestEqual(TEXT("Relic astern"), C->BearingDeg, 180.f, 0.5f);
		TestEqual(TEXT("Relic 8 m below"), C->DzM, -8.f, 0.05f);
	}
	else
	{
		AddError(TEXT("The relic is not on the sonar"));
	}
	if (const FDeepSonarContactSample* C = Find(Serpent))
	{
		const FVector Head = Serpent->GetHeadLocation();
		TestTrue(TEXT("The serpent is the serpent"), C->Kind == EDeepSonarContact::Serpent);
		TestEqual(TEXT("Serpent bearing from its head"), C->BearingDeg, UDeepSonarRules::BearingDeg(FVector::ForwardVector, Head), 0.5f);
		TestEqual(TEXT("Serpent range from its head"), C->RangeM, UDeepSonarRules::PlanRangeM(Head), 0.05f);
	}
	else
	{
		AddError(TEXT("The serpent is not on the sonar"));
	}
	TestNull(TEXT("Out of range: not shown"), Find(Far));
	TestNull(TEXT("Hidden (below the loaded stop): not shown"), Find(Hidden));

	FDeepHudSonar Hud;
	W.Sonar->FillHud(Hud);
	TestEqual(TEXT("A blip per contact"), Hud.Blips.Num(), 3);
	int32 Serpents = 0;
	for (const FDeepHudSonarBlip& B : Hud.Blips)
	{
		Serpents += B.Kind == 2 ? 1 : 0;
		TestEqual(TEXT("A fresh blip is full"), B.Strength, 1.f, 1e-3f);
	}
	TestEqual(TEXT("One serpent blip"), Serpents, 1);
	TestEqual(TEXT("One sonar_contact for the serpent"), W.Sonar->GetContactCalls(), 1);
	TestEqual(TEXT("On the bus"), Bus ? Bus->CountOf(DeepSonarEvent::Contact) : -1, 1);
	TestEqual(TEXT("The call carries the serpent's range"), CalledM, UDeepSonarRules::PlanRangeM(Serpent->GetHeadLocation()), 0.05f);
	W.Sonar->SweepNow();
	TestEqual(TEXT("No second call at the same stop"), W.Sonar->GetContactCalls(), 1);

	// A contact that leaves fades out and drops once the sweep has passed without it.
	Scrap->Destroy();
	for (int32 I = 0; I < 30; ++I)
	{
		W.Sonar->StepSonar(0.1f);
	}
	TestNull(TEXT("A destroyed contact drops"), Find(Scrap));
	TestEqual(TEXT("Still one call while the serpent stays"), W.Sonar->GetContactCalls(), 1);
	if (Bus)
	{
		Bus->OnEvent.Remove(Listen);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSonarSweepTest, "Deep.World.Sonar.Sweep", DEEP_SONAR_TEST_FLAGS)
bool FDeepSonarSweepTest::RunTest(const FString& Parameters)
{
	DeepSonarTest::FSonarWorld W;
	const FDeepSonarTuning& T = DeepSonarTest::Tune();
	// A hull 8 x 4 m (half sizes 4 and 2); a wall 5 m off the bow is 1 m from the hull: inside the full tone.
	W.Sonar->SetTestFrame(FVector::ZeroVector, FVector::ForwardVector, FVector2D(4.0, 2.0));
	W.Block(FVector(550, 0, 0), FVector(50, 800, 400));

	// The first manned frame takes a whole picture and pings.
	W.Sonar->StepSonar(1.f / 60.f);
	TestEqual(TEXT("A fresh helm pings"), W.Sonar->GetPings(), 1);
	TestEqual(TEXT("Wall off the bow"), W.Sonar->GetNearestM(), 5.f, 0.35f);
	TestEqual(TEXT("1 m from the hull"), W.Sonar->GetClearanceM(), 1.f, 0.35f);
	TestEqual(TEXT("Full proximity"), W.Sonar->GetProximity01(), 1.f, 1e-3f);

	// One ping period at 60 fps (a sweep in the jam style, PingPeriodS in the soft one): within the trace budget every
	// frame, one more ping, the tone beeping fast.
	int32 MostTraces = 0;
	const int32 Frames = FMath::CeilToInt(UDeepSonarRules::PingPeriodFor(UDeepCameraSubsystem::GetSonarStyle(), T) * 60.f) + 2;
	for (int32 I = 0; I < Frames; ++I)
	{
		W.Sonar->StepSonar(1.f / 60.f);
		MostTraces = FMath::Max(MostTraces, W.Sonar->GetTracesLastStep());
	}
	TestTrue(FString::Printf(TEXT("Traces a frame within the budget (%d, budget %d plus the bottom probe)"), MostTraces, T.MaxTracesPerFrame), MostTraces <= T.MaxTracesPerFrame + 1);
	TestEqual(TEXT("One ping a period"), W.Sonar->GetPings(), 2);
	// A beep lands on a frame, so at 60 fps each interval rounds up to whole frames (this matters over a 6 s period).
	const int32 ExpectBeeps = Frames / FMath::Max(1, FMath::CeilToInt(T.ToneFastS * 60.f - 1e-3f));
	TestTrue(FString::Printf(TEXT("The tone beeps fast (%d beeps, about %d expected)"), W.Sonar->GetBeeps(), ExpectBeeps), W.Sonar->GetBeeps() >= ExpectBeeps - 2);
	bool bAllFresh = true;
	for (const FDeepSonarSample& S : W.Sonar->GetSlice(0))
	{
		bAllFresh &= W.Sonar->GetClock() - S.At < T.SweepPeriodS + 0.1;
	}
	TestTrue(TEXT("Every beam was traced again within the turn"), bAllFresh);

	// Off: no traces, nothing changes.
	W.Sonar->SetSonarEnabled(false);
	TestFalse(TEXT("Off is not working"), W.Sonar->IsWorking());
	TestEqual(TEXT("Off has no proximity"), W.Sonar->GetProximity01(), 0.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
