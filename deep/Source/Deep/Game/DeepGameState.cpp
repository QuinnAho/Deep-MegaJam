#include "Game/DeepGameState.h"

#include "Net/UnrealNetwork.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"

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
	DiverNumber = 1;
	bWon = false;
	RunSeconds = 0.f;
	TotalGross = 0;
	TotalDirectorCut = 0;
	TotalFees = 0;
	HullDamage = 0;
	HullRepairsOwed = 0;
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
}

void ADeepGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADeepGameState, Stop);
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
