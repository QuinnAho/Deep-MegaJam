---
updated: 2026-11-15 22:55 EST
verified: 2026-11-15 by reading headers, .uplugin files and PCG SKILL.md under C:\Program Files\Epic Games\UE_5.9\Engine\Plugins. Nothing was run. Items marked UNCONFIRMED were not settled from the files. At 22:40 EST the orchestrator checked three more items in the headers; they are marked "checked 22:40".
---
# Cave gen: what UE 5.9 gives us

`GS` = `Engine\Plugins\Runtime\GeometryScripting\Source\GeometryScriptingCore\Public\GeometryScript\`. The editor module is `GeometryScriptingEditor\Public\GeometryScript\`.

## 1. Geometry Script
- **Sweep along a spline:** `AppendSweepPolygon` and `AppendSweepPolyline` (`GS\MeshPrimitiveFunctions.h`). The path is a `TArray<FTransform>` (position, rotation, scale per ring), so a tunnel is a polygon swept with varying scale. Also `AppendRevolvePolygon`, `AppendSimpleExtrudePolygon`. No "sample spline" call here: build the transforms from the spline yourself, or let PCG do it.
- **Voxel and SDF:** `ApplyMeshSolidify` (voxel wrap by winding number plus marching cubes) and `ApplyMeshMorphology` (Dilate, Contract, Close, Open) in `GS\MeshVoxelFunctions.h`. Options: `FGeometryScript3DGridParameters` (`GridCellSize` or `GridResolution`), `WindingThreshold`, `CustomBounds`, `bSolidAtBoundaries`. No public SDF-field or implicit-surface API: grepping the folder for SDF and implicit finds only these two files and `VolumeTextureBakeFunctions.h`. Practical route: sweep tubes and chamber blobs as closed meshes, union them, then Solidify to a clean shell.
- **Booleans:** `ApplyMeshBoolean`, `ApplyMeshSelfUnion`, `ApplyMeshPlaneCut`, `ApplyMeshMirror` (`GS\MeshBooleanFunctions.h`).
- **Noise and displacement:** `ApplyPerlinNoiseToMesh`, `ComputePerlinNoise`, `ApplyDisplaceFromTextureMap`, `ApplyDisplaceFromPerVertexVectors`, `ApplyMathWarpToMesh` (`GS\MeshDeformFunctions.h`). Also `ApplyMeshOffset` (`GS\MeshModelingFunctions.h`).
- **Remesh, simplify, smooth:** `ApplyUniformRemesh`, `ApplyAdaptiveRemesh` (`MeshRemeshFunctions.h`); `ApplySimplifyToTriangleCount`, `ApplySimplifyToTolerance`, `ApplyClusterSimplifyToEdgeLength` (`MeshSimplifyFunctions.h`); `ApplyIterativeSmoothingToMesh`, `ApplyImplicitSmoothingToMesh` (`MeshDeformFunctions.h`).
- **UVs:** `SetMeshUVsFromBoxProjection`, `...FromPlanarProjection`, `...FromCylinderProjection`, `AutoGeneratePatchBuilderMeshUVs`, `AutoGenerateXAtlasMeshUVs`, `ApplyTexelDensityUVScaling` (`GS\MeshUVFunctions.h`). No triplanar UV function; do triplanar in the material.
- **Normals:** `ComputeSplitNormals`, `SetPerVertexNormals`, `ComputeTangents` (`GS\MeshNormalsFunctions.h`). The shell must face inward for the player. UNCONFIRMED which way Solidify faces it. To flip it: `FlipNormals` or `FlipTriangleSelectionNormals` in the same header (checked 22:40).
- **Collision:** `SetStaticMeshCollisionFromMesh`, `SetSimpleCollisionOfStaticMesh` (`GS\CollisionFunctions.h`).
- **Save as asset:** `CopyMeshToStaticMesh` (`GS\MeshAssetFunctions.h`), with `bApplyNaniteSettings` and Nanite options in its options struct. New assets: `GeometryScriptingEditor\CreateNewAssetUtilityFunctions.h`, editor only. UNCONFIRMED: whether `CopyMeshToStaticMesh` itself is editor-only; treat it so.
- **Cost (my estimate, not measured):** a 250 m by 12 m by 8 m tunnel box at 25 cm cells is about 1000 x 48 x 32 = 1.5M cells; the 60 m chamber is about 240^3 = 13.8M cells; 50 cm cells cost 1/8 of that. Solidify scans the whole bounding box, so a winding tunnel wastes most of it. Split into chunks of about 40 to 60 m with `CustomBounds` and a small overlap, and run it offline in the editor, never at runtime.

## 2. PCG and PCGGeometryScriptInterop
- Plugin `Engine\Plugins\PCGInterops\PCGGeometryScriptInterop`: beta, version 0.2, off by default, needs PCG and GeometryScripting.
- **Yes, one graph can go spline to dynamic mesh to Geometry Script to output.** Nodes (from `...\Public\Elements\*.h`): Create Empty Dynamic Mesh, Spline To Mesh, Static Mesh To Dynamic Mesh, Append Meshes From Points, Merge Dynamic Meshes, Boolean Operation, Dynamic Mesh Transform, Get Dynamic Mesh Data, Get Materials, Set Materials, Spawn Dynamic Mesh, Save Dynamic Mesh To Asset, Mesh Sampler, Primitive Cross Section. Display names are from the class names, UNCONFIRMED in the graph editor. **Spline To Mesh does not sweep a tube** (checked 22:40): it offsets and extrudes a flat curve (settings Thickness, ExtrusionVector, CurveOffset, EndShapes, JoinMethod). Tunnels need the custom step below.
- **Custom Geometry Script step:** subclass `UPCGGeometryBlueprintElement` (Blueprint, pre-wired dynamic mesh pins, never cached) and call any function from section 1.
- **Output:** Spawn Dynamic Mesh makes a managed dynamic mesh component; Save Dynamic Mesh To Asset bakes a static mesh asset. Bake once, enable Nanite on the asset.
- **Sample a mesh surface:** `Mesh Sampler` (`PCGMeshSampler.h`: `SamplingMethod` default one point per triangle, optional `bVoxelize` with `VoxelSize`). Core PCG also has Surface Sampler, Volume Sampler, Spline Sampler, World Raycast and Normal To Density (`PCG\Source\PCG\Public\Elements\`).
- **Floor, wall, ceiling:** sampled points carry the surface normal in their rotation. Use Normal To Density, or filter on the dot product of the normal with Z (floor above about 0.7, ceiling below about -0.7, wall between). UNCONFIRMED: exact attribute names; check in the graph. Floor points get boulders, rubble and silt; ceiling points get drips and stalactites; wall points get ledges and growth.
- **Generation modes:** partitioned (component split across World Partition cells), hierarchical generation (`EPCGHiGenGrid`, 400 cm to 51200 cm grids, `PCGCommon.h`), and runtime (`GenerateAtRuntime`, streams around a generation source). Best fit for placed rubble: partitioned or hierarchical, generated in the editor. Keep the shell as a baked asset. Runtime generation is UNCONFIRMED as useful for a cave.
- **Epic's PCG skills** (`Engine\Plugins\PCG\Skills\`). Scope is runtime generation only; offline authoring is "not yet covered".
  - `SKILL.md` (root): scope and index.
  - `runtime-generation-setup` (+ `landscape`, `meshterrain`): inspect the level, pick a recipe; landscape grass graph; meshterrain is an unfinished placeholder.
  - `runtime-generation-validation` (+ the same two): grid alignment, radii, culling.
  - `runtime-generation/low-end-devices`: mobile limits.
  - `performance`: `pcg.LogProfilingData`; cut repeated work and CPU/GPU transfers.
  - `debug-inspection`: `pcg.GraphExecution.LogOutputData 1|2`, node data views.
  - `manual-edit`: generated actors are overwritten on regenerate; keep edits as manual edits or graph changes.

## 3. Look and playing space
- **Nanite tessellation:** Epic's Nanite page describes it as programmable displacement from a displacement map or procedural material and lists no limits. A production-ready label was NOT found: UNCONFIRMED, read the 5.9 release notes. How to switch it on (checked 22:40): per material, with `bEnableTessellation` (Nanite category, `Material.h`) and `DisplacementScaling`; the cvar `r.Nanite.Tessellation` defaults to 1. Related: `Experimental\NaniteDisplacedMesh` (beta, offline pre-displacement).
- **Modeling Mode, by hand:** Sculpt, PolyModel, Boolean, Simplify, Remesh, Hole Fill, Weld, Normals and UV tools. UNCONFIRMED for 5.9: from memory of 5.x, not read here. Plugin `Experimental\MeshModelingToolsetExp` exists.
- **Lumen:** UNCONFIRMED, test in the editor. Expect lamp light to bounce well in a sealed shell and dark areas to be noisy.
- **Fog and murk:** Local Fog Volumes for silt pockets, exponential height fog plus volumetric fog for base murk, Heterogeneous Volumes for dense clouds (costly). From general knowledge, UNCONFIRMED for 5.9.
- **World Partition:** many chunks of about 40 to 60 m, one actor each, so cells stream and cull. One huge mesh cannot stream. Advice, UNCONFIRMED.
- **Collision:** complex-as-simple on a 250 m Nanite triangle soup is simple to set up, but a simplified collision mesh is safer for a moving hull. UNCONFIRMED, test both.

## 4. Anything made for caves
- No plugin is labelled cave, voxel or "Landscape 3D". The only candidates, all experimental:
  - `Experimental\MeshPartition`: spatially partitioned mesh authoring, modifiers (including Boolean) and adaptive runtime meshes.
  - `Experimental\MeshTerrainMode` (off by default): tools for Mesh Partitions; has `MeshPartitionHeightSculptTool.h`.
  - `PCGMeshPartitionInterop`, `PCGPrimitives_MeshPartitionInterop`, `MeshPartitionWater`.
  - `Experimental\VirtualHeightfieldMesh`: a heightfield, so no overhangs.
- UNCONFIRMED whether Mesh Partition handles overhangs; test before relying on it.

## What we would chain together
1. A human blocks out centrelines as splines and chambers as volumes in the map.
2. Geometry Script: sweep a polygon along each tunnel (`AppendSweepPolygon`) with varying scale; make chambers as blobs.
3. Union in chunks of 40 to 60 m (`ApplyMeshSelfUnion` or `ApplyMeshBoolean`).
4. `ApplyMeshSolidify` at 50 cm, then 25 cm; `ApplyMeshMorphology` Close or Open to remove pinholes.
5. `ApplyPerlinNoiseToMesh`, `ApplyImplicitSmoothingToMesh`, `ApplySimplifyToTolerance`; check normals face inward.
6. Box-project UVs; triplanar rock material.
7. `CopyMeshToStaticMesh` with Nanite on, one asset per chunk, plus collision.
8. A partitioned PCG graph on the baked meshes: Mesh Sampler, normal filters, then boulders, rubble and silt props.

## Sources
- `C:\Program Files\Epic Games\UE_5.9\Engine\Plugins\Runtime\GeometryScripting\Source\GeometryScriptingCore\Public\GeometryScript\*.h`
- `...\Plugins\PCGInterops\PCGGeometryScriptInterop\` (uplugin, `Elements\*.h`)
- `...\Plugins\PCG\Source\PCG\Public\PCGCommon.h` and `Elements\`
- `...\Plugins\PCG\Skills\**\SKILL.md`
- `...\Plugins\Experimental\MeshPartition`, `MeshTerrainMode`, `NaniteDisplacedMesh`
- https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-virtualized-geometry-in-unreal-engine
- https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-9-release-notes
