#include "Sim/DeepCaveRules.h"

namespace
{
	/** A fixed phase in [0, 2 pi) per segment and channel: the same shaft on every machine and every run. */
	float Phase(int32 Seed, int32 Segment, int32 Channel)
	{
		const double X = FMath::Sin(double(Segment) * 12.9898 + double(Seed) * 78.233 + double(Channel) * 37.719) * 43758.5453;
		return float((X - FMath::FloorToDouble(X)) * UE_DOUBLE_TWO_PI);
	}

	float Smooth01(float T)
	{
		const float C = FMath::Clamp(T, 0.f, 1.f);
		return C * C * (3.f - 2.f * C);
	}

	/** Two sines (the second shorter and weaker), in [-1, 1]. */
	float Wave(float DepthM, float WavelengthM, float PhaseA, float PhaseB)
	{
		const float W = UE_TWO_PI / FMath::Max(1.f, WavelengthM);
		return 0.8f * FMath::Sin(W * DepthM + PhaseA) + 0.2f * FMath::Sin(W * 2.3f * DepthM + PhaseB);
	}

	/** Integer hash of a lattice cell (murmur-style mixing): fixed for all time, unlike any engine random stream. */
	uint32 HashCell(int32 X, int32 Y, int32 Z, uint32 Seed)
	{
		uint32 H = Seed * 0x9E3779B1u + 0x7F4A7C15u;
		H = (H ^ uint32(X)) * 0x85EBCA6Bu;
		H ^= H >> 15;
		H = (H ^ uint32(Y)) * 0xC2B2AE35u;
		H ^= H >> 13;
		H = (H ^ uint32(Z)) * 0x27D4EB2Fu;
		H ^= H >> 16;
		H *= 0x85EBCA6Bu;
		H ^= H >> 13;
		H *= 0xC2B2AE35u;
		H ^= H >> 16;
		return H;
	}

	/** Perlin's 12 edge gradients (16 with repeats) dotted with the offset. */
	float GradDot(uint32 H, float X, float Y, float Z)
	{
		const uint32 G = H & 15u;
		const float U = G < 8u ? X : Y;
		const float V = G < 4u ? Y : (G == 12u || G == 14u ? X : Z);
		return ((G & 1u) ? -U : U) + ((G & 2u) ? -V : V);
	}

	float Fade(float T)
	{
		return T * T * T * (T * (T * 6.f - 15.f) + 10.f);
	}

	/** Smooth maximum (polynomial, width K): never below either input, so a floor stays a floor without a crease. */
	float SmoothMax(float A, float B, float K)
	{
		const float H = FMath::Max(K - FMath::Abs(A - B), 0.f) / FMath::Max(K, 1e-4f);
		return FMath::Max(A, B) + H * H * K * 0.25f;
	}
}

float UDeepCaveRules::Noise3(const FVector& P, int32 Seed)
{
	const double Fx = FMath::FloorToDouble(P.X);
	const double Fy = FMath::FloorToDouble(P.Y);
	const double Fz = FMath::FloorToDouble(P.Z);
	const int32 X0 = int32(Fx);
	const int32 Y0 = int32(Fy);
	const int32 Z0 = int32(Fz);
	const float X = float(P.X - Fx);
	const float Y = float(P.Y - Fy);
	const float Z = float(P.Z - Fz);
	const uint32 S = uint32(Seed);
	const float N000 = GradDot(HashCell(X0, Y0, Z0, S), X, Y, Z);
	const float N100 = GradDot(HashCell(X0 + 1, Y0, Z0, S), X - 1.f, Y, Z);
	const float N010 = GradDot(HashCell(X0, Y0 + 1, Z0, S), X, Y - 1.f, Z);
	const float N110 = GradDot(HashCell(X0 + 1, Y0 + 1, Z0, S), X - 1.f, Y - 1.f, Z);
	const float N001 = GradDot(HashCell(X0, Y0, Z0 + 1, S), X, Y, Z - 1.f);
	const float N101 = GradDot(HashCell(X0 + 1, Y0, Z0 + 1, S), X - 1.f, Y, Z - 1.f);
	const float N011 = GradDot(HashCell(X0, Y0 + 1, Z0 + 1, S), X, Y - 1.f, Z - 1.f);
	const float N111 = GradDot(HashCell(X0 + 1, Y0 + 1, Z0 + 1, S), X - 1.f, Y - 1.f, Z - 1.f);
	const float U = Fade(X);
	const float V = Fade(Y);
	const float W = Fade(Z);
	const float A = FMath::Lerp(FMath::Lerp(N000, N100, U), FMath::Lerp(N010, N110, U), V);
	const float B = FMath::Lerp(FMath::Lerp(N001, N101, U), FMath::Lerp(N011, N111, U), V);
	return FMath::Clamp(FMath::Lerp(A, B, W), -1.f, 1.f);
}

float UDeepCaveRules::RockFbm(const FVector& P, const FDeepCaveTuning& Cave)
{
	const int32 Octaves = FMath::Clamp(Cave.RockOctaves, 1, 8);
	const float Ridge = FMath::Clamp(Cave.RockRidge, 0.f, 1.f);
	const float Lacunarity = FMath::Clamp(Cave.RockLacunarity, 1.1f, 4.f);
	const float Gain = FMath::Clamp(Cave.RockGain, 0.05f, 0.95f);
	float Sum = 0.f;
	float Norm = 0.f;
	float Amp = 1.f;
	FVector Q = P;
	for (int32 O = 0; O < Octaves; ++O)
	{
		const float N = Noise3(Q, Cave.RockSeed + O * 1013);
		// Ridged: 1 - |n| squared peaks in a sharp crease where n crosses 0 (a rib), flat troughs between.
		float R = 1.f - FMath::Abs(N);
		R *= R;
		Sum += Amp * FMath::Lerp(N, 2.f * R - 1.f, Ridge);
		Norm += Amp;
		Amp *= Gain;
		// Offset each octave so the lattices do not line up at the origin.
		Q = Q * Lacunarity + FVector(17.13, -31.7, 9.41);
	}
	return Norm > 0.f ? Sum / Norm : 0.f;
}

float UDeepCaveRules::RockWeight(float DepthM, const TArray<float>& StopsM, const FDeepCaveTuning& Cave)
{
	if (!Cave.bRock || Segment(DepthM, StopsM) < 0)
	{
		return 0.f;
	}
	return FMath::Lerp(FMath::Clamp(Cave.MouthCalm01, 0.f, 1.f), 1.f, Envelope(DepthM, StopsM, Cave));
}

float UDeepCaveRules::WallFloorM(float MinRadiusM, const FDeepCaveTuning& Cave)
{
	return MinRadiusM + FMath::Max(0.f, Cave.RockFloorMarginM);
}

float UDeepCaveRules::WallRadiusM(float DepthM, float AngleRad, const TArray<float>& StopsM, float MinRadiusM, const FDeepCaveTuning& Cave)
{
	const float Base = RadiusM(DepthM, StopsM, MinRadiusM, Cave);
	const float Weight = RockWeight(DepthM, StopsM, Cave);
	if (Weight <= 0.f)
	{
		return Base;
	}
	// Sampled on the cylinder of the local radius: seamless round the wall, true metres at any width.
	const float Scale = FMath::Max(0.5f, Cave.RockScaleM);
	const float Cx = FMath::Cos(AngleRad) * Base;
	const float Cy = FMath::Sin(AngleRad) * Base;
	const FVector P(Cx / Scale, Cy / Scale, DepthM * FMath::Max(0.05f, Cave.RockStretchZ) / Scale);
	// A crest of the ridged noise stands into the passage: a rib or a bulge; a trough is a recess.
	const float Lumps = -FMath::Max(0.f, Cave.RockAmplitudeM) * RockFbm(P, Cave);

	float Ledge = 0.f;
	if (Cave.StrataSpacingM > 0.1f && Cave.StrataDepthM > 0.f)
	{
		const FVector Wide(Cx / (Scale * 2.5f), Cy / (Scale * 2.5f), DepthM / (Scale * 4.f));
		// Warped bands: the bedding tilts and wanders a little instead of running in perfect rings.
		const float Band = DepthM / Cave.StrataSpacingM + 0.3f * Noise3(Wide, Cave.RockSeed + 77);
		const float S = Band - FMath::FloorToFloat(Band);
		// A shelf lip at the top of each band (up-facing: the silt collects there), the undercut receding below it.
		float Profile = FMath::Pow(1.f - S, 2.5f);
		Profile = FMath::Lerp(Profile, 1.f, FMath::SmoothStep(0.86f, 1.f, S));
		const float Present = FMath::Clamp(0.5f + 1.3f * Noise3(Wide * 1.6f, Cave.RockSeed + 99), 0.f, 1.f);
		const float Breakup = FMath::Lerp(1.f, Present, FMath::Clamp(Cave.StrataBreakup, 0.f, 1.f));
		// Centred round zero so the passage keeps its width on average.
		Ledge = Cave.StrataDepthM * (0.35f - Breakup * Profile);
	}
	const float R = Base + Weight * (Lumps + Ledge);
	return SmoothMax(R, WallFloorM(MinRadiusM, Cave), 1.f);
}

int32 UDeepCaveRules::Segment(float DepthM, const TArray<float>& StopsM)
{
	if (StopsM.IsEmpty() || DepthM < StopsM[0])
	{
		return -1;
	}
	for (int32 K = 0; K + 1 < StopsM.Num(); ++K)
	{
		if (DepthM < StopsM[K + 1])
		{
			return K;
		}
	}
	return StopsM.Num() - 1;
}

float UDeepCaveRules::Envelope(float DepthM, const TArray<float>& StopsM, const FDeepCaveTuning& Cave)
{
	const int32 K = Segment(DepthM, StopsM);
	if (K < 0 || K + 1 >= StopsM.Num())
	{
		return 0.f;
	}
	const float A = StopsM[K] + FMath::Max(0.f, Cave.StraightBelowStopM);
	const float B = StopsM[K + 1] - FMath::Max(0.f, Cave.StraightAboveStopM);
	if (B <= A)
	{
		return 0.f;
	}
	const float Half = (B - A) * 0.5f;
	const float RampIn = FMath::Clamp(Cave.StraightBelowStopM, 1.f, Half);
	const float RampOut = FMath::Clamp(Cave.StraightAboveStopM, 1.f, Half);
	return Smooth01((DepthM - A) / RampIn) * Smooth01((B - DepthM) / RampOut);
}

namespace
{
	/** The bend before the drift cap: both waves at full amplitude, times the envelope. */
	FVector2D RawCentreM(float DepthM, int32 K, float E, const FDeepCaveTuning& Cave)
	{
		const float X = Cave.BendAmplitudeM.X * Wave(DepthM, Cave.BendWavelengthM.X, Phase(Cave.Seed, K, 0), Phase(Cave.Seed, K, 1));
		const float Y = Cave.BendAmplitudeM.Y * Wave(DepthM, Cave.BendWavelengthM.Y, Phase(Cave.Seed, K, 2), Phase(Cave.Seed, K, 3));
		return FVector2D(X, Y) * E;
	}

	/**
	 * One scale per stretch (so the centre stays smooth): the steepest drift of the raw bend, sampled every metre, brought
	 * down to 95% of BendMaxDriftPerM (the 5% covers the curve between samples). 1 when already inside the cap.
	 */
	float ComputeBendScale(int32 K, const TArray<float>& StopsM, const FDeepCaveTuning& Cave)
	{
		const float A = StopsM[K];
		const float B = StopsM[K + 1];
		float Steepest = 0.f;
		FVector2D Prev = FVector2D::ZeroVector;
		for (float D = A; D <= B; D += 1.f)
		{
			const FVector2D C = RawCentreM(D, K, UDeepCaveRules::Envelope(D, StopsM, Cave), Cave);
			if (D > A)
			{
				Steepest = FMath::Max(Steepest, float((C - Prev).Size()));
			}
			Prev = C;
		}
		const float Target = 0.95f * Cave.BendMaxDriftPerM;
		return Steepest > Target ? Target / Steepest : 1.f;
	}

	/** ComputeBendScale, remembered per thread for the last inputs seen (the mesh asks for it at every vertex). */
	float BendScale(int32 K, const TArray<float>& StopsM, const FDeepCaveTuning& Cave)
	{
		if (Cave.BendMaxDriftPerM <= 0.f || K < 0 || K + 1 >= StopsM.Num())
		{
			return 1.f;
		}
		const float Inputs[] = { Cave.BendAmplitudeM.X, Cave.BendAmplitudeM.Y, Cave.BendWavelengthM.X, Cave.BendWavelengthM.Y,
			Cave.StraightBelowStopM, Cave.StraightAboveStopM, Cave.BendMaxDriftPerM, float(Cave.Seed), float(K), StopsM[K], StopsM[K + 1] };
		const uint32 Key = FCrc::MemCrc32(Inputs, sizeof(Inputs));
		struct FEntry { uint32 Key = 0; float Scale = 1.f; bool bSet = false; };
		thread_local FEntry Cache[8];
		FEntry& Slot = Cache[K & 7];
		if (!Slot.bSet || Slot.Key != Key)
		{
			Slot.Key = Key;
			Slot.Scale = ComputeBendScale(K, StopsM, Cave);
			Slot.bSet = true;
		}
		return Slot.Scale;
	}
}

FVector2D UDeepCaveRules::CentreM(float DepthM, const TArray<float>& StopsM, const FDeepCaveTuning& Cave)
{
	const float E = Envelope(DepthM, StopsM, Cave);
	if (E <= 0.f)
	{
		return FVector2D::ZeroVector;
	}
	const int32 K = Segment(DepthM, StopsM);
	return RawCentreM(DepthM, K, E, Cave) * BendScale(K, StopsM, Cave);
}

float UDeepCaveRules::DriftPerM(float DepthM, const TArray<float>& StopsM, const FDeepCaveTuning& Cave)
{
	const float H = 0.25f;
	return float((CentreM(DepthM + H, StopsM, Cave) - CentreM(DepthM - H, StopsM, Cave)).Size()) / (2.f * H);
}

float UDeepCaveRules::RadiusM(float DepthM, const TArray<float>& StopsM, float MinRadiusM, const FDeepCaveTuning& Cave)
{
	const float Stop = Cave.StopRadiusM;
	const float E = Envelope(DepthM, StopsM, Cave);
	float R = Stop;
	if (E > 0.f)
	{
		const int32 K = Segment(DepthM, StopsM);
		// Shaped so the extremes are reached and held a little: real caverns and real squeezes, not a gentle wobble.
		const float N = FMath::Clamp(1.4f * Wave(DepthM, Cave.RadiusWavelengthM, Phase(Cave.Seed, K, 4), Phase(Cave.Seed, K, 5)), -1.f, 1.f);
		const float Raw = N >= 0.f ? Stop + N * (Cave.CavernRadiusM - Stop) : Stop + N * (Stop - MinRadiusM);
		R = Stop + E * (Raw - Stop);
		// Room in the bends: the more the centre drifts here, the wider (smooth, since the drift is).
		if (Cave.BendRoomM > 0.f && Cave.BendMaxDriftPerM > 0.f)
		{
			const float Room = Cave.BendRoomM * FMath::Clamp(DriftPerM(DepthM, StopsM, Cave) / Cave.BendMaxDriftPerM, 0.f, 1.f);
			R = FMath::Min(R + Room, FMath::Max(R, Cave.CavernRadiusM));
		}
	}
	return FMath::Max(R, MinRadiusM);
}

float UDeepCaveRules::MinRadiusM(const FVector& BoxMinM, const FVector& BoxMaxM, const FDeepCaveTuning& Cave)
{
	const float Pad = FMath::Max(0.f, Cave.HullPadM);
	const float HX = FMath::Max(FMath::Abs(BoxMinM.X), FMath::Abs(BoxMaxM.X)) + Pad;
	const float HY = FMath::Max(FMath::Abs(BoxMinM.Y), FMath::Abs(BoxMaxM.Y)) + Pad;
	return FMath::Sqrt(HX * HX + HY * HY) + FMath::Max(0.f, Cave.SqueezeMarginM);
}

FDeepCaveContact UDeepCaveRules::Contact(const FVector2D& OffsetM, float SubDepthM, const FVector& BoxMinM, const FVector& BoxMaxM, const TArray<float>& StopsM, float MinRadiusM, const FDeepCaveTuning& Cave)
{
	FDeepCaveContact Out;
	Out.PenetrationM = -UE_BIG_NUMBER;
	const float Pad = FMath::Max(0.f, Cave.HullPadM);
	const float X0 = float(BoxMinM.X) - Pad;
	const float X1 = float(BoxMaxM.X) + Pad;
	const float Y0 = float(BoxMinM.Y) - Pad;
	const float Y1 = float(BoxMaxM.Y) + Pad;
	const float Z0 = float(BoxMinM.Z) - Pad;
	const float Z1 = float(BoxMaxM.Z) + Pad;
	const float Step = FMath::Clamp(Cave.ContactStepM, 0.25f, 10.f);
	const int32 Nx = FMath::Clamp(FMath::CeilToInt((X1 - X0) / Step), 1, 64);
	const int32 Ny = FMath::Clamp(FMath::CeilToInt((Y1 - Y0) / Step), 1, 64);
	const int32 Nz = FMath::Clamp(FMath::CeilToInt((Z1 - Z0) / Step), 2, 32);
	// The box's outline: corners first (the worst points against a round wall), then the points along each side.
	TArray<FVector2D, TInlineAllocator<160>> Outline;
	Outline.Add(FVector2D(X0, Y0));
	Outline.Add(FVector2D(X0, Y1));
	Outline.Add(FVector2D(X1, Y0));
	Outline.Add(FVector2D(X1, Y1));
	for (int32 I = 1; I < Nx; ++I)
	{
		const float X = FMath::Lerp(X0, X1, float(I) / Nx);
		Outline.Add(FVector2D(X, Y0));
		Outline.Add(FVector2D(X, Y1));
	}
	for (int32 I = 1; I < Ny; ++I)
	{
		const float Y = FMath::Lerp(Y0, Y1, float(I) / Ny);
		Outline.Add(FVector2D(X0, Y));
		Outline.Add(FVector2D(X1, Y));
	}
	// Box Z is up (sub-local); depth grows down.
	for (int32 K = 0; K <= Nz; ++K)
	{
		const float Z = FMath::Lerp(Z0, Z1, float(K) / Nz);
		const float D = SubDepthM - Z;
		const FVector2D C = CentreM(D, StopsM, Cave);
		const float Base = RadiusM(D, StopsM, MinRadiusM, Cave);
		const bool bRound = RockWeight(D, StopsM, Cave) <= 0.f;
		for (const FVector2D& Point : Outline)
		{
			const FVector2D Rel = OffsetM + Point - C;
			const float Dist = Rel.Size();
			const float R = bRound ? Base : WallRadiusM(D, FMath::Atan2(Rel.Y, Rel.X), StopsM, MinRadiusM, Cave);
			const float Pen = Dist - R;
			if (Pen > Out.PenetrationM)
			{
				Out.PenetrationM = Pen;
				Out.Normal = Dist > UE_KINDA_SMALL_NUMBER ? Rel / Dist : FVector2D(1.f, 0.f);
			}
		}
	}
	Out.bTouching = Out.PenetrationM > 0.f;
	return Out;
}

FVector2D UDeepCaveRules::SteerVelocity(const FVector2D& VelocityMps, const FVector2D& Input, float DeltaSeconds, const FDeepCaveTuning& Cave)
{
	FVector2D In = Input;
	if (In.SizeSquared() > 1.f)
	{
		In.Normalize();
	}
	const FVector2D Want = In * FMath::Max(0.f, Cave.SteerSpeedMps);
	const FVector2D Gap = Want - VelocityMps;
	const float Step = FMath::Max(0.f, Cave.SteerAccelMps2) * FMath::Max(0.f, DeltaSeconds);
	const float GapSize = Gap.Size();
	return GapSize <= Step ? Want : VelocityMps + Gap / GapSize * Step;
}

int32 UDeepCaveRules::HitDamage(float ImpactSpeedMps, const FDeepCaveTuning& Cave)
{
	if (ImpactSpeedMps < Cave.HitMinSpeedMps || ImpactSpeedMps <= 0.f)
	{
		return 0;
	}
	return FMath::Clamp(FMath::RoundToInt(ImpactSpeedMps * Cave.DamagePerMps), 1, FMath::Max(1, Cave.MaxHitDamage));
}

float UDeepCaveRules::HullIntegrity01(int32 HullDamage, const FDeepCaveTuning& Cave)
{
	if (Cave.HullBreachDamage <= 0)
	{
		return 1.f;
	}
	return FMath::Clamp(1.f - float(HullDamage) / float(Cave.HullBreachDamage), 0.f, 1.f);
}

bool UDeepCaveRules::IsBreached(int32 HullDamage, const FDeepCaveTuning& Cave)
{
	return Cave.HullBreachDamage > 0 && HullDamage >= Cave.HullBreachDamage;
}

bool UDeepCaveRules::CrossesCritical(int32 DamageBefore, int32 DamageAfter, const FDeepCaveTuning& Cave)
{
	return HullIntegrity01(DamageBefore, Cave) >= Cave.HullCriticalIntegrity01 && HullIntegrity01(DamageAfter, Cave) < Cave.HullCriticalIntegrity01;
}
