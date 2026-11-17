// Shared building blocks for the sub's moving hull (ADeepSubmarine) and its fixed room set (ADeepSubRoom): solid
// shapes, placing a basic shape by centre and size, the worn-steel paints and the screens. Local frame: X ahead,
// Y starboard, Z up, the hatch's centre over the origin. The hull's box deck is at -RoomHeightM / 2; the room set has
// its own, smaller inside (World/DeepRoomLayout, the Sub|Inside tuning).

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepShapes.h"

namespace DeepSubParts
{
	// The hull's helm window: an opening in the box hull's front wall (local cm, centre height and half sizes).
	constexpr double WinZ = 40.0;
	constexpr double WinHalfW = 85.0;
	constexpr double WinHalfH = 35.0;

	inline void Solid(UStaticMeshComponent* Shape)
	{
		Shape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Shape->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}

	/** Centre and size in cm, relative to the parent. */
	inline void Place(UStaticMeshComponent* Shape, const FVector& CentreCm, const FVector& SizeCm)
	{
		Shape->SetRelativeLocation(CentreCm);
		Shape->SetRelativeScale3D(SizeCm / DeepShapes::ShapeSizeCm);
	}

	/** Worn painted steel (M_WornSteel) with this paint; falls back to a flat tint if the asset is missing. */
	struct FSubSteel
	{
		FLinearColor Paint;
		float Wear = 0.22f;
		float Rust = 0.3f;
		float Wet = 0.f;
		float PaintRough = 0.62f;
		float PaintMetal = 0.f;
	};

	/** The paints both halves use (style guide 4: every value at most 35% saturation and 70% value). */
	struct FSubPaints
	{
		FSubSteel Wall, Floor, Ceil, Rim, Cover, Machine, Seat;
	};

	inline FSubPaints Paints()
	{
		FSubPaints P;
		P.Wall.Paint = FLinearColor(0.046f, 0.054f, 0.05f);
		P.Wall.Wear = 0.2f;
		P.Wall.Rust = 0.32f;
		P.Wall.Wet = 0.15f;
		P.Wall.PaintRough = 0.55f;
		P.Floor.Paint = FLinearColor(0.035f, 0.036f, 0.036f);
		P.Floor.Wear = 0.32f;
		P.Floor.Rust = 0.25f;
		P.Floor.Wet = 0.2f;
		P.Floor.PaintRough = 0.5f;
		P.Ceil = P.Wall;
		P.Ceil.Paint = FLinearColor(0.03f, 0.033f, 0.032f);
		P.Ceil.Wet = 0.f;
		P.Ceil.PaintRough = 0.7f;
		P.Rim.Paint = FLinearColor(0.12f, 0.10f, 0.06f);
		P.Rim.Wear = 0.38f;
		P.Rim.Rust = 0.35f;
		P.Rim.Wet = 0.4f;
		P.Cover = P.Rim;
		P.Cover.Paint = FLinearColor(0.10f, 0.05f, 0.045f);
		P.Machine.Paint = FLinearColor(0.03f, 0.032f, 0.034f);
		P.Machine.Wear = 0.15f;
		P.Machine.Rust = 0.15f;
		P.Machine.PaintRough = 0.5f;
		P.Seat = P.Machine;
		P.Seat.Paint = FLinearColor(0.05f, 0.04f, 0.035f);
		P.Seat.PaintRough = 0.8f;
		return P;
	}

	inline void SetSteelParams(UMaterialInstanceDynamic* M, const FSubSteel& S)
	{
		M->SetVectorParameterValue(TEXT("PaintColor"), S.Paint);
		M->SetScalarParameterValue(TEXT("Wear"), S.Wear);
		M->SetScalarParameterValue(TEXT("Rust"), S.Rust);
		M->SetScalarParameterValue(TEXT("Wet"), S.Wet);
		M->SetScalarParameterValue(TEXT("PaintRoughness"), S.PaintRough);
		M->SetScalarParameterValue(TEXT("PaintMetal"), S.PaintMetal);
	}

	inline void SubSteel(UStaticMeshComponent* Shape, const FSubSteel& S, const FVector& PuddleCentre = FVector::ZeroVector, float PuddleRadius = 0.f)
	{
		UMaterialInterface* Base = Cast<UMaterialInterface>(UDeepTuningSettings::Get().Look.SteelMaterial.TryLoad());
		if (!Shape || !Base)
		{
			DeepShapes::Tint(Shape, S.Paint);
			return;
		}
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Shape);
		SetSteelParams(M, S);
		M->SetVectorParameterValue(TEXT("PuddleCentre"), FLinearColor(PuddleCentre.X, PuddleCentre.Y, PuddleCentre.Z));
		M->SetScalarParameterValue(TEXT("PuddleRadius"), PuddleRadius);
		Shape->SetMaterial(0, M);
	}

	/** One shared instance of the worn steel with this paint, or null when the asset is missing. */
	inline UMaterialInstanceDynamic* SteelInstance(UObject* Outer, const FSubSteel& S)
	{
		UMaterialInterface* Base = Cast<UMaterialInterface>(UDeepTuningSettings::Get().Look.SteelMaterial.TryLoad());
		if (!Base)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Outer);
		SetSteelParams(M, S);
		M->SetScalarParameterValue(TEXT("PuddleRadius"), 0.f);
		return M;
	}

	inline void SubScreen(UStaticMeshComponent* Shape, float Glow)
	{
		UMaterialInterface* Base = Cast<UMaterialInterface>(UDeepTuningSettings::Get().Look.ScreenMaterial.TryLoad());
		if (!Shape || !Base)
		{
			DeepShapes::Tint(Shape, FLinearColor(0.6f, 0.3f, 0.08f));
			return;
		}
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Shape);
		M->SetScalarParameterValue(TEXT("Glow"), Glow);
		Shape->SetMaterial(0, M);
	}
}
