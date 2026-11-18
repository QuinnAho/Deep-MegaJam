#include "Sim/DeepCaveDiveRules.h"

FIntVector FDeepCaveRoom::CellAt(const FVector& LocalCm) const
{
	const FVector Rel = (LocalCm - OriginCm) / FMath::Max(1.f, CellCm);
	return FIntVector(
		FMath::Clamp(FMath::FloorToInt32(Rel.X), 0, FMath::Max(0, Dims.X - 1)),
		FMath::Clamp(FMath::FloorToInt32(Rel.Y), 0, FMath::Max(0, Dims.Y - 1)),
		FMath::Clamp(FMath::FloorToInt32(Rel.Z), 0, FMath::Max(0, Dims.Z - 1)));
}

namespace DeepCaveDiveRules
{
	TArray<FVector2D> PileSpotsCm(int32 Index, int32 Count, float SpreadCm, float StepCm, float ReachCm, float YawDeg)
	{
		TArray<FVector2D> Out;
		Count = FMath::Max(1, Count);
		const double Reach = FMath::Max(0.f, ReachCm);
		const double Step = FMath::Max(1.f, StepCm);
		const double Spread = Count == 1 ? 0.0 : FMath::Clamp<double>(SpreadCm, 0.0, Reach);
		const double Share = 360.0 / Count;
		const double Base = YawDeg + Index * Share;

		// Radii: the spread first, then nearer and farther by a step at a time, within [0, Reach].
		TArray<double> Radii = { Spread };
		for (int32 k = 1; ; ++k)
		{
			const double Far = Spread + k * Step;
			const double Near = Spread - k * Step;
			const bool bFar = Far <= Reach + 0.01;
			const bool bNear = Near >= 0.0;
			if (bFar)
			{
				Radii.Add(Far);
			}
			if (bNear)
			{
				Radii.Add(Near);
			}
			if (!bFar && !bNear)
			{
				break;
			}
		}
		// Turns: straight out first, then a little either way, up to just under half a share (each piece keeps its side).
		const double TurnStep = 15.0;
		TArray<double> Turns = { 0.0 };
		for (double T = TurnStep; T < 0.5 * Share - 1.0; T += TurnStep)
		{
			Turns.Add(T);
			Turns.Add(-T);
		}
		bool bCentre = false;
		for (const double R : Radii)
		{
			if (R < 1.0)
			{
				if (!bCentre)
				{
					Out.Add(FVector2D::ZeroVector);
					bCentre = true;
				}
				continue;
			}
			for (const double T : Turns)
			{
				const double A = FMath::DegreesToRadians(Base + T);
				Out.Add(FVector2D(R * FMath::Cos(A), R * FMath::Sin(A)));
			}
		}
		return Out;
	}

	int32 RelicStop(bool bCaves, int32 ShaftRelicStop, int32 CaveBerths)
	{
		return bCaves ? FMath::Max(1, CaveBerths) : ShaftRelicStop;
	}

	int32 ShownStops(bool bCaves, int32 ShaftStops, int32 CaveBerths)
	{
		return bCaves ? FMath::Clamp(CaveBerths, 0, ShaftStops) : ShaftStops;
	}

	/** Felzenszwalb and Huttenlocher's 1D squared distance transform of F (N samples, Stride apart) in place. */
	static void DistanceTransform1D(float* F, int32 N, int32 Stride, TArray<float>& D, TArray<int32>& V, TArray<float>& Z)
	{
		constexpr float Inf = 1e20f;
		D.SetNumUninitialized(N);
		V.SetNumUninitialized(N);
		Z.SetNumUninitialized(N + 1);
		int32 K = 0;
		V[0] = 0;
		Z[0] = -Inf;
		Z[1] = Inf;
		for (int32 Q = 1; Q < N; ++Q)
		{
			const float Fq = F[Q * Stride];
			float S;
			for (;;)
			{
				const int32 P = V[K];
				S = ((Fq + float(Q) * Q) - (F[P * Stride] + float(P) * P)) / (2.f * (Q - P));
				if (S <= Z[K] && K > 0)
				{
					--K;
					continue;
				}
				break;
			}
			if (S <= Z[K])
			{
				// K == 0 and the new parabola is lower everywhere.
				V[0] = Q;
				Z[0] = -Inf;
				Z[1] = Inf;
				continue;
			}
			++K;
			V[K] = Q;
			Z[K] = S;
			Z[K + 1] = Inf;
		}
		K = 0;
		for (int32 Q = 0; Q < N; ++Q)
		{
			while (Z[K + 1] < Q)
			{
				++K;
			}
			const int32 P = V[K];
			D[Q] = float(Q - P) * (Q - P) + F[P * Stride];
		}
		for (int32 Q = 0; Q < N; ++Q)
		{
			F[Q * Stride] = D[Q];
		}
	}

	void BuildRoom(FDeepCaveRoom& Room, const TArray<uint8>& Rock, const FIntVector& Seed, float ClearCm)
	{
		const FIntVector N = Room.Dims;
		const int32 Count = Room.Num();
		Room.ClearCm = ClearCm;
		Room.FreeCount = 0;
		Room.Reached.Init(0, FMath::Max(0, Count));
		Room.Free.Init(0, FMath::Max(0, Count));
		Room.DistCm.Init(0.f, FMath::Max(0, Count));
		Room.Nearest.Init(INDEX_NONE, FMath::Max(0, Count));
		if (Count <= 0 || Rock.Num() != Count)
		{
			return;
		}

		// The seed: its own cell, or the nearest open one.
		const FIntVector SeedCell(FMath::Clamp(Seed.X, 0, N.X - 1), FMath::Clamp(Seed.Y, 0, N.Y - 1), FMath::Clamp(Seed.Z, 0, N.Z - 1));
		int32 Start = Room.Index(SeedCell);
		if (Rock[Start])
		{
			int64 Best = TNumericLimits<int64>::Max();
			Start = INDEX_NONE;
			for (int32 i = 0; i < Count; ++i)
			{
				if (!Rock[i])
				{
					const FIntVector D = Room.CellOf(i) - SeedCell;
					const int64 Sq = int64(D.X) * D.X + int64(D.Y) * D.Y + int64(D.Z) * D.Z;
					if (Sq < Best)
					{
						Best = Sq;
						Start = i;
					}
				}
			}
			if (Start == INDEX_NONE)
			{
				return;
			}
		}

		// The water: flooded from the seed through open cells (faces only, so rock a cell thick is a wall).
		static const FIntVector Faces[6] = { {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1} };
		TArray<int32> Queue;
		Queue.Reserve(Count / 4);
		Queue.Add(Start);
		Room.Reached[Start] = 1;
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const FIntVector C = Room.CellOf(Queue[Head]);
			for (const FIntVector& F : Faces)
			{
				const FIntVector Next = C + F;
				if (Next.X < 0 || Next.Y < 0 || Next.Z < 0 || Next.X >= N.X || Next.Y >= N.Y || Next.Z >= N.Z)
				{
					continue;
				}
				const int32 I = Room.Index(Next);
				if (!Room.Reached[I] && !Rock[I])
				{
					Room.Reached[I] = 1;
					Queue.Add(I);
				}
			}
		}

		// Squared distance (cells) from each centre to the nearest solid centre: separable, one axis at a time.
		constexpr float Inf = 1e20f;
		TArray<float> F;
		F.SetNumUninitialized(Count);
		for (int32 i = 0; i < Count; ++i)
		{
			F[i] = Room.Reached[i] ? Inf : 0.f;
		}
		TArray<float> D;
		TArray<int32> V;
		TArray<float> Z;
		for (int32 z = 0; z < N.Z; ++z)
		{
			for (int32 y = 0; y < N.Y; ++y)
			{
				DistanceTransform1D(&F[Room.Index(FIntVector(0, y, z))], N.X, 1, D, V, Z);
			}
		}
		for (int32 z = 0; z < N.Z; ++z)
		{
			for (int32 x = 0; x < N.X; ++x)
			{
				DistanceTransform1D(&F[Room.Index(FIntVector(x, 0, z))], N.Y, N.X, D, V, Z);
			}
		}
		for (int32 y = 0; y < N.Y; ++y)
		{
			for (int32 x = 0; x < N.X; ++x)
			{
				DistanceTransform1D(&F[Room.Index(FIntVector(x, y, 0))], N.Z, N.X * N.Y, D, V, Z);
			}
		}

		// The grid's outside counts as solid (the next chunk's rock may stand right past it).
		const float Diagonal = Room.CellCm * UE_SQRT_3;
		Queue.Reset();
		for (int32 i = 0; i < Count; ++i)
		{
			const FIntVector C = Room.CellOf(i);
			const int32 Edge = FMath::Min3(FMath::Min(C.X + 1, N.X - C.X), FMath::Min(C.Y + 1, N.Y - C.Y), FMath::Min(C.Z + 1, N.Z - C.Z));
			const float Cells = FMath::Min(FMath::Sqrt(F[i]), float(Edge));
			Room.DistCm[i] = Cells * Room.CellCm;
			if (Room.Reached[i] && Room.DistCm[i] >= ClearCm + Diagonal)
			{
				Room.Free[i] = 1;
				Room.Nearest[i] = i;
				Queue.Add(i);
				++Room.FreeCount;
			}
		}

		// Every other cell's nearest free cell: a breadth-first spread from all of them (26 neighbours).
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const int32 From = Queue[Head];
			const FIntVector C = Room.CellOf(From);
			for (int32 dz = -1; dz <= 1; ++dz)
			{
				for (int32 dy = -1; dy <= 1; ++dy)
				{
					for (int32 dx = -1; dx <= 1; ++dx)
					{
						const FIntVector Next = C + FIntVector(dx, dy, dz);
						if (Next.X < 0 || Next.Y < 0 || Next.Z < 0 || Next.X >= N.X || Next.Y >= N.Y || Next.Z >= N.Z)
						{
							continue;
						}
						const int32 I = Room.Index(Next);
						if (Room.Nearest[I] == INDEX_NONE)
						{
							Room.Nearest[I] = Room.Nearest[From];
							Queue.Add(I);
						}
					}
				}
			}
		}
	}

	FVector KeepInRoom(const FDeepCaveRoom& Room, const FVector& LocalCm)
	{
		if (!Room.IsValid() || Room.FreeCount == 0)
		{
			return LocalCm;
		}
		const int32 I = Room.Index(Room.CellAt(LocalCm));
		const int32 Target = Room.Nearest[I];
		if (Target == INDEX_NONE)
		{
			return LocalCm;
		}
		// The nearest point of that cell (a hair inside it, so the kept point reads as in it again).
		const FBox Box = Room.CellBox(Room.CellOf(Target)).ExpandBy(-0.01 * Room.CellCm);
		return Box.GetClosestPointTo(LocalCm);
	}

	float FreeShareOfCircle(const FDeepCaveRoom& Room, const FVector& CentreCm, float RadiusCm, int32 Samples)
	{
		if (!Room.IsValid() || Samples <= 0)
		{
			return 0.f;
		}
		int32 InFree = 0;
		for (int32 i = 0; i < Samples; ++i)
		{
			const double A = 2.0 * PI * i / Samples;
			const FVector P = CentreCm + FVector(FMath::Cos(A), FMath::Sin(A), 0.0) * RadiusCm;
			const FBox Grid(Room.OriginCm, Room.OriginCm + FVector(Room.Dims) * Room.CellCm);
			InFree += (Grid.IsInsideOrOn(P) && Room.Free[Room.Index(Room.CellAt(P))]) ? 1 : 0;
		}
		return float(InFree) / Samples;
	}
}
