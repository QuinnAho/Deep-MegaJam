#include "Monster/DeepSerpent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Diver/DeepDiver.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
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

	/** `Deep.Monster.Glow 0` turns its own light off (before-and-after shots of the bite in the dark). */
	int32 GGlowAllowed = 1;
	FAutoConsoleVariableRef CVarGlow(
		TEXT("Deep.Monster.Glow"),
		GGlowAllowed,
		TEXT("Deep.Monster.Glow <0|1>: 1 (default) = the serpent's photophores light its head in a bite and faintly on the first pass; 0 = off."));

	/** `Deep.Monster.MeshHead`: the switch for the pack head. Changing it rebuilds every serpent's look at once. */
	int32 GMeshHeadAllowed = 1;
	FAutoConsoleVariableRef CVarMeshHead(
		TEXT("Deep.Monster.MeshHead"),
		GMeshHeadAllowed,
		TEXT("Deep.Monster.MeshHead <0|1>: 1 (default) = the serpent wears the mesh head its settings name, when it loads (none by default since Nov 15: the code-built head); 0 = always the code-built head."),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable*)
		{
			if (!GEngine)
			{
				return;
			}
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (UWorld* World = Context.World())
				{
					for (TActorIterator<ADeepSerpent> It(World); It; ++It)
					{
						It->RefreshLook();
					}
				}
			}
		}),
		ECVF_Default);

	/** Body radius at T (0 = neck, 1 = tail tip): a swell behind the head, then the taper. */
	float RadiusAt(const FDeepSerpentBodyTuning& Body, float T, float Swell = -1.f)
	{
		const float Peak = Body.NeckRadiusM * (Swell > 0.f ? Swell : Body.BellySwell);
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
	// The tube rides with the actor's location (not its rotation): its vertices are written relative to the head, so
	// the renderer sees the body move with the head and its motion vectors are close to right. With the vertices in
	// world space and the component still, temporal AA smeared the moving skin into streaks (dragon pass, Nov 15).
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
	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(HeadSocket);
	GlowLight->SetMobility(EComponentMobility::Movable);
	GlowLight->SetCastShadows(false);
	GlowLight->SetIntensityUnits(ELightUnits::Unitless);
	GlowLight->SetIntensity(0.f);
	GlowLight->SetVisibility(false);

	Body =CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Body"));
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
		// Dorsal spikes (mesh head): horn, matte and dark, so their flat faces never flash white in the lamp.
		SpikeMID = UMaterialInstanceDynamic::Create(SkinBase, this);
		SpikeMID->SetVectorParameterValue(TEXT("SkinColor"), Tuning.SkinColor * 0.6f);
		SpikeMID->SetScalarParameterValue(TEXT("ScaleDepth"), 0.f);
		SpikeMID->SetScalarParameterValue(TEXT("WetCoat"), 0.1f);
		SpikeMID->SetScalarParameterValue(TEXT("Roughness"), 0.7f);
		SpikeMID->SetScalarParameterValue(TEXT("GrooveRoughness"), 0.75f);
		SpikeMID->SetScalarParameterValue(TEXT("Specular"), 0.15f);
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

	// ---- Eyes: the engine's unlit emissive material; dark until a lamp is on them. A mesh head moves them to its eyes.
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

	// ---- Head: a mesh at the socket if one is set and loads, else the code-built wedge and jaw.
	HeadBackCm = HeadBackM(Tuning) * CmPerM;
	HeadFrontCm = HeadFrontM(Tuning) * CmPerM;
	NeckCutLocalCm = FVector(-HeadBackCm, 0.f, 0.f);
	bMeshHead = bTube && IsMeshHeadAllowed() && SetupMeshHead(Tuning);
	HeadMeshComp->SetVisibility(bMeshHead);
	if (bMeshHead)
	{
		// One skin: the body wears the dragon's own neck skin, graded like the head.
		const FDeepSerpentMeshHead& H = Tuning.MeshHead;
		if (UMaterialInterface* BodySkin = Cast<UMaterialInterface>(H.BodyMaterial.TryLoad()))
		{
			SkinMID = UMaterialInstanceDynamic::Create(BodySkin, this);
			SkinMID->SetVectorParameterValue(TEXT("PatchOrigin"), FLinearColor(H.PatchOrigin.X, H.PatchOrigin.Y, 0.f));
			SkinMID->SetVectorParameterValue(TEXT("PatchSize"), FLinearColor(H.PatchSize.X, H.PatchSize.Y, 0.f));
			SkinMID->SetVectorParameterValue(TEXT("PatchMetres"), FLinearColor(H.PatchMetres.X, H.PatchMetres.Y, 0.f));
			SkinMID->SetScalarParameterValue(TEXT("ColorGain"), H.ColorGain);
		}
		else if (SkinMID && H.BodyScaleSizeM > 0.f)
		{
			SkinMID->SetScalarParameterValue(TEXT("ScaleSize"), H.BodyScaleSizeM);
		}
	}
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

bool ADeepSerpent::IsMeshHeadAllowed()
{
	return GMeshHeadAllowed != 0;
}

void ADeepSerpent::RefreshLook()
{
	SetupLook();
	bTubeCreated = false;
	LayBody();
}

FVector ADeepSerpent::GetHeadLocation() const
{
	return HeadSocket ? HeadSocket->GetComponentLocation() : GetActorLocation();
}

void ADeepSerpent::GetDrawnSpheres(TArray<FSphere>& Out) const
{
	const FDeepSerpentBodyTuning& Tuning = UDeepMonsterSettings::Get().Body;
	if (bTube && Tube && Tube->IsVisible())
	{
		// The tube's vertices are relative to the tube's location (absolute rotation and scale): world = location + vertex.
		const int32 Sides = FMath::Max(8, Tuning.TubeSides / 4 * 4);
		const int32 Row = Sides + 1;
		const FVector Origin = Tube->GetComponentLocation();
		for (int32 Start = 0; Start + Row <= TubeVerts.Num(); Start += Row)
		{
			FVector Centre = FVector::ZeroVector;
			for (int32 j = 0; j < Sides; ++j)
			{
				Centre += TubeVerts[Start + j];
			}
			Centre /= Sides;
			double RadiusSq = 0.0;
			for (int32 j = 0; j < Sides; ++j)
			{
				RadiusSq = FMath::Max(RadiusSq, FVector::DistSquared(Centre, TubeVerts[Start + j]));
			}
			Out.Emplace(Origin + Centre, FMath::Sqrt(RadiusSq));
		}
	}
	else
	{
		// The sphere fallback: unit spheres (100 cm across) scaled by the segment's width.
		for (const FTransform& Segment : SegmentTransforms)
		{
			Out.Emplace(Segment.GetLocation(), Segment.GetScale3D().Y * 50.f);
		}
	}
	// The head: three spheres from the neck cut to the snout along the socket's forward axis.
	if (HeadSocket)
	{
		const FTransform Socket = HeadSocket->GetComponentTransform();
		const FVector Forward = Socket.GetUnitAxis(EAxis::X);
		const float RadiusCm = FMath::Max(Tuning.HeadRadiusM, Tuning.NeckRadiusM) * CmPerM;
		for (int32 i = 0; i < 3; ++i)
		{
			const float AlongCm = FMath::Lerp(-HeadBackCm, HeadFrontCm, i / 2.f);
			Out.Emplace(Socket.GetLocation() + Forward * AlongCm, RadiusCm);
		}
	}
}

FVector ADeepSerpent::GetEyeMidpoint() const
{
	return (EyeLeft->GetComponentLocation() + EyeRight->GetComponentLocation()) * 0.5f;
}

float ADeepSerpent::BodyLengthM() const
{
	return FMath::Max(0.5f, UDeepMonsterSettings::Get().Body.LengthM - (HeadBackCm + HeadFrontCm) / CmPerM);
}

bool ADeepSerpent::SetupMeshHead(const FDeepSerpentBodyTuning& Tuning)
{
	const FDeepSerpentMeshHead& H = Tuning.MeshHead;
	UStaticMesh* Mesh = H.Mesh.IsValid() ? Cast<UStaticMesh>(H.Mesh.TryLoad()) : nullptr;
	if (!Mesh)
	{
		return false;
	}

	// The head's own frame from its landmarks: forward, right along the eye line (so the eyes come out level), up.
	const FVector Forward = H.Forward.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	FVector Right = H.EyeRightCm - H.EyeLeftCm;
	Right -= Forward * FVector::DotProduct(Right, Forward);
	if (!Right.Normalize())
	{
		Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
	}
	const FVector Up = FVector::CrossProduct(Forward, Right);
	// That frame onto the socket's X, Y, Z; the skull point onto the socket's origin.
	const FQuat ToSocket = FQuat(FMatrix(Forward, Right, Up, FVector::ZeroVector)).Inverse();
	const float Scale = FMath::Max(0.0001f, H.Scale);
	auto Place = [&](const FVector& MeshCm) { return ToSocket.RotateVector((MeshCm - H.SkullCm) * Scale); };

	HeadMeshComp->SetStaticMesh(Mesh);
	HeadMeshComp->SetRelativeTransform(FTransform(ToSocket, Place(FVector::ZeroVector), FVector(Scale)));
	// The whole dragon is in the mesh: keep it out of distance-field and Lumen scene lighting, where the clip may not reach.
	HeadMeshComp->bAffectDistanceFieldLighting = false;
	HeadMeshComp->SetAffectDynamicIndirectLighting(false);
	HeadMeshComp->SetCastShadow(true);

	HeadMeshMID = nullptr;
	if (UMaterialInterface* Base = Cast<UMaterialInterface>(H.Material.TryLoad()))
	{
		HeadMeshMID = UMaterialInstanceDynamic::Create(Base, this);
		HeadMeshMID->SetVectorParameterValue(TEXT("CutOrigin"), FLinearColor(H.CutCm));
		HeadMeshMID->SetVectorParameterValue(TEXT("CutNormal"), FLinearColor(H.CutNormal.GetSafeNormal(UE_SMALL_NUMBER, Forward)));
		HeadMeshMID->SetVectorParameterValue(TEXT("KeepEnd"), FLinearColor(H.KeepEndCm));
		HeadMeshMID->SetScalarParameterValue(TEXT("KeepRadius"), H.KeepRadiusCm);
		HeadMeshMID->SetScalarParameterValue(TEXT("ColorGain"), H.ColorGain);
		for (int32 Slot = 0; Slot < HeadMeshComp->GetNumMaterials(); ++Slot)
		{
			HeadMeshComp->SetMaterial(Slot, HeadMeshMID);
		}
	}

	// Where the body takes over, and how long the head is either side of the socket.
	NeckCutLocalCm = Place(H.CutCm);
	MouthLocalCm = H.MouthCm * Scale;
	HeadBackCm = FMath::Max(1.f, -NeckCutLocalCm.X);
	HeadFrontCm = FMath::Max(1.f, Place(H.KeepEndCm).X);

	// Eyes (and their light) on the dragon's eyes.
	EyeLeft->SetRelativeLocation(Place(H.EyeLeftCm));
	EyeRight->SetRelativeLocation(Place(H.EyeRightCm));
	const FVector LightForward(Tuning.EyeRadiusM * CmPerM * 2.f, 0.f, 0.f);
	EyeLightLeft->SetRelativeLocation(EyeLeft->GetRelativeLocation() + LightForward);
	EyeLightRight->SetRelativeLocation(EyeRight->GetRelativeLocation() + LightForward);
	return true;
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
	LastHeadCm = HeadCm;
	ResetChain();
	LayBody();
}

void ADeepSerpent::KeepPath(const TFunction<FVector(const FVector&)>& Keep, bool bWholeTrail)
{
	BodyKeep = Keep;
	if (!Keep)
	{
		return;
	}
	const FVector Here = GetActorLocation();
	const FVector Kept = Keep(Here);
	if (!Kept.Equals(Here, 1.0))
	{
		SetActorLocation(Kept);
	}
	const int32 Count = bWholeTrail ? Trail.Num() : FMath::Min(1, Trail.Num());
	for (int32 i = 0; i < Count; ++i)
	{
		Trail[i] = Keep(Trail[i]);
	}
	if (bWholeTrail)
	{
		// The path was snapped: the body lies along it again instead of whipping across the cave.
		LastHeadCm = GetActorLocation();
		ResetChain();
		LayBody();
	}
}

void ADeepSerpent::ResetChain()
{
	const FDeepSerpentBodyTuning& Tuning = UDeepMonsterSettings::Get().Body;
	const int32 N = FMath::Max(2, Tuning.Segments);
	const float SpacingCm = BodyLengthM() / N * CmPerM;
	// Neck, N segment centres, tail tip: half a spacing from the neck to the first centre and from the last to the tip.
	Chain.RestCm.SetNum(N + 1);
	for (int32 i = 0; i <= N; ++i)
	{
		Chain.RestCm[i] = (i == 0 || i == N) ? SpacingCm * 0.5f : SpacingCm;
	}
	TArray<FVector> Path;
	Path.Reserve(Trail.Num() + 2);
	Path.Add(GetActorLocation());
	if (Trail.Num() >= 2)
	{
		Path.Append(Trail);
	}
	else
	{
		Path.Add(GetActorLocation() - Heading * 100.f);
	}
	UDeepMonsterRules::LayChain(Chain, Path, HeadBackCm);
}

void ADeepSerpent::StepBody(float Dt)
{
	const FDeepSerpentBodyTuning& Tuning = UDeepMonsterSettings::Get().Body;
	const int32 N = FMath::Max(2, Tuning.Segments);
	if (Chain.Pos.Num() != N + 2 || !FMath::IsNearlyEqual(Chain.RestCm[1], BodyLengthM() / N * CmPerM, 0.5f))
	{
		ResetChain();
	}
	// The pin: the neck of the head as it is drawn now (stroke, lunge, flinch and thrash included). The head drags the body.
	const FTransform Socket = HeadSocket->GetComponentTransform();
	const FVector Pin = Socket.TransformPosition(NeckCutLocalCm);
	const FVector Forward = Socket.GetUnitAxis(EAxis::X);
	if (bKickPending)
	{
		bKickPending = false;
		UDeepMonsterRules::KickChain(Chain, FVector::UpVector, Tuning.Chain.RecoilKickMps * CmPerM, Tuning.Chain.RecoilKickWaveM * CmPerM, Tuning.Chain.SubstepS);
	}
	TArray<FVector> Path;
	Path.Reserve(Trail.Num() + 1);
	Path.Add(GetActorLocation());
	Path.Append(Trail);
	if (BodyKeep)
	{
		UDeepMonsterRules::StepChain(Chain, Pin, Forward, Path, HeadBackCm, Tuning.Chain, Dt, [this](const FVector& P) { return BodyKeep(P); });
	}
	else
	{
		UDeepMonsterRules::StepChain(Chain, Pin, Forward, Path, HeadBackCm, Tuning.Chain, Dt);
	}
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
	StepBody(DeltaSeconds);
	LayBody();
	UpdateEyes(DeltaSeconds);
	UpdateMarks(DeltaSeconds);
	UpdateGlow(DeltaSeconds);
}

void ADeepSerpent::UpdateGlow(float Dt)
{
	const FDeepSerpentBodyTuning& Tuning = UDeepMonsterSettings::Get().Body;
	const EDeepMonsterState State = static_cast<EDeepMonsterState>(MoodState);
	const bool bPosed = PoseOverride >= 0;
	float Target = 0.f;
	if (GGlowAllowed != 0 && !bPosed)
	{
		if (State == EDeepMonsterState::Bite)
		{
			// Pulses with the spots (PulsePhase runs at the threat rate), never below the low point.
			const float Wave = 0.5f + 0.5f * FMath::Sin(PulsePhase);
			Target = FMath::Lerp(FMath::Clamp(Tuning.GlowPulseLow, 0.f, 1.f), 1.f, Wave);
		}
		else if (State == EDeepMonsterState::Pass)
		{
			Target = FMath::Clamp(Tuning.GlowPassFraction, 0.f, 1.f);
		}
		else if (State == EDeepMonsterState::Recoil)
		{
			// The flinch flares its spots (UpdateMarks) and its light with them, then both die: the lamp's hit reads
			// even though the lamp dims itself for a body this close.
			Target = FMath::Clamp(Flinch01, 0.f, 1.f);
		}
	}
	const float Rate = Target > Glow01 ? Tuning.GlowAttack : Tuning.GlowRelease;
	Glow01 = FMath::FInterpTo(Glow01, Target, Dt, Rate);
	if (Glow01 < 0.005f)
	{
		Glow01 = Target > 0.f ? Glow01 : 0.f;
	}
	GlowLight->SetRelativeLocation(Tuning.GlowOffsetCm);
	GlowLight->SetLightColor(Tuning.GlowColor);
	GlowLight->SetAttenuationRadius(Tuning.GlowRadiusM * CmPerM);
	GlowLight->SetVolumetricScatteringIntensity(Tuning.GlowScattering);
	GlowLight->SetIntensity(Tuning.GlowIntensity * Glow01);
	GlowLight->SetVisibility(Glow01 > 0.f);
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
		// The flinch throws the body: one sideways kick into the chain.
		bKickPending = static_cast<EDeepMonsterState>(Shown) == EDeepMonsterState::Recoil;
	}
	StateAge += Dt;
	LifeAge += Dt;

	// How fast the head travels (0..1 of the stroke's full speed): the swim stroke is zero at rest.
	const FVector HeadNow = GetActorLocation();
	if (Dt > 0.f)
	{
		const float Mps = static_cast<float>(FVector::Dist(HeadNow, LastHeadCm)) / CmPerM / Dt;
		const float Target = FMath::Clamp(Mps / FMath::Max(0.1f, Tuning.Chain.StrokeFullSpeedMps), 0.f, 1.f);
		Speed01 = FMath::FInterpTo(Speed01, Target, Dt, 3.f);
	}
	LastHeadCm = HeadNow;
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
		// A mesh head holds a moment in the beam before it rears (the lit head reads for a few frames).
		const float Rise = bMeshHead ? 0.22f : 0.08f;
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
	float BiteShakePitch = 0.f;
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
		else if (bMeshHead)
		{
			// A mesh head cannot close its jaw: it holds the line in its open mouth (BiteHold01 below) and worries it.
			const FDeepSerpentMeshHead& H = Tuning.MeshHead;
			const float W = 2.f * PI * H.BiteShakeHz * (StateAge - Strike);
			ThrashYaw = H.BiteShakeDeg * 0.35f * FMath::Sin(W * 0.7f + 2.f);
			ThrashRoll = H.BiteShakeDeg * 0.8f * FMath::Sin(W * 0.8f + 1.f);
			BiteShakePitch = H.BiteShakeDeg * FMath::Sin(W);
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

	// The swim stroke: the head sweeps side to side (and yaws along its sweep) by its speed; the chain carries it down the
	// body. Look only: the actor (the AI's path, the bite reach, the cave clearance) never moves with it.
	const FDeepSerpentChainTuning& Stroke = Tuning.Chain;
	StrokePhase = FMath::Fmod(StrokePhase + 2.f * PI * Stroke.StrokeHz * (0.5f + 0.5f * Speed01) * Dt, 2000.f * PI);
	const float Sway = Stroke.StrokeDeg * Speed01 * FMath::Cos(StrokePhase);
	const float StrokeCm = Stroke.StrokeM * CmPerM * Speed01 * FMath::Sin(StrokePhase);

	// Head socket: lunge forward, flinch up and back, the stroke, bank into turns, thrash in the bite.
	const float Bank = Tuning.TurnRollDeg * FMath::Clamp(YawRateDegPerS / 90.f, -1.5f, 1.5f);
	FVector SocketLoc(LungeCm - Tuning.RecoilPullM * CmPerM * Flinch01, StrokeCm, 0.f);
	// Mesh head biting: turn side-on to the nearest diver. The line runs from the diver up to the sub, so side-on to
	// the diver puts the line across the open mouth, through the teeth, seen in profile from where the diver is.
	if (bMeshHead && State == EDeepMonsterState::Bite)
	{
		if (const UWorld* World = GetWorld())
		{
			float Best = TNumericLimits<float>::Max();
			FVector ToDiver = FVector::ZeroVector;
			for (TActorIterator<ADeepDiver> It(World); It; ++It)
			{
				const FVector D = It->GetActorLocation() - GetActorLocation();
				if (D.SizeSquared() < Best)
				{
					Best = static_cast<float>(D.SizeSquared());
					ToDiver = D;
				}
			}
			ToDiver.Z = 0.f;
			if (ToDiver.Normalize())
			{
				FVector Side = FVector::CrossProduct(FVector::UpVector, ToDiver);
				if (FVector::DotProduct(Side, GetActorForwardVector()) < 0.f)
				{
					Side = -Side;
				}
				const float Want = FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, Side.Rotation().Yaw);
				BiteFaceYaw = FMath::FInterpTo(BiteFaceYaw, Want, Dt, 6.f);
			}
		}
	}
	else
	{
		BiteFaceYaw = FMath::FInterpTo(BiteFaceYaw, 0.f, Dt, 4.f);
	}
	const FRotator SocketRot(Tuning.RecoilPitchDeg * Flinch01 + BiteShakePitch, Sway + ThrashYaw + BiteFaceYaw, Bank + ThrashRoll);
	// Mesh head biting: the mouth sits on the line (the actor, which the bite steers onto it) and the head pivots
	// about it, so the line stays between the teeth while the head shakes. Eases in after the strike and out after.
	const bool bHolding = bMeshHead && State == EDeepMonsterState::Bite && StateAge >= Tuning.LungeS;
	BiteHold01 = FMath::FInterpTo(BiteHold01, bHolding ? 1.f : 0.f, Dt, bHolding ? 10.f : 6.f);
	if (BiteHold01 > 0.001f)
	{
		SocketLoc = FMath::Lerp(SocketLoc, -SocketRot.RotateVector(MouthLocalCm), BiteHold01);
	}
	HeadSocket->SetRelativeLocationAndRotation(SocketLoc, SocketRot);
	JawHinge->SetRelativeRotation(FRotator(-GapeDeg, 0.f, 0.f));
}

void ADeepSerpent::LayBody()
{
	const FDeepSerpentBodyTuning& Tuning = UDeepMonsterSettings::Get().Body;
	const int32 N = FMath::Max(2, Tuning.Segments);
	const float SpacingM = BodyLengthM() / N;
	if (Chain.Pos.Num() != N + 2)
	{
		ResetChain();
	}

	// The hit segments are the chain's particles 1..N (0 is the neck, N + 1 the tail tip).
	SegmentTransforms.SetNum(N);
	for (int32 i = 0; i < N; ++i)
	{
		const float T = (i + 0.5f) / static_cast<float>(N);
		const FVector Location = Chain.Pos[i + 1];
		const FVector Facing = (Chain.Pos[i] - Chain.Pos[i + 2]).GetSafeNormal(UE_SMALL_NUMBER, Heading);
		const float RadiusM = bTube ? RadiusAt(Tuning, T) : FMath::Lerp(Tuning.NeckRadiusM, Tuning.TailRadiusM, FMath::Pow(T, 1.2f));
		const FVector Scale(FMath::Max(RadiusM * 2.f, SpacingM * 1.6f), RadiusM * 2.f, RadiusM * 2.f);
		SegmentTransforms[i] = FTransform(Facing.Rotation(), Location, Scale);
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
	// A mesh head: the body starts as a sleeve inside the neck, in front of the cut, and is over it by the cut.
	const FDeepSerpentMeshHead& MeshTuning = Tuning.MeshHead;
	const float HeadBack = HeadBackCm / CmPerM;
	const float FrontM = bMeshHead ? FMath::Max(0.05f, MeshTuning.SleeveM) : NeckOverlapM;
	const float StartM = HeadBack - FrontM;
	const float BodyM = BodyLengthM();
	const float LengthM = BodyM + FrontM;

	// The body runs along the chain, from the neck (its pinned particle) to the tail tip, on a smooth curve through the particles.

	const FTransform Socket = HeadSocket->GetComponentTransform();
	const FVector SocketBack = -Socket.GetUnitAxis(EAxis::X);
	const FVector Origin = Tube->GetComponentLocation();
	// Ring spacing: even for the code-built head; closer near a mesh head, where the sleeve and the join need them.
	const bool bDenseFront = bMeshHead;
	auto RingAlong = [bDenseFront](float T) { return bDenseFront ? FMath::Pow(T, 1.4f) : T; };

	// ---- Ring centres: along the chain; the neck bends toward the head socket.
	TArray<FVector, TInlineAllocator<128>> Centres;
	Centres.SetNum(Rings);
	for (int32 k = 0; k < Rings; ++k)
	{
		const float T = k / static_cast<float>(Rings - 1);
		const float AlongM = StartM + RingAlong(T) * LengthM;
		FVector Centre = UDeepMonsterRules::ChainPoint(Chain, (AlongM - HeadBack) * CmPerM);
		const float ToHeadM = AlongM - HeadBack;
		const float Blend = 1.f - FMath::SmoothStep(0.f, NeckBlendM, FMath::Max(0.f, ToHeadM));
		if (Blend > 0.f)
		{
			// Straight back from where the neck is cut (for the code-built head: straight back from the socket).
			const FVector FromSocket = Socket.TransformPosition(NeckCutLocalCm - FVector(ToHeadM * CmPerM, 0.f, 0.f));
			Centre = FMath::Lerp(Centre, FromSocket, Blend);
		}
		Centres[k] = Centre;
	}

	// ---- Arm's length: the body never comes within BodyClearM of a diver's eyes. It follows the head's path, and the
	// stalk's spiral and the bite's side approach can pass right by the diver, where the lamp burnt it white. Look only:
	// the harpoon's hit segments (LayBody) keep the true path.
	if (UWorld* World = GetWorld())
	{
		const float ClearCm = Tuning.BodyClearM * CmPerM;
		for (TActorIterator<ADeepDiver> It(World); It && ClearCm > 0.f; ++It)
		{
			const FVector Eye = It->GetPawnViewLocation();
			for (int32 k = 0; k < Rings; ++k)
			{
				const FVector D = Centres[k] - Eye;
				const float Dist = static_cast<float>(D.Size());
				if (Dist < ClearCm)
				{
					Centres[k] = Eye + D.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector) * ClearCm;
				}
			}
		}
	}

	// ---- Frames: parallel transport from the head's up, pulled gently back to world up, plus the bank.
	TubeVerts.SetNumUninitialized(Rings * Row);
	TubeUV0.SetNumUninitialized(Rings * Row);
	TubeUV1.SetNumUninitialized(Rings * Row);
	TubeColors.SetNumUninitialized(Rings * Row);
	FVector Up = Socket.GetUnitAxis(EAxis::Z);
	const float Bank = Tuning.TurnRollDeg * FMath::Clamp(YawRateDegPerS / 90.f, -1.5f, 1.5f);
	TArray<FVector, TInlineAllocator<128>> RingUps, RingTangents;
	TArray<float, TInlineAllocator<128>> RingAlongs;
	RingUps.SetNum(Rings);
	RingTangents.SetNum(Rings);
	RingAlongs.SetNum(Rings);
	for (int32 k = 0; k < Rings; ++k)
	{
		const float T = k / static_cast<float>(Rings - 1);
		const FVector Tangent = (Centres[FMath::Min(Rings - 1, k + 1)] - Centres[FMath::Max(0, k - 1)]).GetSafeNormal(UE_SMALL_NUMBER, SocketBack);
		Up = (Up - Tangent * FVector::DotProduct(Up, Tangent)).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		const FVector WorldUp = (FVector::UpVector - Tangent * FVector::DotProduct(FVector::UpVector, Tangent)).GetSafeNormal(UE_SMALL_NUMBER, Up);
		Up = FMath::Lerp(Up, WorldUp, 0.08f).GetSafeNormal(UE_SMALL_NUMBER, WorldUp);

		// Bank: the head's turn at the neck, then each ring into the body's own curve (its top tilts to the inside).
		const float RollDeg = Bank * FMath::Square(1.f - T);
		FVector RingUp = FQuat(Tangent, FMath::DegreesToRadians(RollDeg)).RotateVector(Up);
		{
			const int32 A = FMath::Max(0, k - 3);
			const int32 B = FMath::Min(Rings - 1, k + 3);
			const FVector In = Centres[k] - Centres[A];
			const FVector Out = Centres[B] - Centres[k];
			const double Len = 0.5 * (In.Size() + Out.Size());
			if (A < k && k < B && Len > 1.0)
			{
				const FVector Turn = Out.GetSafeNormal() - In.GetSafeNormal();
				const FVector Across = FVector::CrossProduct(RingUp, Tangent).GetSafeNormal();
				// Curvature (1/m) toward Across: positive when the curve's inside is that way.
				const double CurvePerM = FVector::DotProduct(Turn, Across) / (Len / CmPerM);
				const float CurveRoll = FMath::Clamp(static_cast<float>(CurvePerM) * Tuning.Chain.CurveRollDegPerInvM,
					-Tuning.Chain.MaxRollDeg, Tuning.Chain.MaxRollDeg) * FMath::SmoothStep(0.f, 0.08f, T);
				const float R = FMath::DegreesToRadians(CurveRoll);
				RingUp = (RingUp * FMath::Cos(R) + Across * FMath::Sin(R)).GetSafeNormal(UE_SMALL_NUMBER, RingUp);
			}
		}
		const FVector RingSide = FVector::CrossProduct(RingUp, Tangent);
		RingUps[k] = RingUp;
		RingTangents[k] = Tangent;

		const float AlongM = StartM + RingAlong(T) * LengthM;
		float RadiusM = k == 0 ? Tuning.NeckRadiusM : RadiusAt(Tuning, T);
		float Tall = 1.f;
		float Wide = 1.f;
		if (bMeshHead)
		{
			// Grows from inside the neck to over it at the cut, then the normal body; taller than wide near the head.
			const float BehindCutM = AlongM - HeadBack;
			const float Ease = FMath::SmoothStep(0.f, FMath::Max(0.1f, MeshTuning.NeckEaseM), BehindCutM);
			const float Sleeve = BehindCutM < 0.f
				? FMath::Lerp(MeshTuning.SleeveStartScale, MeshTuning.SleeveEndScale, FMath::SmoothStep(-FrontM, 0.f, BehindCutM))
				: FMath::Lerp(MeshTuning.SleeveEndScale, 1.f, Ease);
			const float Thin = BehindCutM < 0.f ? 1.f : FMath::Lerp(1.f, MeshTuning.BodyRadiusScale, Ease);
			RadiusM = RadiusAt(Tuning, FMath::Clamp(BehindCutM / BodyM, 0.f, 1.f), MeshTuning.BodySwell) * Sleeve * Thin;
			Tall = FMath::Lerp(MeshTuning.NeckTall, 1.f, Ease);
			Wide = FMath::Lerp(MeshTuning.NeckWide, 1.f, Ease);
		}
		FRingShape Shape;
		Shape.HalfWidth = RadiusM * Wide;
		Shape.Top = RadiusM * Tall;
		Shape.Bottom = RadiusM * 0.92f * Tall;
		Shape.Ridge = Tuning.RidgeHeight * (1.f - 0.6f * T);
		const float U = AlongM - HeadBack;
		RingAlongs[k] = AlongM;
		for (int32 j = 0; j <= Sides; ++j)
		{
			const FVector Local = RingPoint(Shape, j, Sides) * CmPerM;
			const int32 Index = k * Row + j;
			TubeVerts[Index] = Centres[k] - Origin + RingSide * Local.Y + RingUp * Local.Z;
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

	// ---- Dorsal spikes (mesh head only): the dragon's neck spikes carried on down the ridge, swept back, shrinking.
	TArray<FVector> SpikeV, SpikeN;
	TArray<FProcMeshTangent> SpikeT;
	TArray<FVector2D> SpikeUV;
	TArray<FLinearColor> SpikeC;
	TArray<int32> SpikeTri;
	if (bMeshHead && MeshTuning.SpikeSpacingM > 0.01f && MeshTuning.SpikeRunM > 0.f)
	{
		const int32 Count = FMath::Min(200, FMath::FloorToInt(MeshTuning.SpikeRunM / MeshTuning.SpikeSpacingM));
		const int32 Quarter = Sides / 4;
		int32 k = 0;
		for (int32 i = 0; i < Count; ++i)
		{
			const float D = (i + 0.5f) * MeshTuning.SpikeSpacingM;
			const float Along = HeadBack + D;
			while (k + 2 < Rings && RingAlongs[k + 1] < Along)
			{
				++k;
			}
			const float A = FMath::Clamp((Along - RingAlongs[k]) / FMath::Max(0.001f, RingAlongs[k + 1] - RingAlongs[k]), 0.f, 1.f);
			const FVector Top = FMath::Lerp(TubeVerts[k * Row + Quarter], TubeVerts[(k + 1) * Row + Quarter], A);
			const FVector SpikeUp = FMath::Lerp(RingUps[k], RingUps[k + 1], A).GetSafeNormal();
			const FVector Back = FMath::Lerp(RingTangents[k], RingTangents[k + 1], A).GetSafeNormal();
			// Deterministic jitter so the row is not a comb.
			const float Rnd = FMath::Frac(FMath::Sin(i * 12.9898f) * 43758.5453f);
			const float LenCm = MeshTuning.SpikeLengthM * CmPerM * FMath::Lerp(1.f, 0.2f, D / MeshTuning.SpikeRunM) * FMath::Lerp(0.7f, 1.15f, Rnd);
			const FVector Lean = FVector::CrossProduct(SpikeUp, Back) * (FMath::Frac(Rnd * 7.13f) - 0.5f) * 0.5f;
			AddTooth(Top - SpikeUp * 3.f, SpikeUp + Back * 0.9f + Lean, LenCm, LenCm * 0.18f, SpikeV, SpikeN, SpikeT, SpikeUV, SpikeC, SpikeTri);
		}
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
		if (SpikeV.Num() > 0)
		{
			Tube->CreateMeshSection_LinearColor(2, SpikeV, SpikeTri, SpikeN, SpikeUV, SpikeUV, None, None, SpikeC, SpikeT, false, false);
			if (UMaterialInstanceDynamic* SpikeSkin = SpikeMID ? SpikeMID.Get() : SkinMID.Get())
			{
				Tube->SetMaterial(2, SpikeSkin);
			}
		}
		else
		{
			Tube->ClearMeshSection(2);
		}
		bTubeCreated = true;
	}
	else
	{
		Tube->UpdateMeshSection_LinearColor(0, TubeVerts, TubeNormals, TubeUV0, TubeUV1, None, None, TubeColors, Tangents, false);
		Tube->UpdateMeshSection_LinearColor(1, ShellVerts, TubeNormals, TubeUV0, TubeUV1, None, None, TubeColors, Tangents, false);
		if (SpikeV.Num() > 0)
		{
			Tube->UpdateMeshSection_LinearColor(2, SpikeV, SpikeN, SpikeUV, SpikeUV, None, None, SpikeC, SpikeT, false);
		}
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
