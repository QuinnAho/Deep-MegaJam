// Cheats for the air line (rescope, Nov 15): unclip by command, and a serpent bite on the rope for shots and tests.

#include "CoreMinimal.h"
#include "Debug/DeepStateReport.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Diver/DeepTetherComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Monster/DeepMonsterBrain.h"
#include "Monster/DeepSerpent.h"
#include "Sim/DeepRopeSim.h"
#include "World/DeepRopeComponent.h"

namespace
{
	/** The diver the cheats act on (as Debug/DeepCheats). */
	ADeepDiver* AirLineDiver(UWorld* World)
	{
		return World ? DeepStateReport::FindDiver(World) : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs UnclipCommand(
		TEXT("Deep.Unclip"),
		TEXT("Deep.Unclip: the diver unclips the air line (as the T key): bailout air, free re-clip at the hatch."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ADeepDiver* Diver = AirLineDiver(World);
			if (!Diver || !Diver->GetRun())
			{
				UE_LOG(LogTemp, Warning, TEXT("Deep.Unclip: no diver"));
				return;
			}
			Diver->GetRun()->Unclip();
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs BiteCommand(
		TEXT("Deep.AirLine.Bite"),
		TEXT("Deep.AirLine.Bite [alpha]: puts the serpent's head on the air line (alpha 0 hatch .. 1 diver, default 0.5), stalking, and checks the bite."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ADeepDiver* Diver = AirLineDiver(World);
			UDeepTetherComponent* Tether = Diver ? Diver->GetTether() : nullptr;
			UDeepMonsterBrain* Brain = World ? World->GetSubsystem<UDeepMonsterBrain>() : nullptr;
			ADeepSerpent* Serpent = Brain ? Brain->GetSerpent() : nullptr;
			if (!Tether || !Serpent)
			{
				UE_LOG(LogTemp, Warning, TEXT("Deep.AirLine.Bite: needs a diver and a serpent (stop 2+)"));
				return;
			}
			const float Alpha = Args.Num() >= 1 ? FMath::Clamp(FCString::Atof(*Args[0]), 0.f, 1.f) : 0.5f;
			const UDeepRopeComponent* Rope = Tether->GetLineRope();
			FVector OnLine = FMath::Lerp(Tether->GetAnchorLocation(), Diver->GetTetherAttachLocation(), Alpha);
			if (Rope && Rope->GetSim().IsValid())
			{
				const TArray<FVector>& Pos = Rope->GetSim().Pos;
				OnLine = Pos[FMath::Clamp(FMath::RoundToInt(Alpha * (Pos.Num() - 1)), 0, Pos.Num() - 1)];
			}
			Serpent->PlaceHead(OnLine, (Diver->GetActorLocation() - OnLine).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector));
			Brain->ForceState(EDeepMonsterState::Stalk);
			const bool bBitten = Tether->CheckSerpentBite();
			UE_LOG(LogTemp, Display, TEXT("DEEPAIRLINE {\"bitten\":%s,\"run_state\":\"%s\"}"), bBitten ? TEXT("true") : TEXT("false"),
				Diver->GetRun() ? *UDeepRunComponent::StateName(Diver->GetRun()->GetState()) : TEXT("none"));
			DeepStateReport::LogState(World);
		}));
}
