// D2 (Nov 25): BT-01's own cabin as the pilot's view. The pure rules (Sim/DeepCabinRules: a pane from traces, the cut
// box, the pane's transform), then on the real model: the bow ports' glass where the tuning puts it (fitted again from
// traces on SM_BT01's Glass slot), the ports cut open with a pane in each, and a clear line from the pilot's eye to both.
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/DeepGameMode.h"
#include "Misc/AutomationTest.h"
#include "Sim/DeepCabinRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepHull.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_CABIN_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCabinRulesTest, "Deep.Sim.Cabin.Rules", DEEP_CABIN_TEST_FLAGS)

bool FDeepCabinRulesTest::RunTest(const FString& Parameters)
{
	// Points on a 30 cm disc facing (0.6, 0.8, 0) at (450, 50, 130), its normals half of them flipped (traces from both sides).
	const FVector Centre(450.0, 50.0, 130.0);
	const FVector Normal = FVector(0.6, 0.8, 0.0).GetSafeNormal();
	const FVector U = FVector::UpVector;
	const FVector V = FVector::CrossProduct(Normal, U);
	TArray<FVector> Points, Normals;
	for (int32 A = -30; A <= 30; A += 3)
	{
		for (int32 B = -30; B <= 30; B += 3)
		{
			if (A * A + B * B <= 30 * 30)
			{
				Points.Add(Centre + U * A + V * B);
				Normals.Add((A + B) % 2 == 0 ? Normal : -Normal);
			}
		}
	}
	const FDeepPortPane Pane = DeepCabin::FitPane(Points, Normals, FVector(1.0, 1.0, 0.0));
	TestTrue(TEXT("fits"), Pane.bValid);
	TestTrue(TEXT("centre"), Pane.CentreCm.Equals(Centre, 0.5));
	TestTrue(TEXT("normal outward"), FVector::DotProduct(Pane.Normal, Normal) > 0.999);
	TestEqual(TEXT("radius"), Pane.RadiusCm, 30.f, 0.5f);
	TestFalse(TEXT("too few points: no pane"), DeepCabin::FitPane({ Centre }, { Normal }, Normal).bValid);

	FDeepCabinTuning T;
	T.PortCentreCm = Centre;
	T.PortNormal = Normal;
	T.PortRadiusCm = 30.f;
	const TArray<FDeepPortPane> Panes = DeepCabin::TunedPanes(T);
	TestEqual(TEXT("two panes"), Panes.Num(), 2);
	TestTrue(TEXT("port mirrors starboard"), Panes[0].CentreCm.Equals(FVector(450.0, -50.0, 130.0)) && Panes[0].Normal.Y < 0.0 && Panes[1].Normal.Y > 0.0);
	const FBox Box = DeepCabin::CutBox(Panes, T);
	bool bAllIn = true;
	for (const FVector& P : Points)
	{
		bAllIn &= Box.IsInside(P) && Box.IsInside(FVector(P.X, -P.Y, P.Z));
	}
	TestTrue(TEXT("the cut box holds both discs"), bAllIn);
	TestTrue(TEXT("and stays near them (not the whole bow)"), Box.GetSize().Z < 2.0 * (30.0 + T.PortCutMarginCm) + 1.0);
	const FTransform Xf = DeepCabin::PaneTransform(Panes[1], T);
	TestTrue(TEXT("the cylinder's axis on the normal"), FVector::DotProduct(Xf.GetUnitAxis(EAxis::Z), Normal) > 0.999);
	TestTrue(TEXT("60 cm across, thickness as tuned"), Xf.GetScale3D().Equals(FVector(0.6, 0.6, T.PortThicknessCm / 100.0), 1e-4));
	const FBox Moved = DeepCabin::BoxInto(FBox(FVector(-1.0), FVector(1.0)), FTransform(FRotator(0.0, 45.0, 0.0), FVector(10.0, 0.0, 0.0), FVector(2.0)));
	TestTrue(TEXT("a box turned 45 deg grows by root 2"), FMath::IsNearlyEqual(Moved.GetExtent().X, 2.0 * FMath::Sqrt(2.0), 1e-3));
	return true;
}

namespace DeepCabinTest
{
	struct FCabinWorld
	{
		UWorld* World = nullptr;
		ADeepSubmarine* Sub = nullptr;

		FCabinWorld()
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
		}

		~FCabinWorld()
		{
			if (World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCabinPortsTest, "Deep.World.Cabin.Ports", DEEP_CABIN_TEST_FLAGS)

bool FDeepCabinPortsTest::RunTest(const FString& Parameters)
{
	using namespace DeepCabinTest;
	const FCabinWorld W;
	UTEST_NOT_NULL(TEXT("the sub"), W.Sub);
	UStaticMeshComponent* Model = W.Sub->GetHullModel();
	UTEST_NOT_NULL(TEXT("BT-01"), Model);
	UTEST_TRUE(TEXT("BT-01 is shown"), W.Sub->IsModelHull());
	const FDeepCabinTuning& C = UDeepTuningSettings::Get().Cabin;
	const FDeepHelmViewTuning& View = UDeepTuningSettings::Get().HelmView;
	const int32 Slot = Model->GetMaterialIndex(C.GlassSlot);
	UTEST_TRUE(TEXT("the Glass slot"), Slot != INDEX_NONE);
	const UMaterialInterface* GlassMaterial = Model->GetMaterial(Slot);
	const FTransform Frame = W.Sub->GetHullVisual()->GetComponentTransform();

	// Fit each port from traces: a grid of rays through the tuned port, from inside the cabin outward, keeping the points
	// where the first surface met is the Glass slot (the model's complex collision, its triangles).
	const TArray<FDeepPortPane> Tuned = DeepCabin::TunedPanes(C);
	TArray<FDeepPortPane> Fitted;
	for (const FDeepPortPane& Guess : Tuned)
	{
		const FVector Up = FVector::UpVector;
		const FVector Side = FVector::CrossProduct(Guess.Normal, Up).GetSafeNormal();
		TArray<FVector> Points, Normals;
		for (int32 A = -48; A <= 48; A += 3)
		{
			for (int32 B = -48; B <= 48; B += 3)
			{
				const FVector Through = Guess.CentreCm + Up * A + Side * B;
				const FVector From = Through - Guess.Normal * 25.0;
				const FVector To = Through + Guess.Normal * 25.0;
				FHitResult Hit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(DeepCabinPorts), true);
				Params.bReturnFaceIndex = true;
				if (!Model->LineTraceComponent(Hit, Frame.TransformPosition(From), Frame.TransformPosition(To), Params))
				{
					continue;
				}
				int32 Section = INDEX_NONE;
				if (Model->GetMaterialFromCollisionFaceIndex(Hit.FaceIndex, Section) == GlassMaterial)
				{
					Points.Add(Frame.InverseTransformPosition(Hit.ImpactPoint));
					Normals.Add(Frame.InverseTransformVectorNoScale(Hit.ImpactNormal));
				}
			}
		}
		const FDeepPortPane Pane = DeepCabin::FitPane(Points, Normals, Guess.Normal);
		AddInfo(FString::Printf(TEXT("port fit: %d glass hits, centre %s, normal %s, radius %.1f cm (tuned %s, %s, %.1f)"), Points.Num(),
			*Pane.CentreCm.ToCompactString(), *Pane.Normal.ToCompactString(), Pane.RadiusCm, *Guess.CentreCm.ToCompactString(), *Guess.Normal.ToCompactString(), Guess.RadiusCm));
		Fitted.Add(Pane);
	}
	UTEST_EQUAL(TEXT("two ports"), Fitted.Num(), 2);
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const TCHAR* Name = Index == 0 ? TEXT("port") : TEXT("starboard");
		TestTrue(FString::Printf(TEXT("the %s port's glass is found"), Name), Fitted[Index].bValid);
		TestTrue(FString::Printf(TEXT("the %s pane sits where the tuning says (3 cm)"), Name), Fitted[Index].CentreCm.Equals(Tuned[Index].CentreCm, 3.0));
		TestTrue(FString::Printf(TEXT("the %s pane faces the same way (5 deg)"), Name), FVector::DotProduct(Fitted[Index].Normal, Tuned[Index].Normal) > FMath::Cos(FMath::DegreesToRadians(5.0)));
		TestEqual(FString::Printf(TEXT("the %s pane's radius (3 cm)"), Name), Fitted[Index].RadiusCm, Tuned[Index].RadiusCm, 3.f);
	}

	// Cut open, a pane in each, and from the pilot's eye both are in clear sight (nothing of the cabin in between).
	TestTrue(TEXT("the ports are cut open"), W.Sub->ArePortsCut());
	TestEqual(TEXT("two panes"), W.Sub->GetPortPanes().Num(), 2);
	for (int32 Index = 0; Index < W.Sub->GetPortPanes().Num() && Index < 2; ++Index)
	{
		const UStaticMeshComponent* Pane = W.Sub->GetPortPanes()[Index];
		TestFalse(TEXT("the pane shows"), Pane->bHiddenInGame);
		TestTrue(TEXT("the pane sits in its port"), Frame.InverseTransformPosition(Pane->GetComponentLocation()).Equals(Tuned[Index].CentreCm, 0.5));
		const FVector Eye = Frame.TransformPosition(View.CabinEyeCm);
		const FVector Target = Frame.TransformPosition(Tuned[Index].CentreCm);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DeepCabinSight), true);
		Params.bReturnFaceIndex = true;
		const bool bHit = Model->LineTraceComponent(Hit, Eye, Target + (Target - Eye).GetSafeNormal() * 20.0, Params);
		int32 Section = INDEX_NONE;
		const bool bGlassFirst = bHit && Model->GetMaterialFromCollisionFaceIndex(Hit.FaceIndex, Section) == W.Sub->GetHullModel()->GetMaterial(Slot);
		AddInfo(FString::Printf(TEXT("from the eye to the %s pane: first surface at %.0f of %.0f cm, %s"), Index == 0 ? TEXT("port") : TEXT("starboard"),
			bHit ? Hit.Distance : -1.f, FVector::Dist(Eye, Target), bGlassFirst ? TEXT("the glass") : TEXT("something else")));
		TestTrue(TEXT("from the eye the first thing on the way to the pane is its glass"), bGlassFirst && Hit.Distance > FVector::Dist(Eye, Target) - 10.0);
	}
	// The cabin's own lights: from the tuning, none lights the fog.
	int32 Lit = 0;
	for (const UPointLightComponent* Light : W.Sub->GetCabinLights())
	{
		TestEqual(TEXT("no cabin light lights the fog"), Light->VolumetricScatteringIntensity, 0.f);
		Lit += Light->Intensity > 0.f ? 1 : 0;
	}
	TestEqual(TEXT("as many cabin lights as the tuning has"), Lit, FMath::Min(C.Lights.Num(), W.Sub->GetCabinLights().Num()));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
