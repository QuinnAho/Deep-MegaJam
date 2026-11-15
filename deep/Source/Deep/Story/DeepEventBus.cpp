#include "Story/DeepEventBus.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepEvent, Log, All);

namespace
{
	constexpr int32 MaxHistory = 64;
}

TArray<FName> DeepEvent::AllFired()
{
	return {
		DiveStart, FirstSalvage, SalvageSmall, SalvageMedium, SalvageLarge, Overweight, TetherTaut, WinchStart,
		Patience50, Patience25, Patience10, CutPatience, CutRequested, Bailout10s, Retether, DeathAir, Respawn,
		BankGood, BankPoor, BankEmpty, HullBought, DescendStop2, DescendStop3,
	};
}

UDeepEventBus* UDeepEventBus::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDeepEventBus>() : nullptr;
}

void UDeepEventBus::Broadcast(FName Event, const FDeepEventPayload& Payload)
{
	if (History.Num() >= MaxHistory)
	{
		History.RemoveAt(0);
	}
	History.Add(Event);
	UE_LOG(LogDeepEvent, Display, TEXT("DEEPEVENT %s n=%d gross=%d cut=%d net=%d depth=%.0f"),
		*Event.ToString(), Payload.DiverNumber, Payload.Gross, Payload.Cut, Payload.Net, Payload.DepthM);
	OnEvent.Broadcast(Event, Payload);
}

int32 UDeepEventBus::CountOf(FName Event) const
{
	int32 Count = 0;
	for (const FName& Name : History)
	{
		Count += Name == Event ? 1 : 0;
	}
	return Count;
}
