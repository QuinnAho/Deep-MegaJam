#include "Sim/DeepAirLineRules.h"

#include "Sim/DeepRules.h"

float DeepAirLine::PatienceAfterStep(float Before, float DeltaSeconds, bool bHunted, const FDeepPatienceTuning& Patience, const FDeepAirLineTuning& AirLine)
{
	return AirLine.bPatienceDrains ? UDeepRules::PatienceStep(Before, DeltaSeconds, bHunted, Patience) : Before;
}

bool DeepAirLine::StateBitesLine(EDeepMonsterState State, const FDeepAirLineTuning& AirLine)
{
	if (!AirLine.bOnlyAttackingBites)
	{
		return true;
	}
	return State == EDeepMonsterState::Stalk || State == EDeepMonsterState::Bite;
}

int32 DeepAirLine::TouchedLink(TConstArrayView<FVector> Points, TConstArrayView<uint8> Linked, const FVector& HeadCm, float TouchM)
{
	const double TouchCm = FMath::Max(0.f, TouchM) * UDeepRules::CmPerM;
	for (int32 Link = 0; Link + 1 < Points.Num(); ++Link)
	{
		if (!Linked.IsEmpty() && (!Linked.IsValidIndex(Link) || Linked[Link] == 0))
		{
			continue;
		}
		const FVector OnLine = FMath::ClosestPointOnSegment(HeadCm, Points[Link], Points[Link + 1]);
		if (FVector::DistSquared(OnLine, HeadCm) <= TouchCm * TouchCm)
		{
			return Link;
		}
	}
	return INDEX_NONE;
}

int32 DeepAirLine::ReclipFee(bool bUnclipped, const FDeepEconomyTuning& Economy, const FDeepAirLineTuning& AirLine)
{
	return FMath::Max(0, bUnclipped ? AirLine.UnclipReclipFee : Economy.RetetherFee);
}
