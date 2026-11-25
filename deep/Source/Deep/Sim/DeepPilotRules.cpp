#include "Sim/DeepPilotRules.h"

namespace
{
	/** x' = Target + (x - Target) * e^(-k dt): exact for a linear drag toward Target. */
	float Approach(float Value, float Target, float Rate, float DeltaSeconds)
	{
		return Target + (Value - Target) * FMath::Exp(-FMath::Max(0.f, Rate) * FMath::Max(0.f, DeltaSeconds));
	}
}

int32 UDeepPilotRules::StepNotch(int32 Notch, int32 Dir, const FDeepPilotTuning& T)
{
	return FMath::Clamp(Notch + FMath::Clamp(Dir, -1, 1), -FMath::Max(0, T.AsternNotches), FMath::Max(0, T.AheadNotches));
}

float UDeepPilotRules::NotchFraction(int32 Notch, const FDeepPilotTuning& T)
{
	if (Notch > 0)
	{
		return T.AheadNotches > 0 ? FMath::Min(1.f, float(Notch) / float(T.AheadNotches)) : 0.f;
	}
	if (Notch < 0)
	{
		return T.AsternNotches > 0 ? FMath::Max(-1.f, float(Notch) / float(T.AsternNotches)) : 0.f;
	}
	return 0.f;
}

float UDeepPilotRules::NotchSpeedMps(int32 Notch, const FDeepPilotTuning& T)
{
	const float F = NotchFraction(Notch, T);
	return F >= 0.f ? F * FMath::Max(0.f, T.AheadSpeedMps) : F * FMath::Max(0.f, T.AsternSpeedMps);
}

float UDeepPilotRules::StepBallast(float Lever, float Input, float DeltaSeconds, float& DetentLeftS, const FDeepPilotTuning& T)
{
	Lever = FMath::Clamp(Lever, -1.f, 1.f);
	if (FMath::IsNearlyZero(Input))
	{
		DetentLeftS = 0.f;
		return FMath::Abs(Lever) < T.BallastDetentSnap ? 0.f : Lever;
	}
	if (DetentLeftS > 0.f)
	{
		// Held in the neutral detent a moment: the pilot feels the middle.
		DetentLeftS = FMath::Max(0.f, DetentLeftS - DeltaSeconds);
		return 0.f;
	}
	const float Next = FMath::Clamp(Lever + FMath::Sign(Input) * FMath::Max(0.f, T.BallastLeverPerS) * FMath::Max(0.f, DeltaSeconds), -1.f, 1.f);
	const bool bCrossed = (Lever > 0.f && Next <= 0.f) || (Lever < 0.f && Next >= 0.f);
	if (bCrossed)
	{
		DetentLeftS = FMath::Max(0.f, T.BallastDetentS);
		return 0.f;
	}
	return Next;
}

FVector2D UDeepPilotRules::Ahead(float YawDeg)
{
	const float R = FMath::DegreesToRadians(YawDeg);
	return FVector2D(FMath::Cos(R), FMath::Sin(R));
}

FVector2D UDeepPilotRules::Starboard(float YawDeg)
{
	const float R = FMath::DegreesToRadians(YawDeg);
	return FVector2D(-FMath::Sin(R), FMath::Cos(R));
}

FDeepPilotMotion UDeepPilotRules::StepMotion(const FDeepPilotMotion& In, int32 Notch, float Ballast, float Rudder, float DeltaSeconds, const FDeepPilotTuning& T)
{
	FDeepPilotMotion Out = In;
	const float Dt = FMath::Max(0.f, DeltaSeconds);
	// Rudder: the turn rate builds and decays; the heading turns under the way the sub carries (a little skid).
	Out.YawRateDegS = Approach(In.YawRateDegS, FMath::Clamp(Rudder, -1.f, 1.f) * T.YawRateDegS, T.YawResponsePerS, Dt);
	Out.YawDeg = FRotator::NormalizeAxis(In.YawDeg + 0.5f * (In.YawRateDegS + Out.YawRateDegS) * Dt);
	const FVector2D Fwd = Ahead(Out.YawDeg);
	const FVector2D Side = Starboard(Out.YawDeg);
	// Thrust against low drag along the heading, high drag across it.
	const float Surge = Approach(FVector2D::DotProduct(In.VelocityMps, Fwd), NotchSpeedMps(Notch, T), T.SurgeDragPerS, Dt);
	const float Sway = Approach(FVector2D::DotProduct(In.VelocityMps, Side), 0.f, T.SwayDragPerS, Dt);
	Out.VelocityMps = Fwd * Surge + Side * Sway;
	// Buoyancy against drag: the lever sets the speed it settles at.
	Out.HeaveMps = Approach(In.HeaveMps, FMath::Clamp(Ballast, -1.f, 1.f) * T.HeaveSpeedMps, T.HeaveDragPerS, Dt);
	return Out;
}

FDeepDriveInput FDeepDriveInput::Clamped() const
{
	FDeepDriveInput Out;
	Out.Surge = FMath::Clamp(Surge, -1.f, 1.f);
	Out.Turn = FMath::Clamp(Turn, -1.f, 1.f);
	Out.Heave = FMath::Clamp(Heave, -1.f, 1.f);
	Out.Strafe = FMath::Clamp(Strafe, -1.f, 1.f);
	return Out;
}

bool FDeepDriveInput::IsIdle() const
{
	return FMath::IsNearlyZero(Surge) && FMath::IsNearlyZero(Turn) && FMath::IsNearlyZero(Heave) && FMath::IsNearlyZero(Strafe);
}

bool FDeepDriveInput::Equals(const FDeepDriveInput& Other, float Tolerance) const
{
	return FMath::IsNearlyEqual(Surge, Other.Surge, Tolerance) && FMath::IsNearlyEqual(Turn, Other.Turn, Tolerance)
		&& FMath::IsNearlyEqual(Heave, Other.Heave, Tolerance) && FMath::IsNearlyEqual(Strafe, Other.Strafe, Tolerance);
}

float UDeepPilotRules::MoveToward(float Value, float Target, float Rate, float DeltaSeconds)
{
	const float Step = FMath::Max(0.f, Rate) * FMath::Max(0.f, DeltaSeconds);
	const float Gap = Target - Value;
	return FMath::Abs(Gap) <= Step ? Target : Value + FMath::Sign(Gap) * Step;
}

namespace
{
	/** Speeding up toward a held speed at Accel; slowing (let go, a smaller speed, or against the way) at Brake. */
	float DriveAxis(float Value, float Target, float Accel, float Brake, float DeltaSeconds)
	{
		const bool bSlowing = FMath::Abs(Target) < FMath::Abs(Value) || (Value * Target < 0.f);
		return UDeepPilotRules::MoveToward(Value, Target, bSlowing ? Brake : Accel, DeltaSeconds);
	}
}

FDeepPilotMotion UDeepPilotRules::StepDrive(const FDeepPilotMotion& In, const FDeepDriveInput& Input, float DeltaSeconds, const FDeepDriveTuning& T)
{
	const FDeepDriveInput Held = Input.Clamped();
	const float Dt = FMath::Max(0.f, DeltaSeconds);
	FDeepPilotMotion Out = In;
	// The turn rate builds and dies over TurnBuildS either way; the heading turns under the way the sub carries.
	const float TurnRate = FMath::Max(0.f, T.TurnRateDegS);
	const float TurnAccel = T.TurnBuildS > UE_KINDA_SMALL_NUMBER ? TurnRate / T.TurnBuildS : TurnRate * 1000.f;
	Out.YawRateDegS = MoveToward(In.YawRateDegS, Held.Turn * TurnRate, TurnAccel, Dt);
	Out.YawDeg = FRotator::NormalizeAxis(In.YawDeg + 0.5f * (In.YawRateDegS + Out.YawRateDegS) * Dt);
	const FVector2D Fwd = Ahead(Out.YawDeg);
	const FVector2D Side = Starboard(Out.YawDeg);
	const float WantSurge = Held.Surge >= 0.f ? Held.Surge * FMath::Max(0.f, T.AheadSpeedMps) : Held.Surge * FMath::Max(0.f, T.AsternSpeedMps);
	const float Surge = DriveAxis(FVector2D::DotProduct(In.VelocityMps, Fwd), WantSurge, T.AccelMps2, T.BrakeMps2, Dt);
	// Sideways: Q/E push at the drive's rates; way nobody asked for (a turn's skid) dies at the hull's grip.
	const float SwayIn = FVector2D::DotProduct(In.VelocityMps, Side);
	const float Sway = FMath::IsNearlyZero(Held.Strafe)
		? MoveToward(SwayIn * FMath::Exp(-FMath::Max(0.f, T.SideGripPerS) * Dt), 0.f, T.SideGripMps2, Dt)
		: DriveAxis(SwayIn, Held.Strafe * FMath::Max(0.f, T.StrafeSpeedMps), T.AccelMps2, T.BrakeMps2, Dt);
	Out.VelocityMps = Fwd * Surge + Side * Sway;
	Out.HeaveMps = DriveAxis(In.HeaveMps, Held.Heave * FMath::Max(0.f, T.HeaveSpeedMps), T.HeaveAccelMps2, T.HeaveBrakeMps2, Dt);
	return Out;
}

FVector2D UDeepPilotRules::MouseSteer(float LookYawDeg, float LookPitchDeg, float Surge, const FDeepDriveTuning& T)
{
	if (FMath::IsNearlyZero(Surge))
	{
		return FVector2D::ZeroVector;
	}
	const float Dead = FMath::Max(0.f, T.MouseSteerDeadDeg);
	const auto Axis = [Dead](float OffDeg, float FullDeg)
	{
		const float Past = FMath::Max(0.f, FMath::Abs(OffDeg) - Dead);
		const float Full = FMath::Max(0.1f, FullDeg - Dead);
		return FMath::Sign(OffDeg) * FMath::Min(1.f, Past / Full);
	};
	// The bow turns toward the view whichever way the sub is driven. Looking up climbs: heave is + down.
	return FVector2D(Axis(FRotator::NormalizeAxis(LookYawDeg), T.MouseSteerFullTurnDeg), -Axis(LookPitchDeg, T.MouseSteerFullHeaveDeg));
}

float UDeepPilotRules::SpeedMps(const FDeepPilotMotion& Motion)
{
	return FMath::Sqrt(Motion.VelocityMps.SizeSquared() + FMath::Square(Motion.HeaveMps));
}

bool UDeepPilotRules::ShouldPark(float DepthM, float StopDepthM, float SpeedMps, int32 Notch, float Ballast, const FDeepPilotTuning& T)
{
	return Notch == 0 && Ballast == 0.f && FMath::Abs(DepthM - StopDepthM) <= T.ParkNearM && SpeedMps <= T.ParkSpeedMps;
}

float UDeepPilotRules::GlideStep(float Value, float Target, float DeltaSeconds, float MinSpeed, const FDeepPilotTuning& T)
{
	const float Gap = Target - Value;
	const float Dt = FMath::Max(0.f, DeltaSeconds);
	const float Step = FMath::Max(FMath::Abs(Gap) * (1.f - FMath::Exp(-FMath::Max(0.f, T.ParkGlidePerS) * Dt)), FMath::Max(0.f, MinSpeed) * Dt);
	return FMath::Abs(Gap) <= Step ? Target : Value + FMath::Sign(Gap) * Step;
}

float UDeepPilotRules::GlideYawStep(float YawDeg, float DeltaSeconds, const FDeepPilotTuning& T)
{
	return FRotator::NormalizeAxis(GlideStep(FRotator::NormalizeAxis(YawDeg), 0.f, DeltaSeconds, T.ParkGlideMinDegS, T));
}

void UDeepPilotRules::TurnFreeBoxM(const FVector& BoxMinM, const FVector& BoxMaxM, FVector& OutMinM, FVector& OutMaxM)
{
	const float HX = FMath::Max(FMath::Abs(BoxMinM.X), FMath::Abs(BoxMaxM.X));
	const float HY = FMath::Max(FMath::Abs(BoxMinM.Y), FMath::Abs(BoxMaxM.Y));
	const float Half = FMath::Sqrt(HX * HX + HY * HY) * UE_INV_SQRT_2;
	OutMinM = FVector(-Half, -Half, BoxMinM.Z);
	OutMaxM = FVector(Half, Half, BoxMaxM.Z);
}

FVector UDeepPilotRules::HangLocationCm(const FVector& SubCm, const FQuat& SubRotation, const FVector2D& OffsetM, float UpM, float YawDeg, FQuat& OutTurn)
{
	OutTurn = FMath::IsNearlyZero(YawDeg) ? FQuat::Identity : FQuat(FVector::UpVector, FMath::DegreesToRadians(-YawDeg));
	const FVector Side = SubRotation.RotateVector(FVector(OffsetM.X, OffsetM.Y, 0.f));
	return SubCm + OutTurn.RotateVector((FVector(0.f, 0.f, UpM) - Side) * 100.f);
}
