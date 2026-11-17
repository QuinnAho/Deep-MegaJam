// BT-01 on the hull (World/DeepHull, UDeepHullModel): the mesh loads with Nanite and carries its own convex hulls, fits
// the cave tool's 12.3 x 3.3 x 5.0 m box, its belly hatch sits on the hatch column, and Deep.Sub.Model swaps it with the
// old dress: the model is then the collider, the box colliders and the square hatch are off, and the outside lamps sit
// on the model's surface with a bulb each.

#include "Components/LocalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Game/DeepGameMode.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/BodySetup.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepHull.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DeepSubModelTest
{
	FVector Outer()
	{
		const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
		const double M = UDeepRules::CmPerM;
		return FVector((T.RoomHalfLengthM + T.WallThicknessM) * M, (T.RoomHalfWidthM + T.WallThicknessM) * M, (T.RoomHeightM * 0.5 + T.WallThicknessM) * M);
	}

	/** A game world with the arena, begun play (the sub's look and lamps applied); the model switched by hand after. */
	struct FSubWorld
	{
		UWorld* World = nullptr;
		ADeepSubmarine* Sub = nullptr;
		UDeepHullModel* Model = nullptr;

		FSubWorld()
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
			Model = Sub ? Sub->GetHullModel() : nullptr;
		}

		~FSubWorld()
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSubModelTest, "Deep.World.SubModel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeepSubModelTest::RunTest(const FString& Parameters)
{
	using namespace DeepSubModelTest;
	const FVector OuterCm = Outer();

	// The mesh: loads, Nanite, its own simple collision (convex hulls from build_sub_hull.py).
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, DeepHull::ModelMesh);
	UTEST_NOT_NULL(TEXT("BT-01 loads"), Mesh);
#if WITH_EDITOR
	TestTrue(TEXT("Nanite on"), Mesh->IsNaniteEnabled());
#endif
	const UBodySetup* Body = Mesh->GetBodySetup();
	UTEST_NOT_NULL(TEXT("BT-01 has a body setup"), Body);
	AddInfo(FString::Printf(TEXT("BT-01 simple collision: %d convex hulls"), Body->AggGeom.ConvexElems.Num()));
	TestTrue(TEXT("Convex hulls on the mesh"), Body->AggGeom.ConvexElems.Num() > 0);
	TestTrue(TEXT("Deep.Sub.Model defaults to the model"), DeepHull::WantsModel());

	// The fit: one scale, inside the envelope, as long as the envelope allows, the hatch on the column at the belly.
	const FBox MeshBox = Mesh->GetBoundingBox();
	const FTransform Fit = DeepHull::ModelTransform(MeshBox, OuterCm);
	const FVector Scale = Fit.GetScale3D();
	TestTrue(TEXT("Uniform scale"), FMath::IsNearlyEqual(Scale.X, Scale.Y) && FMath::IsNearlyEqual(Scale.X, Scale.Z));
	const FVector Size = MeshBox.GetSize() * Scale.X;
	AddInfo(FString::Printf(TEXT("BT-01 scale %.4f: %.1f x %.1f x %.1f cm in %.0f x %.0f x %.0f"), Scale.X, Size.X, Size.Y, Size.Z,
		DeepHull::ModelEnvelopeCm.X, DeepHull::ModelEnvelopeCm.Y, DeepHull::ModelEnvelopeCm.Z));
	TestTrue(TEXT("Length within 12.3 m"), Size.X <= DeepHull::ModelEnvelopeCm.X + 0.1);
	TestTrue(TEXT("Beam within 3.3 m"), Size.Y <= DeepHull::ModelEnvelopeCm.Y + 0.1);
	TestTrue(TEXT("Height within 5.0 m"), Size.Z <= DeepHull::ModelEnvelopeCm.Z + 0.1);
	TestTrue(TEXT("One side touches the envelope"),
		FMath::IsNearlyEqual(Size.X, DeepHull::ModelEnvelopeCm.X, 0.5) || FMath::IsNearlyEqual(Size.Y, DeepHull::ModelEnvelopeCm.Y, 0.5)
		|| FMath::IsNearlyEqual(Size.Z, DeepHull::ModelEnvelopeCm.Z, 0.5));
	const FVector Hatch = Fit.TransformPosition(FVector(DeepHull::ModelHatchLocalCm.X, DeepHull::ModelHatchLocalCm.Y, MeshBox.Min.Z));
	TestTrue(TEXT("Hatch over the hatch column at the belly"), Hatch.Equals(FVector(0.0, 0.0, -OuterCm.Z), 0.5));
	const FBox Placed = MeshBox.TransformBy(Fit);
	TestTrue(TEXT("Model's bottom is the belly: nothing in the hatch column below"), FMath::IsNearlyEqual(Placed.Min.Z, -OuterCm.Z, 0.5));
	const FVector Centre = Placed.GetCenter();
	AddInfo(FString::Printf(TEXT("Model centre from the hull origin (the hatch): %s cm"), *Centre.ToString()));
	TestTrue(FString::Printf(TEXT("DeepHull::ModelCentreOffsetCm %s is the fit's %s"), *DeepHull::ModelCentreOffsetCm.ToString(), *Centre.ToString()),
		DeepHull::ModelCentreOffsetCm.Equals(Centre, 0.5));

	// On the sub, switched both ways by hand.
	FSubWorld W;
	UTEST_NOT_NULL(TEXT("Sub spawns"), W.Sub);
	UTEST_NOT_NULL(TEXT("Sub has the model"), W.Model);
	UDeepHullModel* Model = W.Model;
	ADeepSubmarine* Sub = W.Sub;
	TestTrue(TEXT("Model mesh"), Model->GetStaticMesh() == Mesh);
	TestFalse(TEXT("Model makes no overlaps"), Model->GetGenerateOverlapEvents());
	TestTrue(TEXT("Model placed by the fit"), Model->GetRelativeTransform().Equals(Fit, 0.01));
	UStaticMeshComponent* Cover = nullptr;
	UStaticMeshComponent* Water = nullptr;
	TInlineComponentArray<UStaticMeshComponent*> Meshes(Sub);
	for (UStaticMeshComponent* Shape : Meshes)
	{
		Cover = Shape->GetFName() == TEXT("HatchCover") ? Shape : Cover;
		Water = Shape->GetFName() == TEXT("HatchWater") ? Shape : Water;
	}
	UTEST_TRUE(TEXT("Hatch cover and water"), Cover && Water);

	for (const bool bModel : { true, false, true })
	{
		const TCHAR* Tag = bModel ? TEXT("1") : TEXT("0");
		Model->ApplySwitch(bModel);
		TestEqual(FString::Printf(TEXT("%s: IsModelHull"), Tag), Sub->IsModelHull(), bModel);
		TestEqual(FString::Printf(TEXT("%s: model shown"), Tag), !Model->bHiddenInGame, bModel);
		TestEqual(FString::Printf(TEXT("%s: model is the collider"), Tag), Model->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics
			&& Model->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block, bModel);
		for (const UStaticMeshComponent* Shape : Sub->GetHullDress())
		{
			TestEqual(FString::Printf(TEXT("%s: dress %s hidden"), Tag, *Shape->GetName()), bool(Shape->bHiddenInGame), bModel);
		}
		// With the model, nothing else on the sub blocks: the box colliders, the window, the tower and the cover are off.
		int32 OtherSolid = 0;
		for (const UStaticMeshComponent* Shape : Meshes)
		{
			if (Shape != Model && Shape->IsCollisionEnabled())
			{
				++OtherSolid;
				TestFalse(FString::Printf(TEXT("%s: %s is solid and drawn"), Tag, *Shape->GetName()), Shape->bHiddenInGame);
			}
		}
		TestEqual(FString::Printf(TEXT("%s: box colliders on only without the model (%d solid)"), Tag, OtherSolid), OtherSolid > 0, !bModel);
		TestEqual(FString::Printf(TEXT("%s: square hatch cover hidden"), Tag), bool(Cover->bHiddenInGame), bModel);
		TestEqual(FString::Printf(TEXT("%s: square hatch water hidden"), Tag), bool(Water->bHiddenInGame), bModel);
		for (uint8 Index = 0; Index < uint8(DeepHull::ELamp::Count); ++Index)
		{
			const UStaticMeshComponent* Bulb = Sub->GetBulb(DeepHull::ELamp(Index));
			if (TestNotNull(TEXT("Bulb"), Bulb))
			{
				TestEqual(FString::Printf(TEXT("%s: bulb %s shown with the model"), Tag, *Bulb->GetName()), !Bulb->bHiddenInGame, bModel);
				TestFalse(TEXT("Bulb has no collision"), Bulb->IsCollisionEnabled());
			}
		}
		// The diver still leaves and comes home through the hatch column: the hatch stays at the hull origin.
		TestTrue(FString::Printf(TEXT("%s: hatch column at the origin"), Tag), Sub->IsInHatchColumn(Sub->GetActorTransform().TransformPosition(FVector(0.0, 0.0, -OuterCm.Z - 50.0))));
		Sub->SetHatchOpen(false);
		TestEqual(FString::Printf(TEXT("%s: closed square cover blocks only on the box"), Tag), Cover->IsCollisionEnabled(), !bModel);
		Sub->SetHatchOpen(true);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSubModelLampsTest, "Deep.World.SubModelLamps", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeepSubModelLampsTest::RunTest(const FString& Parameters)
{
	using namespace DeepSubModelTest;
	FSubWorld W;
	UTEST_NOT_NULL(TEXT("Sub spawns"), W.Sub);
	UTEST_NOT_NULL(TEXT("Sub has the model"), W.Model);
	ADeepSubmarine* Sub = W.Sub;
	UDeepHullModel* Model = W.Model;
	Model->ApplySwitch(true);
	UTEST_TRUE(TEXT("Model hull on"), Sub->IsModelHull());
	const FTransform Fit = Model->GetRelativeTransform();
	const FTransform& Frame = Sub->GetActorTransform();

	// Every lamp sits on the model's visible surface: the light and its bulb just outside it (the bulb's far side within
	// LampMaxOutCm), never buried in it, and the bulb touches nothing.
	constexpr double LampMaxOutCm = 10.0;
	FCollisionQueryParams Complex(TEXT("SubLampComplex"), true);
	for (uint8 Index = 0; Index < uint8(DeepHull::ELamp::Count); ++Index)
	{
		const DeepHull::ELamp Lamp = DeepHull::ELamp(Index);
		const TCHAR* Name = DeepHull::ModelLamp(Lamp).Name;
		ULocalLightComponent* Light = Sub->GetLamp(Lamp);
		UStaticMeshComponent* Bulb = Sub->GetBulb(Lamp);
		if (!TestNotNull(FString::Printf(TEXT("%s: light"), Name), Light) || !TestNotNull(FString::Printf(TEXT("%s: bulb"), Name), Bulb))
		{
			continue;
		}
		const FVector Local = Light->GetRelativeLocation();
		TestTrue(FString::Printf(TEXT("%s: placed on the model"), Name), Local.Equals(DeepHull::ModelLampLocation(Lamp, Fit), 0.1));
		TestTrue(FString::Printf(TEXT("%s: bulb at the light"), Name), Bulb->GetComponentLocation().Equals(Light->GetComponentLocation(), 0.1));
		const FVector N = DeepHull::ModelLampNormal(Lamp, Fit);
		const FVector At = Frame.TransformPosition(Local);
		const FVector WorldN = Frame.TransformVectorNoScale(N);
		FHitResult Hit;
		const bool bSurface = Model->LineTraceComponent(Hit, At + WorldN * 50.0, At - WorldN * 50.0, Complex);
		if (!TestTrue(FString::Printf(TEXT("%s: the visible surface is under the lamp"), Name), bSurface))
		{
			continue;
		}
		// How far the light is out from the surface along its normal: the trace starts 50 cm out and meets the surface behind the lamp.
		const double Out = Hit.Distance - 50.0;
		AddInfo(FString::Printf(TEXT("%s at %s: light %.1f cm out of the surface"), Name, *Local.ToCompactString(), Out));
		TestTrue(FString::Printf(TEXT("%s: bulb outside the surface (%.1f cm out, bulb radius %.1f)"), Name, Out, DeepHull::BulbRadiusCm), Out >= DeepHull::BulbRadiusCm);
		TestTrue(FString::Printf(TEXT("%s: within %.0f cm of it (%.1f + %.1f)"), Name, LampMaxOutCm, Out, DeepHull::BulbRadiusCm), Out + DeepHull::BulbRadiusCm <= LampMaxOutCm);
		FHitResult Touch;
		TestFalse(FString::Printf(TEXT("%s: bulb not buried in the hull"), Name),
			Model->SweepComponent(Touch, At, At + WorldN * 0.1, FQuat::Identity, FCollisionShape::MakeSphere(DeepHull::BulbRadiusCm * 0.95), true));
		TestTrue(FString::Printf(TEXT("%s: bulb drawn"), Name), !Bulb->bHiddenInGame);
	}

	// The markers: port red, starboard green, blinking at the tuned rate; their bulbs blink with them.
	const UPointLightComponent* Port = Cast<UPointLightComponent>(Sub->GetLamp(DeepHull::ELamp::MarkerPort));
	const UPointLightComponent* Stbd = Cast<UPointLightComponent>(Sub->GetLamp(DeepHull::ELamp::MarkerStbd));
	UTEST_TRUE(TEXT("Marker lights"), Port && Stbd);
	TestTrue(TEXT("Port marker red #FF2A1A"), Port->LightColor == FColor(0xFF, 0x2A, 0x1A));
	TestTrue(TEXT("Starboard marker green #22D96A"), Stbd->LightColor == FColor(0x22, 0xD9, 0x6A));
	TestEqual(TEXT("Markers blink at 1 Hz"), UDeepTuningSettings::Get().Look.MarkerHz, 1.f);
	TestTrue(TEXT("Port marker on the port side"), Port->GetRelativeLocation().Y < 0.0);
	TestTrue(TEXT("Starboard marker on the starboard side"), Stbd->GetRelativeLocation().Y > 0.0);
	TestTrue(TEXT("Bow lights at the nose"), Sub->GetLamp(DeepHull::ELamp::BowPort)->GetRelativeLocation().X > Model->GetStaticMesh()->GetBoundingBox().TransformBy(Fit).Max.X - 150.0);
	if (!UDeepTuningSettings::Get().Arena.bGreyBox)
	{
		static_cast<AActor*>(Sub)->Tick(0.f);
		TestEqual(TEXT("Port bulb follows its light"), Sub->GetBulb(DeepHull::ELamp::MarkerPort)->IsVisible(), Port->IsVisible());
		TestEqual(TEXT("Starboard bulb follows its light"), Sub->GetBulb(DeepHull::ELamp::MarkerStbd)->IsVisible(), Stbd->IsVisible());
	}
	return true;
}

#endif
