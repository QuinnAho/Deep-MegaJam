#include "UI/DeepUserSettings.h"

#include "AudioDevice.h"
#include "Engine/World.h"
#include "Game/DeepPlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Story/DeepCaptainLines.h"
#include "World/DeepEnvironment.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepUserSettings, Log, All);

namespace DeepSettingsRules
{
	float StepVolume(float Volume01, int32 Direction)
	{
		const float Steps = FMath::RoundToFloat(FMath::Clamp(Volume01, 0.f, 1.f) / VolumeStep) + FMath::Sign(Direction);
		return FMath::Clamp(Steps * VolumeStep, 0.f, 1.f);
	}

	float StepSensitivity(float Scale, int32 Direction)
	{
		const float Steps = FMath::RoundToFloat(FMath::Clamp(Scale, SensitivityMin, SensitivityMax) / SensitivityStep) + FMath::Sign(Direction);
		return FMath::Clamp(Steps * SensitivityStep, SensitivityMin, SensitivityMax);
	}

	float SensitivityBar(float Scale)
	{
		return FMath::Clamp((Scale - SensitivityMin) / (SensitivityMax - SensitivityMin), 0.f, 1.f);
	}

	FString VolumeLabel(float Volume01)
	{
		return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(FMath::Clamp(Volume01, 0.f, 1.f) * 100.f));
	}

	FString SensitivityLabel(float Scale)
	{
		return FString::Printf(TEXT("%.1f"), Scale);
	}

	FString RowLabel(EDeepSettingRow Row)
	{
		switch (Row)
		{
		case EDeepSettingRow::Volume: return TEXT("MASTER VOLUME");
		case EDeepSettingRow::Sensitivity: return TEXT("MOUSE SENSITIVITY");
		case EDeepSettingRow::InvertLook: return TEXT("INVERT VERTICAL LOOK");
		case EDeepSettingRow::RayTracing: return TEXT("RAY TRACING");
		case EDeepSettingRow::Back: return TEXT("BACK");
		default: return FString();
		}
	}

	FVector2D ScaleLook(const FVector2D& Look, float BaseSensitivity, float Scale, bool bInvert)
	{
		const FVector2D Out = Look * BaseSensitivity * FMath::Clamp(Scale, SensitivityMin, SensitivityMax);
		return FVector2D(Out.X, bInvert ? -Out.Y : Out.Y);
	}
}

namespace DeepCredits
{
	TArray<FDeepCreditRow> Parse(const FString& CsvText)
	{
		TArray<FDeepCreditRow> Out;
		for (const TMap<FString, FString>& Map : DeepData::ParseCsv(CsvText))
		{
			FDeepCreditRow Row;
			Row.Item = Map.FindRef(TEXT("item")).TrimStartAndEnd();
			Row.Author = Map.FindRef(TEXT("author")).TrimStartAndEnd();
			Row.Licence = Map.FindRef(TEXT("licence")).TrimStartAndEnd();
			Row.Note = Map.FindRef(TEXT("note")).TrimStartAndEnd();
			if (!Row.Item.IsEmpty())
			{
				Out.Add(MoveTemp(Row));
			}
		}
		return Out;
	}

	const TArray<FDeepCreditRow>& Get()
	{
		static TArray<FDeepCreditRow> Rows;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			FString Text;
			const FString Path = DeepData::DataPath(TEXT("Credits.csv"));
			if (FFileHelper::LoadFileToString(Text, *Path))
			{
				Rows = Parse(Text);
			}
			else
			{
				UE_LOG(LogDeepUserSettings, Warning, TEXT("Could not read %s"), *Path);
			}
		}
		return Rows;
	}
}

void UDeepUserSettings::Save()
{
	// The same file and call as UGameUserSettings::SaveSettings: the user's saved GameUserSettings.ini.
	SaveConfig(CPF_Config, *GGameUserSettingsIni);
	UE_LOG(LogDeepUserSettings, Display, TEXT("DEEPSETTINGS volume=%.2f sensitivity=%.2f invert=%d raytracing=%d chosen=%d file=%s"),
		MasterVolume, LookSensitivity, bInvertLook ? 1 : 0, IsRayTracingOn() ? 1 : 0, bRayTracingChosen ? 1 : 0, *GGameUserSettingsIni);
}

void UDeepUserSettings::ApplyVolume(const UObject* WorldContext) const
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (FAudioDevice* Device = World ? World->GetAudioDeviceRaw() : nullptr)
	{
		Device->SetTransientPrimaryVolume(FMath::Clamp(MasterVolume, 0.f, 1.f));
	}
}

void UDeepUserSettings::ApplyAll(const UObject* WorldContext) const
{
	ApplyVolume(WorldContext);
	if (bRayTracingChosen)
	{
		ADeepEnvironment::SetRayTracing(bRayTracing);
	}
}

void UDeepUserSettings::SetVolume(const UObject* WorldContext, float Volume01)
{
	MasterVolume = FMath::Clamp(Volume01, 0.f, 1.f);
	ApplyVolume(WorldContext);
	Save();
}

void UDeepUserSettings::SetSensitivity(float Scale)
{
	LookSensitivity = FMath::Clamp(Scale, DeepSettingsRules::SensitivityMin, DeepSettingsRules::SensitivityMax);
	Save();
}

void UDeepUserSettings::SetInvertLook(bool bInvert)
{
	bInvertLook = bInvert;
	Save();
}

void UDeepUserSettings::SetRayTracing(bool bOn)
{
	bRayTracingChosen = true;
	bRayTracing = bOn;
	// The same code as the console switch Deep.RayTracing.
	ADeepEnvironment::SetRayTracing(bOn);
	Save();
}

bool UDeepUserSettings::IsRayTracingOn() const
{
	if (bRayTracingChosen)
	{
		return bRayTracing;
	}
	const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.HardwareRayTracing"));
	return Var && Var->GetInt() != 0;
}

namespace
{
	FAutoConsoleCommandWithWorldAndArgs DeepUserSettingsScreenCommand(
		TEXT("Deep.Screen"),
		TEXT("Deep.Screen <none|settings|credits|quit>: opens a menu screen for the first local player (over the title, the pause menu or the review)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ADeepPlayerController* Player = World ? Cast<ADeepPlayerController>(World->GetFirstPlayerController()) : nullptr;
			if (!Player)
			{
				UE_LOG(LogDeepUserSettings, Warning, TEXT("Deep.Screen: no local DEEP player controller"));
				return;
			}
			const FString Name = Args.Num() > 0 ? Args[0].ToLower() : FString(TEXT("none"));
			using EScreen = ADeepPlayerController::EMenuScreen;
			const EScreen Screen = Name == TEXT("settings") ? EScreen::Settings
				: Name == TEXT("credits") ? EScreen::Credits
				: Name == TEXT("quit") ? EScreen::ConfirmQuit : EScreen::None;
			Player->OpenMenuScreen(Screen);
			UE_LOG(LogDeepUserSettings, Display, TEXT("DEEPSCREEN %s"), *Name);
		}));

	FAutoConsoleCommandWithWorldAndArgs DeepUserSettingsLogCommand(
		TEXT("Deep.Settings"),
		TEXT("Deep.Settings: logs the player's settings and the file they are saved to."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const UDeepUserSettings& S = UDeepUserSettings::Get();
			UE_LOG(LogDeepUserSettings, Display, TEXT("DEEPSETTINGS volume=%.2f sensitivity=%.2f invert=%d raytracing=%d chosen=%d file=%s"),
				S.MasterVolume, S.LookSensitivity, S.bInvertLook ? 1 : 0, S.IsRayTracingOn() ? 1 : 0, S.bRayTracingChosen ? 1 : 0, *GGameUserSettingsIni);
		}));
}
