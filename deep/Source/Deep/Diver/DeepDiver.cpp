#include "Diver/DeepDiver.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Diver/DeepRunComponent.h"
#include "Diver/DeepTetherComponent.h"
#include "EngineUtils.h"
#include "Game/DeepGameState.h"
#include "GameFramework/PlayerController.h"
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
	Lamp->SetMobility(EComponentMobility::Movable);
	// Hard shadows: a point source (style guide 1: the lamp is the main subject, hard shadows where it lands).
	Lamp->SetCastShadows(true);
	Lamp->SetSourceRadius(0.f);
	Lamp->SetSoftSourceRadius(0.f);
	// Offset, colour, beam strength and fog shadows come from FDeepLampTuning in BeginPlay.

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
	// Off the eye axis (right and up), so the beam's edge and the shadows it throws read in the murk.
	Lamp->SetRelativeLocation(FVector(LampTuning.OffsetForwardCm, LampTuning.OffsetRightCm, LampTuning.OffsetUpCm));
	Lamp->SetLightColor(LampTuning.Color);
	// The visible beam: the lamp lights the volumetric fog, and its own shadows cut the lit fog.
	Lamp->SetVolumetricScatteringIntensity(LampTuning.VolumetricScattering);
	Lamp->SetCastVolumetricShadow(LampTuning.bVolumetricShadow);
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
	Movement->MaxWalkSpeed = FMath::Max(1.f, Tuning.Sub.WalkSpeedMps * UDeepRules::CmPerM);
	Movement->SetMovementMode(Run && Run->IsInside() ? MOVE_Walking : MOVE_Flying);
}

void ADeepDiver::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The lamp needs the flashlight (D-023): it shows only once it is bought.
	if (Lamp->IsVisible() != ShowsLamp())
	{
		Lamp->SetVisibility(ShowsLamp());
	}
	if (Lamp->IsVisible())
	{
		// One slow, small breath in the light (style guide 5: 2% at 0.3 Hz). No flicker.
		const FDeepLampTuning& LampTuning = UDeepTuningSettings::Get().Lamp;
		const double T = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		Lamp->SetIntensity(LampTuning.Intensity * (1.f + LampTuning.WobbleFraction * FMath::Sin(UE_TWO_PI * LampTuning.WobbleHz * T)));
	}

	// Movement input only comes from the controlling machine; character movement replicates the result.
	if (!IsLocallyControlled())
	{
		return;
	}
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	SyncAboardMode();

	if (Run->IsSeated())
	{
		// At the helm: no walking; down (C, S) and up (Space, W) drive the sub.
		Movement->Velocity = FVector::ZeroVector;
		const float Helm = FMath::Clamp(-VerticalInput - MoveInput.Y, -1.f, 1.f);
		if (!FMath::IsNearlyEqual(Helm, LastHelmInputSent))
		{
			LastHelmInputSent = Helm;
			Run->SetHelmInput(Helm);
		}
		return;
	}
	if (!FMath::IsNearlyZero(LastHelmInputSent))
	{
		LastHelmInputSent = 0.f;
		Run->SetHelmInput(0.f);
	}
	if (Run->IsInside() || Run->IsOnTitle())
	{
		if (Run->CanWalk())
		{
			const FRotator Flat(0.f, GetControlRotation().Yaw, 0.f);
			const FVector Wish = FRotationMatrix(Flat).GetScaledAxis(EAxis::X) * MoveInput.Y + FRotationMatrix(Flat).GetScaledAxis(EAxis::Y) * MoveInput.X;
			if (!Wish.IsNearlyZero())
			{
				AddMovementInput(Wish.GetClampedToMaxSize(1.f), 1.f);
			}
		}
		return;
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

	FVector VelocityMps = UDeepRules::SwimVelocityMps(Wish, bSprinting, GetWeight(), UDeepTuningSettings::Get().Swim);
	if (const ADeepSubmarine* Sub = Tether->GetSubmarine())
	{
		// Pushing into the side of the hull slides the diver down it and under, toward the hatch, instead of pinning.
		const FTransform& SubFrame = Sub->GetActorTransform();
		const FVector Local = UDeepRules::HullSlideVelocity(SubFrame.InverseTransformPosition(GetActorLocation()),
			SubFrame.InverseTransformVectorNoScale(VelocityMps), Sub->GetSolidBoundsLocal(true),
			GetCapsuleComponent()->GetScaledCapsuleRadius(), GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),
			UDeepTuningSettings::Get().Swim.HullContactM * UDeepRules::CmPerM);
		VelocityMps = SubFrame.TransformVectorNoScale(Local);
	}
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

void ADeepDiver::SyncAboardMode()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	const bool bInside = Run->IsInside() || (Run->IsOnTitle() && Run->GetState() == EDeepRunState::Aboard);
	if (bInside && Movement->MovementMode == MOVE_Flying)
	{
		SetAboard(true);
	}
	else if (!bInside && Movement->MovementMode != MOVE_Flying)
	{
		SetAboard(false);
	}
	// Seated: look through the helm camera under the hull; standing or swimming: through the diver's eyes.
	if (APlayerController* Player = Cast<APlayerController>(GetController()))
	{
		AActor* Want = this;
		if (Run->IsSeated())
		{
			if (ADeepSubmarine* Sub = Tether->GetSubmarine())
			{
				Want = Sub;
			}
		}
		if (Player->GetViewTarget() != Want)
		{
			Player->SetViewTargetWithBlend(Want, 0.25f);
		}
	}
}

void ADeepDiver::SetAboard(bool bInside)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->GravityScale = bInside ? 1.f : 0.f;
	Movement->Velocity = FVector::ZeroVector;
	Movement->SetMovementMode(bInside ? MOVE_Walking : MOVE_Flying);
}

bool ADeepDiver::HasFlashlight() const
{
	const UWorld* World = GetWorld();
	const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
	return State && State->HasFlashlight();
}

bool ADeepDiver::IsLampOn() const
{
	return bLampOn && HasFlashlight();
}

void ADeepDiver::Use()
{
	if (HasAuthority())
	{
		Run->Use();
	}
	else
	{
		ServerUse();
	}
}

void ADeepDiver::ServerUse_Implementation()
{
	Run->Use();
}

void ADeepDiver::Grab()
{
	if (Run->IsInside())
	{
		Use();
		return;
	}
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

void ADeepDiver::ResetToRoom()
{
	const ADeepSubmarine* Sub = Tether->GetSubmarine();
	if (!Sub)
	{
		return;
	}
	const FTransform Start = Sub->GetRoomStartTransform();
	TeleportDiver(Start.GetLocation());
	FRotator View = Start.Rotator();
	View.Pitch = UDeepTuningSettings::Get().Sub.RoomStartPitchDeg;
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
	if (!HasFlashlight())
	{
		// No flashlight yet: the HUD says where to buy it.
		return;
	}
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
	Lamp->SetVisibility(ShowsLamp());
}

bool ADeepDiver::ShowsLamp() const
{
	// Aboard, the room light is enough and a torch at 40,000 blew a wall out to a white disc: the lamp is stowed in the
	// dry room and comes back on at the hatch. Only the light is hidden; the switch (bLampOn) is kept.
	return IsLampOn() && !(Run && Run->IsInside());
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
