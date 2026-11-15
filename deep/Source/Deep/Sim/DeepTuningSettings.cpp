#include "Sim/DeepTuningSettings.h"

namespace
{
	FDeepZone MakeZone(const TCHAR* Name, float MinDepthM)
	{
		FDeepZone Zone;
		Zone.Name = Name;
		Zone.MinDepthM = MinDepthM;
		return Zone;
	}

	FDeepStopTuning MakeStop(float SubDepthM, float FogDensity, const FLinearColor& FogColor, float SunIntensity)
	{
		FDeepStopTuning Stop;
		Stop.SubDepthM = SubDepthM;
		Stop.FogDensity = FogDensity;
		Stop.FogColor = FogColor;
		Stop.SunIntensity = SunIntensity;
		return Stop;
	}

	FDeepSalvageKind MakeKind(EDeepSalvageSize Size, const TCHAR* Name, float Kg, int32 Value, float SizeM, const FLinearColor& Color)
	{
		FDeepSalvageKind Kind;
		Kind.Size = Size;
		Kind.Name = Name;
		Kind.Kg = Kg;
		Kind.Value = Value;
		Kind.SizeM = SizeM;
		Kind.Color = Color;
		return Kind;
	}

	FDeepSalvageSpot MakeSpot(EDeepSalvageSize Size, float AngleDeg, float OutM, float BelowM)
	{
		FDeepSalvageSpot Spot;
		Spot.Size = Size;
		Spot.AngleDeg = AngleDeg;
		Spot.OutM = OutM;
		Spot.BelowM = BelowM;
		return Spot;
	}
}

UDeepTuningSettings::UDeepTuningSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("DEEP Tuning");

	// GDD zones, compressed for the game: Sunlight 0-200 m, Twilight 200-400 m, Midnight 400-600 m.
	Zones = {
		MakeZone(TEXT("Sunlight"), 0.f),
		MakeZone(TEXT("Twilight"), 200.f),
		MakeZone(TEXT("Midnight"), 400.f),
	};

	// One arena, three light and fog settings. One stop per zone: 100 m, 300 m and 500 m of displayed depth.
	Stops = {
		MakeStop(100.f, 0.25f, FLinearColor(0.02f, 0.09f, 0.11f), 3.f),
		MakeStop(300.f, 0.4f, FLinearColor(0.01f, 0.04f, 0.06f), 0.8f),
		MakeStop(500.f, 0.6f, FLinearColor(0.004f, 0.01f, 0.02f), 0.1f),
	};

	SalvageKinds = {
		MakeKind(EDeepSalvageSize::Small, TEXT("Scrap"), 2.f, 20, 0.4f, FLinearColor(0.55f, 0.35f, 0.2f)),
		MakeKind(EDeepSalvageSize::Medium, TEXT("Scrap"), 6.f, 70, 0.7f, FLinearColor(0.45f, 0.45f, 0.5f)),
		MakeKind(EDeepSalvageSize::Large, TEXT("Heavy scrap"), 15.f, 200, 1.1f, FLinearColor(0.6f, 0.15f, 0.1f)),
		MakeKind(EDeepSalvageSize::Pearl, TEXT("Pearl"), 1.f, 120, 0.3f, FLinearColor(0.95f, 0.95f, 1.f)),
		MakeKind(EDeepSalvageSize::Relic, TEXT("Relic"), 60.f, 500, 1.6f, FLinearColor(0.9f, 0.7f, 0.2f)),
	};

	// Below and outward from the anchor. Richer is farther and lower; pearls sit just past the 40 m stop-1 tether.
	Arena.SalvageSpots = {
		MakeSpot(EDeepSalvageSize::Small, 0.f, 10.f, 10.f),
		MakeSpot(EDeepSalvageSize::Small, 135.f, 12.f, 8.f),
		MakeSpot(EDeepSalvageSize::Small, 250.f, 8.f, 14.f),
		MakeSpot(EDeepSalvageSize::Medium, 60.f, 18.f, 18.f),
		MakeSpot(EDeepSalvageSize::Medium, 200.f, 20.f, 16.f),
		MakeSpot(EDeepSalvageSize::Medium, 310.f, 15.f, 24.f),
		MakeSpot(EDeepSalvageSize::Large, 100.f, 22.f, 28.f),
		MakeSpot(EDeepSalvageSize::Large, 280.f, 26.f, 26.f),
		MakeSpot(EDeepSalvageSize::Pearl, 30.f, 30.f, 32.f),
		MakeSpot(EDeepSalvageSize::Pearl, 170.f, 33.f, 30.f),
	};
}

FDeepStopTuning UDeepTuningSettings::GetStop(int32 Stop) const
{
	if (Stops.IsEmpty())
	{
		return FDeepStopTuning();
	}
	return Stops[FMath::Clamp(Stop - 1, 0, Stops.Num() - 1)];
}

bool UDeepTuningSettings::FindSalvageKind(EDeepSalvageSize Size, FDeepSalvageKind& Out) const
{
	for (const FDeepSalvageKind& Kind : SalvageKinds)
	{
		if (Kind.Size == Size)
		{
			Out = Kind;
			return true;
		}
	}
	return false;
}
