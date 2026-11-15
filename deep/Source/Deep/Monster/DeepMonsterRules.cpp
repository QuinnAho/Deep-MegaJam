#include "Monster/DeepMonsterRules.h"

#include "Perception/AISense_Hearing.h"

namespace
{
	using DeepMonster::CmPerM;
}

float UDeepMonsterRules::SprintThresholdMps(float SwimSpeedMps, float SprintMultiplier)
{
	return SwimSpeedMps * (1.f + 0.5f * FMath::Max(0.f, SprintMultiplier - 1.f));
}

float UDeepMonsterRules::NoiseFromDiver(bool bWinching, float SpeedMps, float SprintSpeedMps, float CargoGainKg, float DeltaSeconds, const FDeepMonsterTuning& Tuning)
{
	const float Dt = FMath::Max(0.f, DeltaSeconds);
	float Noise = 0.f;
	if (bWinching)
	{
		Noise += Tuning.WinchNoisePerS * Dt;
	}
	else if (SpeedMps > SprintSpeedMps)
	{
		// Winching moves the diver fast too; that is already counted above.
		Noise += Tuning.SprintNoisePerS * Dt;
	}
	if (CargoGainKg > KINDA_SMALL_NUMBER)
	{
		Noise += Tuning.GrabNoise + Tuning.GrabNoisePerKg * CargoGainKg;
	}
	return Noise;
}

float UDeepMonsterRules::NoiseStep(float Noise, float Added, float DeltaSeconds, const FDeepMonsterTuning& Tuning)
{
	const float Next = Noise + FMath::Max(0.f, Added) - Tuning.NoiseDecayPerS * FMath::Max(0.f, DeltaSeconds);
	return FMath::Clamp(Next, 0.f, Tuning.NoiseMax);
}

bool UDeepMonsterRules::IsInBeam(const FVector& LampCm, const FVector& LampDirection, float HalfAngleDeg, float RangeM, const FVector& PointCm, float PointRadiusM)
{
	const FVector Dir = LampDirection.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		return false;
	}
	const FVector To = PointCm - LampCm;
	const float DistM = static_cast<float>(To.Size()) / CmPerM;
	if (DistM > RangeM)
	{
		return false;
	}
	if (DistM < KINDA_SMALL_NUMBER)
	{
		return true;
	}
	const float Cos = static_cast<float>(FVector::DotProduct(To / (DistM * CmPerM), Dir));
	if (Cos <= 0.f)
	{
		return false;
	}
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Cos, -1.f, 1.f)));
	const float AllowedDeg = HalfAngleDeg + FMath::RadiansToDegrees(FMath::Atan2(FMath::Max(0.f, PointRadiusM), DistM));
	return AngleDeg <= AllowedDeg;
}

float UDeepMonsterRules::LitStep(float LitSeconds, bool bLitNow, float DeltaSeconds, const FDeepMonsterTuning& Tuning)
{
	const float Dt = FMath::Max(0.f, DeltaSeconds);
	return bLitNow ? LitSeconds + Dt : FMath::Max(0.f, LitSeconds - Tuning.LitDecayPerS * Dt);
}

FDeepMonsterStep UDeepMonsterRules::NextState(const FDeepMonsterInputs& In, const FDeepMonsterTuning& Tuning)
{
	FDeepMonsterStep Out;
	Out.Next = In.State;
	if (!In.bHasDiver)
	{
		return Out;
	}

	const bool bDrivenOff = In.LitSeconds >= Tuning.LitToRecoilS;
	switch (In.State)
	{
	case EDeepMonsterState::Lurk:
		if (bDrivenOff)
		{
			// Not "repelled": it was not coming for anyone.
			Out.Next = EDeepMonsterState::Recoil;
		}
		else if (In.CooldownS > 0.f)
		{
			// Still shaken from the lamp: nothing brings it in yet.
		}
		else if (!In.bDiverTethered)
		{
			Out.Next = EDeepMonsterState::Hunt;
		}
		else if (In.Noise >= Tuning.StalkNoise)
		{
			Out.Next = EDeepMonsterState::Stalk;
		}
		break;

	case EDeepMonsterState::Stalk:
		if (bDrivenOff)
		{
			Out.Next = EDeepMonsterState::Recoil;
			Out.bRepelled = true;
		}
		else if (!In.bDiverTethered)
		{
			Out.Next = EDeepMonsterState::Hunt;
		}
		else if (In.TetherDistanceM <= Tuning.BiteReachM)
		{
			Out.Next = EDeepMonsterState::Bite;
			Out.bBiteStarted = true;
		}
		else if (In.Noise < Tuning.CalmNoise)
		{
			Out.Next = EDeepMonsterState::Lurk;
		}
		break;

	case EDeepMonsterState::Bite:
		if (In.bLitNow)
		{
			// The lamp always works on a bite, at once.
			Out.Next = EDeepMonsterState::Recoil;
			Out.bBiteBroken = true;
			Out.bRepelled = true;
		}
		else if (!In.bDiverTethered)
		{
			// Something else cut the line; nothing left to hold.
			Out.Next = EDeepMonsterState::Recoil;
		}
		else if (In.StateSeconds >= Tuning.BiteAlarmS)
		{
			Out.Next = EDeepMonsterState::Recoil;
			Out.bSevered = true;
		}
		break;

	case EDeepMonsterState::Recoil:
		if (In.TetherDistanceM >= Tuning.LurkRadiusM || In.StateSeconds >= Tuning.RecoilMaxS)
		{
			Out.Next = EDeepMonsterState::Lurk;
			Out.bStartCooldown = true;
		}
		break;

	case EDeepMonsterState::Hunt:
		if (bDrivenOff)
		{
			Out.Next = EDeepMonsterState::Recoil;
			Out.bRepelled = true;
		}
		else if (In.bDiverTethered)
		{
			Out.Next = EDeepMonsterState::Lurk;
		}
		else if (In.DiverDistanceM <= Tuning.CatchReachM)
		{
			Out.Next = EDeepMonsterState::Recoil;
			Out.bCaught = true;
		}
		break;
	}
	return Out;
}

float UDeepMonsterRules::Threat01(EDeepMonsterState State, float DiverDistanceM, const FDeepMonsterTuning& Tuning)
{
	if (State == EDeepMonsterState::Bite)
	{
		return 1.f;
	}
	const float Span = FMath::Max(KINDA_SMALL_NUMBER, Tuning.ThreatFarM - Tuning.CatchReachM);
	return 1.f - FMath::Clamp((DiverDistanceM - Tuning.CatchReachM) / Span, 0.f, 1.f);
}

float UDeepMonsterRules::DistanceToSegmentM(const FVector& PointCm, const FVector& ACm, const FVector& BCm)
{
	return static_cast<float>(FMath::PointDistToSegment(PointCm, ACm, BCm)) / CmPerM;
}

float UDeepMonsterRules::SegmentAlpha(const FVector& PointCm, const FVector& ACm, const FVector& BCm)
{
	const FVector AB = BCm - ACm;
	const double LenSq = AB.SizeSquared();
	if (LenSq < KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	return FMath::Clamp(static_cast<float>(FVector::DotProduct(PointCm - ACm, AB) / LenSq), 0.f, 1.f);
}

FVector UDeepMonsterRules::OrbitAimPoint(const FVector& CentreCm, const FVector& PosCm, float RadiusM, float LeadDeg)
{
	FVector Rel = PosCm - CentreCm;
	Rel.Z = 0.f;
	if (Rel.IsNearlyZero())
	{
		Rel = FVector::ForwardVector;
	}
	const float Angle = static_cast<float>(FMath::Atan2(Rel.Y, Rel.X)) + FMath::DegreesToRadians(LeadDeg);
	return CentreCm + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (RadiusM * CmPerM);
}

FVector UDeepMonsterRules::SpiralAimPoint(const FVector& TargetCm, const FVector& PosCm, float ArcDeg, float InwardFraction, float DirectWithinM)
{
	const FVector Rel = PosCm - TargetCm;
	if (Rel.Size() / CmPerM <= DirectWithinM)
	{
		return TargetCm;
	}
	return TargetCm + Rel.RotateAngleAxis(ArcDeg, FVector::UpVector) * FMath::Clamp(InwardFraction, 0.f, 1.f);
}

FVector UDeepMonsterRules::TurnToward(const FVector& Heading, const FVector& Wanted, float MaxDegrees)
{
	const FVector H = Heading.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	const FVector W = Wanted.GetSafeNormal();
	if (W.IsNearlyZero())
	{
		return H;
	}
	const float AngleRad = FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(H, W)), -1.f, 1.f));
	const float MaxRad = FMath::DegreesToRadians(FMath::Max(0.f, MaxDegrees));
	if (AngleRad <= MaxRad)
	{
		return W;
	}
	FVector Axis = FVector::CrossProduct(H, W);
	if (Axis.IsNearlyZero())
	{
		// Directly behind: turn around the vertical (or any perpendicular when heading straight up or down).
		Axis = FMath::Abs(H.Z) < 0.99f ? FVector::UpVector : FVector::RightVector;
	}
	return FQuat(Axis.GetSafeNormal(), MaxRad).RotateVector(H).GetSafeNormal();
}

FVector UDeepMonsterRules::PointAlongTrail(const TArray<FVector>& TrailCm, float DistanceCm)
{
	if (TrailCm.Num() == 0)
	{
		return FVector::ZeroVector;
	}
	if (TrailCm.Num() == 1 || DistanceCm <= 0.f)
	{
		return TrailCm[0];
	}
	float Left = DistanceCm;
	for (int32 i = 1; i < TrailCm.Num(); ++i)
	{
		const FVector Step = TrailCm[i] - TrailCm[i - 1];
		const float Len = static_cast<float>(Step.Size());
		if (Len >= Left && Len > KINDA_SMALL_NUMBER)
		{
			return TrailCm[i - 1] + Step * (Left / Len);
		}
		Left -= Len;
	}
	// Past the end: carry on straight along the last piece.
	const int32 N = TrailCm.Num();
	const FVector LastDir = (TrailCm[N - 1] - TrailCm[N - 2]).GetSafeNormal(UE_SMALL_NUMBER, -FVector::ForwardVector);
	return TrailCm[N - 1] + LastDir * Left;
}

FString UDeepMonsterRules::StateName(EDeepMonsterState State)
{
	switch (State)
	{
	case EDeepMonsterState::Lurk: return TEXT("Lurk");
	case EDeepMonsterState::Stalk: return TEXT("Stalk");
	case EDeepMonsterState::Recoil: return TEXT("Recoil");
	case EDeepMonsterState::Bite: return TEXT("Bite");
	case EDeepMonsterState::Hunt: return TEXT("Hunt");
	}
	return TEXT("Unknown");
}

void UDeepMonsterRules::ReportDiverNoise(UObject* WorldContextObject, FVector LocationCm, float Loudness, AActor* Instigator)
{
	if (WorldContextObject && Loudness > 0.f)
	{
		// MaxRange 0: the listener's HearingRange (scaled by loudness) decides who hears it.
		UAISense_Hearing::ReportNoiseEvent(WorldContextObject, LocationCm, Loudness, Instigator, 0.f, TEXT("DeepNoise"));
	}
}
