// Tests for the generated cave rock (UDeepCaveRules::WallRadiusM, ADeepShaft, Deep.Cave.Rebuild): the rock never comes
// nearer the passage's centre than the sub's minimum plus a margin, the same seed makes the same cave, Contact hits the
// shaped rock, the drawn mesh stands where the rule says (what you see is what you hit), and a rebuild from new tuning works.

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepRunComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "ProceduralMeshComponent.h"
#include "Sim/DeepCaveRules.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepArena.h"
#include "World/DeepShaft.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

#define DEEP_CAVESHAPE_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace DeepCaveShapeTest
{
	/** Deep.Cave.Shaft on for the test's lifetime. */
	struct FShaftOn
	{
		IConsoleVariable* Var = nullptr;
		int32 Before = -1;
		FShaftOn()
		{
			Var = IConsoleManager::Get().FindConsoleVariable(TEXT("Deep.Cave.Shaft"));
			if (Var)
			{
				Before = Var->GetInt();
				Var->Set(1, ECVF_SetByCode);
			}
		}
		~FShaftOn()
		{
			if (Var)
			{
				Var->Set(Before, ECVF_SetByCode);
			}
		}
	};

	/** A game world with the arena and one diver, seated at the helm on demand (as Deep.World.Cave's). */
	struct FWorld
	{
		UWorld* World = nullptr;
		ADeepGameMode* GameMode = nullptr;
		ADeepGameState* State = nullptr;
		ADeepSubmarine* Sub = nullptr;
		ADeepDiver* Diver = nullptr;
		UDeepRunComponent* Run = nullptr;

		FWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			GameMode = World->SpawnActor<ADeepGameMode>();
			Sub = GameMode ? GameMode->EnsureArena() : nullptr;
			if (GameMode)
			{
				GameMode->StartPlay();
			}
			State = World->GetGameState<ADeepGameState>();
			if (Sub)
			{
				Diver = World->SpawnActor<ADeepDiver>(Sub->GetRoomStartTransform().GetLocation(), FRotator::ZeroRotator);
			}
			Run = Diver ? Diver->GetRun() : nullptr;
			if (Run)
			{
				Run->StepRun(0.f);
			}
		}

		~FWorld()
		{
			if (World->HasBegunPlay())
			{
				World->EndPlay(EEndPlayReason::Quit);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		bool Valid() const { return GameMode && State && Sub && Diver && Run; }

		ADeepShaft* Shaft() const
		{
			for (TActorIterator<ADeepShaft> It(World); It; ++It)
			{
				return *It;
			}
			return nullptr;
		}

		void HoldAt(float DepthM) const
		{
			if (!Run->IsSeated())
			{
				Run->TakeHelm();
			}
			const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
			State->SubDepthM = DepthM;
			State->SubOffsetM = UDeepCaveRules::CentreM(DepthM, Tuning.StopDepthsM(), Tuning.Cave);
			State->bParked = false;
			DeepArena::ApplySubDepth(World);
		}
	};

	/** The worst gap (m) between a stretch's mesh vertices and the rule, and how many vertices were checked. */
	float MeshVersusRule(const ADeepShaft* Shaft, int32 Stretch, const TArray<float>& Stops, float MinR, const FDeepCaveTuning& Cave, int32& OutChecked)
	{
		float Worst = 0.f;
		OutChecked = 0;
		for (int32 Index = 0; Index < Shaft->GetTubes().Num(); ++Index)
		{
			const UProceduralMeshComponent* Piece = Shaft->GetTubes()[Index];
			if (!Piece || Shaft->GetTubeStretch(Index) != Stretch)
			{
				continue;
			}
			const FProcMeshSection* Section = const_cast<UProceduralMeshComponent*>(Piece)->GetProcMeshSection(0);
			if (!Section)
			{
				continue;
			}
			for (int32 V = 0; V < Section->ProcVertexBuffer.Num(); V += 7)
			{
				const FVector P = Section->ProcVertexBuffer[V].Position / UDeepRules::CmPerM;
				const float D = float(-P.Z);
				const FVector2D Rel = FVector2D(P.X, P.Y) - UDeepCaveRules::CentreM(D, Stops, Cave);
				const float Want = UDeepCaveRules::WallRadiusM(D, FMath::Atan2(Rel.Y, Rel.X), Stops, MinR, Cave);
				Worst = FMath::Max(Worst, FMath::Abs(float(Rel.Size()) - Want));
				++OutChecked;
			}
		}
		return Worst;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveShapeFloorTest, "Deep.Sim.CaveShape.Floor", DEEP_CAVESHAPE_TEST_FLAGS)
bool FDeepCaveShapeFloorTest::RunTest(const FString& Parameters)
{
	const FDeepCaveTuning Cave;
	const TArray<float> Stops = UDeepTuningSettings::Get().StopDepthsM();
	UTEST_TRUE(TEXT("At least two stops"), Stops.Num() >= 2);
	const float Min = UDeepCaveRules::MinRadiusM(FVector(-6.f, -4.f, -2.5f), FVector(6.f, 4.f, 2.5f), Cave);
	const float Floor = UDeepCaveRules::WallFloorM(Min, Cave);
	TestTrue(TEXT("The floor is past the sub's minimum"), Floor > Min);
	float Lowest = UE_BIG_NUMBER;
	float MaxIn = 0.f;
	float MaxOut = 0.f;
	float MouthRelief = 0.f;
	int32 Samples = 0;
	for (float D = Stops[0] - 20.f; D <= Stops.Last() + Cave.BelowLastStopM; D += 0.25f)
	{
		const float Base = UDeepCaveRules::RadiusM(D, Stops, Min, Cave);
		for (int32 A = 0; A < 96; ++A)
		{
			const float Theta = UE_TWO_PI * A / 96.f;
			const float R = UDeepCaveRules::WallRadiusM(D, Theta, Stops, Min, Cave);
			++Samples;
			Lowest = FMath::Min(Lowest, R);
			if (R < Floor - 0.001f)
			{
				AddError(FString::Printf(TEXT("%.2f m, %.0f deg: rock at %.2f m, inside the floor %.2f m"), D, FMath::RadiansToDegrees(Theta), R, Floor));
				return true;
			}
			const float E = UDeepCaveRules::Envelope(D, Stops, Cave);
			if (E > 0.9f)
			{
				MaxIn = FMath::Max(MaxIn, Base - R);
				MaxOut = FMath::Max(MaxOut, R - Base);
			}
			else if (E <= 0.f && UDeepCaveRules::Segment(D, Stops) >= 0)
			{
				MouthRelief = FMath::Max(MouthRelief, FMath::Abs(R - Base));
			}
		}
	}
	AddInfo(FString::Printf(TEXT("%d samples: nearest rock %.2f m (floor %.2f m); mid-shaft it stands in up to %.1f m and recedes up to %.1f m; at the mouths %.1f m"), Samples, Lowest, Floor, MaxIn, MaxOut, MouthRelief));
	TestTrue(TEXT("Mid-shaft the rock stands into the passage by metres (ribs, ledges, bulges)"), MaxIn > 2.f);
	TestTrue(TEXT("Mid-shaft it also recedes (recesses)"), MaxOut > 1.f);
	TestTrue(TEXT("Calmer at the cavern mouths"), MouthRelief < 0.75f * FMath::Max(MaxIn, MaxOut));
	// Above the first stop: no rock relief, the plain round passage.
	TestEqual(TEXT("Above the first stop: round"), UDeepCaveRules::WallRadiusM(Stops[0] - 10.f, 1.f, Stops, Min, Cave), UDeepCaveRules::RadiusM(Stops[0] - 10.f, Stops, Min, Cave), 0.001f);
	// Rock off: the smooth tube.
	FDeepCaveTuning Smooth = Cave;
	Smooth.bRock = false;
	const float Mid = 0.5f * (Stops[0] + Stops[1]);
	TestEqual(TEXT("bRock off: the smooth tube"), UDeepCaveRules::WallRadiusM(Mid, 2.f, Stops, Min, Smooth), UDeepCaveRules::RadiusM(Mid, Stops, Min, Smooth), 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveShapeSeedTest, "Deep.Sim.CaveShape.Seed", DEEP_CAVESHAPE_TEST_FLAGS)
bool FDeepCaveShapeSeedTest::RunTest(const FString& Parameters)
{
	const FDeepCaveTuning A;
	FDeepCaveTuning B;
	FDeepCaveTuning Other;
	Other.RockSeed = A.RockSeed + 1;
	const TArray<float> Stops = { 100.f, 300.f, 500.f };
	const float Min = 9.f;
	float Same = 0.f;
	float Diff = 0.f;
	for (int32 I = 0; I < 400; ++I)
	{
		const float D = 120.f + I * 0.43f;
		const float T = I * 0.731f;
		Same = FMath::Max(Same, FMath::Abs(UDeepCaveRules::WallRadiusM(D, T, Stops, Min, A) - UDeepCaveRules::WallRadiusM(D, T, Stops, Min, B)));
		Diff = FMath::Max(Diff, FMath::Abs(UDeepCaveRules::WallRadiusM(D, T, Stops, Min, A) - UDeepCaveRules::WallRadiusM(D, T, Stops, Min, Other)));
	}
	TestEqual(TEXT("Same seed, same cave"), Same, 0.f);
	TestTrue(TEXT("Another seed, another cave"), Diff > 0.5f);
	// The noise is its own hash: fixed values, so a seed means the same cave in every build.
	const float N = UDeepCaveRules::Noise3(FVector(1.37, -2.71, 3.14), 1337);
	TestTrue(TEXT("Noise in [-1, 1]"), N >= -1.f && N <= 1.f);
	TestEqual(TEXT("Noise is zero on the lattice"), UDeepCaveRules::Noise3(FVector(3.0, -4.0, 5.0), 9), 0.f, 1e-6f);
	TestEqual(TEXT("Noise repeats exactly"), UDeepCaveRules::Noise3(FVector(1.37, -2.71, 3.14), 1337), N);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveShapeContactTest, "Deep.Sim.CaveShape.Contact", DEEP_CAVESHAPE_TEST_FLAGS)
bool FDeepCaveShapeContactTest::RunTest(const FString& Parameters)
{
	FDeepCaveTuning Cave;
	Cave.HullPadM = 0.f;
	const TArray<float> Stops = UDeepTuningSettings::Get().StopDepthsM();
	UTEST_TRUE(TEXT("At least two stops"), Stops.Num() >= 2);
	// A small probe box: Contact against the shaped rock at one point.
	const FVector PMin(-0.05f, -0.05f, -0.05f);
	const FVector PMax(0.05f, 0.05f, 0.05f);
	const FVector BoxMin(-6.f, -4.f, -2.5f);
	const FVector BoxMax(6.f, 4.f, 2.5f);
	const float Min = UDeepCaveRules::MinRadiusM(BoxMin, BoxMax, Cave);
	int32 Agree = 0;
	int32 Checked = 0;
	for (int32 I = 0; I < 60; ++I)
	{
		const float D = FMath::Lerp(Stops[0] + 25.f, Stops[1] - 20.f, I / 59.f);
		const float Theta = I * 1.913f;
		const FVector2D Dir(FMath::Cos(Theta), FMath::Sin(Theta));
		const FVector2D C = UDeepCaveRules::CentreM(D, Stops, Cave);
		const float R = UDeepCaveRules::WallRadiusM(D, Theta, Stops, Min, Cave);
		const FDeepCaveContact In = UDeepCaveRules::Contact(C + Dir * (R - 0.3f), D, PMin, PMax, Stops, Min, Cave);
		const FDeepCaveContact Out = UDeepCaveRules::Contact(C + Dir * (R + 0.3f), D, PMin, PMax, Stops, Min, Cave);
		++Checked;
		Agree += (!In.bTouching && Out.bTouching) ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("Contact agrees with the rock at %d of %d points"), Agree, Checked));
	TestTrue(TEXT("Contact meets the shaped rock where the rule puts it (30 cm either side)"), Agree >= Checked - 2);

	// The sub on the passage's centre never touches, all the way down (the autopilot's line).
	FDeepCaveTuning Padded;
	int32 Touches = 0;
	float Closest = UE_BIG_NUMBER;
	const float MinPadded = UDeepCaveRules::MinRadiusM(BoxMin, BoxMax, Padded);
	for (float D = Stops[0]; D <= Stops.Last(); D += 0.5f)
	{
		const FDeepCaveContact Hit = UDeepCaveRules::Contact(UDeepCaveRules::CentreM(D, Stops, Padded), D, BoxMin, BoxMax, Stops, MinPadded, Padded);
		Closest = FMath::Min(Closest, -Hit.PenetrationM);
		Touches += Hit.bTouching ? 1 : 0;
	}
	AddInfo(FString::Printf(TEXT("On the centreline the padded box is never nearer the rock than %.2f m"), Closest));
	TestEqual(TEXT("The centreline is clear all the way down"), Touches, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveShapeMeshTest, "Deep.World.CaveShape.Mesh", DEEP_CAVESHAPE_TEST_FLAGS)
bool FDeepCaveShapeMeshTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveShapeTest;
	FShaftOn On;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	FWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run"), W.Valid());
	ADeepShaft* Shaft = W.Shaft();
	UTEST_NOT_NULL(TEXT("The shaft"), Shaft);
	AddInfo(FString::Printf(TEXT("Shaft built in %.0f ms: %d pieces, %d triangles, %d scattered rocks"), Shaft->GetBuildSeconds() * 1000.0, Shaft->GetTubes().Num(), Shaft->GetTriangleCount(), Shaft->GetScatterCount()));
	TestTrue(TEXT("Built in under 3 s"), Shaft->GetBuildSeconds() < 3.0);
	const TArray<float> Stops = Tuning.StopDepthsM();
	FVector BoxMin, BoxMax;
	DeepShaft::SubBoxM(W.Sub, BoxMin, BoxMax);
	const float MinR = UDeepCaveRules::MinRadiusM(BoxMin, BoxMax, Tuning.Cave);
	int32 Checked = 0;
	const float Worst = MeshVersusRule(Shaft, 0, Stops, MinR, Tuning.Cave, Checked);
	AddInfo(FString::Printf(TEXT("%d vertices: the mesh is within %.3f m of the rule"), Checked, Worst));
	TestTrue(TEXT("Vertices checked"), Checked > 100);
	TestTrue(TEXT("Every vertex within 3 cm of the rule"), Worst < 0.03f);

	// Traces (the sonar's channel) from the sub on the centre half way down meet the rock where the rule says.
	const float Depth = 0.5f * (Stops[0] + Stops[1]);
	W.HoldAt(Depth);
	const FVector From = W.Sub->GetActorLocation();
	int32 Rays = 0;
	float WorstRay = 0.f;
	for (int32 Index = 0; Index < 16; ++Index)
	{
		const float A = UE_TWO_PI * Index / 16.f;
		const FVector Dir = W.Sub->GetActorRotation().RotateVector(FVector(FMath::Cos(A), FMath::Sin(A), 0.f));
		const FVector To = From + Dir * (Tuning.Cave.CavernRadiusM + 40.f) * UDeepRules::CmPerM;
		float Nearest = TNumericLimits<float>::Max();
		for (int32 P = 0; P < Shaft->GetTubes().Num(); ++P)
		{
			FHitResult Hit;
			if (Shaft->GetTubeStretch(P) == 0 && Shaft->GetTubes()[P] && Shaft->GetTubes()[P]->LineTraceComponent(Hit, From, To, FCollisionQueryParams(TEXT("DeepCaveShape"), true)))
			{
				Nearest = FMath::Min(Nearest, float(Hit.Distance));
			}
		}
		if (Nearest < TNumericLimits<float>::Max())
		{
			++Rays;
			const float Want = UDeepCaveRules::WallRadiusM(Depth, A, Stops, MinR, Tuning.Cave);
			WorstRay = FMath::Max(WorstRay, FMath::Abs(Nearest / UDeepRules::CmPerM - Want));
		}
	}
	AddInfo(FString::Printf(TEXT("%d of 16 rays hit; worst gap to the rule %.2f m"), Rays, WorstRay));
	TestEqual(TEXT("Rock all round"), Rays, 16);
	TestTrue(TEXT("The traced rock is within 25 cm of the rule (between vertices the mesh is flat)"), WorstRay < 0.25f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepCaveShapeRebuildTest, "Deep.World.CaveShape.Rebuild", DEEP_CAVESHAPE_TEST_FLAGS)
bool FDeepCaveShapeRebuildTest::RunTest(const FString& Parameters)
{
	using namespace DeepCaveShapeTest;
	FShaftOn On;
	FWorld W;
	UTEST_TRUE(TEXT("World, game mode, state, sub, diver, run"), W.Valid());
	ADeepShaft* Shaft = W.Shaft();
	UTEST_NOT_NULL(TEXT("The shaft"), Shaft);
	UDeepTuningSettings* Mutable = GetMutableDefault<UDeepTuningSettings>();
	const int32 SeedBefore = Mutable->Cave.RockSeed;
	const float AmpBefore = Mutable->Cave.RockAmplitudeM;
	const uint32 PrintBefore = ADeepShaft::TuningFingerprint();
	const TArray<float> Stops = Mutable->StopDepthsM();
	FVector BoxMin, BoxMax;
	DeepShaft::SubBoxM(W.Sub, BoxMin, BoxMax);

	Mutable->Cave.RockSeed = SeedBefore + 101;
	Mutable->Cave.RockAmplitudeM = AmpBefore * 1.5f;
	TestNotEqual(TEXT("A tuning change changes the fingerprint (AutoRebuild sees it)"), ADeepShaft::TuningFingerprint(), PrintBefore);
	const double Ms = DeepShaft::RebuildAll(W.World, /*bSyncCook*/ true);
	const float MinR = UDeepCaveRules::MinRadiusM(BoxMin, BoxMax, Mutable->Cave);
	int32 Checked = 0;
	const float Worst = MeshVersusRule(Shaft, 0, Stops, MinR, Mutable->Cave, Checked);
	AddInfo(FString::Printf(TEXT("Rebuilt in %.0f ms: %d triangles; %d vertices within %.3f m of the new rule"), Ms, Shaft->GetTriangleCount(), Checked, Worst));
	TestTrue(TEXT("Rebuilt: rock again"), Shaft->GetTriangleCount() > 0 && Checked > 100);
	TestTrue(TEXT("Rebuilt to the new seed and amplitude"), Worst < 0.03f);

	Mutable->Cave.RockSeed = SeedBefore;
	Mutable->Cave.RockAmplitudeM = AmpBefore;
	DeepShaft::RebuildAll(W.World, true);
	TestEqual(TEXT("Restored: the fingerprint is back"), ADeepShaft::TuningFingerprint(), PrintBefore);
	return true;
}

#endif

