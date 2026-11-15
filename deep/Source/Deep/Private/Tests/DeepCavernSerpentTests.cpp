// The serpent inside the cavern (stops 2 and 3): its lurk circle is 45 m plus half the tether's span round the tether's
// middle, which runs through the rock once the diver is out. Every path point is kept a ball of CaveWallClearM +
// CaveBodyReachM clear of the rock (ADeepCavern::KeepClearCm), the drawn body CaveWallClearM; `Deep.Cave.Cavern 0`
// changes nothing. Checked against the true surface (SurfaceDistanceM, every relief term) and the cooked shell.

#include "CoreMinimal.h"
#include "AI/AISystemBase.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/DeepGameMode.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Monster/DeepMonsterBrain.h"
#include "Monster/DeepMonsterRules.h"
#include "Monster/DeepMonsterSettings.h"
#include "Monster/DeepSerpent.h"
#include "Monster/DeepSerpentController.h"
#include "Perception/AIPerceptionSystem.h"
#include "ProceduralMeshComponent.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepCavern.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DeepCavernSerpentTest
{
	constexpr double CmM = UDeepRules::CmPerM;

	/** Unit directions spread evenly over the sphere (a Fibonacci lattice). */
	const TArray<FVector>& BallDirections()
	{
		static TArray<FVector> Dirs;
		if (Dirs.Num() == 0)
		{
			constexpr int32 N = 96;
			const double Golden = PI * (3.0 - FMath::Sqrt(5.0));
			for (int32 i = 0; i < N; ++i)
			{
				const double Z = 1.0 - 2.0 * (i + 0.5) / N;
				const double R = FMath::Sqrt(1.0 - Z * Z);
				Dirs.Add(FVector(R * FMath::Cos(Golden * i), R * FMath::Sin(Golden * i), Z));
			}
		}
		return Dirs;
	}

	/**
	 * Independent of RoomM: every sampled point of the ball (anchor frame, m) is nearer the anchor than the true rock
	 * along its own direction (SurfaceDistanceM, relief and all). Returns the worst (smallest) margin found (m).
	 */
	double BallMarginM(const FVector& CentreM, double RadiusM, float BaseM, const FDeepCaveTuning& Cave)
	{
		double Worst = TNumericLimits<double>::Max();
		auto Check = [&](const FVector& Q)
		{
			const double R = Q.Size();
			if (R > UE_KINDA_SMALL_NUMBER)
			{
				Worst = FMath::Min(Worst, ADeepCavern::SurfaceDistanceM(Q / R, BaseM, Cave) - R);
			}
		};
		Check(CentreM);
		for (const FVector& U : BallDirections())
		{
			Check(CentreM + U * RadiusM);
		}
		return Worst;
	}

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

// ---------------------------------------------------------------- pure: the lurk circle over a full loop

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCavernSerpentRoomTest, "Deep.World.Cavern.SerpentRoom",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepCavernSerpentRoomTest::RunTest(const FString& Parameters)
{
	using namespace DeepCavernSerpentTest;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const FDeepCaveTuning& Cave = Tuning.Cave;
	const int32 LastStop = FMath::Min(3, Tuning.NumStops());
	for (int32 Stop = 2; Stop <= LastStop; ++Stop)
	{
		const FDeepMonsterTuning Monster = UDeepMonsterSettings::Get().GetTuning(Stop);
		const float ClearM = Monster.CaveWallClearM + Monster.CaveBodyReachM;
		const float BaseM = ADeepCavern::StopBaseRadiusM(Stop, Tuning);
		const float TetherM = UDeepRules::TetherLengthM(Stop, Tuning.Tether);
		const FVector HatchM(0.0, 0.0, -3.1);

		int32 Samples = 0;
		int32 Moved = 0;
		double WorstM = TNumericLimits<double>::Max();
		FVector WorstAtM = FVector::ZeroVector;
		// The diver at the hatch, and out at the tether's reach in twelve directions, level and dropping toward the bed.
		for (int32 Case = 0; Case < 25; ++Case)
		{
			FVector DiverM = HatchM;
			if (Case > 0)
			{
				const double Az = 2.0 * PI * ((Case - 1) % 12) / 12.0;
				const double Dip = (Case - 1) < 12 ? 0.0 : 0.5;
				DiverM = HatchM + FVector(FMath::Cos(Az), FMath::Sin(Az), -Dip).GetSafeNormal() * (0.95 * TetherM);
			}
			// The brain's lurk circle (UDeepMonsterBrain::LurkRadiusCm): round the tether's middle, 45 m plus half its span.
			const FVector CentreM = (HatchM + DiverM) * 0.5;
			const double RadiusM = Monster.LurkRadiusM + 0.5 * FVector2D(DiverM.X - HatchM.X, DiverM.Y - HatchM.Y).Size();
			FVector Prev = FVector::ZeroVector;
			for (int32 i = 0; i <= 360; ++i)
			{
				const double A = 2.0 * PI * i / 360.0;
				const FVector Circle = CentreM + FVector(FMath::Cos(A), FMath::Sin(A), 0.0) * RadiusM;
				const FVector Kept = ADeepCavern::KeepClearM(Circle, ClearM, BaseM, Cave);
				Moved += Kept.Equals(Circle, 0.01) ? 0 : 1;
				// The point, and the straight step from the last one (the path between samples).
				for (const FVector& P : { Kept, i > 0 ? (Prev + Kept) * 0.5 : Kept })
				{
					const double Margin = BallMarginM(P, ClearM, BaseM, Cave);
					++Samples;
					if (Margin < WorstM)
					{
						WorstM = Margin;
						WorstAtM = P;
					}
				}
				Prev = Kept;
			}
		}
		AddInfo(FString::Printf(TEXT("Stop %d: base %.1f m, path ball %.1f m; %d samples, %d circle points pulled in from the rock, worst margin past the ball %.2f m at (%.1f, %.1f, %.1f)"),
			Stop, BaseM, ClearM, Samples, Moved, WorstM, WorstAtM.X, WorstAtM.Y, WorstAtM.Z));
		TestTrue(FString::Printf(TEXT("Stop %d: the lurk circle reaches the rock with the diver out (the case this fixes)"), Stop), Moved > 0);
		TestTrue(FString::Printf(TEXT("Stop %d: every lurk point and step is a %.1f m ball clear of the rock (worst %.2f m)"), Stop, ClearM, WorstM), WorstM >= 0.0);

		// Points already clear are left alone: no pull toward the sub inside the cave.
		const FVector Inside(10.0, -5.0, -4.0);
		TestTrue(FString::Printf(TEXT("Stop %d: a point well inside is unchanged"), Stop), ADeepCavern::KeepClearM(Inside, ClearM, BaseM, Cave).Equals(Inside, 1e-3));
		// Under the spurs: nothing higher than their tips less the ball.
		const FVector High = ADeepCavern::KeepClearM(FVector(20.0, 0.0, 30.0), ClearM, BaseM, Cave);
		TestTrue(FString::Printf(TEXT("Stop %d: kept under the roof spurs (%.1f m up)"), Stop, High.Z), High.Z <= Cave.ClearAboveAnchorM + Cave.HangTipClearM - ClearM + 1e-3);
	}
	return true;
}

// ---------------------------------------------------------------- world: the serpent itself, head path and drawn body

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCavernSerpentInsideTest, "Deep.World.Cavern.SerpentInside",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepCavernSerpentInsideTest::RunTest(const FString& Parameters)
{
	using namespace DeepCavernSerpentTest;
	constexpr float Dt = 0.1f;
	FScopedCaveCvar CaveOn(1);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	Context.SetCurrentWorld(World);
	World->CreateAISystem();
	World->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT
	{
		if (World->HasBegunPlay())
		{
			World->EndPlay(EEndPlayReason::Quit);
		}
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};

	ADeepGameMode* GameMode = World->SpawnActor<ADeepGameMode>();
	UTEST_NOT_NULL(TEXT("Game mode"), GameMode);
	ADeepSubmarine* Sub = GameMode->EnsureArena();
	UTEST_NOT_NULL(TEXT("Submarine"), Sub);
	GameMode->StartPlay();
	if (UAISystemBase* AISystem = World->GetAISystem())
	{
		AISystem->StartPlay();
	}
	ADeepDiver* Diver = World->SpawnActor<ADeepDiver>(Sub->GetDiverStartTransform().GetLocation(), FRotator::ZeroRotator);
	UTEST_NOT_NULL(TEXT("Diver"), Diver);
	UDeepRunComponent* Run = Diver->GetRun();
	UDeepMonsterBrain* Brain = World->GetSubsystem<UDeepMonsterBrain>();
	UTEST_TRUE(TEXT("Run and brain"), Run && Brain);
	ADeepCavern* Cavern = ADeepCavern::FindIn(World);
	UTEST_NOT_NULL(TEXT("The arena has a cavern"), Cavern);
	// Every shell cooked now, so the overlap checks meet real collision.
	Cavern->Build(/*bAsyncCook*/ false);

	UAIPerceptionSystem* Perception = UAIPerceptionSystem::GetCurrent(*World);
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const int32 LastStop = FMath::Min(3, Tuning.NumStops());
	for (int32 Stop = 2; Stop <= LastStop; ++Stop)
	{
		GameMode->GotoStop(Stop);
		Cavern->UpdateShells(Stop, /*bBedHidden*/ false);
		UTEST_EQUAL(FString::Printf(TEXT("Stop %d's rock shows"), Stop), Cavern->ShownStop(), Stop);
		UProceduralMeshComponent* Shell = Cavern->GetShell(Stop);
		UTEST_NOT_NULL(TEXT("Shell"), Shell);
		const FVector AnchorCm = Cavern->GetActorLocation();
		const FDeepMonsterTuning Monster = UDeepMonsterSettings::Get().GetTuning(Stop);
		const float PathClearM = Monster.CaveWallClearM + Monster.CaveBodyReachM;
		const float BaseM = ADeepCavern::StopBaseRadiusM(Stop, Tuning);

		// The diver out at most of the tether's reach, a little below the hatch: its lurk circle runs far into the rock.
		const float TetherM = UDeepRules::TetherLengthM(Stop, Tuning.Tether);
		const FVector DiverAt = AnchorCm + FVector(0.8 * TetherM * CmM, 0.0, -6.0 * CmM);
		Diver->SetActorLocation(DiverAt, false, nullptr, ETeleportType::TeleportPhysics);

		int32 PathSamples = 0;
		int32 BodySamples = 0;
		int32 PathFails = 0;
		int32 BodyFails = 0;
		double WorstPathM = TNumericLimits<double>::Max();
		FVector WorstPathAt = FVector::ZeroVector;
		FString FirstBodyFail;
		TSet<FString> States;
		double SweptRad = 0.0;
		double LastAz = 0.0;
		bool bHaveAz = false;
		double FarthestM = 0.0;
		const int32 MaxSteps = FMath::CeilToInt(240.f / Dt);
		int32 Step = 0;
		for (; Step < MaxSteps; ++Step)
		{
			Diver->SetActorLocation(DiverAt, false, nullptr, ETeleportType::TeleportPhysics);
			World->TimeSeconds += Dt;
			Brain->Step(Dt);
			if (Perception)
			{
				Perception->Tick(Dt);
			}
			ADeepSerpentController* Mind = Brain->GetSerpentController();
			ADeepSerpent* Serpent = Brain->GetSerpent();
			if (!Mind || !Serpent)
			{
				continue;
			}
			Mind->StepMonster(Dt);
			Run->StepRun(Dt);
			Serpent->Tick(Dt);
			if (Mind->IsInWellMode())
			{
				continue;
			}
			States.Add(UDeepMonsterRules::StateName(Mind->GetState()));

			// The path (the head's actor point, which the body follows): a ball of the path clearance, against the true rock.
			const FVector PathM = (Serpent->GetActorLocation() - AnchorCm) / CmM;
			const double Margin = BallMarginM(PathM, PathClearM, BaseM, Tuning.Cave);
			++PathSamples;
			FarthestM = FMath::Max(FarthestM, FVector2D(PathM.X, PathM.Y).Size());
			if (Margin < WorstPathM)
			{
				WorstPathM = Margin;
				WorstPathAt = PathM;
			}
			PathFails += Margin < -0.05 ? 1 : 0;

			// Round the cave: how far the path has gone round the anchor.
			const double Az = FMath::Atan2(PathM.Y, PathM.X);
			if (bHaveAz)
			{
				SweptRad += FMath::Abs(FMath::UnwindRadians(Az - LastAz));
			}
			LastAz = Az;
			bHaveAz = true;

			// The drawn body, ring by ring and the head, CaveWallClearM off the cooked rock (spurs included).
			if (Step % 5 == 0)
			{
				TArray<FSphere> Spheres;
				Serpent->GetDrawnSpheres(Spheres);
				for (const FSphere& S : Spheres)
				{
					++BodySamples;
					const FCollisionShape Ball = FCollisionShape::MakeSphere(float(S.W + Monster.CaveWallClearM * CmM));
					if (Shell->OverlapComponent(S.Center, FQuat::Identity, Ball))
					{
						++BodyFails;
						if (FirstBodyFail.IsEmpty())
						{
							const FVector AtM = (S.Center - AnchorCm) / CmM;
							FirstBodyFail = FString::Printf(TEXT("%s, %.1f s: a %.2f m sphere at (%.1f, %.1f, %.1f) m"),
								*UDeepMonsterRules::StateName(Mind->GetState()), Step * Dt, S.W / CmM, AtM.X, AtM.Y, AtM.Z);
						}
					}
				}
			}
			if (SweptRad >= 2.0 * PI && Mind->IsFirstSightDone() && Step * Dt > 30.f)
			{
				break;
			}
		}
		AddInfo(FString::Printf(TEXT("Stop %d: %.1f s, states %s; went %.0f deg round the cave, out to %.1f m; path %d samples, worst margin past its %.1f m ball %.2f m at (%.1f, %.1f, %.1f); body %d spheres, %d within %.1f m of rock%s%s"),
			Stop, Step * Dt, *FString::Join(States.Array(), TEXT("/")), FMath::RadiansToDegrees(SweptRad), FarthestM, PathSamples, PathClearM, WorstPathM,
			WorstPathAt.X, WorstPathAt.Y, WorstPathAt.Z, BodySamples, BodyFails, Monster.CaveWallClearM,
			FirstBodyFail.IsEmpty() ? TEXT("") : TEXT(", first: "), *FirstBodyFail));
		TestTrue(FString::Printf(TEXT("Stop %d: it went a full loop round the cave (%.0f deg)"), Stop, FMath::RadiansToDegrees(SweptRad)), SweptRad >= 2.0 * PI);
		TestTrue(FString::Printf(TEXT("Stop %d: it lurked"), Stop), States.Contains(TEXT("Lurk")));
		TestEqual(FString::Printf(TEXT("Stop %d: path points closer than %.1f m to the rock"), Stop, PathClearM), PathFails, 0);
		TestEqual(FString::Printf(TEXT("Stop %d: drawn body spheres within %.1f m of the cooked rock"), Stop, Monster.CaveWallClearM), BodyFails, 0);
		TestTrue(FString::Printf(TEXT("Stop %d: body sampled"), Stop), BodySamples > 0);
	}

	// With the cave off nothing is moved: the open arena, as before.
	{
		FScopedCaveCvar CaveOff(0);
		TestEqual(TEXT("Deep.Cave.Cavern 0: no rock shows to keep clear of"), Cavern->ShownStop(), 0);
		const FVector Far = Cavern->GetActorLocation() + FVector(500.0 * CmM, 0.0, 0.0);
		TestTrue(TEXT("Deep.Cave.Cavern 0: a point in the rock stays where it is"), Cavern->KeepClearCm(Far, 4.5f).Equals(Far, 0.01));
	}

	// The relic's well (the last stop): it steers there unclamped; report how its shaft sits in the cave's room.
	const FDeepWell Well = Brain->GetWell();
	if (Well.bValid)
	{
		const FVector AnchorCm = Cavern->GetActorLocation();
		const float BaseM = ADeepCavern::StopBaseRadiusM(LastStop, Tuning);
		double Least = TNumericLimits<double>::Max();
		for (int32 i = 0; i <= 10; ++i)
		{
			const FVector P(Well.AxisCm.X, Well.AxisCm.Y, FMath::Lerp(Well.FloorZCm, Well.TopZCm, i / 10.0));
			const FVector PM = (P - AnchorCm) / CmM;
			Least = FMath::Min(Least, double(ADeepCavern::RoomM(PM, 0.f, BaseM, Tuning.Cave)) - PM.Size());
		}
		AddInfo(FString::Printf(TEXT("Well axis: least room left to the cave's base shape %.1f m (negative = below the bed, outside the cave)"), Least));
	}
	return true;
}

#endif
