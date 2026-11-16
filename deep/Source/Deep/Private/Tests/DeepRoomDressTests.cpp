// The room's dressing (World/DeepRoomDress): enough pieces, all inside the room, none in a walk, the hatch or the helm.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepRoomDress.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DeepRoomDressTest
{
#define DEEP_ROOMDRESS_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	constexpr double WalkerRadiusCm = 35.0;  // the diver's capsule

	FString Describe(const FDeepRoomPiece& Piece, int32 Index)
	{
		return FString::Printf(TEXT("piece %d at (%.0f, %.0f, %.0f) size (%.0f, %.0f, %.0f)"), Index, Piece.Centre.X, Piece.Centre.Y, Piece.Centre.Z,
			Piece.Size.X, Piece.Size.Y, Piece.Size.Z);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRoomDressCountTest, "Deep.World.RoomDress.Count", DEEP_ROOMDRESS_FLAGS)
bool FDeepRoomDressCountTest::RunTest(const FString& Parameters)
{
	const FDeepSubTuning& Sub = UDeepTuningSettings::Get().Sub;
	const TArray<FDeepRoomPiece> Pieces = DeepRoomDress::Pieces(Sub);
	AddInfo(FString::Printf(TEXT("DEEPROOMDRESS pieces=%d lights=%d"), Pieces.Num(), DeepRoomDress::Lights(Sub).Num()));
	TestTrue(TEXT("about a hundred pieces or more (a cramped room)"), Pieces.Num() >= 90);
	TestTrue(TEXT("not a junk pile (frame time)"), Pieces.Num() <= 220);

	int32 PerMat[int32(EDeepRoomMat::Count)] = {};
	for (const FDeepRoomPiece& Piece : Pieces)
	{
		++PerMat[int32(Piece.Mat)];
	}
	for (int32 Mat = 0; Mat < int32(EDeepRoomMat::Count); ++Mat)
	{
		TestTrue(*FString::Printf(TEXT("material %d is used"), Mat), PerMat[Mat] > 0);
	}

	// Two practicals, two emergency lights, one strip: light comes from sources, the rest stays dark.
	int32 Kinds[3] = {};
	for (const FDeepRoomLight& Light : DeepRoomDress::Lights(Sub))
	{
		++Kinds[int32(Light.Kind)];
	}
	TestEqual(TEXT("amber practicals"), Kinds[0], 2);
	TestEqual(TEXT("red emergency lights"), Kinds[1], 2);
	TestEqual(TEXT("cool strips"), Kinds[2], 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRoomDressInsideTest, "Deep.World.RoomDress.InsideHull", DEEP_ROOMDRESS_FLAGS)
bool FDeepRoomDressInsideTest::RunTest(const FString& Parameters)
{
	const FDeepSubTuning& Sub = UDeepTuningSettings::Get().Sub;
	const FBox Room = DeepRoomDress::RoomInside(Sub).ExpandBy(0.5);
	const TArray<FDeepRoomPiece> Pieces = DeepRoomDress::Pieces(Sub);
	int32 Outside = 0;
	for (int32 Index = 0; Index < Pieces.Num(); ++Index)
	{
		if (!Room.IsInside(DeepRoomDress::PieceBox(Pieces[Index])))
		{
			++Outside;
			AddError(TEXT("outside the room: ") + DeepRoomDressTest::Describe(Pieces[Index], Index));
		}
	}
	TestEqual(TEXT("pieces outside the room"), Outside, 0);
	for (const FDeepRoomLight& Light : DeepRoomDress::Lights(Sub))
	{
		TestTrue(TEXT("light inside the room"), Room.IsInsideOrOn(Light.Centre));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRoomDressClearTest, "Deep.World.RoomDress.ClearZones", DEEP_ROOMDRESS_FLAGS)
bool FDeepRoomDressClearTest::RunTest(const FString& Parameters)
{
	const UDeepTuningSettings& T = UDeepTuningSettings::Get();
	const FDeepSubTuning& Sub = T.Sub;
	const FDeepRoomDressTuning& Dress = T.RoomDress;
	const TArray<FDeepRoomPiece> Pieces = DeepRoomDress::Pieces(Sub);
	int32 Blocked = 0;
	for (int32 Index = 0; Index < Pieces.Num(); ++Index)
	{
		const FBox Box = DeepRoomDress::PieceBox(Pieces[Index]);
		const FString What = DeepRoomDressTest::Describe(Pieces[Index], Index);
		if (DeepRoomDress::BlocksWalk(Box, Sub, Dress, DeepRoomDressTest::WalkerRadiusCm))
		{
			++Blocked;
			AddError(TEXT("in a walk: ") + What);
		}
		if (DeepRoomDress::InHatchZone(Box, Sub, Dress))
		{
			++Blocked;
			AddError(TEXT("in the hatch zone: ") + What);
		}
		if (DeepRoomDress::InHelmZone(Box, Sub, Dress))
		{
			++Blocked;
			AddError(TEXT("in the helm zone: ") + What);
		}
	}
	TestEqual(TEXT("pieces in a clear zone"), Blocked, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepRoomDressChecksBiteTest, "Deep.World.RoomDress.ChecksBite", DEEP_ROOMDRESS_FLAGS)
bool FDeepRoomDressChecksBiteTest::RunTest(const FString& Parameters)
{
	// The clear-zone checks catch a crate dropped in each zone, and let one through above head height.
	const UDeepTuningSettings& T = UDeepTuningSettings::Get();
	const FDeepSubTuning& Sub = T.Sub;
	const FDeepRoomDressTuning& Dress = T.RoomDress;
	const double Deck = DeepRoomDress::RoomInside(Sub).Min.Z;
	auto Crate = [Deck](const FVector2D& At, double Up = 0.0)
	{
		return FBox(FVector(At.X - 15.0, At.Y - 15.0, Deck + Up), FVector(At.X + 15.0, At.Y + 15.0, Deck + Up + 30.0));
	};
	const FVector2D Seat = DeepRoomDress::HelmSeatCm(Sub);
	TestTrue(TEXT("a crate at the helm seat is in the helm zone"), DeepRoomDress::InHelmZone(Crate(Seat), Sub, Dress));
	TestTrue(TEXT("a crate on the hatch is in the hatch zone"), DeepRoomDress::InHatchZone(Crate(FVector2D::ZeroVector), Sub, Dress));
	const TPair<FVector2D, FVector2D> Walk = DeepRoomDress::Walks(Sub)[0];
	TestTrue(TEXT("a crate mid-walk blocks it"), DeepRoomDress::BlocksWalk(Crate((Walk.Key + Walk.Value) * 0.5), Sub, Dress, 35.0));
	TestFalse(TEXT("a crate overhead does not block a walk"),
		DeepRoomDress::BlocksWalk(Crate((Walk.Key + Walk.Value) * 0.5, Dress.HeadClearCm + 5.0), Sub, Dress, 35.0));
	return true;
}

#endif
