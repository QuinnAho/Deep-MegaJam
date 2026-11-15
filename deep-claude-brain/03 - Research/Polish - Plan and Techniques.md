---
updated: 2026-11-15 01:50 EST
verified: 2026-11-15 01:37 EST against the installed UE_5.9 source and plugin folders (headers, .uplugin files, engine Content), the Deep module source on origin/main (6f2e259) and the stopped look pass on the local branch look/pass1 (2ab227e). Nothing was run in an editor. Items marked UNVERIFIED were not checked.
---
# Polish: plan and techniques (D-028 pass)

The plan for the one polish pass: start about 10 AM, feature freeze 3 PM. Each work package (WP) in section 9 can start from this note plus the files it names. Rules for every WP: the game logic and its tests do not change, art is a skin over the grey boxes, and nothing from the GDD Cut list comes back (no Water plugin surface, no PCG terrain).

**The look in one line:** a black ocean where light is scarce and has to be bought. At 100 m there is dim blue-green water with shafts from above; at 300 m it is near black and only the lamp, the sub's lights and salvage glints show; at 500 m it is pure black with bioluminescence. A thin cyan instrument HUD sits on top, with the sub's screens in the world.

## What exists, and what the stopped look pass taught

- `ADeepEnvironment` (World/DeepEnvironment.*) has one directional `Downlight`, a `Fill`, an `UExponentialHeightFogComponent` and an unbound `UPostProcessComponent`. Grey-box mode (`Arena.bGreyBox = true`, the default) uses fixed exposure and no haze. The water branch switches per stop in `OnRep_Stop` and snaps.
- The renderer is Lumen GI and reflections, VSM, ray tracing on, Substrate on, DX12. There is no project map: the game runs on `/Engine/Maps/Entry`, and `DeepArena::EnsureArena` spawns the world.
- Lessons from `look/pass1` (keep them): the far fog's directional inscattering lifted the view to milk, so set it black. Auto exposure undoes darkness, so lock exposure. Local exposure lifts shadows, so set both contrast scales to 1.0. Volumetric fog needs a start distance of about 1.5 m, or the helmet lamp lights a blob on the visor. Reuse its property list (`SunColor`, `WaterAlbedo`, `ScatteringDistribution`, `VolumetricDistanceM`) and its `Deep.Tune` live-tuning command, but not its numbers.

## 1. The underwater look at 100, 300 and 500 m

**Recommendation:** in code. `ADeepEnvironment` ticks on every machine and blends between three presets by the replicated `SubDepthM`, not by `Stop`, so driving fades continuously. Exposure is manual. Every number lives in a new `FDeepLookPreset` array in `UDeepTuningSettings` (no ini section). Set `bGreyBox` to false.

| Setting (component, property) | 100 m | 300 m | 500 m |
|---|---|---|---|
| Downlight `Intensity` (lux) / `VolumetricScatteringIntensity` | 2.0 / 1.0 | 0.04 / 0.4 | 0, hidden |
| Downlight colour, rotation | (0.35,0.65,0.85), pitch -70, yaw 35 | same | n/a |
| Downlight `bCastVolumetricShadow` (LightComponentBase) | true: the hull and the trench cut shafts | true | n/a |
| Fog `FogDensity` / `FogHeightFalloff` / `FogMaxOpacity` | 0.02 / 0.001 / 1.0 | 0.035 / 0.001 / 1.0 | 0.05 / 0.001 / 1.0 |
| Fog `FogInscatteringColor` (far colour) | (0.010,0.045,0.060) | (0.002,0.008,0.012) | (0,0,0) |
| Fog `SetDirectionalInscatteringColor` | black | black | black |
| Volumetric fog on; `SetVolumetricFogAlbedo` / `ExtinctionScale` | (64,166,191) / 1.0 | same / 1.5 | same / 2.0 |
| `SetVolumetricFogDistance` / `StartDistance` (cm) | 8000 / 150 | 6000 / 150 | 4000 / 150 |
| PP `AutoExposureMethod` Manual, `ApplyPhysicalCameraExposure` false, `AutoExposureBias` | 0.0 | +1.5 | +2.0 |
| PP `LocalExposure{Shadow,Highlight}ContrastScale` | 1.0 | 1.0 | 1.0 |
| PP `BloomIntensity` / `VignetteIntensity` / `FilmGrainIntensity` | 0.4 / 0.35 / 0.10 | 0.5 / 0.45 / 0.18 | 0.6 / 0.55 / 0.25 |
| PP `ColorSaturation` (R,G,B,All): red goes first | (0.75,0.95,1,0.9) | (0.5,0.9,1,0.8) | (0.4,0.85,1,0.75) |
| PP `ColorContrast` All / `SceneFringeIntensity` | 1.05 / 0 | 1.08 / 0.2 | 1.1 / 0.35 |
| PP `MotionBlurAmount` | 0.2 | 0.2 | 0.2 |
| Visibility target with no local light | silhouettes to 40 m | 4 m round sub lights | 0 m |

- **Light shafts:** volumetric fog plus the downlight's volumetric shadow. The sub's hull and the trench wall (section 2) block the light, which makes shafts at 100 m with no extra asset. For the fallback when looking up, use `bEnableLightShaftBloom` and `bEnableLightShaftOcclusion` on the directional light; both were verified in DirectionalLightComponent.h.
- **Caustics:** a light function material (`LightFunctionMaterial` on the light; verified) on the downlight at 100 m only: panning Voronoi noise at a scale of about 3 m and an intensity of 0.3 to 1. This needs one material asset. It is the first thing cut, because at 100 m it is a stylistic choice rather than physics.
- **Particles in the water (marine snow):** code only. A `UInstancedStaticMeshComponent` of about 1,200 tiny engine spheres (scale 0.01 to 0.03) with the lit `BasicShapeMaterial` tinted pale grey, hung from `ADeepArenaRoot`. In Tick, each instance's position (base plus root offset) wraps into a 30 m box around the view. The flakes are lit only by local lights, so they glitter in the lamp beam and vanish in the dark, which sells "you need the light". Fallback: a Niagara GPU sprite system made in the editor (NiagaraToolsets), placed in the root's local space.
- **Silt and murk (optional):** an `ALocalFogVolume` (verified: LocalFogVolumeComponent.h, `RadialFogExtinction`, `FogAlbedo`, `FogPhaseG`) around the relic site at 500 m and on the floor at each stop.
- **The diver's lamp:** keep `FDeepLampTuning` (40,000, inner cone 12, outer 24, range 35 m). Add `VolumetricScatteringIntensity` 2.0 so the beam shows in the water, cast shadows on, and move the light 25 cm right and 15 cm up from the eye. That offset makes shadows and the beam's edge visible; a lamp exactly at the eye looks flat. Add one slow, small intensity wobble (2 percent, 0.3 Hz) and no flicker.
- **The sub's own lights** (the hero shot; reuse the look/pass1 numbers): a belly flood (spot, 100,000, 35 degrees, 60 m, volumetric 1.0), red and green marker lights blinking at 1 Hz on the tower and the nose, and the hatch light. These are what the player sees at 300 and 500 m when the lamp points away.
- **Why the darkness at 300 m gates on the lamp:** exposure is locked, the far fog is near black, and the downlight is about 50 times dimmer, so with the lamp off a salvage piece 10 m away is a dim glint with no shape. Make salvage glints scale down with depth (glint at 300 m = 30 percent of the 100 m value, at 500 m 15 percent): the player sees where something is, not what it is or what lies between. The gate check: two shots from the hatch at stop 2 with the same `Deep.Look`, `Deep.Lamp 0` and `Deep.Lamp 1`. Lamp off must give an average frame luminance below 0.04 with no salvage outline; lamp on must show the nearest salvage inside the beam. The helm still enforces the 200 m limit (D-023); the darkness is the reason the player sees.
- **Fallback for the whole section:** keep the per-stop snap in `OnRep_Stop` and blend only the post-process over 2 s with a second `UPostProcessComponent` weight.

## 2. Driving: the sub stays still and the world moves

**Recommendation, in order of effect:**
1. **Marine snow hung from the arena root** (section 1). When the root moves the flakes stream upward past the window, which is the strongest motion cue and costs little.
2. **A trench wall:** a continuous cliff of Megascans rock pieces from 0 to 620 m, 15 to 25 m beside the sub, hung from the root (dressing rows of the "scatter" kind, section 7). It gives scale and parallax, it catches the belly flood as the sub passes, and it blocks the downlight for shafts.
3. **A continuous depth blend** of fog, light and grade from `SubDepthM`, so the world visibly darkens while driving.
4. **Helm feel:** a low sway (0.3 degrees, 0.2 Hz) and a rumble shake while `HelmStep` input is held (EngineCameras `UPerlinNoiseCameraShakePattern`; the plugin is verified present and enabled by default), plus a 0.5 s settle on park.
5. **Depth markers:** every 50 m an emissive ring or buoy line on the wall with the depth stencilled, so the HUD depth tape and the world agree.
- **Engineering trap:** dressing is placed in absolute depth. `ApplySubDepth` re-bases the root when the nearest stop changes, so the dressing loader must place pieces at (absolute depth minus the current stop depth) and re-place them on a stop change, or the wall jumps.
- **Fallback:** snow and the depth blend only, with the helm camera feed treated as a "hull camera" (section 3).

## 3. The room inside

- **Lighting:** cool, dim ambient (rect light strips in the ceiling, 6,500 K tinted toward cyan, low), warm amber practical lights at the console (a 1 m rect light), a red emergency light over the hatch rim that pulses when the hull is damaged (`HullDamage`), and the screen glow. Keep `RoomLight` as a soft fill at 30 percent of now. Lumen lights the room; there is no baking (static lighting is off).
- **Window:** split the front wall into four cubes around a 1.7 x 0.7 m opening, with a frame. The outside world is real geometry in the same world, so no render target is needed. The glass is one translucent Substrate material (`M_Glass`, a faint fresnel tint) made in the editor. **Move the helm camera inside**, behind the seat, looking out of the window past the helm screen. The static frame against a moving world reads as driving better than any effect. Fallback: keep the outside camera (`HelmCameraM`) and frame it as a hull camera feed with a scanline and vignette overlay in the HUD.
- **Screens:** `UWidgetComponent` (UMG; verified) on `ConsoleScreen` and `HelmPanel`, drawing the same C++ widgets as the HUD (section 5): the console shows cargo, the Captain's cut and the next gate; the helm shows a depth tape, the stops and the limit. Fallback: `UTextRenderComponent` with the engine's `UnlitText` material (verified).
- **Skin:** Sci-Fi Creatures Research Lab wall, floor and ceiling pieces from dressing rows with `parent=sub`. The grey cubes stay as hidden collision. Hide the lab's creature capsules.

## 4. The serpent

**Recommendation:** a procedural tube in code. Use `UProceduralMeshComponent` (ProceduralMeshComponent plugin, enabled by default; add the module to Deep.Build.cs) and rebuild it each tick along the existing `SegmentTransforms`: 48 rings of 12 vertices, radius tapering from 0.9 m behind the head to 0.1 m at the tail, a dorsal ridge (two vertices raised 30 percent), and UVs along the length. Keep the head, jaw and eye spheres, but stretch the head into a wedge. Motion: the existing travelling weave, plus a roll of 10 degrees into turns and the jaw opening on Bite. Give it one material `M_SerpentSkin` (editor): near-black, wet roughness 0.25, a cyan-green fresnel rim so the lamp outlines it, and two rows of emissive spots that pulse faster with `GetMonsterThreat01`. In darkness, a rim and moving spots read as intended.
- **Fallback:** keep the instanced spheres, raise them to 40 smaller overlapping segments with a taper, and use the same material.
- **Owned creature asset:** none fits (shortlist: no serpent, eel or snake; the fantasy packs are stylised and their rigs are unknown). Rejected.

## 5. The HUD

**Recommendation: UMG built in C++, with no widget blueprints.** Native `UUserWidget` subclasses build their tree in `NativeOnInitialized` (verified: a native class gets a `WidgetTree` in `UUserWidget::Initialize`). This keeps everything in text and mergeable, and the same classes work in `UWidgetComponent` for the world screens. Fonts need no asset: `FCoreStyle::GetDefaultFontStyle("Light" | "BoldCondensed" | "Mono", size)`, which is Roboto Light, Roboto Bold Condensed and Droid Sans Mono, verified in LegacySlateFontInfoCache.cpp. `FSlateFontInfo::LetterSpacing` exists. Use `UBackgroundBlur` behind menus (verified). The canvas `ADeepHUD` stays as the owner: it creates the root widget and switches modes. Keep the old canvas path behind a cvar until the new one passes its gate, then delete it. Fallback: pure Slate (`SCompoundWidget` through `AddViewportWidgetContent`), also code only.
- **Style:** one accent colour (cyan, about #58E1D6), amber for warnings, red for danger only, and white text at 88 percent. Numbers in Light, large; labels in Bold Condensed caps with letter spacing 120; readouts in Mono. Panels are black at 55 percent with 1 px hairlines and an 8 px grid. Motion: fades and slides of 150 to 250 ms with ease-out, numbers that tween, bars that lerp, and nothing that bounces. Scale from a 1080p reference.
- **Layout by mode:**
  - **Water:** depth (large) and tether length as a thin arc, bottom left. Cargo in kg and its value, bottom right. Patience as a thin bar at the top centre, with the Captain's mood word. Subtitles in the lower third with a name tag. The harpoon reticle and pip at the centre. Bailout air as a ring around the reticle. Threat as a red pulse on the screen edges, not a bar. Bite as a red band at the top with a countdown.
  - **Aboard:** no HUD. Interaction prompts ([E] plus a verb) are anchored to the hatch, console and helm, and fade in by distance.
  - **Helm:** the world screen carries the data, plus one line of controls.
  - **Title, pause, death, settlement, review:** full-screen panels over a blurred, darkened scene. Settlement and review stay full screen because judges must read the jokes.
- **In the world instead:** the console and helm screens (section 3), and the visor edge (vignette plus, later, a curved-glass post-process material). The wrist gauge is cut: there is no first-person arm mesh.

## 6. Assets or code

| Item | Route | MCP toolsets (Live Catalog) |
|---|---|---|
| Water look, exposure, grade, lamp, sub lights | Code only | none |
| Marine snow, depth blend, helm sway and shake, helm camera move, window opening | Code only | none |
| Dressing loader (trench, floor, ruins, room skin) | Code plus CSV, using Quinn's packs | LevelToolset or SceneCapture only to check |
| Caustics light function, `M_Glass`, `M_SerpentSkin`, emissive depth-marker material | Editor assets | MaterialToolset, MaterialInstanceToolset, TextureGraphTools |
| Serpent tube | Code only | none |
| HUD and world screens | Code only | none |
| Bubbles, silt puffs, harpoon trail (Niagara) | Editor assets | NiagaraToolsets (System [23], Component, Assets) |
| Texture size caps | Text config (`DefaultDeviceProfiles.ini` texture LOD groups, max 2048) | none |
| Installing Fab packs and Megascans | **Quinn's hands** (Fab sign-in, a browser, his licence) | FabToolset could, but it signs into his profile |
| Final taste check, frames, the window and helm framing | **Quinn's hands** | none |

**The rule to replace "agents create no binary assets" for the pass (from 10 AM to 3 PM):**
1. Agents may create `.uasset` files only through MCP in their own lane folder, `deep/Content/Deep/<Lane>/` (Look, Room, Serpent, FX). One lane has one owner branch and one worktree. No agent edits or saves an asset outside its lane folder. No agent creates a `.umap`.
2. Third-party packs go in `deep/Content/Packs/` and are read-only for agents. Quinn installs them once into a folder outside git (`C:\DeepPacks\Packs`), and each worktree links it with `mklink /J deep\Content\Packs C:\DeepPacks\Packs`. Add `deep/Content/Packs/` to .gitignore. Nothing from a pack is committed during the jam (no LFS, GitHub's 100 MB file limit, push time). UNVERIFIED: that the asset registry and cooker follow a junction. Fallback: `robocopy` the folder into each worktree.
3. Add `*.uasset binary` and `*.umap binary` to .gitattributes. Lane folders do not overlap, so merges never need to combine two versions of one asset. If a conflict appears anyway, the orchestrator takes the lane owner's side and never hand-merges.
4. Only one interactive editor runs at a time on the machine (the "editor slot"). Asset lanes take turns in it, booked on the Board. Quinn closes his editor while an agent holds the slot. Code lanes compile with `ue.ps1 build` in their own worktrees meanwhile. Headless `verify` and `shot` runs follow the existing one-builder rule.
5. Pack paths appear only in `Content/Data/Dressing.csv` and lane materials. A missing asset logs a warning and draws nothing, so a worktree without packs still builds, tests and plays.

## 7. Owned packs in a code-built arena

**Recommendation: keep the arena in code and add a placement data file.** A new `ADeepDressing` actor is spawned by `EnsureArena` and reads `deep/Content/Data/Dressing.csv`. Each row has: `id, parent (sub|root), stop (0 = all), kind (place|scatter), mesh path, material override, x_m, y_m, depth_m or z_m, yaw, pitch, roll, scale`, and for scatter rows `count, radius_min_m, radius_max_m, depth_from_m, depth_to_m, scale_min, scale_max, seed`. It builds one HISM per mesh, with no collision and with shadows. It also flips the grey-box cubes to hidden-but-colliding when a skin row targets them (`sub.walls`, `sub.deck`, `floor`).
- **Why:** everything outside must hang from the root that driving moves, and the loader does that for free. Tests and `ue.ps1 shot` keep working on the empty Entry map. Placement is diffable text, so several lanes can add rows without binary conflicts. Scatter rows give a professional density from a few lines.
- **Migration cost:** about 1 to 1.5 h of Opus for the loader with a parse test and a re-base test (`Deep.World.Dressing`), then about 45 min per area of rows tuned by shots. No existing actor changes except a visibility flag.
- **Fallback, an authored level:** one map `/Game/Deep/Maps/Deep` with art actors attached to a placed `ADeepArenaRoot` and `ADeepSubmarine` (`EnsureArena` already uses what a map provides), and `GameDefaultMap` pointed at it. Cost: about 1 h of setup plus one owner for the map all day. Every art change becomes a serialized binary edit, and tests that assume an empty map need checking.

## 8. Budgets

- **Package:** limit 1 GB zipped, now 426 MB. Budget **750 MB zipped**, leaving 250 MB of margin. Cap textures at 2048 in `DefaultDeviceProfiles.ini`. Use at most 10 Megascans models and only the pack meshes the CSV references (only referenced assets cook; delete nothing of Quinn's). Run `.\tools\ue.ps1 package` then `.\tools\ue.ps1 zip` after WP3 and after WP8, and stop adding art above 650 MB zipped. A size lever, but not a pass decision: `r.RayTracing=False` cuts shaders and cook time.
- **Frame rate:** 60 fps at 1920x1080 on this laptop (RTX 4080 Laptop, 12 GB) with no dips below 50, measured at each stop. Frame time must stay at or under 16.6 ms, with GPU time at or under 14 ms. The measurement, one per stop (Delay must exceed the waits): `.\tools\ue.ps1 shot -Name perf-s3 -Delay 40 -Cmds "r.SetRes 1920x1080w;Deep.GotoStop 3;wait 3;stat unit;startfpschart;wait 20;stopfpschart"`. The PNG shows `stat unit`, and the summary goes to the log and to `deep/Saved/Profiling/FPSChartStats/` (both commands verified in UnrealEngine.cpp and ChartCreation.cpp). The `shot` verb runs `-game` uncooked, so confirm once on the packaged build. If over budget, the levers in order are: `r.VolumetricFog.GridPixelSize` from 8 to 16, lamp shadows off, fewer HISM instances, and a Lumen scalability step.
- **Cook time:** the package now takes 5 to 8 min. Budget **20 min**. The first cook after Substrate converts pack materials will compile shaders for a long time, so run the first art package by 12:30 PM to learn the real figure. That is UNVERIFIED and the biggest schedule risk.

## 9. The schedule (10 AM to 3 PM, three lanes)

Lane A and Lane B are code (Opus at high effort) in `.wt\dev` and `.wt\monster`. Lane C holds the editor slot (Sonnet at medium) in a new `.wt\art`. Each gate: the agent looks at its frames, then Quinn checks the ones marked Q. **Top 3 by effect per hour = WP1, WP4, WP6.**

| # | Time | WP and goal | Model | Inputs | Gate |
|---|---|---|---|---|---|
| 0 | 9:30 | **Quinn:** install packs into `C:\DeepPacks` (Research Lab, Corals, Atlantis Ruins, 8 to 10 Megascans rocks and cliffs at 2K), approve the rule in section 6, choose the editor-slot hours | Quinn | shortlist | the packs open in the 5.9 editor |
| 1 | 10:00 | **Water look** (top 3): depth-blended presets, manual exposure, grade, lamp offset and beam, sub lights, glints by depth, marine snow | Opus, A | section 1, look/pass1 diff | 6 shots: stops 1 to 3, lamp on and off from the hatch; the luminance check at stop 2. Q: the stop-2 pair and stop 3 |
| 2 | 10:00 | **Dressing loader** (section 7) | Opus, B | section 7, DeepArena.cpp | `Deep.World.Dressing` tests green; shot with an engine-shape row at all three stops |
| 3 | 10:00 | **Editor materials:** `M_Glass`, `M_SerpentSkin`, depth-marker emissive, caustics | Sonnet, C | sections 1, 3 and 4 | a sphere preview shot per material in the editor |
| 4 | 11:15 | **Driving** (top 3): helm camera inside, window opening, sway and shake, depth markers | Opus, A | section 2 | 3 shots 1 s apart while driving show the snow moving; Q drives 100 to 300 m once |
| 5 | 11:15 | **Trench, floor and ruins rows** in Dressing.csv | Sonnet, C (needs WP2) | packs, section 2 | shots at each stop plus mid-drive; first art package and zip (size and cook time). Q: one frame per stop |
| 6 | 11:15 | **HUD and world screens** (top 3) | Opus, B | section 5, DeepHUD.cpp | 8 shots: title, aboard, helm, water calm, water bite and cut, settlement, review, pause. Q: title, water, settlement |
| 7 | 12:45 | **Serpent tube and skin** | Opus, A | section 4, Monster/ | 4 shots: lit side view at 8 m, bite head-on, recoil, eyes in the dark. Q: one clip |
| 8 | 12:45 | **Room skin and lights** (parent=sub rows) | Sonnet, C | section 3 | 3 shots: start view, console, helm. Q: the helm view |
| 9 | 13:45 | **Effects:** helmet bubbles to the breath, winch and hatch bubbles, a silt puff on grab, the harpoon trail | Sonnet, C | NiagaraToolsets | 4 shots |
| 10 | 14:15 | **Lock:** perf at each stop, package, zip, `playtest.ps1`, devlog frames | Sonnet, plus a Haiku runner | section 8 | budgets met; 8 of 8 playtests; Q plays to the win |

**Cut first if time runs out:** WP9 effects, then caustics, then the serpent tube (keep the spheres with the new skin), then the room pack skin (keep the cubes with the new lights). Never cut WP1, WP6 or the window.

## 10. Preview-engine risks

- **Verified in the 5.9 source or plugins:** the height-fog volumetric setters and `SetDirectionalInscatteringColor`; `ALocalFogVolume`; the post-process fields used above (in Scene.h); the light-shaft flags; `bCastVolumetricShadow`; `LightFunctionMaterial`; `UWidgetComponent`, `UBackgroundBlur` and `URetainerBox`; native `UUserWidget` getting a `WidgetTree`; Roboto's Light, BoldCondensed and Mono faces; `FCompositeFont` from a file (the `FSlateFontInfo(FString)` constructor is deprecated since 5.6); ProceduralMeshComponent and EngineCameras both enabled by default; `UDynamicMeshComponent`; the `MSM_SingleLayerWater` shading model; `STARTFPSCHART` and `r.SetRes`; the `Color` parameter in `BasicShapeMaterial` and `EmissiveMeshMaterial`; the Water, Niagara, NiagaraFluids and Volumetrics plugins.
- **UNVERIFIED:**
  - How Substrate, Lumen and volumetric fog look together on this preview (look/pass1 got milk; tune by shot).
  - Whether 5.8 Fab packs convert to Substrate cleanly, and the cost of the first cook's shader compile.
  - Megascans glTF import through Interchange.
  - Whether a junctioned Packs folder is seen by the asset registry and cooker.
  - Whether MaterialToolset and NiagaraToolsets can author a full graph (the tools are listed in the catalog but none was exercised).
  - `RecordViewport` clips.
  - UMG built in C++ inside `UWidgetComponent` on Substrate (render-target gamma).
  - Lamp intensity units on this engine (keep the current value and tune by shot).
