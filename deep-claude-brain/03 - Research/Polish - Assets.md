---
updated: 2026-11-15
verified: 2026-11-15 about 01:38 local. Queried live through my own editor's Fab MCP toolset (port 8013): SearchFabListings with free-only filter, about 45 keyword searches; GetFabListingDetails for 33 listings (22 free, 11 owned); the three library JSON files in deep\Saved\Harness. No sign-in was needed. Nothing installed or acquired. Thumbnails were not viewed (the tools return links only), so style notes come from descriptions, tags and titles. Machine-readable list with thumbnails: deep\Saved\Harness\polish\assets.json (61 picks, git-ignored).
---

# Polish - Assets (scout list for Quinn)

Listing links are `https://www.fab.com/listings/<listingId>`, the pattern each search result returned as its `fabUrl`. All Fab-search engine versions are the newest listed; 5.8 or older opens fine in 5.9. Code plugins need 5.9 binaries and are flagged.
Free listings often have a free Personal licence and a paid Professional one; a jam entry fits Personal. CC-BY needs a credit line at submission.

## Per need (top pick first)

**1. Submarine interior**
- Top, owned: Sci-Fi Creatures Research Lab (realistic sci-fi, one modular room, display and hologram shaders). Hide the capsules, add rust.
- Alt, free: Modular Underwater Exploration Base and Drone (MSP Assets, glb, worn industrial, Personal free). Best "cheap worn sub" mood; needs import.
- Alt, free: Modular Ship Interior Environment (andi475, UE 5.4): pipes, walls, floors.
- Alt: Free Industrial Pipes Sample (hatch-area pipes), Rudder Angle Indicators (helm gauges), Display Terminal (console, glb, CC-BY), owned Sci-Fi Bio Laboratory and Industrial Infrastructure.
- No: Deep Sea Hunter's Station (cartoony low-poly, 140 props incl. harpoon launchers, would force a whole-game stylised look). DieselPunk by KitBash3D is the right mood but cinematic high-poly, too heavy.

**2. Sea floor, three depths, wrecks**
- Top, owned: Megascans rocks, a handful: Cave African Rock Fallen Stone Large, Coast English Rock Shelf Large, Tundra Volcanic Boulder Large and Formation (dark ones suit 300 m and 500 m).
- Top, free: Thai Beach Corals pack (Megascans, 100 m floor). Free extras: Starfish, seaweed and old shipwreck hull scans (these two are surface scans, not meshes).
- Alt, owned: Atlantis Ruins (500 m relic site; semi-stylised), Ultimate Ship Package (pirate wrecks), Deep Elder Caves, Corals (glowing low-poly).
- No: raw OBJ wreck scans (barge, trawler): very high tris.
- Megascans in the library are rocks and cave ground. There is no owned coral, kelp or nautical scan; harbour crate, rope, boat and lantern are medieval.

**3. Sea serpent**
- Top, free: FREE Eldritch horror Inspired Creature (Axinovium, fbx, about 47k tris, worm-like tentacle body, CC-BY). Description says nothing on rig or animation. Plan to drive it with a spline anyway.
- Alt: Animated Frilled Shark (Anees Animates, fbx, swim animation, eel-shaped; detail not read). Weedy Seadragon sculpt (about 2.5M tris, no rig). Animated cobra (Blender only, CC-BY).
- No: Kraken by Protofactor (Fab says the listing is gone), Stylized Fantasy Creatures Bundle (stylised, contents unchecked).
- Nothing gives a ready, rigged, swimming sea serpent.

**4. Props, relic, harpoon**
- Top, free: Speargun with Arms (Furkan Dogru, CC-BY, 6.5k-tri gun, 13.7k arms, idle/shoot/reload/swim animations). Covers harpoon gun and first-person arms in one.
- Top, free: Drowned Treasure Chest (CC-BY) for salvage or a relic casket.
- Alt, owned: Megascans harbour crate, rope, lantern, hook; Junkyard (scrap metal); Clay Pots Pack (amphorae); Ancient Pillars Nanite (free) for the relic site.
- Harpoon bolt: none found; a stretched cylinder will do.

**5. Diving gear**
- Only a diving tank prop (obj, CC-BY) and a scanned diver figure. No suit, helmet or rigged diver. Skip it; the player is first person.

**6. Effects**
- Top, free: Simple Bubbles (Niagara, iridescent, retune) and Fog Area (noise fog volumes for murk and shafts).
- Alt: Vefects Bubbles VFX (Personal free). Caustics by PRISMIX is tagged Plugin: needs 5.9 binaries, probably none. Owned: Water Materials (CC-BY, 2018), Realistic Starter VFX Vol 2 (water, smoke, silt puffs), Rocket Thruster.
- No ready marine-snow pack found; Niagara Examples Pack is the base to build one.

**7. UI icons and frames**
- Free Game Icons Sampler (24, only 12 sci-fi). PC Keyboard and Mouse Icon Pack (CC-BY). Nothing submarine-themed found. The HUD is code-built, so little is lost.

## Style verdict

One look to commit to: dark, worn, photoreal-leaning (realistic PBR, mid-poly, heavy colour grading to hide scan colours).
Coherent set (7):
1. Megascans rocks (owned) plus free Thai corals and starfish.
2. Sci-Fi Creatures Research Lab (owned) for the room.
3. Modular Underwater Exploration Base (free) for worn hull detail.
4. Modular Ship Interior and Free Industrial Pipes (free).
5. Eldritch horror creature (free) for the serpent.
6. Speargun with Arms (free, low-poly but PBR, will read fine in the dark) and Drowned Treasure Chest.
7. Simple Bubbles and Fog Area (free).
Clash with it: Deep Sea Hunter's Station (cartoon), Corals and Atlantis Ruins and Ultimate Ship Package (stylised or pirate), Stylized Fantasy Creatures, DieselPunk (right mood, wrong weight). If Quinn prefers a stylised game, flip it: Deep Sea Hunter's Station, Corals, Atlantis, Fantasy Creatures become the set, but then the rocks and the serpent need replacing.

## Quinn installs first (in this order)

1. Sci-Fi Creatures Research Lab (owned): the room, console shaders, hologram screen. Unlocks the interior pass.
2. Megascans rocks, six pieces only: the seabed at all three depths.
3. Eldritch horror creature (fbx): replaces the spheres, or a mesh to skin to the spline.
4. Speargun with Arms: the harpoon gun and arms.
5. Simple Bubbles, Fog Area: the underwater look cheaply.
6. Modular Underwater Exploration Base: worn hull and hatch pieces.
7. Thai Beach Corals and Drowned Treasure Chest: seabed and salvage.
Each free listing installs without being added to the library first per the tool notes. Keep the build under the 1 GB cap: cook only what a level uses.

## Gaps nothing fills

- A rigged, animated, eel-shaped sea serpent. Closest: the eldritch mesh (rig unknown) or the frilled shark. A spline-driven body stays necessary.
- A helm wheel, hatch and console made for a sub; only parts exist.
- Marine snow, ready-made light shafts and caustics that work in 5.9.
- Diving suit or helmet, harpoon bolt.
- Submarine-themed UI.
- What Fab could not tell me: download sizes, polygon counts for most packs, 5.9 binaries for plugins, whether the eel mesh has a rig, and what the images look like.
