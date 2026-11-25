#include "Sim/DeepMovingSubRules.h"

#include "Sim/DeepCaveRules.h"
#include "Sim/DeepPilotRules.h"
#include "Sim/DeepRules.h"

FDeepIdleBob UDeepMovingSubRules::IdleBob(double TimeS, float Amount01, const FDeepIdleTuning& T)
{
	// Two sines per axis, 0.7 + 0.3 of the amplitude, the second at 1.7x the rate: never in step, never past the amplitude.
	const double A = FMath::Clamp(Amount01, 0.f, 1.f);
	const auto Wave = [TimeS](float Hz, double Phase)
	{
		const double W = UE_DOUBLE_TWO_PI * FMath::Max(0.f, Hz);
		return 0.7 * FMath::Sin(W * TimeS + Phase) + 0.3 * FMath::Sin(1.7 * W * TimeS + Phase * 2.3 + 0.9);
	};
	FDeepIdleBob Bob;
	Bob.HeaveCm = static_cast<float>(A * T.HeaveCm * Wave(T.HeaveHz, 0.0));
	Bob.PitchDeg = static_cast<float>(A * T.PitchDeg * Wave(T.PitchHz, 1.3));
	Bob.RollDeg = static_cast<float>(A * T.RollDeg * Wave(T.RollHz, 2.6));
	Bob.YawDeg = static_cast<float>(A * T.YawDeg * Wave(T.YawHz, 4.1));
	return Bob;
}

float UDeepMovingSubRules::IdleTarget(float SpeedMps, bool bDiverOut, const FDeepIdleTuning& T)
{
	const float Rest = T.FadeOutMps > 0.f ? 1.f - FMath::Clamp(FMath::Abs(SpeedMps) / T.FadeOutMps, 0.f, 1.f) : 1.f;
	return Rest * (bDiverOut ? FMath::Clamp(T.DiverOutShare, 0.f, 1.f) : 1.f);
}

float UDeepMovingSubRules::IdleStep(float Amount01, float Target01, float DeltaSeconds, const FDeepIdleTuning& T)
{
	return Amount01 + (Target01 - Amount01) * (1.f - FMath::Exp(-FMath::Max(0.f, T.BlendPerS) * FMath::Max(0.f, DeltaSeconds)));
}

float UDeepMovingSubRules::DepthFromZ(double ZCm, double DatumZCm)
{
	return static_cast<float>((DatumZCm - ZCm) / UDeepRules::CmPerM);
}

double UDeepMovingSubRules::ZFromDepth(float DepthM, double DatumZCm)
{
	return DatumZCm - static_cast<double>(DepthM) * UDeepRules::CmPerM;
}

FTransform UDeepMovingSubRules::StillMap(const FTransform& Home, const FVector2D& OffsetM, float DepthM, float YawDeg, double DatumZCm)
{
	// Where D-022 would put the root for the whole depth (not the depth below the loaded stop): stop S's content sits
	// S's depth below that root's frame, and the still frame keeps it S's depth below the datum.
	FQuat Turn;
	const FVector At = UDeepPilotRules::HangLocationCm(Home.GetLocation(), Home.GetRotation(), OffsetM, DepthM, YawDeg, Turn);
	const FTransform FullRoot(Turn, At);
	const FTransform StillFrame(FQuat::Identity, FVector(Home.GetLocation().X, Home.GetLocation().Y, DatumZCm));
	return FullRoot.Inverse() * StillFrame;
}

FTransform UDeepMovingSubRules::StillHull(const FTransform& Home, const FVector2D& OffsetM, float DepthM, float YawDeg, double DatumZCm)
{
	return Home * StillMap(Home, OffsetM, DepthM, YawDeg, DatumZCm);
}

FTransform UDeepMovingSubRules::StillStopFrame(const FTransform& Home, float StopDepthM, double DatumZCm)
{
	return FTransform(FQuat::Identity, FVector(Home.GetLocation().X, Home.GetLocation().Y, ZFromDepth(StopDepthM, DatumZCm)));
}

FTransform UDeepMovingSubRules::StillShaft(const FTransform& Home, double DatumZCm)
{
	return FTransform(Home.GetRotation(), FVector(Home.GetLocation().X, Home.GetLocation().Y, DatumZCm));
}

bool UDeepMovingSubRules::StillBedShows(float SubDepthM, float StopDepthM, const FDeepMovingSubTuning& T)
{
	return SubDepthM <= StopDepthM + FMath::Max(0.f, T.BedShowsBelowStopM);
}

FDeepWallHit UDeepMovingSubRules::ResolveWallHit(const FVector& VelocityMps, const FVector& WallNormal, const FDeepCaveTuning& Cave, const FDeepPilotTuning& Pilot)
{
	return ResolveWallHitKeeping(VelocityMps, WallNormal, Cave, Pilot.KnockKeepAlong, Cave.Bounce);
}

FDeepWallHit UDeepMovingSubRules::ResolveWallHitKeeping(const FVector& VelocityMps, const FVector& WallNormal, const FDeepCaveTuning& Cave, float KnockKeepAlong, float Bounce)
{
	FDeepWallHit Hit;
	Hit.VelocityMps = VelocityMps;
	const FVector N = WallNormal.GetSafeNormal();
	if (N.IsZero())
	{
		return Hit;
	}
	const double Into = -FVector::DotProduct(VelocityMps, N);
	if (Into <= 0.0)
	{
		return Hit;
	}
	Hit.ImpactMps = static_cast<float>(Into);
	const FVector Along = VelocityMps + N * Into;
	Hit.Damage = UDeepCaveRules::HitDamage(Hit.ImpactMps, Cave);
	Hit.bKnock = Hit.Damage > 0;
	Hit.bScrape = !Hit.bKnock;
	if (Hit.bKnock)
	{
		Hit.VelocityMps = Along * FMath::Clamp(KnockKeepAlong, 0.f, 1.f) + N * Into * FMath::Max(0.f, Bounce);
	}
	else
	{
		Hit.VelocityMps = Along;
	}
	return Hit;
}

FDeepParkStep UDeepMovingSubRules::ParkNear(const FVector& HullM, float HullYawDeg, const FVector& StopM, float StopYawDeg, float SpeedMps, bool bControlsIdle, float DeltaSeconds, const FDeepPilotTuning& Pilot)
{
	FDeepParkStep Step;
	Step.LocationM = HullM;
	Step.YawDeg = HullYawDeg;
	const FVector Gap = StopM - HullM;
	const float Distance = static_cast<float>(Gap.Size());
	Step.bInRange = bControlsIdle && Distance <= Pilot.ParkNearM && FMath::Abs(SpeedMps) <= Pilot.ParkSpeedMps;
	if (!Step.bInRange)
	{
		return Step;
	}
	// Straight onto the point, the remaining distance gliding to 0; the heading the short way round.
	const float Left = UDeepPilotRules::GlideStep(Distance, 0.f, DeltaSeconds, Pilot.ParkGlideMinMps, Pilot);
	Step.LocationM = Distance > UE_KINDA_SMALL_NUMBER ? StopM - Gap / Distance * Left : StopM;
	const float YawLeft = UDeepPilotRules::GlideStep(FRotator::NormalizeAxis(HullYawDeg - StopYawDeg), 0.f, DeltaSeconds, Pilot.ParkGlideMinDegS, Pilot);
	Step.YawDeg = FRotator::NormalizeAxis(StopYawDeg + YawLeft);
	Step.bArrived = Left <= 0.f && FMath::IsNearlyZero(YawLeft);
	if (Step.bArrived)
	{
		Step.LocationM = StopM;
		Step.YawDeg = FRotator::NormalizeAxis(StopYawDeg);
	}
	return Step;
}

FDeepRoomSway UDeepMovingSubRules::RoomSway(float SpeedMps, float AheadAccelMps2, float TimeS, float KnockAgeS, float KnockMps, const FDeepMovingSubTuning& T)
{
	FDeepRoomSway Sway;
	const float Speed = FMath::Abs(SpeedMps);
	const float MaxDeg = FMath::Max(0.f, T.SwayMaxDeg);
	const float Amplitude = FMath::Min(MaxDeg, FMath::Max(0.f, T.SwayIdleDeg) + FMath::Max(0.f, T.SwayDegPerMps) * Speed);
	Sway.RollDeg = Amplitude * FMath::Sin(UE_TWO_PI * T.SwayHz * TimeS);
	Sway.PitchDeg = FMath::Clamp(AheadAccelMps2 * T.LeanDegPerMps2, -MaxDeg, MaxDeg);

	const float Swing = FMath::Min(FMath::Max(0.f, T.LightSwingMaxDeg), T.LightSwingDegPerMps * Speed + T.LightSwingDegPerMps2 * FMath::Abs(AheadAccelMps2));
	Sway.LightSwingDeg = Swing * FMath::Sin(UE_TWO_PI * T.LightSwingHz * TimeS);

	Sway.Hum01 = T.HumFullSpeedMps > 0.f ? FMath::Clamp(Speed / T.HumFullSpeedMps, 0.f, 1.f) : 0.f;
	float Creak = T.CreakFullAccelMps2 > 0.f ? FMath::Clamp(FMath::Abs(AheadAccelMps2) / T.CreakFullAccelMps2, 0.f, 1.f) : 0.f;

	if (KnockAgeS >= 0.f && KnockMps > 0.f)
	{
		const float Decay = FMath::Exp(-FMath::Max(0.f, T.KnockShakeDecayPerS) * KnockAgeS);
		const float Peak = FMath::Min(FMath::Max(0.f, T.KnockShakeMaxDeg), KnockMps * FMath::Max(0.f, T.KnockShakeDegPerMps));
		Sway.ShakeDeg = Peak * Decay * FMath::Sin(UE_TWO_PI * T.KnockShakeHz * KnockAgeS + UE_HALF_PI);
		Sway.PitchDeg += Sway.ShakeDeg;
		Sway.RollDeg += 0.5f * Sway.ShakeDeg;
		Creak = FMath::Max(Creak, Decay);
	}
	Sway.Creak01 = Creak;
	return Sway;
}

float UDeepMovingSubRules::AirlockSeconds(bool bOut, const FDeepAirlockTuning& T)
{
	const float Hiss = bOut ? FMath::Max(0.f, T.HissSeconds) : 0.f;
	return Hiss + FMath::Max(0.f, T.FadeOutSeconds) + FMath::Max(0.f, T.BlackSeconds) + FMath::Max(0.f, T.FadeInSeconds);
}

FDeepAirlockState UDeepMovingSubRules::AirlockPhase(float ElapsedS, bool bOut, const FDeepAirlockTuning& T)
{
	FDeepAirlockState State;
	if (ElapsedS < 0.f)
	{
		return State;
	}
	const float Hiss = bOut ? FMath::Max(0.f, T.HissSeconds) : 0.f;
	const float FadeOut = FMath::Max(0.f, T.FadeOutSeconds);
	const float Black = FMath::Max(0.f, T.BlackSeconds);
	const float FadeIn = FMath::Max(0.f, T.FadeInSeconds);

	float At = ElapsedS;
	if (At < Hiss)
	{
		State.Phase = EDeepAirlockPhase::Hiss;
		return State;
	}
	At -= Hiss;
	if (At < FadeOut)
	{
		State.Phase = EDeepAirlockPhase::FadeOut;
		State.Fade01 = At / FadeOut;
		return State;
	}
	At -= FadeOut;
	State.bMoveDue = true;
	if (At < Black)
	{
		State.Phase = EDeepAirlockPhase::Black;
		State.Fade01 = 1.f;
		return State;
	}
	At -= Black;
	if (At < FadeIn)
	{
		State.Phase = EDeepAirlockPhase::FadeIn;
		State.Fade01 = 1.f - At / FadeIn;
		return State;
	}
	State.Phase = EDeepAirlockPhase::Done;
	return State;
}

float UDeepMovingSubRules::ValveTurn01(float HeldS, const FDeepAirlockTuning& T)
{
	return T.ValveHoldSeconds > 0.f ? FMath::Clamp(HeldS / T.ValveHoldSeconds, 0.f, 1.f) : 1.f;
}
