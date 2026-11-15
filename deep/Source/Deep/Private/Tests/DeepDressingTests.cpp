// Deep.World.Dressing: the placement loader (plan 7). Parsing, scatter kept out of the diver's paths, the re-base when
// the nearest stop changes (plan 2's trap), a missing mesh (one warning, nothing drawn), and the cook list.

#include "CoreMinimal.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepCaptainLines.h"
#include "World/DeepArena.h"
#include "World/DeepArenaRoot.h"
#include "World/DeepDressing.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DeepDressingTest
{
	struct FScopedWorld
	{
		UWorld* World = nullptr;

		FScopedWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}

		~FScopedWorld()
		{
			if (World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};

	const TCHAR* Cube = TEXT("/Engine/BasicShapes/Cube.Cube");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepDressingParseTest, "Deep.World.Dressing.Parse",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepDressingParseTest::RunTest(const FString& Parameters)
{
	const FString Csv =
		TEXT("id,stop,kind,parent,ref,mesh,material,x_m,y_m,z_m,z_to_m,yaw,pitch,roll,scale,sink,count,radius_min_m,radius_max_m,scale_min,scale_max,seed\n")
		TEXT("pillar,3,place,root,bed,/Game/DragonCave/Meshes/SM_Column01.SM_Column01,/Game/Deep/Dressing/M_X.M_X,7.5,-2,0.1,,90,4,-3,1.2,0.25,,,,,,\n")
		TEXT("rocks,0,scatter,,,/Game/DragonCave/Meshes/SM_CaveRock01.SM_CaveRock01,,0,0,0,-0.5,0,10,10,1,0.3,24,8,30,0.5,1.5,77\n")
		TEXT("wall,0,place,trench,,/Game/DragonCave/Meshes/SM_LargeRock01.SM_LargeRock01,,-0.5,3,295,,,,,2,,,,,,,\n")
		TEXT("bad,0,spray,root,bed,/Game/X.X,,,,,,,,,,,,,,,,\n")
		TEXT("nomesh,0,place,root,bed,,,,,,,,,,,,,,,,,\n")
		TEXT("badref,0,place,trench,bed,/Game/X.X,,,,,,,,,,,,,,,,\n");
	TArray<FString> Errors;
	const TArray<FDeepDressRow> Rows = DeepDressing::ParseRows(Csv, &Errors);
	UTEST_EQUAL(TEXT("Three good rows"), Rows.Num(), 3);
	TestEqual(TEXT("Three bad rows are described"), Errors.Num(), 3);

	const FDeepDressRow& P = Rows[0];
	TestEqual(TEXT("id"), P.Id, FString(TEXT("pillar")));
	TestEqual(TEXT("stop"), P.Stop, 3);
	TestTrue(TEXT("place on the bed under the root"), P.Kind == EDeepDressKind::Place && P.Parent == EDeepDressParent::Root && P.Ref == EDeepDressRef::Bed);
	TestEqual(TEXT("material"), P.Material, FString(TEXT("/Game/Deep/Dressing/M_X.M_X")));
	TestEqual(TEXT("x"), P.XM, 7.5f);
	TestEqual(TEXT("y"), P.YM, -2.f);
	TestEqual(TEXT("z_to defaults to z"), P.ZToM, 0.1f);
	TestEqual(TEXT("yaw"), P.Yaw, 90.f);
	TestEqual(TEXT("scale"), P.Scale, 1.2f);
	TestEqual(TEXT("sink"), P.Sink, 0.25f);
	TestTrue(TEXT("a bed piece hides with the bed"), P.HidesBelowStop());

	const FDeepDressRow& S = Rows[1];
	TestTrue(TEXT("scatter, defaults to root and bed"), S.Kind == EDeepDressKind::Scatter && S.Parent == EDeepDressParent::Root && S.Ref == EDeepDressRef::Bed);
	TestEqual(TEXT("count"), S.Count, 24);
	TestEqual(TEXT("ring"), FVector2D(S.RadiusMinM, S.RadiusMaxM), FVector2D(8.f, 30.f));
	TestEqual(TEXT("scale range"), FVector2D(S.ScaleMin, S.ScaleMax), FVector2D(0.5f, 1.5f));
	TestEqual(TEXT("seed"), S.Seed, 77);

	const FDeepDressRow& W = Rows[2];
	TestTrue(TEXT("trench rows use the wall"), W.Parent == EDeepDressParent::Trench && W.Ref == EDeepDressRef::Wall);
	TestFalse(TEXT("trench rows do not hide with the bed"), W.HidesBelowStop());
	TestEqual(TEXT("empty cells take defaults"), W.Count, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepDressingScatterTest, "Deep.World.Dressing.Scatter",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepDressingScatterTest::RunTest(const FString& Parameters)
{
	FDeepDressContext Context;
	Context.StopDepthM = 300.f;
	Context.AnchorCm = FVector(0.0, 0.0, -250.0);
	Context.BedBelowM = [](const FVector2D&) { return 10.f; };
	Context.MeshBox = FBox(FVector(-50.0), FVector(50.0));
	// A diver's path from the hatch out along +X and down.
	Context.Clear.Add({ Context.AnchorCm, Context.AnchorCm + FVector(1500.0, 0.0, -1000.0), 200.f });

	FDeepDressRow Row;
	Row.Id = TEXT("rocks");
	Row.Kind = EDeepDressKind::Scatter;
	Row.Ref = EDeepDressRef::Bed;
	Row.Count = 60;
	Row.RadiusMinM = 5.f;
	Row.RadiusMaxM = 15.f;
	Row.ScaleMin = 0.5f;
	Row.ScaleMax = 2.f;
	Row.Sink = 0.5f;
	Row.Seed = 11;

	int32 Rejected = 0;
	const TArray<FTransform> A = DeepDressing::Expand(Row, Context, &Rejected);
	TestEqual(TEXT("Every piece is placed or counted as kept out"), A.Num() + Rejected, Row.Count);
	TestTrue(TEXT("Most pieces find a place off the path"), A.Num() > Row.Count * 3 / 4);
	bool bInRing = true;
	bool bSeated = true;
	bool bClear = true;
	for (const FTransform& T : A)
	{
		const double R = FVector2D(T.GetLocation() - Context.AnchorCm).Size() / UDeepRules::CmPerM;
		bInRing &= R >= 5.0 - 0.01 && R <= 15.0 + 0.01;
		// Yaw only (no pitch or roll in the row): the cube's lowest face is half its height under its centre, and half of it is buried.
		const double Bottom = T.GetLocation().Z - 50.0 * T.GetScale3D().Z;
		const double Bed = Context.AnchorCm.Z - 1000.0;
		bSeated &= FMath::IsNearlyEqual(Bottom, Bed - 0.5 * 100.0 * T.GetScale3D().Z, 0.5);
		bClear &= !DeepDressing::Blocks(T, Context.MeshBox, Context.Clear);
	}
	TestTrue(TEXT("Every piece lies in the ring"), bInRing);
	TestTrue(TEXT("Every piece is sunk half its height into the silt"), bSeated);
	TestTrue(TEXT("No piece stands in the diver's path"), bClear);

	const TArray<FTransform> B = DeepDressing::Expand(Row, Context);
	bool bSame = A.Num() == B.Num();
	for (int32 i = 0; bSame && i < A.Num(); ++i)
	{
		bSame = A[i].Equals(B[i], 0.01);
	}
	TestTrue(TEXT("The same seed gives the same pieces"), bSame);

	// A placed piece in the path is dropped.
	FDeepDressRow Place;
	Place.Id = TEXT("in_the_way");
	Place.Ref = EDeepDressRef::Anchor;
	Place.XM = 7.5f;
	Place.ZM = -5.f;
	int32 Dropped = 0;
	TestEqual(TEXT("A piece placed in the path draws nothing"), DeepDressing::Expand(Place, Context, &Dropped).Num(), 0);
	TestEqual(TEXT("... and is counted"), Dropped, 1);

	// A low piece lying in the silt under the path stays: the bed under it collides anyway.
	FDeepDressRow Low;
	Low.Id = TEXT("bones");
	Low.Ref = EDeepDressRef::Bed;
	Low.XM = 7.5f;
	Low.Scale = 0.4f;
	Low.Sink = 0.5f;
	FDeepDressContext LowContext = Context;
	LowContext.BedBelowM = [](const FVector2D&) { return 5.5f; };
	LowContext.LowCm = 60.f;
	TestEqual(TEXT("A low piece on the bed under a path is kept"), DeepDressing::Expand(Low, LowContext).Num(), 1);
	LowContext.LowCm = 0.f;
	TestEqual(TEXT("... but not if low pieces are not allowed"), DeepDressing::Expand(Low, LowContext).Num(), 0);

	// Shaft rows: the block's inner face stands Out in front of the shaft's wall at that depth, facing the axis.
	FDeepDressContext Shaft = Context;
	Shaft.BedBelowM = [](const FVector2D& P) { return 10.f + 30.f * FMath::Exp(-FMath::Pow(P.Size() / 4.f, 4.f)); };
	Shaft.LowCm = 60.f;
	FDeepDressRow Lining;
	Lining.Id = TEXT("lining");
	Lining.Ref = EDeepDressRef::Shaft;
	Lining.YM = 90.f;
	Lining.ZM = 25.f;
	Lining.XM = 0.2f;
	const TArray<FTransform> Block = DeepDressing::Expand(Lining, Shaft);
	UTEST_EQUAL(TEXT("A lining block against the wall is kept, though it is inside the shaft's capsule"), Block.Num(), 1);
	const double WallR = 4.0 * FMath::Pow(FMath::Loge(2.0), 0.25);
	const FVector Inner = Block[0].TransformPosition(FVector(-50.0, 0.0, 0.0)) - Shaft.AnchorCm;
	TestEqual(TEXT("Its inner face is 0.2 m in front of the wall"), FVector2D(Inner).Size() / 100.0, WallR - 0.2, 0.02);
	TestEqual(TEXT("... on the wall's side at 90 degrees"), Inner.X / 100.0, 0.0, 0.02);
	TestEqual(TEXT("... centred on its depth"), (Block[0].GetLocation().Z - Shaft.AnchorCm.Z) / 100.0, -25.0, 0.02);

	// Depth rows: wherever the root stands for a stop, a point lands at (sub depth - depth) above the sub.
	for (const float SubDepth : { 199.f, 201.f, 450.f })
	{
		for (const float StopDepth : { 100.f, 300.f, 500.f })
		{
			const double RootZ = (SubDepth - StopDepth) * UDeepRules::CmPerM;
			TestEqual(TEXT("Depth rows land at the same place under any stop"), RootZ + DeepDressing::DepthToRootZCm(250.f, StopDepth), (SubDepth - 250.0) * UDeepRules::CmPerM, 0.01);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepDressingRebaseTest, "Deep.World.Dressing.Rebase",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepDressingRebaseTest::RunTest(const FString& Parameters)
{
	using namespace DeepDressingTest;
	FScopedWorld Scoped;
	UWorld* World = Scoped.World;
	ADeepGameMode* GameMode = World->SpawnActor<ADeepGameMode>();
	UTEST_NOT_NULL(TEXT("Game mode"), GameMode);
	ADeepSubmarine* Sub = GameMode->EnsureArena();
	UTEST_NOT_NULL(TEXT("Sub"), Sub);
	ADeepGameState* State = World->GetGameState<ADeepGameState>();
	UTEST_NOT_NULL(TEXT("Game state"), State);
	ADeepDressing* Dressing = nullptr;
	for (TActorIterator<ADeepDressing> It(World); It; ++It)
	{
		Dressing = *It;
	}
	UTEST_NOT_NULL(TEXT("EnsureArena spawns the dressing"), Dressing);

	// A marker by absolute depth (250 m), far off every path, and one on the bed of stop 1 only.
	const FString Csv = FString::Printf(TEXT("id,stop,kind,ref,mesh,x_m,y_m,z_m\nmark,0,place,depth,%s,40,40,250\nbedonly,1,place,bed,%s,40,-40,0\n"), Cube, Cube);
	UTEST_EQUAL(TEXT("Rows loaded"), Dressing->LoadFromString(Csv), 2);
	UHierarchicalInstancedStaticMeshComponent* Mark = Dressing->FindComponentForRow(TEXT("mark"));
	UTEST_NOT_NULL(TEXT("The marker has a component"), Mark);
	TestEqual(TEXT("One piece"), Mark->GetInstanceCount(), 1);
	TestTrue(TEXT("No collision"), Mark->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
	TestTrue(TEXT("Dressing hangs from the arena root"), Dressing->GetAttachParentActor() == DeepArena::FindArenaRoot(World));

	auto MarkAboveSubM = [&]() -> double
	{
		FTransform T;
		Mark->GetInstanceTransform(0, T, /*bWorldSpace*/ true);
		return (T.GetLocation().Z - Sub->GetActorLocation().Z) / UDeepRules::CmPerM;
	};

	State->bParked = false;
	State->SubDepthM = 199.f;
	DeepArena::ApplySubDepth(World);
	TestEqual(TEXT("At 199 m stop 1 is loaded"), State->Stop, 1);
	TestEqual(TEXT("At 199 m the 250 m marker is 51 m below the sub"), MarkAboveSubM(), -51.0, 0.01);
	TestTrue(TEXT("Below the loaded stop the bed rows hide"), !Dressing->FindComponentForRow(TEXT("bedonly"))->IsVisible());

	State->SubDepthM = 201.f;
	DeepArena::ApplySubDepth(World);
	TestEqual(TEXT("At 201 m stop 2 is loaded"), State->Stop, 2);
	TestEqual(TEXT("The dressing was re-placed for stop 2"), Dressing->GetBuiltStop(), 2);
	TestEqual(TEXT("At 201 m the marker is 49 m below the sub: it moved 2 m, it did not jump 200"), MarkAboveSubM(), -49.0, 0.01);
	TestFalse(TEXT("A stop-1 row is hidden at stop 2"), Dressing->FindComponentForRow(TEXT("bedonly"))->IsVisible());

	State->bParked = true;
	State->SubDepthM = 100.f;
	DeepArena::ApplySubDepth(World);
	TestEqual(TEXT("Back at 100 m the marker is 150 m below"), MarkAboveSubM(), -150.0, 0.01);
	TestTrue(TEXT("Parked at stop 1 its bed rows show"), Dressing->FindComponentForRow(TEXT("bedonly"))->IsVisible());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepDressingMissingMeshTest, "Deep.World.Dressing.MissingMesh",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepDressingMissingMeshTest::RunTest(const FString& Parameters)
{
	using namespace DeepDressingTest;
	FScopedWorld Scoped;
	ADeepDressing* Dressing = Scoped.World->SpawnActor<ADeepDressing>();
	UTEST_NOT_NULL(TEXT("Dressing"), Dressing);
	AddExpectedMessagePlain(TEXT("DEEPDRESS mesh missing"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
	const FString Csv = FString::Printf(
		TEXT("id,kind,ref,mesh,x_m,y_m,z_m\n")
		TEXT("gone1,place,anchor,/Game/NotInstalled/SM_Gone.SM_Gone,30,30,-5\n")
		TEXT("gone2,place,anchor,/Game/NotInstalled/SM_Gone.SM_Gone,31,30,-5\n")
		TEXT("here,place,anchor,%s,30,-30,-5\n"), Cube);
	TestEqual(TEXT("All three rows parse"), Dressing->LoadFromString(Csv), 3);
	TestEqual(TEXT("One mesh is missing"), Dressing->GetMissingMeshes().Num(), 1);
	TestNull(TEXT("The missing mesh gets no component"), Dressing->FindComponentForRow(TEXT("gone2")));
	TestEqual(TEXT("Only the installed mesh gets a component"), Dressing->GetComponentCount(), 1);
	TestEqual(TEXT("... and draws its one piece"), Dressing->GetInstanceCount(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepDressingCookTest, "Deep.World.Dressing.CookList",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepDressingCookTest::RunTest(const FString& Parameters)
{
	const FString Dressing =
		TEXT("id,kind,mesh,material\n")
		TEXT("a,place,/Game/DragonCave/Meshes/SM_Column01.SM_Column01,/Game/Deep/Dressing/M_Rock.M_Rock\n")
		TEXT("b,scatter,/Game/DragonCave/Meshes/SM_Column01.SM_Column01,\n")
		TEXT("c,place,/Engine/BasicShapes/Cube.Cube,\n");
	const FString Pack =
		TEXT("\"package\",\"why\"\n")
		TEXT("\"/Game/DragonCave/Meshes/SM_DragonPose01\",\"the serpent's head\"\n")
		TEXT("\"/Game/DragonCave/Meshes/SM_Column01.SM_Column01\",\"listed twice\"\n");
	const TArray<FName> Packages = DeepDressing::PackagesToCook(Dressing, Pack);
	TestEqual(TEXT("Three packages, no engine content, no duplicates"), Packages.Num(), 3);
	TestTrue(TEXT("Object paths become package names"), Packages.Contains(FName(TEXT("/Game/DragonCave/Meshes/SM_Column01"))));
	TestTrue(TEXT("Lane materials are listed"), Packages.Contains(FName(TEXT("/Game/Deep/Dressing/M_Rock"))));
	TestTrue(TEXT("PackAssets.csv rows are listed"), Packages.Contains(FName(TEXT("/Game/DragonCave/Meshes/SM_DragonPose01"))));

	// The shipped files parse, and every listed package is a long package name.
	FString ShippedDressing;
	FString ShippedPack;
	FFileHelper::LoadFileToString(ShippedDressing, *DeepData::DataPath(UDeepTuningSettings::Get().Look.DressingFile));
	FFileHelper::LoadFileToString(ShippedPack, *DeepData::DataPath(TEXT("PackAssets.csv")));
	for (const FName& Package : DeepDressing::PackagesToCook(ShippedDressing, ShippedPack))
	{
		TestTrue(*FString::Printf(TEXT("%s is a package name"), *Package.ToString()), FPackageName::IsValidLongPackageName(Package.ToString()));
		AddInfo(FString::Printf(TEXT("DEEPCOOK would add %s"), *Package.ToString()));
	}
	return true;
}

#endif
