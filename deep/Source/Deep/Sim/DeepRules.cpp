#include "Sim/DeepRules.h"

float UDeepRules::DepthM(float WorldZCm, float AnchorZCm, float AnchorDepthM)
{
	return FMath::Max(0.f, AnchorDepthM + (AnchorZCm - WorldZCm) / CmPerM);
}

int32 UDeepRules::ZoneIndex(float InDepthM, const TArray<FDeepZone>& Zones)
{
	int32 Best = INDEX_NONE;
	int32 Shallowest = INDEX_NONE;
	for (int32 Index = 0; Index < Zones.Num(); ++Index)
	{
		const float Min = Zones[Index].MinDepthM;
		if (Shallowest == INDEX_NONE || Min < Zones[Shallowest].MinDepthM)
		{
			Shallowest = Index;
		}
		if (Min <= InDepthM && (Best == INDEX_NONE || Min > Zones[Best].MinDepthM))
		{
			Best = Index;
		}
	}
	return Best != INDEX_NONE ? Best : Shallowest;
}

FString UDeepRules::ZoneName(float InDepthM, const TArray<FDeepZone>& Zones)
{
	const int32 Index = ZoneIndex(InDepthM, Zones);
	return Zones.IsValidIndex(Index) ? Zones[Index].Name : FString();
}

float UDeepRules::TetherLengthM(int32 Stop, const FDeepTetherTuning& Tuning)
{
	const int32 Extra = FMath::Max(Stop, 1) - 1;
	return FMath::Max(0.f, Tuning.BaseLengthM + Tuning.LengthPerStopM * static_cast<float>(Extra));
}

FDeepTetherSolve UDeepRules::SolveTether(const FVector& AnchorCm, const FVector& WantedCm, float LengthM, const FDeepTetherTuning& Tuning)
{
	FDeepTetherSolve Out;
	Out.LengthM = FMath::Max(0.f, LengthM);

	const FVector Offset = WantedCm - AnchorCm;
	const float DistanceCm = static_cast<float>(Offset.Size());
	const float LengthCm = Out.LengthM * CmPerM;

	Out.DistanceM = DistanceCm / CmPerM;
	Out.LeftM = FMath::Max(0.f, Out.LengthM - Out.DistanceM);

	if (Out.LengthM <= UE_KINDA_SMALL_NUMBER)
	{
		// No line paid out: the diver is held at the anchor and the line is fully taut.
		Out.Strain = 1.f;
		Out.bTaut = true;
		Out.bClamped = DistanceCm > UE_KINDA_SMALL_NUMBER;
		Out.ClampedPositionCm = AnchorCm;
		return Out;
	}

	Out.Strain = FMath::Clamp(Out.DistanceM / Out.LengthM, 0.f, 1.f);
	Out.bTaut = Out.Strain >= Tuning.TautStrain;

	if (DistanceCm > LengthCm)
	{
		Out.bClamped = true;
		Out.ClampedPositionCm = AnchorCm + Offset * (LengthCm / DistanceCm);
	}
	else
	{
		Out.ClampedPositionCm = WantedCm;
	}
	return Out;
}

FVector UDeepRules::WinchStep(const FVector& DiverCm, const FVector& AnchorCm, float DeltaSeconds, const FDeepTetherTuning& Tuning)
{
	const FVector ToAnchor = AnchorCm - DiverCm;
	const float DistanceCm = static_cast<float>(ToAnchor.Size());
	const float StopCm = FMath::Max(0.f, Tuning.WinchArriveM) * CmPerM;
	const float StepCm = FMath::Max(0.f, Tuning.WinchSpeedMps * DeltaSeconds) * CmPerM;

	if (DistanceCm <= StopCm || StepCm <= 0.f || DistanceCm <= UE_KINDA_SMALL_NUMBER)
	{
		return DiverCm;
	}
	const float MoveCm = FMath::Min(StepCm, DistanceCm - StopCm);
	return DiverCm + ToAnchor * (MoveCm / DistanceCm);
}

FVector UDeepRules::WinchStepToAirlock(const FVector& DiverCm, const FVector& AnchorCm, float DeltaSeconds, const FDeepTetherTuning& Tuning)
{
	if (IsHome(DiverCm, AnchorCm, Tuning))
	{
		return DiverCm;
	}
	// Reel toward a point just under the airlock: inside the home radius, and below the hull, so a pull from the
	// side runs under the sub instead of into it.
	const float BelowM = FMath::Clamp(Tuning.WinchApproachBelowM, 0.f, FMath::Max(0.f, Tuning.WinchArriveM - 0.5f));
	const FVector Via = AnchorCm - FVector(0.f, 0.f, BelowM * CmPerM);
	const FVector ToVia = Via - DiverCm;
	const float DistanceCm = static_cast<float>(ToVia.Size());
	const float StepCm = FMath::Max(0.f, Tuning.WinchSpeedMps * DeltaSeconds) * CmPerM;
	if (StepCm <= 0.f || DistanceCm <= UE_KINDA_SMALL_NUMBER)
	{
		return DiverCm;
	}
	return DiverCm + ToVia * (FMath::Min(StepCm, DistanceCm) / DistanceCm);
}

bool UDeepRules::IsHome(const FVector& DiverCm, const FVector& AnchorCm, const FDeepTetherTuning& Tuning)
{
	// A centimetre of slack so a winch that stopped exactly at the arrive distance counts as home.
	return FVector::Dist(DiverCm, AnchorCm) <= FMath::Max(0.f, Tuning.WinchArriveM) * CmPerM + 1.f;
}

float UDeepRules::CargoKg(const TArray<FDeepCargoItem>& Cargo)
{
	float Total = 0.f;
	for (const FDeepCargoItem& Item : Cargo)
	{
		Total += FMath::Max(0.f, Item.Kg);
	}
	return Total;
}

int32 UDeepRules::CargoValue(const TArray<FDeepCargoItem>& Cargo)
{
	int64 Total = 0;
	for (const FDeepCargoItem& Item : Cargo)
	{
		Total += FMath::Max(0, Item.Value);
	}
	return static_cast<int32>(FMath::Min<int64>(Total, MAX_int32));
}

FDeepWeightResult UDeepRules::SolveWeight(float CarriedKg, const FDeepWeightTuning& Tuning)
{
	FDeepWeightResult Out;
	Out.CarriedKg = FMath::Max(0.f, CarriedKg);
	Out.ExcessKg = FMath::Max(0.f, Out.CarriedKg - Tuning.FreeKg);
	Out.SinkSpeedMps = FMath::Clamp(Out.ExcessKg * Tuning.SinkMpsPerKg, 0.f, FMath::Max(0.f, Tuning.MaxSinkMps));
	const float Floor = FMath::Clamp(Tuning.MinSwimMultiplier, 0.f, 1.f);
	Out.SwimMultiplier = FMath::Clamp(1.f - Out.ExcessKg * Tuning.SwimSlowdownPerKg, Floor, 1.f);
	Out.bOverloaded = Out.CarriedKg > 0.f && Out.CarriedKg >= Tuning.OverloadKg;
	return Out;
}

FVector UDeepRules::SwimVelocityMps(const FVector& WishDirection, bool bSprint, const FDeepWeightResult& Weight, const FDeepSwimTuning& Tuning)
{
	const float Sprint = bSprint ? FMath::Max(0.f, Tuning.SprintMultiplier) : 1.f;
	const float Speed = FMath::Max(0.f, Tuning.SwimSpeedMps) * Sprint * Weight.SwimMultiplier;
	FVector Velocity = WishDirection.GetClampedToMaxSize(1.0) * Speed;
	if (Weight.bOverloaded && Velocity.Z > 0.0)
	{
		Velocity.Z = 0.0;
	}
	Velocity.Z -= Weight.SinkSpeedMps;
	return Velocity;
}

float UDeepRules::PatienceStep(float PatienceSeconds, float DeltaSeconds, bool bHunted, const FDeepPatienceTuning& Tuning)
{
	const float Rate = FMath::Max(0.f, Tuning.DrainPerSecond) * (bHunted ? FMath::Max(0.f, Tuning.HuntedMultiplier) : 1.f);
	return FMath::Clamp(PatienceSeconds - Rate * FMath::Max(0.f, DeltaSeconds), 0.f, FMath::Max(0.f, Tuning.MaxSeconds));
}

float UDeepRules::PatienceRefillSeconds(EDeepSalvageSize Size, const FDeepPatienceTuning& Tuning)
{
	float Fraction = 0.f;
	switch (Size)
	{
	case EDeepSalvageSize::Small:  Fraction = Tuning.RefillSmall; break;
	case EDeepSalvageSize::Medium: Fraction = Tuning.RefillMedium; break;
	case EDeepSalvageSize::Large:  Fraction = Tuning.RefillLarge; break;
	case EDeepSalvageSize::Pearl:  Fraction = Tuning.RefillPearl; break;
	case EDeepSalvageSize::Relic:  Fraction = Tuning.RefillRelic; break;
	}
	return FMath::Max(0.f, Fraction) * FMath::Max(0.f, Tuning.MaxSeconds);
}

float UDeepRules::PatienceAfterSalvage(float PatienceSeconds, EDeepSalvageSize Size, const FDeepPatienceTuning& Tuning)
{
	return FMath::Clamp(PatienceSeconds + PatienceRefillSeconds(Size, Tuning), 0.f, FMath::Max(0.f, Tuning.MaxSeconds));
}

int32 UDeepRules::PatienceWarningCrossed(float BeforeSeconds, float AfterSeconds, const FDeepPatienceTuning& Tuning)
{
	if (Tuning.MaxSeconds <= 0.f || AfterSeconds >= BeforeSeconds)
	{
		return INDEX_NONE;
	}
	const float Before = BeforeSeconds / Tuning.MaxSeconds;
	const float After = AfterSeconds / Tuning.MaxSeconds;

	int32 MostSevere = INDEX_NONE;
	for (int32 Index = 0; Index < Tuning.WarningFractions.Num(); ++Index)
	{
		const float Threshold = Tuning.WarningFractions[Index];
		if (Before > Threshold && After <= Threshold)
		{
			if (MostSevere == INDEX_NONE || Threshold < Tuning.WarningFractions[MostSevere])
			{
				MostSevere = Index;
			}
		}
	}
	return MostSevere;
}

int32 UDeepRules::DirectorCut(int32 Gross, int32 Percent)
{
	if (Gross <= 0)
	{
		return 0;
	}
	const int64 P = FMath::Clamp(Percent, 0, 100);
	return static_cast<int32>((static_cast<int64>(Gross) * P + 99) / 100);
}

FDeepSettlement UDeepRules::Settle(int32 Gross, const TArray<int32>& Fees, const FDeepEconomyTuning& Tuning)
{
	FDeepSettlement Out;
	Out.Gross = FMath::Max(0, Gross);
	Out.DirectorCut = DirectorCut(Out.Gross, Tuning.DirectorCutPercent);

	int64 FeeSum = 0;
	for (const int32 Fee : Fees)
	{
		FeeSum += FMath::Max(0, Fee);
	}
	Out.Fees = static_cast<int32>(FMath::Min<int64>(FeeSum, MAX_int32));
	Out.Net = static_cast<int32>(FMath::Max<int64>(0, static_cast<int64>(Out.Gross) - Out.DirectorCut - FeeSum));
	return Out;
}

int32 UDeepRules::ApplyFee(int32 Credits, int32 Fee)
{
	return static_cast<int32>(FMath::Max<int64>(0, static_cast<int64>(FMath::Max(0, Credits)) - FMath::Max(0, Fee)));
}

int32 UDeepRules::NextHullPrice(int32 HullRating, const FDeepEconomyTuning& Tuning)
{
	const int32 Index = FMath::Max(HullRating, 1) - 1;
	return Tuning.HullPrices.IsValidIndex(Index) ? FMath::Max(0, Tuning.HullPrices[Index]) : INDEX_NONE;
}

int32 UDeepRules::MaxHullRating(const FDeepEconomyTuning& Tuning)
{
	return Tuning.HullPrices.Num() + 1;
}

FDeepHullPurchase UDeepRules::TryBuyHull(int32 Credits, int32 HullRating, const FDeepEconomyTuning& Tuning)
{
	FDeepHullPurchase Out;
	Out.Credits = FMath::Max(0, Credits);
	Out.HullRating = FMath::Max(HullRating, 1);

	const int32 Price = NextHullPrice(Out.HullRating, Tuning);
	if (Price >= 0 && Out.Credits >= Price)
	{
		Out.Credits -= Price;
		Out.Spent = Price;
		Out.HullRating += 1;
		Out.bBought = true;
	}
	return Out;
}

int32 UDeepRules::SettlementFeeTotal(int32 Gross, const FDeepEconomyTuning& Tuning)
{
	const int64 Percent = DirectorCut(FMath::Max(0, Gross), FMath::Max(0, Tuning.FeePercent));
	return static_cast<int32>(FMath::Min<int64>(MAX_int32, Percent + FMath::Max(0, Tuning.FeeFlat)));
}

TArray<int32> UDeepRules::SplitFee(int32 Total, int32 Lines, int32 Seed)
{
	const int32 Count = FMath::Max(1, Lines);
	const int32 Sum = FMath::Max(0, Total);
	TArray<int32> Out;
	Out.Init(0, Count);

	// Everyone gets 1 first when the total allows, so no joke line reads "0 cr" unless it must.
	const int32 Base = Sum >= Count ? 1 : 0;
	int32 Left = Sum - Base * Count;
	FRandomStream Rng(Seed);
	TArray<float> Weights;
	float WeightSum = 0.f;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Weights.Add(0.2f + Rng.FRand());
		WeightSum += Weights.Last();
	}
	int32 Given = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 Share = FMath::FloorToInt(Left * (Weights[Index] / WeightSum));
		Out[Index] = Base + Share;
		Given += Share;
	}
	// Remainder from rounding down goes to the first line.
	Out[0] += Left - Given;
	if (Base == 0 && Sum > 0)
	{
		// Too small to spread: put it all on one line.
		Out.Init(0, Count);
		Out[0] = Sum;
	}
	return Out;
}

EDeepBankMood UDeepRules::BankMood(int32 Gross, int32 Net, const FDeepEconomyTuning& Tuning)
{
	if (Gross <= 0)
	{
		return EDeepBankMood::Empty;
	}
	return Net >= Tuning.GoodBankNet ? EDeepBankMood::Good : EDeepBankMood::Poor;
}

float UDeepRules::BailoutStep(float AirSeconds, float RealDeltaSeconds, const FDeepBailoutTuning& Tuning)
{
	float Air = FMath::Max(0.f, AirSeconds);
	float Remaining = FMath::Max(0.f, RealDeltaSeconds);
	const float Window = FMath::Max(0.f, Tuning.SlowWindowSeconds);
	const float SlowScale = FMath::Max(0.f, Tuning.SlowDrainScale);

	if (Air > Window)
	{
		const float Fast = FMath::Min(Remaining, Air - Window);
		Air -= Fast;
		Remaining -= Fast;
	}
	Air -= Remaining * SlowScale;
	return FMath::Max(0.f, Air);
}

int32 UDeepRules::PickGrabTarget(const FVector& EyeCm, const FVector& ViewDirection, const TArray<FVector>& CandidatesCm, const TArray<float>& RadiiM, const FDeepSwimTuning& Tuning)
{
	const FVector View = ViewDirection.GetSafeNormal();
	const float CosCone = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(Tuning.GrabConeDeg, 0.f, 180.f)));
	const float ReachCm = FMath::Max(0.f, Tuning.ReachM) * CmPerM;

	int32 Best = INDEX_NONE;
	float BestSurfaceCm = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < CandidatesCm.Num(); ++Index)
	{
		const FVector ToTarget = CandidatesCm[Index] - EyeCm;
		const float CentreCm = static_cast<float>(ToTarget.Size());
		const float RadiusCm = RadiiM.IsValidIndex(Index) ? FMath::Max(0.f, RadiiM[Index]) * CmPerM : 0.f;
		const float SurfaceCm = FMath::Max(0.f, CentreCm - RadiusCm);
		if (SurfaceCm > ReachCm)
		{
			continue;
		}
		// Anything we are touching counts as in view; otherwise it must be inside the cone.
		const bool bTouching = CentreCm <= RadiusCm || CentreCm <= UE_KINDA_SMALL_NUMBER;
		if (!bTouching && (View.IsNearlyZero() || FVector::DotProduct(ToTarget / CentreCm, View) < CosCone))
		{
			continue;
		}
		if (SurfaceCm < BestSurfaceCm)
		{
			BestSurfaceCm = SurfaceCm;
			Best = Index;
		}
	}
	return Best;
}

FVector UDeepRules::SalvageSpotOffsetCm(const FDeepSalvageSpot& Spot)
{
	const float Radians = FMath::DegreesToRadians(Spot.AngleDeg);
	return FVector(FMath::Cos(Radians) * Spot.OutM, FMath::Sin(Radians) * Spot.OutM, -Spot.BelowM) * CmPerM;
}

FVector UDeepRules::RelicOffsetCm(const FDeepArenaTuning& Arena, float StopDepthM, float TetherLengthM)
{
	const float Reach = FMath::Max(0.f, TetherLengthM - FMath::Max(0.f, Arena.RelicReachMarginM));
	const float Out = FMath::Clamp(Arena.RelicOutM, 0.f, Reach);
	const float MaxBelow = FMath::Sqrt(FMath::Max(0.f, Reach * Reach - Out * Out));
	const float Below = FMath::Clamp(Arena.RelicDepthM - StopDepthM, 0.f, MaxBelow);
	const float Radians = FMath::DegreesToRadians(Arena.RelicAngleDeg);
	return FVector(FMath::Cos(Radians) * Out, FMath::Sin(Radians) * Out, -Below) * CmPerM;
}

bool UDeepRules::IsRelicSeen(const FVector& EyeCm, const FVector& ViewDirection, const FVector& RelicCm, const FDeepArenaTuning& Arena)
{
	const FVector ToRelic = RelicCm - EyeCm;
	const float DistanceM = static_cast<float>(ToRelic.Size()) / CmPerM;
	if (DistanceM <= Arena.RelicSeenNearM)
	{
		return true;
	}
	if (DistanceM > Arena.RelicSeenViewM || ViewDirection.IsNearlyZero())
	{
		return false;
	}
	const float CosAngle = static_cast<float>(FVector::DotProduct(ToRelic.GetSafeNormal(), ViewDirection.GetSafeNormal()));
	return CosAngle >= FMath::Cos(FMath::DegreesToRadians(Arena.RelicSeenConeDeg));
}

FString UDeepRules::ReviewRating(int32 DiversExpended, float Seconds, const FDeepReviewTuning& Tuning)
{
	if (Tuning.Words.IsEmpty())
	{
		return FString();
	}
	int32 Index = Tuning.Words.Num() - 1;
	for (int32 Step = 0; Step < Tuning.MaxDiversExpended.Num(); ++Step)
	{
		if (FMath::Max(0, DiversExpended) <= Tuning.MaxDiversExpended[Step])
		{
			Index = Step;
			break;
		}
	}
	if (Tuning.SlowRunSeconds > 0.f && Seconds > Tuning.SlowRunSeconds)
	{
		++Index;
	}
	return Tuning.Words[FMath::Clamp(Index, 0, Tuning.Words.Num() - 1)];
}

FDeepRunReview UDeepRules::BuildReview(int32 DiversExpended, int32 Gross, int32 DirectorCut, int32 Fees, float Seconds, const FDeepReviewTuning& Tuning)
{
	FDeepRunReview Review;
	Review.DiversExpended = FMath::Max(0, DiversExpended);
	Review.Gross = FMath::Max(0, Gross);
	Review.DirectorCut = FMath::Max(0, DirectorCut);
	Review.Fees = FMath::Max(0, Fees);
	Review.Kept = static_cast<int32>(FMath::Max<int64>(0, static_cast<int64>(Review.Gross) - Review.DirectorCut - Review.Fees));
	Review.Seconds = FMath::Max(0.f, Seconds);
	Review.Rating = ReviewRating(Review.DiversExpended, Review.Seconds, Tuning);
	return Review;
}

// ---------------------------------------------------------------- the submarine

float UDeepRules::HelmDepthLimitM(int32 GatesBought, const FDeepSubTuning& Tuning)
{
	if (Tuning.DepthLimitsM.IsEmpty())
	{
		return 0.f;
	}
	return Tuning.DepthLimitsM[FMath::Clamp(GatesBought, 0, Tuning.DepthLimitsM.Num() - 1)];
}

FDeepHelmStep UDeepRules::HelmStep(float DepthM, float Input, float DeltaSeconds, float TopM, float LimitM, const FDeepSubTuning& Tuning)
{
	FDeepHelmStep Out;
	const float Wanted = DepthM + FMath::Clamp(Input, -1.f, 1.f) * FMath::Max(0.f, Tuning.DriveSpeedMps) * FMath::Max(0.f, DeltaSeconds);
	const float Low = FMath::Max(TopM, LimitM);
	Out.DepthM = FMath::Clamp(Wanted, TopM, Low);
	Out.bAtLimit = Input > 0.f && Wanted >= Low - KINDA_SMALL_NUMBER;
	Out.bAtTop = Input < 0.f && Wanted <= TopM + KINDA_SMALL_NUMBER;
	return Out;
}

int32 UDeepRules::HelmParkStop(float DepthM, const TArray<float>& StopDepthsM, float SnapM)
{
	int32 Best = 0;
	float BestGap = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < StopDepthsM.Num(); ++Index)
	{
		const float Gap = FMath::Abs(StopDepthsM[Index] - DepthM);
		if (Gap <= SnapM && Gap < BestGap)
		{
			Best = Index + 1;
			BestGap = Gap;
		}
	}
	return Best;
}

int32 UDeepRules::NearestStop(float DepthM, const TArray<float>& StopDepthsM)
{
	int32 Best = 1;
	float BestGap = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < StopDepthsM.Num(); ++Index)
	{
		const float Gap = FMath::Abs(StopDepthsM[Index] - DepthM);
		if (Gap < BestGap)
		{
			Best = Index + 1;
			BestGap = Gap;
		}
	}
	return Best;
}

bool UDeepRules::InHatchColumn(const FVector& LocalCm, const FDeepSubTuning& Tuning)
{
	const double Half = Tuning.HatchHalfSizeM * CmPerM;
	return FMath::Abs(LocalCm.X) <= Half && FMath::Abs(LocalCm.Y) <= Half;
}
