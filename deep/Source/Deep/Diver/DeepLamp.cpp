#include "Diver/DeepLamp.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Sim/DeepTuningSettings.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Monster/DeepSerpent.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningTypes.h"
#include "World/DeepAbyssFloor.h"
#include "World/DeepArena.h"
#include "World/DeepDressing.h"

namespace
{
	TAutoConsoleVariable<int32> CVarDeepLampNear(
		TEXT("Deep.Lamp.Near"), 2,
		TEXT("The lamp's close-range dimming: 2 = the whole cone (default), 1 = the old single ray on the axis (before/after frames), 0 = none."));

	/** The old floor, kept only for Deep.Lamp.Near 1 (the before frames). */
	constexpr float OldNearMinScale = 0.06f;

	/** Below this facing a surface still counts a little: wet stone and skin glint at a graze (specular is not Lambert). */
	constexpr float MinFacing = 0.3f;

	/** Ray directions across the cone: the axis, a ring at the inner cone (full strength) and one halfway to the outer edge. */
	void ConeRays(const USpotLightComponent* Lamp, const FDeepLampTuning& Tuning, TArray<FVector, TInlineAllocator<16>>& Out)
	{
		const FVector Axis = Lamp->GetForwardVector();
		const FVector Right = Lamp->GetRightVector();
		const FVector Up = Lamp->GetUpVector();
		Out.Add(Axis);
		const float Inner = FMath::Clamp(Tuning.InnerConeDeg, 1.f, 80.f);
		const float Mid = FMath::Clamp((Tuning.InnerConeDeg + Tuning.OuterConeDeg) * 0.5f, Inner, 85.f);
		const float Rings[2] = { Inner, Mid };
		for (int32 r = 0; r < 2; ++r)
		{
			const float Off = FMath::DegreesToRadians(Rings[r]);
			for (int32 i = 0; i < 6; ++i)
			{
				const float Around = FMath::DegreesToRadians(60.f * i + 30.f * r);
				const FVector Side = Right * FMath::Cos(Around) + Up * FMath::Sin(Around);
				Out.Add((Axis * FMath::Cos(Off) + Side * FMath::Sin(Off)).GetSafeNormal());
			}
		}
	}

	/**
	 * Where a ray first meets the sea bed, from the bed's own height field (ADeepAbyssFloor::SeabedBelowM, the shape its
	 * mesh is built from), as well as the rays above: a trace that starts inside the bed (the pit is a steep funnel, and
	 * a diver at the relic can be in its slope) never sees it. Coarse steps, then bisection to under 1 cm.
	 * Returns the distance in cm (or -1) and the bed's normal there (world).
	 */
	float MarchBed(const FTransform& BedToWorld, const TArray<FVector4>& Rest, const FDeepLookTuning& Look, const FVector& From, const FVector& Dir, float ReachCm, FVector& OutNormal)
	{
		const auto Below = [&](const FVector& WorldCm)
		{
			const FVector L = BedToWorld.InverseTransformPosition(WorldCm);
			// Positive when the point is under the bed's surface.
			return -ADeepAbyssFloor::SeabedBelowM(FVector2D(L.X, L.Y) / UDeepRules::CmPerM, Rest, Look) * UDeepRules::CmPerM - L.Z;
		};
		constexpr float StepCm = 20.f;
		float Prev = 0.f;
		if (Below(From) > 0.f)
		{
			// The lamp, held 20 cm ahead and off to the side, is already in the slope (a diver with his face to the pit's
			// wall): the surface is right at it, so the lamp goes to its floor rather than full (which burnt it white).
			OutNormal = -Dir;
			return 0.f;
		}
		for (float T = StepCm; T <= ReachCm + 0.5f * StepCm; T += StepCm)
		{
			if (Below(From + Dir * T) > 0.f)
			{
				float Lo = Prev, Hi = T;
				for (int32 i = 0; i < 5; ++i)
				{
					const float Mid = 0.5f * (Lo + Hi);
					(Below(From + Dir * Mid) > 0.f ? Hi : Lo) = Mid;
				}
				const FVector L = BedToWorld.InverseTransformPosition(From + Dir * Hi);
				const FVector2D P(L.X / UDeepRules::CmPerM, L.Y / UDeepRules::CmPerM);
				constexpr float Eps = 0.1f;
				const float Dx = -(ADeepAbyssFloor::SeabedBelowM(P + FVector2D(Eps, 0.f), Rest, Look) - ADeepAbyssFloor::SeabedBelowM(P - FVector2D(Eps, 0.f), Rest, Look)) / (2.f * Eps);
				const float Dy = -(ADeepAbyssFloor::SeabedBelowM(P + FVector2D(0.f, Eps), Rest, Look) - ADeepAbyssFloor::SeabedBelowM(P - FVector2D(0.f, Eps), Rest, Look)) / (2.f * Eps);
				OutNormal = BedToWorld.TransformVectorNoScale(FVector(-Dx, -Dy, 1.f).GetSafeNormal());
				return Hi;
			}
			Prev = T;
		}
		return -1.f;
	}
}

int32 DeepLamp::NearMode()
{
	return CVarDeepLampNear.GetValueOnGameThread();
}

float DeepLamp::WantNearScale(const AActor* Holder, const USpotLightComponent* Lamp, const FDeepLampTuning& Tuning)
{
	UWorld* World = Holder ? Holder->GetWorld() : nullptr;
	const int32 Mode = NearMode();
	if (!World || !Lamp || Mode <= 0 || Tuning.NearFullM <= 0.f)
	{
		return 1.f;
	}
	const FVector From = Lamp->GetComponentLocation();
	const FVector Axis = Lamp->GetForwardVector();
	const float ReachCm = Tuning.NearFullM * UDeepRules::CmPerM;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DeepLampNear), false, Holder);

	if (Mode == 1)
	{
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, From, From + Axis * ReachCm, ECC_Visibility, Params))
		{
			return FMath::Max(UDeepRules::LampNearScale(Hit.Distance / UDeepRules::CmPerM, Tuning), OldNearMinScale);
		}
		return 1.f;
	}

	TArray<FVector, TInlineAllocator<16>> Rays;
	ConeRays(Lamp, Tuning, Rays);
	float Brightest = 0.f;

	// 1. Whatever has collision (the bed and the well, salvage, the hull): rays across the cone.
	for (const FVector& Dir : Rays)
	{
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, From, From + Dir * ReachCm, ECC_Visibility, Params))
		{
			const float Facing = FMath::Max(MinFacing, static_cast<float>(FVector::DotProduct(Hit.ImpactNormal, -Dir)));
			Brightest = FMath::Max(Brightest, UDeepRules::LampSampleBrightness(Hit.Distance / UDeepRules::CmPerM, FVector::DotProduct(Dir, Axis), Facing, Tuning));
		}
	}

	// 1b. The sea bed and the well, from the bed's height field (also where a trace misses it: see MarchBed).
	// Only the loaded stop's bed (Deep.World.Still has one per stop), found once.
	if (const ADeepAbyssFloor* Bed = DeepArena::FindLoadedFloor(World))
	{
		const TArray<FVector4> Rest = ADeepAbyssFloor::RestPointsFromTuning();
		const FDeepLookTuning& Look = UDeepTuningSettings::Get().Look;
		const FTransform BedToWorld = Bed->GetActorTransform();
		for (const FVector& Dir : Rays)
		{
			FVector Normal = FVector::UpVector;
			const float HitCm = MarchBed(BedToWorld, Rest, Look, From, Dir, ReachCm, Normal);
			if (HitCm >= 0.f)
			{
				const float Facing = FMath::Max(MinFacing, static_cast<float>(FVector::DotProduct(Normal, -Dir)));
				Brightest = FMath::Max(Brightest, UDeepRules::LampSampleBrightness(HitCm / UDeepRules::CmPerM, FVector::DotProduct(Dir, Axis), Facing, Tuning));
			}
		}
	}

	// 2. The serpent, which has no collision by design: its drawn body as spheres. The nearest point of each sphere, at
	// the smallest angle any of it makes with the axis; facing taken as full (wet skin).
	TArray<FSphere> Spheres;
	for (TActorIterator<ADeepSerpent> It(World); It; ++It)
	{
		Spheres.Reset();
		It->GetDrawnSpheres(Spheres);
		for (const FSphere& S : Spheres)
		{
			const FVector V = S.Center - From;
			const float L = static_cast<float>(V.Size());
			const float R = static_cast<float>(S.W);
			const float NearCm = L - R;
			if (NearCm > ReachCm)
			{
				continue;
			}
			float CosOff = 1.f;
			if (L > R && L > KINDA_SMALL_NUMBER)
			{
				const float Centre = FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(V / L, Axis)), -1.f, 1.f));
				const float Spread = FMath::Asin(FMath::Clamp(R / L, 0.f, 1.f));
				CosOff = FMath::Cos(FMath::Max(0.f, Centre - Spread));
			}
			Brightest = FMath::Max(Brightest, Tuning.NearSerpentReflectance * UDeepRules::LampSampleBrightness(FMath::Max(0.f, NearCm) / UDeepRules::CmPerM, CosOff, 1.f, Tuning));
		}
	}

	// 3. The pack rock and ruin, which have no collision either: each nearby instance's box against the same rays.
	// A box is a little bigger than its rock, so the lamp dims a touch early, never late. A lamp inside a box is skipped.
	TArray<UInstancedStaticMeshComponent*> Comps;
	for (TActorIterator<ADeepDressing> It(World); It; ++It)
	{
		Comps.Reset();
		It->GetComponents<UInstancedStaticMeshComponent>(Comps);
		for (const UInstancedStaticMeshComponent* Comp : Comps)
		{
			const UStaticMesh* Mesh = Comp ? Comp->GetStaticMesh() : nullptr;
			if (!Mesh || !Comp->IsVisible() || Comp->GetInstanceCount() == 0)
			{
				continue;
			}
			const FBox LocalBox = Mesh->GetBounds().GetBox();
			for (const int32 Index : Comp->GetInstancesOverlappingSphere(From, ReachCm, true))
			{
				FTransform Instance;
				if (!Comp->GetInstanceTransform(Index, Instance, true))
				{
					continue;
				}
				const FVector LocalFrom = Instance.InverseTransformPosition(From);
				if (LocalBox.IsInside(LocalFrom))
				{
					continue;
				}
				for (const FVector& Dir : Rays)
				{
					FVector HitLocation, HitNormal;
					float HitTime = 0.f;
					const FVector LocalTo = Instance.InverseTransformPosition(From + Dir * ReachCm);
					if (FMath::LineExtentBoxIntersection(LocalBox, LocalFrom, LocalTo, FVector::ZeroVector, HitLocation, HitNormal, HitTime))
					{
						Brightest = FMath::Max(Brightest, UDeepRules::LampSampleBrightness(HitTime * Tuning.NearFullM, FVector::DotProduct(Dir, Axis), 1.f, Tuning));
					}
				}
			}
		}
	}

	return UDeepRules::LampScaleForBrightest(Brightest, Tuning);
}

void DeepLamp::UpdateCarriedGlow(ADeepDiver& Diver)
{
	static const FName GlowName(TEXT("RelicGlow"));
	const UDeepRunComponent* Run = Diver.GetRun();
	const bool bWant = Run && Run->CarriesRelic();
	UPointLightComponent* Glow = nullptr;
	TInlineComponentArray<UPointLightComponent*> Lights(&Diver);
	for (UPointLightComponent* Light : Lights)
	{
		if (Light && Light->GetFName() == GlowName)
		{
			Glow = Light;
			break;
		}
	}
	if (!Glow)
	{
		if (!bWant || !Diver.GetRootComponent())
		{
			return;
		}
		const FDeepArenaTuning& Arena = UDeepTuningSettings::Get().Arena;
		Glow = NewObject<UPointLightComponent>(&Diver, GlowName);
		Glow->SetMobility(EComponentMobility::Movable);
		Glow->SetupAttachment(Diver.GetRootComponent());
		// In front of the chest, below the eye: it lights the shaft wall round the diver and the bottom of the view.
		Glow->SetRelativeLocation(FVector(25.f, 10.f, -25.f));
		Glow->SetCastShadows(false);
		Glow->SetIntensity(Arena.RelicLightIntensity * Arena.RelicCarriedLightFraction);
		Glow->SetAttenuationRadius(Arena.RelicCarriedLightRadiusM * UDeepRules::CmPerM);
		Glow->SetLightColor(Arena.RelicLightColor);
		Glow->SetVolumetricScatteringIntensity(Arena.RelicCarriedLightScattering);
		Glow->RegisterComponent();
	}
	if (Glow->IsVisible() != bWant)
	{
		Glow->SetVisibility(bWant);
	}
}

float DeepLamp::RayToBedCm(UWorld* World, const FVector& From, const FVector& Dir, float MaxCm)
{
	if (!World)
	{
		return -1.f;
	}
	const ADeepAbyssFloor* Bed = DeepArena::FindLoadedFloor(World);
	if (!Bed)
	{
		return -1.f;
	}
	FVector Normal;
	return MarchBed(Bed->GetActorTransform(), ADeepAbyssFloor::RestPointsFromTuning(), UDeepTuningSettings::Get().Look, From, Dir.GetSafeNormal(), MaxCm, Normal);
}
