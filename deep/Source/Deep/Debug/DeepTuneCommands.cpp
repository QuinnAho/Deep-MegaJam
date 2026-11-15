// Live tuning console commands, so the look (and any other design number) can be changed in a running game
// with no rebuild. Humans type them in the console; agents send them through DeepToolset.Exec.
//
//   Deep.Tune <Path> <Value>   set a value on UDeepTuningSettings, e.g.
//                              Deep.Tune Stops[0].FogDensity 0.8
//                              Deep.Tune Stops[1].FogColor (R=0.01,G=0.03,B=0.05,A=1)
//                              Deep.Tune Arena.ExposureMinEV -1
//                              Deep.Tune Env.WaterAlbedo.G 0.6     (Env. = the live ADeepEnvironment actor)
//                            then re-applies the look (same as Deep.Env.Apply).
//   Deep.TuneDump [Path]       print current values (no path: every top-level tuning property).
//   Deep.Env.Apply             re-apply light, fog, exposure, sub lights, glints and the lamp for the current stop.
//
// Paths are dot-separated property names; arrays take [i] (or .i). Values use Unreal's text format
// (the same as in DefaultGame.ini). Changes live in memory only: bake good values into the code defaults.

#include "Components/SpotLightComponent.h"
#include "CoreMinimal.h"
#include "Debug/DeepStateReport.h"
#include "Diver/DeepDiver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Sim/DeepRules.h"
#include "Sim/DeepTuningSettings.h"
#include "UObject/UnrealType.h"
#include "World/DeepEnvironment.h"
#include "World/DeepSalvage.h"
#include "World/DeepSubmarine.h"

DEFINE_LOG_CATEGORY_STATIC(LogDeepTune, Log, All);

namespace
{
	/** Where a property path ends: the property, the memory holding its value, and the object that owns it. */
	struct FTuneTarget
	{
		FProperty* Property = nullptr;
		void* Value = nullptr;
		UObject* Owner = nullptr;
	};

	ADeepEnvironment* FindEnvironment(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		for (TActorIterator<ADeepEnvironment> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** "Stops[0].FogColor.R" -> {Stops, 0, FogColor, R}. Brackets and dots both split. */
	TArray<FString> SplitPath(const FString& Path)
	{
		FString Flat = Path.Replace(TEXT("["), TEXT(".")).Replace(TEXT("]"), TEXT(""));
		TArray<FString> Parts;
		Flat.ParseIntoArray(Parts, TEXT("."), true);
		return Parts;
	}

	/** Walks Path from (Struct, Container). Returns false and fills Error when a step does not resolve. */
	bool Resolve(UStruct* Struct, void* Container, UObject* Owner, const TArray<FString>& Parts, FTuneTarget& Out, FString& Error)
	{
		FProperty* Property = nullptr;
		void* Value = nullptr;
		for (int32 i = 0; i < Parts.Num(); ++i)
		{
			const FString& Part = Parts[i];
			if (Part.IsNumeric() && Property)
			{
				const int32 Index = FCString::Atoi(*Part);
				if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
				{
					FScriptArrayHelper Helper(ArrayProperty, Value);
					if (!Helper.IsValidIndex(Index))
					{
						Error = FString::Printf(TEXT("index %d out of range (%s has %d)"), Index, *Property->GetName(), Helper.Num());
						return false;
					}
					Value = Helper.GetRawPtr(Index);
					Property = ArrayProperty->Inner;
					continue;
				}
				Error = FString::Printf(TEXT("%s is not an array"), *Property->GetName());
				return false;
			}
			if (Property)
			{
				FStructProperty* StructProperty = CastField<FStructProperty>(Property);
				if (!StructProperty)
				{
					Error = FString::Printf(TEXT("%s has no member %s"), *Property->GetName(), *Part);
					return false;
				}
				Struct = StructProperty->Struct;
				Container = Value;
			}
			Property = Struct->FindPropertyByName(FName(*Part));
			if (!Property)
			{
				Error = FString::Printf(TEXT("no property %s on %s"), *Part, *Struct->GetName());
				return false;
			}
			Value = Property->ContainerPtrToValuePtr<void>(Container);
		}
		if (!Property)
		{
			Error = TEXT("empty path");
			return false;
		}
		Out.Property = Property;
		Out.Value = Value;
		Out.Owner = Owner;
		return true;
	}

	/** "Env.X" targets the live environment actor; anything else targets the tuning settings. */
	bool ResolvePath(UWorld* World, const FString& Path, FTuneTarget& Out, FString& Error)
	{
		TArray<FString> Parts = SplitPath(Path);
		if (Parts.Num() > 0 && Parts[0].Equals(TEXT("Env"), ESearchCase::IgnoreCase))
		{
			ADeepEnvironment* Environment = FindEnvironment(World);
			if (!Environment)
			{
				Error = TEXT("no ADeepEnvironment in this world");
				return false;
			}
			Parts.RemoveAt(0);
			return Resolve(Environment->GetClass(), Environment, Environment, Parts, Out, Error);
		}
		UDeepTuningSettings* Settings = GetMutableDefault<UDeepTuningSettings>();
		return Resolve(Settings->GetClass(), Settings, Settings, Parts, Out, Error);
	}

	FString ExportValue(const FTuneTarget& Target)
	{
		FString Text;
		Target.Property->ExportTextItem_Direct(Text, Target.Value, nullptr, Target.Owner, PPF_None);
		return Text;
	}

	/** Pushes the current tuning into everything that read it at BeginPlay. Debug only. */
	void ApplyLook(UWorld* World)
	{
		if (!World)
		{
			return;
		}
		int32 Glints = 0;
		if (ADeepEnvironment* Environment = FindEnvironment(World))
		{
			Environment->ApplyCurrentStop();
		}
		for (TActorIterator<ADeepSubmarine> It(World); It; ++It)
		{
			It->ApplyTuning();
		}
		for (TActorIterator<ADeepSalvage> It(World); It; ++It)
		{
			It->ApplyKind();
			++Glints;
		}
		// The diver sets its lamp once at BeginPlay; mirror those calls here so lamp tuning shows live.
		const FDeepLampTuning& Lamp = UDeepTuningSettings::Get().Lamp;
		for (TActorIterator<ADeepDiver> It(World); It; ++It)
		{
			if (USpotLightComponent* Spot = It->FindComponentByClass<USpotLightComponent>())
			{
				Spot->SetIntensity(Lamp.Intensity);
				Spot->SetInnerConeAngle(Lamp.InnerConeDeg);
				Spot->SetOuterConeAngle(Lamp.OuterConeDeg);
				Spot->SetAttenuationRadius(Lamp.RangeM * UDeepRules::CmPerM);
			}
		}
		UE_LOG(LogDeepTune, Log, TEXT("Deep.Env.Apply: environment, sub, %d glints and lamp re-applied"), Glints);
	}

	FAutoConsoleCommandWithWorldAndArgs TuneCommand(
		TEXT("Deep.Tune"),
		TEXT("Deep.Tune <Path> <Value>: sets a tuning value live, e.g. Stops[0].FogDensity 0.8 or Env.SunScattering 2, then re-applies the look."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 2)
			{
				UE_LOG(LogDeepTune, Warning, TEXT("Usage: Deep.Tune <Path> <Value>"));
				return;
			}
			// A struct value such as (R=1,G=0,B=0,A=1) never has spaces, but join the rest in case it does.
			const FString ValueText = FString::Join(TArrayView<const FString>(Args).RightChop(1), TEXT(" "));
			FTuneTarget Target;
			FString Error;
			if (!ResolvePath(World, Args[0], Target, Error))
			{
				UE_LOG(LogDeepTune, Warning, TEXT("Deep.Tune %s: %s"), *Args[0], *Error);
				return;
			}
			const FString Before = ExportValue(Target);
			if (!Target.Property->ImportText_Direct(*ValueText, Target.Value, Target.Owner, PPF_None))
			{
				UE_LOG(LogDeepTune, Warning, TEXT("Deep.Tune %s: could not read '%s' as %s"), *Args[0], *ValueText, *Target.Property->GetCPPType());
				return;
			}
			UE_LOG(LogDeepTune, Display, TEXT("DEEPTUNE %s = %s (was %s)"), *Args[0], *ExportValue(Target), *Before);
			ApplyLook(World);
		}));

	FAutoConsoleCommandWithWorldAndArgs TuneDumpCommand(
		TEXT("Deep.TuneDump"),
		TEXT("Deep.TuneDump [Path]: prints current tuning values (Env.<Property> for the live environment)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() == 0)
			{
				UDeepTuningSettings* Settings = GetMutableDefault<UDeepTuningSettings>();
				for (TFieldIterator<FProperty> It(Settings->GetClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
				{
					FTuneTarget Target{ *It, It->ContainerPtrToValuePtr<void>(Settings), Settings };
					UE_LOG(LogDeepTune, Display, TEXT("DEEPTUNE %s = %s"), *It->GetName(), *ExportValue(Target));
				}
				return;
			}
			for (const FString& Path : Args)
			{
				FTuneTarget Target;
				FString Error;
				if (ResolvePath(World, Path, Target, Error))
				{
					UE_LOG(LogDeepTune, Display, TEXT("DEEPTUNE %s = %s"), *Path, *ExportValue(Target));
				}
				else
				{
					UE_LOG(LogDeepTune, Warning, TEXT("Deep.TuneDump %s: %s"), *Path, *Error);
				}
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs EnvApplyCommand(
		TEXT("Deep.Env.Apply"),
		TEXT("Deep.Env.Apply: re-applies light, fog, exposure, sub lights, salvage glints and the lamp for the current stop."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ApplyLook(World);
		}));
}
