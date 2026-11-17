// BT-01 on the hull (World/DeepHull, UDeepHullModel): the mesh loads with Nanite, fits the cave tool's 12.3 x 3.3 x 5.0 m
// box, its belly hatch sits on the hatch column, it has no collision, and Deep.Sub.Model swaps it with the old dress.

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/BodySetup.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepHull.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSubModelTest, "Deep.World.SubModel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeepSubModelTest::RunTest(const FString& Parameters)
{
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	const double M = UDeepRules::CmPerM;
	const FVector Outer((T.RoomHalfLengthM + T.WallThicknessM) * M, (T.RoomHalfWidthM + T.WallThicknessM) * M, (T.RoomHeightM * 0.5 + T.WallThicknessM) * M);

	// The mesh: loads, Nanite, no simple collision of its own.
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, DeepHull::ModelMesh);
	UTEST_NOT_NULL(TEXT("BT-01 loads"), Mesh);
#if WITH_EDITOR
	TestTrue(TEXT("Nanite on"), Mesh->IsNaniteEnabled());
#endif
	if (const UBodySetup* Body = Mesh->GetBodySetup())
	{
		TestEqual(TEXT("No simple collision on the mesh"), Body->AggGeom.GetElementCount(), 0);
	}
	TestTrue(TEXT("Deep.Sub.Model defaults to the model"), DeepHull::WantsModel());

	// The fit: one scale, inside the envelope, as long as the envelope allows, the hatch on the column at the belly.
	const FBox MeshBox = Mesh->GetBoundingBox();
	const FTransform Fit = DeepHull::ModelTransform(MeshBox, Outer);
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
	TestTrue(TEXT("Hatch over the hatch column at the belly"), Hatch.Equals(FVector(0.0, 0.0, -Outer.Z), 0.5));
	const FBox Placed = MeshBox.TransformBy(Fit);
	TestTrue(TEXT("Model's bottom is the belly: nothing in the hatch column below"), FMath::IsNearlyEqual(Placed.Min.Z, -Outer.Z, 0.5));

	// On the sub: a component with no collision, placed by the fit; the switch hides one dress or the other.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ADeepSubmarine* Sub = World->SpawnActor<ADeepSubmarine>();
	bool bOk = TestNotNull(TEXT("Sub spawns"), Sub);
	UDeepHullModel* Model = Sub ? Sub->FindComponentByClass<UDeepHullModel>() : nullptr;
	bOk = bOk && TestNotNull(TEXT("Sub has the model"), Model);
	if (bOk)
	{
		TestTrue(TEXT("Model mesh"), Model->GetStaticMesh() == Mesh);
		TestTrue(TEXT("Model has no collision"), Model->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
		TestFalse(TEXT("Model makes no overlaps"), Model->GetGenerateOverlapEvents());
		TestTrue(TEXT("Model placed by the fit"), Model->GetRelativeTransform().Equals(Fit, 0.01));

		Model->ApplySwitch(true);
		TestFalse(TEXT("1: model shown"), Model->bHiddenInGame);
		int32 Solid = 0;
		for (const UStaticMeshComponent* Shape : Sub->GetHullDress())
		{
			TestTrue(TEXT("1: dress hidden"), Shape->bHiddenInGame);
		}
		TInlineComponentArray<UStaticMeshComponent*> Meshes(Sub);
		for (const UStaticMeshComponent* Shape : Meshes)
		{
			if (Shape->IsCollisionEnabled() && Shape->GetFName() != TEXT("HatchCover"))
			{
				++Solid;
				TestTrue(FString::Printf(TEXT("1: %s (collider) hidden but solid"), *Shape->GetName()), Shape->bHiddenInGame);
			}
		}
		TestTrue(TEXT("The box stays the collider"), Solid > 0);

		Model->ApplySwitch(false);
		TestTrue(TEXT("0: model hidden"), Model->bHiddenInGame);
		for (const UStaticMeshComponent* Shape : Sub->GetHullDress())
		{
			TestFalse(TEXT("0: dress shown"), Shape->bHiddenInGame);
		}
	}
	World->DestroyWorld(false);
	return true;
}

#endif
