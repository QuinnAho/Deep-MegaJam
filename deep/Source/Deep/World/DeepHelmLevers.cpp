#include "World/DeepHelmLevers.h"

#include "Audio/DeepAudioSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Debug/DeepStateReport.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/World.h"
#include "Game/DeepGameState.h"
#include "HAL/IConsoleManager.h"
#include "Sim/DeepPilotRules.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepEventBus.h"
#include "World/DeepArena.h"
#include "World/DeepRoomLayout.h"
#include "World/DeepShapes.h"
#include "World/DeepSubmarine.h"
#include "World/DeepSubRoom.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepPilot, Log, All);

namespace
{
	// Shape sizes (cm): a squat housing on the deck, the arm from its top, a knob on the arm.
	constexpr float BaseW = 18.f;
	constexpr float BaseH = 42.f;
	constexpr float ArmLen = 52.f;
	constexpr float ArmR = 2.6f;
	constexpr float KnobD = 10.f;
	const FLinearColor Housing(0.10f, 0.11f, 0.11f);
	const FLinearColor ThrottleKnob(0.55f, 0.42f, 0.18f);
	const FLinearColor BallastKnob(0.45f, 0.07f, 0.05f);

	UStaticMeshComponent* MakePart(UDeepHelmLevers& Owner, USceneComponent* Parent, const FString& Name, const TCHAR* Mesh, const FVector& AtCm, const FVector& SizeCm)
	{
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Owner.GetOwner(), *Name);
		Part->SetStaticMesh(DeepShapes::LoadMesh(Mesh));
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		Part->SetupAttachment(Parent);
		Part->SetRelativeLocation(AtCm);
		Part->SetRelativeScale3D(SizeCm / DeepShapes::ShapeSizeCm);
		Part->RegisterComponent();
		return Part;
	}
}

UDeepHelmLevers::UDeepHelmLevers()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(false);
}

void UDeepHelmLevers::BuildLever(FLever& Lever, const TCHAR* Name, const FVector2D& AtM, const FLinearColor& KnobPaint)
{
	// On the room set's deck (World/DeepRoomLayout), beside the helm seat.
	const float DeckZ = static_cast<float>(DeepRoomLayout::DeckTopCm(UDeepTuningSettings::Get().Sub));
	const FVector At(AtM.X * UDeepRules::CmPerM, AtM.Y * UDeepRules::CmPerM, DeckZ);
	Lever.Base = MakePart(*this, this, FString::Printf(TEXT("%sBase"), Name), DeepShapes::Cube, At + FVector(0.f, 0.f, BaseH * 0.5f), FVector(BaseW, BaseW, BaseH));
	Lever.Pivot = NewObject<USceneComponent>(GetOwner(), *FString::Printf(TEXT("%sPivot"), Name));
	Lever.Pivot->SetupAttachment(this);
	Lever.Pivot->SetRelativeLocation(At + FVector(0.f, 0.f, BaseH));
	Lever.Pivot->RegisterComponent();
	Lever.Arm = MakePart(*this, Lever.Pivot, FString::Printf(TEXT("%sArm"), Name), DeepShapes::Cylinder, FVector(0.f, 0.f, ArmLen * 0.5f), FVector(ArmR * 2.f, ArmR * 2.f, ArmLen));
	Lever.Knob = MakePart(*this, Lever.Pivot, FString::Printf(TEXT("%sKnob"), Name), DeepShapes::Sphere, FVector(0.f, 0.f, ArmLen), FVector(KnobD, KnobD, KnobD * 1.3f));
	// Worn steel: a dark painted housing chipped at the edges, a bare rubbed arm, a painted knob.
	DeepShapes::Steel(Lever.Base, Housing, 0.45f, 0.25f, 0.7f, 0.2f);
	DeepShapes::Steel(Lever.Arm, Housing, 0.9f, 0.1f, 0.45f, 0.6f);
	DeepShapes::Steel(Lever.Knob, KnobPaint, 0.3f, 0.05f, 0.5f, 0.1f);
	Parts.Append({ Lever.Base, Lever.Pivot, Lever.Arm, Lever.Knob });
}

void UDeepHelmLevers::Build()
{
	if (bBuilt || !GetOwner())
	{
		return;
	}
	bBuilt = true;
	const FDeepPilotTuning& P = UDeepTuningSettings::Get().Pilot;
	BuildLever(Throttle, TEXT("Throttle"), P.ThrottleLeverM, ThrottleKnob);
	BuildLever(Ballast, TEXT("Ballast"), P.BallastLeverM, BallastKnob);
}

void UDeepHelmLevers::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const ADeepGameState* State = GetWorld() ? GetWorld()->GetGameState<ADeepGameState>() : nullptr;
	if (!State || !bBuilt)
	{
		return;
	}
	const FDeepPilotTuning& P = UDeepTuningSettings::Get().Pilot;
	// Forward is ahead (and flood: the bow goes down); a pitch below zero tips the arm forward. D-070's direct drive:
	// the levers follow the held keys (W/S, C/Space) and spring back when let go.
	const float ThrottleShare = State->IsHelmDirect() ? FMath::Clamp(State->HelmHold.Surge, -1.f, 1.f) : UDeepPilotRules::NotchFraction(State->HelmThrottleNotch, P);
	const float BallastShare = State->IsHelmDirect() ? FMath::Clamp(State->HelmHold.Heave, -1.f, 1.f) : FMath::Clamp(State->HelmBallast, -1.f, 1.f);
	const float WantThrottle = -ThrottleShare * P.LeverThrowDeg;
	const float WantBallast = -BallastShare * P.LeverThrowDeg;
	const float Follow = 1.f - FMath::Exp(-FMath::Max(0.f, P.LeverFollowPerS) * FMath::Max(0.f, DeltaTime));
	ThrottlePitchDeg += (WantThrottle - ThrottlePitchDeg) * Follow;
	BallastPitchDeg += (WantBallast - BallastPitchDeg) * Follow;
	Throttle.Pivot->SetRelativeRotation(FRotator(ThrottlePitchDeg, 0.f, 0.f));
	Ballast.Pivot->SetRelativeRotation(FRotator(BallastPitchDeg, 0.f, 0.f));

	// A click per notch, and as the ballast lever leaves or drops into its neutral detent (the levers' mode only).
	const int32 BallastStep = State->HelmBallast > 0.f ? 1 : (State->HelmBallast < 0.f ? -1 : 0);
	if (!State->IsHelmDirect() && (State->HelmThrottleNotch != LastNotch || BallastStep != LastBallastStep))
	{
		LastNotch = State->HelmThrottleNotch;
		LastBallastStep = BallastStep;
		if (UDeepAudioSubsystem* Audio = UDeepAudioSubsystem::Get(this))
		{
			Audio->PlayEvent(FName(DeepEvent::HelmTake), true);
		}
	}
}

namespace DeepHelmLevers
{
	UDeepHelmLevers* Ensure(UWorld* World)
	{
		// The levers stand beside the seat in the room set (World/DeepSubRoom), where the divers walk.
		ADeepSubRoom* Room = DeepArena::EnsureRoom(World);
		if (!Room)
		{
			return nullptr;
		}
		if (UDeepHelmLevers* Levers = Room->FindComponentByClass<UDeepHelmLevers>())
		{
			return Levers;
		}
		UDeepHelmLevers* Levers = NewObject<UDeepHelmLevers>(Room, TEXT("HelmLevers"));
		Levers->SetupAttachment(Room->GetRootComponent());
		Levers->RegisterComponent();
		Levers->Build();
		return Levers;
	}
}

// ---------------------------------------------------------------- cheats (server)

namespace
{
	/** The first diver's run, seated at the helm (boards and sits if need be). */
	UDeepRunComponent* PilotRun(UWorld* World, const TCHAR* Command)
	{
		ADeepDiver* Diver = DeepStateReport::FindDiver(World);
		UDeepRunComponent* Run = Diver ? Diver->GetRun() : nullptr;
		if (!Run || !Diver->HasAuthority())
		{
			UE_LOG(LogDeepPilot, Warning, TEXT("%s: needs a diver, on the server"), Command);
			return nullptr;
		}
		if (Run->CanSwim())
		{
			Run->EnterSub();
		}
		Run->TakeHelm();
		return Run;
	}

	FAutoConsoleCommandWithWorldAndArgs PilotThrottleCommand(
		TEXT("Deep.Pilot.Throttle"),
		TEXT("Deep.Pilot.Throttle <+1|-1|notch>: steps the throttle a notch (+1 ahead, -1 astern), or 'set <notch>' (-2..3). Takes the helm."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UDeepRunComponent* Run = PilotRun(World, TEXT("Deep.Pilot.Throttle"));
			if (!Run || Args.Num() < 1)
			{
				return;
			}
			if (Run->IsDirectDrive())
			{
				// D-070: holds W (+) or S (-) as the keys would; 'set <notch>' holds that share of full ahead or astern.
				FDeepDriveInput Hold = Run->GetDriveHold();
				const bool bSet = Args[0].ToLower() == TEXT("set") && Args.Num() >= 2;
				Hold.Surge = bSet ? UDeepPilotRules::NotchFraction(FCString::Atoi(*Args[1]), UDeepTuningSettings::Get().Pilot) : FMath::Clamp(FCString::Atof(*Args[0]), -1.f, 1.f);
				Run->SetDriveHold(Hold);
			}
			else if (Args[0].ToLower() == TEXT("set") && Args.Num() >= 2)
			{
				const int32 Want = FCString::Atoi(*Args[1]);
				for (int32 Guard = 0; Guard < 12 && Run->GetThrottleNotch() != Want; ++Guard)
				{
					const int32 Before = Run->GetThrottleNotch();
					Run->StepThrottle(Want > Before ? 1 : -1);
					if (Run->GetThrottleNotch() == Before)
					{
						break;
					}
				}
			}
			else
			{
				Run->StepThrottle(FCString::Atoi(*Args[0]));
			}
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs PilotBallastCommand(
		TEXT("Deep.Pilot.Ballast"),
		TEXT("Deep.Pilot.Ballast <-1..1>: sets the ballast lever (+ floods: sinks, - blows: rises, 0 neutral); direct drive: holds C (+) or Space (-). Takes the helm."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UDeepRunComponent* Run = PilotRun(World, TEXT("Deep.Pilot.Ballast"));
			ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
			if (!Run || !State || Args.Num() < 1)
			{
				return;
			}
			if (Run->IsDirectDrive())
			{
				FDeepDriveInput Hold = Run->GetDriveHold();
				Hold.Heave = FMath::Clamp(FCString::Atof(*Args[0]), -1.f, 1.f);
				Run->SetDriveHold(Hold);
			}
			else
			{
				State->HelmBallast = FMath::Clamp(FCString::Atof(*Args[0]), -1.f, 1.f);
			}
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs PilotRudderCommand(
		TEXT("Deep.Pilot.Rudder"),
		TEXT("Deep.Pilot.Rudder <-1..1>: holds the rudder (+ starboard, - port, 0 amidships) as A/D would. Takes the helm."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UDeepRunComponent* Run = PilotRun(World, TEXT("Deep.Pilot.Rudder"));
			if (!Run || Args.Num() < 1)
			{
				return;
			}
			if (Run->IsDirectDrive())
			{
				FDeepDriveInput Hold = Run->GetDriveHold();
				Hold.Turn = FMath::Clamp(FCString::Atof(*Args[0]), -1.f, 1.f);
				Run->SetDriveHold(Hold);
			}
			else
			{
				Run->SetPilotHold(0.f, FCString::Atof(*Args[0]));
			}
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs PilotStrafeCommand(
		TEXT("Deep.Pilot.Strafe"),
		TEXT("Deep.Pilot.Strafe <-1..1>: direct drive: holds E (+ starboard) or Q (- port). Takes the helm."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UDeepRunComponent* Run = PilotRun(World, TEXT("Deep.Pilot.Strafe"));
			if (!Run || Args.Num() < 1)
			{
				return;
			}
			FDeepDriveInput Hold = Run->GetDriveHold();
			Hold.Strafe = FMath::Clamp(FCString::Atof(*Args[0]), -1.f, 1.f);
			Run->SetDriveHold(Hold);
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs PilotHoldCommand(
		TEXT("Deep.Pilot.Hold"),
		TEXT("Deep.Pilot.Hold <ahead> <turn> <down> <strafe>: direct drive: holds the keys as -1..1 each (W/S, D/A, C/Space, E/Q) until changed; 0 0 0 0 lets go. Takes the helm."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UDeepRunComponent* Run = PilotRun(World, TEXT("Deep.Pilot.Hold"));
			if (!Run)
			{
				return;
			}
			FDeepDriveInput Hold;
			Hold.Surge = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.f;
			Hold.Turn = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.f;
			Hold.Heave = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 0.f;
			Hold.Strafe = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.f;
			Run->SetDriveHold(Hold);
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs PilotModeCommand(
		TEXT("Deep.Pilot.Mode"),
		TEXT("Deep.Pilot.Mode <direct|levers>: the helm's controls (D-070): direct drive (held keys, the default) or the levers (D-048), for comparison. Lets go of everything."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const FString Mode = Args.Num() > 0 ? Args[0].ToLower() : FString();
			IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("Deep.Pilot.Levers"));
			if (!Var || (Mode != TEXT("direct") && Mode != TEXT("levers")))
			{
				UE_LOG(LogDeepPilot, Warning, TEXT("Usage: Deep.Pilot.Mode direct|levers (now %s)"), ADeepGameState::WantsDirectDrive() ? TEXT("direct") : TEXT("levers"));
				return;
			}
			Var->Set(Mode == TEXT("levers") ? 1 : 0, ECVF_SetByConsole);
			if (World)
			{
				DeepStateReport::LogState(World);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs PilotViewCommand(
		TEXT("Deep.Pilot.View"),
		TEXT("Deep.Pilot.View <chase|cockpit>: the pilot's view on this machine (V at the helm), for frames and tests."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ADeepSubmarine* Sub = World ? DeepArena::FindSubmarine(World) : nullptr;
			const FString View = Args.Num() > 0 ? Args[0].ToLower() : FString();
			if (!Sub || (View != TEXT("chase") && View != TEXT("cockpit")))
			{
				UE_LOG(LogDeepPilot, Warning, TEXT("Usage: Deep.Pilot.View chase|cockpit"));
				return;
			}
			Sub->SetHelmCockpit(View == TEXT("cockpit"));
		}));

	FAutoConsoleCommandWithWorldAndArgs PilotAllStopCommand(
		TEXT("Deep.Pilot.AllStop"),
		TEXT("Deep.Pilot.AllStop: throttle to stop, ballast to neutral, rudder amidships; direct drive: lets go of every key."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDeepRunComponent* Run = PilotRun(World, TEXT("Deep.Pilot.AllStop")))
			{
				Run->SetPilotHold(0.f, 0.f);
				Run->AllStop();
				DeepStateReport::LogState(World);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs PilotYawCommand(
		TEXT("Deep.Pilot.Yaw"),
		TEXT("Deep.Pilot.Yaw <deg>: turns the sub to a heading at once (the world turns about it; 0 = as parked)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
			if (!State || !State->HasAuthority() || Args.Num() < 1)
			{
				UE_LOG(LogDeepPilot, Warning, TEXT("Usage (server): Deep.Pilot.Yaw <deg>"));
				return;
			}
			State->SubYawDeg = FRotator::NormalizeAxis(FCString::Atof(*Args[0]));
			DeepArena::ApplySubDepth(World);
			DeepStateReport::LogState(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs PilotRunCommand(
		TEXT("Deep.Pilot.Run"),
		TEXT("Deep.Pilot.Run <seconds>: runs the helm that long at once in 0.1 s steps (the levers as they are)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UDeepRunComponent* Run = PilotRun(World, TEXT("Deep.Pilot.Run"));
			if (!Run || Args.Num() < 1)
			{
				return;
			}
			const int32 Steps = FMath::Clamp(FMath::RoundToInt(FCString::Atof(*Args[0]) / 0.1f), 0, 6000);
			for (int32 Step = 0; Step < Steps && Run->IsSeated(); ++Step)
			{
				Run->StepRun(0.1f);
			}
			DeepStateReport::LogState(World);
		}));
}
