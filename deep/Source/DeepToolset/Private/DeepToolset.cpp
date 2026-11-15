#include "DeepToolset.h"

#include "Editor.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DeepToolset)

FString UDeepToolset::GetState()
{
	const UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	W->WriteObjectStart();
	W->WriteValue(TEXT("pieRunning"), World != nullptr);
	W->WriteValue(TEXT("world"), World ? World->GetName() : FString());
	W->WriteValue(TEXT("actorCount"), World ? World->GetActorCount() : 0);
	W->WriteValue(TEXT("timeSeconds"), World ? World->GetTimeSeconds() : 0.0);
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
