# Lane: rock on the cave walls (D-070, from Nov 25)

Quinn, Nov 25: "we need materu\ial on the wall". He said "go" at about 2:05 AM, with Dragon Cave's rock as the texture source. The research and the orchestrator's read (option B) are in [[Cave Rock Material - Approaches]]. How the generator runs is in [[Cave Spike Recipe]].

The branch is `build/cavestop3` in `.wt\cave3`. It's the generator's lane, and it already holds C2's shape (`4888633`), which still waits for Quinn's OK. Another lane is changing the helm's camera, controls and floods at the same time on `build/drive` in `.wt\cave`. Don't touch `deep/Source` (the floods and the camera are its job) or `Deep/Cave/`.

**First, merge `origin/main`.** It brings M2b (`a64d217`) and the vault. Expect one conflict in `deep/Content/Data/CaveChunkSockets.csv`, where both sides appended rows: M2b's `Relic` row for C1, and C2's rows. Keep both, and keep every existing row byte-identical.

## R1: the rock material
1. **A material built by a script** and saved only in `deep/Content/Deep/CaveGen/` (for example `Materials/M_CaveWall`, with the script in `CaveGen/Python/`).
   - Copy the recipe of the jam's `M_CaveRock` (`deep/Content/Deep/Cave/build_cave_materials.py`):
     - object-space triplanar sampling of Dragon Cave's `T_CaveRocks01` and `02`, at two tile sizes with a slow mask between them;
     - silt on faces that point up, and UDN normals;
     - the style-guide grade: value at most 0.25 linear, saturation at most 0.35;
     - wet roughness 0.35 to 0.55 from ORM.
   - Never edit `Deep/Cave/` or the pack.
2. **Add these:**
   - Cavity darkening from a per-vertex bake. Geometry Script's vertex AO bake goes into vertex colour R in `cavegen.py`'s `finish()`, at about 3 m distance. It darkens the albedo and cuts the specular in cracks.
   - A macro value shift of about ±15% over 8 to 15 m.
   - A lower-roughness flowstone patch, but only if it reads as wet rock.
   - Test the bake on C1 first, and check that the vertex colour survives the static mesh write.
   - Keep vertex colour B free. There's no route band; a guideline comes later.
3. **Re-bake every chunk in the string** (S, A, T1, C1, B, C2) with the new material and vertex data. The geometry doesn't change, so the string checks (joints, lips, overlap, the hull sweep, cameras) give the same numbers as before.
4. **Set the usage flags** so a cooked game draws the material: the Nanite flag on, even though the chunks run with Nanite off.

**Frames:**
- The generator's frame cameras (`CaveCameras.csv`) and the game with the caves on, `M_CaveGrey` against the new material at the same views, under today's helm floods.
- If the flood pitch can be set without a code change (Project Settings in an editor session, not committed), the same views with `Look.HelmFloodPitchDeg` and `HelmFloodLowPitchDeg` near 0.
- The diver's lamp at 2, 10 and 30 m from a wall in S and in C1.
- C2's hall from the berth.
- One packaged frame in the caves, to check nothing falls back to the default material.

**Numbers:**
- GPU time at the same view, against today's (about 4 ms in the caves). The budget is +0.5 ms.
- The packaged zip size.

**Tests:** the generator's own checks, and once before reporting, `tools\ue.ps1 test -Filter Deep.Sim.CaveString` and `-Filter Deep.World.CaveDive`.

Not in R1: the guideline, decals, Megascans, Nanite displacement, and any map other than the lane's scratch map `L_CaveSpike`.

## Log
- Nov 25, about 2:10 AM: the lane was planned on `build/cavestop3` in `.wt\cave3`.
- Nov 25, about 2:15 AM: merged `origin/main` (`a06b524`). The sockets CSV kept main's C1 `Relic` row and C2's rows, all byte-identical.
- Nov 25, about 2:15 AM: **every Unreal launch on this machine failed**: `Plugin 'Fab' failed to load`. The engine's Fab plugin was updated on Nov 19 (BuildId 60738873) and the engine is still 60351404. Workaround with no file change: the process environment `UE-CmdLineArgs=-DisablePlugins=Fab` (read by `LaunchEngineLoop.cpp` in editor builds), set in the lane's slot wrapper. The real fix is Quinn's: update the engine in the launcher, or turn Fab off.
- Nov 25, about 2:35 AM, step 1: `M_CaveWall` built by `CaveGen/Python/build_cavewall_material.py` (M_CaveRock's recipe plus cavity from vertex R, a ±15% macro shift, flowstone patches). First frames on C1: the pow-4 triplanar blend smeared texture on 45° faces and the silt read as a flat pale sheet, so the blend is pow 8 and the silt keeps some rock grain. Some stretched streaks remain on sharp folds.
- Nov 25, step 2 finding: a full `bake('C1')` gives the same counts and bounds but a different position hash, so a full re-bake is not bit-identical. The string is therefore re-painted, not rebuilt: `cavegen.repaint(name)` reads the saved mesh, bakes the vertex data and writes it back. `finish()` bakes the same data for future builds.
- Nov 25, step 2: `repaint('C1')` in 4.9 s. Positions and triangles hash the same as before (192,765 vertices, 384,098 triangles). Vertex colour survives the static mesh write: R (AO) p10 0.75, p50 0.94, min 0; G and B 0, A 1; one colour per vertex, so no vertex splits.
