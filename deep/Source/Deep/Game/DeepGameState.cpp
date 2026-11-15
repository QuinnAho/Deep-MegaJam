#include "Game/DeepGameState.h"

#include "Net/UnrealNetwork.h"

void ADeepGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADeepGameState, Stop);
	DOREPLIFETIME(ADeepGameState, Credits);
	DOREPLIFETIME(ADeepGameState, HullRating);
	DOREPLIFETIME(ADeepGameState, DiverNumber);
	DOREPLIFETIME(ADeepGameState, HullDamage);
	DOREPLIFETIME(ADeepGameState, HullRepairsOwed);
}
