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

		// One pool: only the largest set of free cells joined by faces stays free (a pocket by the seed, too tight to
		// leave, would hold the serpent in place; two pools would let it jump through rock between them).
		if (Queue.Num() > 0)
		{
			TArray<int32> Pool;
			Pool.Init(INDEX_NONE, Count);
			TArray<int32> Flood;
			Flood.Reserve(Queue.Num());
			int32 BestLabel = INDEX_NONE;
			int32 BestSize = 0;
			int32 Label = 0;
			for (const int32 Root : Queue)
			{
				if (Pool[Root] != INDEX_NONE)
				{
					continue;
				}
				Flood.Reset();
				Flood.Add(Root);
				Pool[Root] = Label;
				for (int32 Head = 0; Head < Flood.Num(); ++Head)
				{
					const FIntVector C = Room.CellOf(Flood[Head]);
					for (const FIntVector& Fc : Faces)
					{
						const FIntVector Next = C + Fc;
						if (Next.X < 0 || Next.Y < 0 || Next.Z < 0 || Next.X >= N.X || Next.Y >= N.Y || Next.Z >= N.Z)
						{
							continue;
						}
						const int32 I = Room.Index(Next);
						if (Room.Free[I] && Pool[I] == INDEX_NONE)
						{
							Pool[I] = Label;
							Flood.Add(I);
						}
					}
				}
				if (Flood.Num() > BestSize)
				{
					BestSize = Flood.Num();
					BestLabel = Label;
				}
				++Label;
			}
			Room.PoolCount = Label;
			TArray<int32> Kept;
			Kept.Reserve(BestSize);
			for (const int32 I : Queue)
			{
				if (Pool[I] == BestLabel)
				{
					Kept.Add(I);
				}
				else
				{
					Room.Free[I] = 0;
					Room.Nearest[I] = INDEX_NONE;
				}
			}
			Room.FreeCount = Kept.Num();
			Queue = MoveTemp(Kept);
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

	float FitLurkRadiusCm(const FDeepCaveRoom& Room, const FVector& CentreCm, float MinCm, float MaxCm, float StepCm, float MinShare, int32 Samples)
	{
		const float Step = FMath::Max(1.f, StepCm);
		const float Lo = FMath::Max(0.f, MinCm);
		const float Hi = FMath::Max(Lo, MaxCm);
		float Best = Lo;
		float BestShare = -1.f;
		for (float R = Hi; R >= Lo - 0.01f; R -= Step)
		{
			const float Share = FreeShareOfCircle(Room, CentreCm, R, Samples);
			if (Share >= MinShare)
			{
				return R;
			}
			if (Share >= BestShare)
			{
				BestShare = Share;
				Best = R;
			}
		}
		return Best;
	}

	bool FitLurkCircle(const FDeepCaveRoom& Room, const FVector& SocketCm, float ReachCm, float StrideCm, float MinCm, float MaxCm, float StepCm,
		float MinShare, int32 Samples, FVector& OutCentreCm, float& OutRadiusCm)
	{
		const float Step = FMath::Max(1.f, StepCm);
		const float Lo = FMath::Max(0.f, MinCm);
		const float Hi = FMath::Max(Lo, MaxCm);
		const float Stride = FMath::Max(1.f, StrideCm);
		const int32 Span = FMath::FloorToInt32(FMath::Max(0.f, ReachCm) / Stride);
		bool bFound = false;
		float BestR = 0.f;
		double BestSq = 0.0;
		FVector BestCentre = SocketCm;
		if (Room.IsValid() && Room.FreeCount > 0)
		{
			for (int32 z = -Span; z <= Span; ++z)
			{
				for (int32 y = -Span; y <= Span; ++y)
				{
					for (int32 x = -Span; x <= Span; ++x)
					{
						const FVector Offset = FVector(x, y, z) * Stride;
						const double Sq = Offset.SizeSquared();
						if (Sq > FMath::Square(double(ReachCm)) + 1.0)
						{
							continue;
						}
						const FVector Centre = SocketCm + Offset;
						if (!Room.Free[Room.Index(Room.CellAt(Centre))])
						{
							continue;
						}
						// Only radii that would win: wider than the best, or as wide and nearer the socket.
						const float Floor = bFound ? BestR : Lo;
						for (float R = Hi; R >= Floor - 0.01f; R -= Step)
						{
							if (FreeShareOfCircle(Room, Centre, R, Samples) >= MinShare)
							{
								if (!bFound || R > BestR + 0.01f || Sq < BestSq)
								{
									bFound = true;
									BestR = R;
									BestSq = Sq;
									BestCentre = Centre;
								}
								break;
							}
						}
					}
				}
			}
		}
		if (bFound)
		{
			OutCentreCm = BestCentre;
			OutRadiusCm = BestR;
			return true;
		}
		OutCentreCm = SocketCm;
		OutRadiusCm = FitLurkRadiusCm(Room, SocketCm, Lo, Hi, Step, MinShare, Samples);
		return false;
	}

	bool LineFree(const FDeepCaveRoom& Room, const FVector& FromCm, const FVector& ToCm)
	{
		if (!Room.IsValid())
		{
			return true;
		}
		const double Length = FVector::Dist(FromCm, ToCm);
		const int32 Samples = FMath::Max(1, FMath::CeilToInt32(Length / (0.5 * Room.CellCm)));
		for (int32 i = 0; i <= Samples; ++i)
		{
			if (!Room.Free[Room.Index(Room.CellAt(FMath::Lerp(FromCm, ToCm, double(i) / Samples)))])
			{
				return false;
			}
		}
		return true;
	}

	static const FIntVector RouteFaces[6] = { FIntVector(1, 0, 0), FIntVector(-1, 0, 0), FIntVector(0, 1, 0), FIntVector(0, -1, 0), FIntVector(0, 0, 1), FIntVector(0, 0, -1) };

	void BuildRoute(const FDeepCaveRoom& Room, int32 TargetIndex, FDeepCaveRoute& Route)
	{
		Route.Target = TargetIndex;
		Route.Steps.Init(-1, Room.Num());
		if (!Room.IsValid() || !Room.Free.IsValidIndex(TargetIndex) || !Room.Free[TargetIndex])
		{
			return;
		}
		TArray<int32> Queue;
		Queue.Reserve(Room.FreeCount);
		Queue.Add(TargetIndex);
		Route.Steps[TargetIndex] = 0;
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const int32 I = Queue[Head];
			const FIntVector C = Room.CellOf(I);
			for (const FIntVector& F : RouteFaces)
			{
				const FIntVector N = C + F;
				if (N.X < 0 || N.Y < 0 || N.Z < 0 || N.X >= Room.Dims.X || N.Y >= Room.Dims.Y || N.Z >= Room.Dims.Z)
				{
					continue;
				}
				const int32 J = Room.Index(N);
				if (Room.Free[J] && Route.Steps[J] < 0)
				{
					Route.Steps[J] = Route.Steps[I] + 1;
					Queue.Add(J);
				}
			}
		}
	}

	FVector RouteAim(const FDeepCaveRoom& Room, FDeepCaveRoute& Route, const FVector& FromCm, const FVector& ToCm, float LookCm)
	{
		if (!Room.IsValid() || Room.FreeCount == 0)
		{
			return ToCm;
		}
		const FVector From = KeepInRoom(Room, FromCm);
		const FVector To = KeepInRoom(Room, ToCm);
		if (LineFree(Room, From, To))
		{
			return ToCm;
		}
		const int32 Target = Room.Nearest[Room.Index(Room.CellAt(To))];
		const int32 Start = Room.Nearest[Room.Index(Room.CellAt(From))];
		if (Target == INDEX_NONE || Start == INDEX_NONE)
		{
			return ToCm;
		}
		if (Route.Target != Target || Route.Steps.Num() != Room.Num())
		{
			BuildRoute(Room, Target, Route);
		}
		if (Route.Steps[Start] < 0)
		{
			return ToCm;
		}
		// Down the steps toward the target; the farthest cell seen in a free straight line wins.
		const int32 Look = FMath::Max(1, FMath::CeilToInt32(LookCm / Room.CellCm));
		int32 Cell = Start;
		FVector Best = Room.CentreCm(Room.CellOf(Start));
		for (int32 k = 0; k < Look && Route.Steps[Cell] > 0; ++k)
		{
			const FIntVector C = Room.CellOf(Cell);
			int32 Next = INDEX_NONE;
			double NextSq = TNumericLimits<double>::Max();
			for (const FIntVector& F : RouteFaces)
			{
				const FIntVector N = C + F;
				if (N.X < 0 || N.Y < 0 || N.Z < 0 || N.X >= Room.Dims.X || N.Y >= Room.Dims.Y || N.Z >= Room.Dims.Z)
				{
					continue;
				}
				const int32 J = Room.Index(N);
				// Of the steps down, the one nearest the target keeps the route from staircasing away from it.
				const double Sq = FVector::DistSquared(Room.CentreCm(N), To);
				if (Route.Steps[J] == Route.Steps[Cell] - 1 && Sq < NextSq)
				{
					Next = J;
					NextSq = Sq;
				}
			}
			if (Next == INDEX_NONE)
			{
				break;
			}
			Cell = Next;
			const FVector P = Room.CentreCm(Room.CellOf(Cell));
			if (LineFree(Room, From, P))
			{
				Best = P;
			}
		}
		return Best;
	}

	FVector ReachableOnLine(const FDeepCaveRoom& Room, const FVector& AnchorCm, const FVector& DiverEndCm, float FromDiverCm, float ToleranceCm, float StepCm)
	{
		const FVector Line = AnchorCm - DiverEndCm;
		const double Length = Line.Size();
		if (Length < 1.0)
		{
			return DiverEndCm;
		}
		const FVector Dir = Line / Length;
		const double Step = FMath::Max(1.f, StepCm);
		FVector Best = DiverEndCm + Dir * FMath::Clamp<double>(FromDiverCm, 0.0, Length);
		double BestGap = TNumericLimits<double>::Max();
		for (double Along = FMath::Clamp<double>(FromDiverCm, 0.0, Length); ; Along += Step)
		{
			const double At = FMath::Min(Along, Length);
			const FVector P = DiverEndCm + Dir * At;
			const double Gap = FVector::Dist(KeepInRoom(Room, P), P);
			if (Gap <= ToleranceCm)
			{
				return P;
			}
			if (Gap < BestGap)
			{
				BestGap = Gap;
				Best = P;
			}
			if (At >= Length)
			{
				break;
			}
		}
		return Best;
	}
}
