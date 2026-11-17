---
updated: 2026-11-15
source: web search 2026-11-15; items marked (unverified) come from general knowledge, not a page I read
---
# Cave Gen: Real Caves and Methods

Scale reference: [[Blockout Scale Card]] (sub 12 x 5 x 4 m, tunnels 7 to 16 m, chambers about 60 m).

## 1. Real flooded caves

**Passage types and typical sizes** (sizes are rules of thumb, unverified)
- **Phreatic tube:** round or elliptical, smooth, formed full of water ([TRU Environmental Geology](https://environmental-geol.pressbooks.tru.ca/chapter/karst-caves-cave-contents-and-subterranean-life/)). Usually 1 to 10 m across. 7 to 16 m is large but real.
- **Bedding-plane passage:** low and very wide, following a layer parting ([Caving Ireland](https://caving.ie/publications/caving-information-pack/types-of-caves)). Often 0.5 to 3 m high and 10 to 50 m wide. Good as a squeeze that is wide but not tall.
- **Keyhole (T-shaped):** a tall narrow canyon cut into the floor of a tube or bedding plane when the water table dropped (same source). Roughly 2 to 5 m wide, 10 to 30 m tall. This is the best shape for a 7 m "squeeze, 6 m tall" slot.
- **Vadose canyon:** a stream-cut slot, tall and narrow, with a meandering floor.
- **Breakdown chamber:** roof collapse with a floor of boulders and a domed or stoped ceiling. Chambers of 60 m are rare, so make them the exception.
- **Domes and shafts:** vertical dissolution or collapse. Drops of 60 to 120 m are plausible (Pozo Azul and Cenote Angelita go deep).
- **Restrictions:** short pinches where a passage narrows to the width of the object that must pass, often at a bedding-plane or collapse choke.

**Walls and floors**
- **Scallops:** dimples from flowing water. They are small where flow was fast and large where it was slow. Their steep side points upstream (unverified), so they give the passage a direction.
- **Spongework, pockets, flakes and ledges** appear where a soft layer weathers out. Fossil beds show as ribbed or knobbly bands (unverified).
- **Floors:** fine silt that lifts at a fin kick, clay banks with cracks, and breakdown blocks, often with rounded cobbles in old streambeds.

**Speleothems under water.** Stalactites, columns, flowstone and curtains grew while the cave was dry, then flooded. They are thick, white to tan, and sometimes eroded or coated by the water. Use them sparingly, in side chambers.

**Water.** At a halocline, fresh water sits on salt water. It shows as a shimmering blur, and the diver sees double. At Cenote Angelita a milky hydrogen sulphide layer, more than 3 m thick, sits at about 28 to 30 m and looks like a river with trees on its bank ([ScienceAlert](https://www.sciencealert.com/?p=32596), [Atlas Obscura](https://assets.atlasobscura.com/articles/angelita-cenote-underwater-river)). Tannic water is clear but tea-brown, so light dies fast and the world is amber. Clear caves give 30 to 80 m or more of visibility (unverified). Silt-outs happen when fins or bubbles disturb the floor: visibility drops to zero for minutes.

**What a diver's light shows.** A narrow bright core, a faint cone, and almost nothing past 15 to 30 m. The walls are cream or grey. Dark water hides the far side of a room. Close surfaces show texture, and the far walls vanish. Floating particles light up in the beam. Bubbles from the diver collect in the ceiling and drop flakes and silt.

**Real caves with good public media**
1. **Cenote Angelita, Mexico:** the H2S cloud and halocline. See [Vimeo](https://vimeo.com/77922751).
2. **Wakulla Springs and the Woodville Karst Plain, Florida:** big phreatic tunnels, tannic water, mastodon bones, clear springs (unverified, search "Wakulla Springs cave dive").
3. **Pozo Azul, Spain:** a stream-cut vertical system with an outstanding photo and video record (unverified).
4. **Cenote Dos Ojos and the Sac Actun system, Mexico:** speleothem-lined halocline tunnels, columns and flowstone below the water.
5. **Blue Hole of Dahab, Egypt:** a vertical shaft with an arch, deep blue (unverified).
6. **Pines Cave, South Australia:** a [Sketchfab scan](https://sketchfab.com/3d-models/pines-cave-limestone-coast-burrumgule-sa-9afbfe2aa4304384924ac841a83202de) of restrictions and small passages. Check its licence first.
7. **Mount Gambier sinkholes, Australia:** the Shaft and Ewens Ponds, clear water and vertical walls (unverified).

## 2. Generation methods

| Method | Realism | Designer control | Build cost |
|---|---|---|---|
| Geology network synthesis | High | Medium: inlets, outlets, faults | High |
| Implicit or SDF with authored skeleton | High | High | Medium |
| Noise caves | Low to medium | Low | Low |
| Hand-built blockout plus detail | Depends on the artist | Full | High in artist time |

**Geology-based synthesis.**
- **Paris et al. 2021** ([project](https://perso.liris.cnrs.fr/aparis/public_html/projects/paris2021_Karsts.html), [paper](https://diglib.eg.org/handle/10.1111/cgf14420)): an anisotropic shortest-path skeleton driven by inlets, outlets, faults, inception horizons and permeability. The conduit is an SDF construction tree with blending and warping. The artist can author the network and the cross-sections. The [code](https://github.com/aparis69/Karst-Synthesis) is C++ and **MIT** (credit the paper). It is a re-implementation and omits midpoint subdivision and implicit surface generation, so it outputs skeletons as OBJ, not meshes.
- **pyKasso** ([repo](https://github.com/randlab/pyKasso)): Python, 3D, stochastic. **GPL-3.0.** Use it offline as a skeleton generator. Its output is data, but have Quinn decide about GPL in the pipeline. It targets hydrology and ignores looks.
- **KarstNSim** ([paper, CC BY 4.0](https://hal.univ-lorraine.fr/hal-04468018)): graph-based and geology-driven. I found no public code or licence.

**Implicit/SDF.** Skeleton splines with a radius and cross-section profile per point, smooth-unioned, then warped by noise and scalloped. It gives the closest match to the Paris method at small cost. Unreal's Geometry Script and Voxel Plugin both support it.

**Noise caves.** Minecraft 1.18 has three kinds ([Minecraft Wiki](https://minecraft.wiki/w/Caves_and_Cliffs_Update)): cheese (big blobs with pillars), spaghetti (long tunnels) and noodle (thin ones). They look like cave-shaped holes: nothing follows a layer, a fault or a water table. Perlin worms are the same idea. Cheap for games where the player digs, wrong for a drowned cave we want to feel real.

**What shipped games did**
- **Deep Rock Galactic:** a random layout assembled from hand-built room pieces ([GamesRadar](https://gamesradar.com/games/it-took-5-years-but-our-cave-generation-finally-delivered-at-last-the-deep-rock-galactic-devs-have-found-their-holy-grail-a-big-stone-penis-perfectly-crafted-by-the-game-engine), thin on detail). It began as a Minecraft-style block generator.
- **Subnautica:** the terrain is voxel data meshed by Dual Contouring in 160 m batches ([Unknown Worlds](https://unknownworlds.com/news/terrain-data-format)). Whether the caves were hand-authored is unconfirmed.
- **Astroneer:** soft voxel planets the player can deform to the core. Not tuned for cave realism.
- **Enshrouded:** a voxel world built with brushes, negative stamps and splines ([Gameoneer](https://gameoneer.com/enshrouded-developers-showcase-how-they-developed-a-detailed-world/)). The cave method is unverified.
- **No Man's Sky:** not found in this search.

**Designer-draws-the-skeleton hybrid.** Quinn and Colby draw splines with radius and a cross-section tag (tube, keyhole, bedding). A generator adds scallops, ledges, a breakdown floor and speleothems on top, following layer direction. Paris et al. confirm this is a sound split: they let the artist author the network and cross-sections.

## 3. Tools outside the engine (checked 2026-11-15)

- **Voxel Plugin 2:** the vendor's page lists a one-time **$349**, sold on its own site, with one year of updates ([licence page](https://voxelpluginvitedocs.pages.dev/resources/licensing)). Under $100k of game budget needs nothing more. Sharing in one project is allowed. The docs I found say it targets **UE 5.6 and 5.7**. I found **no confirmation of 5.9**. PCG support is announced, and one forum user says it works ([forum](https://forums.unrealengine.com/t/ue5-7-3-and-voxel-pro-1-2/2702927)). Ask the vendor about 5.9 before buying.
- **PCGEx** ([GitHub](https://github.com/PCGEx/PCGExtendedToolkit)): **MIT**, free for commercial use. Graph, Delaunay and pathfinding nodes for PCG. A ZoneGraph add-on is source-available and non-commercial, so skip it.
- **Houdini Indie:** about **$299 a year** (secondary sources say $269 to $299, unverified). Limit: **under $100k gross revenue**, up to 3 Indie and 3 Engine Indie seats ([SideFX](https://www.sidefx.com/faq/question/houdini-engine-indie)). Houdini Engine Indie licences run it in Unreal. Houdini's SDF and VDB nodes (VDB from Polygons, VDB Smooth, Convert VDB) suit cave meshing. Two people need two seats.
- **Gaea and World Machine:** Gaea works as a 2.5D heightfield ([features](https://quadspinner.com/Gaea/Features)), with caves "coming in Gaea 3" per its order page. World Machine's Dragontail Peak adds vector-displacement terrain with overhangs and caves ([CGChannel](https://www.cgchannel.com/2026/07/world-machine-software-ships-world-machine-dragontail-peak/)). Neither builds a navigable cave network. Skip both.
- **Scans:** Megascans is free on Fab since Nov 2026 ([CGChannel](https://www.cgchannel.com/?p=179945)) under the Fab licence. I could not confirm which cave and rock assets are free: browse Fab. On Sketchfab, many cave scans are CC BY, but CC BY-NC (Gruta do Escoural, U-Tut) is out. I found no CC0 cave. Check each model's licence field. A scan gives wall detail only. A real cave scan won't fit our 12 m sub.

## What looks most real in a game, and why
1. A geologic skeleton (tubes, keyholes, bedding planes) with consistent direction gives passages that plausibly connect and that read as cut by water.
2. Hand-sculpted or SDF cross-sections beat noise: a keyhole or a flat-roofed slot is more recognisable than a blob.
3. Surface detail (scallops, ledges, silt, breakdown blocks) must follow the flow and layers, so players read where the water went.
4. Darkness, halocline shimmer, silt-outs and a lamp that fades at 20 m do more for realism than extra triangles.
5. Fewer, bolder landmarks (one cloud layer, one chamber with speleothems) beat constant variety.

## Sources
[Paris 2021 project](https://perso.liris.cnrs.fr/aparis/public_html/projects/paris2021_Karsts.html) · [Paris 2021 paper](https://diglib.eg.org/handle/10.1111/cgf14420) · [Karst-Synthesis (MIT)](https://github.com/aparis69/Karst-Synthesis) · [pyKasso (GPL-3.0)](https://github.com/randlab/pyKasso) · [KarstNSim paper](https://hal.univ-lorraine.fr/hal-04468018) · [Cave types, TRU](https://environmental-geol.pressbooks.tru.ca/chapter/karst-caves-cave-contents-and-subterranean-life/) · [Caving Ireland](https://caving.ie/publications/caving-information-pack/types-of-caves) · [Angelita, Atlas Obscura](https://assets.atlasobscura.com/articles/angelita-cenote-underwater-river) · [Angelita, ScienceAlert](https://www.sciencealert.com/?p=32596) · [Minecraft caves](https://minecraft.wiki/w/Caves_and_Cliffs_Update) · [Subnautica terrain](https://unknownworlds.com/news/terrain-data-format) · [Enshrouded](https://gameoneer.com/enshrouded-developers-showcase-how-they-developed-a-detailed-world/) · [Voxel Plugin licence](https://voxelpluginvitedocs.pages.dev/resources/licensing) · [PCGEx](https://github.com/PCGEx/PCGExtendedToolkit) · [SideFX Engine Indie](https://www.sidefx.com/faq/question/houdini-engine-indie) · [Gaea](https://quadspinner.com/Gaea/Features) · [World Machine 2026](https://www.cgchannel.com/2026/07/world-machine-software-ships-world-machine-dragontail-peak/) · [Megascans free](https://www.cgchannel.com/?p=179945) · [Sketchfab Pines Cave](https://sketchfab.com/3d-models/pines-cave-limestone-coast-burrumgule-sa-9afbfe2aa4304384924ac841a83202de)
