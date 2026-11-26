// D2: Deep.Judder <seconds> [label] records the player's view and the hull every frame, after the camera has been placed
// (FWorldDelegates::OnWorldPostActorTick), then logs DEEPJUDDER {json} (Sim/DeepJudderRules) and writes the frames to
// Saved/Harness/judder-<label>.csv. Pair it with Deep.Pilot.Hold for a scripted drive.

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sim/DeepJudderRules.h"
#include "World/DeepArena.h"
#include "World/DeepSubmarine.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepJudder, Log, All);

namespace
{
	struct FJudderRecord
	{
		TWeakObjectPtr<UWorld> World;
		double Left = 0.0;
		FString Label;
		TArray<FDeepJudderSample> Samples;
		FDelegateHandle Handle;
	};

	void Finish(const TSharedRef<FJudderRecord>& Record)
	{
		FWorldDelegates::OnWorldPostActorTick.Remove(Record->Handle);
		const FDeepJudderStats Stats = DeepJudder::Measure(Record->Samples);
		UE_LOG(LogDeepJudder, Display, TEXT("DEEPJUDDER %s"), *DeepJudder::ToJson(Stats, Record->Label));
		FString Csv = TEXT("t,cam_x,cam_y,cam_z,cam_pitch,cam_yaw,cam_roll,hull_x,hull_y,hull_z,hull_yaw\n");
		for (const FDeepJudderSample& S : Record->Samples)
		{
			const FRotator C = S.CamRot.Rotator();
			Csv += FString::Printf(TEXT("%.5f,%.2f,%.2f,%.2f,%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%.3f\n"), S.TimeS, S.CamCm.X, S.CamCm.Y, S.CamCm.Z,
				C.Pitch, C.Yaw, C.Roll, S.HullCm.X, S.HullCm.Y, S.HullCm.Z, S.HullRot.Rotator().Yaw);
		}
		const FString Safe = Record->Label.IsEmpty() ? TEXT("record") : FPaths::MakeValidFileName(Record->Label);
		FFileHelper::SaveStringToFile(Csv, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Harness"), FString::Printf(TEXT("judder-%s.csv"), *Safe)));
	}

	FAutoConsoleCommandWithWorldAndArgs JudderCommand(
		TEXT("Deep.Judder"),
		TEXT("Deep.Judder <seconds> [label]: records the view and the hull every frame for that long, then logs DEEPJUDDER {json} (how far each frame is off a smooth path) and writes Saved/Harness/judder-<label>.csv."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World)
			{
				return;
			}
			TSharedRef<FJudderRecord> Record = MakeShared<FJudderRecord>();
			Record->World = World;
			Record->Left = Args.Num() > 0 ? FMath::Max(0.5, FCString::Atod(*Args[0])) : 20.0;
			Record->Label = Args.Num() > 1 ? Args[1] : TEXT("");
			TWeakPtr<FJudderRecord> Weak = Record;
			Record->Handle = FWorldDelegates::OnWorldPostActorTick.AddLambda([Weak](UWorld* TickWorld, ELevelTick, float DeltaSeconds)
			{
				TSharedPtr<FJudderRecord> Pinned = Weak.Pin();
				if (!Pinned.IsValid() || TickWorld != Pinned->World.Get())
				{
					return;
				}
				const APlayerController* PC = TickWorld->GetFirstPlayerController();
				const ADeepSubmarine* Sub = DeepArena::FindSubmarine(TickWorld);
				if (PC && PC->PlayerCameraManager && Sub)
				{
					const FMinimalViewInfo& View = PC->PlayerCameraManager->GetCameraCacheView();
					FDeepJudderSample Sample;
					Sample.TimeS = TickWorld->GetTimeSeconds();
					Sample.CamCm = View.Location;
					Sample.CamRot = View.Rotation.Quaternion();
					Sample.HullCm = Sub->GetActorLocation();
					Sample.HullRot = Sub->GetActorQuat();
					Pinned->Samples.Add(Sample);
				}
				Pinned->Left -= DeltaSeconds;
				if (Pinned->Left <= 0.0)
				{
					Finish(Pinned.ToSharedRef());
				}
			});
			// The delegate holds only a weak pointer: keep the record alive until it finishes.
			static TArray<TSharedRef<FJudderRecord>> Live;
			Live.RemoveAll([](const TSharedRef<FJudderRecord>& R) { return R->Left <= 0.0 || !R->World.IsValid(); });
			Live.Add(Record);
			UE_LOG(LogDeepJudder, Display, TEXT("Deep.Judder: recording %.1f s (%s)"), Record->Left, *Record->Label);
		}));
}
