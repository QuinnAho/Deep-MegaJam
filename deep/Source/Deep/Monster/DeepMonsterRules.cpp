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
		else if (In.bFirstSightPending && (In.Noise >= Tuning.StalkNoise || !In.bDiverTethered || In.StateSeconds >= Tuning.FirstSightAfterS))
		{
			// Before its first hunt it shows itself once: a sound, then part of it at the edge of the lamp.
			Out.Next = EDeepMonsterState::Pass;
			Out.bPassStarted = true;
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

	case EDeepMonsterState::Pass:
		if (In.LitSeconds >= Tuning.PassLitToRecoilS)
		{
			// The lamp works here too: the first lesson.
			Out.Next = EDeepMonsterState::Recoil;
			Out.bRepelled = true;
		}
		else if (In.bPassComplete || In.StateSeconds >= Tuning.PassMaxS)
		{
			// Gone into the dark; normal hunting from here.
			Out.Next = EDeepMonsterState::Lurk;
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

namespace
{
	/** Out's direction, turned toward In until it is within the limit's angle of it (In is a unit vector). */
	FVector BendClamped(const FVector& In, const FVector& Out, double CosLimit, double SinLimit)
	{
		const FVector Dir = Out.GetSafeNormal(UE_SMALL_NUMBER, In);
		const double Cos = FVector::DotProduct(In, Dir);
		if (Cos >= CosLimit)
		{
			return Dir;
		}
		FVector Perp = Dir - In * Cos;
		if (!Perp.Normalize())
		{
			Perp = FVector::CrossProduct(In, FVector::UpVector);
			if (!Perp.Normalize())
			{
				Perp = FVector::CrossProduct(In, FVector::RightVector).GetSafeNormal();
			}
		}
		return In * CosLimit + Perp * SinLimit;
	}

	bool ChainIsSound(const FDeepSerpentChain& Chain)
	{
		const int32 N = Chain.Pos.Num();
		return N >= 2 && Chain.Old.Num() == N && Chain.RestCm.Num() == N - 1;
	}
}

void UDeepMonsterRules::LayChain(FDeepSerpentChain& Chain, const TArray<FVector>& PathCm, float StartCm)
{
	const int32 N = Chain.RestCm.Num() + 1;
	Chain.Pos.SetNum(N);
	float Along = StartCm;
	for (int32 i = 0; i < N; ++i)
	{
		if (i > 0)
		{
			Along += Chain.RestCm[i - 1];
		}
		Chain.Pos[i] = PointAlongTrail(PathCm, Along);
	}
	Chain.Old = Chain.Pos;
	Chain.CarryS = 0.f;
}

int32 UDeepMonsterRules::StepChain(FDeepSerpentChain& Chain, const FVector& PinCm, const FVector& PinForward, const TArray<FVector>& PathCm,
	float StartCm, const FDeepSerpentChainTuning& Tuning, float DeltaSeconds)
{
	return StepChain(Chain, PinCm, PinForward, PathCm, StartCm, Tuning, DeltaSeconds, [](const FVector& P) { return P; });
}

int32 UDeepMonsterRules::StepChain(FDeepSerpentChain& Chain, const FVector& PinCm, const FVector& PinForward, const TArray<FVector>& PathCm,
	float StartCm, const FDeepSerpentChainTuning& Tuning, float DeltaSeconds, TFunctionRef<FVector(const FVector&)> Keep)
{
	if (!ChainIsSound(Chain) || PinCm.ContainsNaN() || PinForward.ContainsNaN())
	{
		return 0;
	}
	const int32 N = Chain.Pos.Num();
	TArray<FVector>& Pos = Chain.Pos;
	TArray<FVector>& Old = Chain.Old;
	const TArray<float>& Rest = Chain.RestCm;

	// Fixed substeps; a long frame is clamped so a hitch never throws the body.
	const float H = FMath::Max(0.001f, Tuning.SubstepS);
	const float MaxFrame = FMath::Max(H, Tuning.MaxFrameS);
	const float Dt = FMath::IsFinite(DeltaSeconds) ? FMath::Clamp(DeltaSeconds, 0.f, MaxFrame) : 0.f;
	Chain.CarryS = FMath::Clamp(Chain.CarryS + Dt, 0.f, MaxFrame + H);
	const int32 Steps = FMath::FloorToInt(Chain.CarryS / H);
	Chain.CarryS -= Steps * H;
	if (Steps <= 0)
	{
		// No substep this frame: the pin still follows the head, so the neck never lags a frame.
		Pos[0] = PinCm;
		Old[0] = PinCm;
		return 0;
	}

	const FVector Back = -PinForward.GetSafeNormal(UE_SMALL_NUMBER, -(Pos[1] - Pos[0]).GetSafeNormal(UE_SMALL_NUMBER, -FVector::ForwardVector));
	const double Limit = FMath::DegreesToRadians(FMath::Clamp(Tuning.BendLimitDeg, 1.f, 179.f));
	const double CosL = FMath::Cos(Limit);
	const double SinL = FMath::Sin(Limit);
	const float Damp = FMath::Exp(-FMath::Max(0.f, Tuning.DragPerS) * H);
	const float Pull = PathCm.Num() > 0 ? 1.f - FMath::Exp(-FMath::Max(0.f, Tuning.TrailPullPerS) * H) : 0.f;
	const int32 Iterations = FMath::Clamp(Tuning.Iterations, 1, 16);

	// Where on the head's path each particle belongs (the path does not change within a frame).
	TArray<FVector, TInlineAllocator<72>> Targets;
	if (Pull > 0.f)
	{
		Targets.SetNum(N);
		float Along = StartCm;
		for (int32 i = 0; i < N; ++i)
		{
			Along += i > 0 ? Rest[i - 1] : 0.f;
			Targets[i] = PointAlongTrail(PathCm, Along);
		}
	}

	const FVector PinFrom = Pos[0];
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		// Verlet: x' = x + (x - x_old) * drag. Neutrally buoyant: no gravity.
		for (int32 i = 1; i < N; ++i)
		{
			const FVector Vel = (Pos[i] - Old[i]) * Damp;
			Old[i] = Pos[i];
			Pos[i] += Vel;
		}
		Pos[0] = FMath::Lerp(PinFrom, PinCm, (Step + 1) / static_cast<float>(Steps));
		Old[0] = Pos[0];

		// A light pull onto the head's path: in a winding shaft the body goes where the head went.
		if (Pull > 0.f)
		{
			for (int32 i = 1; i < N; ++i)
			{
				Pos[i] += (Targets[i] - Pos[i]) * Pull;
			}
		}

		// Relax the links (Jakobsen): each pair shares the correction; the pinned neck gives none.
		for (int32 It = 0; It < Iterations; ++It)
		{
			for (int32 i = 0; i + 1 < N; ++i)
			{
				FVector Delta = Pos[i + 1] - Pos[i];
				const double Len = Delta.Size();
				const FVector Dir = Len > UE_SMALL_NUMBER ? Delta / Len : Back;
				const FVector Corr = Dir * (Len - Rest[i]);
				if (i == 0)
				{
					Pos[1] -= Corr;
				}
				else
				{
					Pos[i] += Corr * 0.5f;
					Pos[i + 1] -= Corr * 0.5f;
				}
			}
		}

		// Head to tail: every joint within the bend limit, every link at its rest length, then out of the rock.
		for (int32 i = 0; i + 1 < N; ++i)
		{
			const FVector In = i == 0 ? Back : (Pos[i] - Pos[i - 1]).GetSafeNormal(UE_SMALL_NUMBER, Back);
			Pos[i + 1] = Pos[i] + BendClamped(In, Pos[i + 1] - Pos[i], CosL, SinL) * Rest[i];
		}
		for (int32 i = 1; i < N; ++i)
		{
			Pos[i] = Keep(Pos[i]);
		}
	}

	for (int32 i = 0; i < N; ++i)
	{
		if (Pos[i].ContainsNaN() || Old[i].ContainsNaN())
		{
			TArray<FVector> Straight = { PinCm, PinCm + Back * 100.f };
			LayChain(Chain, Straight, 0.f);
			break;
		}
	}
	return Steps;
}

void UDeepMonsterRules::KickChain(FDeepSerpentChain& Chain, const FVector& Up, float KickCmPerS, float WaveCm, float SubstepS)
{
	if (!ChainIsSound(Chain))
	{
		return;
	}
	const int32 N = Chain.Pos.Num();
	float Total = 0.f;
	for (const float R : Chain.RestCm)
	{
		Total += R;
	}
	float Along = 0.f;
	for (int32 i = 1; i < N; ++i)
	{
		Along += Chain.RestCm[i - 1];
		const FVector Link = Chain.Pos[i] - Chain.Pos[i - 1];
		FVector Side = FVector::CrossProduct(Up, Link);
		if (!Side.Normalize())
		{
			continue;
		}
		// A cosine, so the tail always gets the whole kick whatever the body length.
		const float Wave = FMath::Cos(2.f * PI * Along / FMath::Max(1.f, WaveCm));
		const float Kick = KickCmPerS * (Along / FMath::Max(1.f, Total)) * Wave;
		Chain.Old[i] -= Side * (Kick * FMath::Max(0.f, SubstepS));
	}
}

float UDeepMonsterRules::ChainBendDeg(const FDeepSerpentChain& Chain, int32 Joint, const FVector& PinForward)
{
	if (!ChainIsSound(Chain) || Joint < 0 || Joint + 1 >= Chain.Pos.Num())
	{
		return 0.f;
	}
	const FVector In = Joint == 0 ? -PinForward.GetSafeNormal() : (Chain.Pos[Joint] - Chain.Pos[Joint - 1]).GetSafeNormal();
	const FVector Out = (Chain.Pos[Joint + 1] - Chain.Pos[Joint]).GetSafeNormal();
	return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(In, Out), -1.0, 1.0)));
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
	case EDeepMonsterState::Pass: return TEXT("Pass");
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

// ---------------------------------------------------------------- the well

bool UDeepMonsterRules::IsInWell(const FDeepWell& Well, const FVector& PointCm, double MarginCm)
{
	if (!Well.bValid)
	{
		return false;
	}
	const double Off = FVector2D::Distance(FVector2D(PointCm.X, PointCm.Y), Well.AxisCm);
	return Off <= Well.RadiusCm - MarginCm && PointCm.Z >= Well.FloorZCm && PointCm.Z <= Well.TopZCm;
}

float UDeepMonsterRules::WellAxisDistanceM(const FDeepWell& Well, const FVector& PointCm)
{
	return static_cast<float>(FVector2D::Distance(FVector2D(PointCm.X, PointCm.Y), Well.AxisCm) / CmPerM);
}

FVector UDeepMonsterRules::KeepInWell(const FDeepWell& Well, const FVector& AimCm, const FDeepMonsterTuning& Tuning, float WallClearM)
{
	if (!Well.bValid)
	{
		return AimCm;
	}
	const double MaxOff = FMath::Max(0.0, Well.RadiusCm - (WallClearM >= 0.f ? WallClearM : Tuning.WellWallClearM) * CmPerM);
	FVector2D Off = FVector2D(AimCm.X, AimCm.Y) - Well.AxisCm;
	if (Off.Size() > MaxOff)
	{
		Off = Off.GetSafeNormal() * MaxOff;
	}
	const double Z = FMath::Max(AimCm.Z, Well.FloorZCm + Tuning.WellFloorClearM * CmPerM);
	return FVector(Well.AxisCm.X + Off.X, Well.AxisCm.Y + Off.Y, Z);
}

float UDeepMonsterRules::WellHoldBelowM(float RiseMps, const FDeepMonsterTuning& Tuning)
{
	const float Stalk = FMath::Max(0.1f, Tuning.StalkSpeedMps);
	const float OpenWaterApproachS = FMath::Max(0.f, Tuning.LurkRadiusM - Tuning.BiteReachM) / Stalk;
	const float Closing = FMath::Clamp(Stalk - FMath::Max(0.f, RiseMps), 0.f, Stalk);
	return FMath::Clamp(Tuning.BiteReachM + OpenWaterApproachS * Closing, 2.f * Tuning.BiteReachM, FMath::Max(2.f * Tuning.BiteReachM, Tuning.LurkRadiusM));
}

FVector UDeepMonsterRules::WellAimPoint(EDeepMonsterState State, const FDeepWell& Well, const FVector& HeadCm, const FVector& DiverCm,
	const FVector& OpenWaterAimCm, const FVector2D& SideDir, float RiseMps, const FDeepMonsterTuning& Tuning)
{
	if (!Well.bValid)
	{
		return OpenWaterAimCm;
	}
	FVector Aim = OpenWaterAimCm;
	switch (State)
	{
	case EDeepMonsterState::Lurk:
	{
		// Below the diver at the hold depth, circling the axis slowly: the head keeps turning up into the edge of the beam.
		const double HoldZ = DiverCm.Z - WellHoldBelowM(RiseMps, Tuning) * CmPerM;
		FVector2D Rel = FVector2D(HeadCm.X, HeadCm.Y) - Well.AxisCm;
		if (Rel.IsNearlyZero())
		{
			Rel = FVector2D(1.0, 0.0);
		}
		const double Angle = FMath::Atan2(Rel.Y, Rel.X) + FMath::DegreesToRadians(60.0);
		const FVector2D On = Well.AxisCm + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * (Tuning.WellLurkCircleM * CmPerM);
		Aim = FVector(On.X, On.Y, HoldZ);
		break;
	}
	case EDeepMonsterState::Stalk:
	{
		// Up the shaft from below, head first, to the diver's end of the line, a little to one side of it.
		const FVector2D Side = SideDir.GetSafeNormal() * (Tuning.WellStalkSideM * CmPerM);
		Aim = FVector(DiverCm.X + Side.X, DiverCm.Y + Side.Y, DiverCm.Z - 0.5 * CmPerM);
		break;
	}
	case EDeepMonsterState::Recoil:
	{
		// Back down the shaft, not through its wall: the head turns down and it dives for the bottom of the axis. A fixed
		// point, so it arrives and stops there; an aim that moved with the head made it circle at 10 m/s at the floor and
		// swing a third of the body into the stone (DEEPWELL log, Nov 15).
		Aim = FVector(Well.AxisCm.X, Well.AxisCm.Y, Well.FloorZCm);
		break;
	}
	default:
		break;
	}
	return KeepInWell(Well, Aim, Tuning);
}
