// D1b: the pilot's chase framing (Sim/DeepHelmViewRules: FrameY, FramePitch) and the hull's own work lamps
// (Sim/DeepHullLightRules: probes, Mount, InCone), as pure rules. Deep.Sim.HelmView.*, Deep.Sim.HullLights.*.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Sim/DeepHelmLightRules.h"
#include "Sim/DeepHelmViewRules.h"
#include "Sim/DeepHullLightRules.h"
#include "Sim/DeepTuningTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_HELMVIEW_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHelmViewFrameYTest, "Deep.Sim.HelmView.FrameY", DEEP_HELMVIEW_TEST_FLAGS)
bool FDeepHelmViewFrameYTest::RunTest(const FString& Parameters)
{
	// 90 deg wide at 16:9: about 58.7 deg tall.
	const float VFov = DeepHelmView::VerticalFovDeg(90.f, 16.f / 9.f);
	TestEqual(TEXT("vertical field of view at 16:9"), VFov, 58.72f, 0.05f);
	const FVector Cam(0.0, 0.0, 0.0);
	TestEqual(TEXT("straight ahead is the centre"), DeepHelmView::FrameY(Cam, FRotator::ZeroRotator, FVector(1000.0, 0.0, 0.0), 90.f, 16.f / 9.f), 0.f, 0.001f);
	// At the bottom edge: half the vertical field under the axis.
	const double Down = FMath::Tan(FMath::DegreesToRadians(VFov * 0.5)) * 1000.0;
	TestEqual(TEXT("the bottom edge is +1"), DeepHelmView::FrameY(Cam, FRotator::ZeroRotator, FVector(1000.0, 0.0, -Down), 90.f, 16.f / 9.f), 1.f, 0.001f);
	TestEqual(TEXT("the top edge is -1"), DeepHelmView::FrameY(Cam, FRotator::ZeroRotator, FVector(1000.0, 0.0, Down), 90.f, 16.f / 9.f), -1.f, 0.001f);
	TestTrue(TEXT("behind the camera is off the frame"), DeepHelmView::FrameY(Cam, FRotator::ZeroRotator, FVector(-1000.0, 0.0, 0.0), 90.f, 16.f / 9.f) > 1.f);
	// Pitching the view up moves a point down the frame.
	TestTrue(TEXT("pitch up, the point sinks"), DeepHelmView::FrameY(Cam, FRotator(10.f, 0.f, 0.f), FVector(1000.0, 0.0, 0.0), 90.f, 16.f / 9.f) > 0.3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHelmViewFramePitchTest, "Deep.Sim.HelmView.FramePitch", DEEP_HELMVIEW_TEST_FLAGS)
bool FDeepHelmViewFramePitchTest::RunTest(const FString& Parameters)
{
	FDeepHelmViewTuning T;
	T.HullTopFrameFrac = 0.3f;
	T.FrameAspect = 16.f / 9.f;
	const float Fov = 85.f;
	// The camera 12 m astern and 1 m over the hull's top: a look ahead pitched -15 would put the hull mid frame.
	const FVector Cam(-1200.0, 0.0, 100.0);
	const FVector Top(0.0, 0.0, 0.0);
	const float Pitch = DeepHelmView::FramePitch(Cam, Top, -15.f, Fov, T);
	const float Y = DeepHelmView::FrameY(Cam, FRotator(Pitch, 0.f, 0.f), Top, Fov, T.FrameAspect);
	AddInfo(FString::Printf(TEXT("pitch %.2f, hull top at %.3f of the half frame under the centre"), Pitch, Y));
	TestTrue(TEXT("the view pitches up from the look"), Pitch > -15.f);
	TestEqual(TEXT("the hull's top sits at the share asked for"), Y, T.HullTopFrameFrac, 0.01f);
	// A look already high enough is kept (never pitched down for the hull).
	TestEqual(TEXT("a higher look is kept"), DeepHelmView::FramePitch(Cam, Top, 5.f, Fov, T), 5.f, 0.001f);
	// The camera well over the hull: the look ahead already has the hull low.
	const FVector High(-1200.0, 0.0, 600.0);
	const float Look = -10.f;
	TestEqual(TEXT("high over it, the look ahead stands"), DeepHelmView::FramePitch(High, Top, Look, Fov, T), Look, 0.001f);
	TestTrue(TEXT("and the hull is in the lower part"), DeepHelmView::FrameY(High, FRotator(Look, 0.f, 0.f), Top, Fov, T.FrameAspect) >= T.HullTopFrameFrac);
	// D1c: a look up never drops the hull's top under HullTopMaxFrameFrac: the hull stays in the frame.
	const float Up = DeepHelmView::FramePitch(Cam, Top, 24.f, Fov, T);
	TestTrue(TEXT("looking up, the hull's top stays in the frame"), DeepHelmView::FrameY(Cam, FRotator(Up, 0.f, 0.f), Top, Fov, T.FrameAspect) <= T.HullTopMaxFrameFrac + 0.01f);
	TestTrue(TEXT("but the view still looks up"), Up > Pitch);
	// Never past the look's limits.
	T.LookMaxPitchDeg = 10.f;
	TestTrue(TEXT("inside the pitch limits"), DeepHelmView::FramePitch(FVector(-100.0, 0.0, -500.0), Top, -15.f, Fov, T) <= 10.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHelmViewHeaveTest, "Deep.Sim.HelmView.Heave", DEEP_HELMVIEW_TEST_FLAGS)
bool FDeepHelmViewHeaveTest::RunTest(const FString& Parameters)
{
	// D1c: heaving, the chase view shows where the hull is going. Sinking straight down it climbs over the hull, a little
	// astern, and looks steeply down past the hull at the floor; rising it drops behind and looks up the way out.
	FDeepHelmViewTuning T;
	T.FrameAspect = 16.f / 9.f;
	const float Fov = 85.f;
	TestEqual(TEXT("level travel, no heave"), DeepHelmView::HeaveShare(0.f, T), 0.f, 0.001f);
	TestEqual(TEXT("straight down, full sink"), DeepHelmView::HeaveShare(-90.f, T), -1.f, 0.001f);
	TestEqual(TEXT("straight up, full rise"), DeepHelmView::HeaveShare(90.f, T), 1.f, 0.001f);
	TestEqual(TEXT("half the full pitch, half"), DeepHelmView::HeaveShare(-0.5f * T.HeaveFullDeg, T), -0.5f, 0.001f);

	const FDeepHelmViewTuning Level = DeepHelmView::Heaved(T, 0.f);
	TestEqual(TEXT("no heave keeps the level place"), Level.ChaseBackM, T.ChaseBackM, 0.001f);
	TestEqual(TEXT("no heave keeps the level height"), Level.ChaseUpM, T.ChaseUpM, 0.001f);
	const FDeepHelmViewTuning Sink = DeepHelmView::Heaved(T, -1.f);
	TestEqual(TEXT("sinking, the view comes in over the hull"), Sink.ChaseBackM, T.SinkBackM, 0.001f);
	TestEqual(TEXT("sinking, the view climbs"), Sink.ChaseUpM, T.SinkUpM, 0.001f);
	TestTrue(TEXT("sinking, the look may pitch down steeply"), Sink.LookMinPitchDeg <= T.SinkMinPitchDeg + 0.001f);
	const FDeepHelmViewTuning Half = DeepHelmView::Heaved(T, -0.5f);
	TestTrue(TEXT("half a sink, half way"), Half.ChaseUpM > T.ChaseUpM && Half.ChaseUpM < T.SinkUpM);
	const FDeepHelmViewTuning Rise = DeepHelmView::Heaved(T, 1.f);
	TestEqual(TEXT("rising, the view drops"), Rise.ChaseUpM, T.RiseUpM, 0.001f);
	TestTrue(TEXT("rising, the look may pitch up"), Rise.LookMaxPitchDeg >= T.RiseMaxPitchDeg - 0.001f);

	// Sinking straight down in a sink: the camera over the hull, SinkBackM astern of its centre and SinkUpM over it. The hull's
	// top is 2.5 m over the centre. The look along the travel (straight down past the hull) is not pulled back up to frame
	// the hull low: the frame's centre goes down past the hull, and the hull still shows.
	const FVector HullTop(0.0, 0.0, 250.0);
	const FVector SinkCam(-T.SinkBackM * 100.0, 0.0, T.SinkUpM * 100.0);
	const FRotator SinkLook = DeepHelmView::ChaseLook(SinkCam, FVector::ZeroVector, FVector(0.0, 0.0, -1.0), Sink);
	const float SinkPitch = DeepHelmView::FramePitch(SinkCam, HullTop, SinkLook.Pitch, Fov, Sink);
	const float TopY = DeepHelmView::FrameY(SinkCam, FRotator(SinkPitch, 0.f, 0.f), HullTop, Fov, T.FrameAspect);
	AddInfo(FString::Printf(TEXT("sinking: look %.1f, framed %.1f, hull top at %.2f"), SinkLook.Pitch, SinkPitch, TopY));
	TestTrue(TEXT("sinking, the view looks steeply down (60 deg or more)"), SinkPitch <= -60.f);
	TestTrue(TEXT("sinking, the hull is still in the frame"), TopY > -1.f && TopY < 1.f);
	// The floor 20 m under the hull, beside it (a hull's half width plus 3 m to starboard): in the frame.
	const FVector Floor(0.0, 165.0 + 300.0, -2000.0);
	const FRotator SinkView(SinkPitch, 0.f, 0.f);
	const FVector InView = SinkView.UnrotateVector(Floor - SinkCam);
	const double TanHalfH = FMath::Tan(FMath::DegreesToRadians(Fov * 0.5));
	TestTrue(TEXT("sinking, the floor beside the hull is in the frame"), InView.X > 0.0 && FMath::Abs(InView.Y / InView.X) <= TanHalfH
		&& FMath::Abs(DeepHelmView::FrameY(SinkCam, SinkView, Floor, Fov, T.FrameAspect)) <= 1.f);
	// Level, from the same place, the view would not look down that far.
	TestTrue(TEXT("level, the look stays near level"), DeepHelmView::FramePitch(SinkCam, HullTop, -20.f, Fov, T) > SinkPitch + 20.f);

	// Rising: the camera behind and level with the hull's top; the look up the way out, the hull at the frame's foot.
	const FVector RiseCam(-T.RiseBackM * 100.0, 0.0, T.RiseUpM * 100.0);
	const FRotator RiseLook = DeepHelmView::ChaseLook(RiseCam, FVector::ZeroVector, FVector(0.0, 0.0, 1.0), Rise);
	const float RisePitch = DeepHelmView::FramePitch(RiseCam, HullTop, RiseLook.Pitch, Fov, Rise);
	const float RiseTopY = DeepHelmView::FrameY(RiseCam, FRotator(RisePitch, 0.f, 0.f), HullTop, Fov, T.FrameAspect);
	AddInfo(FString::Printf(TEXT("rising: look %.1f, framed %.1f, hull top at %.2f"), RiseLook.Pitch, RisePitch, RiseTopY));
	TestTrue(TEXT("rising, the view looks up (15 deg or more)"), RisePitch >= 15.f);
	TestTrue(TEXT("rising, the hull stays in the frame"), RiseTopY <= T.HullTopMaxFrameFrac + 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHelmLightNearTest, "Deep.Sim.HelmLight.FillCap", DEEP_HELMVIEW_TEST_FLAGS)
bool FDeepHelmLightNearTest::RunTest(const FString& Parameters)
{
	// D1c: in the cockpit view the fill (by the camera) dims for the nearest rock so a small chamber's walls get less light
	// than a cavern's: the floods shape a small room, the fill does not wash it flat. The light on that rock is share /
	// distance squared. The chase view keeps D1b's cap (power 2: the light on the nearest rock stays the same).
	FDeepLookTuning Look;
	const float Full = Look.HelmFillNearFullM;
	const float Power = Look.HelmFillCockpitNearPower;
	const auto OnRock = [&](float M) { return DeepHelmLight::NearShare(M, Full, Power, Look.HelmFillNearMin) / (M * M); };
	TestTrue(TEXT("the cockpit caps harder than the chase view"), Power > Look.HelmFillNearPower);
	TestEqual(TEXT("full at the far reach"), DeepHelmLight::NearShare(Full, Full, Power, Look.HelmFillNearMin), 1.f, 0.001f);
	TestEqual(TEXT("full beyond it"), DeepHelmLight::NearShare(Full * 2.f, Full, Power, Look.HelmFillNearMin), 1.f, 0.001f);
	TestTrue(TEXT("the light on rock is capped near walls: no brighter than on a cavern's wall"), OnRock(10.f) <= OnRock(Full) + 1e-6f);
	TestTrue(TEXT("a small chamber's walls (6 m) get well under a cavern's light (60%)"), OnRock(6.f) < 0.6f * OnRock(Full));
	TestTrue(TEXT("nearer, less"), OnRock(4.f) < OnRock(8.f));
	TestEqual(TEXT("never under the floor"), DeepHelmLight::NearShare(0.f, Full, Power, Look.HelmFillNearMin), Look.HelmFillNearMin, 1e-6f);
	TestTrue(TEXT("the cockpit's fill is graded from its axis, the chase view's has D1b's even core"),
		Look.HelmFillCockpitInnerFrac < Look.HelmFillInnerFrac && Look.HelmFillCockpitSpecular < Look.HelmFillSpecular);
	// Power 2 is the old rule: the light on the nearest rock stays the same.
	TestEqual(TEXT("power 2 holds the light on rock"), DeepHelmLight::NearShare(10.f, 20.f, 2.f, 0.f) / 100.f, 1.f / 400.f, 1e-6f);
	// Sinking, a throw is lifted for a far floor (the light on it as if at the full distance), up to the boost.
	TestEqual(TEXT("no boost: far rock is full, not more"), DeepHelmLight::NearShare(36.f, 18.f, 2.f, 0.f), 1.f, 1e-6f);
	TestEqual(TEXT("a boost lifts a lamp for far rock"), DeepHelmLight::NearShare(27.f, 18.f, 2.f, 0.f, 3.f), 2.25f, 1e-4f);
	TestEqual(TEXT("up to the boost"), DeepHelmLight::NearShare(60.f, 18.f, 2.f, 0.f, 3.f), 3.f, 1e-6f);
	TestEqual(TEXT("a boost leaves near rock dimmed"), DeepHelmLight::NearShare(9.f, 18.f, 2.f, 0.f, 3.f), 0.25f, 1e-6f);

	// Heaving, the lamps tip toward the way the hull goes: down sinking, up rising, by the replicated heave (+ down).
	TestEqual(TEXT("sinking at full heave is -1"), DeepHelmLight::HeaveFromSpeed(2.5f, 2.5f), -1.f, 0.001f);
	TestEqual(TEXT("rising at half is +0.5"), DeepHelmLight::HeaveFromSpeed(-1.25f, 2.5f), 0.5f, 0.001f);
	TestEqual(TEXT("level, the flood stays level"), DeepHelmLight::HeavePitchDeg(Look.HelmFloodPitchDeg, 0.f, Look.HelmFloodSinkPitchDeg, Look.HelmFloodRisePitchDeg),
		Look.HelmFloodPitchDeg, 0.001f);
	TestEqual(TEXT("sinking, the flood aims down the sink"), DeepHelmLight::HeavePitchDeg(Look.HelmFloodPitchDeg, -1.f, Look.HelmFloodSinkPitchDeg, Look.HelmFloodRisePitchDeg),
		Look.HelmFloodSinkPitchDeg, 0.001f);
	TestTrue(TEXT("the sink's pitch is well down"), Look.HelmFloodSinkPitchDeg <= -30.f && Look.HelmFillSinkPitchDeg <= -30.f);
	TestTrue(TEXT("rising, the flood aims up"), DeepHelmLight::HeavePitchDeg(Look.HelmFloodPitchDeg, 0.5f, Look.HelmFloodSinkPitchDeg, Look.HelmFloodRisePitchDeg) > 10.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHelmLightSnowTest, "Deep.Sim.HelmLight.Snow", DEEP_HELMVIEW_TEST_FLAGS)
bool FDeepHelmLightSnowTest::RunTest(const FString& Parameters)
{
	// D1c: from the helm the marine snow reads as specks the floods light, never as dark streaks: no streak in the chase
	// view, only specks a light reaches, and far specks never under about a pixel.
	FDeepLookTuning Look;
	Look.bHelmStreakInChase = false;
	TestEqual(TEXT("no streak in the chase view, however fast"), DeepHelmLight::SnowStreakCm(20.f, false, Look), 0.f, 0.001f);
	TestEqual(TEXT("the cockpit streaks by the real speed"), DeepHelmLight::SnowStreakCm(4.f, true, Look), 4.f * Look.HelmStreakSeconds * 100.f, 0.01f);
	TestTrue(TEXT("a cockpit streak at full ahead is short (under 15 cm)"), DeepHelmLight::SnowStreakCm(4.f, true, Look) < 15.f);

	// A flood at the origin aiming +X, a 22 deg half cone, reaching where 250000 / d^2 falls to HelmSnowLitMin (capped at 55 m).
	DeepHelmLight::FSnowLight Flood;
	Flood.Dir = FVector::ForwardVector;
	Flood.CosOuter = FMath::Cos(FMath::DegreesToRadians(22.f));
	Flood.ReachCm = DeepHelmLight::SnowReachCm(250000.f, 5500.f, Look.HelmSnowLitMin);
	TestEqual(TEXT("a strong flood reaches its range"), Flood.ReachCm, 5500.f, 0.5f);
	TestTrue(TEXT("a dimmed lamp reaches less"), DeepHelmLight::SnowReachCm(2500.f, 5500.f, Look.HelmSnowLitMin) < 1000.f);
	const TArray<DeepHelmLight::FSnowLight> Lights = { Flood };
	TestTrue(TEXT("a speck in the beam is lit"), DeepHelmLight::SnowLit(FVector(1000.0, 100.0, 0.0), Lights));
	TestFalse(TEXT("a speck beside the beam is not"), DeepHelmLight::SnowLit(FVector(1000.0, 800.0, 0.0), Lights));
	TestFalse(TEXT("nor one behind the lamp"), DeepHelmLight::SnowLit(FVector(-1000.0, 0.0, 0.0), Lights));
	TestFalse(TEXT("nor one past its reach"), DeepHelmLight::SnowLit(FVector(6000.0, 0.0, 0.0), Lights));
	// An all-round lamp (a point light) lights every speck within its reach.
	DeepHelmLight::FSnowLight Point;
	Point.AtCm = FVector(0.0, 0.0, 500.0);
	Point.ReachCm = 300.f;
	const TArray<DeepHelmLight::FSnowLight> Both = { Flood, Point };
	TestTrue(TEXT("a point lamp lights round it"), DeepHelmLight::SnowLit(FVector(0.0, 0.0, 300.0), Both));
	// A wide cone (over 90 deg) lights a little behind its face.
	DeepHelmLight::FSnowLight Wide = Flood;
	Wide.CosOuter = FMath::Cos(FMath::DegreesToRadians(110.f));
	const TArray<DeepHelmLight::FSnowLight> WideOnly = { Wide };
	TestTrue(TEXT("a wide cone lights just behind its face"), DeepHelmLight::SnowLit(FVector(-100.0, 1000.0, 0.0), WideOnly));
	TestFalse(TEXT("but not straight behind"), DeepHelmLight::SnowLit(FVector(-1000.0, 0.0, 0.0), WideOnly));

	// Sizes: near, the speck's own (times the helm's scale); far, never under the minimum angle.
	TestEqual(TEXT("a near speck keeps its size"), DeepHelmLight::SnowSizeCm(0.4f, 100.f, Look), 0.4f * Look.HelmSnowSizeScale, 0.001f);
	TestEqual(TEXT("a far speck is about a pixel"), DeepHelmLight::SnowSizeCm(0.4f, 2000.f, Look), 2000.f * Look.HelmSnowMinAngle, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHullLightsProbeTest, "Deep.Sim.HullLights.Probes", DEEP_HELMVIEW_TEST_FLAGS)
bool FDeepHullLightsProbeTest::RunTest(const FString& Parameters)
{
	const FDeepHullLightTuning T;
	const FBox Box(FVector(-600.0, -150.0, -200.0), FVector(600.0, 150.0, 250.0));
	const TArray<DeepHullLights::FProbe> Tops = DeepHullLights::TowerTopProbes(Box, 9);
	TestEqual(TEXT("as many top probes as asked"), Tops.Num(), 9);
	bool bDown = true;
	for (const DeepHullLights::FProbe& Probe : Tops)
	{
		bDown &= Probe.FromCm.Z > Box.Max.Z && Probe.ToCm.Z <= Box.Min.Z && FMath::IsNearlyEqual(Probe.FromCm.X, Probe.ToCm.X);
	}
	TestTrue(TEXT("the top probes run straight down through the box from over it"), bDown);
	const FVector TowerTop(100.0, 0.0, 250.0);
	const DeepHullLights::FProbe Back = DeepHullLights::TowerBackProbe(Box, TowerTop, T);
	TestTrue(TEXT("the tower's back is probed forward from astern"), Back.FromCm.X < Box.Min.X && Back.ToCm.X == TowerTop.X);
	TestEqual(TEXT("under the tower's top"), Back.FromCm.Z, TowerTop.Z - T.TowerLampBelowTopCm, 0.01);
	const DeepHullLights::FProbe Port = DeepHullLights::SternProbe(Box, -1, T);
	const DeepHullLights::FProbe Stbd = DeepHullLights::SternProbe(Box, 1, T);
	TestTrue(TEXT("the stern lamps' probes run straight down onto the after deck"),
		Port.FromCm.Z > Box.Max.Z && Port.ToCm.Z <= Box.Min.Z && FMath::IsNearlyEqual(Port.FromCm.X, Port.ToCm.X) && Port.FromCm.X < Box.GetCenter().X);
	TestTrue(TEXT("port to port, starboard to starboard"), Port.FromCm.Y < 0.0 && Stbd.FromCm.Y > 0.0);
	TestEqual(TEXT("mirrored"), Port.FromCm.Y, -Stbd.FromCm.Y, 0.01);
	const DeepHullLights::FProbe Deck = DeepHullLights::DeckProbe(Box, T);
	TestTrue(TEXT("the after deck is probed straight down, aft"), Deck.FromCm.Z > Box.Max.Z && Deck.FromCm.X < Box.GetCenter().X);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepHullLightsMountTest, "Deep.Sim.HullLights.Mount", DEEP_HELMVIEW_TEST_FLAGS)
bool FDeepHullLightsMountTest::RunTest(const FString& Parameters)
{
	FDeepHullLightTuning T;
	T.MountOutCm = 8.f;
	// On the tower's aft face (normal astern), aimed at the after deck below and astern of it.
	const FVector Surface(0.0, 0.0, 300.0);
	const FVector Deck(-400.0, 0.0, 150.0);
	const FTransform Lamp = DeepHullLights::Mount(Surface, FVector(-1.0, 0.0, 0.0), Deck, T);
	TestEqual(TEXT("stands off the surface along its normal"), Lamp.GetLocation(), FVector(-8.0, 0.0, 300.0), 0.01);
	const FVector Fwd = Lamp.GetRotation().GetForwardVector();
	TestTrue(TEXT("aims aft and down at the deck"), Fwd.X < 0.0 && Fwd.Z < 0.0);
	TestTrue(TEXT("the deck is in its cone"), DeepHullLights::InCone(Lamp, Deck, 30.f, 1000.f));
	TestFalse(TEXT("past its range is not"), DeepHullLights::InCone(Lamp, Deck, 30.f, 100.f));
	TestFalse(TEXT("the bow is not"), DeepHullLights::InCone(Lamp, FVector(600.0, 0.0, 200.0), 60.f, 2000.f));
	return true;
}

#endif
