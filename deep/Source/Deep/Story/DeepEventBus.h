// One place for named gameplay events. Gameplay code calls Broadcast; the Captain (and later sound, music, the
// monster) listen. Event names are the `event` column of Content/Data/CaptainLines.csv; use the DeepEvent constants.
// Events fire where the rule runs (the server). UDeepRunComponent forwards its diver's events to a remote owner.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DeepEventBus.generated.h"

class AActor;

/** Event names. Each must exist in the `event` column of CaptainLines.csv (Deep.Story.CaptainLines checks). */
namespace DeepEvent
{
	inline const TCHAR* DiveStart = TEXT("dive_start");
	inline const TCHAR* FirstSalvage = TEXT("first_salvage");
	inline const TCHAR* SalvageSmall = TEXT("salvage_small");
	inline const TCHAR* SalvageMedium = TEXT("salvage_medium");
	inline const TCHAR* SalvageLarge = TEXT("salvage_large");
	inline const TCHAR* Overweight = TEXT("overweight");
	inline const TCHAR* TetherTaut = TEXT("tether_taut");
	inline const TCHAR* WinchStart = TEXT("winch_start");
	inline const TCHAR* Patience50 = TEXT("patience_50");
	inline const TCHAR* Patience25 = TEXT("patience_25");
	inline const TCHAR* Patience10 = TEXT("patience_10");
	inline const TCHAR* CutPatience = TEXT("cut_patience");
	inline const TCHAR* CutRequested = TEXT("cut_requested");
	inline const TCHAR* Bailout10s = TEXT("bailout_10s");
	inline const TCHAR* Retether = TEXT("retether");
	inline const TCHAR* DeathAir = TEXT("death_air");
	inline const TCHAR* Respawn = TEXT("respawn");
	inline const TCHAR* BankGood = TEXT("bank_good");
	inline const TCHAR* BankPoor = TEXT("bank_poor");
	inline const TCHAR* BankEmpty = TEXT("bank_empty");
	inline const TCHAR* HullBought = TEXT("hull_bought");
	inline const TCHAR* DescendStop2 = TEXT("descend_stop2");
	inline const TCHAR* DescendStop3 = TEXT("descend_stop3");
	inline const TCHAR* MonsterFirstHeard = TEXT("monster_first_heard");
	inline const TCHAR* MonsterNear = TEXT("monster_near");
	inline const TCHAR* MonsterRepelled = TEXT("monster_repelled");
	inline const TCHAR* BiteAlarm = TEXT("bite_alarm");
	inline const TCHAR* CutBite = TEXT("cut_bite");
	inline const TCHAR* DeathMonster = TEXT("death_monster");
	inline const TCHAR* Title = TEXT("title");
	inline const TCHAR* RelicSeen = TEXT("relic_seen");
	inline const TCHAR* RelicGrabbed = TEXT("relic_grabbed");
	inline const TCHAR* RelicWinch = TEXT("relic_winch");
	inline const TCHAR* Win = TEXT("win");

	/** Every event the game fires today. */
	DEEP_API TArray<FName> AllFired();
}

/** Small payload. Tokens in Captain lines: {n} = DiverNumber, {gross}, {cut}, {net}, {depth} = DepthM rounded. */
USTRUCT(BlueprintType)
struct FDeepEventPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	int32 DiverNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	int32 Gross = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	int32 Cut = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	int32 Net = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	float DepthM = 0.f;

	/** The actor the event is about (usually the diver). May be null. */
	UPROPERTY(BlueprintReadOnly, Category = "Deep")
	TObjectPtr<AActor> Source = nullptr;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDeepEvent, FName /*Event*/, const FDeepEventPayload& /*Payload*/);

UCLASS()
class DEEP_API UDeepEventBus : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UDeepEventBus* Get(const UObject* WorldContext);

	/** Logs `DEEPEVENT <name> ...` and notifies every listener. */
	void Broadcast(FName Event, const FDeepEventPayload& Payload);

	FOnDeepEvent OnEvent;

	/** Most recent events, oldest first (bounded). For tests and DEEPSTATE. */
	const TArray<FName>& GetHistory() const { return History; }
	int32 CountOf(FName Event) const;
	FName GetLastEvent() const { return History.IsEmpty() ? NAME_None : History.Last(); }
	void ClearHistory() { History.Reset(); }

private:
	TArray<FName> History;
};
