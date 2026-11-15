#include "Monster/DeepSerpent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Diver/DeepDiver.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Monster/DeepMonsterRules.h"
#include "Monster/DeepMonsterSettings.h"
#include "Monster/DeepSerpentController.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"
#include "World/DeepShapes.h"

namespace
{
	using DeepMonster::CmPerM;
	constexpr float TrailStepCm = 10.f;
	const TCHAR* EyeMaterialPath = TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial");
	const TCHAR* BasicMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	/** The code-built head spans this fraction of HeadLengthM behind the head centre; the rest is in front. */
	constexpr float HeadBackFraction = 0.4f;
	/** The tube starts this far inside the back of the head, so the seam is hidden. */
	constexpr float NeckOverlapM = 0.18f;
	/** The first metres of the neck bend toward the head socket (lunge, flinch, sway) instead of the trail. */
	constexpr float NeckBlendM = 2.2f;
	/** The markings layer sits this far out along the normal. */
	constexpr float MarksLiftCm = 0.6f;

	float HeadBackM(const FDeepSerpentBodyTuning& Body)
	{
		return Body.bTubeBody ? Body.HeadLengthM * HeadBackFraction : Body.HeadRadiusM * 1.2f;
	}

	float HeadFrontM(const FDeepSerpentBodyTuning& Body)
	{
		return Body.bTubeBody ? Body.HeadLengthM * (1.f - HeadBackFraction) : Body.HeadRadiusM * 0.3f;
	}

	float BodyLengthM(const FDeepSerpentBodyTuning& Body)
	{
		return FMath::Max(0.5f, Body.LengthM - HeadBackM(Body) - HeadFrontM(Body));
	}

	/** Body radius at T (0 = neck, 1 = tail tip): a swell behind the head, then the taper. */
	float RadiusAt(const FDeepSerpentBodyTuning& Body, float T)
	{
		const float Peak = Body.NeckRadiusM * Body.BellySwell;
		constexpr float SwellEnd = 0.18f;
		if (T < SwellEnd)
		{
			return FMath::Lerp(Body.NeckRadiusM, Peak, FMath::SmoothStep(0.f, SwellEnd, T));
		}
		const float U = (T - SwellEnd) / (1.f - SwellEnd);
		return FMath::Lerp(Peak, Body.TailRadiusM, FMath::Pow(U, FMath::Max(0.1f, Body.TaperPower)));
	}

	float SignedPow(float V, float E)
	{
		return FMath::Sign(V) * FMath::Pow(FMath::Abs(V), E);
	}

	float SerpentEaseOutCubic(float X)
	{
		const float C = 1.f - FMath::Clamp(X, 0.f, 1.f);
		return 1.f - C * C * C;
	}

	/** A grid of rings (Sides + 1 vertices each: the seam is doubled for the UVs), outward-facing triangles. */
	void RingGridTriangles(int32 Rings, int32 Sides, int32 Base, TArray<int32>& Out)
	{
		const int32 Row = Sides + 1;
		for (int32 i = 0; i + 1 < Rings; ++i)
		{
			for (int32 j = 0; j < Sides; ++j)
			{
				const int32 A = Base + i * Row + j;
				const int32 B = A + Row;
				const int32 C = A + 1;
				const int32 D = B + 1;
				// Front faces outward (checked by frame: the other order showed the inside of the tube, unlit).
				Out.Append({ A, B, C, C, B, D });
			}
		}
	}

	/** Smooth normals and along-the-body tangents for a ring grid, from neighbouring positions. */
	void RingGridNormals(const TArray<FVector>& V, int32 Rings, int32 Sides, TArray<FVector>& OutN, TArray<FVector>& OutT)
	{
		const int32 Row = Sides + 1;
		OutN.SetNumUninitialized(Rings * Row);
		OutT.SetNumUninitialized(Rings * Row);
		for (int32 i = 0; i < Rings; ++i)
		{
			const int32 Prev = FMath::Max(0, i - 1);
			const int32 Next = FMath::Min(Rings - 1, i + 1);
			for (int32 j = 0; j <= Sides; ++j)
			{
				const int32 jj = j % Sides;
				const int32 L = (jj + Sides - 1) % Sides;
				const int32 R = (jj + 1) % Sides;
				const FVector Around = V[i * Row + R] - V[i * Row + L];
				FVector Along = V[Next * Row + jj] - V[Prev * Row + jj];
				if (!Along.Normalize())
				{
					Along = FVector::ForwardVector;
				}
				FVector N = FVector::CrossProduct(Around, Along);
				if (!N.Normalize())
				{
					N = FVector::UpVector;
				}
				OutN[i * Row + j] = N;
				OutT[i * Row + j] = Along;
			}
		}
	}

	void ToProcTangents(const TArray<FVector>& In, TArray<FProcMeshTangent>& Out)
	{
		Out.SetNumUninitialized(In.Num());
		for (int32 i = 0; i < In.Num(); ++i)
		{
			Out[i] = FProcMeshTangent(In[i], false);
		}
	}

	/** One cross-section ring: superellipse, separate heights above and below, an optional dorsal ridge. */
	struct FRingShape
	{
		float HalfWidth = 0.4f;
		float Top = 0.4f;
		float Bottom = 0.4f;
		float Exponent = 1.f;
		float Ridge = 0.f;
		/** Raised ridges either side of the top (brows over the eyes), metres. */
		float Brow = 0.f;
	};

	FVector RingPoint(const FRingShape& S, int32 j, int32 Sides)
	{
		const float A = 2.f * PI * j / Sides;
		const float C = FMath::Cos(A);
		const float Sn = FMath::Sin(A);
		float Y = S.HalfWidth * SignedPow(C, S.Exponent);
		float Z = (Sn >= 0.f ? S.Top : S.Bottom) * SignedPow(Sn, S.Exponent);
		const int32 Quarter = Sides / 4;
		const int32 jj = j % Sides;
		if (jj == Quarter)
		{
			Z += S.Ridge * S.Top;
		}
		else if (jj == Quarter - 1 || jj == Quarter + 1)
		{
			Z += S.Ridge * S.Top * 0.25f;
		}
		if (S.Brow > 0.f && (jj == Quarter - 2 || jj == Quarter + 2))
		{
			Z += S.Brow;
		}
		return FVector(0.f, Y, Z);
	}

	/** Lofts a static shell along local +X (back to front) into Out arrays, in the socket's space. */
	template <typename FShapeFn>
	void LoftShell(int32 Sections, int32 Sides, float XBack, float XFront, float TilesPerM, float TilesAround, FShapeFn&& ShapeAt,
		TArray<FVector>& V, TArray<FVector2D>& UV0, TArray<FVector2D>& UV1, TArray<FLinearColor>& C, TArray<int32>& Tri)
	{
		for (int32 i = 0; i < Sections; ++i)
		{
			const float S = i / static_cast<float>(Sections - 1);
			const float X = FMath::Lerp(XBack, XFront, S);
			const FRingShape Shape = ShapeAt(S);
			// U runs toward the tail, so it falls toward the snout (the tube starts at 0 behind the head).
			const float U = -(X - XBack) / CmPerM;
			for (int32 j = 0; j <= Sides; ++j)
			{
				V.Add(FVector(X, 0.f, 0.f) + RingPoint(Shape, j, Sides) * CmPerM);
				const float Around = j / static_cast<float>(Sides);
				UV1.Add(FVector2D(U, Around));
				UV0.Add(FVector2D(U * TilesPerM, Around * TilesAround));
				C.Add(FLinearColor(0.5f * (Shape.HalfWidth + Shape.Top), 0.f, 0.f, 1.f));
			}
		}
		RingGridTriangles(Sections, Sides, 0, Tri);
		// Close the snout with a fan to its centre.
		const int32 Tip = V.Add(FVector(XFront + 0.5f, 0.f, 0.f));
		UV1.Add(FVector2D(-(XFront - XBack) / CmPerM, 0.25f));
		UV0.Add(UV1.Last() * FVector2D(TilesPerM, TilesAround));
		C.Add(FLinearColor(0.05f, 0.f, 0.f, 1.f));
		const int32 Last = (Sections - 1) * (Sides + 1);
		for (int32 j = 0; j < Sides; ++j)
		{
			Tri.Append({ Last + j, Tip, Last + j + 1 });
		}
	}

	void MakeShell(UProceduralMeshComponent* Mesh, TArray<FVector>& V, const TArray<FVector2D>& UV0, const TArray<FVector2D>& UV1,
		const TArray<FLinearColor>& C, const TArray<int32>& Tri, int32 Sections, int32 Sides)
	{
		TArray<FVector> N, T;
		RingGridNormals(V, Sections, Sides, N, T);
		// The snout tip vertex.
		N.Add(FVector::ForwardVector);
		T.Add(FVector::RightVector);
		TArray<FProcMeshTangent> PT;
		ToProcTangents(T, PT);
		const TArray<FVector2D> None;
		Mesh->CreateMeshSection_LinearColor(0, V, Tri, N, UV0, UV1, None, None, C, PT, false, false);
		TArray<FVector> Lifted = V;
		for (int32 i = 0; i < Lifted.Num(); ++i)
		{
			Lifted[i] += N[i] * MarksLiftCm;
		}
		Mesh->CreateMeshSection_LinearColor(1, Lifted, Tri, N, UV0, UV1, None, None, C, PT, false, false);
	}

	/** One tooth: a three-sided spike from Base along Dir, flat-shaded, drawn from both sides (so winding cannot hide it). */
	void AddTooth(const FVector& Base, const FVector& Dir, float LenCm, float RadCm, TArray<FVector>& V, TArray<FVector>& N,
		TArray<FProcMeshTangent>& T, TArray<FVector2D>& UV, TArray<FLinearColor>& C, TArray<int32>& Tri)
	{
		const FVector Axis = Dir.GetSafeNormal(UE_SMALL_NUMBER, FVector::DownVector);
		FVector A, B;
		Axis.FindBestAxisVectors(A, B);
		const FVector Tip = Base + Axis * LenCm;
		FVector P[3];
		for (int32 k = 0; k < 3; ++k)
		{
			const float Ang = 2.f * PI * k / 3.f;
			P[k] = Base + (A * FMath::Cos(Ang) + B * FMath::Sin(Ang)) * RadCm;
		}
		for (int32 k = 0; k < 3; ++k)
		{
			const FVector& P0 = P[k];
			const FVector& P1 = P[(k + 1) % 3];
			FVector Normal = FVector::CrossProduct(P1 - P0, Tip - P0).GetSafeNormal();
			if (FVector::DotProduct(Normal, (P0 + P1) * 0.5f - Base) < 0.f)
			{
				Normal = -Normal;
			}
			const int32 I = V.Num();
			for (const FVector& Q : { P0, P1, Tip })
			{
				V.Add(Q);
				N.Add(Normal);
				T.Add(FProcMeshTangent(Axis, false));
				UV.Add(FVector2D::ZeroVector);
				C.Add(FLinearColor(0.05f, 0.f, 0.f, 1.f));
			}
			Tri.Append({ I, I + 1, I + 2, I, I + 2, I + 1 });
		}
	}

	void QuietMesh(UPrimitiveComponent* Mesh)
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
	}
}

ADeepSerpent::ADeepSerpent()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);

	// Unreal game AI: our controller possesses it as soon as it is spawned. It steers itself (SwimToward),
	// so the controller's rotation must not override the heading.
	AIControllerClass = ADeepSerpentController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	HeadSocket = CreateDefaultSubobject<USceneComponent>(TEXT("HeadSocket"));
	HeadSocket->SetupAttachment(Root);

	HeadShell = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HeadShell"));
	HeadShell->SetupAttachment(HeadSocket);
	QuietMesh(HeadShell);

	JawHinge = CreateDefaultSubobject<USceneComponent>(TEXT("JawHinge"));
	JawHinge->SetupAttachment(HeadSocket);
	JawShell = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("JawShell"));
	JawShell->SetupAttachment(JawHinge);
	QuietMesh(JawShell);

	HeadMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMeshComp->SetupAttachment(HeadSocket);
	QuietMesh(HeadMeshComp);

	Tube = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Tube"));
	Tube->SetupAttachment(Root);
	QuietMesh(Tube);
	Tube->SetUsingAbsoluteLocation(true);
	Tube->SetUsingAbsoluteRotation(true);
	Tube->SetUsingAbsoluteScale(true);

	Head = DeepShapes::CreateShape(this, TEXT("Head"), DeepShapes::Sphere);
	Head->SetupAttachment(Root);

	Jaw = DeepShapes::CreateShape(this, TEXT("Jaw"), DeepShapes::Cone);
	Jaw->SetupAttachment(Root);

	EyeLeft = DeepShapes::CreateShape(this, TEXT("EyeLeft"), DeepShapes::Sphere);
	EyeLeft->SetupAttachment(HeadSocket);
	EyeRight = DeepShapes::CreateShape(this, TEXT("EyeRight"), DeepShapes::Sphere);
	EyeRight->SetupAttachment(HeadSocket);
	for (UStaticMeshComponent* Eye : { EyeLeft.Get(), EyeRight.Get() })
	{
		Eye->SetCastShadow(false);
		QuietMesh(Eye);
	}

	EyeLightLeft = CreateDefaultSubobject<UPointLightComponent>(TEXT("EyeLightLeft"));
	EyeLightLeft->SetupAttachment(HeadSocket);
	EyeLightRight = CreateDefaultSubobject<UPointLightComponent>(TEXT("EyeLightRight"));
	EyeLightRight->SetupAttachment(HeadSocket);
	for (UPointLightComponent* Light : { EyeLightLeft.Get(), EyeLightRight.Get() })
	{
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetCastShadows(false);
		Light->SetIntensityUnits(ELightUnits::Unitless);
		Light->SetIntensity(0.f);
	}

	Body = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetStaticMesh(DeepShapes::LoadMesh(DeepShapes::Sphere));
	QuietMesh(Body);
	Body->SetMobility(EComponentMobility::Movable);
	// Instances are given in world space; keep the component at the origin.
	Body->SetUsingAbsoluteLocation(true);
	Body->SetUsingAbsoluteRotation(true);
	Body->SetUsingAbsoluteScale(true);
}

void ADeepSerpent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADeepSerpent, MoodState);
	DOREPLIFETIME(ADeepSerpent, MoodThreat);
}

void ADeepSerpent::SetMood(EDeepMonsterState InState, float InThreat01)
{
	MoodState = static_cast<uint8>(InState);
	MoodThreat = FMath::Clamp(InThreat01, 0.f, 1.f);
}

void ADeepSerpent::BeginPlay()
{
	Super::BeginPlay();
	SetupLook();
	Heading = GetActorForwardVector();
	LastHeading = Heading;
	PlaceHead(GetActorLocation(), Heading);
}

void ADeepSerpent::SetupLook()
{
	const FDeepSerpentBodyTuning& Tuning = UDeepMonsterSettings::Get().Body;
	bTube = Tuning.bTubeBody;

	// ---- Materials: the skin (body slot) and the markings layer.
	UMaterialInterface* SkinBase = Cast<UMaterialInterface>(Tuning.SkinMaterial.TryLoad());
	if (SkinBase)
	{
		SkinMID = UMaterialInstanceDynamic::Create(SkinBase, this);
	}
	else if (UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, BasicMaterialPath))
	{
		SkinMID = UMaterialInstanceDynamic::Create(Basic, this);
		SkinMID->SetVectorParameterValue(TEXT("Color"), Tuning.SkinColor);
	}
	if (SkinMID)
	{
		SkinMID->SetVectorParameterValue(TEXT("SkinColor"), Tuning.SkinColor);
	}
	if (SkinBase)
	{
		// Teeth: the same wet material, bone-coloured, no scales (they read UV 0).
		TeethMID = UMaterialInstanceDynamic::Create(SkinBase, this);
		TeethMID->SetVectorParameterValue(TEXT("SkinColor"), Tuning.TeethColor);
		TeethMID->SetScalarParameterValue(TEXT("ScaleDepth"), 0.f);
		TeethMID->SetScalarParameterValue(TEXT("GrooveRoughness"), 0.35f);
		// The jaw (the mouth's floor faces the lamp in a strike) is darker still, so the open mouth reads as a hole.
		MouthMID = UMaterialInstanceDynamic::Create(SkinBase, this);
		MouthMID->SetVectorParameterValue(TEXT("SkinColor"), Tuning.SkinColor * 0.25f);
		MouthMID->SetScalarParameterValue(TEXT("WetCoat"), 0.3f);
	}
	if (UMaterialInterface* MarksBase = Cast<UMaterialInterface>(Tuning.MarksMaterial.TryLoad()))
	{
		MarksMID = UMaterialInstanceDynamic::Create(MarksBase, this);
		MarksMID->SetVectorParameterValue(TEXT("MarkColor"), Tuning.MarkColor);
		MarksMID->SetScalarParameterValue(TEXT("SpotGain"), 0.f);
		MarksMID->SetScalarParameterValue(TEXT("RimGain"), 0.f);
		MarksMID->SetScalarParameterValue(TEXT("SpotSize"), Tuning.SpotSizeM);
		MarksMID->SetScalarParameterValue(TEXT("SpotSpacing"), Tuning.SpotSpacingM);
	}

	// ---- Eyes: the engine's unlit emissive material; dark until a lamp is on them.
	const FVector EyeOffsetCm = Tuning.EyeOffsetM * CmPerM;
	EyeLeft->SetRelativeLocation(FVector(EyeOffsetCm.X, -EyeOffsetCm.Y, EyeOffsetCm.Z));
	EyeRight->SetRelativeLocation(EyeOffsetCm);
	for (UStaticMeshComponent* Eye : { EyeLeft.Get(), EyeRight.Get() })
	{
		Eye->SetRelativeScale3D(FVector(Tuning.EyeRadiusM * 2.f));
	}
	const FVector LightForward(Tuning.EyeRadiusM * CmPerM * 2.f, 0.f, 0.f);
	EyeLightLeft->SetRelativeLocation(EyeLeft->GetRelativeLocation() + LightForward);
	EyeLightRight->SetRelativeLocation(EyeRight->GetRelativeLocation() + LightForward);
	for (UPointLightComponent* Light : { EyeLightLeft.Get(), EyeLightRight.Get() })
	{
		Light->SetLightColor(Tuning.EyeColor);
		Light->SetAttenuationRadius(Tuning.EyeLightRadiusM * CmPerM);
		Light->SetIntensity(0.f);
	}
	EyeMIDs.Reset();
	if (UMaterialInterface* EyeBase = LoadObject<UMaterialInterface>(nullptr, EyeMaterialPath))
	{
		for (UStaticMeshComponent* Eye : { EyeLeft.Get(), EyeRight.Get() })
		{
			UMaterialInstanceDynamic* Glow = UMaterialInstanceDynamic::Create(EyeBase, Eye);
			Glow->SetVectorParameterValue(TEXT("Color"), FLinearColor::Black);
			Eye->SetMaterial(0, Glow);
			EyeMIDs.Add(Glow);
		}
	}

	// ---- Head: a mesh at the socket if one is set, else the code-built wedge and jaw.
	UStaticMesh* MeshHead = Tuning.HeadMesh.IsValid() ? Cast<UStaticMesh>(Tuning.HeadMesh.TryLoad()) : nullptr;
	bMeshHead = bTube && MeshHead != nullptr;
	if (bMeshHead)
	{
		HeadMeshComp->SetStaticMesh(MeshHead);
		HeadMeshComp->SetRelativeTransform(FTransform(Tuning.HeadMeshRotation, Tuning.HeadMeshOffsetCm, Tuning.HeadMeshScale));
		if (UMaterialInterface* HeadMaterial = Cast<UMaterialInterface>(Tuning.HeadMeshMaterial.TryLoad()))
		{
			for (int32 Slot = 0; Slot < HeadMeshComp->GetNumMaterials(); ++Slot)
			{
				HeadMeshComp->SetMaterial(Slot, HeadMaterial);
			}
		}
	}
	HeadMeshComp->SetVisibility(bMeshHead);
	HeadShell->SetVisibility(bTube && !bMeshHead);
	JawShell->SetVisibility(bTube && !bMeshHead);
	Tube->SetVisibility(bTube);
	Body->SetVisibility(!bTube);
	Head->SetVisibility(!bTube);
	Jaw->SetVisibility(!bTube);
	if (bTube && !bMeshHead)
	{
		BuildHeadShells();
	}

	// ---- The sphere fallback.
	const float R = Tuning.HeadRadiusM;
	Head->SetRelativeScale3D(FVector(R * 3.f, R * 2.f, R * 1.6f));
	Jaw->SetRelativeLocation(FVector(R * CmPerM * 0.9f, 0.f, -R * CmPerM * 0.35f));
	Jaw->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	Jaw->SetRelativeScale3D(FVector(R * 1.3f, R * 1.3f, R * 1.4f));
	if (SkinMID)
	{
		Head->SetMaterial(0, SkinMID);
		Jaw->SetMaterial(0, SkinMID);
		Body->SetMaterial(0, SkinMID);
	}
}

void ADeepSerpent::BuildHeadShells()
{
	const FDeepSerpentBodyTuning& Tuning = UDeepMonsterSettings::Get().Body;
	const int32 Sides = FMath::Max(8, Tuning.TubeSides / 4 * 4) + 4;
	constexpr int32 Sections = 14;
	const float L = Tuning.HeadLengthM;
	const float XBack = -L * HeadBackFraction * CmPerM;
	const float XFront = L * (1.f - HeadBackFraction) * CmPerM;
	const float Neck = Tuning.NeckRadiusM * 1.06f;
	const float W = Tuning.HeadWidthM * 0.5f;
	const float H = Tuning.HeadHeightM;

	// The skull: round where it meets the neck, wide over the jaw muscles, a flat wedge to a narrow snout.
	auto Skull = [&](float S)
	{
		FRingShape Shape;
		constexpr float Wide = 0.35f;
		if (S < Wide)
		{
			const float K = FMath::SmoothStep(0.f, Wide, S);
			Shape.HalfWidth = FMath::Lerp(Neck, W, K);
			Shape.Top = FMath::Lerp(Neck, H, K);
			Shape.Bottom = FMath::Lerp(Neck, 0.1f, K);
		}
		else
		{
			const float K = FMath::Pow((S - Wide) / (1.f - Wide), 1.25f);
			Shape.HalfWidth = FMath::Lerp(W, 0.1f, K);
			Shape.Top = FMath::Lerp(H, 0.07f, K);
			Shape.Bottom = FMath::Lerp(0.1f, 0.03f, K);
		}
		Shape.Exponent = FMath::Lerp(1.f, 0.75f, FMath::SmoothStep(0.f, Wide, S));
		Shape.Ridge = FMath::Lerp(Tuning.RidgeHeight, 0.08f, S);
		// Brows over the eyes.
		Shape.Brow = 0.06f * FMath::Max(0.f, 1.f - FMath::Abs(S - 0.68f) / 0.14f);
		return Shape;
	};
	TArray<FVector> V;
	TArray<FVector2D> UV0, UV1;
	TArray<FLinearColor> C;
	TArray<int32> Tri;
	LoftShell(Sections, Sides, XBack, XFront, Tuning.SkinTilesPerM, Tuning.SkinTilesAround, Skull, V, UV0, UV1, C, Tri);
	MakeShell(HeadShell, V, UV0, UV1, C, Tri, Sections, Sides);

	// The jaw hangs from a hinge low at the back of the skull; it is flat on top and deep at the chin muscles.
	const float HingeX = XBack + L * 0.25f * CmPerM;
	const float HingeZ = -0.06f * CmPerM;
	JawHinge->SetRelativeLocation(FVector(HingeX, 0.f, HingeZ));
	auto JawShape = [&](float S)
	{
		FRingShape Shape;
		const float K = FMath::Pow(S, 1.2f);
		Shape.HalfWidth = FMath::Lerp(W * 0.82f, 0.08f, K);
		Shape.Top = 0.03f;
		Shape.Bottom = FMath::Lerp(0.26f, 0.05f, FMath::SmoothStep(0.15f, 1.f, S));
		Shape.Exponent = 0.8f;
		return Shape;
	};
	V.Reset(); UV0.Reset(); UV1.Reset(); C.Reset(); Tri.Reset();
	LoftShell(Sections, Sides, -0.15f * L * CmPerM, XFront - HingeX - 4.f, Tuning.SkinTilesPerM, Tuning.SkinTilesAround, JawShape, V, UV0, UV1, C, Tri);
	MakeShell(JawShell, V, UV0, UV1, C, Tri, Sections, Sides);

	// Teeth: a row along each side of the upper lip pointing down, and of the jaw pointing up, interleaved; longer at
	// the front. They are what makes the open mouth read in silhouette and in the beam.
	const int32 Teeth = FMath::Max(0, Tuning.TeethPerSide);
	if (Teeth > 0)
	{
		const float JawBack = -0.15f * L * CmPerM;
		const float JawFront = XFront - HingeX - 4.f;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			const bool bUpper = Pass == 0;
			TArray<FVector> TV, TN;
			TArray<FProcMeshTangent> TT;
			TArray<FVector2D> TUV;
			TArray<FLinearColor> TC;
			TArray<int32> TTri;
			for (int32 i = 0; i < Teeth; ++i)
			{
				const float Frac = (i + (bUpper ? 0.f : 0.5f)) / static_cast<float>(Teeth);
				const float LenM = FMath::Lerp(0.05f, 0.1f, Frac) * (i == Teeth - 1 ? 1.5f : 1.f);
				for (const float SideSign : { -1.f, 1.f })
				{
					FVector Base, Dir;
					if (bUpper)
					{
						const float S = FMath::Lerp(0.42f, 0.96f, Frac);
						const FRingShape Shape = Skull(S);
						Base = FVector(FMath::Lerp(XBack, XFront, S), SideSign * Shape.HalfWidth * 0.8f * CmPerM, -Shape.Bottom * 0.5f * CmPerM);
						Dir = FVector(-0.15f, -SideSign * 0.2f, -1.f);
					}
					else
					{
						const float S = FMath::Lerp(0.3f, 0.95f, Frac);
						const FRingShape Shape = JawShape(S);
						Base = FVector(FMath::Lerp(JawBack, JawFront, S), SideSign * Shape.HalfWidth * 0.8f * CmPerM, Shape.Top * CmPerM);
						Dir = FVector(-0.15f, -SideSign * 0.2f, 1.f);
					}
					AddTooth(Base, Dir, LenM * CmPerM, LenM * 0.22f * CmPerM, TV, TN, TT, TUV, TC, TTri);
				}
			}
			UProceduralMeshComponent* Mesh = bUpper ? HeadShell.Get() : JawShell.Get();
			const TArray<FVector2D> None;
			Mesh->CreateMeshSection_LinearColor(2, TV, TTri, TN, TUV, TUV, None, None, TC, TT, false, false);
			if (TeethMID)
			{
				Mesh->SetMaterial(2, TeethMID);
			}
		}
	}

	for (UProceduralMeshComponent* Mesh : { HeadShell.Get(), JawShell.Get() })
	{
		UMaterialInstanceDynamic* Wear = (Mesh == JawShell.Get() && MouthMID) ? MouthMID.Get() : SkinMID.Get();
		if (Wear)
		{
			Mesh->SetMaterial(0, Wear);
		}
		if (MarksMID)
		{
			Mesh->SetMaterial(1, MarksMID);
		}
		Mesh->SetMeshSectionVisible(1, MarksMID != nullptr);
	}
}

void ADeepSerpent::PlaceHead(const FVector& HeadCm, const FVector& Facing)
{
	Heading = Facing.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	LastHeading = Heading;
	SetActorLocationAndRotation(HeadCm, Heading.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);

	const FDeepSerpentBodyTuning& Tuning = UDeepMonsterSettings::Get().Body;
	Trail.Reset();
	const int32 Points = FMath::CeilToInt((Tuning.LengthM + 1.f) * CmPerM / TrailStepCm) + 1;
	for (int32 i = 0; i < Points; ++i)
	{
		Trail.Add(HeadCm - Heading * (i * TrailStepCm));
	}
	LayBody();
}

void ADeepSerpent::SwimToward(const FVector& AimCm, float SpeedMps, float TurnRateDegPerS, float DeltaSeconds)
{
	const float Dt = FMath::Max(0.f, DeltaSeconds);
	const FVector Here = GetActorLocation();
	const FVector ToAim = AimCm - Here;
	Heading = UDeepMonsterRules::TurnToward(Heading, ToAim, TurnRateDegPerS * Dt);

	const float StepCm = FMath::Min(FMath::Max(0.f, SpeedMps) * CmPerM * Dt, static_cast<float>(ToAim.Size()));
	SetActorLocationAndRotation(Here + Heading * StepCm, Heading.Rotation());
	RecordTrail();
}

void ADeepSerpent::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		Heading = GetActorForwardVector();
	}
	RecordTrail();
	UpdateMotion(DeltaSeconds);
	LayBody();
	UpdateEyes(DeltaSeconds);
	UpdateMarks(DeltaSeconds);
}

void ADeepSerpent::RecordTrail()
{
	const FVector HeadCm = GetActorLocation();
	if (Trail.Num() == 0 || FVector::Dist(HeadCm, Trail[0]) >= TrailStepCm)
	{
		Trail.Insert(HeadCm, 0);
	}

	// Keep just enough trail for the body plus a margin.
	const float KeepCm = (UDeepMonsterSettings::Get().Body.LengthM + 1.f) * CmPerM;
	float Total = 0.f;
	for (int32 i = 1; i < Trail.Num(); ++i)
	{
		Total += static_cast<float>(FVector::Dist(Trail[i], Trail[i - 1]));
		if (Total > KeepCm && i + 1 < Trail.Num())
		{
			Trail.SetNum(i + 1);
			break;
		}
	}
}

void ADeepSerpent::UpdateMotion(float Dt)
{
	const FDeepSerpentBodyTuning& Tuning = UDeepMonsterSettings::Get().Body;
	Dt = FMath::Max(0.f, Dt);

	// A pose override (shots only) shows that state's motion; the brain's own state is untouched.
	const uint8 Shown = PoseOverride >= 0 ? static_cast<uint8>(PoseOverride) : MoodState;
	if (Shown != SeenState)
	{
		SeenState = Shown;
		StateAge = 0.f;
	}
	StateAge += Dt;
	LifeAge += Dt;
	const EDeepMonsterState State = static_cast<EDeepMonsterState>(Shown);
	// The first sight (Pass) does not count: its markings stay dark, it is seen by the lamp alone.
	if (State != EDeepMonsterState::Lurk && State != EDeepMonsterState::Pass)
	{
		bHeardOnce = true;
	}

	// Turn rate (signed yaw, deg/s), smoothed: the head and neck bank into turns.
	if (Dt > 0.f)
	{
		const float Yaw = FMath::FindDeltaAngleDegrees(LastHeading.Rotation().Yaw, Heading.Rotation().Yaw) / Dt;
		YawRateDegPerS = FMath::FInterpTo(YawRateDegPerS, FMath::Clamp(Yaw, -360.f, 360.f), Dt, 4.f);
	}
	LastHeading = Heading;

	// Recoil: a sharp flinch that dies away.
	if (State == EDeepMonsterState::Recoil)
	{
		const float Rise = 0.08f;
		Flinch01 = StateAge < Rise ? StateAge / Rise
			: FMath::Exp(-(StateAge - Rise) * 4.f / FMath::Max(0.1f, Tuning.RecoilFlinchS));
	}
	else
	{
		Flinch01 = FMath::FInterpTo(Flinch01, 0.f, Dt, 6.f);
	}

	// The bite: strike, clamp, thrash.
	float ThrashYaw = 0.f;
	float ThrashRoll = 0.f;
	// A slit of a mouth at rest: the jaw line reads in silhouette.
	float TargetGape = 4.f;
	float TargetLungeCm = 0.f;
	switch (State)
	{
	case EDeepMonsterState::Bite:
	{
		const float Strike = FMath::Max(0.01f, Tuning.LungeS);
		if (StateAge < Strike)
		{
			LungeCm = SerpentEaseOutCubic(StateAge / Strike) * Tuning.LungeM * CmPerM;
			TargetGape = Tuning.JawOpenDeg;
			GapeDeg = Tuning.JawOpenDeg * FMath::Min(1.f, StateAge / (Strike * 0.5f));
		}
		else
		{
			TargetLungeCm = Tuning.LungeM * 0.35f * CmPerM;
			// Holding the line between its teeth: open enough that the teeth stay visible in the thrash.
			TargetGape = Tuning.BiteHoldGapeDeg;
			const float W = 2.f * PI * Tuning.BiteThrashHz * (StateAge - Strike);
			ThrashYaw = Tuning.BiteThrashDeg * FMath::Sin(W);
			ThrashRoll = Tuning.BiteThrashDeg * 0.6f * FMath::Sin(W + 1.f);
		}
		break;
	}
	case EDeepMonsterState::Recoil:
		TargetGape = Tuning.JawOpenDeg * Flinch01;
		break;
	case EDeepMonsterState::Hunt:
		TargetGape = Tuning.JawHuntDeg + 4.f * FMath::Sin(LifeAge * 3.f);
		break;
	case EDeepMonsterState::Stalk:
		TargetGape = Tuning.JawHuntDeg * 0.5f * MoodThreat;
		break;
	default:
		break;
	}
	if (!(State == EDeepMonsterState::Bite && StateAge < Tuning.LungeS))
	{
		LungeCm = FMath::FInterpTo(LungeCm, TargetLungeCm, Dt, 6.f);
		GapeDeg = FMath::FInterpTo(GapeDeg, TargetGape, Dt, 18.f);
	}

	// The weave speeds and widens in the flinch (the body convulses away from the light).
	const float Boost = 1.f + (Tuning.RecoilWeaveBoost - 1.f) * Flinch01;
	WeavePhase = FMath::Fmod(WeavePhase + 2.f * PI * Tuning.WeaveHz * Boost * Dt, 2000.f * PI);

	// Head socket: lunge forward, flinch up and back, sway with the weave, bank into turns, thrash in the bite.
	const float Sway = Tuning.HeadSwayDeg * FMath::Sin(WeavePhase + 0.5f * PI);
	const float Bank = Tuning.TurnRollDeg * FMath::Clamp(YawRateDegPerS / 90.f, -1.5f, 1.5f);
	HeadSocket->SetRelativeLocationAndRotation(
		FVector(LungeCm - Tuning.RecoilPullM * CmPerM * Flinch01, 0.f, 0.f),
		FRotator(Tuning.RecoilPitchDeg * Flinch01, Sway + ThrashYaw, Bank + ThrashRoll));
	JawHinge->SetRelativeRotation(FRotator(-GapeDeg, 0.f, 0.f));
}

void ADeepSerpent::LayBody()
{
	const FDeepSerpentBodyTuning& Tuning = UDeepMonsterSettings::Get().Body;
	const int32 N = FMath::Max(2, Tuning.Segments);
	const float SpacingM = BodyLengthM(Tuning) / N;
	const float Boost = 1.f + (Tuning.RecoilWeaveBoost - 1.f) * Flinch01;

	// The path starts at the head as it is now, then the recorded trail.
	TArray<FVector> Path;
	Path.Reserve(Trail.Num() + 1);
	Path.Add(GetActorLocation());
	Path.Append(Trail);

	SegmentTransforms.SetNum(N);
	FVector Previous = GetActorLocation();
	for (int32 i = 0; i < N; ++i)
	{
		const float T = (i + 0.5f) / static_cast<float>(N);
		const float AlongM = HeadBackM(Tuning) + (i + 0.5f) * SpacingM;
		const FVector OnPath = UDeepMonsterRules::PointAlongTrail(Path, AlongM * CmPerM);
		const FVector Ahead = UDeepMonsterRules::PointAlongTrail(Path, (AlongM - SpacingM) * CmPerM);
		const FVector Dir = (Ahead - OnPath).GetSafeNormal(UE_SMALL_NUMBER, Heading);
		FVector Side = FVector::CrossProduct(FVector::UpVector, Dir);
		if (!Side.Normalize())
		{
			Side = FVector::RightVector;
		}

		// A wave travelling down the body, growing toward the tail.
		const float Phase = WeavePhase - 2.f * PI * AlongM / FMath::Max(0.1f, Tuning.WeaveWavelengthM);
		const float SwingM = Tuning.WeaveAmplitudeM * Boost * (0.15f + 0.85f * T) * FMath::Sin(Phase);
		const FVector Location = OnPath + Side * (SwingM * CmPerM);

		const float RadiusM = bTube ? RadiusAt(Tuning, T) : FMath::Lerp(Tuning.NeckRadiusM, Tuning.TailRadiusM, FMath::Pow(T, 1.2f));
		const FVector Facing = (Previous - Location).GetSafeNormal(UE_SMALL_NUMBER, Dir);
		const FVector Scale(FMath::Max(RadiusM * 2.f, SpacingM * 1.6f), RadiusM * 2.f, RadiusM * 2.f);
		SegmentTransforms[i] = FTransform(Facing.Rotation(), Location, Scale);
		Previous = Location;
	}

	if (bTube)
	{
		BuildTube();
		return;
	}
	if (Body->GetInstanceCount() != N)
	{
		Body->ClearInstances();
		for (const FTransform& Segment : SegmentTransforms)
		{
			Body->AddInstance(Segment, true);
		}
	}
	else
	{
		Body->BatchUpdateInstancesTransforms(0, SegmentTransforms, true, true, false);
	}
}

void ADeepSerpent::BuildTube()
{
	const FDeepSerpentBodyTuning& Tuning = UDeepMonsterSettings::Get().Body;
	const int32 Rings = FMath::Max(4, Tuning.TubeRings);
	const int32 Sides = FMath::Max(8, Tuning.TubeSides / 4 * 4);
	const int32 Row = Sides + 1;
	const float StartM = HeadBackM(Tuning) - NeckOverlapM;
	const float LengthM = BodyLengthM(Tuning) + NeckOverlapM;
	const float Boost = 1.f + (Tuning.RecoilWeaveBoost - 1.f) * Flinch01;

	TArray<FVector> Path;
	Path.Reserve(Trail.Num() + 1);
	Path.Add(GetActorLocation());
	Path.Append(Trail);

	const FTransform Socket = HeadSocket->GetComponentTransform();
	const FVector SocketBack = -Socket.GetUnitAxis(EAxis::X);

	// ---- Ring centres: the trail plus the weave; the neck bends toward the head socket.
	TArray<FVector, TInlineAllocator<128>> Centres;
	TArray<float, TInlineAllocator<128>> Phases;
	Centres.SetNum(Rings);
	Phases.SetNum(Rings);
	for (int32 k = 0; k < Rings; ++k)
	{
		const float T = k / static_cast<float>(Rings - 1);
		const float AlongM = StartM + T * LengthM;
		const FVector OnPath = UDeepMonsterRules::PointAlongTrail(Path, AlongM * CmPerM);
		const FVector Ahead = UDeepMonsterRules::PointAlongTrail(Path, (AlongM - 0.3f) * CmPerM);
		const FVector Dir = (Ahead - OnPath).GetSafeNormal(UE_SMALL_NUMBER, Heading);
		FVector Side = FVector::CrossProduct(FVector::UpVector, Dir);
		if (!Side.Normalize())
		{
			Side = FVector::RightVector;
		}
		Phases[k] = WeavePhase - 2.f * PI * AlongM / FMath::Max(0.1f, Tuning.WeaveWavelengthM);
		const float SwingM = Tuning.WeaveAmplitudeM * Boost * (0.15f + 0.85f * T) * FMath::Sin(Phases[k]);
		FVector Centre = OnPath + Side * (SwingM * CmPerM);
		const float ToHeadM = AlongM - HeadBackM(Tuning);
		const float Blend = 1.f - FMath::SmoothStep(0.f, NeckBlendM, FMath::Max(0.f, ToHeadM));
		if (Blend > 0.f)
		{
			const FVector FromSocket = Socket.GetLocation() + SocketBack * (AlongM * CmPerM);
			Centre = FMath::Lerp(Centre, FromSocket, Blend);
		}
		Centres[k] = Centre;
	}

	// ---- Frames: parallel transport from the head's up, pulled gently back to world up, plus the bank.
	TubeVerts.SetNumUninitialized(Rings * Row);
	TubeUV0.SetNumUninitialized(Rings * Row);
	TubeUV1.SetNumUninitialized(Rings * Row);
	TubeColors.SetNumUninitialized(Rings * Row);
	FVector Up = Socket.GetUnitAxis(EAxis::Z);
	const float Bank = Tuning.TurnRollDeg * FMath::Clamp(YawRateDegPerS / 90.f, -1.5f, 1.5f);
	for (int32 k = 0; k < Rings; ++k)
	{
		const float T = k / static_cast<float>(Rings - 1);
		const FVector Tangent = (Centres[FMath::Min(Rings - 1, k + 1)] - Centres[FMath::Max(0, k - 1)]).GetSafeNormal(UE_SMALL_NUMBER, SocketBack);
		Up = (Up - Tangent * FVector::DotProduct(Up, Tangent)).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		const FVector WorldUp = (FVector::UpVector - Tangent * FVector::DotProduct(FVector::UpVector, Tangent)).GetSafeNormal(UE_SMALL_NUMBER, Up);
		Up = FMath::Lerp(Up, WorldUp, 0.08f).GetSafeNormal(UE_SMALL_NUMBER, WorldUp);

		const float RollDeg = Tuning.WeaveRollDeg * Boost * (0.3f + 0.7f * T) * FMath::Cos(Phases[k]) + Bank * FMath::Square(1.f - T);
		const FVector RingUp = FQuat(Tangent, FMath::DegreesToRadians(RollDeg)).RotateVector(Up);
		const FVector RingSide = FVector::CrossProduct(RingUp, Tangent);

		const float RadiusM = k == 0 ? Tuning.NeckRadiusM : RadiusAt(Tuning, T);
		FRingShape Shape;
		Shape.HalfWidth = RadiusM;
		Shape.Top = RadiusM;
		Shape.Bottom = RadiusM * 0.92f;
		Shape.Ridge = Tuning.RidgeHeight * (1.f - 0.6f * T);
		const float U = T * LengthM - NeckOverlapM;
		for (int32 j = 0; j <= Sides; ++j)
		{
			const FVector Local = RingPoint(Shape, j, Sides) * CmPerM;
			const int32 Index = k * Row + j;
			TubeVerts[Index] = Centres[k] + RingSide * Local.Y + RingUp * Local.Z;
			const float Around = j / static_cast<float>(Sides);
			TubeUV1[Index] = FVector2D(U, Around);
			TubeUV0[Index] = FVector2D(U * Tuning.SkinTilesPerM, Around * Tuning.SkinTilesAround);
			TubeColors[Index] = FLinearColor(RadiusM, 0.f, 0.f, 1.f);
		}
	}
	RingGridNormals(TubeVerts, Rings, Sides, TubeNormals, TubeTangents);
	ShellVerts.SetNumUninitialized(TubeVerts.Num());
	for (int32 i = 0; i < TubeVerts.Num(); ++i)
	{
		ShellVerts[i] = TubeVerts[i] + TubeNormals[i] * MarksLiftCm;
	}

	TArray<FProcMeshTangent> Tangents;
	ToProcTangents(TubeTangents, Tangents);
	const TArray<FVector2D> None;
	const int32 Expected = Rings * Row;
	if (!bTubeCreated || TubeTris.Num() != (Rings - 1) * Sides * 6 || Tube->GetProcMeshSection(0) == nullptr
		|| Tube->GetProcMeshSection(0)->ProcVertexBuffer.Num() != Expected)
	{
		TubeTris.Reset();
		RingGridTriangles(Rings, Sides, 0, TubeTris);
		Tube->CreateMeshSection_LinearColor(0, TubeVerts, TubeTris, TubeNormals, TubeUV0, TubeUV1, None, None, TubeColors, Tangents, false, false);
		Tube->CreateMeshSection_LinearColor(1, ShellVerts, TubeTris, TubeNormals, TubeUV0, TubeUV1, None, None, TubeColors, Tangents, false, false);
		if (SkinMID)
		{
			Tube->SetMaterial(0, SkinMID);
		}
		if (MarksMID)
		{
			Tube->SetMaterial(1, MarksMID);
		}
		Tube->SetMeshSectionVisible(1, MarksMID != nullptr);
		bTubeCreated = true;
	}
	else
	{
		Tube->UpdateMeshSection_LinearColor(0, TubeVerts, TubeNormals, TubeUV0, TubeUV1, None, None, TubeColors, Tangents, false);
		Tube->UpdateMeshSection_LinearColor(1, ShellVerts, TubeNormals, TubeUV0, TubeUV1, None, None, TubeColors, Tangents, false);
	}
}

void ADeepSerpent::UpdateEyes(float Dt)
{
	const FDeepSerpentBodyTuning& Tuning = UDeepMonsterSettings::Get().Body;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Retro-reflection: an eye shines back toward a light that is on it, so with the lamp beside the diver's eyes
	// it shines only while the beam is on it. In the cone, facing the lamp, within range; dark otherwise.
	const FTransform Socket = HeadSocket->GetComponentTransform();
	UStaticMeshComponent* Eyes[2] = { EyeLeft.Get(), EyeRight.Get() };
	UPointLightComponent* Lights[2] = { EyeLightLeft.Get(), EyeLightRight.Get() };
	for (int32 e = 0; e < 2; ++e)
	{
		const float SideSign = e == 0 ? -1.f : 1.f;
		const FVector EyePos = Eyes[e]->GetComponentLocation();
		const FVector EyeNormal = Socket.TransformVectorNoScale(FVector(1.f, 0.7f * SideSign, 0.15f).GetSafeNormal());
		float Target = 0.f;
		for (TActorIterator<ADeepDiver> It(World); It; ++It)
		{
			const ADeepDiver* Diver = *It;
			if (!Diver->IsLampOn())
			{
				continue;
			}
			const USpotLightComponent* Lamp = Diver->FindComponentByClass<USpotLightComponent>();
			if (!Lamp)
			{
				continue;
			}
			const FVector ToEye = EyePos - Lamp->GetComponentLocation();
			const float Dist = static_cast<float>(ToEye.Size());
			const float Range = FMath::Max(1.f, Lamp->AttenuationRadius);
			if (Dist < 1.f || Dist > Range)
			{
				continue;
			}
			const FVector Dir = ToEye / Dist;
			const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Lamp->GetForwardVector(), Dir), -1.f, 1.f)));
			const float Outer = FMath::Max(1.f, Lamp->OuterConeAngle);
			const float Cone = 1.f - FMath::SmoothStep(Outer * 0.85f, Outer * 1.05f, AngleDeg);
			const float Facing = FMath::Pow(FMath::Max(0.f, FVector::DotProduct(EyeNormal, -Dir)), Tuning.EyeShineFacingPower);
			const float Fall = 1.f - FMath::Pow(Dist / Range, 4.f);
			Target = FMath::Max(Target, Cone * (0.15f + 0.85f * Facing) * Fall);
		}
		const float Rate = Target > EyeShine[e] ? Tuning.EyeShineAttack : Tuning.EyeShineRelease;
		EyeShine[e] = FMath::FInterpTo(EyeShine[e], Target, Dt, Rate);
		if (EyeMIDs.IsValidIndex(e))
		{
			EyeMIDs[e]->SetVectorParameterValue(TEXT("Color"), Tuning.EyeColor * (Tuning.EyeShineEmissive * EyeShine[e]));
		}
		Lights[e]->SetIntensity(Tuning.EyeLightIntensity * EyeShine[e]);
	}
}

void ADeepSerpent::UpdateMarks(float Dt)
{
	if (!MarksMID)
	{
		return;
	}
	const FDeepSerpentBodyTuning& Tuning = UDeepMonsterSettings::Get().Body;
	// Not seen in the first minute: dark until RevealDelayS has passed and it has come in on a sound once.
	const bool bReveal = bForceReveal || (LifeAge >= Tuning.RevealDelayS && bHeardOnce);
	Reveal01 = FMath::FInterpConstantTo(Reveal01, bReveal ? 1.f : 0.f, Dt, 1.f / FMath::Max(0.1f, Tuning.RevealFadeS));

	const float Threat = PoseOverride >= 0 ? 1.f : MoodThreat;
	// Spots stay near dark until threat peaks (power curve), then pulse fast.
	const float ThreatCurve = FMath::Pow(Threat, FMath::Max(0.1f, Tuning.SpotThreatPower));
	PulsePhase = FMath::Fmod(PulsePhase + 2.f * PI * FMath::Lerp(Tuning.PulseHzCalm, Tuning.PulseHzThreat, Threat) * Dt, 2000.f * PI);
	// The flinch flares the spots, then they die with it. The rim gain defaults to 0: an outline read as a toy.
	const float Flare = 1.f + Flinch01;
	MarksMID->SetScalarParameterValue(TEXT("PulsePhase"), PulsePhase);
	MarksMID->SetScalarParameterValue(TEXT("SpotGain"), Tuning.SpotGain * Reveal01 * Flare * FMath::Lerp(Tuning.SpotCalmFraction, 1.f, ThreatCurve));
	MarksMID->SetScalarParameterValue(TEXT("RimGain"), Reveal01 * FMath::Lerp(Tuning.RimGainCalm, Tuning.RimGainThreat, Threat));
}
