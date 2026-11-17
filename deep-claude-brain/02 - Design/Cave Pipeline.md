---
updated: 2026-11-15 22:55 EST
status: Proposed by Claude (D-053). Waiting on Quinn. Nothing here is built.
---
# Cave Pipeline

How we make caves that look and play like real drowned caves, with the editor's own tools. The research behind it: [[Cave Gen - Engine Tools]] (what UE 5.9 has, read from its headers), [[Cave Gen - Real Caves and Methods]] (real passages, and how others generate caves) and [[Games to Learn From]]. Sizes come from [[Blockout Scale Card]], the route and beats from [[Slice Brief]].

## Short answer
Generate the rock first, then dress it with PCG. A human draws the cave's skeleton in the map: splines for passages, volumes for chambers. Geometry Script turns the skeleton into one solid rock shell, shaped by the rules real caves follow, and bakes it into static meshes in 40 to 60 m chunks. PCG then dresses the baked rock with boulders, rubble and silt, and a human finishes by hand. Everything is built in the editor and saved; nothing generates while the game runs.

Why that order: PCG places things on surfaces; it cannot make the rock. Its one mesh-from-spline node, Spline To Mesh, offsets and extrudes a flat curve and does not sweep a tube. The shell needs Geometry Script, which can still run inside a PCG graph through one custom node.

## The six steps

| # | Step | Who | Tools | Result |
|---|---|---|---|---|
| 1 | Skeleton | Quinn or Colby, in the map | Splines, volumes | The route, with sizes and passage types |
| 2 | Shell | A tool an agent builds; humans run it | Geometry Script in a PCG graph | One closed rock shell per chunk, baked |
| 3 | Geology | The same tool, and the rock material | Geometry Script, material | Rock that reads as cut by water |
| 4 | Dressing | A PCG graph an agent builds | PCG | Boulders, rubble, silt, rare formations |
| 5 | Hand pass | Quinn or Colby | Modeling Mode, placed actors | Landmarks and fixes |
| 6 | Atmosphere | An agent; Quinn approves frames | Fog, lights, Niagara | Murk, layers in the water, marine snow |

### 1. Skeleton
- One spline per passage. Each point has a width, a height and a type: tube, keyhole, bedding plane, canyon or restriction. Real caves give the shapes; the Scale Card gives the clearances.
- One volume per chamber, dome or shaft.
- The skeleton stays hand-drawn because it is the pacing: where the squeeze is, when the cavern opens.

### 2. Shell
- Passages: rings sampled along the spline, then `AppendSweepPolygon` sweeps that type's cross-section through them, scaled per ring. Chambers: closed blobs.
- `ApplyMeshSolidify` (a voxel wrap) merges it all into one watertight shell, with no seams where passages meet. `ApplyMeshMorphology` closes pinholes; `FlipNormals` turns the shell inward if it faces out.
- Two qualities: a quick preview at 1 m cells while someone moves spline points, and the bake at 25 cm.
- Bake with Save Dynamic Mesh To Asset: one static mesh per chunk, Nanite on, collision from the mesh, so World Partition can stream the chunks.
- Where it runs: a PCG graph with one custom node (a Blueprint subclass of `UPCGGeometryBlueprintElement`), so editing a spline rebuilds its chunk. That needs the beta PCG Geometry Script Interop plugin. Fallback: the same steps in an Editor Utility with a Build button.

### 3. Geology rules
These are what make it read as real.
- Tubes are round or oval. A keyhole is a tube with a slot cut into its floor. A bedding plane is wide and low, so it is for the diver only.
- Scallops, small dishes that flowing water dissolves into the rock, on walls where water ran; from a displacement texture.
- Bedding: horizontal ledges and colour bands.
- Breakdown: chambers floored with fallen blocks; tubes floored with flat silt.
- Formations (stalactites and the like) only in side chambers that were once dry, and few of them.
- A triplanar wet-limestone material with silt on up-facing surfaces. Nanite tessellation, a per-material switch in 5.9, adds close-up relief if it tests well.

### 4. Dressing
- Mesh Sampler (in the same beta plugin) on the baked shell; split the points into floor, wall and ceiling by their normals.
- Floors get boulders, rubble and silt; walls get ledges and flakes; ceilings get the odd formation.
- Pieces from Megascans (free on Fab, [CGChannel](https://www.cgchannel.com/?p=179945)) where they pass the [[Style Guide]].
- Generated in the editor and kept. Read Epic's PCG skills first (`Engine\Plugins\PCG\Skills`).

### 5. Hand pass
- Fixes in Modeling Mode, and one landmark per section: the wreck, a collapsed arch, light from far above at the entrance.

### 6. Atmosphere
- Base murk from height fog and volumetric fog; Local Fog Volumes for silt clouds.
- One halocline (the shimmering line where fresh water sits on salt) and one sulphide cloud, as at Cenote Angelita.
- Silt that billows when the sub or the diver touches the floor: a silt-out, one of cave diving's classic dangers.
- Light dies with distance as a real dive light's does (little past 15 to 30 m), while the cone stays as wide as Quinn asked (D-049).

## Why not the others

| Option | Why not, for now |
|---|---|
| Runtime noise caves (today's stand-in) | Looks like noise, not water-cut rock, and can't place a squeeze before a reveal |
| Simulating karst | The route must be drawn for pacing anyway; the open code we found (Paris et al. 2021, MIT) gives a skeleton, not rock |
| Voxel Plugin 2 | $349, and no confirmed support for UE 5.9 |
| Houdini | About $299 a year a seat (Indie), and you would both need one |
| A modular cave kit | Seams and repeats show in a cave |
| Sculpting it all by hand | Too slow for hundreds of metres of passage |

## Where I'd push back
- **Don't simulate karst.** A player can't tell a simulated route from a drawn one, but they can tell the surface: tube shapes, scallops, breakdown, silt, light. Put the realism there.
- **Bake in the editor.** Building the shell at runtime is slow, and the collision has to be ready before the sub arrives.
- **Buy nothing yet.** Prove the free route with a spike first.
- **Settle the hull's size before anyone draws a passage.** Every clearance depends on it.

## Hull size sets the passages
BT-01, our sub, is 6.15 m long, 1.66 m wide and 2.51 m tall as modelled. At ×2 it is 12.3 × 3.3 × 5.0 m: as long as the Scale Card's hull, but narrower and taller. In the card's 7 × 6 m squeeze that leaves 1.85 m each side but only 0.5 m above and below. I'd keep ×2 and make squeezes 7 m tall. The Atlantic Explorer, about 8 m long, becomes the wreck in the cavern.

## The spike
- **Scope:** one 60 m stretch, a tube into a restriction into a breakdown chamber, in a scratch map.
- **Measure:** time to build a chunk, triangles, GPU time, whether the hull collides cleanly, and frames.
- **Pass:** Quinn says it looks real; a chunk builds in 2 minutes or less; the hull collides cleanly; GPU under 16.7 ms.
- **If it fails:** a Voxel Plugin 2 trial or Houdini. If only the beta interop misbehaves, the Editor Utility fallback.
- **Needs:** Quinn's OK for `deep/Content/Deep/CaveGen/L_CaveSpike.umap`; the editor for a few hours while nobody else has it open (one Unreal at a time); probably the beta interop plugin on. One opus agent, which writes the recipe and its sources down before building.

## Games to learn from
The cave lessons from [[Games to Learn From]]:
- **Deep Rock Galactic:** darkness is a resource, and the player's own lights show the cave's shape. Give the diver a few placeable lights; the serpent notices them.
- **Abzu:** light and colour change with depth. Light from above only near the entrance; below that, only the sub's and the diver's lamps.

The five to do first, across the game: noise-driven hearing (Outer Wilds); a director above the serpent's senses (Alien: Isolation); sound before shape, with long silences (Subnautica); failure shown in the world, not in bars (Dead Space, Barotrauma); interference as the alarm (Content Warning).

## Reverse engineering: where the line is
- **Yes:** learn how a game behaves from playing it, footage, GDC talks, dev blogs, postmortems and published breakdowns. Measure what we can see in our own play: record ten seconds of a sub turning, and an agent can work out its speed and turn rate from the frames. Then build our own version.
- **No:** decompiling, datamining, or pulling code, shaders, models, textures, sounds or levels out of a game's files, and no leaked material. EULAs forbid it and copyright covers the assets, and the useful part, the behaviour, can be seen without it. We also don't copy a specific game's characters, art or levels.
