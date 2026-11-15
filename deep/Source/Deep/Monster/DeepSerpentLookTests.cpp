// The serpent's look: the tube body is built, the eyes shine only with a lamp on them, the markings stay dark
// in the first minute. Behaviour is covered by Deep.World.Monster; this checks only what is drawn.

#include "CoreMinimal.h"
#include "Components/SpotLightComponent.h"
#include "Diver/DeepDiver.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/DeepGameMode.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Monster/DeepMonsterSettings.h"
#include "Monster/DeepSerpent.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepSerpentLookTest, "Deep.World.Monster.Look",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepSerpentLookTest::RunTest(const FString& Parameters)
{
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
	UTEST_NOT_NULL(TEXT("Game mode spawned"), GameMode);
	ADeepSubmarine* Sub = GameMode->EnsureArena();
	UTEST_NOT_NULL(TEXT("Submarine spawned"), Sub);
	GameMode->StartPlay();
	// Stop 2, as in Deep.World.Monster: the diver has the flashlight there.
	GameMode->GotoStop(2);

	ADeepDiver* Diver = World->SpawnActor<ADeepDiver>(Sub->GetDiverStartTransform().GetLocation() + FVector(0.f, 0.f, -3000.f), FRotator::ZeroRotator);
	UTEST_NOT_NULL(TEXT("Diver spawned"), Diver);
	USpotLightComponent* Lamp = Diver->FindComponentByClass<USpotLightComponent>();
	UTEST_NOT_NULL(TEXT("Diver has a lamp"), Lamp);

	ADeepSerpent* Serpent = World->SpawnActor<ADeepSerpent>(FVector(0.f, 0.f, -100000.f), FRotator::ZeroRotator);
	UTEST_NOT_NULL(TEXT("Serpent spawned"), Serpent);

	const FDeepSerpentBodyTuning& Body = UDeepMonsterSettings::Get().Body;
	TestTrue(TEXT("One continuous body (not spheres) by default"), Serpent->IsTubeBody());
	const int32 Sides = FMath::Max(8, Body.TubeSides / 4 * 4);
	TestEqual(TEXT("Tube has rings x (sides + seam) vertices"), Serpent->GetTubeVertexCount(), FMath::Max(4, Body.TubeRings) * (Sides + 1));
	TestEqual(TEXT("Harpoon hit segments are kept"), Serpent->GetSegmentTransforms().Num(), Body.Segments);
	// The pack head is worn when its mesh loads (Dragon Cave installed) and the switch is on; otherwise the code-built head.
	const bool bPackHead = Body.MeshHead.Mesh.IsValid() && Body.MeshHead.Mesh.TryLoad() != nullptr && ADeepSerpent::IsMeshHeadAllowed();
	TestEqual(TEXT("Mesh head exactly when the pack mesh loads"), Serpent->HasMeshHead(), bPackHead);
	TestFalse(TEXT("The code-built head with its moving jaw by default (Quinn, Nov 15: the pack head read as two models)"), Serpent->HasMeshHead());

	// The body follows the head (a physics chain, no sideways sine): at rest it lies still; after the head swims 3 m and
	// turns, the tail has moved and the spacing holds. The head's stroke swings the neck while it swims.
	{
		const FVector Start(0.f, 0.f, -100000.f);
		Serpent->PlaceHead(Start, FVector::ForwardVector);
		const int32 N = Serpent->GetSegmentTransforms().Num();
		UTEST_TRUE(TEXT("Body has segments"), N > 2);
		const FVector TailAtRest = Serpent->GetSegmentTransforms().Last().GetLocation();
		for (int32 i = 0; i < 20; ++i)
		{
			Serpent->Tick(0.05f);
		}
		const float RestDrift = static_cast<float>(FVector::Dist(Serpent->GetSegmentTransforms().Last().GetLocation(), TailAtRest));
		AddInfo(FString::Printf(TEXT("At rest for 1 s the tail drifted %.1f cm"), RestDrift));
		TestTrue(TEXT("At rest the body lies still (no slither)"), RestDrift < 5.f);

		const FVector Tail0 = Serpent->GetSegmentTransforms().Last().GetLocation();
		float MinSide = TNumericLimits<float>::Max();
		float MaxSide = -MinSide;
		// 3 m straight at 3 m/s, then a turn to the right for 2 s.
		for (int32 i = 0; i < 60; ++i)
		{
			const FVector Aim = i < 20 ? Start + FVector(10000.f, 0.f, 0.f) : Serpent->GetActorLocation() + FVector(0.f, 10000.f, 0.f);
			Serpent->SwimToward(Aim, 3.f, 60.f, 0.05f);
			Serpent->Tick(0.05f);
			if (i < 20)
			{
				const float Side = static_cast<float>(Serpent->GetSegmentTransforms()[0].GetLocation().Y);
				MinSide = FMath::Min(MinSide, Side);
				MaxSide = FMath::Max(MaxSide, Side);
			}
		}
		const TArray<FTransform>& Segs = Serpent->GetSegmentTransforms();
		const float SpacingCm = static_cast<float>(FVector::Dist(Segs[1].GetLocation(), Segs[2].GetLocation()));
		float WorstSpacing01 = 0.f;
		const float Expected = static_cast<float>(FVector::Dist(Segs[N / 2].GetLocation(), Segs[N / 2 + 1].GetLocation()));
		for (int32 i = 0; i + 1 < N; ++i)
		{
			const float Len = static_cast<float>(FVector::Dist(Segs[i].GetLocation(), Segs[i + 1].GetLocation()));
			WorstSpacing01 = FMath::Max(WorstSpacing01, FMath::Abs(Len - Expected) / Expected);
		}
		const float TailMoved = static_cast<float>(FVector::Dist(Segs.Last().GetLocation(), Tail0));
		AddInfo(FString::Printf(TEXT("Swim 3 m and turn: tail moved %.0f cm, spacing %.0f cm off by at most %.1f%%, neck swung %.0f cm with the stroke"),
			TailMoved, SpacingCm, WorstSpacing01 * 100.f, MaxSide - MinSide));
		TestTrue(TEXT("The tail has followed the head (moved at least 2 m)"), TailMoved >= 200.f);
		TestTrue(TEXT("The spacing holds within 2%"), WorstSpacing01 < 0.02f);
		TestTrue(TEXT("The head's stroke swings the neck while it swims (at least 10 cm)"), MaxSide - MinSide >= 10.f);
	}
	{
		IConsoleVariable* Switch = IConsoleManager::Get().FindConsoleVariable(TEXT("Deep.Monster.MeshHead"));
		UTEST_NOT_NULL(TEXT("Deep.Monster.MeshHead exists"), Switch);
		const int32 Was = Switch->GetInt();
		Switch->Set(0, ECVF_SetByCode);
		TestFalse(TEXT("Deep.Monster.MeshHead 0 brings back the code-built head"), Serpent->HasMeshHead());
		Switch->Set(Was, ECVF_SetByCode);
		TestEqual(TEXT("Deep.Monster.MeshHead back on restores it"), Serpent->HasMeshHead(), bPackHead);
	}

	// Head 8 m down the beam, facing the lamp: the eyes shine.
	const FVector Facing = -Lamp->GetForwardVector();
	Serpent->PlaceHead(Lamp->GetComponentLocation() + Lamp->GetForwardVector() * 800.f, Facing);
	if (!Diver->IsLampOn())
	{
		Diver->ToggleLamp();
	}
	UTEST_TRUE(TEXT("Lamp is on"), Diver->IsLampOn());
	for (int32 i = 0; i < 10; ++i)
	{
		Serpent->Tick(0.05f);
	}
	TestTrue(TEXT("Eyes shine back while the beam is on them"), Serpent->GetEyeShine01() > 0.4f);
	// Gameplay honesty: the head the lamp, the bite and DEEPSTATE use sits at the visible head (between its eyes).
	TestTrue(TEXT("Gameplay head is at the visible head"), FVector::Dist(Serpent->GetHeadLocation(), Serpent->GetEyeMidpoint()) < 100.f);

	// Lamp off: dark within a fraction of a second.
	Diver->ToggleLamp();
	TestFalse(TEXT("Lamp is off"), Diver->IsLampOn());
	for (int32 i = 0; i < 10; ++i)
	{
		Serpent->Tick(0.05f);
	}
	TestTrue(TEXT("Eyes go dark with the beam off them"), Serpent->GetEyeShine01() < 0.05f);

	// Not shown in the first minute: the markings stay dark until the delay has passed and it has stalked once.
	TestTrue(TEXT("Markings dark at first"), Serpent->GetReveal01() < 0.01f);
	Serpent->RevealNow();
	TestTrue(TEXT("Reveal cheat shows the markings"), Serpent->GetReveal01() > 0.99f);

	// Ready for a pack head (the Dragon Cave dragon): a static mesh in the head socket by path, and a body material by
	// path. The eyes are a layer on the socket, so they still answer the lamp over a mesh head. An engine cone stands in.
	UDeepMonsterSettings* Settings = GetMutableDefault<UDeepMonsterSettings>();
	const FDeepSerpentBodyTuning Saved = Settings->Body;
	ON_SCOPE_EXIT { Settings->Body = Saved; };
	// No pack: a mesh path that does not load gives the code-built head (the shipped game without Dragon Cave).
	Settings->Body.MeshHead.Mesh = FSoftObjectPath(TEXT("/Game/DragonCave/Meshes/SM_NotInstalled.SM_NotInstalled"));
	ADeepSerpent* NoPack = World->SpawnActor<ADeepSerpent>(FVector(0.f, 0.f, -120000.f), FRotator::ZeroRotator);
	UTEST_NOT_NULL(TEXT("Serpent without the pack spawned"), NoPack);
	TestFalse(TEXT("Missing pack mesh: the code-built head"), NoPack->HasMeshHead());
	TestEqual(TEXT("Missing pack mesh: the body is whole"), NoPack->GetTubeVertexCount(), FMath::Max(4, Body.TubeRings) * (Sides + 1));

	// The cone (100 cm, base at Z -50, tip at +50) points along +Z: forward +Z, eyes level across Y.
	FDeepSerpentMeshHead& Cone = Settings->Body.MeshHead;
	Cone.Mesh = FSoftObjectPath(TEXT("/Engine/BasicShapes/Cone.Cone"));
	Cone.Material = Settings->Body.SkinMaterial;
	Cone.Scale = 1.f;
	Cone.SkullCm = FVector(0.f, 0.f, 10.f);
	Cone.Forward = FVector(0.f, 0.f, 1.f);
	Cone.EyeLeftCm = FVector(-15.f, -20.f, 10.f);
	Cone.EyeRightCm = FVector(-15.f, 20.f, 10.f);
	Cone.CutCm = FVector(0.f, 0.f, -40.f);
	Cone.CutNormal = FVector(0.f, 0.f, 1.f);
	Cone.KeepEndCm = FVector(0.f, 0.f, 60.f);
	ADeepSerpent* Dragon = World->SpawnActor<ADeepSerpent>(FVector(0.f, 0.f, -110000.f), FRotator::ZeroRotator);
	UTEST_NOT_NULL(TEXT("Second serpent spawned"), Dragon);
	TestTrue(TEXT("A mesh head set by path is worn in the socket"), Dragon->HasMeshHead());
	Dragon->PlaceHead(Lamp->GetComponentLocation() + Lamp->GetForwardVector() * 800.f, Facing);
	Diver->ToggleLamp();
	for (int32 i = 0; i < 10; ++i)
	{
		Dragon->Tick(0.05f);
	}
	TestTrue(TEXT("Eyes still shine over a mesh head"), Dragon->GetEyeShine01() > 0.4f);
	TestTrue(TEXT("Mesh head: gameplay head is at the visible head"), FVector::Dist(Dragon->GetHeadLocation(), Dragon->GetEyeMidpoint()) < 30.f);
	// Its eyes are where the landmarks put them: level, 40 cm apart, on the socket's right and left.
	const FVector EyeGap = Dragon->GetEyeMidpoint() - Dragon->GetHeadLocation();
	TestTrue(TEXT("Mesh head: eyes placed from its landmarks"), EyeGap.Size() > 5.f && EyeGap.Size() < 25.f);
	return true;
}

#endif
