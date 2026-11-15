// The camera and the hull: a wall hit shakes the view by how hard the sub hit (DeepCameraRules::ShakeForHit, the
// payload's ImpactMps), and the sonar pulse's hull box covers every part of the sub so the shell never lights it.

#include "CoreMinimal.h"
#include "Camera/DeepCameraRules.h"
#include "Camera/DeepCameraSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Sim/DeepCaveRules.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepEventBus.h"
#include "World/DeepArena.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DeepCameraHullTest
{
	/** A console variable held at a value for the test's lifetime. */
	struct FCVarScope
	{
		IConsoleVariable* Var = nullptr;
		int32 Before = -1;

		FCVarScope(const TCHAR* Name, int32 Value)
		{
			Var = IConsoleManager::Get().FindConsoleVariable(Name);
			if (Var)
			{
				Before = Var->GetInt();
				Var->Set(Value, ECVF_SetByCode);
			}
		}

		~FCVarScope()
		{
			if (Var)
			{
				Var->Set(Before, ECVF_SetByCode);
			}
		}
	};

	/** A game world with the arena and one diver in the room, as a run starts (as Deep.World.Cave's). */
	struct FHullWorld
	{
		UWorld* World = nullptr;
		ADeepGameMode* GameMode = nullptr;
		ADeepGameState* State = nullptr;
		ADeepSubmarine* Sub = nullptr;
		ADeepDiver* Diver = nullptr;
		UDeepRunComponent* Run = nullptr;
		UDeepEventBus* Bus = nullptr;

		FHullWorld()
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
			Bus = UDeepEventBus::Get(World);
			if (Sub)
			{
				Diver = World->SpawnActor<ADeepDiver>(Sub->GetRoomStartTransform().GetLocation(), FRotator::ZeroRotator);
			}
			Run = Diver ? Diver->GetRun() : nullptr;
			if (Run)
			{
				Run->StepRun(0.f);
			}
		}

		~FHullWorld()
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCameraHitShakeTest, "Deep.Camera.HitShake", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeepCameraHitShakeTest::RunTest(const FString& Parameters)
{
	const FDeepCameraTuning T;
	// The shaft's own event names (DeepEvent::) are the rows' names: a rename on either side fails here.
	for (const TCHAR* Event : { DeepEvent::HullDamage, DeepEvent::HullHit, DeepEvent::HullCritical, DeepEvent::HullBreach })
	{
		TestFalse(FString::Printf(TEXT("%s shakes"), Event), DeepCameraRules::ShakeForEvent(FName(Event), T).IsNone());
	}
	const FName Hit(DeepEvent::HullHit);
	const FDeepShakeSpec Row = DeepCameraRules::ShakeForEvent(Hit, T);
	TestTrue(TEXT("A scrape is a judder under a knock"), DeepCameraRules::ShakeForEvent(FName(DeepEvent::HullDamage), T).RotationDeg < Row.RotationDeg);

	auto Rot = [&T, &Hit](float Mps) { return DeepCameraRules::ShakeForHit(Hit, Mps, T).RotationDeg; };
	TestTrue(TEXT("At the reference speed the row plays as written"), FMath::IsNearlyEqual(Rot(T.HitShakeRefMps), Row.RotationDeg));
	TestTrue(TEXT("No impact (a breach): the row as written"), FMath::IsNearlyEqual(Rot(0.f), Row.RotationDeg));
	const FDeepCaveTuning Cave;
	TestTrue(TEXT("The slowest hit is smaller than the row, but still reads"),
		Rot(Cave.HitMinSpeedMps) < Row.RotationDeg && FMath::IsNearlyEqual(Rot(Cave.HitMinSpeedMps), float(Row.RotationDeg * T.HitShakeScaleRange.X), 1e-4f));
	TestTrue(TEXT("A harder hit shakes harder"), Rot(4.f) > Rot(3.5f) && Rot(3.5f) > Rot(2.f));
	TestTrue(TEXT("A ram is capped"), FMath::IsNearlyEqual(Rot(50.f), float(Row.RotationDeg * T.HitShakeScaleRange.Y), 1e-4f));
	TestTrue(TEXT("Location scales with rotation"), FMath::IsNearlyEqual(DeepCameraRules::ShakeForHit(Hit, 6.f, T).LocationCm, float(Row.LocationCm * FMath::Min(2.0, T.HitShakeScaleRange.Y)), 1e-4f));
	TestTrue(TEXT("Duration does not scale"), FMath::IsNearlyEqual(DeepCameraRules::ShakeForHit(Hit, 6.f, T).DurationS, Row.DurationS));
	TestTrue(TEXT("A hit at the reference speed stays under critical"), Rot(T.HitShakeRefMps) < DeepCameraRules::ShakeForEvent(FName(DeepEvent::HullCritical), T).RotationDeg);
	TestTrue(TEXT("Events that do not shake still do not"), DeepCameraRules::ShakeForHit(FName(TEXT("first_salvage")), 5.f, T).IsNone());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCameraPulseBoxTest, "Deep.Camera.PulseBox", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeepCameraPulseBoxTest::RunTest(const FString& Parameters)
{
	const FBox Parts(FVector(-500.0, -250.0, -170.0), FVector(800.0, 250.0, 260.0));
	const FTransform SubToWorld(FRotator(0.f, 30.f, 0.f), FVector(1000.0, -2000.0, -30000.0));
	FVector C, X, Y, Z;
	DeepCameraRules::PulseHullBox(Parts, SubToWorld, 0.3f, C, X, Y, Z);
	auto In = [&](const FVector& Local) { return DeepCameraRules::IsInPulseHull(SubToWorld.TransformPosition(Local), C, X, Y, Z); };
	TestTrue(TEXT("The room is inside"), In(FVector::ZeroVector));
	TestTrue(TEXT("The bow tip is inside"), In(FVector(799.0, 0.0, 0.0)));
	TestTrue(TEXT("The margin covers a part's skin"), In(FVector(820.0, 240.0, -190.0)));
	TestFalse(TEXT("Rock a metre off the bow is outside"), In(FVector(930.0, 0.0, 0.0)));
	TestFalse(TEXT("Rock a metre off the side is outside"), In(FVector(0.0, 380.0, 0.0)));
	TestFalse(TEXT("The sea bed under the sub is outside"), In(FVector(0.0, 0.0, -300.0)));
	TestTrue(TEXT("Turns with the sub: the bow's port corner"), In(FVector(790.0, -245.0, 250.0)));

	DeepCameraRules::PulseHullBox(FBox(ForceInit), FTransform(FVector(5.0, 0.0, 0.0)), 0.3f, C, X, Y, Z);
	TestFalse(TEXT("No sub: nothing a metre away is masked"), DeepCameraRules::IsInPulseHull(FVector(105.0, 0.0, 0.0), C, X, Y, Z));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCameraHullHitTest, "Deep.Camera.HullHit", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepCameraHullHitTest::RunTest(const FString& Parameters)
{
	using namespace DeepCameraHullTest;
	FCVarScope Shaft(TEXT("Deep.Cave.Shaft"), 1);
	FCVarScope Shake(TEXT("Deep.Camera.Shake"), 1);
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FHullWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run, bus"), W.Valid());
	UDeepCameraSubsystem* Camera = UDeepCameraSubsystem::Get(W.World);
	UTEST_NOT_NULL(TEXT("The camera subsystem runs in a game world"), Camera);

	// The pulse's hull box covers every part of the sub, and stops short of rock beside it.
	const FBox Local = UDeepCameraSubsystem::SubPartsLocal(W.Sub);
	UTEST_TRUE(TEXT("The sub has parts"), Local.IsValid != 0);
	AddInfo(FString::Printf(TEXT("Sub parts box %s cm"), *Local.GetSize().ToCompactString()));
	TestTrue(TEXT("No stray part far from the hull (box under 20 m)"), Local.GetSize().GetMax() < 2000.0);
	FVector C, X, Y, Z;
	DeepCameraRules::PulseHullBox(Local, W.Sub->GetActorTransform(), Tuning.Camera.PulseHullMarginM, C, X, Y, Z);
	TArray<UStaticMeshComponent*> Parts;
	W.Sub->GetComponents<UStaticMeshComponent>(Parts);
	int32 Outside = 0;
	for (const UStaticMeshComponent* Part : Parts)
	{
		if (!Part || !Part->GetStaticMesh())
		{
			continue;
		}
		const FBoxSphereBounds B = Part->Bounds;
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			const FVector P = B.Origin + B.BoxExtent * 0.99 * FVector((Corner & 1) ? 1.0 : -1.0, (Corner & 2) ? 1.0 : -1.0, (Corner & 4) ? 1.0 : -1.0);
			if (!DeepCameraRules::IsInPulseHull(P, C, X, Y, Z))
			{
				++Outside;
				AddError(FString::Printf(TEXT("%s pokes out of the pulse's hull box"), *Part->GetName()));
				break;
			}
		}
	}
	TestEqual(TEXT("Every part of the sub is inside the hull box"), Outside, 0);
	const FVector Beside = W.Sub->GetActorTransform().TransformPosition(FVector(0.0, (Local.Max.Y + 150.0), 0.0));
	TestFalse(TEXT("Rock 1.5 m off the widest part is lit"), DeepCameraRules::IsInPulseHull(Beside, C, X, Y, Z));

	// Steer into the shaft wall: hull_hit carries the impact speed and the camera shakes by it.
	float ImpactMps = -1.f;
	const FDelegateHandle Listen = W.Bus->OnEvent.AddLambda([&ImpactMps](FName Event, const FDeepEventPayload& Payload)
	{
		if (Event == FName(DeepEvent::HullHit))
		{
			ImpactMps = Payload.ImpactMps;
		}
	});
	if (!W.Run->IsSeated())
	{
		W.Run->TakeHelm();
	}
	const float DepthM = 0.5f * (Tuning.GetStop(1).SubDepthM + Tuning.GetStop(2).SubDepthM);
	W.State->SubDepthM = DepthM;
	W.State->SubOffsetM = UDeepCaveRules::CentreM(DepthM, Tuning.StopDepthsM(), Tuning.Cave);
	W.State->bParked = false;
	DeepArena::ApplySubDepth(W.World);
	W.Run->SetHelmSteer(FVector2D(0.f, 1.f));
	for (float T = 0.f; T < 30.f && W.Bus->CountOf(DeepEvent::HullHit) == 0; T += 0.05f)
	{
		W.Run->StepRun(0.05f);
	}
	W.Run->SetHelmSteer(FVector2D::ZeroVector);
	W.Bus->OnEvent.Remove(Listen);

	UTEST_EQUAL(TEXT("hull_hit fired"), W.Bus->CountOf(DeepEvent::HullHit), 1);
	AddInfo(FString::Printf(TEXT("Hit the wall at %.2f m/s"), ImpactMps));
	TestTrue(TEXT("The payload carries the impact speed"), ImpactMps >= Tuning.Cave.HitMinSpeedMps);
	TestEqual(TEXT("The camera chose the hull_hit shake"), Camera->GetLastEventShake(), FName(DeepEvent::HullHit));
	const FDeepShakeSpec Want = DeepCameraRules::ShakeForHit(FName(DeepEvent::HullHit), ImpactMps, Tuning.Camera);
	const FDeepShakeSpec Row = DeepCameraRules::ShakeForEvent(FName(DeepEvent::HullHit), Tuning.Camera);
	const float Scale = FMath::Clamp(ImpactMps / Tuning.Camera.HitShakeRefMps, Tuning.Camera.HitShakeScaleRange.X, Tuning.Camera.HitShakeScaleRange.Y);
	TestTrue(TEXT("Sized by the hit"), FMath::IsNearlyEqual(Camera->GetLastEventSpec().RotationDeg, Want.RotationDeg, 1e-4f)
		&& FMath::IsNearlyEqual(Camera->GetLastEventSpec().RotationDeg, Row.RotationDeg * Scale, 1e-4f));
	return true;
}

#endif
