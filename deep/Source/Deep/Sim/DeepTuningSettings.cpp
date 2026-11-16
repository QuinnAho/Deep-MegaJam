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

	// Style guide 5: the sea at 100, 300 and 500 m. Colours are the guide's sRGB hex.
	// Fog density: the guide's 0.02 / 0.035 / 0.05 are 30x too thin (the engine divides by 1000: 0.02 = 0.002 per m, 92% clear
	// at 40 m). These meet the guide's own visibility row instead: silhouettes to 40 m at 100 m (0.75 = 0.075 per m, 5%
	// left at 40 m), and the lamp still shows salvage 10 m away at 300 and 500 m (extinction scale 1, not 1.5 and 2).
	auto MakeLook = [](float DepthM, float Lux, float Vol, float Density, FColor Water, float Extinction, float VolDistM,
		float Bias, float Bloom, float Vignette, float Grain, FVector4 Sat, float Contrast, float Fringe, float Glint)
	{
		FDeepLookPreset P;
		P.DepthM = DepthM; P.DownlightLux = Lux; P.DownlightVolumetric = Vol; P.FogDensity = Density;
		P.FogColor = FLinearColor(Water); P.ExtinctionScale = Extinction; P.VolumetricDistanceM = VolDistM;
		P.ExposureBias = Bias; P.Bloom = Bloom; P.Vignette = Vignette; P.Grain = Grain; P.Saturation = Sat;
		P.Contrast = Contrast; P.Fringe = Fringe; P.GlintScale = Glint;
		return P;
	};
	// The sub's outside lights: full at 100 m, dim with a little glow at 300 m, at 500 m only a faint warm mark at the
	// hatch that lights the rim and no water, so the sea is black and the lamp is the only light.
	// Volumetric distance 64/48/40 m (was 80/60/40): the same Z slices over less water give the 35 m lamp beam finer
	// slices; past it the plain height fog carries the water colour.
	Look.Presets = {
		MakeLook(100.f, 2.0f, 3.0f, 0.75f, FColor(0x19, 0x3C, 0x45), 1.0f, 64.f, 0.0f, 0.4f, 0.35f, 0.10f, FVector4(0.75, 0.95, 1.0, 0.9), 1.05f, 0.f, 1.0f),
		MakeLook(300.f, 0.04f, 0.4f, 1.0f, FColor(0x07, 0x1A, 0x28), 1.0f, 48.f, 1.5f, 0.5f, 0.45f, 0.18f, FVector4(0.5, 0.9, 1.0, 0.8), 1.08f, 0.2f, 0.3f),
		MakeLook(500.f, 0.0f, 0.0f, 1.2f, FColor(0x00, 0x00, 0x00), 1.0f, 40.f, 2.0f, 0.6f, 0.55f, 0.25f, FVector4(0.4, 0.85, 1.0, 0.75), 1.1f, 0.35f, 0.15f),
	};
	Look.Presets[0].SubLightScale = 1.f;   Look.Presets[0].SubLightFog = 0.6f;
	Look.Presets[1].SubLightScale = 0.4f;  Look.Presets[1].SubLightFog = 0.15f;
	Look.Presets[2].SubLightScale = 0.08f; Look.Presets[2].SubLightFog = 0.f;

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

TArray<float> UDeepTuningSettings::StopDepthsM() const
{
	TArray<float> Out;
	for (const FDeepStopTuning& Stop : Stops)
	{
		Out.Add(Stop.SubDepthM);
	}
	return Out;
}
