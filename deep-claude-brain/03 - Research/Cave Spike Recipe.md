---
updated: 2026-11-16 06:00 EST
status: Round 3 built and measured (cave agent, branch build/cavegen): the hull fits by construction (a clear core), passages change section, walls follow beds and joints, joint rims match vertex for vertex, and the PCG path is removed. "Round 3" below overrides earlier sections where they differ. Look awaits Quinn.
---
# Cave Spike Recipe

How the spike builds two 30 m cave chunks with the editor's own tools, and where each step comes from. Plan: [[Cave Pipeline]]. Tools: [[Cave Gen - Engine Tools]]. Joint rules: [[Cave Gen - Chunk Assembly]] section 4.

## Brief, as changed on Nov 16
- **Joint profile:** a round neck, 9 m across. One parameter, `JointDiameter` = 900 cm. Every chunk starts and ends on it.
- **Chunk A:** neck, an oval tube with a flat silt floor, a 7 x 7 m squeeze in the middle, an oval tube again, neck.
- **Chunk B:** neck, a breakdown chamber (wider, taller, floored with fallen blocks), neck.
- **Hull:** BT-01 at x2, 12.3 x 3.3 x 5.0 m (working assumption). Margin at the neck: the hull's corner sits 3.0 m from the axis against a 4.5 m radius. In the 7 x 7 squeeze, 1.85 m each side, 1 m above and below.
- **Level:** `L_CaveSpike` places A, B, A end to end, so the A to B and B to A joints are both visible and the hull sweep runs A, B, A.

## Where it runs
A PCG graph, `PCG_CaveShell`, with one custom node:

`Create Empty Dynamic Mesh` -> `BP_CaveShellElement` (Blueprint subclass of `UPCGGeometryBlueprintElement`) -> `Spawn Dynamic Mesh` (preview) and `Save Dynamic Mesh To Asset` (bake).

- `UPCGGeometryBlueprintElement` comes with dynamic-mesh pins and is never cached. Override `ProcessDynamicMesh(InDynMesh, OutTags)` and work on the mesh in place. (`PCGGeometryBlueprintElement.h`)
- The node finds its skeleton from the context: `GetContextHandle` -> `PCGBlueprintHelpers::GetComponentWithContext` -> the PCG component's owner. (`PCGBlueprintBaseElement.h`, `PCGBlueprintHelpers.h`)
- The skeleton is an actor, `BP_CaveSkeleton`, with a spline (the passage), a box (the chamber volume) and the PCG component. So editing a spline point regenerates that chunk.
- `Save Dynamic Mesh To Asset` takes `ExportParams` (`AssetName`, `AssetPath`, `bSaveOnExportEnded`, `bOpenSaveDialog` off) and `CopyMeshToAssetOptions` with `bApplyNaniteSettings` on. (`PCGSaveDynamicMeshToAsset.h`, `PCGAssetExporter.h`, `MeshAssetFunctions.h`)
- Plugin: `PCGGeometryScriptInterop`, beta 0.2, off by default, enabled in `deep/Deep.uproject` (Quinn OK'd, Nov 16). Removed again in round 3.
- **Fallback** if the beta plugin misbehaves: the same Blueprint function called from an Editor Utility with a Build button.

## Skeleton
- Spline points carry width, height, silt depth and type. Stored as named float properties on spline points if they save with the level (`SplineComponent.Add/GetFloatPropertyAtSplineInputKey`), otherwise in the point scale (Y = width, Z = height).
- End tangents point along +X, so a chunk's ends are square to its axis and both joints sit on the X axis. The exit transform is `(L, 0, 0)`, no turn. A chunk can wander between its ends.

## Shell (Geometry Script, in the Blueprint node)
1. **Rings:** sample the spline every 50 cm (preview: 100 cm). Each ring is a transform: position, rotation from the tangent, scale (width, height). The ends are extended 3 m straight out past each joint plane, so the voxel surface is clean when it is cut.
2. **Tube:** `AppendSweepPolygon` with a 48-point superellipse (exponent 2.4, nearly an oval) of unit size, swept along the rings. Capped. (`MeshPrimitiveFunctions.h`)
3. **Silt floor:** a second sweep of a wide rectangle, offset down by the ring's silt depth, appended with its triangles flipped. Solidify works by winding number, so a reversed closed mesh subtracts where it overlaps the tube. Where the silt depth is large (necks, squeeze), the slab misses the tube and does nothing.
4. **Chamber (B):** overlapping ellipsoids inside the chamber box (`AppendSphereBox`, scaled), seeded, plus a reversed slab below the chamber floor.
5. **Breakdown (B):** 20 to 30 tilted boxes on the chamber floor, appended reversed, so they are rock in the air. Blocks within 3.5 m of the axis stay below the hull's keel (z < -300 cm).
6. **Solidify:** `ApplyMeshSolidify`, `SizeMethod = GridCellSize`, cell 100 cm (preview) or 25 cm (bake), `WindingThreshold` 0.5, `bSolidAtBoundaries`. (`MeshVoxelFunctions.h`)
7. **Pinholes:** `ApplyMeshMorphology`, `Close`, distance one cell.
8. **Joint cut:** `ApplyMeshPlaneCut` at x = 0 and x = L, `bFillHoles` off, so each end is an open loop lying exactly in the joint plane. (`MeshBooleanFunctions.h`)
9. **Joint snap:** for every vertex within 1.5 m of an end, blend its (y, z) onto the exact 9 m circle, weight 1 at the plane and 0 at 1.5 m. Both chunks then end on the same circle, square to the axis, so the surfaces and their normals meet.
10. **Geology:** `ComputePerlinNoise` at two scales, plus a bedding step on wall vertices (horizontal ledges every 1.4 m), applied with `ApplyDisplaceFromPerVertexVectors` along the normal. Fades to zero in the 1.5 m snap zone and next to it, and scales down in the squeeze so the hull keeps its clearance. (`MeshDeformFunctions.h`)
11. **Inward:** the solid is the water, so its normals face out of the water. `FlipNormals` turns them to face the player. (`MeshNormalsFunctions.h`)
12. **UVs and normals:** `SetMeshUVsFromBoxProjection` (the material is triplanar and does not need them), `RecomputeNormals`.

**Known risk at the joint:** both ends lie on the same circle, but the two chunks' end loops have different vertices. Chords of the circle differ by under a millimetre at 25 cm edges, so a hairline gap is possible. If the frames show it, the fallback is to cut each end 30 cm past the plane (an overlap) and lay a breakdown block over the joint.

## Bake
- `Save Dynamic Mesh To Asset` to `/Game/Deep/CaveGen/Meshes/SM_CaveChunk_A` and `_B`, Nanite on.
- Collision from the mesh: the static mesh's body setup set to complex as simple (`CollisionTraceFlag = UseComplexAsSimple`). A sealed concave shell cannot be approximated with simple shapes.

## Material
`M_CaveRock_Wet` in `Deep/CaveGen/Materials`: world-aligned (triplanar) wet limestone.
- Base colour, normal and roughness from Dragon Cave rock textures, sampled read-only with `WorldAlignedTexture` / `WorldAlignedNormal` (engine material functions), at about 4 m per tile.
- Silt on up-facing surfaces: lerp to a pale silt colour by `saturate((VertexNormalWS.z - 0.55) * 4)`.
- Bedding bands: a faint colour band from world Z.
- Wet: low roughness (0.25 to 0.4) on the rock, higher on the silt.
- Used with Nanite (`bUsedWithNanite`), so a cooked build keeps it (lesson from Nov 15 in `.claude/loop.md`).

## Measure
- Build time: wall clock around the node at each quality, logged per chunk.
- Triangles: `GetNumTriangleIDs` after the bake.
- GPU: `stat gpu` / `GpuProfilerToolset` in the tube, at a joint and in the chamber. Lighting: no sky, no sun, exposure fixed, one spotlight at the camera (diver lamp).
- Hull: `BoxTraceSingle` (complex trace) of a 12.3 x 3.3 x 5.0 m box along the centreline of A, B, A in 25 cm steps, oriented to the spline tangent. A start-penetrating hit reports its depth. Control: the same sweep offset 3 m sideways must hit, to prove the test sees the shell.
- Frames: 4 PNGs in `deep/Saved/CaveSpike/` (tube, A to B joint, chamber, B to A joint).

## Sources
- Epic PCG skills: `C:\Program Files\Epic Games\UE_5.9\Engine\Plugins\PCG\Skills\SKILL.md`, `debug-inspection`, `performance`, `manual-edit`. Their scope is runtime generation; offline authoring is "not yet covered". Used: profiling with `pcg.LogProfilingData`, inspecting node output with `GetNodeDataView`, not editing generated output by hand.
- Interop plugin: `Engine\Plugins\PCGInterops\PCGGeometryScriptInterop\` (`.uplugin`; `Public\Elements\PCGGeometryBlueprintElement.h`, `PCGSaveDynamicMeshToAsset.h`, `PCGMeshSampler.h`, `PCGSpawnDynamicMesh.h`).
- PCG: `Engine\Plugins\PCG\Source\PCG\Public\Elements\Blueprint\PCGBlueprintBaseElement.h`, `Helpers\PCGBlueprintHelpers.h`, `PCGAssetExporter.h`.
- Geometry Script: `Engine\Plugins\Runtime\GeometryScripting\Source\GeometryScriptingCore\Public\GeometryScript\` (`MeshPrimitiveFunctions.h`, `MeshVoxelFunctions.h`, `MeshBooleanFunctions.h`, `MeshDeformFunctions.h`, `MeshNormalsFunctions.h`, `MeshAssetFunctions.h`, `CollisionFunctions.h`), and the live `unreal` Python API in the editor.
- Winding-number solidify (why a reversed mesh subtracts): Jacobson et al., "Robust Inside-Outside Segmentation using Generalized Winding Numbers", SIGGRAPH 2013; `ApplyMeshSolidify` documents "space with generalized winding number higher than this threshold is considered inside".
- Joint rules: [[Cave Gen - Chunk Assembly]] section 3 (one fixed profile, weld or overlap, a rock over the joint) and section 4 (9 m neck, 7 x 7 squeezes).
- World-aligned textures: https://dev.epicgames.com/documentation/en-us/unreal-engine/world-aligned-textures-in-unreal-engine
- Nanite: https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-virtualized-geometry-in-unreal-engine

## As built (differences from the plan above)
- **Ring profile:** a 64-point ellipse, not the superellipse. The silt slab is `w * 0.62` wide, so it never reaches the walls above the floor line.
- **Solidify in two stages:** tube and chamber first, then the reversed silt slab and breakdown blocks. Then `RemoveSmallComponents` drops the voxel crumbs.
- **PCG graph:** a `Bake` bool on the skeleton drives a Branch. Off goes to Spawn Dynamic Mesh (preview at 100 cm); on goes to Save Dynamic Mesh To Asset (25 cm). The skeleton's `ChunkName` sets `AssetName`.
- **Gotcha, auto-regen:** setting spline points from Python does not regenerate PCG until `spline_has_been_edited` is True and `update_spline()` has run. Remove the preview meshes after each bake, or they double the geometry.
- **Gotcha, material slot:** Save Dynamic Mesh To Asset leaves the static mesh with no material slots. After each bake, set `static_materials` to one slot (`Rock`, `M_CaveRock_Wet`), set collision to complex-as-simple and save.
- **Texture:** `M_CaveRock_Wet` samples DragonCave `T_LargeRock02` (read-only) through `WorldAlignedTexture`.
- **Hull clearance rules found by the sweep:**
  - A squeeze must be level and straight for at least the hull length plus the bends either side. A's squeeze is 7.8 m nominal, which leaves about 7 m clear after rock noise.
  - A neck must be entered and left gently. B's floor rises and falls over 4 m or more, not 2 m.
- **Shadows:** with hardware ray-traced shadows, the diver lamp drew black jagged patches on the walls. They came from the coarse ray-tracing fallback of the Nanite mesh. `r.RayTracing.Shadows 0` (virtual shadow maps) removes them; this is set per editor session and is not saved. Fix for the game: either full-detail ray-tracing proxies, or VSM for local lights.

## Results (Nov 16, editor on a laptop GPU)
| Measure | Result | Bar |
|---|---|---|
| Shell build, `BuildShell` | A: 0.04 s at 100 cm, 0.53 s at 25 cm. B: 0.07 s and 0.84 s | |
| Full PCG bake, saved, Nanite | A about 3.4 s, B about 4.4 s; preview round trip about 2 s | 2 min or less: **pass** |
| Triangles | A: 2,356 preview, 38,475 baked. B: 3,976 and 66,028 | |
| Joints | Every end-ring vertex at r = 450.00 cm at both ends of both chunks. Ring vertex counts differ (284 to 288), which leaves T-junction gaps of about 0.03 cm. No seam visible in the frames. | |
| Hull sweep, A-B-A, 366 samples at 25 cm, level hull (yaw only) | 0 contacts | clean: **pass** |
| Same, hull pitched to the path | 8 contacts, under 30 cm deep | (a sub stays level) |
| Control, 8 x 8 m section | 355 contacts: the test can fail | |
| GPU, Graphics queue median, VSM shadows | tube 10.6 ms, A-B joint 9.3, chamber 18.1 (only 4 samples, noisy), B-A joint 10.4. GPU at about 1.0 to 1.4 GHz, below full clock. | 16.7 ms: tube and joints pass, chamber not shown |

**Look issues, for Quinn:**
- Triplanar stretch streaks on the near walls.
- A horizontal projection seam across the walls.
- The silt reads too white under the lamp.
- These come from the material, not the joints.

Frames: `deep/Saved/CaveSpike/` (not committed): `tube.png`, `joint_AB.png`, `chamber.png`, `joint_BA.png`.

## Round 2 (Nov 16 night): turning, dropping chunks and a cavern
Brief: dive shape D-058, tunnels that open into caverns. Level and nose-first in tunnels, a ballast sink in caverns, stops in caverns.

### What changed and why
- **Generator moved to Python**: `deep/Content/Deep/CaveGen/Python/cavegen.py`. It still uses Geometry Script only: append shapes, solidify, close, plane cut, snap, flip, box-UV. Authoring a cavern as a Blueprint graph over MCP was impractical. The PCG skeleton actors (`CaveSkeleton_A/B`) are still in the level. With `Bake` on they would overwrite the A and B meshes, so leave it off. (Removed in round 3.)
- **Shapes:** `tube` along a path of straights and arcs, with yaw-only ring frames, so the floor never banks. Plus `ellipsoid`, `vcyl`, oriented `obox` and a `heightfield` floor. A negative noise amplitude pushes only outward, which keeps squeezes at their size.
- **Assembly and checks:** `cavestring.py`. It writes the CSVs, places the string from them, and checks joints and overlap. It also sweeps the hull and checks the sink column.
- **Bake settings, applied automatically** (`apply_mesh_settings`):
  - slot `Rock` with the grey placeholder `M_CaveGrey` (base 0.18, roughness 0.75);
  - collision complex-as-simple;
  - **Nanite off**. Measured against Nanite on with the fallback and the ray-tracing proxy both at 100% triangles, the cost was a wash, within about 2%. Without Nanite, ray tracing traces the same triangles as raster, by construction. The jagged black shards at C1's exit-tunnel ring disappeared. `nanite=True` keeps the 100% fallback and proxy path. No cvars were used.

### Chunks (cell 25 cm)
| Chunk | Type | Exit (cm, deg, entry frame) | Tris | Bake |
|---|---|---|---|---|
| A | tunnel | (3000, 0, 0), 0 | 40,996 | 2.3 s |
| B | cavern-ish | (3000, 0, 0), 0 | 64,932 | 3.8 s |
| T1 | tunnel | (5300, 4800, -600), +90 | 142,072 | 10.6 s (9.6 s of it the Nanite build) |
| C1 | stop | (4600, -2771, -2400), -60 | about 324k | 7.6 to 30 s (voxel step 3.3 s, the rest the Nanite build) |

- **T1:** a 3900 cm straight, then an R 1400 arc through 90 degrees, then a 3400 cm straight. A cosine drop of 600 cm runs over s = 3900 to 8400, about 11.8 degrees at most. The floor is deeper while it drops. The squeeze is r 350 (7 x 7 m) at s = 1260 to 2640, level and straight with a hull length either side.
- **C1:** an entry tube, then a sink column at local (3000, 0), built from three stacked ellipsoids, lobes, a roof dome, ledges, a floor heightfield at -2950 and a rubble pile. The exit is 24 m lower, turned -60 degrees, through a level 9 m neck. A side pocket holds the Serpent.
- **Bend radius**, derived from the box sweep: in a bend of centreline radius R, the outer corner of the level hull reaches sqrt((R+165)^2 + 615^2) - R sideways. With the corner 250 cm above or below the axis, it stays 0.5 m inside the 450 cm joint radius only if the lateral reach is at most sqrt(400^2 - 250^2) = 312 cm. That gives R >= about 1050 cm on the centreline. With zero margin the limit is 635 cm. T1 uses 1400.

### Data
- `deep/Content/Data/CaveChunks.csv`: Name, Mesh, Type, ExitX/Y/Z, ExitYaw, Min and Max XYZ. Entry frame: the joint centre at the origin, +X into the chunk, Z up, cm and degrees.
- `deep/Content/Data/CaveChunkSockets.csv`: Chunk, Socket, X, Y, Z, Yaw. C1 has Park (keel gap 297 cm), Salvage1 to 4 (9.6 to 16.9 m from Park, on up-facing rock found by traces) and Serpent (in the void, 3.8 to 5 m from rock on five sides, open toward the mouth).

### Results (Nov 16, RTX 4080 Laptop, editor viewport 1912 x 637)
| Check | Result | Bar |
|---|---|---|
| Joints A-T1, T1-C1, C1-B | every loop vertex at r 450.00 cm; largest gap 0.31 cm (chord sag) | gap < 1 cm, **pass** |
| Overlap away from joints | 0 wall samples inside a neighbour (19k, 55k, 45k tested) | 0, **pass** |
| Hull sweep A-T1-C1-B, 999 boxes: level, yaw only, sink at Park, in-place turn of -60 degrees | 0 contacts; least clearance 34 cm (uniform box inflation), in T1's squeeze | 0, **pass** |
| Control, the same path 3 m sideways | 616 of 999 hit | must hit, **pass** |
| C1 sink column | narrowest clear width 24.0 m | 16 m or more, **pass** |
| GPU, Graphics queue, 40 to 60 samples, Nanite off | T1 squeeze: median 4.76 ms, p95 4.95. C1 at the Park helm: median 4.82, p95 5.08. Clock 2.2 to 2.3 GHz (max 3.1) | 16.7 ms, **pass** |
| GPU, Nanite on, 100% fallback and proxy | T1 4.85 / 5.12; C1 4.74 / 5.05 | |

### Gotchas
- **The editor throttles in the background.** `EditorPerformanceSettings.bThrottleCPUWhenNotForeground` disables viewport rendering when the editor lacks focus, and GPU samples then read 0 to 47 ms. Set it to False for the session through `get_default_object(load_class(None, '/Script/UnrealEd.EditorPerformanceSettings'))`, using the C++ name `bThrottleCPUWhenNotForeground` (the snake-case name fails), then restore it. Code: `EditorEngine.cpp`, around line 1870.
- **Winding:** every appended shape gets a volume-sign check and a flip. Caps must wind the same way as their bands, or solidify drops whole parts. C1's entry tube vanished once for this reason.
- **Plane cut** removes the +normal side.
- **Python signatures:** `get_all_vertex_positions(mesh, skip_gaps)[1]`; `is_point_inside_mesh(...)[1]`; `build_bvh_for_mesh(mesh)[1]`. `Rotator` has no `rotate_vector`.
- **T1's first drop** was too steep (13.3 degrees) over a flat floor, and the sweep found 16 contacts. Stretching the drop to 45 m and deepening the floor while it drops fixed it.
- **The spike lamp** is now 1500 cd, down from 9000, which blew out walls that close.

### Open
- **Small black specks** near the tunnel lips in T1 and C1, under ray-traced shadows. Possibly voxel slivers; not yet traced.
- **T1's squeeze reads like a pipe:** the outward-only noise is small. C1 reads as a chamber with pockets, but only in grey.
- **C1 has 4 salvage points, not 5.** The ledge at azimuth 120 started inside rock and was skipped.
- **No real rock material yet.** The look pass and Quinn's frames come first.

Frames (not committed), in `deep/Saved/CaveGen/`: `T1_squeeze_helm.png`, `C1_from_entry.png`, `C1_from_Park.png`, `String_overview.png` (top-down silhouette).

### Sources
- Nanite and ray tracing: https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-virtualized-geometry-in-unreal-engine (ray tracing uses the fallback mesh; Fallback Relative Error and Percent Triangles keep detail).
- `UStaticMesh::RayTracingProxySettings` (`Engine\Source\Runtime\Engine\Classes\Engine\StaticMesh.h`) and `FMeshRayTracingProxySettings` (`EngineTypes.h`).
- `EditorPerformanceSettings.h` and `EditorEngine.cpp` (background throttle).
- Geometry Script headers, as listed above.

## Round 3 (Nov 16, early morning): a hull that fits by construction, rock with structure
Brief: in plain grey under the spike lamp, A, T1, C1 and B read as natural rock passages and a natural chamber, and the hull (12.3 x 3.3 x 5.0 m) fits by construction, not by luck in the sweep.

### What changed and why
- **A clear core in every chunk.** It is the hull's box swept along the flight path. The box is grown by 1.0 m in tunnels and caverns (`M_BODY`) and by 0.75 m wherever it reaches a squeeze (`M_SQUEEZE`). Another 15 cm (`CORE_E`) keeps the 35 cm corner rounding (`CORE_RHO`) from eating the margin.
  - In tunnels, the core is one rounded rectangle per ring: the exact slab clip of every grown hull box within 9 m along the path (`core_slice`), so bends and drops are covered. A ring is clamped against its neighbours' slices too, because the core is lofted between rings and the loft cuts in where the margin steps. C1 adds boxes for the entry run, the sink and the exit run, and a vertical cylinder for the in-place turn at Park.
  - The core works three ways. Each section radius is pushed out to at least 30 cm past the core along its ray (`BASE_GAP`, joined by a smooth max 60 cm wide, so no crease). Rock displacement toward the core is squeezed with tanh (`clamp2` in a ring plane, `clamp3` in 3D), so displaced rock stays at least 12 cm outside it (`DELTA`) and never lies flat on it. And the core is appended as water in a third solidify pass, so no block, ledge or pile can enter it.
  - The necks stay exact: the core's rounded corner reaches 446 cm from the axis, inside the 450 cm neck.
  - `core_exposure` counts final vertices within 4 cm of the core, away from the joints. Such a vertex would be a flat patch.
- **Passage sections** (`section_defs`) change every 10 to 20 m and blend between anchors with a smoothstep:
  - `canyon`: 8 m wide at the hull, stepping in to 6 m and then a 3.6 m slot, about 18 m tall, meandering;
  - `keyhole`: a 9 m tube over a 2.6 m floor slot about 5 m deep, meandering;
  - `widelow`: about 15 m wide and 9 m tall, its roof on a bedding plane;
  - `rift`: the squeeze, a near-rectangular rift with a roof slot, its width set by the core;
  - `round`: the occasional round tube; `hall`: B's chamber, about 22 m wide; `neck`: the 9 m circle.
- **Rock structure** (`Rock`, one seeded rock mass per chunk):
  - Beds are 55 to 210 cm thick and tilt 1.2 degrees, rising toward yaw 30. Two vertical joint families strike at yaw 25 and 110, 1.5 to 5 m apart.
  - Each wall point moves along its horizontal normal by the sum of five terms:
    - a per-bed offset of up to 45 cm, so hard beds stand out as ledges and weak ones recess under overhangs;
    - a slow variation of 30 cm, stretched 3.5 times horizontally;
    - a per-block offset of 25 cm between joints, which gives blocky faces with straight vertical edges;
    - an 18 cm groove at each bedding plane;
    - 10 cm of fine noise.
  - Roofs snap up to the bedding plane above, which gives flat, stepped ceilings. Rock floors snap down to the bed below and are blocky. Silt floors only swell downward.
- **Features** (`tunnel_features`, plus shapes placed by hand in B and C1):
  - joint slots: where a joint plane crosses the passage, a straight slot along its strike, 45 to 90 cm wide and thinner at the top, reaching 1.5 to 4 m past the wall;
  - alcoves, as notches along a bedding plane (round alcoves read as windows);
  - `joint_dome`: stepped breakdown domes of 2 or 3 slabs. Each slab ends on a flat bedding-plane ceiling, is squared to a joint strike, and is smaller than the one below and offset from it. A single ellipsoid seen from below read as an eye: a round rim, with its bedding-snapped crown as the pupil;
  - breakdown blocks of 1 to 4 m, tilted, kept out of the core: 12 in T1's wide-low run, 34 each in B and C1;
  - in C1: the sink shaft (clear radius 9.5 m, relief only outward), 4 ledges on bedding planes starting 12.5 m from the column axis, a rubble pile, a floor heightfield, a roof dome, a stepped side dome, and the Serpent's pocket with a narrow mouth;
  - in B: 5 lobes that break the hall's outline and 2 domes.
- **Black specks: the cause, fixed at the source.** Each chunk was cut at its joints on its own, so each rim was its own polygon inscribed in the 9 m circle. Where one rim's edge cut inside the other's, a sliver up to about 0.3 cm wide opened, and the background showed through as a dashed hairline.
  - `conform_rims` moves each rim vertex along the circle to the nearest of 720 canonical angles, counted from the joint's side axis `(-fy, fx)`. Rim edges that span several angles are split, and their triangle becomes a fan.
  - The map keeps the vertex order, so no triangle flips. Both sides of a joint now share their rim edge for edge.
  - The other source of specks was frames captured while the editor was throttled (see Gotchas).
- **The PCG path is removed.**
  - `CaveSkeleton_A/B` and `PCGWorldActor0` are gone from `L_CaveSpike`.
  - `PCG_CaveShell`, `BP_CaveShellElement` and `BP_CaveSkeleton` were referenced only by each other and by those actors, so they are deleted.
  - The `PCGGeometryScriptInterop` entry is gone from `deep/Deep.uproject`, and `tools\ue.ps1 build` passes.
  - "Where it runs" and "Bake" above are now history. The generator is `cavegen.py`, using Geometry Script only.

### Chunks (cell 25 cm, Nanite off)
| Chunk | Sections | Features | Tris (static mesh) | Bake |
|---|---|---|---|---|
| A | neck, canyon, keyhole, neck | 2 slots, 2 alcoves, 1 dome | 104,644 | 5.7 s |
| T1 | neck, canyon, rift squeeze, keyhole, canyon round the bend, wide-low down the drop, round, neck | 7 slots, 10 alcoves, 3 domes, 12 blocks | 280,158 | 15.6 s |
| C1 | keyhole entry, chamber and sink, round then wide-low exit, neck | listed above; 34 blocks | 379,656 | 19.6 s |
| B | neck, hall, neck | 5 lobes, 2 domes, 2 slots, 1 alcove, silt floor, 34 blocks | 115,764 | 7.7 s |

In all, 880,222 triangles, up from about 572k in round 2. Each chunk is 1 component with 2 border loops.

### Results (Nov 16, RTX 4080 Laptop GPU at about 2.0 GHz of 3.1, editor viewport, Nanite off)
| Check | Result | Bar |
|---|---|---|
| Joints A-T1, T1-C1, C1-B | every rim vertex at r 450.00 cm, on the same 720-gon from both sides; largest gap 0.000 cm (round 2: 0.31) | gap < 1 cm, **pass** |
| Overlap away from joints | 0 wall samples inside a neighbour (44,289, 78,669 and 58,004 tested) | 0, **pass** |
| Hull sweep A-T1-C1-B, 1000 samples, 999 boxes | 0 contacts | 0, **pass** |
| Least clearance by region | squeeze 92.2 cm (round 2: 34), neck 93.8, tunnel 106.2, cavern 123.4 | |
| Control, the same path 3 m to each side | 650 and 693 of 999 hit | must hit, **pass** |
| Flat patches (`core_exposure`) | 0 in A, T1 and B. 1 vertex in C1, 4.0 cm from the core, where its entry neck ends: the exact 9 m circle, by design | 0 away from the necks, **pass** |
| C1 sink column | narrowest clear width 21.9 m | 16 m or more, **pass** |
| GPU, Graphics queue, 40 samples, throttle off | T1 squeeze: median 4.40 ms, p95 4.71. C1 at the Park helm: median 4.20, p95 4.48 (round 2: 4.76 / 4.95 and 4.82 / 5.08) | 16.7 ms, **pass** |

- **Nanite stays off.** It was not re-measured on. Round 2 found it a wash, and the Nanite-off numbers improved on round 2's even with 54% more triangles.
- **Sockets** (`CaveChunkSockets.csv`, same columns): Park has a keel gap of 295.3 cm. C1 now has 5 salvage points, 10.7 to 23.4 m from Park, on rock facing up (nz 0.80 to 1.00): 3 ledges, the pile and the floor. The ledge at azimuth 160 was skipped, 25.4 m from Park. The Serpent is in the void, with rock 4.25 to 30.7 m away on its six sides.
- **`CaveChunks.csv`** has the new bounds, with the same columns. The exits did not change.

### Gotchas
- **A mesh with no UVs crashes the editor.** If a DynamicMesh has no UV set, the static mesh build asserts on "NumUVs > 0" and takes the editor down. `finish` now makes sure there is one UV set before the box projection, and raises an error if there is still none. `get_num_uv_sets` is in `GeometryScript_MeshQueries`, not in `_UVs`.
- **The Restore Packages modal blocks MCP.** After a crash, the restarted editor waits on "Restore Packages", and MCP answers "slow task" until it is closed. Closing the window (UIA `WindowPattern.Close`) skips the restore.
- **Flanges did not cure the specks.** A thin flange behind each rim covered the sliver but left a dashed, unlit hairline. Matching the rim polygons is the cure.
- **Collapsed rim edges leave degenerate triangles**, 85 to 593 per chunk, where two rim vertices land on the same canonical angle. The static mesh build drops them, which is why a static mesh has a few hundred fewer triangles than its DynamicMesh.
- **Throttled captures show specks.** Frames captured while the editor was throttled in the background were speckled. Keep `bThrottleCPUWhenNotForeground` off for frames as well as for GPU numbers.
- **The orange box in the overview** was the PostProcessVolume's brush. `ShowFlag.Volumes 0` hides it, along with `ShowFlag.Grid 0` and `ShowFlag.BillboardSprites 0`. These are console commands on the editor world, so restore them afterwards.
- **`tools\ue.ps1 build` fails while an editor is open, and it does not take `Global\DeepUnrealSlot` itself.** Close the editor, take the slot, build, then release the slot.

### Open
- **The look awaits Quinn**, in plain grey only.
- From Park, C1's exit reads as a round tube, with B's open end as a bright disc: an eye.
- The wide-low bedding notches read as rounded fingers.
- In T1's keyhole bend, a flat far wall with a straight notch reads as a doorway.
- Thin dark lines show at concave edges. They are the lamp's shadows, because the lamp sits 40 cm below the camera.
- The degenerate rim triangles could be welded in a later pass.
- There is no real rock material yet.

Frames (not committed), in `deep/Saved/CaveGen/round3/`, plain grey under the 1500 cd lamp:
- the 4 round-2 views: `T1_squeeze_helm.png`, `C1_from_entry.png`, `C1_from_Park.png`, `String_overview.png`;
- T1's tunnel body from the helm: `T1_tunnel_body_helm.png`;
- C1 from Park up the column: `C1_Park_up_column.png`;
- extras: `T1_keyhole_helm.png`, `Joint_A_T1.png`, `Joint_T1_C1.png`, `Joint_C1_B.png`.

### Sources
- Arthur N. Palmer, *Cave Geology*, Cave Books, 2007. Passages follow beds and joints. A canyon cut down from a tube gives a keyhole. Wide, low passages follow bedding planes. Breakdown domes rise in steps along joints and stop at a strong bed.
- Derek Ford and Paul Williams, *Karst Hydrogeology and Geomorphology*, Wiley, 2007: conduit shapes and breakdown.
- Geometry Script headers in `Engine\Plugins\Runtime\GeometryScripting\Source\GeometryScriptingCore\Public\GeometryScript\`:
  - `MeshQueryFunctions.h` (`GetNumUVSets`);
  - `MeshUVFunctions.h` (`SetNumUVSets`, box projection);
  - `MeshRepairFunctions.h` (`CompactMesh`, `RemoveSmallComponents`);
  - `MeshBasicEditFunctions.h`, `MeshVoxelFunctions.h`, `MeshNormalsFunctions.h`.
- Jacobson et al., "Robust Inside-Outside Segmentation using Generalized Winding Numbers", SIGGRAPH 2013. This is why the core, appended as water in the third solidify pass, always wins.
- The smooth max and the rounded-rectangle distance follow Inigo Quilez's formulas: https://iquilezles.org/articles/smin/ and https://iquilezles.org/articles/distfunctions2d/
