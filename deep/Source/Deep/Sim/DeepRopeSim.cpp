#include "Sim/DeepRopeSim.h"

int32 FDeepRopeSim::CountLinked() const
{
	int32 Count = 0;
	for (const uint8 L : Linked)
	{
		Count += L ? 1 : 0;
	}
	return Count;
}

void FDeepRopeSim::Init(int32 Points, const FVector& A, const FVector& B)
{
	Points = FMath::Max(2, Points);
	Pos.SetNum(Points);
	Prev.SetNum(Points);
	Linked.Init(1, Points - 1);
	for (int32 i = 0; i < Points; ++i)
	{
		Pos[i] = FMath::Lerp(A, B, static_cast<double>(i) / (Points - 1));
		Prev[i] = Pos[i];
	}
	RestCm = static_cast<float>(FVector::Dist(A, B)) / (Points - 1);
	Accumulator = 0.f;
}

void FDeepRopeSim::Advance(float DeltaSeconds, const FVector* StartPin, const FVector* EndPin, float LengthCm, float SinkCmS2, float DragPerS, int32 Iterations)
{
	if (!IsValid())
	{
		return;
	}
	const int32 Last = Pos.Num() - 1;
	RestCm = FMath::Max(0.1f, LengthCm / Last);

	// Pins hold even on a zero-length frame (teleports, tests).
	auto ApplyPins = [&]()
	{
		if (StartPin)
		{
			Pos[0] = *StartPin;
		}
		if (EndPin)
		{
			Pos[Last] = *EndPin;
		}
	};

	Accumulator = FMath::Min(Accumulator + FMath::Max(0.f, DeltaSeconds), StepSeconds * MaxStepsPerFrame);
	const FVector Sink(0.f, 0.f, -SinkCmS2 * StepSeconds * StepSeconds);
	const double Keep = FMath::Exp(-DragPerS * StepSeconds);
	bool bStepped = false;
	while (Accumulator >= StepSeconds)
	{
		Accumulator -= StepSeconds;
		bStepped = true;
		for (int32 i = 0; i <= Last; ++i)
		{
			const FVector Velocity = (Pos[i] - Prev[i]) * Keep;
			Prev[i] = Pos[i];
			Pos[i] += Velocity + Sink;
		}
		ApplyPins();
		for (int32 Iter = 0; Iter < Iterations; ++Iter)
		{
			for (int32 i = 0; i < Last; ++i)
			{
				if (!Linked[i])
				{
					continue;
				}
				const bool bFixedA = (i == 0 && StartPin);
				const bool bFixedB = (i + 1 == Last && EndPin);
				if (bFixedA && bFixedB)
				{
					continue;
				}
				const FVector Delta = Pos[i + 1] - Pos[i];
				const double Dist = Delta.Size();
				if (Dist < UE_KINDA_SMALL_NUMBER)
				{
					continue;
				}
				// A rope pushes back only when stretched: slack is free to fold.
				const double Error = Dist - RestCm;
				if (Error <= 0.0)
				{
					continue;
				}
				const FVector Fix = Delta * (Error / Dist);
				if (bFixedA)
				{
					Pos[i + 1] -= Fix;
				}
				else if (bFixedB)
				{
					Pos[i] += Fix;
				}
				else
				{
					Pos[i] += Fix * 0.5;
					Pos[i + 1] -= Fix * 0.5;
				}
			}
			// A little bending stiffness: each free point inside a joined run eases toward the middle of its
			// neighbours, so a fold rounds over several links instead of kinking at one joint.
			if (BendStiffness > 0.f)
			{
				for (int32 i = 1; i < Last; ++i)
				{
					if (Linked[i - 1] && Linked[i])
					{
						Pos[i] += ((Pos[i - 1] + Pos[i + 1]) * 0.5 - Pos[i]) * BendStiffness;
					}
				}
				ApplyPins();
			}
		}
	}
	if (!bStepped)
	{
		ApplyPins();
	}
}

int32 FDeepRopeSim::NearestLink(const FVector& P) const
{
	int32 Best = INDEX_NONE;
	double BestDistSq = TNumericLimits<double>::Max();
	for (int32 i = 0; i < NumLinks(); ++i)
	{
		if (!Linked[i])
		{
			continue;
		}
		const FVector Closest = FMath::ClosestPointOnSegment(P, Pos[i], Pos[i + 1]);
		const double DistSq = FVector::DistSquared(Closest, P);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = i;
		}
	}
	return Best;
}

void FDeepRopeSim::Cut(int32 Link, float WhipCmS, int32 WhipPoints)
{
	if (!IsLinked(Link))
	{
		return;
	}
	Linked[Link] = 0;
	const FVector Along = (Pos[Link + 1] - Pos[Link]).GetSafeNormal();
	// A sideways kick perpendicular to the line, so the ends snake instead of sliding along themselves.
	FVector Side = FVector::CrossProduct(Along, FVector::UpVector);
	if (Side.IsNearlyZero())
	{
		Side = FVector::CrossProduct(Along, FVector::ForwardVector);
	}
	Side.Normalize();
	WhipPoints = FMath::Max(1, WhipPoints);
	for (int32 k = 0; k < WhipPoints; ++k)
	{
		const float Fade = 1.f - static_cast<float>(k) / WhipPoints;
		const float Speed = WhipCmS * Fade * StepSeconds;
		// The upper end recoils back toward the start, the lower end toward the end; the kicks go opposite ways.
		const int32 Up = Link - k;
		if (Pos.IsValidIndex(Up))
		{
			Prev[Up] = Pos[Up] - (-Along + Side) * Speed;
		}
		const int32 Down = Link + 1 + k;
		if (Pos.IsValidIndex(Down))
		{
			Prev[Down] = Pos[Down] - (Along - Side) * Speed;
		}
	}
}

float FDeepRopeSim::LengthTo(int32 Index) const
{
	float Sum = 0.f;
	for (int32 i = 0; i < Index && i + 1 < Pos.Num(); ++i)
	{
		Sum += static_cast<float>(FVector::Dist(Pos[i], Pos[i + 1]));
	}
	return Sum;
}
