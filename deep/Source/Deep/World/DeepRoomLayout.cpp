#include "World/DeepRoomLayout.h"

#include "Sim/DeepRules.h"
#include "Sim/DeepTuningTypes.h"

namespace
{
	// Fixture sizes (cm). The console's back stands on the starboard wall; its use point (ConsoleM) is its front face.
	constexpr double RlConsoleLength = 100.0;
	constexpr double RlConsoleDepth = 45.0;
	constexpr double RlConsoleHeight = 110.0;
	constexpr double RlSeatBackCm = 60.0;  // the seat's centre aft of the helm point
	constexpr double RlSeatSize = 55.0;
	constexpr double RlSeatHeight = 50.0;
	constexpr double RlPanelForeCm = 30.0;  // the panel's centre ahead of the helm point
	constexpr double RlPanelDepth = 35.0;
	constexpr double RlPanelWidth = 120.0;
	constexpr double RlPanelHeight = 90.0;
	// UDeepHelmLevers: an 18 cm base; a 52 cm arm on a 42 cm base throws up to 38 degrees, plus the knob.
	constexpr double RlLeverHalfAlong = 37.0;
	constexpr double RlLeverHalfAcross = 9.0;
	constexpr double RlLeverHeight = 100.0;
}

FBox DeepRoomLayout::Inside(const FDeepSubTuning& T)
{
	const double M = UDeepRules::CmPerM;
	const double H = T.InsideHeightM * M;
	return FBox(FVector(-T.InsideAftM * M, -T.InsideHalfWidthM * M, -H * 0.5), FVector(T.InsideForeM * M, T.InsideHalfWidthM * M, H * 0.5));
}

double DeepRoomLayout::DeckTopCm(const FDeepSubTuning& T)
{
	return -T.InsideHeightM * UDeepRules::CmPerM * 0.5;
}

double DeepRoomLayout::HatchRadiusCm(const FDeepSubTuning& T)
{
	return T.InsideHatchRadiusM * UDeepRules::CmPerM;
}

FBox DeepRoomLayout::Console(const FDeepSubTuning& T)
{
	const FVector2D C = T.ConsoleM * UDeepRules::CmPerM;
	const double Deck = DeckTopCm(T);
	const double Wall = T.InsideHalfWidthM * UDeepRules::CmPerM;
	// Against whichever side wall the use point is nearer.
	const double Sign = C.Y >= 0.0 ? 1.0 : -1.0;
	const double Back = Sign * Wall;
	const double Front = Back - Sign * RlConsoleDepth;
	return FBox(FVector(C.X - RlConsoleLength * 0.5, FMath::Min(Front, Back), Deck), FVector(C.X + RlConsoleLength * 0.5, FMath::Max(Front, Back), Deck + RlConsoleHeight));
}

FBox DeepRoomLayout::HelmPanel(const FDeepSubTuning& T)
{
	const FVector2D Hm = T.HelmM * UDeepRules::CmPerM;
	const double Deck = DeckTopCm(T);
	const FVector Centre(Hm.X + RlPanelForeCm, Hm.Y, Deck + RlPanelHeight * 0.5);
	return FBox::BuildAABB(Centre, FVector(RlPanelDepth, RlPanelWidth, RlPanelHeight) * 0.5);
}

FBox DeepRoomLayout::HelmSeat(const FDeepSubTuning& T)
{
	const FVector2D Hm = T.HelmM * UDeepRules::CmPerM;
	const double Deck = DeckTopCm(T);
	const FVector Centre(Hm.X - RlSeatBackCm, Hm.Y, Deck + RlSeatHeight * 0.5);
	return FBox::BuildAABB(Centre, FVector(RlSeatSize, RlSeatSize, RlSeatHeight) * 0.5);
}

FBox DeepRoomLayout::Lever(const FVector2D& AtM, const FDeepSubTuning& T)
{
	const FVector2D At = AtM * UDeepRules::CmPerM;
	const double Deck = DeckTopCm(T);
	return FBox(FVector(At.X - RlLeverHalfAlong, At.Y - RlLeverHalfAcross, Deck), FVector(At.X + RlLeverHalfAlong, At.Y + RlLeverHalfAcross, Deck + RlLeverHeight));
}

TArray<FBox> DeepRoomLayout::Levers(const FDeepSubTuning& T, const FDeepPilotTuning& P)
{
	return { Lever(P.ThrottleLeverM, T), Lever(P.BallastLeverM, T) };
}

FBox DeepRoomLayout::Porthole(const FDeepSubTuning& T)
{
	const double M = UDeepRules::CmPerM;
	const double X = T.InsideForeM * M;
	const double Z = DeckTopCm(T) + T.InsidePortholeZM * M;
	const FVector2D Half = T.InsidePortholeHalfM * M;
	return FBox(FVector(X, -Half.X, Z - Half.Y), FVector(X, Half.X, Z + Half.Y));
}

FVector DeepRoomLayout::HatchLampPost(const FDeepSubTuning& T)
{
	// Aft of the hatch's centre and against the port wall, out of the round hatch's clear zone.
	return FVector(-35.0, -T.InsideHalfWidthM * UDeepRules::CmPerM + 10.0, DeckTopCm(T));
}
