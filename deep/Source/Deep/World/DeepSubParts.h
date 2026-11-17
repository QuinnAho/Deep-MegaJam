// Shared building blocks for the sub's moving hull (ADeepSubmarine) and its fixed room set (ADeepSubRoom): solid
// shapes, placing a basic shape by centre and size, the worn-steel paints and the screens. Both share one local frame:
// X ahead, Y starboard, Z up, the room's centre at the origin and the deck's top at -RoomHeightM / 2.

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
	// The helm window: an opening in the front wall (local cm, centre height and half sizes).
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

	/** The four deck slabs round the hatch opening, the seven wall pieces round the window, the ceiling (local cm). */
	inline void LayOutShell(const TArray<TObjectPtr<UStaticMeshComponent>>& Deck, const TArray<TObjectPtr<UStaticMeshComponent>>& Walls, UStaticMeshComponent* Ceiling)
	{
		const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
		const double M = UDeepRules::CmPerM;
		const double L = T.RoomHalfLengthM * M;
		const double W = T.RoomHalfWidthM * M;
		const double H = T.RoomHeightM * M;
		const double Th = T.WallThicknessM * M;
		const double Hh = T.HatchHalfSizeM * M;
		const double DeckZ = -H * 0.5 - Th * 0.5;
		const double OuterL = L + Th;
		const double OuterW = W + Th;
		if (Deck.Num() == 4)
		{
			Place(Deck[0], FVector((Hh + OuterL) * 0.5, 0.0, DeckZ), FVector(OuterL - Hh, OuterW * 2.0, Th));
			Place(Deck[1], FVector(-(Hh + OuterL) * 0.5, 0.0, DeckZ), FVector(OuterL - Hh, OuterW * 2.0, Th));
			Place(Deck[2], FVector(0.0, (Hh + OuterW) * 0.5, DeckZ), FVector(Hh * 2.0, OuterW - Hh, Th));
			Place(Deck[3], FVector(0.0, -(Hh + OuterW) * 0.5, DeckZ), FVector(Hh * 2.0, OuterW - Hh, Th));
		}
		if (Walls.Num() == 7)
		{
			const double WinBottom = WinZ - WinHalfH;
			const double WinTop = WinZ + WinHalfH;
			Place(Walls[0], FVector(L + Th * 0.5, 0.0, (-H * 0.5 + WinBottom) * 0.5), FVector(Th, OuterW * 2.0, WinBottom + H * 0.5));
			Place(Walls[1], FVector(-L - Th * 0.5, 0.0, 0.0), FVector(Th, OuterW * 2.0, H));
			Place(Walls[2], FVector(0.0, W + Th * 0.5, 0.0), FVector(L * 2.0, Th, H));
			Place(Walls[3], FVector(0.0, -W - Th * 0.5, 0.0), FVector(L * 2.0, Th, H));
			Place(Walls[4], FVector(L + Th * 0.5, 0.0, (WinTop + H * 0.5) * 0.5), FVector(Th, OuterW * 2.0, H * 0.5 - WinTop));
			Place(Walls[5], FVector(L + Th * 0.5, -(WinHalfW + OuterW) * 0.5, WinZ), FVector(Th, OuterW - WinHalfW, WinHalfH * 2.0));
			Place(Walls[6], FVector(L + Th * 0.5, (WinHalfW + OuterW) * 0.5, WinZ), FVector(Th, OuterW - WinHalfW, WinHalfH * 2.0));
		}
		if (Ceiling)
		{
			Place(Ceiling, FVector(0.0, 0.0, H * 0.5 + Th * 0.5), FVector(OuterL * 2.0, OuterW * 2.0, Th));
		}
	}

	/** The hatch's rim: four bars through the deck, so the frame shows from above and below (local cm). */
	inline void LayOutRim(const TArray<TObjectPtr<UStaticMeshComponent>>& Rim)
	{
		const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
		const double H = T.RoomHeightM * UDeepRules::CmPerM;
		const double Th = T.WallThicknessM * UDeepRules::CmPerM;
		const double Hh = T.HatchHalfSizeM * UDeepRules::CmPerM;
		const double RimW = 16.0;
		const double RimH = 6.0;
		const double RimZ = -H * 0.5 - Th * 0.5;
		const double RimT = Th + RimH * 2.0;
		if (Rim.Num() == 4)
		{
			Place(Rim[0], FVector(0.0, Hh + RimW * 0.5, RimZ), FVector(Hh * 2.0 + RimW * 2.0, RimW, RimT));
			Place(Rim[1], FVector(0.0, -Hh - RimW * 0.5, RimZ), FVector(Hh * 2.0 + RimW * 2.0, RimW, RimT));
			Place(Rim[2], FVector(Hh + RimW * 0.5, 0.0, RimZ), FVector(RimW, Hh * 2.0, RimT));
			Place(Rim[3], FVector(-Hh - RimW * 0.5, 0.0, RimZ), FVector(RimW, Hh * 2.0, RimT));
		}
	}
}
