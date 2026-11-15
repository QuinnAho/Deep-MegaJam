#include "Game/DeepGameState.h"

#include "Net/UnrealNetwork.h"
#include "Sim/DeepCaveRules.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepArena.h"
#include "World/DeepShaft.h"

ADeepGameState::ADeepGameState()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void ADeepGameState::ResetRun()
{
	Credits = FMath::Max(0, UDeepTuningSettings::Get().Economy.StartingCredits);
	HullRating = 1;
	Stop = 1;
	SubDepthM = UDeepTuningSettings::Get().GetStop(1).SubDepthM;
	SubOffsetM = FVector2D::ZeroVector;
	bParked = true;
	DeepestStopReached = 1;
	DiverNumber = 1;
	bWon = false;
	RunSeconds = 0.f;
	TotalGross = 0;
	TotalDirectorCut = 0;
	TotalFees = 0;
	HullDamage = 0;
	HullRepairsOwed = 0;
}

float ADeepGameState::DepthLimitM() const
{
	return UDeepRules::HelmDepthLimitM(GatesBought(), UDeepTuningSettings::Get().Sub);
}

FDeepRunReview ADeepGameState::BuildReview() const
{
	return UDeepRules::BuildReview(DiverNumber - 1, TotalGross, TotalDirectorCut, TotalFees, RunSeconds, UDeepTuningSettings::Get().Review);
}

void ADeepGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && !bOnTitle && !bWon)
	{
		RunSeconds += DeltaSeconds;
	}
	if (HasAuthority())
	{
		StepParkGlide(DeltaSeconds);
	}
}

float ADeepGameState::GetHullIntegrity01() const
{
	return UDeepCaveRules::HullIntegrity01(HullDamage, UDeepTuningSettings::Get().Cave);
}

void ADeepGameState::StepParkGlide(float DeltaSeconds)
{
	if (SubOffsetM.IsZero())
	{
		return;
	}
	// Shaft off: no offset at all. Parked: glide home to the stop's centre; the hatch opens once there (ApplySubDepth).
	if (!DeepShaft::IsOn())
	{
		SubOffsetM = FVector2D::ZeroVector;
	}
	else if (bParked)
	{
		const float Step = FMath::Max(0.01f, UDeepTuningSettings::Get().Cave.ParkGlideMps) * FMath::Max(0.f, DeltaSeconds);
		const float Gap = SubOffsetM.Size();
		SubOffsetM = Gap <= Step ? FVector2D::ZeroVector : SubOffsetM * (1.f - Step / Gap);
	}
	else
	{
		return;
	}
	DeepArena::ApplySubDepth(GetWorld());
}

void ADeepGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADeepGameState, Stop);
	DOREPLIFETIME(ADeepGameState, SubDepthM);
	DOREPLIFETIME(ADeepGameState, SubOffsetM);
	DOREPLIFETIME(ADeepGameState, bParked);
	DOREPLIFETIME(ADeepGameState, DeepestStopReached);
	DOREPLIFETIME(ADeepGameState, Credits);
	DOREPLIFETIME(ADeepGameState, HullRating);
	DOREPLIFETIME(ADeepGameState, DiverNumber);
	DOREPLIFETIME(ADeepGameState, HullDamage);
	DOREPLIFETIME(ADeepGameState, HullRepairsOwed);
	DOREPLIFETIME(ADeepGameState, bOnTitle);
	DOREPLIFETIME(ADeepGameState, bWon);
	DOREPLIFETIME(ADeepGameState, RunSeconds);
	DOREPLIFETIME(ADeepGameState, TotalGross);
	DOREPLIFETIME(ADeepGameState, TotalDirectorCut);
	DOREPLIFETIME(ADeepGameState, TotalFees);
}
