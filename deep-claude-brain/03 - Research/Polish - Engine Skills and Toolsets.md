---
updated: 2026-11-15 02:15 EST
verified: 2026-11-15 02:15 EST by reading files under C:\Program Files\Epic Games\UE_5.9\Engine\Plugins (skill .py files, SceneCaptureDiagnosticsSkill.cpp, toolset headers .h, agent_skill.py, AgentSkill.h, Niagara template folder listing). Nothing was called live and no editor was started. Items marked UNCONFIRMED could not be settled from the files.
---
# Polish: Epic's built-in skills and toolsets (UE 5.9)

Short version: Epic ships about 41 Python skills, 1 C++ skill for scene capture, and PCG, Dataflow and cloth skills we do not need. Root for Python skills: `Engine\Plugins\Experimental\Toolsets\<Toolset>\Content\Python\<pkg>\skills\<name>.py`. Each file is a docstring (the trigger) plus an `_INSTRUCTIONS` string. Below, `TS` means that Toolsets root.

## How an agent loads a skill
- **Cheapest, no editor needed:** Read the `.py` file path from the table. The text between the triple quotes is the skill.
- **Through MCP** (`ToolsetRegistry/Source/ToolsetRegistry/Public/ToolsetRegistry/AgentSkill.h`): `ToolsetRegistry.AgentSkillToolset.ListSkills` takes no arguments and returns a map of skill path to description. `...GetSkills` takes `{"SkillPaths": [..]}` and returns the instructions. Call with `tools\mcp.ps1 -Port 8012 call ToolsetRegistry.AgentSkillToolset.GetSkills '{"SkillPaths":["<path from ListSkills>"]}'` (call form from the Live Catalog; this exact call was not run). The path form for a Python skill is UNCONFIRMED: take it from `ListSkills`. `CreateSkill` and `UpdateSkill` exist; do not use them.
- Python skills register with `@agent_skill` in `TS\ToolsetRegistry\...\toolset_registry\agent_skill.py` (the docstring becomes the description, `_INSTRUCTIONS` the body).

## The skills
| Skill (file under TS\...) | What it teaches | Read when |
|---|---|---|
| Niagara `fundamentals` (NiagaraToolsets\Content\Python\skills) | Stack order, CPU vs GPU, coordinate space, params, materials, verify the system | any Niagara work (WP9) |
| Niagara `renderers` | Sprite, mesh, ribbon, light, decal choice; silent-fail material rules | bubbles, silt, trails |
| Niagara `optimization` | Fewer emitters, bounds, Effect Types, pooling | an FX is slow |
| Niagara `blueprint_interop` | Spawn mechanism, binding user params, pooling | firing FX from C++/BP |
| Niagara `level_placement`, `cleanup`, `custom_hlsl` | Reuse templates; orphan audit; single-rvalue HLSL | rarely |
| MetaSound `fundamentals`, `workflows` | Builder model; the "build from attached asset forks a duplicate" trap | any sound graph (WP sound) |
| MetaSound `recipes`, `trigger_topology` | Random one-shot layers over a loop; cycle breaking, start/finish wiring | ambience, stingers |
| MetaSound `presets`, `performance`, `paged_graphs` | Preset retune; ship rules; Mobile page | retune; skip the rest |
| MovieRenderPipeline `movie_render_queue`, `movie_render_graph` (MovieRenderPipelineToolset\...\movie_render_pipeline_toolset\skills) | Python-driven MRQ and MRG | only if a Level Sequence trailer is chosen |
| Sequencer family (AnimationAssistantToolset\...\animation_toolset\skills): `sequencer_core`, `sequencer_camera`, `sequencer_keyframing`, `sequencer_bindings`, `sequencer_gameplay`, `director` | Cameras, cuts, keys, DoF, shakes; verify by scrubbing and capture | trailer by sequence |
| Same folder: `sequencer_control_rig`, `sequencer_anim_tools`, `sequencer_import_export`; SequencerAnimMixerToolset `animmixer` | Rig and animation tools | not needed (no characters) |
| WidgetAnimationToolset `widget_animation` | UMG animations via tracks, only inside a Widget Blueprint | only if the HUD moves to Widget Blueprints |
| MVVMToolset `widget_binding` | ViewModel to widget bindings, conversion functions | same condition |
| EditorToolset `umg_basics`, `material_basics`, `placing_actors`, `blueprint_basics`, `diffing`, `source_control`, `unreal_skill_best_practices` | Short rules; `material_basics`: instance before new material | WP3 materials |
| EditorToolset `default_outdoor_lighting` | Sun, sky, cloud, fog rules | do NOT load (sky setup; see correction 4) |
| EditorToolset `analyze_widget_navigation`, `debug_runtime_ui_bug`, `create_editor_utility_widget`; SlateInspectorToolset `input_simulation`; WidgetStaticAnalysisToolset `generate_widget_description_document` | UI debugging and docs | `debug_runtime_ui_bug` if the HUD misbehaves in PIE |
| SceneCaptureToolset C++ skill (`Source\SceneCaptureToolset\Private\SceneCaptureDiagnosticsSkill.cpp`) | Capture vs CaptureGameViewport, modes (Unlit, LightingOnly, depth, ShaderComplexity, post-process stack), blank-capture fixes | WP1 look checks, WP lock |
| PCG: `Engine\Plugins\PCG\Skills\*\SKILL.md` (11) and PCGToolset\Content\Skills (8 .uasset); Dataflow, ChaosCloth (C++) | PCG, Dataflow, cloth | never (PCG terrain is on the Cut list); Dataflow and cloth not read |

## Per work package (WP numbers from the plan, section 9)
- **WP1 water look.** Skills: SceneCapture diagnostics. Tools: `SceneCaptureToolset` Capture with modes (LightingOnly, post-process stack) to debug why the fog milks the frame; `CaptureGameViewport` in PIE; `ConsoleToolset` for `Deep.Tune`. Marine snow: see correction 1. Only by hand or C++: the whole look (the plan keeps it in C++); no tool edits our code.
- **WP2/5/8 dressing, room skin.** No skill fits. `LevelToolset`, `ActorToolset`, `StaticMeshToolset` can place and inspect; `placing_actors` only says snap props to the ground. Hand or C++: the CSV loader.
- **WP3 materials.** Skill: `material_basics`. Tools: `MaterialToolset` (`EditorToolset\...\MaterialToolset.h`): `CreateMaterial`, `AddExpression`, `ConnectExpressions`, `ConnectToOutput`, `CreateFunction`; `MaterialInstanceToolset`. So a material can be built from nothing. TextureGraph (`TS\TextureGraphToolset\...\TextureGraphTools.h`): `CreateTextureGraph`, `AddNode`, `ConnectPins`, `SetOutputSettings`, `ExportTextureGraph` for a procedural caustic or noise texture; it has no skill, and its header says to call create "ONLY after getting explicit direction". UNCONFIRMED: Substrate-specific expressions through these tools.
- **WP4 driving.** No skill. Camera shake and sway are C++. Sequencer camera shake track exists but is not for gameplay.
- **WP6 HUD.** Plan is native C++ widgets; the UMG skills do not apply (correction 2). Tools that still help: `SlateInspectorToolset` (Click, Snapshot, Screenshot) and `debug_runtime_ui_bug` to test the HUD live; `CommonUIToolset` is for focus and routing, not look. Hand or C++: everything visual.
- **WP7 serpent.** No skill. `GeometryToolset` (`geometry_ops.py`) appends box, cylinder, sphere, cone, torus and disc to a DynamicMesh component, not a skinned tube; not useful. Material per WP3. C++ procedural mesh stays.
- **WP9 effects.** Skills: Niagara `fundamentals`, `renderers`, `optimization`. Tools: `NiagaraToolset_System`: `CreateNiagaraSystem(AssetName, AssetPath, TemplateSystem)` where the template is "(required)", `AddEmitter(System, TemplateEmitter, Name)`, `AddRenderer`, `SetEmitterData`, `AddUserVariables`; `NiagaraToolset_Component.SetSystem/SetVariable`. So: edit and compose from templates, not from nothing. Templates are on disk: `Plugins\FX\Niagara\Content\DefaultAssets\Templates\Emitters\` (BlowingParticles, Fountain, HangingParticulates, SimpleSpriteBurst, SingleLoopingParticle, DynamicBeam, LocationBasedRibbon and more) and `Systems\` (RadialBurst and more); also `TS\NiagaraToolsets\Content\Templates\` (NS_Fire, NS_Smoke_Plume, M_SimpleSmoke). Hand: the Niagara editor check for stack errors ("Opening the editor is necessary for the errors to surface", fundamentals).
- **WP sound.** Skills: MetaSound `fundamentals`, `workflows`, `recipes`, `trigger_topology`. Tools: `MetaSoundAuthoringToolset` (`CreateSourceBuilder`, `AddNodes`, `Connect`, `SetDefaults`, `BuildToAsset`, `CommitBuilderToAsset`, `LayoutGraph`), `MetaSoundDiscoveryToolset`, `MetaSoundInspectionToolset`. Can build a drone, sonar ping, random creaks. Cannot audition: "does not audition, render, or measure runtime cost, and it does not edit the wave assets". Hand: listening, mixing, any recorded sound. Also Fab search for free audio (Quinn's call).
- **WP lock and gameplay video.** Skills: SceneCapture diagnostics; MRQ and sequencer skills only for the sequence route. Tools: `SceneCaptureToolset.RecordViewport` / `StartViewportRecording` / `StopViewportRecording` (MP4 under Saved/Recordings); `MediaToolset` `SkimVideo`, `SampleFrames`, `GetVideoInfo` to check the clip with JPEG frames; `PerfToolset` `StatsToolset.GetFrameTimings`, `GetFrameTree`, `GpuProfilerToolset.TriggerGPUProfile`, `CsvProfilerToolset`. Hand: cutting and editing, music, any audio in the clip (UNCONFIRMED whether the MP4 has sound; the header lists none), the final taste check.

## Five things that contradict or improve the plan
1. **Marine snow: a ready Niagara emitter exists.** Plan section 1 builds 1,200 instanced spheres in code, with Niagara as the fallback. `Plugins\FX\Niagara\Content\DefaultAssets\Templates\Emitters\HangingParticulates.uasset` is a stock template, and `CreateNiagaraSystem` plus `AddEmitter` can make a system from it with no clicking (`NiagaraToolset_System.h`: "Template system to base the new system on (required)"). Catch, from `NiagaraToolsets\Content\Python\skills\fundamentals.py`: "attachment controls spawn origin, space setting controls post-spawn behavior" and "Default to World Space unless particles must rigidly follow the actor". Snow that must stream past a still sub as the root moves needs the root-relative setup chosen deliberately. Keep the code version if time is short; it is already specified and testable.
2. **The HUD plan is right and the widget-animation skill does not apply.** `widget_animation.py`: "A UMG Widget Animation is a `UWidgetAnimation` asset stored inside a `UWidgetBlueprint`." The plan builds native `UUserWidget`s with no Widget Blueprints, so the animation, MVVM and `umg_basics` tools have nothing to edit. Do the tweens in C++ as planned. If anyone switches to Widget Blueprints, the same file warns: the Visibility track "has no MCP key tool... without Python, drive fades with RenderOpacity instead", and a Widget Material track "animates nothing" from script.
3. **Trailer: use RecordViewport, not Movie Render Queue.** `movie_render_queue.py`: "Use EDA's execute_python_in_editor tool", and its jobs render a Level Sequence. Our Live Catalog says "The Python toolset exposes only get_symbol and read_me here, not a generic exec tool", and the game runs on `/Engine/Maps/Entry` with no sequence. `SceneCaptureRecordingTypes.h` records PIE or game to MP4 but defaults to `MaxResolution = 1280` and `FrameRate = 15.f` (max 60 s): pass 1920 and 30 or 0 (every frame). `SceneCaptureToolset.h`: "Capture timings are not representative game-frame timings", so record after, not during, perf measurements.
4. **Do not load the outdoor-lighting skill.** `default_outdoor_lighting.py` is a sky-and-sun recipe: "If the Directional Light intensity is zero, the Sky Atmosphere is black." DEEP's look is a code-driven black ocean (plan section 1); following it would add sky, cloud and sky-light actors. Use `material_basics` instead: "Search for an existing Material or MaterialInstance... If a suitable parent exists, create a MaterialInstance from it."
5. **Sound has an Epic recipe and a trap.** `recipes.py` gives the random-layer ambience pattern (Trigger Any, random float, float-to-time conversion, delayed variable read to break the cycle). `workflows.py`: "Building from an attached builder is the failure case: it succeeds, spawns a second asset, and leaves the original untouched." `performance.py`: print-log nodes "are not stripped from Test or Shipping". The plan has no sound section; give WP sound a MetaSound brief.

Also: `level_placement.py` says "do not author a new system unless the user explicitly asks", and `movie_render_queue.py` says to render "only when the user explicitly requests". The brief must say so.

## What the skills do NOT cover
- Underwater look, fog, exposure, volumetrics, light shafts, caustics as a method (no look skill; only the sky skill).
- Submarine interior, level dressing, Fab pack use, Megascans handling, package size.
- Creature or serpent modelling and rigging; procedural meshes.
- HUD design, fonts, native C++ UMG, Slate; the only UI skills are Widget Blueprint ones.
- Audio mixing, listening, wave editing, spatialisation settings.
- Video editing, audio capture, encoding to a size.
- Our `ue.ps1` flow: skills assume a live editor with the MCP server, and the editor is memory-heavy on this machine.
- Performance tuning of the renderer (only Niagara and MetaSound cost); the PerfToolset has tools but no skill.
- Anything not read: Dataflow, ChaosCloth, Control Rig, AnimMixer, `source_control` bodies, and every live behaviour of every tool above.
