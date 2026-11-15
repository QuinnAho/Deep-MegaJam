---
updated: 2026-11-14 22:06 EST
verified: live on UE 5.9.0-60351404 editor, port 8012, via tools/mcp.ps1 (ping, list_toolsets, describe_toolsets on all 106 toolsets, StartPIE, IsPIERunning, RunConsoleCommand, GetLogEntries, CaptureGameViewport, StopPIE, ExitEditor, and our own DeepToolset.GetState after a rebuild and restart)
---
# Unreal MCP live catalog (UE 5.9)

Source of truth is the running editor, not the engine source. Server `unreal-mcp`, protocol 2025-11-25, session header issued. Start the editor with `tools\ue.ps1 editor -Port N`, talk to it with `tools\mcp.ps1`.

The editor boots to a listening MCP port in about 50 s warm (about 10 s on a restart); a map-less project opens an `Untitled_1` level.

## How to call (verified)

- `mcp.ps1 -Port 8012 ping` | `toolsets` | `describe <Toolset>` | `describe <Toolset> -Tool <Name>` | `call <Toolset>.<Tool> '<json>'`.
- Toolset names are `Module.Class`, for example `EditorToolset.EditorAppToolset`. Full tool name is `Module.Class.Tool`. Underneath, `call_tool` takes `{toolset_name, tool_name, arguments}`; `describe_toolsets` takes `{toolset_names: []}`; `describe_tools` takes `{tool_names: ["Module.Class.Tool"]}`.
- Results arrive as `{"returnValue": ...}`. Script errors come back as `isError` with the reason in plain text (exit code 1 from mcp.ps1).
- Object arguments are `{"refPath": "/Game/..."}`. A required object argument that may be unset must be passed as `null`, not omitted.

## Round trip on the empty game (all passed)

| Step | Call | Result |
|---|---|---|
| start | `EditorToolset.EditorAppToolset.StartPIE '{}'` | `{"returnValue":null}` |
| check | `EditorToolset.EditorAppToolset.IsPIERunning '{}'` | `{"returnValue":true}` |
| console | `ConsoleToolset.ConsoleToolset.RunConsoleCommand '{"command":"stat fps"}'` | `{"returnValue":["Cmd"]}`; output goes to the log |
| log | `ConsoleToolset.LogsToolset.GetLogEntries '{"pattern":"PIE\|Cmd","maxEntries":4,"maxCharsPerEntry":160}'` | list of log lines; also `logCategory` filter |
| capture | `SceneCaptureToolset.SceneCaptureToolset.CaptureGameViewport '{"world":null,"maxResolution":960}'` | PNG 960x320 saved to `deep\Saved\Harness\mcp\CaptureGameViewport-<ts>.png`; a real frame (sky, desert, tile floor) |
| stop | `EditorToolset.EditorAppToolset.StopPIE '{}'` | `IsPIERunning` then returns false |

Notes: `world` and `maxResolution` are both required for the capture; `world: null` picks the PIE world (or call `ListWorlds` and pass `{"refPath": ...}`). The capture viewport was small (960x320) because the editor's PIE window was; set the window size before relying on resolution. `RunConsoleCommand` refuses any command containing the word `py`.

## Toolsets (106 reported, about 840 tools; tool counts in brackets)

**Editor and world.** EditorToolset: EditorAppToolset [21] PIE start/stop, selection, camera, show flags, editor image capture; LevelToolset [24] actors in level, traces, level instances, data layers; ActorToolset [15] tags, components, bounds; AssetToolset [21]; ObjectToolset [4]; PrimitiveToolset [4]; UndoHistoryToolset [3]. Asset kinds: MaterialToolset [16], MaterialInstanceToolset [5], StaticMeshToolset [15], SkeletalMeshToolset [12], TextureToolset [4], SoundWaveToolset [2], DataTableToolset [9], CurveTableToolset [9], CurveAtlasToolset [7], DataAssetToolset [2], StringTableToolset [7], UserDefinedEnum [4], UserDefinedStruct [7].

**Run, test, observe.** ConsoleToolset [4] RunConsoleCommand, Get/SetCVarValue, SearchCVars. LogsToolset [3] GetLogEntries, GetLogCategories, GetVerbosity. AutomationTestToolset [3] DiscoverTests, RunTestsByFilter, GetTestResults. CheatsToolset [2] ListCheats, CallCheat (needs a live PIE player and cheat manager). InputToolset [2] ApplyInput, IsInputApplied. SceneCaptureToolset [10] Capture, CaptureGameViewport, CaptureEditorViewport, RecordViewport, StopViewportRecording, ListWorlds, ListViews, ListModes, DescribeMode. LiveCodingToolset [2] IsLiveCodingReady, CompileLiveCoding. RewindDebuggerToolset [3].

**Perf.** PerfToolset: StatsToolset [3] ListStatGroups, GetStatCounters, GetFrameTimings; CsvProfilerToolset [2]; GpuProfilerToolset [1] TriggerGPUProfile.

**UI.** SlateInspectorToolset [17] drives the live UI: Click, Hover, Type, Drag, Scroll, Screenshot, Snapshot, WaitFor. UMGEditor.WidgetBlueprintEditorSubsystem [7], UMGDebuggerSubsystem [3], WidgetFinderSubsystem [6], WidgetAnimationToolset [5], MVVMToolset [19], CommonUIToolset [6], WidgetStaticAnalysisToolset [0 reported], SlateDebuggerSubsystem [1].

**Audio.** MetaSoundToolset: AuthoringToolset [8] (AddNodes, Connect, SetDefaults), InspectionToolset [4], DiscoveryToolset [0 reported]. EditorToolset.SoundWaveToolset [2]. MediaToolset [3] SkimVideo, SampleFrames, GetVideoInfo.

**Gameplay systems.** BlueprintToolsets.BlueprintToolset [35], BlueprintDslToolset [1], editor_toolset BlueprintDslTools [3] and BlueprintDiffTools [1]. GASToolsets: GameplayCue [4], AttributeSet [1], AbilitySystemInspector [3], GameplayAbility [0 reported]. GameplayTagsToolset [4], DataRegistryToolset [4], GameFeaturesToolset [4], ConfigSettingsToolset [6], StateTree [20], BehaviorTree [12], EnvironmentQuery [9], NavMeshToolset [12], WorldConditionTools [0 reported].

**VFX, world building, animation.** NiagaraToolsets (Info, Component, Blueprint, System [23], Assets), PCGToolset [23] and PCGSpatialToolset [1], PCGEditorSubsystem [26], PhysicsAssetToolset [8], TextureGraphTools [10], ChaosClothAssetToolset [3], Optimus deformer graph [22], Dataflow [13] and DataflowAgent [11], ControlRig [65 + 18], Sequencer family (SequencerScripting extensions, LevelSequenceEditor [45 + 6], SequencerAnimTools [15], SequenceValidator, TemplateSequence, SequencerDirector [3]).

**Project and meta.** FabToolset [5], PluginToolset [14], SourceControlHelpers [21], FileSystemToolset [2] ReadTextFile, DeleteFile, ToolsetRegistry.AgentSkillToolset [2], AgentPluginToolset [2], AIAssistantEditor.AIAssistantToolset [0 reported], python_scripting PythonScriptingTools [2] (get_symbol, read_me), conversation ConversationTools [2].

Counts of 0 mean the describe call listed no tools (disabled or session-only); not investigated.

## Announced by Epic, present or missing

- Fab toolset: PRESENT. `FabToolset.FabToolset` [5]: SearchFabListings, SearchFabMyLibrary, GetFabListingDetails, InstallFabListing, FabSignIn. Listing search needs no sign-in; library search and install do. Not exercised live (it would install into Quinn's profile and open a browser).
- Nothing Epic announced was found missing in this pass. The Python toolset exposes only get_symbol and read_me here, not a generic exec tool; running Python is not a visible path.

## Worth using for DEEP

1. **Gameplay testing loop.** StartPIE, `RunConsoleCommand` (any cvar or our own exec commands), ApplyInput (press keys or Enhanced Input actions for N seconds), GetLogEntries to assert, CaptureGameViewport to look, StopPIE. A custom `DeepToolset.GetState` (verified, see below) closes the loop with JSON state.
2. **Capture and recording.** CaptureGameViewport for stills; RecordViewport / StopViewportRecording for clips (not exercised) , good for the devlog Glass Box entry. MediaToolset SkimVideo/SampleFrames can check a clip.
3. **Cheats.** CallCheat runs UCheatManager exec functions in PIE: God, Teleport, our own debug cheats, with no new tool code. Needs a player controller with a cheat manager.
4. **Perf.** GetFrameTimings and GetStatCounters per frame, TriggerGPUProfile, CSV profiler: cheap frame-time budget checks before the build is due.
5. **UI.** SlateInspectorToolset Click/Type/Snapshot to test menus and the HUD; CommonUI and UMG toolsets for widget trees.
6. **Audio.** MetaSoundAuthoringToolset builds MetaSound graphs from text, useful for sonar pings and ambience without clicking nodes.
7. **Assets without binaries in git.** DataTable, CurveTable and StringTable toolsets edit data assets; Fab search and install for free props and audio (Quinn decides what to install).
8. **Test runner.** AutomationTestToolset DiscoverTests / RunTestsByFilter inside the live editor, faster than a cold headless run.
9. **LiveCodingToolset.** CompileLiveCoding avoids closing the editor to rebuild C++.

## Writing our own toolset (verified, module `DeepToolset`)

Mirrors the engine's `CheatsToolset` and `ConsoleToolset`.

- **Module.** Editor-only C++ module (`"Type": "Editor"`, `"LoadingPhase": "PostEngineInit"` in `Deep.uproject`; add it to `ExtraModuleNames` in `DeepEditor.Target.cs` only, not the game target). Build.cs dependencies: Core, CoreUObject, Engine, `ToolsetRegistry`, plus UnrealEd/Json as needed. The project already enables `AllToolsets`, which brings `ToolsetRegistry` along; UBT warns that the uproject does not list it, harmless (we list it).
- **Base class.** `UCLASS(BlueprintType) class UMyToolset : public UToolsetDefinition` from `ToolsetRegistry/ToolsetDefinition.h`. Tools are `static` UFUNCTIONs. Toolset name becomes `<Module>.<Class minus U>`, e.g. `DeepToolset.DeepToolset`.
- **Marking a tool.** `UFUNCTION(meta = (AICallable, AIAccessMode = "Read"), Category = "Deep")`. `AIAccessMode` is Read, Write or Execute (Execute is the default). `AIRuntimeCallable` instead of `AICallable` also allows cooked builds. `AIIgnore` hides a function without an error. Doxygen comments become the tool description, `@param` lines the argument descriptions, `@return` the output.
- **Results and errors.** Return a value (FString, USTRUCT, TArray); it is serialised as `returnValue`. Return a JSON string for flexible shapes. Raise failure with `UKismetSystemLibrary::RaiseScriptError(TEXT("reason"))`, which reaches the caller as `isError` text.
- **Registration.** In `StartupModule`: `UToolsetRegistry::RegisterToolsetClass(UMyToolset::StaticClass())`; unregister in `ShutdownModule` if `UObjectInitialized()`. No other wiring; it appears in `list_toolsets` on the next editor start.
- **Restart vs Live Coding.** A new module, new UCLASS or changed header needs an editor restart (and the editor must be closed to build with `tools\ue.ps1 build`). Changing only the body of an existing tool may work through Live Coding (UNVERIFIED; the registry reads UFUNCTION signatures at registration, so any new or changed signature needs a restart). Only the restart path was tested.

Verified skeleton (`deep\Source\DeepToolset\Private\DeepToolset.h` and `.cpp` hold the full version; call: `mcp.ps1 -Port 8012 call DeepToolset.DeepToolset.GetState '{}'` returned `{"returnValue":"{\"pieRunning\":true,\"world\":\"Untitled_1\",\"actorCount\":160,\"timeSeconds\":3.19}"}` during PIE):

```cpp
// DeepToolset.h
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "DeepToolset.generated.h"
UCLASS(BlueprintType)
class UDeepToolset : public UToolsetDefinition
{
	GENERATED_BODY()
public:
	/** Returns a JSON string describing the running PIE game. */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"), Category = "Deep")
	static FString GetState();
};
// DeepToolset.cpp
FString UDeepToolset::GetState()
{
	const UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
	FString Out;
	auto W = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	W->WriteObjectStart();
	W->WriteValue(TEXT("pieRunning"), World != nullptr);
	W->WriteValue(TEXT("actorCount"), World ? World->GetActorCount() : 0);
	W->WriteObjectEnd(); W->Close();
	return Out;
}
class FDeepToolsetModule : public IModuleInterface
{
	virtual void StartupModule() override { UToolsetRegistry::RegisterToolsetClass(UDeepToolset::StaticClass()); }
	virtual void ShutdownModule() override { if (UObjectInitialized()) UToolsetRegistry::UnregisterToolsetClass(UDeepToolset::StaticClass()); }
};
IMPLEMENT_MODULE(FDeepToolsetModule, DeepToolset);
```

When gameplay code exists, add tools to `UDeepToolset` that read the Deep module's state (Captain trust, oxygen, depth) and a `SetState` Write tool to teleport the diver or force an event. The DeepToolset module may depend on `Deep`.
