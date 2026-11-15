// The cavern (ADeepCavern): its rock stays out of everything a diver does today, its roof opening is the shaft's
// 14 m and centred over the sub, it is the same every build, and its collision answers the sonar and the diver.

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameMode.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "ProceduralMeshComponent.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepAbyssFloor.h"
#include "World/DeepArena.h"
#include "World/DeepArenaRoot.h"
#include "World/DeepCavern.h"
#include "World/DeepHull.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DeepCavernTest
{
	/** A bare game world, destroyed when this goes out of scope (as in DeepWorldTests.cpp). */
	struct FScopedWorld
	{
		UWorld* World = nullptr;

		FScopedWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}

		~FScopedWorld()
		{
			if (World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
	};

	/** A cavern at the origin (the origin is the anchor), every shell cooked at once so traces work. */
	ADeepCavern* SpawnBuilt(UWorld* World)
	{
		ADeepCavern* Cavern = World->SpawnActor<ADeepCavern>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (Cavern)
		{
			Cavern->Build(/*bAsyncCook*/ false);
		}
		return Cavern;
	}

	/** Where the last trace that hit met the rock (m) and what it hit, for the failure message. */
	FVector GLastHitM = FVector::ZeroVector;
	FString GLastHitName;

	/**
	 * A Visibility trace through the world, as the sonar and the lamp do (the world holds only the cavern). Only the
	 * loaded stop's shell collides, so any hit is rock the game would meet.
	 */
	bool HitsShell(UProceduralMeshComponent* Shell, const FVector& FromM, const FVector& ToM)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DeepCaveTest), /*bTraceComplex*/ true);
		const bool bHit = Shell->GetWorld()->LineTraceSingleByChannel(Hit, FromM * UDeepRules::CmPerM, ToM * UDeepRules::CmPerM, ECC_Visibility, Params);
		if (bHit)
		{
			GLastHitM = FVector(Hit.Location / UDeepRules::CmPerM);
			GLastHitName = Hit.GetComponent() ? Hit.GetComponent()->GetName() : FString(TEXT("?"));
		}
		return bHit;
	}

	/** Sets Deep.Cave.Cavern for the scope and puts it back. */
	struct FScopedCaveCvar
	{
		IConsoleVariable* Var = nullptr;
		int32 Old = 1;

		explicit FScopedCaveCvar(int32 Value)
		{
			Var = IConsoleManager::Get().FindConsoleVariable(TEXT("Deep.Cave.Cavern"));
			if (Var)
			{
				Old = Var->GetInt();
				Var->Set(Value, ECVF_SetByCode);
			}
		}

		~FScopedCaveCvar()
		{
			if (Var)
			{
				Var->Set(Old, ECVF_SetByCode);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCavernClearanceTest, "Deep.World.Cavern.Clearance",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepCavernClearanceTest::RunTest(const FString& Parameters)
{
	using namespace DeepCavernTest;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const FDeepCavernTuning& Cave = Tuning.Cavern;
	FScopedCaveCvar On(1);
	FScopedWorld Scoped;
	ADeepCavern* Cavern = SpawnBuilt(Scoped.World);
	UTEST_NOT_NULL(TEXT("Cavern spawned"), Cavern);

	const TArray<FVector4> RestM = ADeepAbyssFloor::RestPointsFromTuning();
	for (int32 Stop = 1; Stop <= FMath::Max(1, Tuning.NumStops()); ++Stop)
	{
		const float NeedM = ADeepCavern::NeedRadiusM(Stop, Tuning);
		const float TetherM = UDeepRules::TetherLengthM(Stop, Tuning.Tether);
		TestTrue(FString::Printf(TEXT("Stop %d: rock stands past the tether and its margin"), Stop), NeedM >= TetherM + Cave.TetherMarginM - 1e-3f);

		// The mesh itself: below the clear height no vertex is nearer than NeedM; every spur stays above the clear space.
		const FDeepCaveMesh Mesh = ADeepCavern::MakeMesh(Stop, RestM, Tuning);
		int32 Near = 0;
		for (const FVector& V : Mesh.Shell.VerticesM)
		{
			Near += V.Z <= Cave.ClearAboveAnchorM && V.Size() < NeedM - 0.01 ? 1 : 0;
		}
		TestEqual(FString::Printf(TEXT("Stop %d: no wall vertex inside the clear space"), Stop), Near, 0);
		int32 LowSpur = 0;
		for (const FVector& V : Mesh.Hangs.VerticesM)
		{
			LowSpur += V.Z < Cave.ClearAboveAnchorM + Cave.HangTipClearM - 0.01 ? 1 : 0;
		}
		TestEqual(FString::Printf(TEXT("Stop %d: every spur ends above the clear space"), Stop), LowSpur, 0);
		TestTrue(FString::Printf(TEXT("Stop %d: some spurs hang from the roof"), Stop), Mesh.Hangs.Triangles.Num() > 0);

		// The collision: straight lines from the hatch out to NeedM, round the cave and from straight down up to the
		// clear height, meet no rock. The shape is star-shaped round the anchor, so this covers the whole clear space.
		Cavern->UpdateShells(Stop, false);
		UProceduralMeshComponent* Shell = Cavern->GetShell(Stop);
		UTEST_NOT_NULL(FString::Printf(TEXT("Stop %d: a shell"), Stop), Shell);
		const double TopDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Cave.ClearAboveAnchorM / NeedM, 0.f, 1.f)));
		int32 Blocked = 0;
		int32 Rays = 0;
		for (int32 AzDeg = 0; AzDeg < 360; AzDeg += 5)
		{
			for (double ElDeg = -88.0; ElDeg <= TopDeg + 1e-3; ElDeg += FMath::Min(4.0, TopDeg - ElDeg > 1e-3 ? TopDeg - ElDeg : 4.0))
			{
				const double A = FMath::DegreesToRadians(double(AzDeg));
				const double E = FMath::DegreesToRadians(ElDeg);
				const FVector To = FVector(FMath::Cos(E) * FMath::Cos(A), FMath::Cos(E) * FMath::Sin(A), FMath::Sin(E)) * (NeedM - 0.05);
				if (HitsShell(Shell, FVector::ZeroVector, To) && Blocked++ < 3)
				{
					AddError(FString::Printf(TEXT("Stop %d: the ray to %s meets %s at %s (%.1f m out)"), Stop, *To.ToCompactString(), *GLastHitName, *GLastHitM.ToCompactString(), GLastHitM.Size()));
				}
				++Rays;
				if (ElDeg >= TopDeg - 1e-3)
				{
					break;
				}
			}
		}
		AddInfo(FString::Printf(TEXT("Stop %d: rock past %.1f m (tether %.0f m), %d rays from the hatch"), Stop, NeedM, TetherM, Rays));
		TestEqual(FString::Printf(TEXT("Stop %d: no ray from the hatch meets rock inside the clear space"), Stop), Blocked, 0);

		// Every salvage spot, pushed out by the margin, is in plain reach; so is the well under the hatch.
		int32 SpotsBlocked = 0;
		for (const FDeepSalvageSpot& Spot : Tuning.Arena.SalvageSpots)
		{
			const FVector SpotM = UDeepRules::SalvageSpotOffsetCm(Spot) / UDeepRules::CmPerM;
			SpotsBlocked += HitsShell(Shell, FVector::ZeroVector, SpotM + SpotM.GetSafeNormal() * Cave.SpotMarginM) ? 1 : 0;
		}
		TestEqual(FString::Printf(TEXT("Stop %d: every salvage spot plus its margin is clear"), Stop), SpotsBlocked, 0);
		TestFalse(FString::Printf(TEXT("Stop %d: the well under the hatch is clear"), Stop), HitsShell(Shell, FVector::ZeroVector, FVector(0.0, 0.0, -90.0)));
		// Swimming home from any side: a diver 3 m under the hatch level, 25 m out (the scripted far-out path), comes straight in.
		int32 HomeBlocked = 0;
		for (int32 AzDeg = 0; AzDeg < 360; AzDeg += 15)
		{
			const double A = FMath::DegreesToRadians(double(AzDeg));
			HomeBlocked += HitsShell(Shell, FVector(FMath::Cos(A) * 25.0, FMath::Sin(A) * 25.0, -3.0), FVector::ZeroVector) ? 1 : 0;
		}
		TestEqual(FString::Printf(TEXT("Stop %d: home is clear from every side"), Stop), HomeBlocked, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCavernRoofOpeningTest, "Deep.World.Cavern.RoofOpening",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepCavernRoofOpeningTest::RunTest(const FString& Parameters)
{
	using namespace DeepCavernTest;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const FDeepCavernTuning& Cave = Tuning.Cavern;
	TestEqual(TEXT("The opening is the shaft's radius"), Cave.HoleRadiusM, 14.f);

	// The sub, hull and outside dress, in the anchor's frame (m): the anchor sits AnchorBelowSubM under the sub's centre.
	const FDeepSubTuning& Sub = Tuning.Sub;
	const double M = UDeepRules::CmPerM;
	const FVector OuterCm((Sub.RoomHalfLengthM + Sub.WallThicknessM) * M, (Sub.RoomHalfWidthM + Sub.WallThicknessM) * M, (Sub.RoomHeightM * 0.5 + Sub.WallThicknessM) * M);
	const double AnchorZCm = -Tuning.Arena.AnchorBelowSubM * M;
	FBox SubCm(-OuterCm, OuterCm);
	for (const FDeepHullPart& Part : DeepHull::Parts(OuterCm, Sub.HatchHalfSizeM * M, AnchorZCm))
	{
		SubCm += DeepHull::PartBox(Part);
	}
	const FBox SubM(SubCm.Min / M - FVector(0, 0, AnchorZCm / M), SubCm.Max / M - FVector(0, 0, AnchorZCm / M));
	double SubOutM = 0.0;
	for (int32 Corner = 0; Corner < 4; ++Corner)
	{
		const FVector2D C((Corner & 1) ? SubM.Max.X : SubM.Min.X, (Corner & 2) ? SubM.Max.Y : SubM.Min.Y);
		SubOutM = FMath::Max(SubOutM, C.Size());
	}
	AddInfo(FString::Printf(TEXT("Sub with dress: %.1f m from the axis at most, top %.1f m over the anchor"), SubOutM, SubM.Max.Z));
	TestTrue(TEXT("The sub and its dress fit inside the opening with a metre to spare"), SubOutM < Cave.HoleRadiusM - 1.0);

	const TArray<FVector4> RestM = ADeepAbyssFloor::RestPointsFromTuning();
	FScopedWorld Scoped;
	ADeepCavern* Cavern = SpawnBuilt(Scoped.World);
	UTEST_NOT_NULL(TEXT("Cavern spawned"), Cavern);
	for (int32 Stop = 1; Stop <= FMath::Max(1, Tuning.NumStops()); ++Stop)
	{
		const FDeepCaveMesh Mesh = ADeepCavern::MakeMesh(Stop, RestM, Tuning);
		const int32 Stride = Mesh.Columns + 1;
		FVector2D Sum = FVector2D::ZeroVector;
		double WorstRim = 0.0;
		double RimLow = TNumericLimits<double>::Max();
		for (int32 i = 0; i < Mesh.Columns; ++i)
		{
			const FVector& V = Mesh.Shell.VerticesM[Mesh.RimRow * Stride + i];
			Sum += FVector2D(V.X, V.Y);
			WorstRim = FMath::Max(WorstRim, FMath::Abs(FVector2D(V.X, V.Y).Size() - Cave.HoleRadiusM));
			RimLow = FMath::Min(RimLow, V.Z);
		}
		TestTrue(FString::Printf(TEXT("Stop %d: the rim is %.0f m from the axis (worst %.4f m off)"), Stop, Cave.HoleRadiusM, WorstRim), WorstRim < 1e-3);
		TestTrue(FString::Printf(TEXT("Stop %d: the opening is centred on the sub"), Stop), (Sum / Mesh.Columns).Size() < 0.01);
		TestTrue(FString::Printf(TEXT("Stop %d: the rim is above the sub's top"), Stop), RimLow > SubM.Max.Z + 1.0);
		int32 Inside = 0;
		for (const FVector& V : Mesh.Shell.VerticesM)
		{
			Inside += FVector2D(V.X, V.Y).Size() < Cave.HoleRadiusM - 1e-3 ? 1 : 0;
		}
		for (const FVector& V : Mesh.Hangs.VerticesM)
		{
			Inside += FVector2D(V.X, V.Y).Size() < Cave.HoleRadiusM + 0.5 ? 1 : 0;
		}
		TestEqual(FString::Printf(TEXT("Stop %d: no rock inside the shaft's column"), Stop), Inside, 0);

		// Straight up through the opening, from under the sub to well above the collar: nothing.
		Cavern->UpdateShells(Stop, false);
		UProceduralMeshComponent* Shell = Cavern->GetShell(Stop);
		UTEST_NOT_NULL(FString::Printf(TEXT("Stop %d: a shell"), Stop), Shell);
		int32 Blocked = 0;
		for (const double OutM : { 0.0, 6.0, 13.0 })
		{
			for (int32 AzDeg = 0; AzDeg < 360; AzDeg += 30)
			{
				const double A = FMath::DegreesToRadians(double(AzDeg));
				const FVector Base(FMath::Cos(A) * OutM, FMath::Sin(A) * OutM, -5.0);
				Blocked += HitsShell(Shell, Base, Base + FVector(0.0, 0.0, 120.0)) ? 1 : 0;
			}
		}
		TestEqual(FString::Printf(TEXT("Stop %d: the shaft's column is open from under the sub to the surface"), Stop), Blocked, 0);
		AddInfo(FString::Printf(TEXT("Stop %d: rim %.1f m over the anchor at its lowest"), Stop, RimLow));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCavernDeterministicTest, "Deep.World.Cavern.Deterministic",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepCavernDeterministicTest::RunTest(const FString& Parameters)
{
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const TArray<FVector4> RestM = ADeepAbyssFloor::RestPointsFromTuning();
	for (int32 Stop = 1; Stop <= FMath::Max(1, Tuning.NumStops()); ++Stop)
	{
		const FDeepCaveMesh A = ADeepCavern::MakeMesh(Stop, RestM, Tuning);
		const FDeepCaveMesh B = ADeepCavern::MakeMesh(Stop, RestM, Tuning);
		TestTrue(FString::Printf(TEXT("Stop %d: the same shell every build"), Stop), A.Shell.VerticesM == B.Shell.VerticesM && A.Shell.Triangles == B.Shell.Triangles);
		TestTrue(FString::Printf(TEXT("Stop %d: the same spurs every build"), Stop), A.Hangs.VerticesM == B.Hangs.VerticesM && A.Hangs.Triangles == B.Hangs.Triangles);
		TestTrue(FString::Printf(TEXT("Stop %d: a whole grid"), Stop), A.Shell.VerticesM.Num() == A.Rows * (A.Columns + 1) && A.Shell.Triangles.Num() == (A.Rows - 1) * A.Columns * 6);
		// The seam closes: the last column repeats the first.
		int32 Open = 0;
		for (int32 j = 0; j < A.Rows; ++j)
		{
			Open += A.Shell.VerticesM[j * (A.Columns + 1)] == A.Shell.VerticesM[j * (A.Columns + 1) + A.Columns] ? 0 : 1;
		}
		TestEqual(FString::Printf(TEXT("Stop %d: the seam closes"), Stop), Open, 0);
		// Faces point into the cave (toward the anchor) on the walls.
		int32 Outward = 0;
		const int32 Mid = A.RimRow + (A.Rows - A.RimRow) / 2;
		for (int32 i = 0; i < A.Columns; ++i)
		{
			const int32 Index = Mid * (A.Columns + 1) + i;
			Outward += FVector::DotProduct(A.Shell.Normals[Index], -A.Shell.VerticesM[Index]) > 0.0 ? 0 : 1;
		}
		TestEqual(FString::Printf(TEXT("Stop %d: wall normals face the cave"), Stop), Outward, 0);
	}
	// Relief only ever pushes rock out: never nearer than the base shape.
	const FDeepCavernTuning& Cave = Tuning.Cavern;
	FDeepCavernTuning Flat = Cave;
	Flat.ReliefM = 0.f;
	Flat.AlcoveDepthM = 0.f;
	Flat.StrataM = 0.f;
	Flat.RoofReliefM = 0.f;
	int32 Inward = 0;
	FRandomStream Rng(7);
	for (int32 Sample = 0; Sample < 2000; ++Sample)
	{
		const FVector Dir = Rng.GetUnitVector();
		Inward += ADeepCavern::SurfaceDistanceM(Dir, 50.f, Cave) < ADeepCavern::SurfaceDistanceM(Dir, 50.f, Flat) - 1e-4f ? 1 : 0;
	}
	TestEqual(TEXT("Relief never moves rock inward"), Inward, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCavernCollisionTest, "Deep.World.Cavern.Collision",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepCavernCollisionTest::RunTest(const FString& Parameters)
{
	using namespace DeepCavernTest;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FScopedCaveCvar On(1);
	FScopedWorld Scoped;
	UWorld* World = Scoped.World;
	ADeepCavern* Cavern = SpawnBuilt(World);
	UTEST_NOT_NULL(TEXT("Cavern spawned"), Cavern);

	const float NeedM = ADeepCavern::NeedRadiusM(1, Tuning);
	const FVector Start = FVector(0.0, 0.0, -5.0) * UDeepRules::CmPerM;
	const FVector End = FVector(200.0, 0.0, -5.0) * UDeepRules::CmPerM;
	auto Trace = [&](ECollisionChannel Channel, FHitResult& Hit)
	{
		return World->LineTraceSingleByChannel(Hit, Start, End, Channel, FCollisionQueryParams(SCENE_QUERY_STAT(DeepCaveTest), true));
	};

	Cavern->UpdateShells(1, false);
	FHitResult Hit;
	TestTrue(TEXT("A Visibility trace (the sonar, the lamp) meets the wall"), Trace(ECC_Visibility, Hit));
	TestTrue(TEXT("... the cave's wall"), Hit.GetActor() == Cavern);
	const double HitM = Hit.Distance / UDeepRules::CmPerM;
	AddInfo(FString::Printf(TEXT("Wall at %.1f m along +X at 5 m under the hatch (rock past %.1f m)"), HitM, NeedM));
	TestTrue(TEXT("... past the clear space"), HitM >= NeedM - 0.05);
	TestTrue(TEXT("... and close enough to be a wall, not a horizon"), HitM <= NeedM + Tuning.Cavern.ReliefM + Tuning.Cavern.AlcoveDepthM + Tuning.Cavern.StrataM + 3.0);
	FHitResult PawnHit;
	TestTrue(TEXT("A Pawn trace (the diver) meets the wall"), Trace(ECC_Pawn, PawnHit) && PawnHit.GetActor() == Cavern);
	UProceduralMeshComponent* Shell = Cavern->GetShell(1);
	UTEST_NOT_NULL(TEXT("A stop 1 shell"), Shell);
	TestEqual(TEXT("The shell blocks Pawn"), Shell->GetCollisionResponseToChannel(ECC_Pawn), ECR_Block);
	TestEqual(TEXT("The shell blocks Visibility"), Shell->GetCollisionResponseToChannel(ECC_Visibility), ECR_Block);

	// Only the loaded stop's shell collides; a hidden bed takes the cave with it.
	Cavern->UpdateShells(2, false);
	TestFalse(TEXT("Stop 2 loaded: the stop 1 shell neither shows nor blocks"), Shell->IsVisible() || Shell->IsCollisionEnabled());
	Cavern->UpdateShells(1, true);
	TestFalse(TEXT("Bed hidden: no rock blocks"), Trace(ECC_Visibility, Hit) && Hit.GetActor() == Cavern);
	{
		FScopedCaveCvar Off(0);
		Cavern->UpdateShells(1, false);
		TestFalse(TEXT("Deep.Cave.Cavern 0: no rock blocks"), Trace(ECC_Visibility, Hit) && Hit.GetActor() == Cavern);
		TestFalse(TEXT("Deep.Cave.Cavern 0: nothing shows"), Shell->IsVisible());
	}
	Cavern->UpdateShells(1, false);
	TestTrue(TEXT("Back on: the wall blocks again"), Trace(ECC_Visibility, Hit) && Hit.GetActor() == Cavern);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCavernArenaTest, "Deep.World.Cavern.Arena",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepCavernArenaTest::RunTest(const FString& Parameters)
{
	using namespace DeepCavernTest;
	FScopedWorld Scoped;
	UWorld* World = Scoped.World;
	ADeepGameMode* GameMode = World->SpawnActor<ADeepGameMode>();
	UTEST_NOT_NULL(TEXT("Game mode spawned"), GameMode);
	GameMode->EnsureArena();
	GameMode->EnsureArena();
	int32 Count = 0;
	ADeepCavern* Cavern = nullptr;
	for (TActorIterator<ADeepCavern> It(World); It; ++It)
	{
		Cavern = *It;
		++Count;
	}
	TestEqual(TEXT("EnsureArena spawns one cavern"), Count, 1);
	UTEST_NOT_NULL(TEXT("A cavern"), Cavern);
	TestTrue(TEXT("It hangs from the arena root, so it slides and hides with the stop's bed"), Cavern->GetAttachParentActor() == DeepArena::FindArenaRoot(World));
	TestTrue(TEXT("It is centred on the anchor, like the bed"), Cavern->GetActorLocation().Equals(DeepArena::ArenaAnchor(World), 1.0));
	return true;
}

#endif
