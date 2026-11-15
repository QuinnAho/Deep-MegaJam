#include "Diver/DeepDiver.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Diver/DeepRunComponent.h"
#include "Diver/DeepTetherComponent.h"
#include "EngineUtils.h"
#include "Harpoon/DeepHarpoonComponent.h"
#include "World/DeepSubmarine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepArena.h"
#include "World/DeepSalvage.h"
#include "World/DeepShapes.h"

namespace
{
	constexpr float EyeHeightCm = 55.f;
	constexpr float HarnessHeightCm = -30.f;
	constexpr float HarnessBackCm = 45.f;
	constexpr float HarnessSideCm = 35.f;
}

ADeepDiver::ADeepDiver()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(35.f, 80.f);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->DefaultLandMovementMode = MOVE_Flying;
	Movement->DefaultWaterMovementMode = MOVE_Flying;
	Movement->bOrientRotationToMovement = false;
	Movement->GravityScale = 0.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, EyeHeightCm));
	Camera->bUsePawnControlRotation = true;

	Lamp = CreateDefaultSubobject<USpotLightComponent>(TEXT("Lamp"));
	Lamp->SetupAttachment(Camera);
	// A little right of and below the eye, like a helmet lamp, so the beam reads in the fog.
	Lamp->SetRelativeLocation(FVector(10.f, 18.f, -12.f));
	Lamp->SetMobility(EComponentMobility::Movable);
	Lamp->SetLightColor(FLinearColor(1.f, 0.95f, 0.85f));
	// A plain spotlight: no light shafts.
	Lamp->SetVolumetricScatteringIntensity(0.f);

	Tether = CreateDefaultSubobject<UDeepTetherComponent>(TEXT("Tether"));
	Run = CreateDefaultSubobject<UDeepRunComponent>(TEXT("Run"));
	Harpoon = CreateDefaultSubobject<UDeepHarpoonComponent>(TEXT("Harpoon"));

	TetherLine = DeepShapes::CreateShape(this, TEXT("TetherLine"), DeepShapes::Cylinder);
	TetherLine->SetupAttachment(GetCapsuleComponent());
	TetherLine->SetUsingAbsoluteLocation(true);
	TetherLine->SetUsingAbsoluteRotation(true);
	TetherLine->SetUsingAbsoluteScale(true);
	TetherLine->SetCastShadow(false);
	// Lighting channel 1 only: lit by the scene's even key and fill (ADeepEnvironment), not by the helmet lamp right
	// next to it, which made it glow.
	TetherLine->SetLightingChannels(false, true, false);
	Tether->SetLineMesh(TetherLine);
}

void ADeepDiver::BeginPlay()
{
	Super::BeginPlay();
	ApplySwimTuning();

	const FDeepLampTuning& LampTuning = UDeepTuningSettings::Get().Lamp;
	Lamp->SetIntensity(LampTuning.Intensity);
	Lamp->SetInnerConeAngle(LampTuning.InnerConeDeg);
	Lamp->SetOuterConeAngle(LampTuning.OuterConeDeg);
	Lamp->SetAttenuationRadius(LampTuning.RangeM * UDeepRules::CmPerM);
	if (HasAuthority())
	{
		bLampOn = LampTuning.bStartsOn;
	}
	OnRep_Lamp();

	// A dull yellow cable: reads against the dark water and the floor without glowing.
	DeepShapes::Tint(TetherLine, FLinearColor(0.45f, 0.36f, 0.08f));
}

void ADeepDiver::ApplySwimTuning()
{
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	// Max fly speed covers the fastest velocity the swim rule can ask for, so input scale maps 1:1 to m/s.
	const float FastestMps = Tuning.Swim.SwimSpeedMps * FMath::Max(1.f, Tuning.Swim.SprintMultiplier) + Tuning.Weight.MaxSinkMps;
	Movement->MaxFlySpeed = FMath::Max(1.f, FastestMps * UDeepRules::CmPerM);
	Movement->MaxAcceleration = Tuning.Swim.AccelerationMps2 * UDeepRules::CmPerM;
	Movement->BrakingDecelerationFlying = Tuning.Swim.DragMps2 * UDeepRules::CmPerM;
	Movement->SetMovementMode(MOVE_Flying);
}

void ADeepDiver::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Movement input only comes from the controlling machine; character movement replicates the result.
	if (!IsLocallyControlled())
	{
		return;
	}
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Movement->MovementMode != MOVE_Flying)
	{
		Movement->SetMovementMode(MOVE_Flying);
	}

	// Asking to be cut loose: hold the key while tethered.
	if (bCutHeld && Run->GetState() == EDeepRunState::Tethered)
	{
		CutHoldSeconds += DeltaSeconds;
		if (CutHoldSeconds >= UDeepTuningSettings::Get().Run.CutHoldSeconds)
		{
			CutHoldSeconds = 0.f;
			bCutHeld = false;
			Run->RequestCut();
		}
	}
	else
	{
		CutHoldSeconds = 0.f;
	}

	if (!Run->CanSwim())
	{
		// Dead, settling or descending: the diver hangs still.
		Movement->Velocity = FVector::ZeroVector;
		return;
	}
	if (bWinching)
	{
		// The tether component moves the diver while the winch runs.
		return;
	}

	const FRotator View = GetControlRotation();
	const FVector Forward = View.Vector();
	const FVector Right = FRotationMatrix(View).GetScaledAxis(EAxis::Y);
	const FVector Wish = Forward * MoveInput.Y + Right * MoveInput.X + FVector::UpVector * VerticalInput;

	const FVector VelocityMps = UDeepRules::SwimVelocityMps(Wish, bSprinting, GetWeight(), UDeepTuningSettings::Get().Swim);
	const float SpeedCm = static_cast<float>(VelocityMps.Size()) * UDeepRules::CmPerM;
	if (SpeedCm > KINDA_SMALL_NUMBER)
	{
		AddMovementInput(VelocityMps.GetSafeNormal(), SpeedCm / Movement->MaxFlySpeed);
	}
}

float ADeepDiver::GetCargoKg() const
{
	return UDeepRules::CargoKg(Cargo);
}

int32 ADeepDiver::GetCargoValue() const
{
	return UDeepRules::CargoValue(Cargo);
}

FDeepWeightResult ADeepDiver::GetWeight() const
{
	return UDeepRules::SolveWeight(GetCargoKg(), UDeepTuningSettings::Get().Weight);
}

float ADeepDiver::GetDepthM() const
{
	return UDeepRules::DepthM(static_cast<float>(GetActorLocation().Z), static_cast<float>(Tether->GetAnchorLocation().Z), Tether->GetAnchorDepthM());
}

FString ADeepDiver::GetZoneName() const
{
	return UDeepRules::ZoneName(GetDepthM(), UDeepTuningSettings::Get().Zones);
}

FVector ADeepDiver::GetTetherAttachLocation() const
{
	// On the back, behind the camera: the line leaves the diver out of view instead of streaking up the middle
	// of the screen, lit by the lamp.
	return GetActorLocation() + FVector(0.f, 0.f, HarnessHeightCm) - GetActorForwardVector() * HarnessBackCm - GetActorRightVector() * HarnessSideCm;
}

ADeepSalvage* ADeepDiver::FindGrabTarget() const
{
	TArray<ADeepSalvage*> Pieces;
	TArray<FVector> Locations;
	TArray<float> Radii;
	for (TActorIterator<ADeepSalvage> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It))
		{
			Pieces.Add(*It);
			Locations.Add(It->GetActorLocation());
			Radii.Add(It->GetRadiusM());
		}
	}
	const int32 Pick = UDeepRules::PickGrabTarget(Camera->GetComponentLocation(), GetControlRotation().Vector(), Locations, Radii, UDeepTuningSettings::Get().Swim);
	return Pieces.IsValidIndex(Pick) ? Pieces[Pick] : nullptr;
}

void ADeepDiver::Grab()
{
	if (HasAuthority())
	{
		DoGrab();
	}
	else
	{
		ServerGrab();
	}
}

void ADeepDiver::ServerGrab_Implementation()
{
	DoGrab();
}

void ADeepDiver::DoGrab()
{
	if (!Run->CanSwim())
	{
		return;
	}
	if (ADeepSalvage* Target = FindGrabTarget())
	{
		const FDeepCargoItem Item = Target->ToCargo();
		Cargo.Add(Item);
		Target->Destroy();
		Run->NotifyPickup(Item);
	}
}

float ADeepDiver::GetCutHoldFraction() const
{
	const float Hold = UDeepTuningSettings::Get().Run.CutHoldSeconds;
	return Hold > 0.f ? FMath::Clamp(CutHoldSeconds / Hold, 0.f, 1.f) : 0.f;
}

void ADeepDiver::Continue()
{
	if (Run->GetState() == EDeepRunState::Settling)
	{
		Run->RequestContinue();
	}
}

void ADeepDiver::ClearCargo()
{
	if (HasAuthority())
	{
		Cargo.Reset();
	}
}

void ADeepDiver::ResetToStart()
{
	const ADeepSubmarine* Sub = Tether->GetSubmarine();
	if (!Sub)
	{
		return;
	}
	const FTransform Start = Sub->GetDiverStartTransform();
	TeleportDiver(Start.GetLocation());
	FRotator View = Start.Rotator();
	View.Pitch = UDeepTuningSettings::Get().Arena.DiverStartPitchDeg;
	View.Roll = 0.f;
	SetActorRotation(FRotator(0.f, View.Yaw, 0.f));
	if (AController* PawnController = GetController())
	{
		PawnController->SetControlRotation(View);
	}
}

void ADeepDiver::Drop()
{
	if (HasAuthority())
	{
		DoDrop();
	}
	else
	{
		ServerDrop();
	}
}

void ADeepDiver::ServerDrop_Implementation()
{
	DoDrop();
}

void ADeepDiver::DoDrop()
{
	if (Cargo.IsEmpty())
	{
		return;
	}
	const FDeepCargoItem Item = Cargo.Pop();
	FDeepSalvageKind Kind;
	UDeepTuningSettings::Get().FindSalvageKind(Item.Size, Kind);
	Kind.Size = Item.Size;
	Kind.Kg = Item.Kg;
	Kind.Value = Item.Value;
	const FVector At = Camera->GetComponentLocation() + GetControlRotation().Vector() * 120.f;
	DeepArena::SpawnSalvage(GetWorld(), Kind, At);
}

void ADeepDiver::SetWinching(bool bInWinching)
{
	if (bInWinching && Run->GetState() != EDeepRunState::Tethered)
	{
		bInWinching = false;
	}
	bWinching = bInWinching;
	if (!HasAuthority())
	{
		ServerSetWinching(bInWinching);
	}
}

void ADeepDiver::ServerSetWinching_Implementation(bool bInWinching)
{
	bWinching = bInWinching && Run->GetState() == EDeepRunState::Tethered;
}

void ADeepDiver::ToggleLamp()
{
	bLampOn = !bLampOn;
	OnRep_Lamp();
	if (!HasAuthority())
	{
		ServerSetLamp(bLampOn);
	}
}

void ADeepDiver::ServerSetLamp_Implementation(bool bOn)
{
	bLampOn = bOn;
	OnRep_Lamp();
}

void ADeepDiver::OnRep_Lamp()
{
	Lamp->SetVisibility(bLampOn);
}

void ADeepDiver::AddCargo(const FDeepCargoItem& Item)
{
	if (HasAuthority())
	{
		Cargo.Add(Item);
	}
}

void ADeepDiver::TeleportDiver(const FVector& LocationCm)
{
	SetActorLocation(LocationCm, false, nullptr, ETeleportType::TeleportPhysics);
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	Tether->UpdateTether(0.f);
}

void ADeepDiver::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADeepDiver, Cargo);
	DOREPLIFETIME(ADeepDiver, bWinching);
	DOREPLIFETIME(ADeepDiver, bLampOn);
}
