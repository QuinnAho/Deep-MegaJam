// Coming home to the hatch from any side (S8-keys playtest, Nov 15): the winch routes round and under the hull, a
// swimmer pushing into the hull's side slides down it and under it, the hull's collision is only what is drawn (the
// box's cubes, or BT-01's hulls within 20 cm of its surface),
// and no straight walk between the room's stations crosses the open hatch.

#include "CoreMinimal.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Diver/DeepTetherComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepSubmarine.h"
#include "World/DeepHull.h"
#include "World/DeepArena.h"
#include "World/DeepSubRoom.h"
#include "World/DeepRoomDress.h"
#include "World/DeepRoomLayout.h"
#include "Camera/CameraComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_RETURN_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepReturnTest
{
	// The shipped hull, by hand: 4.7 x 2.45 x 1.7 m half extents; the diver's capsule is 35 x 80 cm.
	const FBox Hull(FVector(-470.f, -245.f, -170.f), FVector(470.f, 245.f, 170.f));
	constexpr float Radius = 35.f;
	constexpr float HalfHeight = 80.f;

	bool Overlaps(const FBox& Solid, const FVector& Centre, double RadiusCm, double HalfHeightCm)
	{
		// The capsule's box, 2 cm shy, against the solid: touching is fine, sinking in is not.
		const FVector Ext(RadiusCm - 2.0, RadiusCm - 2.0, HalfHeightCm - 2.0);
		return Solid.Intersect(FBox(Centre - Ext, Centre + Ext));
	}

	/** A game world with the arena and one diver in the room, dropped through the hatch (tethered). */
	struct FReturnWorld
	{
		UWorld* World = nullptr;
		ADeepSubmarine* Sub = nullptr;
		ADeepDiver* Diver = nullptr;
		UDeepRunComponent* Run = nullptr;

		FReturnWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			ADeepGameMode* GameMode = World->SpawnActor<ADeepGameMode>();
			Sub = GameMode ? GameMode->EnsureArena() : nullptr;
			if (GameMode)
			{
				GameMode->StartPlay();
			}
			if (Sub)
			{
				Diver = World->SpawnActor<ADeepDiver>(DeepArena::EnsureRoom(World)->GetRoomStartTransform().GetLocation(), FRotator::ZeroRotator);
			}
			Run = Diver ? Diver->GetRun() : nullptr;
			if (Run)
			{
				Run->StepRun(0.f);
			}
		}

		~FReturnWorld()
		{
			if (World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		bool Valid() const { return Sub && Diver && Run; }

		/** Through the hatch, then put in the water at a sub-local point (cm). */
		bool DiveTo(const FVector& LocalCm) const
		{
			if (Run->GetState() == EDeepRunState::Aboard && !Run->LeaveSub())
			{
				return false;
			}
			Diver->TeleportDiver(Sub->GetActorTransform().TransformPosition(LocalCm));
			Run->StepRun(0.f);
			return Run->GetState() == EDeepRunState::Tethered;
		}

		FVector LocalDiver() const { return Sub->GetActorTransform().InverseTransformPosition(Diver->GetActorLocation()); }
	};

	// Points round the sub (local cm) at different heights: behind at hatch height (the S8 stall), beside low, a low
	// far corner, over the tower, high off the side, hugging the side wall, and off the nose at mid height.
	const FVector Around[] = {
		FVector(-900.f, 0.f, -150.f), FVector(0.f, 900.f, -300.f), FVector(800.f, -700.f, -600.f), FVector(100.f, 0.f, 700.f),
		FVector(-500.f, 700.f, 400.f), FVector(-200.f, -290.f, 0.f), FVector(1100.f, 200.f, 50.f) };

	/** Deep.Sub.Model for one scope (set before the world is built; the old value back after). */
	struct FModelMode
	{
		IConsoleVariable* Var = nullptr;
		int32 Old = 1;
		explicit FModelMode(int32 Mode) : Var(IConsoleManager::Get().FindConsoleVariable(TEXT("Deep.Sub.Model")))
		{
			if (Var)
			{
				Old = Var->GetInt();
				Var->Set(Mode, ECVF_SetByCode);
			}
		}
		~FModelMode()
		{
			if (Var)
			{
				Var->Set(Old, ECVF_SetByCode);
			}
		}
	};

	/** The model hull's collision stays this close to its visible surface (cm), as the box did to its cubes. */
	constexpr double ModelFitCm = 20.0;

	/**
	 * BT-01's parts too thin for the hulls to follow, by material slot: a ray that meets one first is not measured.
	 * The game lets a swimmer brush through them, as through the old dress's rails.
	 */
	const TCHAR* const ThinParts[] = { TEXT("Rope"), TEXT("Propellers"), TEXT("mechanical_hands"), TEXT("drill") };

	bool IsThinMaterial(const UMaterialInterface* Material)
	{
		const FString Name = Material ? Material->GetName() : FString();
		for (const TCHAR* Thin : ThinParts)
		{
			if (Name.Contains(Thin))
			{
				return true;
			}
		}
		return false;
	}

	bool IsThinPart(const UPrimitiveComponent* Model, const FHitResult& Hit)
	{
		int32 Section = 0;
		return IsThinMaterial(Hit.FaceIndex >= 0 ? Model->GetMaterialFromCollisionFaceIndex(Hit.FaceIndex, Section) : nullptr);
	}

	/**
	 * The box of what the model draws, without the thin parts: its LOD 0 triangles, mesh-local. The asset's own
	 * bounding box is not used: BT-01's runs 54 cm past its last triangle at the stern and 17 cm at the bow.
	 */
	FBox DrawnSolidBox(const UStaticMesh* Mesh)
	{
		FBox Box(ForceInit);
		const FStaticMeshRenderData* Render = Mesh ? Mesh->GetRenderData() : nullptr;
		if (!Render || Render->LODResources.IsEmpty())
		{
			return Box;
		}
		const FStaticMeshLODResources& Lod = Render->LODResources[0];
		const FIndexArrayView Indices = Lod.IndexBuffer.GetArrayView();
		const FPositionVertexBuffer& Positions = Lod.VertexBuffers.PositionVertexBuffer;
		for (const FStaticMeshSection& Section : Lod.Sections)
		{
			if (IsThinMaterial(Mesh->GetMaterial(Section.MaterialIndex)))
			{
				continue;
			}
			for (uint32 I = Section.FirstIndex; I < Section.FirstIndex + Section.NumTriangles * 3; ++I)
			{
				Box += FVector(Positions.VertexPosition(Indices[I]));
			}
		}
		return Box;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepWinchRouteTest, "Deep.Sim.Rules.WinchRoute", DEEP_RETURN_TEST_FLAGS)
bool FDeepWinchRouteTest::RunTest(const FString& Parameters)
{
	using namespace DeepReturnTest;
	FDeepTetherTuning T;
	T.WinchSpeedMps = 7.f;
	T.WinchArriveM = 3.f;
	T.WinchApproachBelowM = 2.f;
	T.WinchHullClearM = 0.3f;
	const FBox Blocked = Hull.ExpandBy(FVector(Radius, Radius, HalfHeight));
	const FVector Anchor(0.f, 0.f, -250.f);
	const FVector Via(0.f, 0.f, -450.f);

	TestEqual(TEXT("Below the hull: straight to the point under the hatch"), UDeepRules::WinchRouteTarget(FVector(600.f, 0.f, -600.f), Via, Blocked, 30.f), Via);
	const FVector Behind = UDeepRules::WinchRouteTarget(FVector(-900.f, 0.f, 0.f), Via, Blocked, 30.f);
	TestTrue(TEXT("Behind at hull height: straight down first"), FMath::IsNearlyEqual(Behind.X, -900.0) && Behind.Z < Blocked.Min.Z);
	const FVector Over = UDeepRules::WinchRouteTarget(FVector(100.f, 0.f, 600.f), Via, Blocked, 30.f);
	TestTrue(TEXT("Over the hull: out past its edge first, same height"), FMath::IsNearlyEqual(Over.Z, 600.0) && FMath::Abs(Over.Y) > Blocked.Max.Y);

	// Every start reaches home within 4 s of winch and never puts the capsule into the hull.
	for (const FVector& From : Around)
	{
		FVector At = From;
		bool bHit = false;
		float Seconds = 0.f;
		for (; Seconds < 4.f && !UDeepRules::IsHome(At, Anchor, T); Seconds += 0.05f)
		{
			At = UDeepRules::WinchStepAroundHull(At, Anchor, Blocked, 0.05f, T);
			bHit |= Overlaps(Hull, At, Radius, HalfHeight);
		}
		TestTrue(FString::Printf(TEXT("From %s: home in %.2f s"), *From.ToString(), Seconds), UDeepRules::IsHome(At, Anchor, T));
		TestFalse(FString::Printf(TEXT("From %s: never through the hull"), *From.ToString()), bHit);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSwimHullSlideTest, "Deep.Sim.Rules.SwimHullSlide", DEEP_RETURN_TEST_FLAGS)
bool FDeepSwimHullSlideTest::RunTest(const FString& Parameters)
{
	using namespace DeepReturnTest;
	const FVector Touching(-470.f - Radius - 5.f, 0.f, -150.f);
	const FVector Push(400.f, 0.f, 30.f);
	const FVector Slid = UDeepRules::HullSlideVelocity(Touching, Push, Hull, Radius, HalfHeight, 15.f);
	TestTrue(TEXT("Into the back wall: slides down it"), Slid.Z < -300.f && FMath::Abs(Slid.X) < 1.f);
	TestTrue(TEXT("Never faster than the push"), Slid.Size() <= Push.Size() + 0.01f);
	const FVector Glance(300.f, 200.f, 0.f);
	const FVector Along = UDeepRules::HullSlideVelocity(Touching, Glance, Hull, Radius, HalfHeight, 15.f);
	TestTrue(TEXT("A glancing push keeps its sideways part"), FMath::IsNearlyEqual(Along.Y, 200.0, 1.0) && Along.Z < 0.f);
	TestEqual(TEXT("Pushing away: unchanged"), UDeepRules::HullSlideVelocity(Touching, -Push, Hull, Radius, HalfHeight, 15.f), -Push);
	TestEqual(TEXT("Clear of the side: unchanged"), UDeepRules::HullSlideVelocity(Touching - FVector(100.f, 0.f, 0.f), Push, Hull, Radius, HalfHeight, 15.f), Push);
	TestEqual(TEXT("Under the hull: unchanged (rise into the hatch)"), UDeepRules::HullSlideVelocity(FVector(-300.f, 0.f, -260.f), Push, Hull, Radius, HalfHeight, 15.f), Push);
	TestEqual(TEXT("Below the side's bottom edge: unchanged"), UDeepRules::HullSlideVelocity(Touching - FVector(0.f, 0.f, 120.f), Push, Hull, Radius, HalfHeight, 15.f), Push);
	TestEqual(TEXT("Over the hull's top: unchanged"), UDeepRules::HullSlideVelocity(Touching + FVector(0.f, 0.f, 420.f), Push, Hull, Radius, HalfHeight, 15.f), Push);

	// At a real surface (the model hull): the normal from a sweep says which way is out.
	const FVector Back(-1.f, 0.f, 0.f);
	TestTrue(TEXT("Normal: into a flat side, same slide as the box"), UDeepRules::HullSlideAlongNormal(Push, Back).Equals(Slid, 0.01));
	const FVector NoseSlope = FVector(1.f, 0.f, -0.6f).GetSafeNormal();
	const FVector IntoNose(-300.f, 0.f, 0.f);
	const FVector OffNose = UDeepRules::HullSlideAlongNormal(IntoNose, NoseSlope);
	TestTrue(TEXT("Normal: into the tapered nose, slides down it"), OffNose.Z < -250.f && FMath::Abs(OffNose.X) < 1.f);
	const FVector Quarter = FVector(1.f, 1.f, 0.f).GetSafeNormal();
	const FVector IntoQuarter = UDeepRules::HullSlideAlongNormal(FVector(-200.f, 0.f, 0.f), Quarter);
	TestTrue(TEXT("Normal: a curved quarter turns the push along it and down"), IntoQuarter.Y > 50.f && IntoQuarter.Z < -50.f && IntoQuarter.Size() <= 200.01f);
	TestEqual(TEXT("Normal: the belly does not slide (rise into the hatch)"), UDeepRules::HullSlideAlongNormal(Push, FVector(-0.3f, 0.f, -0.95f)), Push);
	TestEqual(TEXT("Normal: the top does not slide"), UDeepRules::HullSlideAlongNormal(Push, FVector(-0.3f, 0.f, 0.95f)), Push);
	TestEqual(TEXT("Normal: no contact, unchanged"), UDeepRules::HullSlideAlongNormal(Push, FVector::ZeroVector), Push);
	TestEqual(TEXT("Normal: pushing away, unchanged"), UDeepRules::HullSlideAlongNormal(-Push, Back), -Push);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSubHullShapeTest, "Deep.World.SubHullShape", DEEP_RETURN_TEST_FLAGS)
bool FDeepSubHullShapeTest::RunTest(const FString& Parameters)
{
	using namespace DeepReturnTest;
	const FDeepSubTuning& T = UDeepTuningSettings::Get().Sub;
	const double M = UDeepRules::CmPerM;
	const FVector Outer((T.RoomHalfLengthM + T.WallThicknessM) * M, (T.RoomHalfWidthM + T.WallThicknessM) * M, (T.RoomHeightM * 0.5 + T.WallThicknessM) * M);

	for (const int32 Mode : { 0, 1 })
	{
		FModelMode Switch(Mode);
		FReturnWorld W;
		UTEST_TRUE(TEXT("World, sub, diver, run"), W.Valid());
		const TCHAR* Tag = Mode ? TEXT("model") : TEXT("box");
		UTEST_EQUAL(FString::Printf(TEXT("%s: the switch took"), Tag), W.Sub->IsModelHull(), Mode == 1);

		// Every shape with collision is drawn: no invisible volume reaches past what the player sees.
		TArray<UStaticMeshComponent*> Shapes;
		W.Sub->GetComponents<UStaticMeshComponent>(Shapes);
		int32 Solid = 0;
		for (const UStaticMeshComponent* Shape : Shapes)
		{
			if (Shape->IsCollisionEnabled())
			{
				++Solid;
				TestTrue(FString::Printf(TEXT("%s: %s has collision and is visible"), Tag, *Shape->GetName()), Shape->IsVisible() && !Shape->bHiddenInGame);
			}
		}
		TestTrue(FString::Printf(TEXT("%s: some shapes are solid"), Tag), Solid > 0);
		const FTransform& Frame = W.Sub->GetActorTransform();
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(W.Diver);

		if (Mode == 0)
		{
			const FBox HullBox = W.Sub->GetSolidBoundsLocal(true);
			TestTrue(FString::Printf(TEXT("box: hull collision %s is the visible box +-%s"), *HullBox.ToString(), *Outer.ToString()),
				HullBox.Min.Equals(-Outer, 1.0) && HullBox.Max.Equals(Outer, 1.0));
			// A trace at the hull from each side, at hatch height, meets the visible face (not something in front of it).
			const double Z = -T.RoomHeightM * 0.5 * M;
			for (const FVector& Dir : { FVector(1.0, 0.0, 0.0), FVector(-1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0), FVector(0.0, -1.0, 0.0) })
			{
				FHitResult Hit;
				if (W.World->LineTraceSingleByChannel(Hit, Frame.TransformPosition(FVector(0.0, 0.0, Z) + Dir * 1500.0), Frame.TransformPosition(FVector(0.0, 0.0, Z)), ECC_Pawn, Params))
				{
					const double Face = FMath::Abs(Dir.X) > 0.5 ? Outer.X : Outer.Y;
					TestEqual(FString::Printf(TEXT("box: trace from %s meets the visible face"), *Dir.ToString()),
						FMath::Abs(FVector::DotProduct(Frame.InverseTransformPosition(Hit.ImpactPoint), Dir)), Face, 2.0);
				}
				else
				{
					AddError(FString::Printf(TEXT("box: trace from %s hit nothing"), *Dir.ToString()));
				}
			}
			continue;
		}

		// The model: its collision box is its visible box (its drawn triangles, less the thin parts), within ModelFitCm a side.
		UDeepHullModel* Model = W.Sub->GetHullModel();
		UTEST_NOT_NULL(TEXT("model: component"), Model);
		const FBox Drawn = DrawnSolidBox(Model->GetStaticMesh());
		UTEST_TRUE(TEXT("model: its triangles are readable"), Drawn.IsValid != 0);
		const FBox Visible = Drawn.TransformBy(Model->GetRelativeTransform());
		const FBox Collision = W.Sub->GetSolidBoundsLocal(true);
		TestTrue(FString::Printf(TEXT("model: collision box %s is the visible box %s within %.0f cm a side"), *Collision.ToString(), *Visible.ToString(), ModelFitCm),
			Collision.Min.Equals(Visible.Min, ModelFitCm) && Collision.Max.Equals(Visible.Max, ModelFitCm));
		TestTrue(TEXT("model: one solid, with or without the tower"), Collision.Equals(W.Sub->GetSolidBoundsLocal(false)));

		// Traces from the four sides, above and below, at stations along the hull and heights up it: what the game's
		// trace meets (the simple collision, ECC_Pawn) is within ModelFitCm of the visible surface (the model's own
		// triangles), and every visible surface is solid within ModelFitCm. Where a ray grazes, the gap along it can be
		// long while the surfaces are close: then a sphere of ModelFitCm at each hit must touch the other surface.
		const FVector C = Visible.GetCenter();
		const FVector E = Visible.GetExtent() + FVector(300.0);
		TArray<TPair<FVector, FVector>> Rays;  // sub-local start, end
		for (double X = Visible.Min.X + 30.0; X < Visible.Max.X; X += 60.0)
		{
			for (double Z = Visible.Min.Z + 20.0; Z < Visible.Max.Z; Z += 40.0)
			{
				Rays.Emplace(FVector(X, C.Y + E.Y, Z), FVector(X, C.Y - E.Y, Z));
				Rays.Emplace(FVector(X, C.Y - E.Y, Z), FVector(X, C.Y + E.Y, Z));
			}
			for (double Y = Visible.Min.Y + 15.0; Y < Visible.Max.Y; Y += 30.0)
			{
				Rays.Emplace(FVector(X, Y, C.Z + E.Z), FVector(X, Y, C.Z - E.Z));
				Rays.Emplace(FVector(X, Y, C.Z - E.Z), FVector(X, Y, C.Z + E.Z));
			}
		}
		for (double Y = Visible.Min.Y + 15.0; Y < Visible.Max.Y; Y += 30.0)
		{
			for (double Z = Visible.Min.Z + 20.0; Z < Visible.Max.Z; Z += 40.0)
			{
				Rays.Emplace(FVector(C.X + E.X, Y, Z), FVector(C.X - E.X, Y, Z));
				Rays.Emplace(FVector(C.X - E.X, Y, Z), FVector(C.X + E.X, Y, Z));
			}
		}
		// Only the sub: the shaft's rock and the arena round it are not the hull.
		for (TActorIterator<AActor> It(W.World); It; ++It)
		{
			if (*It != W.Sub)
			{
				Params.AddIgnoredActor(*It);
			}
		}
		FCollisionQueryParams Complex(TEXT("SubHullShapeComplex"), true);
		Complex.bReturnFaceIndex = true;
		const FCollisionShape Near = FCollisionShape::MakeSphere(ModelFitCm);
		int32 Met = 0;
		int32 Excluded = 0;
		int32 Bad = 0;
		int32 Grazing = 0;
		TArray<double> Gaps;
		for (const TPair<FVector, FVector>& Ray : Rays)
		{
			const FVector Start = Frame.TransformPosition(Ray.Key);
			const FVector End = Frame.TransformPosition(Ray.Value);
			FHitResult Game;
			FHitResult Seen;
			const bool bGame = W.World->LineTraceSingleByChannel(Game, Start, End, ECC_Pawn, Params);
			const bool bSeen = Model->LineTraceComponent(Seen, Start, End, Complex);
			if (!bGame && !bSeen)
			{
				continue;
			}
			// A thin part the hulls leave out (ThinParts): the ray meets it first.
			if (bSeen && IsThinPart(Model, Seen))
			{
				++Excluded;
				continue;
			}
			++Met;
			const double Gap = (bGame && bSeen) ? FMath::Abs(Game.Distance - Seen.Distance) : UE_BIG_NUMBER;
			if (Gap <= ModelFitCm)
			{
				Gaps.Add(Gap);
				continue;
			}
			// Grazing: each hit within ModelFitCm of the other surface (a sphere there touches it). A line run along a
			// surface can slip past its triangles while the hull, a hair proud of them, stops it (BT-01's trim strips
			// and windows), or the reverse: then the one hit must still lie within ModelFitCm of the other surface.
			FHitResult Probe;
			const bool bGameOnModel = bGame && Game.GetComponent() == Model;
			const bool bGameNearSeen = bGameOnModel
				&& Model->SweepComponent(Probe, Game.ImpactPoint, Game.ImpactPoint + FVector(0.0, 0.0, 0.1), FQuat::Identity, Near, true);
			const bool bSeenNearGame = bSeen
				&& Model->SweepComponent(Probe, Seen.ImpactPoint, Seen.ImpactPoint + FVector(0.0, 0.0, 0.1), FQuat::Identity, Near, false);
			if ((!bGame || bGameNearSeen) && (!bSeen || bSeenNearGame))
			{
				++Grazing;
				continue;
			}
			if (++Bad <= 12)
			{
				AddError(FString::Printf(TEXT("model: ray %s -> %s: game trace %s (%s), visible surface %s: more than %.0f cm apart"),
					*Ray.Key.ToCompactString(), *Ray.Value.ToCompactString(),
					bGame ? *FString::Printf(TEXT("%.1f"), Game.Distance) : TEXT("miss"), bGame ? *GetNameSafe(Game.GetComponent()) : TEXT("-"),
					bSeen ? *FString::Printf(TEXT("%.1f"), Seen.Distance) : TEXT("miss"), ModelFitCm));
			}
		}
		Gaps.Sort();
		AddInfo(FString::Printf(TEXT("model: %d of %d rays met the hull, %d meet a thin part first (excluded), %d grazing, %d off by more than %.0f cm; gap median %.1f, p95 %.1f cm"),
			Met, Rays.Num(), Excluded, Grazing, Bad, ModelFitCm, Gaps.Num() ? Gaps[Gaps.Num() / 2] : 0.0, Gaps.Num() ? Gaps[Gaps.Num() * 95 / 100] : 0.0));
		TestTrue(TEXT("model: enough rays met the hull"), Met > 400);
		TestEqual(TEXT("model: rays off the visible surface"), Bad, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSubReturnWinchTest, "Deep.World.SubReturnWinch", DEEP_RETURN_TEST_FLAGS)
bool FDeepSubReturnWinchTest::RunTest(const FString& Parameters)
{
	using namespace DeepReturnTest;
	FReturnWorld W;
	UTEST_TRUE(TEXT("World, sub, diver, run"), W.Valid());
	const float DiverRadius = W.Diver->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float DiverHalfHeight = W.Diver->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FBox Solid = W.Sub->GetSolidBoundsLocal(false);
	constexpr float LimitSeconds = 6.f;

	for (const FVector& From : Around)
	{
		UTEST_TRUE(FString::Printf(TEXT("Dived and placed at %s"), *From.ToString()), W.DiveTo(From));
		W.Diver->SetWinching(true);
		bool bHit = false;
		float Seconds = 0.f;
		for (; Seconds < LimitSeconds && W.Run->GetState() == EDeepRunState::Tethered; Seconds += 0.05f)
		{
			W.Diver->GetTether()->UpdateTether(0.05f);
			if (W.Run->GetState() == EDeepRunState::Tethered)
			{
				bHit |= Overlaps(Solid, W.LocalDiver(), DiverRadius, DiverHalfHeight);
			}
			W.Run->StepRun(0.05f);
		}
		TestEqual(FString::Printf(TEXT("Winched from %s: aboard in %.2f s (limit %.0f)"), *From.ToString(), Seconds, LimitSeconds), W.Run->GetState(), EDeepRunState::Aboard);
		TestFalse(FString::Printf(TEXT("Winched from %s: never through the hull"), *From.ToString()), bHit);
		W.Diver->SetWinching(false);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSubReturnSwimTest, "Deep.World.SubReturnSwim", DEEP_RETURN_TEST_FLAGS)
bool FDeepSubReturnSwimTest::RunTest(const FString& Parameters)
{
	using namespace DeepReturnTest;
	// Both hulls: the old box (Deep.Sub.Model 0) and BT-01, whose slide happens at its real surface.
	for (const int32 Mode : { 0, 1 })
	{
		FModelMode Switch(Mode);
		FReturnWorld W;
		UTEST_TRUE(TEXT("World, sub, diver, run"), W.Valid());
		UTEST_EQUAL(TEXT("The switch took"), W.Sub->IsModelHull(), Mode == 1);
		// A controller aimed at the hatch every frame while forward is held: the diver swims straight at it, as the S8
		// script did. The diver's own Tick turns that into swim input, character movement sweeps it against the hull.
		AAIController* Brain = W.World->SpawnActor<AAIController>();
		UTEST_NOT_NULL(TEXT("Controller"), Brain);
		Brain->Possess(W.Diver);
		UCharacterMovementComponent* Movement = W.Diver->GetCharacterMovement();
		constexpr float LimitSeconds = 15.f;
		constexpr float Dt = 1.f / 30.f;

		// Behind, beside, high off a quarter, and ahead of the nose (the model's tapers there).
		for (const FVector& From : { FVector(-900.f, 0.f, -150.f), FVector(0.f, 800.f, -150.f), FVector(-700.f, -600.f, 100.f), FVector(900.f, 0.f, 50.f) })
		{
			UTEST_TRUE(FString::Printf(TEXT("Dived and placed at %s"), *From.ToString()), W.DiveTo(From));
			W.Diver->SetMoveInput(FVector2D(0.f, 1.f));
			float Seconds = 0.f;
			for (; Seconds < LimitSeconds && W.Run->GetState() == EDeepRunState::Tethered; Seconds += Dt)
			{
				Brain->SetControlRotation((W.Sub->GetHatchLocation() - W.Diver->GetActorLocation()).Rotation());
				W.Diver->Tick(Dt);
				Movement->TickComponent(Dt, LEVELTICK_All, &Movement->PrimaryComponentTick);
				W.Diver->GetTether()->UpdateTether(Dt);
				W.Run->StepRun(Dt);
			}
			W.Diver->SetMoveInput(FVector2D::ZeroVector);
			TestEqual(FString::Printf(TEXT("%s: swam at the hatch from %s: aboard in %.1f s (limit %.0f, ended at %s)"), Mode ? TEXT("model") : TEXT("box"),
				*From.ToString(), Seconds, LimitSeconds, *W.LocalDiver().ToString()),
				W.Run->GetState(), EDeepRunState::Aboard);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSubRoomLayoutTest, "Deep.World.SubRoomLayout", DEEP_RETURN_TEST_FLAGS)
bool FDeepSubRoomLayoutTest::RunTest(const FString& Parameters)
{
	// The room set fitted to BT-01 (World/DeepRoomLayout): low and narrow, but every station in reach, no walk over the
	// hatch, room for two to pass, and the diver's camera never in the ceiling or the dressing.
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const FDeepSubTuning& T = Tuning.Sub;
	const FDeepRoomDressTuning& Dress = Tuning.RoomDress;
	const ADeepDiver* Default = GetDefault<ADeepDiver>();
	const double WalkerRadius = Default->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const double WalkerHalfHeight = Default->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const UCameraComponent* Eye = Default->FindComponentByClass<UCameraComponent>();
	UTEST_NOT_NULL(TEXT("The diver's camera"), Eye);
	const double Clear = WalkerRadius + Dress.WalkMarginCm;
	const double NearClipCm = 10.0;  // the engine's default near clipping plane
	const double M = UDeepRules::CmPerM;
	const FBox In = DeepRoomLayout::Inside(T);
	const double Deck = DeepRoomLayout::DeckTopCm(T);
	const FBox Console = DeepRoomLayout::Console(T);
	const FBox Panel = DeepRoomLayout::HelmPanel(T);
	const FBox Solids[] = { Console, Panel };

	// The round hatch grown by the walker's radius: a centre outside it stands on the deck, not on the hatch.
	const double Opening = DeepRoomLayout::HatchRadiusCm(T) + WalkerRadius;
	TestTrue(TEXT("The round hatch is at least 1 m across"), DeepRoomLayout::HatchRadiusCm(T) * 2.0 >= 100.0);
	const TPair<const TCHAR*, FVector2D> Stations[] = {
		{ TEXT("start"), T.RoomStartM * M }, { TEXT("enter stand"), T.EnterStandM * M }, { TEXT("console"), T.ConsoleM * M }, { TEXT("helm"), T.HelmM * M } };
	for (int32 A = 0; A < UE_ARRAY_COUNT(Stations); ++A)
	{
		const FVector2D P = Stations[A].Value;
		TestTrue(FString::Printf(TEXT("The %s is off the hatch"), Stations[A].Key), P.Size() > Opening);
		TestTrue(FString::Printf(TEXT("The %s is inside the room with room for the walker"), Stations[A].Key),
			P.X >= In.Min.X + WalkerRadius && P.X <= In.Max.X - WalkerRadius && FMath::Abs(P.Y) <= In.Max.Y - WalkerRadius);
		for (int32 B = A + 1; B < UE_ARRAY_COUNT(Stations); ++B)
		{
			const FVector2D Q = Stations[B].Value;
			const FVector2D Closest = FMath::ClosestPointOnSegment2D(FVector2D::ZeroVector, P, Q);
			TestTrue(FString::Printf(TEXT("The straight walk %s -> %s misses the hatch (%.0f cm off, needs %.0f)"), Stations[A].Key, Stations[B].Key,
				Closest.Size(), Opening), Closest.Size() > Opening);
		}
	}
	// Where a diver is put (the start, the enter stand) a whole capsule fits clear of the console and the helm panel.
	for (const FVector2D& Stand : { T.RoomStartM * M, T.EnterStandM * M })
	{
		const FBox Foot(FVector(Stand.X - WalkerRadius, Stand.Y - WalkerRadius, Deck), FVector(Stand.X + WalkerRadius, Stand.Y + WalkerRadius, Deck + 1.0));
		for (const FBox& Solid : Solids)
		{
			TestFalse(FString::Printf(TEXT("A diver put at %s stands clear of %s"), *Stand.ToString(), *Solid.ToString()), Foot.Intersect(Solid));
		}
	}
	// Two divers put at the start and the enter stand at once do not overlap.
	TestTrue(TEXT("The start and the enter stand are a walker apart"), FVector2D::Distance(T.RoomStartM, T.EnterStandM) * M >= 2.0 * Clear);
	// The start, the enter stand, the console and the hatch's valve are all in use range of each other: no walking to work.
	const TPair<const TCHAR*, FVector2D> Near[] = {
		{ TEXT("start"), T.RoomStartM }, { TEXT("enter stand"), T.EnterStandM }, { TEXT("console"), T.ConsoleM }, { TEXT("valve"), T.ValveM } };
	for (int32 A = 0; A < UE_ARRAY_COUNT(Near); ++A)
	{
		for (int32 B = A + 1; B < UE_ARRAY_COUNT(Near); ++B)
		{
			const double D = FVector2D::Distance(Near[A].Value, Near[B].Value);
			TestTrue(FString::Printf(TEXT("The %s and the %s are in use range (%.2f m, range %.2f m)"), Near[A].Key, Near[B].Key, D, T.UseRangeM), D <= T.UseRangeM);
		}
	}
	// The helm is at the bow: forward of everything else, the panel inside the room.
	TestTrue(TEXT("The helm is the station furthest forward"), T.HelmM.X > T.RoomStartM.X && T.HelmM.X > T.ConsoleM.X && T.HelmM.X > T.EnterStandM.X);
	TestTrue(TEXT("The helm panel stands inside the room"), In.ExpandBy(0.5).IsInside(Panel));
	// The levers stand inside the room and clear of the panel (their knobs throw toward it).
	for (const FBox& Lever : DeepRoomLayout::Levers(T, Tuning.Pilot))
	{
		TestTrue(FString::Printf(TEXT("A lever (%s) is inside the room"), *Lever.ToString()), In.IsInside(Lever));
		TestFalse(FString::Printf(TEXT("A lever (%s) is clear of the helm panel"), *Lever.ToString()), Lever.Intersect(Panel));
	}

	// Two can pass (D-057): between the hatch and the helm seat there is a section where two walkers fit side by side,
	// counting the console, the panel, the seat, the levers and every piece of dressing below head height.
	TArray<FBox> Blocking = { Console, Panel, DeepRoomLayout::HelmSeat(T) };
	Blocking.Append(DeepRoomLayout::Levers(T, Tuning.Pilot));
	const TArray<FDeepRoomPiece> Pieces = DeepRoomDress::Pieces(T);
	for (const FDeepRoomPiece& Piece : Pieces)
	{
		const FBox Box = DeepRoomDress::PieceBox(Piece);
		if (Box.Min.Z < Deck + Dress.HeadClearCm)
		{
			Blocking.Add(Box);
		}
	}
	double BestGap = 0.0;
	double BestX = 0.0;
	for (double X = 0.0; X <= T.HelmM.X * M; X += 2.0)
	{
		// Every box that reaches into a walker's length round X narrows the room there.
		TArray<FVector2D> Taken;
		for (const FBox& Box : Blocking)
		{
			if (Box.Max.X > X - Clear && Box.Min.X < X + Clear)
			{
				Taken.Emplace(Box.Min.Y, Box.Max.Y);
			}
		}
		Taken.Sort([](const FVector2D& L, const FVector2D& R) { return L.X < R.X; });
		double Edge = In.Min.Y;
		double Gap = 0.0;
		for (const FVector2D& Span : Taken)
		{
			Gap = FMath::Max(Gap, Span.X - Edge);
			Edge = FMath::Max(Edge, Span.Y);
		}
		Gap = FMath::Max(Gap, In.Max.Y - Edge);
		if (Gap > BestGap)
		{
			BestGap = Gap;
			BestX = X;
		}
	}
	TestTrue(FString::Printf(TEXT("Two walkers can pass between the hatch and the helm (widest %.0f cm at x %.0f, needs %.0f)"), BestGap, BestX, 4.0 * Clear),
		BestGap >= 4.0 * Clear);

	// Low and cramped, but the head clears: the dressing's head height is over a walker's head, the ceiling over that.
	TestTrue(TEXT("The dressing's head clearance is over a walker's head"), Dress.HeadClearCm >= 2.0 * WalkerHalfHeight);
	TestTrue(TEXT("The ceiling is over the dressing's head clearance"), In.Max.Z - Deck > Dress.HeadClearCm);
	// The camera never clips: wherever the walker's centre can be, the eye stays a near clip plane from the ceiling and
	// every piece of dressing.
	const double EyeZ = Deck + WalkerHalfHeight + Eye->GetRelativeLocation().Z;
	const FBox Eyes(FVector(In.Min.X + WalkerRadius, In.Min.Y + WalkerRadius, EyeZ), FVector(In.Max.X - WalkerRadius, In.Max.Y - WalkerRadius, EyeZ));
	AddInfo(FString::Printf(TEXT("DEEPROOMFIT inside=%s eye=%.0f widest=%.0f@%.0f"), *In.ToString(), EyeZ - Deck, BestGap, BestX));
	TestTrue(FString::Printf(TEXT("The eye (%.0f cm over the deck) is under the ceiling by more than the near clip"), EyeZ - Deck), In.Max.Z - EyeZ > NearClipCm);
	int32 Clipped = 0;
	for (const FDeepRoomPiece& Piece : Pieces)
	{
		const FBox Box = DeepRoomDress::PieceBox(Piece);
		const double D = FMath::Sqrt(Box.ComputeSquaredDistanceToBox(Eyes));
		if (D < NearClipCm)
		{
			++Clipped;
			AddError(FString::Printf(TEXT("The camera can come %.1f cm from a dressing piece at %s"), D, *Box.ToString()));
		}
	}
	TestEqual(TEXT("Dressing pieces the camera can clip"), Clipped, 0);
	return true;
}

#endif
