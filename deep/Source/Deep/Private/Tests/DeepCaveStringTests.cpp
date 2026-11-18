// Tests for the cave string's pure rules (Sim/DeepCaveStringRules): the Content/Data tables parsed, the joints chained,
// the chunks composed as cavestring.string_places does, and the hull's pose between the game state and the string.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Sim/DeepCaveStringRules.h"
#include "Sim/DeepTuningSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_CAVESTRING_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepCaveStringTest
{
	// CaveChunks.csv as baked (Nov 17): the rows the composition is checked against.
	const TCHAR* Chunks =
		TEXT("Name,Mesh,Type,InType,OutType,ExitX,ExitY,ExitZ,ExitYaw,MinX,MinY,MinZ,MaxX,MaxY,MaxZ\r\n")
		TEXT("S,/Game/S.S,start,,rift,4200,0,-250,0,-3045.9,-2542.6,-957.9,4200,1916.1,3100.4\r\n")
		TEXT("A,/Game/A.A,tunnel,rift,key,3000,0,0,0,0,-611.2,-1278,3000,703.9,1449.7\r\n")
		TEXT("B,/Game/B.B,cavern,rift,bed,3000,0,0,0,0,-1593.5,-711.9,3000,1679.4,1844.7\r\n")
		TEXT("T1,/Game/T1.T1,tunnel,key,bed,5300,4800,-600,90,0,-1095.6,-1308.9,6315.9,4800,1659.5\r\n")
		TEXT("C1,/Game/C1.C1,stop,bed,rift,4600,-2771.281,-2400,-60,0,-3003.2,-3107.9,5277.3,1830.3,2311.1\r\n")
		TEXT("E,/Game/E.E,end,bed,,0,0,0,0,0,-1016.9,-481.5,2574.5,1052.7,642.4\r\n");

	const TCHAR* Sockets =
		TEXT("Chunk,Socket,X,Y,Z,Yaw\n")
		TEXT("S,Park,0,0,0,0\n")
		TEXT("S,Salvage1,1050,800,-539,0\n")
		TEXT("C1,Park,3000,0,-2400,0\n");

	TArray<FName> String()
	{
		return FDeepCaveStringTuning().String;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveStringParseTest, "Deep.Sim.CaveString.Parse", DEEP_CAVESTRING_TEST_FLAGS)
bool FDeepCaveStringParseTest::RunTest(const FString& Parameters)
{
	TArray<FDeepCaveChunkRow> Rows;
	FString Error;
	TestTrue(TEXT("The chunk table parses"), UDeepCaveStringRules::ParseChunks(DeepCaveStringTest::Chunks, Rows, Error));
	TestEqual(TEXT("Six chunks"), Rows.Num(), 6);
	if (Rows.Num() == 6)
	{
		TestEqual(TEXT("S first"), Rows[0].Name, FName(TEXT("S")));
		TestTrue(TEXT("S has no entry joint (an empty cell)"), Rows[0].InType.IsNone());
		TestEqual(TEXT("S's exit joint"), Rows[0].OutType, FName(TEXT("rift")));
		TestEqual(TEXT("S's mesh path"), Rows[0].Mesh, FString(TEXT("/Game/S.S")));
		TestEqual(TEXT("T1's exit"), Rows[3].ExitCm, FVector(5300.0, 4800.0, -600.0));
		TestEqual(TEXT("T1's exit yaw"), Rows[3].ExitYawDeg, 90.f);
		TestEqual(TEXT("C1's exit Y (decimals)"), Rows[4].ExitCm.Y, -2771.281, 0.001);
		TestEqual(TEXT("S's bounds min"), Rows[0].MinCm, FVector(-3045.9, -2542.6, -957.9));
		TestTrue(TEXT("E has no exit joint (a trailing empty cell)"), Rows[5].OutType.IsNone());
	}

	// The columns are found by name, in any order.
	TArray<FDeepCaveSocketRow> SocketRows;
	TestTrue(TEXT("The socket table parses"), UDeepCaveStringRules::ParseSockets(DeepCaveStringTest::Sockets, SocketRows, Error));
	TestEqual(TEXT("Three sockets"), SocketRows.Num(), 3);
	const FDeepCaveSocketRow* Park = UDeepCaveStringRules::FindSocket(SocketRows, FName(TEXT("C1")), FName(TEXT("Park")));
	TestTrue(TEXT("C1's park found"), Park && Park->LocalCm.Equals(FVector(3000.0, 0.0, -2400.0)));
	TestNull(TEXT("No such socket"), UDeepCaveStringRules::FindSocket(SocketRows, FName(TEXT("B")), FName(TEXT("Park"))));

	TArray<FDeepCavePathPoint> Path;
	TestTrue(TEXT("The path parses (columns out of order)"), UDeepCaveStringRules::ParsePath(TEXT("Region,Chunk,X,Y,Z,Yaw\nneck,A,10,-2.5,3,0\nsqueeze,T1,4000,2000,-300,45.5\n"), Path, Error));
	TestEqual(TEXT("Two samples"), Path.Num(), 2);
	if (Path.Num() == 2)
	{
		TestEqual(TEXT("Chunk"), Path[1].Chunk, FName(TEXT("T1")));
		TestEqual(TEXT("Region"), Path[1].Region, FName(TEXT("squeeze")));
		TestEqual(TEXT("Local point"), Path[0].LocalCm, FVector(10.0, -2.5, 3.0));
		TestEqual(TEXT("Yaw"), Path[1].YawDeg, 45.5f);
	}

	// What does not parse says why.
	TestFalse(TEXT("A missing column fails"), UDeepCaveStringRules::ParseSockets(TEXT("Chunk,Socket,X,Y,Z\nS,Park,0,0,0\n"), SocketRows, Error));
	TestTrue(TEXT("... and names it"), Error.Contains(TEXT("Yaw")));
	TestFalse(TEXT("A short row fails"), UDeepCaveStringRules::ParseSockets(TEXT("Chunk,Socket,X,Y,Z,Yaw\nS,Park,0,0\n"), SocketRows, Error));
	TestFalse(TEXT("An empty table fails"), UDeepCaveStringRules::ParseChunks(TEXT(""), Rows, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveStringJointsTest, "Deep.Sim.CaveString.Joints", DEEP_CAVESTRING_TEST_FLAGS)
bool FDeepCaveStringJointsTest::RunTest(const FString& Parameters)
{
	TArray<FDeepCaveChunkRow> Rows;
	FString Error;
	UDeepCaveStringRules::ParseChunks(DeepCaveStringTest::Chunks, Rows, Error);
	TestEqual(TEXT("S, A, T1, C1, B, E chains"), UDeepCaveStringRules::CheckJoints(Rows, DeepCaveStringTest::String()), FString());

	const auto Names = [](std::initializer_list<const TCHAR*> List)
	{
		TArray<FName> Out;
		for (const TCHAR* Name : List)
		{
			Out.Add(FName(Name));
		}
		return Out;
	};
	TestFalse(TEXT("A after T1 does not (bed into rift)"), UDeepCaveStringRules::CheckJoints(Rows, Names({ TEXT("S"), TEXT("A"), TEXT("T1"), TEXT("A") })).IsEmpty());
	TestFalse(TEXT("Not starting at a start"), UDeepCaveStringRules::CheckJoints(Rows, Names({ TEXT("A"), TEXT("T1"), TEXT("C1"), TEXT("B"), TEXT("E") })).IsEmpty());
	TestFalse(TEXT("Not ending at an end"), UDeepCaveStringRules::CheckJoints(Rows, Names({ TEXT("S"), TEXT("A"), TEXT("T1"), TEXT("C1") })).IsEmpty());
	TestFalse(TEXT("An unknown chunk"), UDeepCaveStringRules::CheckJoints(Rows, Names({ TEXT("S"), TEXT("Q"), TEXT("E") })).IsEmpty());
	TestFalse(TEXT("An empty string"), UDeepCaveStringRules::CheckJoints(Rows, TArray<FName>()).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveStringComposeTest, "Deep.Sim.CaveString.Compose", DEEP_CAVESTRING_TEST_FLAGS)
bool FDeepCaveStringComposeTest::RunTest(const FString& Parameters)
{
	TArray<FDeepCaveChunkRow> Rows;
	TArray<FDeepCaveSocketRow> Sockets;
	FString Error;
	UDeepCaveStringRules::ParseChunks(DeepCaveStringTest::Chunks, Rows, Error);
	UDeepCaveStringRules::ParseSockets(DeepCaveStringTest::Sockets, Sockets, Error);
	const FDeepCaveSocketRow* SPark = UDeepCaveStringRules::FindSocket(Sockets, FName(TEXT("S")), FName(TEXT("Park")));
	const FDeepCaveSocketRow* C1Park = UDeepCaveStringRules::FindSocket(Sockets, FName(TEXT("C1")), FName(TEXT("Park")));
	if (!TestNotNull(TEXT("S's park"), SPark) || !TestNotNull(TEXT("C1's park"), C1Park))
	{
		return false;
	}

	TArray<FDeepCavePlace> Places;
	TestTrue(TEXT("The string composes"), UDeepCaveStringRules::ComposeString(Rows, DeepCaveStringTest::String(), *SPark, Places, Error));
	TestEqual(TEXT("One place per chunk"), Places.Num(), 6);

	// The lane note's table (cavestring.string_places, moved so S's berth is the origin).
	struct FExpect
	{
		const TCHAR* Chunk;
		FVector At;
		float Yaw;
	};
	const FExpect Expected[] = {
		{ TEXT("S"), FVector(0.0, 0.0, 0.0), 0.f },
		{ TEXT("A"), FVector(4200.0, 0.0, -250.0), 0.f },
		{ TEXT("T1"), FVector(7200.0, 0.0, -250.0), 0.f },
		{ TEXT("C1"), FVector(12500.0, 4800.0, -850.0), 90.f },
		{ TEXT("B"), FVector(15271.281, 9400.0, -3250.0), 30.f },
		{ TEXT("E"), FVector(17869.357, 10900.0, -3250.0), 30.f },
	};
	for (const FExpect& Expect : Expected)
	{
		const FDeepCavePlace* Place = UDeepCaveStringRules::FindPlace(Places, FName(Expect.Chunk));
		if (TestNotNull(FString::Printf(TEXT("%s placed"), Expect.Chunk), Place))
		{
			TestTrue(FString::Printf(TEXT("%s at %s (got %s)"), Expect.Chunk, *Expect.At.ToString(), *Place->LocationCm.ToString()), Place->LocationCm.Equals(Expect.At, 0.01));
			TestEqual(FString::Printf(TEXT("%s's yaw"), Expect.Chunk), float(FRotator::NormalizeAxis(Place->YawDeg)), Expect.Yaw, 0.001f);
		}
	}

	// The joints meet: each chunk's exit lands on the next one's entry.
	for (int32 Index = 0; Index + 1 < Places.Num(); ++Index)
	{
		const FDeepCaveChunkRow* Row = Rows.FindByPredicate([&](const FDeepCaveChunkRow& R) { return R.Name == Places[Index].Chunk; });
		const FDeepCavePose Exit = UDeepCaveStringRules::ToString(Places[Index], Row->ExitCm, Row->ExitYawDeg);
		TestTrue(FString::Printf(TEXT("%s meets %s"), *Places[Index].Chunk.ToString(), *Places[Index + 1].Chunk.ToString()), Exit.LocationCm.Equals(Places[Index + 1].LocationCm, 0.01));
		TestEqual(TEXT("... facing the same way"), float(FRotator::NormalizeAxis(Exit.YawDeg - Places[Index + 1].YawDeg)), 0.f, 0.001f);
	}

	// C1's berth: 32.5 m below S's, turned a quarter.
	const FDeepCavePlace* C1 = UDeepCaveStringRules::FindPlace(Places, FName(TEXT("C1")));
	const FDeepCavePose Berth = UDeepCaveStringRules::ToString(*C1, C1Park->LocalCm, C1Park->YawDeg);
	TestTrue(TEXT("C1's berth"), Berth.LocationCm.Equals(FVector(12500.0, 7800.0, -3250.0), 0.01));
	TestEqual(TEXT("C1's berth faces +Y"), float(FRotator::NormalizeAxis(Berth.YawDeg)), 90.f, 0.001f);
	TestTrue(TEXT("ToChunk inverts ToString"), UDeepCaveStringRules::ToChunk(*C1, Berth.LocationCm).Equals(C1Park->LocalCm, 0.01));

	// Rebased at C1's berth instead: C1's berth is the origin facing +X, and S lies behind and above.
	TArray<FDeepCavePlace> AtC1;
	TestTrue(TEXT("Rebased at C1's berth"), UDeepCaveStringRules::ComposeString(Rows, DeepCaveStringTest::String(), *C1Park, AtC1, Error));
	const FDeepCavePlace* C1b = UDeepCaveStringRules::FindPlace(AtC1, FName(TEXT("C1")));
	const FDeepCavePose BerthB = UDeepCaveStringRules::ToString(*C1b, C1Park->LocalCm, C1Park->YawDeg);
	TestTrue(TEXT("... at the origin"), BerthB.LocationCm.Equals(FVector::ZeroVector, 0.01));
	TestEqual(TEXT("... facing +X"), float(FRotator::NormalizeAxis(BerthB.YawDeg)), 0.f, 0.001f);
	TestEqual(TEXT("... S 32.5 m above"), UDeepCaveStringRules::FindPlace(AtC1, FName(TEXT("S")))->LocationCm.Z, 3250.0, 0.01);

	// A berth outside the string, or a broken string, does not compose.
	FDeepCaveSocketRow Stray = *SPark;
	Stray.Chunk = FName(TEXT("Q"));
	TestFalse(TEXT("A berth outside the string"), UDeepCaveStringRules::ComposeString(Rows, DeepCaveStringTest::String(), Stray, Places, Error));
	TArray<FName> Broken = DeepCaveStringTest::String();
	Broken.Swap(1, 2);
	TestFalse(TEXT("A string whose joints do not chain"), UDeepCaveStringRules::ComposeString(Rows, Broken, *SPark, Places, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveStringHullTest, "Deep.Sim.CaveString.Hull", DEEP_CAVESTRING_TEST_FLAGS)
bool FDeepCaveStringHullTest::RunTest(const FString& Parameters)
{
	// Parked at the top stop with no offset: the berth.
	TestTrue(TEXT("At the top stop: the origin"), UDeepCaveStringRules::HullCm(FVector2D::ZeroVector, 100.f, 100.f).Equals(FVector::ZeroVector));
	// 12.5 m ahead, 7.8 m to starboard, 32.5 m down: C1's berth.
	TestTrue(TEXT("Offset and depth"), UDeepCaveStringRules::HullCm(FVector2D(125.f, 78.f), 132.5f, 100.f).Equals(FVector(12500.0, 7800.0, -3250.0), 0.01));
	FVector2D Offset;
	float Depth = 0.f;
	UDeepCaveStringRules::HullToState(FVector(12500.0, 7800.0, -3250.0), 100.f, Offset, Depth);
	TestTrue(TEXT("HullToState inverts it (offset)"), Offset.Equals(FVector2D(125.f, 78.f), 0.001f));
	TestEqual(TEXT("HullToState inverts it (depth)"), Depth, 132.5f, 0.001f);
	TestTrue(TEXT("Heave is positive down"), UDeepCaveStringRules::VelocityMps(FVector2D(1.f, -2.f), 0.5f).Equals(FVector(1.0, -2.0, -0.5)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveStringPathTest, "Deep.Sim.CaveString.Path", DEEP_CAVESTRING_TEST_FLAGS)
bool FDeepCaveStringPathTest::RunTest(const FString& Parameters)
{
	TArray<FDeepCavePlace> Places;
	Places.Add({ FName(TEXT("S")), FVector::ZeroVector, 0.f });
	Places.Add({ FName(TEXT("A")), FVector(4200.0, 0.0, -250.0), 0.f });
	Places.Add({ FName(TEXT("C1")), FVector(12500.0, 4800.0, -850.0), 90.f });

	TArray<FDeepCavePathPoint> Path;
	FString Error;
	UDeepCaveStringRules::ParsePath(
		TEXT("Chunk,X,Y,Z,Yaw,Region\n")
		TEXT("S,0,0,0,0,cavern\n")
		TEXT("S,3000,0,0,0,cavern\n")
		TEXT("S,4200,0,-250,0,neck\n")
		TEXT("A,1000,0,0,0,tunnel\n")
		TEXT("A,2000,0,0,0,tunnel\n")
		TEXT("A,3000,0,0,0,tunnel\n")
		TEXT("Q,0,0,0,0,tunnel\n")
		TEXT("C1,100,0,0,10,cavern\n"),
		Path, Error);
	const TArray<FDeepCavePose> Poses = UDeepCaveStringRules::PathPoses(Places, Path);
	TestEqual(TEXT("An unknown chunk's sample is skipped"), Poses.Num(), 7);
	TestTrue(TEXT("A's samples in the string"), Poses[3].LocationCm.Equals(FVector(5200.0, 0.0, -250.0)));
	TestTrue(TEXT("C1's turned a quarter"), Poses[6].LocationCm.Equals(FVector(12500.0, 4900.0, -850.0), 0.01));
	TestEqual(TEXT("... and its yaw added"), Poses[6].YawDeg, 100.f, 0.001f);

	const TArray<float> Metres = UDeepCaveStringRules::PathMetres(Poses);
	TestEqual(TEXT("Starts at 0 m"), Metres[0], 0.f);
	TestEqual(TEXT("30 m on"), Metres[1], 30.f, 0.01f);
	TestEqual(TEXT("52 m to A's 1000"), Metres[3], 30.f + FMath::Sqrt(1200.f * 1200.f + 250.f * 250.f) / 100.f + 10.f, 0.01f);

	// Goto by metres, by chunk (plus metres), by region.
	TArray<FDeepCavePathPoint> Known = Path;
	Known.RemoveAt(6);
	TestEqual(TEXT("By metres"), UDeepCaveStringRules::FindPathSample(Known, Metres, TEXT("30"), 0.f), 1);
	TestEqual(TEXT("Past the end: the last"), UDeepCaveStringRules::FindPathSample(Known, Metres, TEXT("9999"), 0.f), Metres.Num() - 1);
	TestEqual(TEXT("By chunk"), UDeepCaveStringRules::FindPathSample(Known, Metres, TEXT("A"), 0.f), 3);
	TestEqual(TEXT("By chunk, case aside"), UDeepCaveStringRules::FindPathSample(Known, Metres, TEXT("c1"), 0.f), 6);
	TestEqual(TEXT("By chunk and metres"), UDeepCaveStringRules::FindPathSample(Known, Metres, TEXT("A"), 10.f), 4);
	TestEqual(TEXT("By region: the middle of the first run"), UDeepCaveStringRules::FindPathSample(Known, Metres, TEXT("tunnel"), 0.f), 4);
	TestEqual(TEXT("Unknown"), UDeepCaveStringRules::FindPathSample(Known, Metres, TEXT("squeeze"), 0.f), int32(INDEX_NONE));
	return true;
}

#endif
