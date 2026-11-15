// The submarine's outside dress (World/DeepHull): visual only, tight to the colliding box, the hatch column open, and
// the trench seen through the window only while the sub moves.

#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepHull.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSubHullDressTest, "Deep.World.SubHullDress", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeepSubHullDressTest::RunTest(const FString& Parameters)
{
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const FDeepSubTuning& T = Tuning.Sub;
	const double M = UDeepRules::CmPerM;
	const FVector Outer((T.RoomHalfLengthM + T.WallThicknessM) * M, (T.RoomHalfWidthM + T.WallThicknessM) * M, (T.RoomHeightM * 0.5 + T.WallThicknessM) * M);
	const double Hh = T.HatchHalfSizeM * M;
	const double AnchorZ = -Tuning.Arena.AnchorBelowSubM * M;
	const TArray<FDeepHullPart> Parts = DeepHull::Parts(Outer, Hh, AnchorZ);
	UTEST_TRUE(TEXT("Some parts"), Parts.Num() > 50);

	// Proud of the colliding box (hull and tower) by at most 60 cm where a diver passes; the bow (in front of the front
	// face) may reach as far forward as the nose (3 m) but not out to the sides. The bridle hangs to the anchor.
	const ADeepSubmarine* Sub = GetDefault<ADeepSubmarine>();
	// The tower: 3 x 1.4 x 2.2 m on the ceiling from x -0.5 m (ADeepSubmarine::LayOut).
	const FBox Solid = FBox(-Outer, Outer) + FBox(FVector(-50.0, -70.0, Outer.Z), FVector(250.0, 70.0, Outer.Z + 220.0));
	const FBox Tight = Solid.ExpandBy(60.0);
	// The hatch column below the deck, down to just over the anchor, shrunk by 1 cm.
	const FBox Column(FVector(-Hh + 1.0, -Hh + 1.0, AnchorZ + 20.0), FVector(Hh - 1.0, Hh - 1.0, -Outer.Z - 1.0));
	TSet<FName> Names;
	for (const FDeepHullPart& Part : Parts)
	{
		bool bDup = false;
		Names.Add(Part.Name, &bDup);
		TestFalse(FString::Printf(TEXT("%s: unique name"), *Part.Name.ToString()), bDup);
		const FBox Box = DeepHull::PartBox(Part);
		const bool bBow = Box.Min.X >= Outer.X - 1.0;
		const FBox Allowed = bBow ? FBox(Tight.Min, Tight.Max + FVector(330.0, 0.0, 0.0)) : Tight;
		if (Part.bInHatchColumn)
		{
			TestTrue(FString::Printf(TEXT("%s %s: no lower than the anchor"), *Part.Name.ToString(), *Box.ToString()), Box.Min.Z >= AnchorZ - 10.0);
		}
		else
		{
			TestTrue(FString::Printf(TEXT("%s %s: within 60 cm of the hull"), *Part.Name.ToString(), *Box.ToString()), Allowed.IsInside(Box.Min) && Allowed.IsInside(Box.Max));
			TestFalse(FString::Printf(TEXT("%s %s: clear of the hatch column"), *Part.Name.ToString(), *Box.ToString()), Box.Intersect(Column));
		}
	}

	// The sub's dress components match the list and none collides.
	UTEST_EQUAL(TEXT("One component per part"), Sub->GetHullDress().Num(), Parts.Num());
	for (const UStaticMeshComponent* Shape : Sub->GetHullDress())
	{
		TestFalse(FString::Printf(TEXT("%s has no collision"), *Shape->GetName()), Shape->IsCollisionEnabled());
	}

	// The window shows the trench while the sub moves, never to a diver outside, and not while parked.
	TestTrue(TEXT("Helm camera: always"), ADeepSubmarine::TrenchShows(true, false, true));
	TestTrue(TEXT("Room, moving: through the window"), ADeepSubmarine::TrenchShows(false, true, false));
	TestFalse(TEXT("Room, parked: open water"), ADeepSubmarine::TrenchShows(false, true, true));
	TestFalse(TEXT("Outside: never"), ADeepSubmarine::TrenchShows(false, false, false));
	return true;
}

#endif
