---
updated: 2026-11-14 20:50 EST
verified: Plugin layout, settings, flags and tool names read from the engine source installed at C:\Program Files\Epic Games\UE_5.9 on this machine. Behaviour read from Epic's docs. Port 8000 was confirmed listening while Quinn's editor was open. No tool has been called yet.
---
# Unreal MCP in UE 5.9

UE 5.9 ships Epic's own MCP server inside the editor. Claude Code is its default client. No community plugin is needed.

## What it is

- Plugin **Unreal MCP** (`ModelContextProtocol`), experimental, enabled by default. It depends on **Toolset Registry**.
- The server runs inside the editor process and starts with the editor (`bAutoStartServer=True` in the engine's `BaseEditorPerProjectUserSettings.ini`).
- Endpoint: **`http://127.0.0.1:8000/mcp`**, Streamable HTTP. No stdio, no WebSocket. Server name `unreal-mcp`.
- No authentication. Loopback only. Any local process can call it.
- By default it exposes four meta-tools: `list_toolsets`, `describe_toolsets`, `describe_tools`, `call_tool`. The agent discovers tools on demand instead of loading hundreds up front.

## Setup for a new project

1. **Enable the toolsets. This is a known issue (UE-412135): new projects have the server on but every toolset off**, so an agent sees no level, actor, asset or Blueprint tools and nothing explains why. Fix once per project: Edit > Plugins > enable **All Toolsets**, restart. Or add this to the `.uproject` plugins list:
   `{ "Name": "AllToolsets", "Enabled": true, "TargetAllowList": [ "Editor" ] }`
2. Generate the client config: Editor Preferences > Agentic Workflows > Model Context Protocol > **Generate Client Config** (Claude Code is the default), or console `ModelContextProtocol.GenerateClientConfig ClaudeCode`. It writes `.mcp.json` in the project root:
   ```json
   { "mcpServers": { "unreal-mcp": { "type": "http", "url": "http://127.0.0.1:8000/mcp" } } }
   ```
3. Start Claude Code from the folder that holds `.mcp.json`, after the editor is up.

Console commands: `ModelContextProtocol.StartServer [port]`, `.StopServer`, `.RefreshTools`, `.GenerateClientConfig <ClaudeCode|Cursor|VSCode|Gemini|Codex|All>`.
Command-line flags: `-ModelContextProtocolStartServer`, `-ModelContextProtocolPort=N`.
Useful setting: **Dump Media To Disk** saves every image the server returns to `Saved/ModelContextProtocol/`.

## What an agent can do

Tool names below were extracted from the 5.9 toolset source on this machine.

| Need | Toolset and tools |
|---|---|
| Build levels | `LevelToolset`: LoadLevel, CreateLevel, FindActors, AddToSceneFromClass, AddToSceneFromAsset, RemoveFromScene, TraceWorld, SaveActor. `ActorToolset`: transforms, components, tags. `PrimitiveToolset`: AddCube, AddSphere, AddCylinder, AddCone. |
| Edit any object | `ObjectToolset`: ListProperties, GetProperties, SetProperties, SearchSubclasses. |
| Assets | `AssetToolset`: FindAssets, Duplicate, Move, Delete, SaveAsset, ValidateAssets, GetReferencers. Importers: `StaticMeshToolset.ImportFile`, `TextureToolset.ImportFile`, `SoundWaveToolset.ImportFile`. |
| Data-driven design | `DataTableToolset`, `CurveTableToolset`, `DataAssetToolset`, `StringTableToolset`, `UserDefinedStructToolset`, `UserDefinedEnumToolset`. |
| Blueprints | `BlueprintToolset` (create, compile, graphs, nodes, pins, variables) and `BlueprintDslToolset` (Read, Write, Edit, Diff a text form of a graph). |
| Materials | `MaterialToolset` (expressions, connections, Recompile) and `MaterialInstanceToolset`. |
| Effects and audio | `NiagaraToolsets` (create systems, emitters, modules), `MetaSoundToolset` (author MetaSound graphs). |
| World dressing | `PCGToolset`: CreateGraph, AddNode, ConnectNodePins, SpawnGraphInstance, ExecuteGraphInstance. |
| Compile C++ | `LiveCodingToolset`: EnableLiveCoding, IsLiveCodingReady, CompileLiveCoding. |
| Run tests | `AutomationTestToolset`: DiscoverTests, ListTests, RunTests, RunTestsByFilter, GetTestResults, GetTestStatus, StopTests. |
| **Play the game** | `EditorAppToolset`: StartPIE, StopPIE, IsPIERunning. `InputToolset`: ApplyInput (press actions or keys for a duration), IsInputApplied. `CheatsToolset`: ListCheats, CallCheat. |
| **See the game** | `SceneCaptureToolset`: CaptureGameViewport, CaptureEditorViewport, Capture, RecordViewport, StartViewportRecording, StopViewportRecording. |
| Read state | `ConsoleToolset`: RunConsoleCommand, GetCVarValue, SetCVarValue. `LogsToolset`: GetLogEntries, GetLogCategories. |
| Performance | `PerfToolset`: GetFrameTimings, GetStatCounters, TriggerGPUProfile. |
| AI navigation | `NavMeshToolset`: FindPath, ProjectPointToNavMesh, GetNavMeshBuildStatus. |
| Editor control | `EditorAppToolset`: selection, camera, view modes, OpenEditorForAsset, ExitEditor. |
| Fab | A Fab toolset lets agents search the catalog and install assets (from Epic's announcement; not yet seen locally). |

Usage notes from the tool headers:

- **Tests:** call `DiscoverTests` once (takes several seconds), then `RunTestsByFilter("StartsWith:Deep")`. Filter syntax: `StartsWith:X`, `^X`, `X$`, bare substring, `Group:Name`, joined with `+`.
- **Live Coding:** `EnableLiveCoding`, poll `IsLiveCodingReady`, then `CompileLiveCoding`. It returns compiler diagnostics.
- **Input:** `ApplyInput(Entries, DurationSeconds)`. Each entry sets exactly one of an Enhanced Input action or a key. Zero duration is a single tap.
- **Cheats:** `CallCheat(Name, Args)` runs a cheat on the local player as if typed in the console. Our own cheat manager functions will show up in `ListCheats`.

## Limits and known issues

- **One call at a time.** Tool calls run on the game thread. Clients must not send overlapping calls. So only one agent drives the editor at any moment. See [[Agent Orchestration]].
- **About 120 seconds per call** before Claude Code times out. Split long work (big test runs, asset scans, Live Coding builds) into shorter calls and poll.
- **Live Coding only patches function bodies.** New classes, properties or `UFUNCTION`s need an editor restart. `ExitEditor` then relaunch and reconnect.
- **Headless runs also start the server.** A second editor process (for example a headless test run) will try to bind port 8000 too. Give it another port with `-ModelContextProtocolPort=N`. Commandlets do not auto-start.
- Experimental: "APIs and data formats may change at any time."
- Editor crashes on record for 5.9 Preview: Python Blueprint root swaps (UE-411502), undo after scripted Deformer Graph additions (UE-410941).
- In cooked builds only tools marked runtime-callable exist. We keep MCP out of the shipped game by limiting the plugin to the Editor target.

## Unreal Assistant panel

The editor has a chat panel (Window > Unreal Assistant) that can run Claude Code inside the editor over the Agent Client Protocol.

- Needs Node.js, the Claude Code CLI signed in with `claude login`, and the adapter: `npm install -g @agentclientprotocol/claude-agent-acp`. Restart the editor afterwards.
- Editor Preferences > General > Unreal Assistant > External Agent: choose Claude Code, enable Use External Agent.
- Known issue: in **Manual** permission mode it sticks on "Waiting for your approval" with no buttons. Use Auto or Accept edits.
- The panel and a terminal session reach the same tools. The docs do not say whether both may be connected at once; assume the one-call-at-a-time rule applies and do not run both.

This is the simplest way for Colby to talk to Claude with full editor access. See [[Teammate Access]].

## Sources

- Local engine source: `Engine/Plugins/Experimental/ModelContextProtocol`, `Engine/Plugins/Experimental/Toolsets/*`.
- https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor
- https://dev.epicgames.com/documentation/unreal-engine/run-external-ai-agents-in-the-unreal-assistant-panel-in-unreal-engine
- https://forums.unrealengine.com/t/unreal-engine-5-9-megajam-preview/2852840
