// World-level test: an empty game world builds the arena, a diver obeys the tether, grabs salvage and winches,
// the game mode changes stop, and Deep.State produces parseable JSON.

#include "CoreMinimal.h"
#include "Debug/DeepStateReport.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepTetherComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameMode.h"
#include "Game/DeepGameState.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "World/DeepAbyssFloor.h"
#include "World/DeepArena.h"
#include "World/DeepEnvironment.h"
#include "World/DeepSalvage.h"
#include "World/DeepSubmarine.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DeepWorldTest
{
	template <typename T>
	int32 Count(UWorld* World)
	{
		int32 N = 0;
		for (TActorIterator<T> It(World); It; ++It)
		{
			N += IsValid(*It) ? 1 : 0;
		}
		return N;
	}

	/** A bare game world, destroyed when this goes out of scope. */
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

	TSharedPtr<FJsonObject> Parse(const FString& Json)
	{
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Object);
		return Object;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepWorldArenaTest, "Deep.World.ArenaAndDiver",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepWorldArenaTest::RunTest(const FString& Parameters)
{
	using namespace DeepWorldTest;
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();

	FScopedWorld Scoped;
	UWorld* World = Scoped.World;
	UTEST_NOT_NULL(TEXT("World created"), World);

	// The game mode spawns its game state; the arena builds itself from an empty map.
	ADeepGameMode* GameMode = World->SpawnActor<ADeepGameMode>();
	UTEST_NOT_NULL(TEXT("Game mode spawned"), GameMode);
	ADeepSubmarine* Sub = GameMode->EnsureArena();
	UTEST_NOT_NULL(TEXT("Submarine spawned in an empty map"), Sub);
	TestEqual(TEXT("One submarine"), Count<ADeepSubmarine>(World), 1);
	TestEqual(TEXT("One salvage piece per tuned spot"), Count<ADeepSalvage>(World), Tuning.Arena.SalvageSpots.Num());
	TestEqual(TEXT("An abyss floor"), Count<ADeepAbyssFloor>(World), 1);
	TestEqual(TEXT("An environment"), Count<ADeepEnvironment>(World), 1);

	GameMode->EnsureArena();
	TestEqual(TEXT("Second EnsureArena spawns no second sub"), Count<ADeepSubmarine>(World), 1);
	TestEqual(TEXT("Second EnsureArena spawns no extra salvage"), Count<ADeepSalvage>(World), Tuning.Arena.SalvageSpots.Num());

	const FVector Anchor = Sub->GetAnchorLocation();
	for (TActorIterator<ADeepSalvage> It(World); It; ++It)
	{
		TestTrue(TEXT("Salvage is below the airlock"), It->GetActorLocation().Z < Anchor.Z);
		TestTrue(TEXT("Salvage has mass and value from tuning"), It->GetKind().Kg > 0.f && It->GetKind().Value > 0);
	}

	// The world has no authority game mode (that needs a game instance), so start play through ours directly.
	GameMode->StartPlay();
	TestTrue(TEXT("World has begun play"), World->HasBegunPlay());
	TestEqual(TEXT("StartPlay spawns no second sub"), Count<ADeepSubmarine>(World), 1);

	ADeepDiver* Diver = World->SpawnActor<ADeepDiver>(Sub->GetDiverStartTransform().GetLocation(), FRotator::ZeroRotator);
	UTEST_NOT_NULL(TEXT("Diver spawned"), Diver);
	UDeepTetherComponent* Tether = Diver->GetTether();

	// Tether: teleporting 100 m straight down is pulled back to the stop-1 length.
	const float Stop1M = UDeepRules::TetherLengthM(1, Tuning.Tether);
	Diver->TeleportDiver(Anchor - FVector(0.f, 0.f, 100.f * UDeepRules::CmPerM));
	TestEqual(TEXT("Tether holds the diver at its length"), FVector::Dist(Diver->GetActorLocation(), Anchor) / UDeepRules::CmPerM, static_cast<double>(Stop1M), 0.05);
	TestTrue(TEXT("Tether reports taut"), Tether->GetSolve().bTaut);
	const float ExpectedDepth = Tuning.GetStop(1).SubDepthM + Stop1M;
	TestEqual(TEXT("Depth counts from the stop depth"), Diver->GetDepthM(), ExpectedDepth, 0.1f);

	// Winch: one second reels WinchSpeedMps in.
	Diver->SetWinching(true);
	Tether->UpdateTether(1.f);
	TestEqual(TEXT("Winch reels the diver in"), FVector::Dist(Diver->GetActorLocation(), Anchor) / UDeepRules::CmPerM, static_cast<double>(Stop1M - Tuning.Tether.WinchSpeedMps), 0.05);
	for (int32 Step = 0; Step < 20; ++Step)
	{
		Tether->UpdateTether(1.f);
	}
	TestTrue(TEXT("Winch brings the diver home"), Tether->IsHome());
	Diver->SetWinching(false);

	// Grab: face a scrap piece from 1.5 m away.
	ADeepSalvage* Scrap = nullptr;
	for (TActorIterator<ADeepSalvage> It(World); It; ++It)
	{
		if (It->GetKind().Size != EDeepSalvageSize::Pearl)
		{
			Scrap = *It;
			break;
		}
	}
	UTEST_NOT_NULL(TEXT("Found a scrap piece"), Scrap);
	const FDeepCargoItem Expected = Scrap->ToCargo();
	const int32 PiecesBefore = Count<ADeepSalvage>(World);
	Diver->TeleportDiver(Scrap->GetActorLocation() - FVector(150.f, 0.f, 55.f));
	TestTrue(TEXT("Scrap is the grab target"), Diver->FindGrabTarget() == Scrap);
	Diver->Grab();
	TestEqual(TEXT("Grab adds the piece's kg"), Diver->GetCargoKg(), Expected.Kg, 0.001f);
	TestEqual(TEXT("Grab adds the piece's value"), Diver->GetCargoValue(), Expected.Value);
	TestEqual(TEXT("Grabbed piece leaves the world"), Count<ADeepSalvage>(World), PiecesBefore - 1);

	Diver->Drop();
	TestEqual(TEXT("Drop empties cargo"), Diver->GetCargoKg(), 0.f, 0.001f);
	TestEqual(TEXT("Dropped piece is back in the world"), Count<ADeepSalvage>(World), PiecesBefore);

	// Weight from cargo.
	FDeepCargoItem Heavy;
	Heavy.Size = EDeepSalvageSize::Large;
	Heavy.Kg = Tuning.Weight.OverloadKg;
	Heavy.Value = 200;
	Diver->AddCargo(Heavy);
	TestTrue(TEXT("Heavy cargo overloads the diver"), Diver->GetWeight().bOverloaded);

	// Stop change: longer tether.
	GameMode->GotoStop(2);
	const ADeepGameState* State = World->GetGameState<ADeepGameState>();
	UTEST_NOT_NULL(TEXT("Game state exists"), State);
	TestEqual(TEXT("GotoStop sets the stop"), State->Stop, 2);
	Diver->TeleportDiver(Anchor - FVector(0.f, 0.f, 100.f * UDeepRules::CmPerM));
	TestEqual(TEXT("Stop 2 tether is longer"), Tether->GetSolve().LengthM, UDeepRules::TetherLengthM(2, Tuning.Tether), 0.01f);
	GameMode->GotoStop(99);
	TestEqual(TEXT("GotoStop clamps to the stop table"), State->Stop, Tuning.NumStops());

	// A few real ticks must not break anything.
	for (int32 Frame = 0; Frame < 5; ++Frame)
	{
		World->Tick(LEVELTICK_All, 1.f / 30.f);
	}

	// Deep.State JSON.
	const TSharedPtr<FJsonObject> Json = Parse(DeepStateReport::BuildJson(World));
	UTEST_TRUE(TEXT("DEEPSTATE is valid JSON"), Json.IsValid());
	TestTrue(TEXT("Reports the diver"), Json->GetBoolField(TEXT("diver_present")));
	TestEqual(TEXT("Reports cargo kg"), Json->GetNumberField(TEXT("cargo_kg")), static_cast<double>(Heavy.Kg), 0.001);
	TestEqual(TEXT("Reports cargo value"), static_cast<int32>(Json->GetNumberField(TEXT("cargo_value"))), Heavy.Value);
	TestEqual(TEXT("Reports the stop"), static_cast<int32>(Json->GetNumberField(TEXT("stop"))), Tuning.NumStops());
	TestTrue(TEXT("Reports a zone"), !Json->GetStringField(TEXT("zone")).IsEmpty());
	TestTrue(TEXT("Reports depth"), Json->GetNumberField(TEXT("depth_m")) > 0.0);
	TestTrue(TEXT("Reports tether length"), Json->GetNumberField(TEXT("tether_length_m")) > 0.0);
	TestTrue(TEXT("Reports strain"), Json->HasField(TEXT("tether_strain")));
	TestTrue(TEXT("Reports credits"), Json->HasField(TEXT("credits")));
	TestTrue(TEXT("Reports hull rating"), Json->HasField(TEXT("hull_rating")));
	TestEqual(TEXT("Reports a 3-number position"), Json->GetArrayField(TEXT("pos_cm")).Num(), 3);
	TestTrue(TEXT("Log line has the DEEPSTATE prefix"), DeepStateReport::LogState(World).StartsWith(TEXT("DEEPSTATE {")));

	for (const TCHAR* Command : { TEXT("Deep.State"), TEXT("Deep.GiveSalvage"), TEXT("Deep.GotoStop"), TEXT("Deep.Teleport"), TEXT("Deep.Look") })
	{
		TestNotNull(FString::Printf(TEXT("%s is registered"), Command), IConsoleManager::Get().FindConsoleObject(Command));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepWorldGreyBoxTest, "Deep.World.GreyBoxLight",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepWorldGreyBoxTest::RunTest(const FString& Parameters)
{
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	// The polish pass (D-033) turns the water look on; the grey box stays as a switch.
	TestFalse(TEXT("The water look is on by default"), FDeepArenaTuning().bGreyBox);
	TestTrue(TEXT("Backdrop fog is near zero"), FDeepArenaTuning().GreyBoxBackdropFogDensity <= 0.001f);
	TestTrue(TEXT("Key light brighter than fill"), FDeepArenaTuning().GreyBoxKeyLux > FDeepArenaTuning().GreyBoxFillLux && FDeepArenaTuning().GreyBoxFillLux > 0.f);

	// Every stop's light is close to white: stops differ by a mild tint at most.
	for (int32 Stop = 1; Stop <= Tuning.Stops.Num(); ++Stop)
	{
		const FLinearColor Tint = ADeepEnvironment::GreyBoxTint(Tuning.GetStop(Stop), FDeepArenaTuning());
		TestTrue(FString::Printf(TEXT("Stop %d light is mild"), Stop), FMath::Min3(Tint.R, Tint.G, Tint.B) >= 0.75f && FMath::Max3(Tint.R, Tint.G, Tint.B) <= 1.f + KINDA_SMALL_NUMBER);
	}
	FDeepStopTuning Black;
	Black.FogColor = FLinearColor::Black;
	TestTrue(TEXT("A black water colour gives white light"), ADeepEnvironment::GreyBoxTint(Black, FDeepArenaTuning()).Equals(FLinearColor::White));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepWorldLookBlendTest, "Deep.World.LookBlend",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepWorldLookBlendTest::RunTest(const FString& Parameters)
{
	const FDeepLookTuning& Look = UDeepTuningSettings::Get().Look;
	TestEqual(TEXT("Three presets: 100, 300, 500 m"), Look.Presets.Num(), 3);
	if (Look.Presets.Num() != 3)
	{
		return false;
	}
	// The presets hold at their depths and clamp beyond the ends.
	TestEqual(TEXT("100 m fog"), ADeepEnvironment::BlendLook(Look, 100.f).FogDensity, Look.Presets[0].FogDensity);
	TestEqual(TEXT("Above 100 m clamps"), ADeepEnvironment::BlendLook(Look, 0.f).DownlightLux, 2.f);
	TestEqual(TEXT("300 m exposure"), ADeepEnvironment::BlendLook(Look, 300.f).ExposureBias, 1.5f);
	TestEqual(TEXT("Below 500 m clamps, no downlight"), ADeepEnvironment::BlendLook(Look, 700.f).DownlightLux, 0.f);
	// Driving blends continuously: halfway between stops is halfway between their numbers.
	const FDeepLookPreset Mid = ADeepEnvironment::BlendLook(Look, 200.f);
	TestTrue(TEXT("200 m fog is halfway"), FMath::IsNearlyEqual(Mid.FogDensity, (Look.Presets[0].FogDensity + Look.Presets[1].FogDensity) * 0.5f, 1e-4f));
	TestTrue(TEXT("200 m exposure is halfway"), FMath::IsNearlyEqual(Mid.ExposureBias, 0.75f, 1e-4f));
	// Darker with depth; the 500 m far fog is black (style guide 3).
	TestTrue(TEXT("500 m water is black"), Look.Presets[2].FogColor.Equals(FLinearColor(0.f, 0.f, 0.f, 1.f)));
	TestTrue(TEXT("Light falls with depth"), Look.Presets[0].DownlightLux > Look.Presets[1].DownlightLux && Look.Presets[1].DownlightLux > Look.Presets[2].DownlightLux);
	TestNotNull(TEXT("Deep.RayTracing is registered"), IConsoleManager::Get().FindConsoleObject(TEXT("Deep.RayTracing")));
	// At 500 m the sub's lights stop lighting the water, so it stays black with the lamp off.
	TestEqual(TEXT("500 m: sub lights do not light the fog"), ADeepEnvironment::BlendLook(Look, 500.f).SubLightFog, 0.f);
	TestTrue(TEXT("Sub lights dim with depth"), Look.Presets[0].SubLightScale > Look.Presets[1].SubLightScale && Look.Presets[1].SubLightScale > Look.Presets[2].SubLightScale);

	// The lamp up close: full in open water and past NearFullM; inverse-square compensation below it, with a floor.
	FDeepLampTuning Lamp;
	Lamp.NearFullM = 6.f;
	Lamp.NearMinScale = 0.05f;
	TestEqual(TEXT("Lamp: nothing ahead, full"), UDeepRules::LampNearScale(-1.f, Lamp), 1.f);
	TestEqual(TEXT("Lamp: 10 m, full"), UDeepRules::LampNearScale(10.f, Lamp), 1.f);
	TestTrue(TEXT("Lamp: 3 m, a quarter"), FMath::IsNearlyEqual(UDeepRules::LampNearScale(3.f, Lamp), 0.25f, 1e-4f));
	TestEqual(TEXT("Lamp: touching, the floor"), UDeepRules::LampNearScale(0.1f, Lamp), 0.05f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeepWorldSeabedTest, "Deep.World.Seabed",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FDeepWorldSeabedTest::RunTest(const FString& Parameters)
{
	const UDeepTuningSettings& Tuning = UDeepTuningSettings::Get();
	const TArray<FVector4> Rest = ADeepAbyssFloor::RestPointsFromTuning();
	TestEqual(TEXT("A resting point per salvage spot plus the relic"), Rest.Num(), Tuning.Arena.SalvageSpots.Num() + 1);
	// Salvage lies on the bed: at each resting point the bed is within 25 cm of it.
	for (const FVector4& R : Rest)
	{
		const float Below = ADeepAbyssFloor::SeabedBelowM(FVector2D(R.X, R.Y), Rest, Tuning.Look);
		TestTrue(*FString::Printf(TEXT("Bed meets the salvage at (%.0f, %.0f): %.2f vs %.2f m"), R.X, R.Y, Below, R.Z), FMath::Abs(Below - R.Z) < 0.25f);
	}
	// The hatch column is clear: under the anchor the bed is well below the hull, and it falls away into the dark.
	TestTrue(TEXT("Bed is at least 6 m under the anchor"), ADeepAbyssFloor::SeabedBelowM(FVector2D::ZeroVector, Rest, Tuning.Look) > 6.f);
	TestTrue(TEXT("Bed falls away far out"), ADeepAbyssFloor::SeabedBelowM(FVector2D(150.f, 0.f), Rest, Tuning.Look) > 40.f);
	// A scanned material can be slotted in by path; the made one is the default.
	TestEqual(TEXT("Seabed material by path"), Tuning.Look.SeabedMaterial.ToString(), FString(TEXT("/Game/Deep/Look/M_Silt.M_Silt")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
