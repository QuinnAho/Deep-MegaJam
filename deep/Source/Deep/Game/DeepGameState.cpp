#include "Game/DeepGameState.h"

#include "Net/UnrealNetwork.h"
#include "Sim/DeepCaveRules.h"
#include "Sim/DeepPilotRules.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepArena.h"
#include "World/DeepShaft.h"
#include "World/DeepHelmLevers.h"

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
	SubYawDeg = 0.f;
	HelmThrottleNotch = 0;
	HelmBallast = 0.f;
	HelmSpeedMps = 0.f;
	HelmSurgeMps = 0.f;
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
	// Every machine: the helm's levers on the sub, moving with the replicated state.
	DeepHelmLevers::Ensure(GetWorld());
}

float ADeepGameState::GetHullIntegrity01() const
{
	return UDeepCaveRules::HullIntegrity01(HullDamage, UDeepTuningSettings::Get().Cave);
}

bool ADeepGameState::IsSettledAtStop() const
{
	return bParked && SubOffsetM.IsNearlyZero(0.01f) && FMath::IsNearlyZero(FRotator::NormalizeAxis(SubYawDeg), 0.05f)
		&& FMath::IsNearlyEqual(SubDepthM, UDeepTuningSettings::Get().GetStop(Stop).SubDepthM, 0.01f);
}

void ADeepGameState::StepParkGlide(float DeltaSeconds)
{
	// Deep.World.Caves: the helm parks at a berth where it is (ParkNear); nothing glides the hull home through rock.
	if (DeepArena::IsWorldCaves(GetWorld()))
	{
		return;
	}
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const float StopDepth = Tuning.GetStop(Stop).SubDepthM;
	const bool bAtDepth = !bParked || SubDepthM == StopDepth;
	if (SubOffsetM.IsZero() && SubYawDeg == 0.f && bAtDepth)
	{
		return;
	}
	// Shaft off: no offset at all. Parked: glide home to the stop's frame (offset 0, heading 0, its depth) in about 2 s;
	// the hatch opens once there (DeepArena::ApplyStop, IsSettledAtStop).
	bool bChanged = false;
	if (!DeepShaft::IsOn() && !SubOffsetM.IsZero())
	{
		SubOffsetM = FVector2D::ZeroVector;
		bChanged = true;
	}
	if (bParked)
	{
		const FDeepPilotTuning& P = Tuning.Pilot;
		const float MinMps = FMath::Max(P.ParkGlideMinMps, Tuning.Cave.ParkGlideMps);
		const float Gap = SubOffsetM.Size();
		if (Gap > 0.f)
		{
			const float Next = UDeepPilotRules::GlideStep(Gap, 0.f, DeltaSeconds, MinMps, P);
			SubOffsetM = Next <= 0.f ? FVector2D::ZeroVector : SubOffsetM * (Next / Gap);
		}
		SubYawDeg = UDeepPilotRules::GlideYawStep(SubYawDeg, DeltaSeconds, P);
		SubDepthM = UDeepPilotRules::GlideStep(SubDepthM, StopDepth, DeltaSeconds, P.ParkGlideMinMps, P);
		bChanged = true;
	}
	if (!bChanged)
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
	DOREPLIFETIME(ADeepGameState, SubYawDeg);
	DOREPLIFETIME(ADeepGameState, HelmThrottleNotch);
	DOREPLIFETIME(ADeepGameState, HelmBallast);
	DOREPLIFETIME(ADeepGameState, HelmSpeedMps);
	DOREPLIFETIME(ADeepGameState, HelmSurgeMps);
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
	DOREPLIFETIME(ADeepGameState, bWorldStill);
	DOREPLIFETIME(ADeepGameState, bWorldCaves);
}
