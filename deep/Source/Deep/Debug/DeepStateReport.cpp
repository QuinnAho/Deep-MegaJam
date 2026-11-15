#include "Debug/DeepStateReport.h"

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
#include "World/DeepSalvage.h"

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
		Json->SetNumberField(TEXT("tether_distance_m"), Solve.DistanceM);
		Json->SetNumberField(TEXT("tether_left_m"), Solve.LeftM);
		Json->SetNumberField(TEXT("tether_strain"), Solve.Strain);
		Json->SetBoolField(TEXT("tether_taut"), Solve.bTaut);
		Json->SetBoolField(TEXT("winching"), Diver && Diver->IsWinching());
		Json->SetBoolField(TEXT("home"), Tether && Tether->IsHome());
		Json->SetBoolField(TEXT("lamp"), Diver && Diver->IsLampOn());

		const ADeepGameState* State = World ? World->GetGameState<ADeepGameState>() : nullptr;
		Json->SetNumberField(TEXT("credits"), State ? State->Credits : 0);
		Json->SetNumberField(TEXT("hull_rating"), State ? State->HullRating : 1);
		Json->SetNumberField(TEXT("stop"), State ? State->Stop : 1);
		Json->SetNumberField(TEXT("diver_number"), State ? State->DiverNumber : 1);

		int32 SalvageCount = 0;
		if (World)
		{
			for (TActorIterator<ADeepSalvage> It(World); It; ++It)
			{
				SalvageCount += IsValid(*It) ? 1 : 0;
			}
		}
		Json->SetNumberField(TEXT("salvage_in_world"), SalvageCount);

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
