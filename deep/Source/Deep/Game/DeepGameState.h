// Shared run state every player sees: which stop the sub is at, the company account, hull rating, diver number.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "DeepGameState.generated.h"

UCLASS()
class DEEP_API ADeepGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	/** 1-based descent stop. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	int32 Stop = 1;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	int32 Credits = 0;

	/** Hull rating N lets the sub descend to stop N. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	int32 HullRating = 1;

	/** Replacement diver number; goes up each time a diver is written off. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Deep")
	int32 DiverNumber = 1;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
