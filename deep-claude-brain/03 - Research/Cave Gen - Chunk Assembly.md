---
updated: 2026-11-16 01:15 EST
---
# Cave Gen - Chunk Assembly

Research for D-055: caves are a library of baked chunks, strung together in a new order every dive. How Lethal Company-style interiors are built, how Unreal 5.9 can assemble baked chunks at runtime, how to hide the joints, and what to prototype. Context: [[Cave Pipeline]], [[Cave Gen - Engine Tools]]. Public sources only; no datamining.

Source quality: a lot of what is written about Lethal Company's generator is community guesswork. Where a source is unofficial or unconfirmed it is marked.

## 1. How Lethal Company builds its interiors

### What is confirmed and what is not
- **The generator is not confirmed.** No source I found has the developer naming DunGen. A search for official statements found none, and a Steam thread asking how the facility is generated got no developer answer ([Steam thread](https://steamcommunity.com/app/1966720/discussions/0/4140564923177621836)). The Unity asset DunGen is the usual guess, and modders author interiors with DunGen-style flows ([Badham SCP interior changelog](https://thunderstore.io/c/lethal-company/p/Badham_Mods/SCPFoundationDungeon/changelog)). Treat "Lethal Company uses DunGen" as reported, not proven.
- **The model it follows is documented.** DunGen's own docs describe a tile-and-doorway generator, below. That is the model to learn from whether or not Lethal Company uses it.

### The DunGen model
| Concept | What it is | Source |
|---|---|---|
| Tile | One room or corridor prefab; the building block. Tiles are grouped into tile sets, and DunGen picks from a set at random | [Core concepts](https://dungen-docs.aegongames.com/2.18/core-concepts/) |
| Doorway | An attachment point on a tile. DunGen aligns doorways of adjacent tiles to connect them | [Archetypes and doorways search](https://dungen-docs.aegongames.com/2.18/core-concepts/doorways/) |
| Socket | Two doorways connect only when they share the same socket asset; use different sockets for size or style | [Doorways](https://dungen-docs.aegongames.com/2.18/core-concepts/doorways/) |
| Connector, blocker | A connector prefab spawns when two doorways join; a blocker (wall, closed door) spawns on a doorway left unused | [Doorways](https://dungen-docs.aegongames.com/2.18/core-concepts/doorways/) |
| Dungeon flow | The master graph that controls structure and progression | [Core concepts](https://dungen-docs.aegongames.com/2.18/core-concepts/) |
| Archetype | A type of section; picks tile sets and sets branch count and depth | [Archetypes](https://dungen-docs.aegongames.com/2.18/core-concepts/archetypes/) |
| Main path | The route from start tile to goal tile, built one tile at a time through the flow; it backtracks or restarts if it breaks a constraint | [Core concepts](https://dungen-docs.aegongames.com/2.18/core-concepts/) |
| Branch | Optional routes added after the main path is complete; if one fails to grow, the last tile placed is its end | [Core concepts](https://dungen-docs.aegongames.com/2.18/core-concepts/) |
| Branch cap | Special tiles reserved for the last tile of a branch, so dead ends end in something that looks like an end | [Archetypes](https://dungen-docs.aegongames.com/2.18/core-concepts/archetypes/) |
| Doorway placement rule | A doorway sits on the edge of the tile's bounding box with its axis pointing outward, and not on a concave part of the outline | [Doorways](https://dungen-docs.aegongames.com/2.18/core-concepts/doorways/) |

The same advice shows up in an Unreal context: make every door frame identical so pieces line up, put the sockets at the rim of the frame, and drive everything from one integer seed ([Unreal forum reply](https://forums.unrealengine.com/t/how-does-lethal-company-generate-maps-ue5-0-3-blueprint-help/2531400)). That is advice for the poster's own project, not a description of Lethal Company.

### What makes those interiors play well
These figures come from one unofficial guide that cites no data ([dungeonpath guide](https://dungeonpath.com/posts/lethal-company/interior-layouts-guide/)). Use them as design hints, not facts.

| Aspect | Claim | Use for DEEP |
|---|---|---|
| Length | A small facility has roughly 8 to 12 rooms, a large one 20 to 30 | A dive is a handful of long pieces, not dozens |
| Goal | The deepest room is the one farthest by path from the entrance | The wreck chamber is the goal, and always last |
| Dead ends | About 30% of branches end in a small room with no exit; they hold the most scrap because fewer players look there | Side pockets off a passage hold the best salvage |
| Loops | About 10% of layouts have a loop | Loops are rare; mostly tree-shaped. A sub cannot turn round easily, so keep loops out |
| Scrap | Dense in particular room types and dead ends | Salvage spawns in marked slots per chunk type |
| Hazards | Fixed kinds per room type, on ceilings, walls, pipes | Hazards (silt, tight squeezes, the serpent) are tied to chunk type |

- **One seed for everyone.** Indirect evidence only: a forum post says levels are generated per client from a seed to save bandwidth ([Steam post](https://steamcommunity.com/app/1966720/discussions/0/4034728215968143475)); an interior-loader mod says only the host picks the dungeon flow and size and sends it to the other clients ([LethalLevelLoader](https://thunderstore.io/c/lethal-company/p/IAmBatby/LethalLevelLoader/v/1.1.1/)); and an interior mod warns that a seed mismatch gives different doors, layout and scrap ([Random Interiors fix](https://thunderstore.io/c/lethal-company/p/thedemonmods/Random_Interiors_Normal_Seeds/)). The pattern is clear: host sends a seed and a few parameters, every client builds the same layout itself.

## 2. Ways to assemble baked chunks in UE 5.9

Facts below are from Epic's documentation and forum reports. I could not read the `LoadLevelInstance` API page (the fetch returned only a table of contents), so its parameters need to be checked in the engine headers before the spike.

### Option A: Level Instances loaded at runtime
- **What:** `ULevelStreamingDynamic::LoadLevelInstance` creates a dynamic streaming level for a level not known at build time, at a transform you give ([forum thread](https://forums.unrealengine.com/t/issues-loading-sub-levels-and-lighting-scenarios-in-dynamically-streamed-levels-ulevelstreamingdynamic/2717153)). Each chunk is a small level holding the baked mesh, collision and dressing.
- **Level Instance actors spawned at runtime** are not clearly supported. A community reply says to call Load Level Instance instead of spawning the actor ([forum](https://forums.unrealengine.com/t/dynamically-spawned-level-instance-actors-do-not-have-their-sub-actors-world-partition-stream-in-pie-starting-level/2599306)).
- **A bug to know about:** Epic's tracker has UE-314556, spatially loaded actors of standalone level instances not loading even when the instance is force loaded; it lists 5.5 and 5.6 and a target fix of 5.8 ([issue](https://issues.unrealengine.com/issue/UE-314556)). Re-check against 5.9.
- **Duplicates:** loading duplicates of a level instance that contains a Packed Level Actor can assert ([forum](https://forums.unrealengine.com/t/assert-when-trying-to-load-duplicates-of-a-level-instance-containing-a-packed-level-actor/2835586)). Reuse of a chunk is the whole point, so this needs a test.

### Option B: Packed Level Actors
- A Packed Level Actor combines static meshes into one optimised Blueprint actor, suited to static scenery ([Epic docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/level-instancing-in-unreal-engine)). Good for the rock; it has no gameplay actors. The docs I read do not say how collision is handled, so test it.
- It could sit inside a Level Instance, or be spawned straight into the game world as an actor.

### Option C: World Partition
- Level Instances in a World Partition world use Embedded Mode (default) or Level Streaming Mode. In Embedded Mode, one-file-per-actor instances are discarded at runtime and their actors are added to the grid, so the instance exists only in the editor ([Epic docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/level-instancing-in-unreal-engine)). That is built for placement in the editor, not for a layout chosen at runtime.
- Level Streaming Mode loads the level with its owning cell, and Epic discourages many levels in that mode (same page).
- Outside a World Partition main world, Level Instances get no automatic streaming management (same page).
- **Read:** World Partition is the wrong host for runtime assembly. Make the dive a plain persistent level and load chunks into it.

### Option D: ProceduralDungeon plugin (BenPyton)
| Question | Answer | Source |
|---|---|---|
| Licence | Fab EULA for copies from Fab; CeCILL-C for copies from anywhere else | [GitHub README](https://github.com/BenPyton/ProceduralDungeon) |
| Versions | Pre-built releases list 4.27 and 5.4 to 5.8. The three latest (v3.9.0 to v3.9.4) include 5.6, 5.7 and 5.8. **5.9 is not listed anywhere.** | [Releases](https://github.com/BenPyton/ProceduralDungeon/releases) |
| Model | Rooms are hand-made Unreal levels, instanced into the world, with bounds and doors; generation logic in Blueprint or C++ (`ChooseNextRoomData`) | [README](https://github.com/BenPyton/ProceduralDungeon) |
| 3D | Rooms are placed in three dimensions | [Guide home](https://benpyton.github.io/ProceduralDungeon/guides/Home) |
| Multiplayer | README says "ready for multiplayer" with push model and subobject lists; room culling is client-side | [README](https://github.com/BenPyton/ProceduralDungeon) |
| Seed | Fixed, incrementing or random per generation | [README](https://github.com/BenPyton/ProceduralDungeon) |
| Loading time | Room load time grows with the number and complexity of rooms | [Guide home](https://benpyton.github.io/ProceduralDungeon/guides/Home) |

- No source says 5.9. CeCILL-C is a weak copyleft licence (modifications to the plugin stay open; linking from a closed game is allowed); read the licence before shipping.
- The plugin has no overlap or seed detail on the pages I could read, and the multiplayer guide page returned empty. Check both in the source.
- It solves rooms-and-doors, which is our model, but it is a dependency on a plugin that may lag the engine. The core idea is small enough to write ourselves.

### Option E: Our own assembler (C++ or Blueprint)
- Spawn each chunk as an actor (static mesh with baked collision) at a computed transform, and place gameplay actors from per-chunk sockets. No levels, so no streaming bugs; the cost is that the dressing and spawn points must live in the actor.
- Collision is ready the moment the actor is spawned and the mesh's collision is cooked, so the assembler can build the whole dive ahead, or a few chunks ahead of the sub.
- The overlap check is ours: each chunk has an oriented box, and the assembler rejects a placement that intersects one already placed.
- Anything credible else: Epic's own tools do not ship a runtime dungeon assembler that I found. DunGen is Unity-only.

### Comparison
| | Collision ready before the sub | Lead time | Descending layout | Overlap checks | Co-op determinism |
|---|---|---|---|---|---|
| A. Level Instance | After load completes; must wait on the streaming-complete event | Per-level load, a hitch risk | Any transform | Ours | Same seed, same order; each client loads |
| B. Packed Level Actor | Static mesh collision, ready on spawn (to verify) | Spawn cost only | Any transform | Ours | Same |
| C. World Partition | Not designed for it | n/a | n/a | n/a | n/a |
| D. ProceduralDungeon | After room level loads; plugin handles it | Load time grows with room size | 3D | Built in (unverified) | Seeded; replicated multiplayer claimed |
| E. Own assembler | On spawn, if collision is baked in the mesh | Spawn cost only; can spawn 2 to 3 chunks ahead | Any transform | Ours, a box test | One replicated int, one RNG, same order everywhere |

Determinism in all cases: use one `FRandomStream` seeded from one replicated integer; consume it only for assembly decisions, in a fixed order, never from gameplay. The host picks the seed; every client builds the layout locally.

## 3. Seams

A cave joint can read as a joint, so design the join before the chunks.

| Technique | How | Where it comes from |
|---|---|---|
| One fixed joint profile | Every chunk ends in the same cross-section, same size, same orientation, so any two ends mate. Same idea as identical door frames | [Unreal forum reply](https://forums.unrealengine.com/t/how-does-lethal-company-generate-maps-ue5-0-3-blueprint-help/2531400), and DunGen sockets ([Doorways](https://dungen-docs.aegongames.com/2.18/core-concepts/doorways/)) |
| Joint at a restriction | Put the joint where the passage narrows, so the eye reads a squeeze, not a cut; the narrow part also hides misalignment | Our own design, tied to real caves ([[Cave Gen - Real Caves and Methods]]) |
| Shared vertices and matching texture | Weld the end rings and let the material align across the join (a devlog found a visible cover strip looked bad, and welding fixed it) | [itch.io devlog](https://itch.io/post/908071) |
| Blend strip | A mesh over the join with a material that fades in over the seam | [itch.io post](https://itch.io/post/1566525) |
| Volume at the join | Zero-thickness pieces leak light and show gaps; give end walls thickness and overlap them | [Unreal forum](https://forums.unrealengine.com/t/modular-pieces-have-a-see-through-seam/430242/3) |
| Shared source | In terrain chunking, all chunks read the same data so edge values match | [Cinevva blog](https://app.cinevva.com/blog/2026-04-13-open-world-browser-part-13-terrain-sculpting) |
| Rocks over the joint | A placed boulder or breakdown block, one per joint, hides the line and adds weight | Our own design |

I found no shipped game that documents how it hides cave joints. The sources above are general modular-kit and terrain examples; the combination for caves is our proposal and the spike tests it.

## 4. Recommendation for DEEP

1. **Chunk types:** descent passages (the way between stops, mostly vertical, with bends); a chamber at each stop (where the sub parks and the diver exits); a wreck chamber at the bottom. Side pockets off passages, as dead-end caps, hold salvage.
   - **Replaced by D-058 (Nov 16):** the descent passage is dropped. The sub sinks in caverns, and tunnels stay near level.
2. **One joint profile:** a round tube section of 9 m diameter at the joint, set inside a restriction. BT-01 at x2 is 12.3 x 3.3 x 5.0 m; squeezes are 7 x 7 m, which leaves about 1 m above and below the hull and 1.85 m to each side. Open passages are wider; only the joint ends are fixed. If Quinn changes the scale, only this number changes.
3. **Joints at restrictions:** every chunk begins and ends inside a squeeze, and a boulder or breakdown block is placed over each seam.
4. **Spawn points:** every chunk carries named sockets for salvage and serpent lairs. Salvage goes in side pockets and the wreck chamber; serpent spawn and patrol sockets are in chambers and in long descents. The assembler places them with the same seeded stream.
5. **Library size:** my estimate, not sourced: about 8 to 10 passages, 4 chambers and 2 wreck chambers before a regular player sees repeats. A dive of 6 to 8 chunks drawn from 14 gives well over a thousand orderings, and each chunk can be flipped or rotated about the descent axis for variety. Check by playing, not by arithmetic.
6. **Prototype first:** Option E, our own assembler placing baked mesh actors. It avoids the streaming bugs and the Packed-Level-Actor duplicate assert, keeps collision ready on spawn, and fits a vertical layout with one replicated seed. ProceduralDungeon has no 5.9 build and adds a plugin dependency for something small.
7. **Spike:** two 30 m chunks joined both ways in `L_CaveSpike`, then a ten-chunk string. Check the seam visually, hull collision at the joint and spawn time per chunk.
8. **If E disappoints:** try a Level Instance wrapper (Option A) around the same chunks only if dressing actors need a level, and test duplicates and load hitches first.

## Open
- Confirm `LoadLevelInstance`'s parameters and completion callback from the 5.9 headers.
- Whether the chunk collision from Geometry Script bakes as complex-as-simple at 30 m scale.
- Whether UE-314556 is fixed in 5.9.
