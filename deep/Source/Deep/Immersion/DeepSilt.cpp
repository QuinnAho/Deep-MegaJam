#include "Immersion/DeepSilt.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Diver/DeepDiver.h"
#include "DynamicRHI.h"
#include "Misc/App.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Immersion/DeepImmersion.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "Story/DeepEventBus.h"
#include "World/DeepArena.h"
#include "World/DeepArenaRoot.h"
#include "World/DeepShapes.h"
#include "World/DeepSubmarine.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepSilt, Log, All);

namespace
{
	const FDeepSiltTuning& SiltTune() { return UDeepTuningSettings::Get().Immersion.Silt; }

	/** Basic shapes are 100 cm across: a sphere's scale for a radius. */
	double SiltSphereScale(float RadiusCm) { return FMath::Max(0.f, RadiusCm) / (0.5 * DeepShapes::ShapeSizeCm); }

	double SiltRound2(float V) { return FMath::RoundToDouble(double(V) * 100.0) / 100.0; }

	/** Hull probes: three down (bow, middle, stern; stern weighted: the thrusters' wash) and five out (sides, top, ends). */
	constexpr int32 SiltDownProbes = 3;
	constexpr int32 SiltOutProbes = 5;
	const float SiltDownAlong[SiltDownProbes] = { 0.6f, 0.f, -0.6f };
	const float SiltDownWeight[SiltDownProbes] = { 0.7f, 1.f, 1.3f };
}

ADeepSilt::ADeepSilt()
{
	PrimaryActorTick.bCanEverTick = true;
	// After the hull is placed for the frame (ADeepSubmarine::PlaceStill) and the root has moved.
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetReplicates(false);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Puffs = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Puffs"));
	Puffs->SetupAttachment(Root);
	Puffs->SetMobility(EComponentMobility::Movable);
	Puffs->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Puffs->SetCastShadow(false);
	Puffs->bAffectDynamicIndirectLighting = false;
	Puffs->bAffectDistanceFieldLighting = false;
	Puffs->bVisibleInRayTracing = false;
	Puffs->SetStaticMesh(DeepShapes::LoadMesh(DeepShapes::Sphere));
	Puffs->NumCustomDataFloats = 1;
}

ADeepSilt* ADeepSilt::Find(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ADeepSilt> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

ADeepSilt* ADeepSilt::Ensure(UWorld* World)
{
	if (ADeepSilt* Found = Find(World))
	{
		return Found;
	}
	if (!World)
	{
		return nullptr;
	}
	ADeepArenaRoot* Root = DeepArena::FindArenaRoot(World);
	const FTransform At = Root ? Root->GetActorTransform() : FTransform::Identity;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	ADeepSilt* Silt = World->SpawnActor<ADeepSilt>(ADeepSilt::StaticClass(), At, Params);
	if (Silt && Root)
	{
		// Hung from the arena root: the puffs stay where they were lifted while the root moves (the shaft's helm).
		Silt->AttachToActor(Root, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	return Silt;
}

void ADeepSilt::BeginPlay()
{
	Super::BeginPlay();
	const FDeepSiltTuning& S = SiltTune();
	SetMaterialPath(S.Material);
	SetLook(S.Albedo, S.Density, S.EdgePower);
	if (UDeepEventBus* Bus = UDeepEventBus::Get(this))
	{
		EventHandle = Bus->OnEvent.AddUObject(this, &ADeepSilt::HandleEvent);
	}
	Draw();
}

bool ADeepSilt::SetMaterialPath(const FString& Path)
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Base)
	{
		// Without the soft translucent material a sphere would draw as a grey ball: draw nothing instead.
		UE_LOG(LogDeepSilt, Warning, TEXT("Silt: no material at %s; puffs are simulated but not drawn"), *Path);
		PuffMaterial = nullptr;
		return false;
	}
	PuffMaterial = UMaterialInstanceDynamic::Create(Base, this);
	PuffMaterial->SetVectorParameterValue(TEXT("Albedo"), LookAlbedo);
	PuffMaterial->SetScalarParameterValue(TEXT("Density"), LookDensity);
	PuffMaterial->SetScalarParameterValue(TEXT("EdgePower"), LookEdgePower);
	Puffs->SetMaterial(0, PuffMaterial);
	return true;
}

void ADeepSilt::SetLook(const FLinearColor& Albedo, float Density, float EdgePower)
{
	LookAlbedo = Albedo;
	LookDensity = FMath::Max(0.f, Density);
	LookEdgePower = FMath::Max(0.1f, EdgePower);
	if (PuffMaterial)
	{
		PuffMaterial->SetVectorParameterValue(TEXT("Albedo"), LookAlbedo);
		PuffMaterial->SetScalarParameterValue(TEXT("Density"), LookDensity);
		PuffMaterial->SetScalarParameterValue(TEXT("EdgePower"), LookEdgePower);
	}
}

void ADeepSilt::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UDeepEventBus* Bus = UDeepEventBus::Get(this))
	{
		Bus->OnEvent.Remove(EventHandle);
	}
	Super::EndPlay(Reason);
}

void ADeepSilt::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	StepSilt(DeltaSeconds);
	if (BenchLeft > 0.f)
	{
		++BenchFrames;
		BenchFrameS += FApp::GetDeltaTime();
		BenchGpuMs += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
		BenchPuffs += Pool.Num();
		BenchLeft -= DeltaSeconds;
		if (BenchLeft <= 0.f && BenchFrames > 0)
		{
			UE_LOG(LogDeepSilt, Display, TEXT("DEEPSILT bench {\"frames\":%d,\"frame_ms\":%.3f,\"gpu_ms\":%.3f,\"puffs\":%.1f,\"hidden\":%s}"),
				BenchFrames, 1000.0 * BenchFrameS / BenchFrames, BenchGpuMs / BenchFrames, double(BenchPuffs) / BenchFrames,
				bDrawHidden ? TEXT("true") : TEXT("false"));
		}
	}
}

void ADeepSilt::StartBench(float Seconds)
{
	BenchLeft = FMath::Max(0.1f, Seconds);
	BenchFrames = 0;
	BenchFrameS = 0.0;
	BenchGpuMs = 0.0;
	BenchPuffs = 0;
}

bool ADeepSilt::Trace(const FVector& FromCm, const FVector& ToCm, FHitResult& Hit) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	// The rock only (chunks, shaft pieces, beds and caverns are world static): never the hull, a diver or the serpent.
	// Complex: the cave chunks have complex collision only (the drive lane found the same for its probes).
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DeepSilt), true, this);
	if (const ADeepSubmarine* Sub = DeepArena::FindSubmarine(World))
	{
		Params.AddIgnoredActor(Sub);
	}
	return World->LineTraceSingleByObjectType(Hit, FromCm, ToCm, FCollisionObjectQueryParams(ECC_WorldStatic), Params);
}

void ADeepSilt::StepSilt(float DeltaSeconds)
{
	const float Dt = FMath::Max(0.f, DeltaSeconds);
	if (!UDeepImmersion::IsOn())
	{
		if (Pool.Num() > 0)
		{
			Clear();
		}
		Draw();
		return;
	}
	Pool.Step(Dt);
	ProbeClock += Dt;
	const float Period = 1.f / FMath::Clamp(SiltTune().ProbeHz, 1.f, 60.f);
	if (ProbeClock >= Period)
	{
		// A long hitch is not a long probe: the hull did not lift a hitch's worth of silt in one place.
		const float ProbeDt = FMath::Min(ProbeClock >= 999.f ? Period : ProbeClock, 0.5f);
		ProbeClock = 0.f;
		ProbeHull(ProbeDt);
		ProbeDivers(ProbeDt);
	}
	KeepOffHull();
	Draw();
}

int32 ADeepSilt::AddPuffs(const FVector& SiltCm, const FVector& SiltNormal, int32 Count, float Scale, float JitterCm, const FVector& WashMps)
{
	const FDeepSiltTuning& S = SiltTune();
	const FVector Normal = SiltNormal.GetSafeNormal(1e-4, FVector::UpVector);
	int32 Added = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// Spread over the surface round the point (in its plane), so a lift is a bank of silt, not a column.
		const FVector Any = FVector(Rand.VRand());
		const FVector Along = (Any - Normal * FVector::DotProduct(Any, Normal)).GetSafeNormal();
		const FVector At = SiltCm + Along * (Rand.FRand() * FMath::Max(0.f, JitterCm));
		FDeepSiltPuff Puff = DeepImmersionRules::MakePuff(At, Normal, Scale, Rand, S);
		if (!WashMps.IsNearlyZero())
		{
			DeepImmersionRules::Wash(Puff, WashMps, S);
		}
		Pool.Add(Puff, S.MaxPuffs);
		++Added;
	}
	return Added;
}

int32 ADeepSilt::Burst(const FVector& WorldCm, const FVector& WorldNormal, int32 Count, float Scale)
{
	const FTransform Frame = SiltFrame();
	return AddPuffs(Frame.InverseTransformPosition(WorldCm), Frame.InverseTransformVector(WorldNormal), FMath::Max(0, Count), Scale, 80.f);
}

bool ADeepSilt::BurstUnderHull(int32 Count)
{
	const ADeepSubmarine* Sub = DeepArena::FindSubmarine(GetWorld());
	if (!Sub)
	{
		return false;
	}
	const FBox Box = Sub->GetSolidBoundsLocal(false);
	const FVector Centre = Sub->GetActorTransform().TransformPosition(Box.GetCenter());
	FHitResult Hit;
	if (!Trace(Centre, Centre - FVector(0.0, 0.0, 6000.0), Hit))
	{
		return false;
	}
	// Under the whole hull, bow to stern, as a run along the floor would leave it.
	const FVector Ahead = Sub->GetActorForwardVector();
	const double HalfLen = Box.GetExtent().X;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const double Along = FMath::Lerp(-HalfLen, HalfLen, (Index + 0.5) / FMath::Max(1, Count));
		Burst(Hit.ImpactPoint + Ahead * Along, Hit.ImpactNormal, 1, 1.f);
	}
	return true;
}

void ADeepSilt::Clear()
{
	Pool.Puffs.Reset();
	LiftCarry = 0.f;
}

void ADeepSilt::SetDrawHidden(bool bHide)
{
	bDrawHidden = bHide;
	Draw();
}

void ADeepSilt::ProbeHull(float ProbeDt)
{
	++Probes;
	const ADeepSubmarine* Sub = DeepArena::FindSubmarine(GetWorld());
	Contacts.Reset();
	HullClearanceM = -1.f;
	HullGapM = -1.f;
	if (!Sub)
	{
		bHasLastHull = false;
		return;
	}
	const FBox Box = Sub->GetSolidBoundsLocal(false);
	if (!Box.IsValid)
	{
		return;
	}
	const FDeepSiltTuning& S = SiltTune();
	const FTransform Hull = Sub->GetActorTransform();
	const FTransform Frame = SiltFrame();
	const FVector CentreLocal = Box.GetCenter();
	const FVector Ext = Box.GetExtent();
	const FVector CentreW = Hull.TransformPosition(CentreLocal);

	// The hull's speed through the water, from where it was in the silt's frame a probe ago.
	const FVector HullSilt = Frame.InverseTransformPosition(CentreW);
	HullSpeedMps = 0.f;
	HullVelocityMps = FVector::ZeroVector;
	if (bHasLastHull && ProbeDt > 0.f)
	{
		const float Mps = static_cast<float>(FVector::Dist(HullSilt, LastHullSiltCm) / ProbeDt / UDeepRules::CmPerM);
		// Faster than any drive is a jump (a cheat, a stop loading): no wash from it.
		HullSpeedMps = Mps < 25.f ? Mps : 0.f;
		HullVelocityMps = HullSpeedMps > 0.f ? (HullSilt - LastHullSiltCm) / ProbeDt / UDeepRules::CmPerM : FVector::ZeroVector;
	}
	LastHullSiltCm = HullSilt;
	bHasLastHull = true;

	auto Keep = [&](const FHitResult& Hit, float GapM)
	{
		FContact C;
		C.HitCm = Frame.InverseTransformPosition(Hit.ImpactPoint);
		C.Normal = Frame.InverseTransformVector(Hit.ImpactNormal).GetSafeNormal(1e-4, FVector::UpVector);
		C.GapM = GapM;
		Contacts.Add(C);
		HullGapM = HullGapM < 0.f ? GapM : FMath::Min(HullGapM, GapM);
		return Contacts.Num() - 1;
	};
	auto Scrape = [&](int32 Probe, int32 ContactIndex)
	{
		if (ContactIndex == INDEX_NONE || !DeepImmersionRules::IsScrape(Contacts[ContactIndex].GapM, S) || HullSpeedMps <= S.LiftMinMps)
		{
			// Ready at once for the next touch.
			ScrapeClocks[Probe] = S.ScrapeEveryS;
			return;
		}
		ScrapeClocks[Probe] += ProbeDt;
		if (ScrapeClocks[Probe] >= S.ScrapeEveryS)
		{
			ScrapeClocks[Probe] = 0.f;
			Scraped += AddPuffs(Contacts[ContactIndex].HitCm, Contacts[ContactIndex].Normal, 1, 1.f, 30.f);
		}
	};

	// Down: bow, middle and stern, from the box's centre straight down past the lift height.
	const double DownCm = Ext.Z + (S.LiftHeightM + 1.0) * UDeepRules::CmPerM;
	float LiftRate = 0.f;
	int32 LiftContact[SiltDownProbes] = { INDEX_NONE, INDEX_NONE, INDEX_NONE };
	for (int32 K = 0; K < SiltDownProbes; ++K)
	{
		const FVector From = Hull.TransformPosition(CentreLocal + FVector(SiltDownAlong[K] * Ext.X, 0.0, 0.0));
		FHitResult Hit;
		int32 ContactIndex = INDEX_NONE;
		if (Trace(From, From - FVector(0.0, 0.0, DownCm), Hit))
		{
			const float GapM = static_cast<float>((Hit.Distance - Ext.Z) / UDeepRules::CmPerM);
			ContactIndex = Keep(Hit, GapM);
			LiftContact[K] = ContactIndex;
			HullClearanceM = HullClearanceM < 0.f ? GapM : FMath::Min(HullClearanceM, GapM);
			LiftRate += DeepImmersionRules::LiftPuffsPerS(GapM, HullSpeedMps, S) * SiltDownWeight[K] / SiltDownProbes;
		}
		Scrape(K, ContactIndex);
	}
	// The lift: puffs at the down probes' floor points, spread across the hull's beam.
	LiftCarry += LiftRate * ProbeDt;
	int32 Turn = 0;
	while (LiftCarry >= 1.f)
	{
		LiftCarry -= 1.f;
		// Cycle the probes that found a floor, stern first (the wash).
		for (int32 Try = 0; Try < SiltDownProbes; ++Try)
		{
			const int32 K = (SiltDownProbes - 1 - (Turn + Try) % SiltDownProbes);
			if (LiftContact[K] != INDEX_NONE)
			{
				Lifted += AddPuffs(Contacts[LiftContact[K]].HitCm, Contacts[LiftContact[K]].Normal, 1, 1.f, static_cast<float>(Ext.Y * 0.9), HullVelocityMps);
				break;
			}
		}
		++Turn;
	}
	if (LiftRate <= 0.f)
	{
		LiftCarry = 0.f;
	}

	// Out: both sides, the top and both ends, a little past the scrape gap.
	const FVector Dirs[SiltOutProbes] = { FVector(0, 1, 0), FVector(0, -1, 0), FVector(0, 0, 1), FVector(1, 0, 0), FVector(-1, 0, 0) };
	const double Extents[SiltOutProbes] = { Ext.Y, Ext.Y, Ext.Z, Ext.X, Ext.X };
	for (int32 K = 0; K < SiltOutProbes; ++K)
	{
		const double ReachCm = Extents[K] + (S.ScrapeGapM + 0.5) * UDeepRules::CmPerM;
		const FVector Dir = Hull.TransformVectorNoScale(Dirs[K]);
		FHitResult Hit;
		int32 ContactIndex = INDEX_NONE;
		if (Trace(CentreW, CentreW + Dir * ReachCm, Hit))
		{
			ContactIndex = Keep(Hit, static_cast<float>((Hit.Distance - Extents[K]) / UDeepRules::CmPerM));
		}
		Scrape(SiltDownProbes + K, ContactIndex);
	}
}

void ADeepSilt::ProbeDivers(float ProbeDt)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FDeepSiltTuning& S = SiltTune();
	const FTransform Frame = SiltFrame();
	for (TActorIterator<ADeepDiver> It(World); It; ++It)
	{
		ADeepDiver* Diver = *It;
		const UCharacterMovementComponent* Move = IsValid(Diver) ? Diver->GetCharacterMovement() : nullptr;
		// Swimming only (MOVE_Flying in the water); aboard the diver walks in the room set, far from any silt.
		if (!Move || Move->MovementMode != MOVE_Flying)
		{
			continue;
		}
		float& Clock = DiverClocks.FindOrAdd(Diver);
		Clock += ProbeDt;
		const float SpeedMps = static_cast<float>(Diver->GetVelocity().Size() / UDeepRules::CmPerM);
		if (SpeedMps < S.DiverMinMps)
		{
			continue;
		}
		const double HalfHeight = Diver->GetSimpleCollisionHalfHeight();
		const FVector From = Diver->GetActorLocation();
		FHitResult Hit;
		if (!Trace(From, From - FVector(0.0, 0.0, HalfHeight + (S.DiverLiftHeightM + 0.5) * UDeepRules::CmPerM), Hit))
		{
			continue;
		}
		const float ClearM = static_cast<float>((Hit.Distance - HalfHeight) / UDeepRules::CmPerM);
		if (DeepImmersionRules::DiverLifts(ClearM, SpeedMps, S) && Clock >= S.DiverEveryS)
		{
			Clock = 0.f;
			Kicked += AddPuffs(Frame.InverseTransformPosition(Hit.ImpactPoint), Frame.InverseTransformVector(Hit.ImpactNormal), 1, S.DiverPuffScale, 25.f);
		}
	}
	for (auto It = DiverClocks.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void ADeepSilt::HandleEvent(FName Event, const FDeepEventPayload& Payload)
{
	if (Event != FName(DeepEvent::HullHit) || !UDeepImmersion::IsOn())
	{
		return;
	}
	const FDeepSiltTuning& S = SiltTune();
	const int32 Count = DeepImmersionRules::KnockPuffs(Payload.ImpactMps, S);
	if (Count <= 0)
	{
		return;
	}
	auto Nearest = [this]() -> int32
	{
		int32 Best = INDEX_NONE;
		for (int32 I = 0; I < Contacts.Num(); ++I)
		{
			if (Best == INDEX_NONE || Contacts[I].GapM < Contacts[Best].GapM)
			{
				Best = I;
			}
		}
		return Best;
	};
	int32 Best = Nearest();
	if (Best == INDEX_NONE || Contacts[Best].GapM > 4.f * S.ScrapeGapM + 0.5f)
	{
		// The knock came between probes: look now.
		ProbeHull(0.f);
		Best = Nearest();
	}
	if (Best == INDEX_NONE)
	{
		return;
	}
	Knocked += AddPuffs(Contacts[Best].HitCm, Contacts[Best].Normal, Count, 1.15f, 90.f);
}

void ADeepSilt::KeepOffHull()
{
	const ADeepSubmarine* Sub = DeepArena::FindSubmarine(GetWorld());
	if (!Sub || Pool.Num() == 0)
	{
		return;
	}
	const FBox Box = Sub->GetSolidBoundsLocal(false);
	if (!Box.IsValid)
	{
		return;
	}
	const FDeepSiltTuning& S = SiltTune();
	const FTransform Hull = Sub->GetActorTransform();
	const FTransform Frame = SiltFrame();
	for (FDeepSiltPuff& Puff : Pool.Puffs)
	{
		// Its own size this step, before any squeeze: the hull may have moved off it.
		Puff.FitRadiusCm = -1.f;
		const FDeepSiltPuffNow Now = DeepImmersionRules::PuffAt(Puff, S);
		const FVector Local = Hull.InverseTransformPosition(Frame.TransformPosition(Now.LocationCm));
		const FVector SurfaceLocal = Hull.InverseTransformPosition(Frame.TransformPosition(Puff.SurfaceCm));
		const FVector NormalLocal = Hull.InverseTransformVectorNoScale(Frame.TransformVectorNoScale(Puff.Normal));
		const float MinAbove = static_cast<float>(FVector::DotProduct(Puff.BirthCm - Puff.SurfaceCm, Puff.Normal));
		const FVector Kept = DeepImmersionRules::KeepOffBox(Local, Now.RadiusCm, S.HullKeepShare, Box, SurfaceLocal, NormalLocal, MinAbove);
		if (!Kept.Equals(Local, 0.01))
		{
			Puff.PushCm += Frame.InverseTransformVector(Hull.TransformVector(Kept - Local));
		}
		// Between the hull and the rock with nowhere to go: squeezed thin instead of reaching into the hull.
		if (DeepImmersionRules::TouchesBox(Kept, Now.RadiusCm * S.HullKeepShare, Box))
		{
			Puff.FitRadiusCm = DeepImmersionRules::FitRadiusCm(Kept, S.HullKeepShare, Box);
		}
	}
}

bool ADeepSilt::AnyPuffInHull() const
{
	const ADeepSubmarine* Sub = DeepArena::FindSubmarine(GetWorld());
	if (!Sub)
	{
		return false;
	}
	const FBox Box = Sub->GetSolidBoundsLocal(false);
	const FTransform Hull = Sub->GetActorTransform();
	const float Share = SiltTune().HullKeepShare;
	for (int32 I = 0; I < Pool.Num(); ++I)
	{
		const FDeepSiltPuffNow Now = PuffNowWorld(I);
		// A centimetre of slack for the float round trip through the frames.
		if (DeepImmersionRules::TouchesBox(Hull.InverseTransformPosition(Now.LocationCm), Now.RadiusCm * Share - 1.f, Box))
		{
			return true;
		}
	}
	return false;
}

FDeepSiltPuffNow ADeepSilt::PuffNowWorld(int32 Index) const
{
	FDeepSiltPuffNow Now;
	if (!Pool.Puffs.IsValidIndex(Index))
	{
		return Now;
	}
	Now = DeepImmersionRules::PuffAt(Pool.Puffs[Index], SiltTune());
	Now.LocationCm = SiltFrame().TransformPosition(Now.LocationCm);
	return Now;
}

void ADeepSilt::Draw()
{
	const int32 N = Pool.Num();
	const bool bShow = !bDrawHidden && PuffMaterial && N > 0 && UDeepImmersion::IsOn();
	if (!bShow)
	{
		if (Puffs->GetInstanceCount() > 0)
		{
			Puffs->ClearInstances();
		}
		Puffs->SetVisibility(false);
		return;
	}
	const FDeepSiltTuning& S = SiltTune();
	Xforms.SetNum(N);
	Opacity.SetNum(N);
	for (int32 I = 0; I < N; ++I)
	{
		const FDeepSiltPuffNow Now = DeepImmersionRules::PuffAt(Pool.Puffs[I], S);
		// Instances are in the actor's space, which is the silt frame.
		Xforms[I] = FTransform(FQuat::Identity, Now.LocationCm, FVector(SiltSphereScale(Now.RadiusCm)));
		Opacity[I] = Now.Opacity01;
	}
	if (Puffs->GetInstanceCount() != N)
	{
		Puffs->ClearInstances();
		Puffs->AddInstances(Xforms, false, false, false);
	}
	else
	{
		Puffs->BatchUpdateInstancesTransforms(0, Xforms, false, true, true);
	}
	Puffs->SetCustomData(0, N - 1, Opacity, true);
	Puffs->SetVisibility(true);
}

TSharedRef<FJsonObject> ADeepSilt::BuildStateJson() const
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("puffs"), Pool.Num());
	Json->SetNumberField(TEXT("max"), SiltTune().MaxPuffs);
	Json->SetNumberField(TEXT("lifted"), Lifted);
	Json->SetNumberField(TEXT("scraped"), Scraped);
	Json->SetNumberField(TEXT("knocked"), Knocked);
	Json->SetNumberField(TEXT("kicked"), Kicked);
	Json->SetNumberField(TEXT("probes"), Probes);
	Json->SetNumberField(TEXT("clearance_m"), SiltRound2(HullClearanceM));
	Json->SetNumberField(TEXT("gap_m"), SiltRound2(HullGapM));
	Json->SetNumberField(TEXT("speed_mps"), SiltRound2(HullSpeedMps));
	Json->SetBoolField(TEXT("material"), PuffMaterial != nullptr);
	Json->SetBoolField(TEXT("hidden"), bDrawHidden);
	Json->SetBoolField(TEXT("in_hull"), AnyPuffInHull());
	return Json;
}
