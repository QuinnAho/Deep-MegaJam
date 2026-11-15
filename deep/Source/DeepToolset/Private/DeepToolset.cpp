#include "DeepToolset.h"

#include "Debug/DeepStateReport.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"
#include "World/DeepSalvage.h"
#include "World/DeepSubmarine.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/StringOutputDevice.h"
#include "Modules/ModuleManager.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DeepToolset)

static UWorld* DeepPieWorld()
{
	return GEditor ? GEditor->PlayWorld.Get() : nullptr;
}

FString UDeepToolset::GetState()
{
	UWorld* World = DeepPieWorld();
	if (!World)
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("PIE is not running. Call EditorToolset.EditorAppToolset.StartPIE first."));
		return FString();
	}
	return DeepStateReport::BuildJson(World);
}

FString UDeepToolset::Exec(const FString& Command)
{
	UWorld* World = DeepPieWorld();
	if (!World)
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("PIE is not running. Call EditorToolset.EditorAppToolset.StartPIE first."));
		return FString();
	}
	if (Command.TrimStartAndEnd().IsEmpty())
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("Command is empty."));
		return FString();
	}
	FStringOutputDevice Out;
	if (!IConsoleManager::Get().ProcessUserConsoleInput(*Command, Out, World))
	{
		GEngine->Exec(World, *Command, Out);
	}
	return DeepStateReport::BuildJson(World);
}

FString UDeepToolset::GetArena()
{
	UWorld* World = DeepPieWorld();
	if (!World)
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("PIE is not running. Call EditorToolset.EditorAppToolset.StartPIE first."));
		return FString();
	}
	auto Vec = [](TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>>& W, const TCHAR* Key, const FVector& V)
	{
		W->WriteArrayStart(Key);
		W->WriteValue(V.X); W->WriteValue(V.Y); W->WriteValue(V.Z);
		W->WriteArrayEnd();
	};
	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	W->WriteObjectStart();
	ADeepSubmarine* Sub = nullptr;
	for (TActorIterator<ADeepSubmarine> It(World); It; ++It) { Sub = *It; break; }
	if (Sub)
	{
		Vec(W, TEXT("submarine"), Sub->GetActorLocation());
		Vec(W, TEXT("anchor_cm"), Sub->GetAnchorLocation());
	}
	else
	{
		W->WriteNull(TEXT("submarine"));
		W->WriteNull(TEXT("anchor_cm"));
	}
	W->WriteArrayStart(TEXT("salvage"));
	for (TActorIterator<ADeepSalvage> It(World); It; ++It)
	{
		if (!IsValid(*It)) { continue; }
		W->WriteObjectStart();
		Vec(W, TEXT("pos_cm"), It->GetActorLocation());
		W->WriteValue(TEXT("name"), It->GetName());
		W->WriteObjectEnd();
	}
	W->WriteArrayEnd();
	W->WriteObjectEnd();
	W->Close();
	return Out;
}

class FDeepToolsetModule : public IModuleInterface
{
	virtual void StartupModule() override { UToolsetRegistry::RegisterToolsetClass(UDeepToolset::StaticClass()); }
	virtual void ShutdownModule() override
	{
		if (UObjectInitialized()) { UToolsetRegistry::UnregisterToolsetClass(UDeepToolset::StaticClass()); }
	}
};

IMPLEMENT_MODULE(FDeepToolsetModule, DeepToolset);
