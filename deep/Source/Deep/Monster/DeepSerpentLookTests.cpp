// The serpent's look: the tube body is built, the eyes shine only with a lamp on them, the markings stay dark
// in the first minute. Behaviour is covered by Deep.World.Monster; this checks only what is drawn.

#include "CoreMinimal.h"
#include "Components/SpotLightComponent.h"
#include "Diver/DeepDiver.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/DeepGameMode.h"
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
	TestFalse(TEXT("No mesh head unless one is set"), Serpent->HasMeshHead());

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
	Settings->Body.HeadMesh = FSoftObjectPath(TEXT("/Engine/BasicShapes/Cone.Cone"));
	Settings->Body.HeadMeshRotation = FRotator(-90.f, 0.f, 0.f);
	Settings->Body.HeadMeshMaterial = Settings->Body.SkinMaterial;
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
	return true;
}

#endif
