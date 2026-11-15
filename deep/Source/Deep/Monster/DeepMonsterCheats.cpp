// Sea serpent console commands. Each ends by logging one `DEEPMONSTER {json}` line (see UDeepMonsterBrain::BuildStateJson).

#include "CoreMinimal.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Diver/DeepDiver.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Monster/DeepMonsterSettings.h"
#include "Diver/DeepTetherComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Monster/DeepSerpent.h"
#include "Monster/DeepMonsterBrain.h"
#include "Monster/DeepMonsterRules.h"
#include "Monster/DeepSerpentController.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepMonsterCheats, Log, All);

namespace
{
	UDeepMonsterBrain* RequireBrain(UWorld* World, const TCHAR* Command)
	{
		UDeepMonsterBrain* Brain = World ? World->GetSubsystem<UDeepMonsterBrain>() : nullptr;
		if (!Brain)
		{
			UE_LOG(LogDeepMonsterCheats, Warning, TEXT("%s: no monster brain in this world (game or PIE worlds only)"), Command);
		}
		return Brain;
	}

	void LogMonster(UDeepMonsterBrain* Brain)
	{
		UE_LOG(LogDeepMonsterCheats, Display, TEXT("DEEPMONSTER %s"), *Brain->BuildStateJson());
	}

	FAutoConsoleCommandWithWorldAndArgs MonsterStateCommand(
		TEXT("Deep.Monster.State"),
		TEXT("Logs one line: DEEPMONSTER {json} with state, noise, lit seconds, cooldown, distance to tether and diver (m), threat, stop."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDeepMonsterBrain* Brain = RequireBrain(World, TEXT("Deep.Monster.State")))
			{
				LogMonster(Brain);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs MonsterSpawnCommand(
		TEXT("Deep.Monster.Spawn"),
		TEXT("Spawns the sea serpent now, at any stop, in the dark at the lurk radius."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDeepMonsterBrain* Brain = RequireBrain(World, TEXT("Deep.Monster.Spawn")))
			{
				Brain->SpawnSerpent(true);
				LogMonster(Brain);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs MonsterPlaceCommand(
		TEXT("Deep.Monster.Place"),
		TEXT("Deep.Monster.Place <metres> [side_deg] [right_m] [up_m]: puts the serpent's head that far in front of the view (and right_m to the right, up_m up), swimming across it (side_deg turns its heading; default 90 = left to right)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UDeepMonsterBrain* Brain = RequireBrain(World, TEXT("Deep.Monster.Place"));
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			if (!Brain || !Player)
			{
				return;
			}
			const float Metres = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 12.f;
			const float SideDeg = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 90.f;
			FVector ViewLocation;
			FRotator ViewRotation;
			Player->GetPlayerViewPoint(ViewLocation, ViewRotation);
			FVector Forward = ViewRotation.Vector();
			ADeepSerpent* Serpent = Brain->SpawnSerpent(true);
			if (!Serpent)
			{
				return;
			}
			// Heading: the view's forward turned SideDeg about up, kept level so the body lies across the screen.
			const FVector Level = FVector(Forward.X, Forward.Y, 0.f).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
			const FVector Facing = Level.RotateAngleAxis(SideDeg, FVector::UpVector);
			// Optional offsets to the view's right and up (metres): put the head outside the beam so only the body
			// crosses it.
			const float RightM = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 0.f;
			const float UpM = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.f;
			const FVector Right = FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Y);
			const FVector Up = FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Z);
			Serpent->PlaceHead(ViewLocation + (Forward * Metres + Right * RightM + Up * UpM) * 100.f, Facing);
			LogMonster(Brain);
		}));

	FAutoConsoleCommandWithWorldAndArgs MonsterPoseCommand(
		TEXT("Deep.Monster.Pose"),
		TEXT("Deep.Monster.Pose <state|-1>: shots only. The serpent shows that state's motion and markings (0 lurk, 1 stalk, 2 recoil, 3 bite, 4 hunt, 5 pass) at full threat; the brain is untouched. -1 turns it off."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDeepMonsterBrain* Brain = RequireBrain(World, TEXT("Deep.Monster.Pose")))
			{
				if (ADeepSerpent* Serpent = Brain->SpawnSerpent(true))
				{
					Serpent->SetPoseOverride(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : -1);
				}
				LogMonster(Brain);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs MonsterNoiseCommand(
		TEXT("Deep.Monster.Noise"),
		TEXT("Deep.Monster.Noise <amount>: adds noise as if the diver made it (60 brings it in by default)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogDeepMonsterCheats, Warning, TEXT("Usage: Deep.Monster.Noise <amount>"));
				return;
			}
			if (UDeepMonsterBrain* Brain = RequireBrain(World, TEXT("Deep.Monster.Noise")))
			{
				// Through the engine hearing sense at the diver, like real noise; straight in if there is no diver.
				const float Amount = FCString::Atof(*Args[0]);
				TActorIterator<ADeepDiver> It(World);
				if (It)
				{
					UDeepMonsterRules::ReportDiverNoise(World, It->GetActorLocation(), Amount, *It);
				}
				else
				{
					Brain->AddNoise(Amount);
				}
				LogMonster(Brain);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs MonsterLitCommand(
		TEXT("Deep.Monster.Lit"),
		TEXT("Deep.Monster.Lit <seconds>: pretends the lamp beam is on its head for that long."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogDeepMonsterCheats, Warning, TEXT("Usage: Deep.Monster.Lit <seconds>"));
				return;
			}
			if (UDeepMonsterBrain* Brain = RequireBrain(World, TEXT("Deep.Monster.Lit")))
			{
				Brain->SetDebugLit(FCString::Atof(*Args[0]));
				LogMonster(Brain);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs MonsterFirstSightCommand(
		TEXT("Deep.Monster.FirstSight"),
		TEXT("The first sight now: the first-heard sound, then one slow pass across the diver's lamp at the edge of its reach, then it leaves and hunts as normal."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDeepMonsterBrain* Brain = RequireBrain(World, TEXT("Deep.Monster.FirstSight")))
			{
				Brain->SpawnSerpent(true);
				if (!Brain->StartFirstSight())
				{
					UE_LOG(LogDeepMonsterCheats, Warning, TEXT("Deep.Monster.FirstSight: no diver to show it to"));
				}
				LogMonster(Brain);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs MonsterStageBiteCommand(
		TEXT("Deep.Monster.StageBite"),
		TEXT("Deep.Monster.StageBite [side_m] [out_m]: shots. Moves the diver out_m (default 12) out from the anchor along the view and 4 m down, turns the view back to the bite point on the tether, puts the head side_m (default 8) to its right, and sends it stalking: it strikes across the line from the side."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UDeepMonsterBrain* Brain = RequireBrain(World, TEXT("Deep.Monster.StageBite"));
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			ADeepDiver* Diver = Player ? Cast<ADeepDiver>(Player->GetPawn()) : nullptr;
			if (!Brain || !Diver)
			{
				return;
			}
			Brain->SpawnSerpent(true);
			ADeepSerpentController* Mind = Brain->GetSerpentController();
			ADeepSerpent* Serpent = Brain->GetSerpent();
			if (!Mind || !Serpent)
			{
				return;
			}
			FVector Eye;
			FRotator View;
			Player->GetPlayerViewPoint(Eye, View);
			if (const UDeepTetherComponent* Tether = Diver->GetTether())
			{
				// Out in the water on a real length of line, so there is a line to bite between the diver and the sub.
				const float OutM = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 12.f;
				const FVector Out = FRotator(0.f, View.Yaw, 0.f).Vector();
				Diver->TeleportDiver(Tether->GetAnchorLocation() + Out * (OutM * 100.f) - FVector(0.f, 0.f, 400.f));
				// The camera manager's view point is a frame behind a teleport; the pawn's own view location is not.
				Eye = Diver->GetPawnViewLocation();
			}
			const FVector Point = Mind->GetBitePoint(Diver);
			const FRotator Look = (Point - Eye).Rotation();
			Player->SetControlRotation(Look);
			const float SideM = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 8.f;
			const FVector Right = FRotationMatrix(FRotator(0.f, Look.Yaw, 0.f)).GetUnitAxis(EAxis::Y);
			Serpent->PlaceHead(Point + Right * (SideM * 100.f), -Right);
			Brain->ForceState(EDeepMonsterState::Stalk);
			Brain->AddNoise(100.f);
			LogMonster(Brain);
		}));

	/** The look-dev light (one per world): a plain white directional light from over the view's left shoulder. */
	TWeakObjectPtr<ADirectionalLight> GPlainLight;

	void SetPlainLight(UWorld* World, float Lux)
	{
		APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
		if (!Player)
		{
			return;
		}
		if (Lux <= 0.f)
		{
			if (GPlainLight.IsValid())
			{
				GPlainLight->Destroy();
			}
			GPlainLight.Reset();
			return;
		}
		FVector Eye;
		FRotator View;
		Player->GetPlayerViewPoint(Eye, View);
		if (!GPlainLight.IsValid() || GPlainLight->GetWorld() != World)
		{
			GPlainLight = World->SpawnActor<ADirectionalLight>(Eye, FRotator::ZeroRotator);
		}
		if (ADirectionalLight* Light = GPlainLight.Get())
		{
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetActorRotation(FRotator(-40.f, View.Yaw + 35.f, 0.f));
			if (UDirectionalLightComponent* Component = Cast<UDirectionalLightComponent>(Light->GetLightComponent()))
			{
				Component->SetIntensity(Lux);
				Component->SetLightColor(FLinearColor::White);
				// No shadows: the hull above the hatch would shade everything near the sub.
				Component->SetCastShadows(false);
				Component->SetAtmosphereSunLight(false);
			}
		}
	}

	FAutoConsoleCommandWithWorldAndArgs MonsterPlainLightCommand(
		TEXT("Deep.Monster.PlainLight"),
		TEXT("Deep.Monster.PlainLight <lux|0>: look-dev only. A plain white directional light from over the view's left shoulder (0 removes it). Pair with ShowFlag.Fog 0 and ShowFlag.VolumetricFog 0 for a frame in plain light."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			SetPlainLight(World, Args.Num() > 0 ? FCString::Atof(*Args[0]) : 4.f);
		}));

	FAutoConsoleCommandWithWorldAndArgs MonsterSpecimensCommand(
		TEXT("Deep.Monster.Specimens"),
		TEXT("Deep.Monster.Specimens [cell_m] [clip 0|1] [lux]: look-dev contact sheet. The four Dragon Cave poses (rows) seen from the side, front, top and three-quarter (columns), each scaled into a cell in front of the view, under a plain light. clip 1 wears the serpent's head material with its clip."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			if (!Player)
			{
				return;
			}
			const float CellCm = (Args.Num() > 0 ? FCString::Atof(*Args[0]) : 2.4f) * 100.f;
			const bool bClip = Args.Num() > 1 && FCString::Atoi(*Args[1]) != 0;
			SetPlainLight(World, Args.Num() > 2 ? FCString::Atof(*Args[2]) : 4.f);
			// The camera as drawn (the view point lags a frame behind Deep.Look and teleports).
			FVector Eye;
			FRotator View;
			Player->GetPlayerViewPoint(Eye, View);
			if (Player->PlayerCameraManager)
			{
				Eye = Player->PlayerCameraManager->GetCameraLocation();
				View = Player->PlayerCameraManager->GetCameraRotation();
			}
			const FRotationMatrix ViewAxes(View);
			const FVector F = ViewAxes.GetUnitAxis(EAxis::X);
			const FVector R = ViewAxes.GetUnitAxis(EAxis::Y);
			const FVector U = ViewAxes.GetUnitAxis(EAxis::Z);
			// Dragon frame (pose 4): head along +Y, up +Z. Each column turns that frame to show one side to the view.
			const FVector Q = (R - F).GetSafeNormal();
			const FVector Heads[4] = { R, -F, U, Q };
			const FVector Ups[4] = { U, U, -F, (U - F * 0.4f).GetSafeNormal() };
			const FDeepSerpentMeshHead& HeadTuning = UDeepMonsterSettings::Get().Body.MeshHead;
			UMaterialInterface* ClipBase = bClip ? Cast<UMaterialInterface>(HeadTuning.Material.TryLoad()) : nullptr;
			const FVector Centre = Eye + F * (CellCm * 4.6f);
			for (int32 Pose = 0; Pose < 4; ++Pose)
			{
				const FString Path = FString::Printf(TEXT("/Game/DragonCave/Meshes/SM_DragonPose0%d.SM_DragonPose0%d"), Pose + 1, Pose + 1);
				UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
				if (!Mesh)
				{
					UE_LOG(LogDeepMonsterCheats, Warning, TEXT("Deep.Monster.Specimens: %s not found (pack not installed?)"), *Path);
					continue;
				}
				const FBoxSphereBounds Bounds = Mesh->GetBounds();
				const float Scale = CellCm * 0.92f / FMath::Max(1.f, static_cast<float>(Bounds.BoxExtent.GetMax() * 2.0));
				for (int32 Col = 0; Col < 4; ++Col)
				{
					// Mesh +Y -> Heads[Col], mesh +Z -> Ups[Col]: rotation from the basis (X = Y x Z, Y, Z).
					const FVector Y = Heads[Col];
					const FVector Z = (Ups[Col] - Y * FVector::DotProduct(Ups[Col], Y)).GetSafeNormal();
					const FVector X = FVector::CrossProduct(Y, Z);
					const FQuat Rot(FMatrix(X, Y, Z, FVector::ZeroVector));
					const FVector Cell = Centre + R * ((Col - 1.5f) * CellCm) + U * ((1.5f - Pose) * CellCm);
					const FVector Location = Cell - Rot.RotateVector(Bounds.Origin * Scale);
					AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Location, Rot.Rotator());
					if (!Actor)
					{
						continue;
					}
					Actor->SetMobility(EComponentMobility::Movable);
					UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
					Component->SetStaticMesh(Mesh);
					Component->SetWorldScale3D(FVector(Scale));
					Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					if (ClipBase)
					{
						UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ClipBase, Actor);
						MID->SetVectorParameterValue(TEXT("CutOrigin"), FLinearColor(HeadTuning.CutCm));
						MID->SetVectorParameterValue(TEXT("CutNormal"), FLinearColor(HeadTuning.CutNormal.GetSafeNormal()));
						MID->SetVectorParameterValue(TEXT("KeepEnd"), FLinearColor(HeadTuning.KeepEndCm));
						MID->SetScalarParameterValue(TEXT("KeepRadius"), HeadTuning.KeepRadiusCm);
						MID->SetScalarParameterValue(TEXT("ColorGain"), 1.f);
						for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
						{
							Component->SetMaterial(Slot, MID);
						}
					}
				}
				UE_LOG(LogDeepMonsterCheats, Display, TEXT("DEEPSPECIMEN pose=%d tris=%d nanite=%d bounds_cm=%s slots=%d scale=%.5f"),
					Pose + 1, Mesh->GetNumTriangles(0), Mesh->IsNaniteEnabled() ? 1 : 0, *(Bounds.BoxExtent * 2.0).ToString(),
					Mesh->GetStaticMaterials().Num(), Scale);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs MonsterParamCommand(
		TEXT("Deep.Monster.Param"),
		TEXT("Deep.Monster.Param <head|body> <name> <x> [y z]: look-dev. Sets a parameter on the serpent's mesh head or body material now (one value = scalar, three = vector)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UDeepMonsterBrain* Brain = RequireBrain(World, TEXT("Deep.Monster.Param"));
			ADeepSerpent* Serpent = Brain ? Brain->SpawnSerpent(true) : nullptr;
			if (!Serpent || Args.Num() < 3)
			{
				UE_LOG(LogDeepMonsterCheats, Warning, TEXT("Usage: Deep.Monster.Param <head|body> <name> <x> [y z]"));
				return;
			}
			UMaterialInstanceDynamic* MID = Serpent->GetLookMID(Args[0].Equals(TEXT("head"), ESearchCase::IgnoreCase));
			if (!MID)
			{
				UE_LOG(LogDeepMonsterCheats, Warning, TEXT("Deep.Monster.Param: no %s material"), *Args[0]);
				return;
			}
			if (Args.Num() >= 5)
			{
				MID->SetVectorParameterValue(FName(*Args[1]), FLinearColor(FCString::Atof(*Args[2]), FCString::Atof(*Args[3]), FCString::Atof(*Args[4])));
			}
			else
			{
				MID->SetScalarParameterValue(FName(*Args[1]), FCString::Atof(*Args[2]));
			}
			UE_LOG(LogDeepMonsterCheats, Display, TEXT("DEEPMONSTERPARAM %s %s set"), *Args[0], *Args[1]);
		}));

	FAutoConsoleCommandWithWorldAndArgs MonsterRevealCommand(
		TEXT("Deep.Monster.Reveal"),
		TEXT("Shows the serpent's markings now, skipping the first-minute rule (for shots)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDeepMonsterBrain* Brain = RequireBrain(World, TEXT("Deep.Monster.Reveal")))
			{
				if (ADeepSerpent* Serpent = Brain->SpawnSerpent(true))
				{
					Serpent->RevealNow();
				}
				LogMonster(Brain);
			}
		}));
}
