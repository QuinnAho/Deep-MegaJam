// Live tuning for the generated cave (Sim/DeepCaveRules::WallRadiusM): change a number in Project Settings > DEEP Tuning >
// Cave while playing, and the shaft and the caverns rebuild from it (by hand with Deep.Cave.Rebuild, or by themselves
// with Deep.Cave.AutoRebuild 1, the default). Local to each machine, like the rock itself.

#include "CoreMinimal.h"
#include "Debug/DeepStateReport.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameState.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepCavern.h"
#include "World/DeepShaft.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepCaveTune, Log, All);

namespace
{
	TAutoConsoleVariable<int32> CVarDeepCaveAutoRebuild(
		TEXT("Deep.Cave.AutoRebuild"), 1,
		TEXT("1 = the cave rebuilds by itself when a DEEP Tuning > Cave number changes (checked twice a second); 0 = only Deep.Cave.Rebuild."));
}

bool DeepShaft::AutoRebuild()
{
	return CVarDeepCaveAutoRebuild.GetValueOnGameThread() != 0;
}

double DeepShaft::RebuildAll(UWorld* World, bool bSyncCook)
{
	if (!World)
	{
		return 0.0;
	}
	const double Start = FPlatformTime::Seconds();
	int32 Shafts = 0;
	int32 Caverns = 0;
	for (TActorIterator<ADeepShaft> It(World); It; ++It)
	{
		It->Rebuild();
		++Shafts;
	}
	const ADeepGameState* State = World->GetGameState<ADeepGameState>();
	for (TActorIterator<ADeepCavern> It(World); It; ++It)
	{
		It->Build(/*bAsyncCook*/ !bSyncCook);
		It->UpdateShells(State ? State->Stop : 1, false);
		++Caverns;
	}
	const double Ms = (FPlatformTime::Seconds() - Start) * 1000.0;
	const FDeepCaveTuning& Cave = UDeepTuningSettings::Get().Cave;
	UE_LOG(LogDeepCaveTune, Display, TEXT("DEEPCAVE rebuilt %d shaft(s) and %d cavern set(s) in %.0f ms (seed %d, octaves %d, amplitude %.1f m, scale %.1f m, ridge %.2f, strata %.1f m / %.1f m)"),
		Shafts, Caverns, Ms, Cave.RockSeed, Cave.RockOctaves, Cave.RockAmplitudeM, Cave.RockScaleM, Cave.RockRidge, Cave.StrataSpacingM, Cave.StrataDepthM);
	return Ms;
}

namespace
{
	FAutoConsoleCommandWithWorldAndArgs CaveRebuildCommand(
		TEXT("Deep.Cave.Rebuild"),
		TEXT("Deep.Cave.Rebuild [seed]: rebuilds the shaft and the caverns from DEEP Tuning > Cave (a seed sets Cave.RockSeed first)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() > 0 && Args[0].IsNumeric())
			{
				GetMutableDefault<UDeepTuningSettings>()->Cave.RockSeed = FCString::Atoi(*Args[0]);
			}
			DeepShaft::RebuildAll(World);
			if (World)
			{
				DeepStateReport::LogState(World);
			}
		}));
}
