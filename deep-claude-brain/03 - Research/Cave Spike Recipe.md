---
updated: 2026-11-16 02:30 EST
status: Built and measured (cave-spike agent, branch build/cavegen). "As built" and Results below override the plan where they differ. Look awaits Quinn.
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
- Plugin: `PCGGeometryScriptInterop`, beta 0.2, off by default, enabled in `deep/Deep.uproject` (Quinn OK'd, Nov 16).
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
