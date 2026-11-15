#include "World/DeepAbyssFloor.h"

#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepShapes.h"

namespace
{
	/** How far below its centre a kind of salvage touches the bed (m): flattened scrap, round pearls, the tall relic. */
	float HalfHeightM(const FDeepSalvageKind& Kind)
	{
		switch (Kind.Size)
		{
		case EDeepSalvageSize::Pearl: return Kind.SizeM * 0.5f;
		case EDeepSalvageSize::Relic: return Kind.SizeM * 1.1f;
		default: return Kind.SizeM * 0.35f;
		}
	}

	/** Base shape (m below the anchor): steep near the sub, easing out to a slow fall into the dark. */
	float BaseBelowM(float RadiusM, const FDeepLookTuning& Look)
	{
		// Rounded under the sub (no point at the centre).
		RadiusM = FMath::Sqrt(RadiusM * RadiusM + 36.f) - 6.f;
		const float Near = FMath::Min(RadiusM, Look.SeabedSlopeBreakM);
		const float Far = FMath::Max(0.f, RadiusM - Look.SeabedSlopeBreakM);
		return Look.SeabedBaseBelowM + Near * Look.SeabedSlopeNear + Far * Look.SeabedSlopeFar;
	}

	/** Gentle rolling: a few crossed long waves (m), different in every direction. */
	float Roll(const FVector2D& P)
	{
		return 0.45f * FMath::Sin(0.19f * P.X + 0.7f) * FMath::Sin(0.15f * P.Y + 1.3f)
			+ 0.3f * FMath::Sin(0.083f * P.X - 0.121f * P.Y + 2.1f)
			+ 0.15f * FMath::Sin(0.41f * P.X + 0.29f * P.Y)
			+ 0.1f * FMath::Sin(0.67f * P.Y - 0.53f * P.X + 0.4f);
	}

	/** Grid coordinate: about 0.5 m cells near the anchor, growing toward the edge. T in [-1, 1]. */
	double GridM(double T, double HalfWidthM)
	{
		const double A = FMath::Abs(T);
		return FMath::Sign(T) * HalfWidthM * (0.25 * A + 0.75 * A * A * A);
	}
}

ADeepAbyssFloor::ADeepAbyssFloor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	Bed = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Bed"));
	Bed->SetupAttachment(Root);
	Bed->SetMobility(EComponentMobility::Movable);
	Bed->bUseAsyncCooking = true;
	Bed->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Bed->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

float ADeepAbyssFloor::SeabedBelowM(const FVector2D& XyM, const TArray<FVector4>& RestM, const FDeepLookTuning& Look)
{
	// Blend of the base shape (weight 1) and each resting point: a flat-topped bump of weight 60 (exp(-d^4)), so the bed is
	// level for about 0.7 R round a resting point (within 2% of its height) and eases back to the base by 1.4 R. Inverse
	// distance weights made a sharp spike at every point.
	double Sum = BaseBelowM(XyM.Size(), Look);
	double Weight = 1.0;
	double NearestSq = TNumericLimits<double>::Max();
	for (const FVector4& Rest : RestM)
	{
		const double R = FMath::Max(0.5, FMath::Abs(Rest.W));
		const double DSq = FVector2D::DistSquared(XyM, FVector2D(Rest.X, Rest.Y)) / (R * R);
		NearestSq = FMath::Min(NearestSq, DSq);
		if (Rest.W < 0.f)
		{
			continue;
		}
		const double W = 60.0 * FMath::Exp(-DSq * DSq);
		Sum += W * Rest.Z;
		Weight += W;
	}
	// The roll fades out near a resting point so salvage lies exactly on the bed.
	const double Keep = RestM.IsEmpty() ? 1.0 : 1.0 - FMath::Exp(-NearestSq * 2.5);
	double Below = Sum / Weight - Look.SeabedRollM * Roll(XyM) * Keep;
	// A sinkhole (negative radius) cuts into the blended bed: exact at its centre, gone by about 1.4 radii, so a
	// neighbouring spot's bump cannot lift its floor.
	for (const FVector4& Rest : RestM)
	{
		if (Rest.W < 0.f)
		{
			const double R = FMath::Max(0.5, -Rest.W);
			const double DSq = FVector2D::DistSquared(XyM, FVector2D(Rest.X, Rest.Y)) / (R * R);
			Below = FMath::Lerp(Below, double(Rest.Z), FMath::Exp(-DSq * DSq));
		}
	}
	return float(Below);
}

FVector4 ADeepAbyssFloor::RestPointM(const FVector& OffsetCm, float HalfM, float RadiusM, const FDeepLookTuning& Look)
{
	const FVector M = OffsetCm / UDeepRules::CmPerM;
	// Settled a little into the silt.
	const float Sink = 2.f * HalfM * FMath::Clamp(Look.SeabedSinkFraction, 0.f, 0.5f);
	return FVector4(M.X, M.Y, -M.Z + HalfM - Sink, RadiusM);
}

TArray<FVector4> ADeepAbyssFloor::RestPointsFromTuning()
{
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	TArray<FVector4> Out;
	for (const FDeepSalvageSpot& Spot : Tuning.Arena.SalvageSpots)
	{
		FDeepSalvageKind Kind;
		if (Tuning.FindSalvageKind(Spot.Size, Kind))
		{
			Out.Add(RestPointM(UDeepRules::SalvageSpotOffsetCm(Spot), HalfHeightM(Kind), Tuning.Look.SeabedRestRadiusM, Tuning.Look));
		}
	}
	// The relic exists only at its stop; its sinkhole is there at every stop (the bed is one surface).
	FDeepSalvageKind Relic;
	if (Tuning.FindSalvageKind(EDeepSalvageSize::Relic, Relic))
	{
		const int32 Stop = Tuning.Arena.RelicStop;
		const FVector Offset = UDeepRules::RelicOffsetCm(Tuning.Arena, Tuning.GetStop(Stop).SubDepthM, UDeepRules::TetherLengthM(Stop, Tuning.Tether));
		// Negative radius: a sinkhole that cuts into the bed rather than joining the blend.
		Out.Add(RestPointM(Offset, HalfHeightM(Relic), -Tuning.Look.SeabedPitRadiusM, Tuning.Look));
	}
	return Out;
}

void ADeepAbyssFloor::BeginPlay()
{
	Super::BeginPlay();
	Build(RestPointsFromTuning());
}

void ADeepAbyssFloor::Build(const TArray<FVector4>& RestM)
{
	const FDeepLookTuning& Look = UDeepTuningSettings::Get().Look;
	const int32 Cells = FMath::Clamp(Look.SeabedCells, 8, 600);
	const int32 Side = Cells + 1;
	const double Half = FMath::Max(10.f, Look.SeabedHalfWidthM);
	const double Tile = FMath::Max(0.1f, Look.SeabedUvTileM);

	TArray<double> Coord;
	Coord.SetNum(Side);
	for (int32 i = 0; i < Side; ++i)
	{
		Coord[i] = GridM(-1.0 + 2.0 * i / Cells, Half);
	}

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FProcMeshTangent> Tangents;
	TArray<int32> Triangles;
	Vertices.Reserve(Side * Side);
	Normals.Reserve(Side * Side);
	UV0.Reserve(Side * Side);
	Tangents.Reserve(Side * Side);
	Triangles.Reserve(Cells * Cells * 6);

	const double Eps = 0.25;
	for (int32 j = 0; j < Side; ++j)
	{
		for (int32 i = 0; i < Side; ++i)
		{
			const FVector2D P(Coord[i], Coord[j]);
			const double Z = -SeabedBelowM(P, RestM, Look);
			// Normal from the height field's slope (central differences in metres).
			const double Dx = (-SeabedBelowM(P + FVector2D(Eps, 0.0), RestM, Look) + SeabedBelowM(P - FVector2D(Eps, 0.0), RestM, Look)) / (2.0 * Eps);
			const double Dy = (-SeabedBelowM(P + FVector2D(0.0, Eps), RestM, Look) + SeabedBelowM(P - FVector2D(0.0, Eps), RestM, Look)) / (2.0 * Eps);
			Vertices.Add(FVector(P.X, P.Y, Z) * UDeepRules::CmPerM);
			Normals.Add(FVector(-Dx, -Dy, 1.0).GetSafeNormal());
			Tangents.Add(FProcMeshTangent(FVector(1.0, 0.0, Dx).GetSafeNormal(), false));
			UV0.Add(P / Tile);
		}
	}
	for (int32 j = 0; j < Cells; ++j)
	{
		for (int32 i = 0; i < Cells; ++i)
		{
			const int32 V00 = j * Side + i;
			const int32 V10 = V00 + 1;
			const int32 V01 = V00 + Side;
			const int32 V11 = V01 + 1;
			// Faces up: Unreal's face normal is (P1 - P2) x (P0 - P2). Wound the other way, the two-sided silt shades the
			// flipped normal of the underside and the bed is black under every light.
			Triangles.Append({ V00, V01, V10, V10, V01, V11 });
		}
	}

	Bed->ClearAllMeshSections();
	Bed->CreateMeshSection(0, Vertices, Triangles, Normals, UV0, TArray<FColor>(), Tangents, /*bCreateCollision*/ true);
	Bed->SetCastShadow(true);

	UMaterialInterface* Silt = Look.SeabedMaterial.IsNull() ? nullptr : Cast<UMaterialInterface>(Look.SeabedMaterial.TryLoad());
	if (Silt)
	{
		Bed->SetMaterial(0, Silt);
	}
	else
	{
		// Fallback: the basic material tinted to wet silt (dark, a little warm).
		if (UMaterialInterface* Basic = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			UMaterialInstanceDynamic* Tinted = UMaterialInstanceDynamic::Create(Basic, Bed);
			Tinted->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.035f, 0.032f, 0.028f));
			Bed->SetMaterial(0, Tinted);
		}
		UE_LOG(LogTemp, Warning, TEXT("DEEPLOOK seabed material missing (%s): tinted fallback"), *Look.SeabedMaterial.ToString());
	}
	UE_LOG(LogTemp, Display, TEXT("DEEPLOOK seabed %d vertices, %d rest points, %.0f m wide, at %s, bounds %s, material %s, centre %.1f m below the anchor"),
		Vertices.Num(), RestM.Num(), Half * 2.0, *GetActorLocation().ToString(), *Bed->Bounds.GetBox().ToString(),
		*GetNameSafe(Bed->GetMaterial(0)), SeabedBelowM(FVector2D::ZeroVector, RestM, Look));
}
