#include "Debug/DeepStateReport.h"

#include "Audio/DeepAudioSubsystem.h"
#include "Camera/DeepCameraSubsystem.h"
#include "Diver/DeepDiver.h"
#include "Diver/DeepTetherComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DeepGameState.h"
#include "GameFramework/PlayerController.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Sim/DeepRules.h"
#include "Diver/DeepRunComponent.h"
#include "Harpoon/DeepHarpoonComponent.h"
#include "Monster/DeepMonsterBrain.h"
#include "Monster/DeepMonsterRules.h"
#include "Monster/DeepSerpent.h"
#include "Story/DeepCaptain.h"
#include "Story/DeepEventBus.h"
#include "World/DeepArena.h"
#include "World/DeepSalvage.h"
#include "World/DeepSubmarine.h"
#include "Camera/CameraComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepState, Log, All);

namespace DeepStateReport
{
	ADeepDiver* FindDiver(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		if (APlayerController* Player = World->GetFirstPlayerController())
		{
			if (ADeepDiver* Diver = Cast<ADeepDiver>(Player->GetPawn()))
			{
				return Diver;
			}
		}
		for (TActorIterator<ADeepDiver> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				return *It;
			}
		}
		return nullptr;
	}

	FString BuildJson(UWorld* World)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		ADeepDiver* Diver = FindDiver(World);
		const UDeepTetherComponent* Tether = Diver ? Diver->GetTether() : nullptr;
		const FDeepTetherSolve Solve = Tether ? Tether->GetSolve() : FDeepTetherSolve();
		const FDeepWeightResult Weight = Diver ? Diver->GetWeight() : FDeepWeightResult();

		Json->SetBoolField(TEXT("diver_present"), Diver != nullptr);
		Json->SetNumberField(TEXT("depth_m"), Diver ? Diver->GetDepthM() : 0.f);
		Json->SetStringField(TEXT("zone"), Diver ? Diver->GetZoneName() : FString());
		Json->SetNumberField(TEXT("cargo_kg"), Weight.CarriedKg);
		Json->SetNumberField(TEXT("cargo_value"), Diver ? Diver->GetCargoValue() : 0);
		Json->SetNumberField(TEXT("cargo_items"), Diver ? Diver->GetCargo().Num() : 0);
		Json->SetBoolField(TEXT("overloaded"), Weight.bOverloaded);
		Json->SetNumberField(TEXT("tether_length_m"), Solve.LengthM);
		// The distance to the anchor is measured, not taken from the solve: after a cut the solve stops updating.
		const bool bMeasure = Diver && Tether && Tether->GetSubmarine();
		const float AnchorM = bMeasure ? FVector::Dist(Diver->GetActorLocation(), Tether->GetAnchorLocation()) / UDeepRules::CmPerM : Solve.DistanceM;
		Json->SetNumberField(TEXT("tether_distance_m"), AnchorM);
		Json->SetNumberField(TEXT("tether_left_m"), Solve.LeftM);
		Json->SetNumberField(TEXT("tether_strain"), Solve.Strain);
		Json->SetBoolField(TEXT("tether_taut"), Solve.bTaut);
		Json->SetBoolField(TEXT("winching"), Diver && Diver->IsWinching());
		Json->SetBoolField(TEXT("home"), Tether && Tether->IsHome());
		Json->SetBoolField(TEXT("lamp"), Diver && Diver->IsLampOn());

		const UDeepRunComponent* Run = Diver ? Diver->GetRun() : nullptr;
		Json->SetStringField(TEXT("run_state"), Run ? UDeepRunComponent::StateName(Run->GetState()) : FString(TEXT("none")));
		Json->SetNumberField(TEXT("patience"), Run ? Run->GetPatienceFraction() : 0.f);
		Json->SetNumberField(TEXT("patience_s"), Run ? Run->GetPatienceSeconds() : 0.f);
		Json->SetNumberField(TEXT("bailout_air_s"), Run ? Run->GetBailoutAirSeconds() : 0.f);
		if (Run && Run->GetSettlement().bValid)
		{
			const FDeepSettlementReport& Report = Run->GetSettlement();
			TSharedRef<FJsonObject> Bank = MakeShared<FJsonObject>();
			Bank->SetNumberField(TEXT("gross"), Report.Gross);
			Bank->SetNumberField(TEXT("cut"), Report.DirectorCut);
			Bank->SetNumberField(TEXT("fees"), Report.Fees);
			Bank->SetNumberField(TEXT("net"), Report.Net);
			Bank->SetBoolField(TEXT("hull_bought"), Report.bHullBought);
			Json->SetObjectField(TEXT("last_bank"), Bank);
		}
		const UDeepEventBus* Bus = UDeepEventBus::Get(World);
		Json->SetStringField(TEXT("last_event"), Bus ? Bus->GetLastEvent().ToString() : FString());
		const UDeepCaptain* Captain = UDeepCaptain::Get(World);
		Json->SetStringField(TEXT("captain_line"), Captain ? Captain->GetLastText() : FString());

		const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
		Json->SetNumberField(TEXT("credits"), State ? State->Credits : 0);
		Json->SetNumberField(TEXT("hull_rating"), State ? State->HullRating : 1);
		Json->SetNumberField(TEXT("stop"), State ? State->Stop : 1);
		Json->SetNumberField(TEXT("diver_number"), State ? State->DiverNumber : 1);
		Json->SetNumberField(TEXT("hull_damage"), State ? State->HullDamage : 0);
		Json->SetNumberField(TEXT("hull_repairs_owed"), State ? State->HullRepairsOwed : 0);
		if (const UDeepHarpoonComponent* Harpoon = Diver ? Diver->GetHarpoon() : nullptr)
		{
			Json->SetObjectField(TEXT("harpoon"), DeepHarpoon::BuildStateJson(*Harpoon));
		}
		Json->SetBoolField(TEXT("on_title"), State && State->bOnTitle);
		Json->SetBoolField(TEXT("won"), State && State->bWon);
		Json->SetBoolField(TEXT("inside"), Run && Run->IsInside());
		Json->SetBoolField(TEXT("seated"), Run && Run->IsSeated());
		Json->SetNumberField(TEXT("airlock"), Run && Run->IsInAirlock() ? static_cast<int32>(Run->GetAirlockState().Phase) : 0);
		Json->SetNumberField(TEXT("valve"), Run ? Run->GetValveTurn01() : 0.f);
		Json->SetNumberField(TEXT("sub_depth_m"), State ? State->SubDepthM : 0.f);
		Json->SetNumberField(TEXT("depth_limit_m"), State ? State->DepthLimitM() : 0.f);
		// The cave shaft: the sideways offset from the stops' axis (sub frame, m) and the hull left before a breach.
		Json->SetNumberField(TEXT("sub_offset_x_m"), State ? State->SubOffsetM.X : 0.f);
		Json->SetNumberField(TEXT("sub_offset_y_m"), State ? State->SubOffsetM.Y : 0.f);
		// The helm's levers and the sub's heading (Sim/DeepPilotRules): notch -2..3, ballast -1 blown .. 1 flooded.
		Json->SetNumberField(TEXT("sub_yaw_deg"), State ? State->SubYawDeg : 0.f);
		// Deep.World.Caves: the baked chunk string is the rock, and parked means at one of its berths.
		Json->SetBoolField(TEXT("world_caves"), State && State->bWorldCaves);
		Json->SetBoolField(TEXT("parked"), State && State->bParked);
		Json->SetNumberField(TEXT("throttle_notch"), State ? State->HelmThrottleNotch : 0);
		Json->SetNumberField(TEXT("ballast"), State ? State->HelmBallast : 0.f);
		Json->SetNumberField(TEXT("helm_speed_mps"), State ? State->HelmSpeedMps : 0.f);
		// D-070: the helm's mode and what the pilot holds (ahead, turn, down, strafe; -1..1 each).
		Json->SetStringField(TEXT("pilot_mode"), !State || State->IsHelmDirect() ? TEXT("direct") : TEXT("levers"));
		{
			const FDeepDriveInput Hold = State ? State->HelmHold : FDeepDriveInput();
			Json->SetStringField(TEXT("drive_hold"), FString::Printf(TEXT("%.2f %.2f %.2f %.2f"), Hold.Surge, Hold.Turn, Hold.Heave, Hold.Strafe));
		}
		// D1b: the pilot's view on this machine: chase or cockpit, the hull top's place on the frame (0 the centre, 1 the
		// bottom edge), the chase height over the trail, the floods' and fill's share for near rock, the work lamps placed.
		if (const ADeepSubmarine* Sub = Tether ? Tether->GetSubmarine() : nullptr; Sub && Run && Run->IsSeated())
		{
			TSharedRef<FJsonObject> View = MakeShared<FJsonObject>();
			View->SetStringField(TEXT("mode"), Sub->IsHelmCockpit() ? TEXT("cockpit") : TEXT("chase"));
			View->SetNumberField(TEXT("hull_top_y"), Sub->GetHullTopFrameY());
			View->SetNumberField(TEXT("chase_up_m"), Sub->GetChaseUpM());
			View->SetStringField(TEXT("flood_near"), FString::Printf(TEXT("%.2f %.2f"), Sub->GetFloodNear(0), Sub->GetFloodNear(1)));
			View->SetNumberField(TEXT("fill_near"), Sub->GetFillNear());
			View->SetBoolField(TEXT("work_lamps"), Sub->AreWorkLampsPlaced());
			const FBox Box = Sub->GetSolidBoundsLocal(false);
			View->SetStringField(TEXT("hull_box_m"), FString::Printf(TEXT("%.2f %.2f %.2f / %.2f %.2f %.2f"), Box.Min.X / 100.0, Box.Min.Y / 100.0, Box.Min.Z / 100.0,
				Box.Max.X / 100.0, Box.Max.Y / 100.0, Box.Max.Z / 100.0));
			if (const UCameraComponent* Camera = Sub->GetHelmCamera())
			{
				const FVector Off = Sub->GetActorTransform().InverseTransformPosition(Camera->GetComponentLocation()) - Box.GetCenter();
				View->SetStringField(TEXT("camera_m"), FString::Printf(TEXT("%.2f %.2f %.2f pitch %.1f"), Off.X / 100.0, Off.Y / 100.0, Off.Z / 100.0,
					Camera->GetComponentRotation().Pitch));
			}
			Json->SetObjectField(TEXT("helm_view"), View);
		}
		Json->SetNumberField(TEXT("helm_surge_mps"), State ? State->HelmSurgeMps : 0.f);
		Json->SetNumberField(TEXT("helm_heave_mps"), State ? State->HelmHeaveMps : 0.f);
		Json->SetNumberField(TEXT("hull_integrity"), State ? State->GetHullIntegrity01() : 1.f);
		Json->SetBoolField(TEXT("has_flashlight"), State && State->HasFlashlight());
		Json->SetBoolField(TEXT("hull_upgraded"), State && State->IsHullUpgraded());
		Json->SetNumberField(TEXT("parked_stop"), State ? State->ParkedStop() : 0);
		Json->SetBoolField(TEXT("near_console"), Run && Run->IsNearConsole());
		Json->SetBoolField(TEXT("near_helm"), Run && Run->IsNearHelm());
		Json->SetNumberField(TEXT("run_s"), State ? State->RunSeconds : 0.f);
		if (State)
		{
			const FDeepRunReview Review = State->BuildReview();
			TSharedRef<FJsonObject> Totals = MakeShared<FJsonObject>();
			Totals->SetNumberField(TEXT("divers_expended"), Review.DiversExpended);
			Totals->SetNumberField(TEXT("gross"), Review.Gross);
			Totals->SetNumberField(TEXT("cut"), Review.DirectorCut);
			Totals->SetNumberField(TEXT("fees"), Review.Fees);
			Totals->SetNumberField(TEXT("kept"), Review.Kept);
			Totals->SetStringField(TEXT("rating"), Review.Rating);
			Json->SetObjectField(TEXT("run_totals"), Totals);
		}
		const ADeepSalvage* Relic = DeepArena::FindRelic(World);
		Json->SetBoolField(TEXT("relic_in_world"), Relic != nullptr);
		Json->SetNumberField(TEXT("relic_distance_m"), Relic && Diver ? FVector::Dist(Relic->GetActorLocation(), Diver->GetActorLocation()) / 100.0 : -1.0);
		Json->SetBoolField(TEXT("carrying_relic"), Run && Run->CarriesRelic());

		// The serpent, for scripted players that aim the lamp at it: state, head position, the bite alarm left.
		if (const UDeepMonsterBrain* Brain = World ? World->GetSubsystem<UDeepMonsterBrain>() : nullptr)
		{
			if (const ADeepSerpent* Serpent = Brain->GetSerpent())
			{
				const TSharedRef<FJsonObject> SerpentJson = MakeShared<FJsonObject>();
				SerpentJson->SetStringField(TEXT("state"), UDeepMonsterRules::StateName(Brain->GetState()));
				const FVector Head = Serpent->GetHeadLocation();
				SerpentJson->SetArrayField(TEXT("head_cm"), {
					MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Head.X)),
					MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Head.Y)),
					MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Head.Z)) });
				SerpentJson->SetNumberField(TEXT("diver_distance_m"), Brain->GetDiverDistanceM());
				SerpentJson->SetNumberField(TEXT("tether_distance_m"), Brain->GetTetherDistanceM());
				SerpentJson->SetNumberField(TEXT("noise"), Brain->GetNoise());
				SerpentJson->SetNumberField(TEXT("bite_alarm_s"), Run ? Run->GetBiteAlarmLeft() : 0.f);
				Json->SetObjectField(TEXT("serpent"), SerpentJson);
			}
		}

		int32 SalvageCount = 0;
		if (World)
		{
			for (TActorIterator<ADeepSalvage> It(World); It; ++It)
			{
				SalvageCount += IsValid(*It) ? 1 : 0;
			}
		}
		Json->SetNumberField(TEXT("salvage_in_world"), SalvageCount);
		if (const UDeepAudioSubsystem* Audio = UDeepAudioSubsystem::Get(World))
		{
			Json->SetObjectField(TEXT("audio"), Audio->BuildStateJson());
		}
		if (const UDeepCameraSubsystem* Camera = UDeepCameraSubsystem::Get(World))
		{
			Json->SetObjectField(TEXT("camera"), Camera->BuildStateJson());
		}

		const FVector Pos = Diver ? Diver->GetActorLocation() : FVector::ZeroVector;
		TArray<TSharedPtr<FJsonValue>> PosArray = {
			MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Pos.X)),
			MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Pos.Y)),
			MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Pos.Z)),
		};
		Json->SetArrayField(TEXT("pos_cm"), PosArray);

		FString Out;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Json, Writer);
		return Out;
	}

	FString LogState(UWorld* World)
	{
		const FString Line = FString::Printf(TEXT("DEEPSTATE %s"), *BuildJson(World));
		UE_LOG(LogDeepState, Display, TEXT("%s"), *Line);
		return Line;
	}
}
