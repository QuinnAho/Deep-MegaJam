// D1b: the pilot's chase framing (Sim/DeepHelmViewRules: FrameY, FramePitch) and the hull's own work lamps
// (Sim/DeepHullLightRules: probes, Mount, InCone), as pure rules. Deep.Sim.HelmView.*, Deep.Sim.HullLights.*.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
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
	// Sinking: the way lies below the hull, so the view may look down past it, but the hull's top never rises over the centre.
	const float Sinking = DeepHelmView::FramePitch(Cam, Top, -35.f, Fov, T, -T.HullTopSinkDeg);
	TestTrue(TEXT("sinking, the view looks lower than when level"), Sinking < Pitch);
	TestTrue(TEXT("sinking, the hull's top stays at or under the centre"),
		DeepHelmView::FrameY(Cam, FRotator(Sinking, 0.f, 0.f), Top, Fov, T.FrameAspect) >= -0.001f);
	const float HalfSink = DeepHelmView::FramePitch(Cam, Top, -35.f, Fov, T, -0.5f * T.HullTopSinkDeg);
	TestTrue(TEXT("half as steep, half way"), HalfSink < Pitch && HalfSink > Sinking);
	// Never past the look's limits.
	T.LookMaxPitchDeg = 10.f;
	TestTrue(TEXT("inside the pitch limits"), DeepHelmView::FramePitch(FVector(-100.0, 0.0, -500.0), Top, -15.f, Fov, T) <= 10.f);
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
