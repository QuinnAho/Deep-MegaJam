# DEEP

*Cheapest submarine on the market. Loudest opinion of you.*

<!-- Draft for itch.io, refreshed Nov 15 at about 3:05 PM for the scope change (D-039 to D-046), and checked against polish/integration f0b7fd0. Body (from "The game" to the end) is under 500 words. Every TODO(Quinn) is a fact the repo cannot confirm. TODO(Quinn): the tagline predates D-040, when the Captain began to say little; keep it or change it. -->

## The game

You are a salvage diver on a tether. Your submarine goes down a winding cave shaft, and you drive it by sonar. Every stop is a cavern. Hit the walls and the hull takes damage. Breach it and the diver aboard dies.

At each stop, drop through the hatch, swim out, grab salvage, and get back up the line. Past 10 kg you sink. At 15 kg you cannot swim up, and the loud winch is the only way home.

A sea serpent hears loud. It comes in toward your tether, and a bite starts a five-second alarm. Turn your lamp on it and it lets go. Too slow, and it bites through the line.

The Captain is a voice on the radio, and he works for the company. He says little. He takes 40 percent of every haul. When his patience runs out he cuts your tether. Dying is billed as a fee. The next diver is already on the way.

**The theme.** Every haul gets edited by the Captain's cut, and the cut tether is the final edit. TODO(Quinn): confirm this sentence, and that the theme's name appears only here (D-025).

## Controls

| Key | Action |
|---|---|
| W A S D | Swim or walk. At the helm: steer the submarine |
| Mouse | Look |
| Space / C | Up / down. At the helm: hold C to dive, Space to rise |
| Shift | Sprint (noisy) |
| E | Grab; use console and helm |
| G | Drop what you carry |
| Left click | Harpoon, one per dive |
| V | Sonar ping in the water, every 3 s |
| R (hold) | Winch home |
| F | Lamp, once bought |
| Q (hold) | Ask to be cut loose |
| Esc | Menu |

## In the build

- Title, settings, credits, pause, quit, and a win screen with "dive again".
- A walkable submarine with a hatch, a settlement console and a helm.
- Three stops and a relic, to 600 m. A flashlight and a hull upgrade are bought at settlement.
- Camera shake on hits, bites and the cut. 74 lines for the Captain. TODO(Quinn): confirm no voice or music files ship; the sound is made in code.

## How it was made

Quinn, Colby and Claude agents. The people chose the idea and every call on style and scope. The agents wrote the C++, tests, playtests, the Captain's lines and the dev log. One orchestrating agent wrote no game code. TODO(Quinn): confirm the start time to quote and Colby's role. The commit count and the final test result go in at the 6 PM package.

Dev log: TODO(Quinn): link.

## Credits

- Unreal Engine 5.9 (MegaJam Preview), Epic Games.
- All sound generated in code.
- Dragon Cave, Denys Rutkovskyi, Fab personal licence (the relic). TODO(Quinn): confirm seller, licence, and whether it still supplies the serpent's head after D-046.
- Megascans: TODO(Quinn): list only what shipped.

Tools: Unreal Engine 5.9, Unreal MCP, Claude Code. TODO(Quinn): add the others (Obsidian, Google Docs).

## To run

Windows 10 or 11, DirectX 12. TODO(Quinn): minimum GPU and RAM, and the team name.
