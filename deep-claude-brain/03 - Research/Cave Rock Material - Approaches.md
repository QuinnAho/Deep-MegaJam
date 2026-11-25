---
updated: 2026-11-25 01:56 EST
---
# Cave Rock Material - Approaches

Question from Quinn: "we need material on the wall." Research only; nothing was built. Related: [[Style Guide]], [[Polish - Engine Skills and Toolsets]], [[Cave Gen - Engine Tools]], [[Cave Gen - Real Caves and Methods]], [[Games to Learn From]], [[Asset Library Shortlist]].

Legend: **[V]** verified in our files. **[R]** read about, with a link. **[?]** not confirmed.

## 1. What we have [V]
- `cavegen.py` `finish()` (line 2403) flips and recomputes normals, then box-projects UV set 0 (`set_mesh_u_vs_from_box_projection`, box 4x4x4). UVs exist but are crude box UVs with seams. `write_mesh` sets `enable_recompute_tangents = True`, so tangents exist.
- No vertex colour, AO, curvature or other per-vertex data is baked. The words "vertex color" never appear in the script.
- Every chunk gets `M_CaveGrey` (0.18 grey, roughness 0.75, `used_with_nanite` set) in slot "Rock" (`grey_material()`, line 3233). Nanite is off by default (`apply_mesh_settings`).
- `M_CaveRock` (`deep/Content/Deep/Cave/build_cave_materials.py`) already does object-space triplanar, two tile sizes, silt on up faces, a style-grade and wet roughness 0.35 to 0.55. It reads no UVs and is not applied to the chunks. It lives in `Deep/Cave`, not the CaveGen lane folder.
- The Dragon Cave pack has `T_CaveRocks01/02/03`, `T_LargeRock01/02/03`, `T_StoneWall`, `T_InfernoRocks01` (BC, N, ORM each) in `deep/Content/DragonCave/Textures`. It is not in git.
- Gap: none of this has been seen in frames on the chunks.

## 2. Ways to texture generated rock

| Method | Gives | Cost and risk |
|---|---|---|
| **Triplanar / world-aligned** | Texture with no UVs, no seams on a smooth surface. Epic ships `WorldAlignedTexture` for this ([material functions](https://dev.epicgames.com/documentation/en-us/unreal-engine/texturing-material-functions-in-unreal-engine)). | 3 samples per texture per layer. Stretches on slopes between axes and blurs where projections blend. In **world** space the texture swims on anything that moves, so use **object** space (our `M_CaveRock` does) ([same page, swimming note](https://docs.unrealengine.com/4.27/en-US/RenderingAndGraphics/Materials/Functions/Reference/Texturing)). Chunks are static, so object space is safe. |
| **Geometry Script UVs** (box, planar, cylinder, patch builder, XAtlas, texel-density scaling) | Real UVs, which allow baked textures and decals-by-UV. | [V] These functions exist ([[Cave Gen - Engine Tools]]). Box UVs are already in. XAtlas or patch UVs cost bake time on 100k+ triangle meshes and cut many islands, which show as seams under a tiling texture. **Do not use for tiling rock.** [?] bake time not measured. |
| **Bake per-vertex data** (AO, curvature, up-facing, height) | Cheap masks for cavity darkening, wetness and silt that follow the real shape. Epic's Geometry Script reference lists **Bake Vertex**, which fills vertex colours from AO and other bake types ([reference](https://dev.epicgames.com/documentation/unreal-engine/geometry-scripting-reference-in-unreal-engine), [Python API, 5.2](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/GeometryScript_Bake?application_version=5.2)). | Zero runtime cost beyond 4 bytes a vertex. Needs vertex density: the mesh has about 720-point rims and tens of thousands of vertices, so colour is smooth but low in detail. A vendor says the Modeling Mode vertex bake works on LOD0 only ([Fab listing](https://www.fab.com/listings/321ec0b6-7f89-4728-9b66-e6a27a9c6daf?lang=en), vendor claim, [?]). Up-facing and height need no bake: `normal.z` and object-space Z in the material give them. |
| **Nanite tessellation and displacement** | Real silhouette relief from a displacement map or procedural material. | Epic describes it as "dynamic programmable displacement" ([Nanite docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-virtualized-geometry-in-unreal-engine)). A search found Epic text labelling static displacement experimental and noting no crack-free solution ([search summary, not read first-hand](https://dev.epicgames.com/documentation/unreal-engine/working-with-naniteenabled-content)). [?] 5.9 status unconfirmed; 5.7 notes do not mention it ([Tom Looman](https://tomlooman.com/unreal-engine-5-7-performance-highlights/)). It needs Nanite on, but we run Nanite off because it measured cheaper ([V] `apply_mesh_settings`). **Skip.** |
| **Detail normal** | Fine grain at 5 to 50 cm that a 30 to 60 m chunk cannot hold. | One extra triplanar normal fetch. Cheap. Already half-built (two tile sizes). |
| **Decals** (streaks, stains) | Flowstone streaks and mineral stains where we choose. | Needs placed actors or a spawner, which means a map or level change (humans own maps). Deferred decals add cost on many overlaps. Defer. |
| **Substrate / material layers** | Layered wet-over-dry coat. | Substrate is optional, adds shader cost, and nothing in our files says the project enabled it. A plain roughness lerp does the wet look. Skip. [?] Substrate status in 5.9 not read. |
| **Runtime Virtual Texturing** | Blends rock into a silt floor by sampling one shared texture. | Needs RVT volumes in a map and a project setting. Overkill when silt is a normal.z mask. Skip. |

Packaged-build risks that apply to all of them [V from our own notes, `Nanite usage flag` and the `2048` cap]: a material without the Nanite flag falls back to the default material in a cooked game; `Custom` nodes can cause Nanite artefacts ([Nanite docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-virtualized-geometry-in-unreal-engine)); pack textures need the pack at cook.

## 3. Texture sources
- **Dragon Cave** (installed, read-only): `T_CaveRocks01/02/03` for wet limestone and breakdown, `T_LargeRock01-03` for big blocks, `T_StoneWall` as a cut-stone fallback only if it grades well. Never `T_InfernoRocks`. Already graded in `M_CaveRock`.
- **Megascans** (Quinn owns 47 surfaces plus 1,399 3D models, [[Asset Library Shortlist]]). Epic made the whole library free on Fab on 5 Nov 2026 ([DigitalProduction](https://digitalproduction.com/2026/11/13/quixel-megascans-free-again/)). Licence: sources disagree. One says the Fab Standard Licence covers any engine; items claimed under the old terms stay Unreal-only until re-claimed ([Indie Tooling](https://indietooling.com/posts/megascans-free-fab-again), [Quixel licence](https://quixel.com/license)). We ship in Unreal, so either way is fine. Read the licence text before shipping the repo with any source file.
- Surface fit: **limestone wall** = a limestone or cave-wall surface (look for wet, layered, pitted); **flowstone** = a smooth calcite or travertine surface, roughness low, used on a mask; **silt** = fine mud or sand, dark, flat, roughness 0.9; **breakdown blocks** = large rock or cliff surface at the big tile size. [?] The exact Megascans surface names were not checked against Quinn's library.
- Pack and Fab textures are capped at 2048 [V] `DefaultDeviceProfiles.ini`.

## 4. What reads under a moving spotlight underwater
Only what is new beyond [[Cave Gen - Real Caves and Methods]]:
- Specular is the point. Wet flowstone looks shiny under a dive light and scallops catch it ([Wikipedia: Scalloping](https://en.wikipedia.org/wiki/Scalloping_(cave_feature)), [Just Gotta Dive](https://www.justgottadive.com/dive_resources/technical_articles/explore-cave-diving-cavern-formations-explained)). A broad roughness range (0.35 to 0.55 for rock, 0.9 for silt, per the style guide) gives the lamp something to find.
- Silt backscatter washes out lit surfaces ([Scuba Diving](https://www.scubadiving.com/diving-deep-indonesias-unexplored-cave-system)), so keep the silt albedo low (0.09 to 0.16) and rough.
- Cavity darkening makes cracks read at distance, where the beam is wide and the normal map is gone.
- Two scales: a 1 to 2 m detail (grain, pits) and a 8 to 15 m macro tint patch. Without macro variation, repeats show in a 30 m cone.
- Colour stays inside the style guide: value at most 0.25, saturation at most 0.35. Warm hues only for work lights.
- Wayfinding without breaking the style guide ("emissive glow as decoration" is banned): no emissive on rock. Use a lighter, lower-roughness band on the intended route, painted in vertex colour, so the lamp finds it.

## 5. How other games do it
Sources here are thin. I found no GDC talk on the texturing of any of these.
- **Deep Rock Galactic**: rooms are hand-built volumes; the biome sets how surfaces are sculpted and what colours and dressing are used ([Steam discussion quoting the devs](https://steamcommunity.com/app/548430/discussions/1/3874842509595862797/), [GamingBolt interview](https://gamingbolt.com/deep-rock-galactic-interview-heigh-ho-off-to-plunder-we-go)). Take: biome-level material sets with the shape logic doing the work. A triplanar shader is **not** confirmed by any source [?].
- **No Man's Sky**: the GDC talk covers shaping terrain with noise filters (ridges, valleys, erosion), not materials ([Game Developer](https://www.gamedeveloper.com/design/video-building-the-worlds-of-i-no-man-s-sky-i-using-math-s-)).
- **Astroneer**: density voxels meshed per chunk ([Game Developer](https://www.gamedeveloper.com/design/what-i-astroneer-i-s-devs-learned-while-leaving-early-access)). Texture method not found.
- **Subnautica** (look reference): removed outside ambient light from caves with volumetric data, so lights carry the scene ([Game Developer](https://www.gamedeveloper.com/design/how-i-subnautica-i-plunges-deeper-into-rendering-realistic-water)). Same principle as DEEP.

## 6. Options

| | A. Wire up `M_CaveRock` as is | B. **Recommended:** `M_CaveRock` plus baked vertex data | C. Real UVs and baked textures |
|---|---|---|---|
| Work | Small: a material instance in the CaveGen folder, set in `grey_material()` | Medium: add a bake step in `finish()`, a new material variant | Large: XAtlas UVs, bake textures per chunk |
| Gain | Rock, silt, wet | Adds cavity, route band and macro control that follows the shape | Unique detail per place |
| Cost | About 3 triplanar sets | Same, plus vertex colour read | Texture memory per chunk, seams |
| Risk | Flat look on large faces | Bake needs a test on one chunk | High; hurts the "one command bake" loop |

## 7. Recommended recipe (B)
Layers, in order, in a new `M_CaveRock_Gen` (child or copy of `M_CaveRock`, saved in `Deep/CaveGen/Materials`):
1. **Base rock**: `T_CaveRocks01` and `02` triplanar in object space, small tile about 2 m.
2. **Macro variation**: the existing slow world mask blending the two tile sizes (8 to 15 m) and a value shift of plus or minus 15%.
3. **Silt on up faces**: `normal.z` plus a wobble (existing); add the object-space height so silt pools lower.
4. **Cavity and AO**: vertex colour R, baked with Bake Vertex AO; multiply albedo and cut specular.
5. **Wetness**: roughness 0.35 to 0.55 from ORM, lowered 0.1 by a noise mask, and a lower roughness patch (0.25 to 0.3) on flowstone areas picked by a vertex-colour G mask or normal.z plus noise.
6. **Detail normal**: the small-tile normal, blended UDN (existing).
7. **Route accent** (optional, last): vertex colour B, a lighter albedo band, not emissive.
8. **Grade**: existing style-guide cap.

What changes in `cavegen.py`:
- `finish()`: after normals, call Bake Vertex (AO, 16 rays, a distance of about 3 m) into vertex colour R; write G and B from code (G = flowstone, B = route) by position.
- `grey_material()`: return `M_CaveRock_Gen` when it exists.
- `write_mesh`: keep `enable_recompute_normals = False`; check vertex colour survives `copy_mesh_to_static_mesh`.
- Material build script: copy the `M_CaveRock` recipe, add a vertex-colour node, set `used_with_nanite = True`, and build with `build_cave_materials.py`-style Python.
- The agent works only in `Deep/CaveGen/`. Do not touch `Deep/Cave/` or the pack.

Risks:
- **Pack dependency**: the cook fails without Dragon Cave. Mitigation: copy the three or four graded textures into `Deep/CaveGen/Textures` (graded, capped at 2048, a derived work; check the pack's licence first) or accept the Fab install step already in use.
- **Texture memory**: 3 textures x 3 maps x 2048 = about 50 MB with BC compression, one set shared by all chunks.
- **Usage flags**: Nanite flag on (we are Nanite off, but a future switch needs it), and `used_with_static_lighting` is not needed.
- **Vertex colour loss** on mesh rewrite: test before committing to the bake.

## 8. Test plan
1. Bake one chunk (C1). Frame under the helm floods, the bow lights and the diver lamp at 2 m, 10 m and 30 m, in the editor.
2. Compare against `M_CaveGrey` and the plain `M_CaveRock` (A) from the same camera. Quinn approves frames before anything merges.
3. Package, and take the same frames in the cooked game; look for default-material fallback and missing textures.
4. GPU cost: `stat gpu` on the same view, against the 4 ms baseline; the budget is plus 0.5 ms.
5. Check value and saturation against the style guide in the frames.
6. Tests: `tools\ue.ps1 test -Filter <cavegen prefix>` for the lane only.

Open for Quinn: should the route accent exist at all, or is the sonar pulse enough? Which Megascans surface does he want for flowstone?

## Orchestrator's read (Nov 25, 1:56 AM)
- **Agree with B.** Start from Dragon Cave's rock, which is installed and already graded in `M_CaveRock`. If Quinn installs a Megascans limestone surface, compare it in the same frames.
- **Route band:** I prefer the guideline in [[Sub Driving and Seeing - Approaches]], a real line along the route. Keep vertex colour B free.
- **Order:**
  - This lane edits `cavegen.py` and re-bakes every chunk, and so does `build/cavestop3` (C2). C2's shape merges first, and this lane starts from that main.
  - It's part of the caves' look pass (D-028), so judge its frames under the new forward floods as well as today's.
