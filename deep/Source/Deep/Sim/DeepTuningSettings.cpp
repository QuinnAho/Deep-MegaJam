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

	// Real ocean zones: the Director names them on the depth counter.
	Zones = {
		MakeZone(TEXT("Sunlight"), 0.f),
		MakeZone(TEXT("Twilight"), 200.f),
		MakeZone(TEXT("Midnight"), 1000.f),
		MakeZone(TEXT("Abyss"), 4000.f),
		MakeZone(TEXT("Hadal"), 6000.f),
	};

	// One arena, three light and fog settings.
	Stops = {
		MakeStop(300.f, 0.25f, FLinearColor(0.02f, 0.09f, 0.11f), 3.f),
		MakeStop(1200.f, 0.4f, FLinearColor(0.01f, 0.04f, 0.06f), 0.8f),
		MakeStop(4200.f, 0.6f, FLinearColor(0.004f, 0.01f, 0.02f), 0.1f),
	};

	// The look of each stop, set apart from the depths above so the two can change on separate branches.
	// FogDensity 1 = light falls to 1/e in 10 m. FogColor is the far fog's luminance (cd/m2) under the locked
	// exposure below. SunIntensity is lux from above. Tune live with Deep.Tune Stops[i].<Field> <Value>.
	if (Stops.Num() >= 3)
	{
		Stops[0].FogDensity = 0.6f;
		Stops[0].FogColor = FLinearColor(0.01f, 0.05f, 0.07f);
		Stops[0].SunIntensity = 3.f;
		Stops[1].FogDensity = 0.9f;
		Stops[1].FogColor = FLinearColor(0.002f, 0.008f, 0.02f);
		Stops[1].SunIntensity = 0.6f;
		Stops[2].FogDensity = 1.f;
		Stops[2].FogColor = FLinearColor(0.0003f, 0.0006f, 0.0015f);
		Stops[2].SunIntensity = 0.05f;
	}

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
