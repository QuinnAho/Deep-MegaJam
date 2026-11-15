// Console commands for the water effects: frame-time A/B and staging frames.

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "Debug/DeepStateReport.h"
#include "Diver/DeepDiver.h"
#include "Engine/World.h"
#include "FX/DeepFxSubsystem.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "HAL/IConsoleManager.h"
#include "World/DeepSalvage.h"

namespace DeepFxCheats
{
	static FTransform ViewOf(const ADeepDiver& Diver)
	{
		const UCameraComponent* Camera = Diver.FindComponentByClass<UCameraComponent>();
		return Camera ? Camera->GetComponentTransform() : Diver.GetActorTransform();
	}

	FAutoConsoleCommandWithWorldAndArgs FxCommand(
		TEXT("Deep.Fx"),
		TEXT("Deep.Fx <0|1>: bubbles and silt off or on (frame-time A/B)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDeepFxSubsystem* Fx = UDeepFxSubsystem::Get(World))
			{
				Fx->SetEnabled(Args.Num() < 1 || FCString::Atoi(*Args[0]) != 0);
				UE_LOG(LogTemp, Display, TEXT("DEEPFX enabled=%d"), Fx->IsEnabled() ? 1 : 0);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs BubblesCommand(
		TEXT("Deep.Fx.Bubbles"),
		TEXT("Deep.Fx.Bubbles [count=12] [size=1] [ahead_cm=60] [down_cm=25]: a burst of bubbles in front of the diver's eye."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ADeepDiver* Diver = DeepStateReport::FindDiver(World);
			UDeepFxSubsystem* Fx = UDeepFxSubsystem::Get(World);
			if (!Diver || !Fx)
			{
				return;
			}
			const int32 Count = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 12;
			const float Size = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 1.f;
			const float Ahead = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 60.f;
			const float Down = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 25.f;
			const FVector At = ViewOf(*Diver).TransformPosition(FVector(Ahead, 0.f, -Down));
			Fx->SpawnBubbles(At, Count, Size);
			UE_LOG(LogTemp, Display, TEXT("DEEPFX bubbles at %s count=%d"), *At.ToCompactString(), Count);
		}));

	FAutoConsoleCommandWithWorldArgsAndOutputDevice StateCommand(
		TEXT("Deep.Fx.State"),
		TEXT("Deep.Fx.State: bursts so far (bubbles, breath, silt)."),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>&, UWorld* World, FOutputDevice&)
		{
			if (const UDeepFxSubsystem* Fx = UDeepFxSubsystem::Get(World))
			{
				UE_LOG(LogTemp, Display, TEXT("DEEPFX {\"enabled\":%d,\"bubble_bursts\":%d,\"breath_bursts\":%d,\"silt_puffs\":%d}"),
					Fx->IsEnabled() ? 1 : 0, Fx->GetBubbleBursts(), Fx->GetBreathBursts(), Fx->GetSiltPuffs());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs StageGrabCommand(
		TEXT("Deep.Fx.StageGrab"),
		TEXT("Deep.Fx.StageGrab [back_cm=160] [up_cm=120] [grab=1]: shots. Puts the diver above and behind the nearest salvage, looking at it, then grabs it (the real grab, so the silt puff is the real one)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ADeepDiver* Diver = DeepStateReport::FindDiver(World);
			if (!Diver || !World)
			{
				return;
			}
			const float Back = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 160.f;
			const float Up = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 120.f;
			const bool bGrab = Args.Num() < 3 || FCString::Atoi(*Args[2]) != 0;
			ADeepSalvage* Best = nullptr;
			for (TActorIterator<ADeepSalvage> It(World); It; ++It)
			{
				if (!It->IsHidden() && It->GetKind().Size != EDeepSalvageSize::Relic && (!Best || FVector::DistSquared(It->GetActorLocation(), Diver->GetActorLocation()) < FVector::DistSquared(Best->GetActorLocation(), Diver->GetActorLocation())))
				{
					Best = *It;
				}
			}
			if (!Best)
			{
				UE_LOG(LogTemp, Warning, TEXT("DEEPFX StageGrab: no salvage"));
				return;
			}
			const FVector Target = Best->GetActorLocation();
			FVector Away = Diver->GetActorLocation() - Target;
			Away.Z = 0.f;
			Away = Away.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
			Diver->TeleportDiver(Target + Away * Back + FVector(0.f, 0.f, Up));
			if (AController* Controller = Diver->GetController())
			{
				FRotator View = (Target - Diver->GetPawnViewLocation()).Rotation();
				View.Roll = 0.f;
				Controller->SetControlRotation(View);
			}
			if (bGrab)
			{
				Diver->Grab();
			}
			UE_LOG(LogTemp, Display, TEXT("DEEPFX StageGrab %s at %s grab=%d"), *Best->GetName(), *Target.ToCompactString(), bGrab ? 1 : 0);
			DeepStateReport::LogState(World);
		}));
}
