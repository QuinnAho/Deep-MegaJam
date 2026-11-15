---
updated: 2026-11-14
verified: 2026-11-14 about 23:55 local. Queried live through the editor's Fab MCP toolset (SearchFabMyLibrary, all pages; GetFabListingDetails for 14 listings), plus a disk listing of the Epic VaultCache and Engine\Plugins\Marketplace for UE 5.8 and 5.9. Nothing installed. Raw results are in deep\Saved\Harness\fab-library-*.json (git-ignored).
---

# Asset Library Shortlist (DEEP)

**Owned: 1,694 items.** 1,486 are Quixel (1,399 Megascans 3D models, 47 Megascans materials, 40 Megaplants), and 208 are other sellers: 103 3D model packs, 49 tools/code plugins, 18 tutorials/examples, 15 game systems, 11 VFX, 6 materials, 4 game templates, 2 animation. Audio: 0. HDRI, decal, brush, UI-type and sprite listings: 0.

What the Fab tools do NOT report: download size, polygon counts for most packs, and per-version binary status for plugins. Engine versions below are what Fab lists (newest = 5.8, except Surface Forge at 5.9). Nothing he owns lists 5.9 for a pack we want. Content packs made for 5.8 or earlier open fine in 5.9. **Code plugins need binaries built for 5.9; most will not have them, so treat each as "needs compiling for 5.9, may not work".**

Size reality: the jam upload limit is 1 GB and the empty game is already 707 MB. Package only what a level uses (cook only referenced assets), and delete unused packs from /Game before packaging. Already downloaded in the vault: Asian Canal (2.1 GB), Dogs Big pack (1.2 GB), Ultra Dynamic Sky (0.5 GB), and one folder "Ultimate5bd978ec..." at 6.5 GB (likely a big pack; cooked size will be far smaller than the download).

## Submarine and interior

| Title | Use | Engine | Risk |
|---|---|---|---|
| Sci-Fi Creatures Research Lab (Personal, Professional) | One game-ready modular room, two metal skins, hologram and display-screen shaders, glass-liquid shader: the best single-room match for the sub interior and the console screen. | up to 5.8 | Room has creature capsules we would hide or delete. Mid-poly. Size not reported. |
| Sci-Fi Bio Laboratory | Modular sci-fi interior kit, lots of props (storage, desks, equipment) to dress the sub. | up to 5.8 | Plants and hydroponics are off theme. Size not reported. |
| Modular Sci-Fi Treatment Station Environment | Industrial modular walls, floors and pipes for a cramped sub hull. | up to 5.5 | Older, rating 4.2 from 13. Not checked beyond description. |
| Industrial Infrastructure | Pipes, tanks, valves for the console and engine corners. | up to 5.8 | Not checked in detail. |

## Sea floor and environment

| Title | Use | Engine | Risk |
|---|---|---|---|
| Corals (Personal, Professional) | About 79 low-poly coral models with glow textures made for a night sea bottom: dress the floor. | up to 5.8 | Low tri counts (370 to 3,300). Old pack, rating 4.68. |
| Quixel Megascans (1,399 models) | Rocks, cliffs, ledges, columns (Cave and Desert sets), rusty metal props for rubble and a rocky sea floor. | glTF/FBX only | Installs via Interchange as glTF, not ready-made Unreal assets. Heavy textures; use a few. Personal licence. |
| Atlantis Ruins / 37 Assets | Sunken ruin pieces: arches, columns, statues for the relic site. | up to 5.7 | Size not reported. |
| Ultimate Ship Package / 110+ Assets | Ship parts (galleon, anchors, cannon) for wrecks to salvage from. Pirate style, not modern. | up to 5.7 | Seller note says the marketplace shows a false "unsupported version" error; import from the Launcher. Possibly the 6.5 GB vault item (unconfirmed). |
| Cave Ruins / 47 Assets and Deep Elder Caves | Walls, rocks, arches for a trench or cave to the relic. | 5.7 and 5.8 | Fantasy RPG look. Pick one. |
| Underground Tunnel | A tunnel/corridor shell for a cave route. | up to 5.8 | Not checked. |

## Creatures

Nothing for a sea serpent. No serpent, eel, snake or sea monster is in the library. Candidates only if we fake it:
| Title | Use | Engine | Risk |
|---|---|---|---|
| Stylized Fantasy Creatures Bundle | Might hold a long body to repurpose; contents not checked. | up to 5.8 | Stylized, probably clashes with realistic rocks. Rigging and swim animation unknown. |

Plan the serpent as a procedural spline mesh with the engine shapes already in the game.

## Props (salvage, relic, console, helm, hatch)

| Title | Use | Engine | Risk |
|---|---|---|---|
| Atlantis Ruins (see above) | Relic: statues, urns. | 5.7 | As above. |
| Clay Pots Pack | Amphora-style salvage and a relic base. | up to 5.8 | Not checked. |
| Radical Robots - Mike the Customizable Robot | Possible body for the Captain on the console screen or as a bobblehead. | up to 5.8 | Not checked; style unknown. |
| Quixel rusty metal props (hooks, hinges, lamps, pots) | Small salvage junk. | glTF | Medieval and bazaar items, not nautical. |

No helm wheel, hatch or console prop found by title. The Sci-Fi room packs above are the best bet for a console and hatch.

## Effects (bubbles, particles, water)

| Title | Use | Engine | Risk |
|---|---|---|---|
| Water Materials (licence CC-BY) | 12 water materials (ocean, river, pool): base for water surface and underwater look. | up to 5.8 | **CC-BY: must credit the author at submission.** Made in 2018 style; test in 5.9 Substrate/Lumen. |
| Rocket Thruster Exhaust FX | 12 Niagara thruster effects, scalable: use for sub thruster and as a base for bubble streams. | up to 5.8 | Not bubbles; will need retuning. |
| Realistic Starter VFX Pack Vol 2 and Big Explosions | Impact and explosion Niagara for harpoon hits and sparks. | up to 5.8 | Not checked; explosions are large for a quiet game. |
| Camera Shakes Free Pack | Blueprint camera shakes: serpent hit, sub rumble. | up to 5.5 | Tiny. Low risk. |
| Ultra Dynamic Sky | Sky and weather. | up to 5.8 | Not useful underwater (0.5 GB vault). Skip unless the surface scene matters. |

## Sound and music

Nothing. He owns no audio listings. Pirate music and ambience packs are mentioned by the sellers as separate products but are not owned. We need free or recorded sound from elsewhere. (Audio Exporter is a tool, not sound.)

## UI

Nothing worth installing. UI Material Lab and the UI plugins below do not fit a one-day UI. Build the HUD with UMG.

## Code plugins (all: needs compiling for 5.9, may not work)

| Title | Use | Engine | Risk |
|---|---|---|---|
| Screen Fade | Fade to black on death and return. | up to 5.8 | Trivial to rewrite in Blueprint. Needs compiling for 5.9, may not work. |
| Look At Actor | Captain or turret facing. | up to 5.8 | Needs compiling for 5.9, may not work. |
| Surface Footstep System | Footsteps in the sub. | up to 5.8 | No footstep sounds owned. Needs compiling for 5.9, may not work. |
| Expose Niagara Variables to Blueprint & C++ | Drive effect parameters from code. | up to 5.8 | Needs compiling for 5.9, may not work. |

## Top five to install first

1. **Sci-Fi Creatures Research Lab**: a finished single modular room with hologram and screen shaders matches "one room with a console" and replaces grey boxes fastest.
2. **Corals**: cheap, low-poly, glowing sea-floor dressing; small footprint.
3. **Water Materials**: gives the underwater look; remember the CC-BY credit.
4. **Atlantis Ruins / 37 Assets**: relic and ruin pieces for the bottom.
5. **Quixel Megascans rocks (a handful of Cave and Desert rocks)**: sea-floor terrain; take only 5 to 10 for size.

## Probably not

- Any sea-serpent (nothing owned). Fantasy creature, dragon and golem packs: wrong style and rigging time.
- Ultra Dynamic Sky, Cesium, MetaHumans and the other 20 GB-class items: size and time.
- OpenAI, HttpGPT and AI-NPC dialogue plugins for the Captain: runtime network, API keys and 5.9 compile risk; script the lines instead.
- Lyra, City Sample, Electric Dreams, Cropout and other sample projects: far too big.
- Multiplayer, RPG, combat and crafting systems and Niagara/dragon IK plugins: not a fit for one day.
- Ready-made ships (Ultimate Ship Package): only for wrecks, and pirate style.

Reminder: add every asset we install, with its licence (note CC-BY for Water Materials), to the list in the "Submission Checklist" note.
