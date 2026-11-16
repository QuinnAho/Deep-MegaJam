// Coming home to the hatch from any side (S8-keys playtest, Nov 15): the winch routes round and under the hull, a
// swimmer pushing into the hull's side slides down it and under it, the hull's collision is only its visible cubes,
// and no straight walk between the room's stations crosses the open hatch.

#include "CoreMinimal.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Diver/DeepTetherComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepSubmarine.h"
#include "World/DeepArena.h"
#include "World/DeepSubRoom.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_RETURN_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepReturnTest
{
	// The shipped hull, by hand: 4.7 x 2.45 x 1.7 m half extents; the diver's capsule is 35 x 80 cm.
	const FBox Hull(FVector(-470.f, -245.f, -170.f), FVector(470.f, 245.f, 170.f));
	constexpr float Radius = 35.f;
	constexpr float HalfHeight = 80.f;

	bool Overlaps(const FBox& Solid, const FVector& Centre, double RadiusCm, double HalfHeightCm)
	{
		// The capsule's box, 2 cm shy, against the solid: touching is fine, sinking in is not.
		const FVector Ext(RadiusCm - 2.0, RadiusCm - 2.0, HalfHeightCm - 2.0);
		return Solid.Intersect(FBox(Centre - Ext, Centre + Ext));
	}

	/** A game world with the arena and one diver in the room, dropped through the hatch (tethered). */
	struct FReturnWorld
	{
		UWorld* World = nullptr;
		ADeepSubmarine* Sub = nullptr;
		ADeepDiver* Diver = nullptr;
		UDeepRunComponent* Run = nullptr;

		FReturnWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			ADeepGameMode* GameMode = World->SpawnActor<ADeepGameMode>();
			Sub = GameMode ? GameMode->EnsureArena() : nullptr;
			if (GameMode)
			{
				GameMode->StartPlay();
			}
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

		~FReturnWorld()
		{
			if (World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		bool Valid() const { return Sub && Diver && Run; }

		/** Through the hatch, then put in the water at a sub-local point (cm). */
		bool DiveTo(const FVector& LocalCm) const
		{
			if (Run->GetState() == EDeepRunState::Aboard && !Run->LeaveSub())
			{
				return false;
			}
			Diver->TeleportDiver(Sub->GetActorTransform().TransformPosition(LocalCm));
			Run->StepRun(0.f);
			return Run->GetState() == EDeepRunState::Tethered;
		}

		FVector LocalDiver() const { return Sub->GetActorTransform().InverseTransformPosition(Diver->GetActorLocation()); }
	};

	// Points round the sub (local cm) at different heights: behind at hatch height (the S8 stall), beside low, a low
	// far corner, over the tower, high off the side, hugging the side wall, and off the nose at mid height.
	const FVector Around[] = {
		FVector(-900.f, 0.f, -150.f), FVector(0.f, 900.f, -300.f), FVector(800.f, -700.f, -600.f), FVector(100.f, 0.f, 700.f),
		FVector(-500.f, 700.f, 400.f), FVector(-200.f, -290.f, 0.f), FVector(1100.f, 200.f, 50.f) };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepWinchRouteTest, "Deep.Sim.Rules.WinchRoute", DEEP_RETURN_TEST_FLAGS)
bool FDeepWinchRouteTest::RunTest(const FString& Parameters)
{
	using namespace DeepReturnTest;
	FDeepTetherTuning T;
	T.WinchSpeedMps = 7.f;
	T.WinchArriveM = 3.f;
	T.WinchApproachBelowM = 2.f;
	T.WinchHullClearM = 0.3f;
	const FBox Blocked = Hull.ExpandBy(FVector(Radius, Radius, HalfHeight));
	const FVector Anchor(0.f, 0.f, -250.f);
	const FVector Via(0.f, 0.f, -450.f);

	TestEqual(TEXT("Below the hull: straight to the point under the hatch"), UDeepRules::WinchRouteTarget(FVector(600.f, 0.f, -600.f), Via, Blocked, 30.f), Via);
	const FVector Behind = UDeepRules::WinchRouteTarget(FVector(-900.f, 0.f, 0.f), Via, Blocked, 30.f);
	TestTrue(TEXT("Behind at hull height: straight down first"), FMath::IsNearlyEqual(Behind.X, -900.0) && Behind.Z < Blocked.Min.Z);
	const FVector Over = UDeepRules::WinchRouteTarget(FVector(100.f, 0.f, 600.f), Via, Blocked, 30.f);
	TestTrue(TEXT("Over the hull: out past its edge first, same height"), FMath::IsNearlyEqual(Over.Z, 600.0) && FMath::Abs(Over.Y) > Blocked.Max.Y);

	// Every start reaches home within 4 s of winch and never puts the capsule into the hull.
	for (const FVector& From : Around)
	{
		FVector At = From;
		bool bHit = false;
		float Seconds = 0.f;
		for (; Seconds < 4.f && !UDeepRules::IsHome(At, Anchor, T); Seconds += 0.05f)
		{
			At = UDeepRules::WinchStepAroundHull(At, Anchor, Blocked, 0.05f, T);
			bHit |= Overlaps(Hull, At, Radius, HalfHeight);
		}
		TestTrue(FString::Printf(TEXT("From %s: home in %.2f s"), *From.ToString(), Seconds), UDeepRules::IsHome(At, Anchor, T));
		TestFalse(FString::Printf(TEXT("From %s: never through the hull"), *From.ToString()), bHit);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSwimHullSlideTest, "Deep.Sim.Rules.SwimHullSlide", DEEP_RETURN_TEST_FLAGS)
bool FDeepSwimHullSlideTest::RunTest(const FString& Parameters)
{
	using namespace DeepReturnTest;
	const FVector Touching(-470.f - Radius - 5.f, 0.f, -150.f);
	const FVector Push(400.f, 0.f, 30.f);
	const FVector Slid = UDeepRules::HullSlideVelocity(Touching, Push, Hull, Radius, HalfHeight, 15.f);
	TestTrue(TEXT("Into the back wall: slides down it"), Slid.Z < -300.f && FMath::Abs(Slid.X) < 1.f);
	TestTrue(TEXT("Never faster than the push"), Slid.Size() <= Push.Size() + 0.01f);
	const FVector Glance(300.f, 200.f, 0.f);
	const FVector Along = UDeepRules::HullSlideVelocity(Touching, Glance, Hull, Radius, HalfHeight, 15.f);
	TestTrue(TEXT("A glancing push keeps its sideways part"), FMath::IsNearlyEqual(Along.Y, 200.0, 1.0) && Along.Z < 0.f);
	TestEqual(TEXT("Pushing away: unchanged"), UDeepRules::HullSlideVelocity(Touching, -Push, Hull, Radius, HalfHeight, 15.f), -Push);
	TestEqual(TEXT("Clear of the side: unchanged"), UDeepRules::HullSlideVelocity(Touching - FVector(100.f, 0.f, 0.f), Push, Hull, Radius, HalfHeight, 15.f), Push);
	TestEqual(TEXT("Under the hull: unchanged (rise into the hatch)"), UDeepRules::HullSlideVelocity(FVector(-300.f, 0.f, -260.f), Push, Hull, Radius, HalfHeight, 15.f), Push);
	TestEqual(TEXT("Below the side's bottom edge: unchanged"), UDeepRules::HullSlideVelocity(Touching - FVector(0.f, 0.f, 120.f), Push, Hull, Radius, HalfHeight, 15.f), Push);
	TestEqual(TEXT("Over the hull's top: unchanged"), UDeepRules::HullSlideVelocity(Touching + FVector(0.f, 0.f, 420.f), Push, Hull, Radius, HalfHeight, 15.f), Push);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSubHullShapeTest, "Deep.World.SubHullShape", DEEP_RETURN_TEST_FLAGS)
bool FDeepSubHullShapeTest::RunTest(const FString& Parameters)
{
	using namespace DeepReturnTest;
	FReturnWorld W;
	UTEST_TRUE(TEXT("World, sub, diver, run"), W.Valid());
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	const double M = UDeepRules::CmPerM;
	const FVector Outer((T.RoomHalfLengthM + T.WallThicknessM) * M, (T.RoomHalfWidthM + T.WallThicknessM) * M, (T.RoomHeightM * 0.5 + T.WallThicknessM) * M);
	const FBox HullBox = W.Sub->GetSolidBoundsLocal(true);
	TestTrue(FString::Printf(TEXT("Hull collision %s is the visible box +-%s"), *HullBox.ToString(), *Outer.ToString()),
		HullBox.Min.Equals(-Outer, 1.0) && HullBox.Max.Equals(Outer, 1.0));

	// Every shape with collision is drawn: no invisible volume reaches past what the player sees.
	TArray<UStaticMeshComponent*> Shapes;
	W.Sub->GetComponents<UStaticMeshComponent>(Shapes);
	int32 Solid = 0;
	for (const UStaticMeshComponent* Shape : Shapes)
	{
		if (Shape->IsCollisionEnabled())
		{
			++Solid;
			TestTrue(FString::Printf(TEXT("%s has collision and is visible"), *Shape->GetName()), Shape->IsVisible());
		}
	}
	TestTrue(TEXT("Some shapes are solid"), Solid > 0);

	// A trace at the hull from each side, at hatch height, meets the visible face (not something in front of it).
	const FTransform& Frame = W.Sub->GetActorTransform();
	const double Z = -T.RoomHeightM * 0.5 * M;
	for (const FVector& Dir : { FVector(1.0, 0.0, 0.0), FVector(-1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), FVector(0.0, -1.0, 0.0) })
	{
		const FVector Start = Frame.TransformPosition(FVector(0.0, 0.0, Z) + Dir * 1500.0);
		const FVector End = Frame.TransformPosition(FVector(0.0, 0.0, Z));
		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(W.Diver);
		if (W.World->LineTraceSingleByChannel(Hit, Start, End, ECC_Pawn, Params))
		{
			const FVector Local = Frame.InverseTransformPosition(Hit.ImpactPoint);
			const double Face = FMath::Abs(Dir.X) > 0.5 ? Outer.X : Outer.Y;
			TestEqual(FString::Printf(TEXT("Trace from %s meets the visible face"), *Dir.ToString()), FMath::Abs(FVector::DotProduct(Local, Dir)), Face, 2.0);
		}
		else
		{
			AddWarning(FString::Printf(TEXT("Trace from %s hit nothing (no physics scene in this test world?)"), *Dir.ToString()));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSubReturnWinchTest, "Deep.World.SubReturnWinch", DEEP_RETURN_TEST_FLAGS)
bool FDeepSubReturnWinchTest::RunTest(const FString& Parameters)
{
	using namespace DeepReturnTest;
	FReturnWorld W;
	UTEST_TRUE(TEXT("World, sub, diver, run"), W.Valid());
	const float DiverRadius = W.Diver->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float DiverHalfHeight = W.Diver->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FBox Solid = W.Sub->GetSolidBoundsLocal(false);
	constexpr float LimitSeconds = 6.f;

	for (const FVector& From : Around)
	{
		UTEST_TRUE(FString::Printf(TEXT("Dived and placed at %s"), *From.ToString()), W.DiveTo(From));
		W.Diver->SetWinching(true);
		bool bHit = false;
		float Seconds = 0.f;
		for (; Seconds < LimitSeconds && W.Run->GetState() == EDeepRunState::Tethered; Seconds += 0.05f)
		{
			W.Diver->GetTether()->UpdateTether(0.05f);
			if (W.Run->GetState() == EDeepRunState::Tethered)
			{
				bHit |= Overlaps(Solid, W.LocalDiver(), DiverRadius, DiverHalfHeight);
			}
			W.Run->StepRun(0.05f);
		}
		TestEqual(FString::Printf(TEXT("Winched from %s: aboard in %.2f s (limit %.0f)"), *From.ToString(), Seconds, LimitSeconds), W.Run->GetState(), EDeepRunState::Aboard);
		TestFalse(FString::Printf(TEXT("Winched from %s: never through the hull"), *From.ToString()), bHit);
		W.Diver->SetWinching(false);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSubReturnSwimTest, "Deep.World.SubReturnSwim", DEEP_RETURN_TEST_FLAGS)
bool FDeepSubReturnSwimTest::RunTest(const FString& Parameters)
{
	using namespace DeepReturnTest;
	FReturnWorld W;
	UTEST_TRUE(TEXT("World, sub, diver, run"), W.Valid());
	// A controller aimed at the hatch every frame while forward is held: the diver swims straight at it, as the S8
	// script did. The diver's own Tick turns that into swim input, character movement sweeps it against the hull.
	AAIController* Brain = W.World->SpawnActor<AAIController>();
	UTEST_NOT_NULL(TEXT("Controller"), Brain);
	Brain->Possess(W.Diver);
	UCharacterMovementComponent* Movement = W.Diver->GetCharacterMovement();
	constexpr float LimitSeconds = 15.f;
	constexpr float Dt = 1.f / 30.f;

	for (const FVector& From : { FVector(-900.f, 0.f, -150.f), FVector(0.f, 800.f, -150.f), FVector(-700.f, -600.f, 100.f) })
	{
		UTEST_TRUE(FString::Printf(TEXT("Dived and placed at %s"), *From.ToString()), W.DiveTo(From));
		W.Diver->SetMoveInput(FVector2D(0.f, 1.f));
		float Seconds = 0.f;
		for (; Seconds < LimitSeconds && W.Run->GetState() == EDeepRunState::Tethered; Seconds += Dt)
		{
			Brain->SetControlRotation((W.Sub->GetHatchLocation() - W.Diver->GetActorLocation()).Rotation());
			W.Diver->Tick(Dt);
			Movement->TickComponent(Dt, LEVELTICK_All, &Movement->PrimaryComponentTick);
			W.Diver->GetTether()->UpdateTether(Dt);
			W.Run->StepRun(Dt);
		}
		W.Diver->SetMoveInput(FVector2D::ZeroVector);
		TestEqual(FString::Printf(TEXT("Swam at the hatch from %s: aboard in %.1f s (limit %.0f, ended at %s)"), *From.ToString(), Seconds, LimitSeconds, *W.LocalDiver().ToString()),
			W.Run->GetState(), EDeepRunState::Aboard);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSubRoomLayoutTest, "Deep.World.SubRoomLayout", DEEP_RETURN_TEST_FLAGS)
bool FDeepSubRoomLayoutTest::RunTest(const FString& Parameters)
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	const ADeepDiver* Default = GetDefault<ADeepDiver>();
	const double WalkerRadius = Default->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const double M = UDeepRules::CmPerM;
	// The opening grown by the walker's radius: a centre outside it never drops (it still stands on the rim).
	const double Half = T.HatchHalfSizeM * M + WalkerRadius;
	const FBox Opening(FVector(-Half, -Half, -1.0), FVector(Half, Half, 1.0));
	const TPair<const TCHAR*, FVector2D> Stations[] = {
		{ TEXT("start"), T.RoomStartM }, { TEXT("enter stand"), T.EnterStandM }, { TEXT("console"), T.ConsoleM }, { TEXT("helm"), T.HelmM } };
	for (int32 A = 0; A < UE_ARRAY_COUNT(Stations); ++A)
	{
		const FVector P(Stations[A].Value * M, 0.0);
		TestFalse(FString::Printf(TEXT("The %s is off the hatch"), Stations[A].Key), Opening.IsInside(P));
		for (int32 B = A + 1; B < UE_ARRAY_COUNT(Stations); ++B)
		{
			const FVector Q(Stations[B].Value * M, 0.0);
			TestFalse(FString::Printf(TEXT("The straight walk %s -> %s misses the hatch"), Stations[A].Key, Stations[B].Key),
				FMath::LineBoxIntersection(Opening, P, Q, Q - P));
		}
	}
	// Each station is inside the room with room for the walker.
	for (const TPair<const TCHAR*, FVector2D>& Station : Stations)
	{
		TestTrue(FString::Printf(TEXT("The %s is inside the room"), Station.Key),
			FMath::Abs(Station.Value.X * M) <= T.RoomHalfLengthM * M - WalkerRadius && FMath::Abs(Station.Value.Y * M) <= T.RoomHalfWidthM * M - WalkerRadius);
	}
	return true;
}

#endif
