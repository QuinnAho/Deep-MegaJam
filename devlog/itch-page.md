# DEEP

*Cheapest submarine on the market. Loudest opinion of you.*

<!-- Draft for itch.io, refreshed Nov 15 at about 6:05 PM after the re-scope (helm, calm HUD, air line, taunting serpent, generated cave, lamp, dressed room), from the lane notes; the orchestrator checked the keys, the 40 percent cut, Q, T and the lamp driving the serpent off against integration `9ac970e`. Body (from "The game" to the end) is under 500 words. Every TODO(Quinn) is a fact the repo cannot confirm. TODO(Quinn): the tagline predates the Captain going quiet; keep it or change it. -->

## The game

You are a salvage diver on a tether. You pilot a cheap submarine down a winding, generated cave and steer by sonar. Every stop is a cavern. Hit the rock hard and the hull takes damage. Breach it and the diver aboard dies.

At each stop, drop through the hatch, swim out, grab salvage, and get back up the line. Past 10 kg you sink. At 15 kg you cannot swim up, and the loud winch is the only way home.

The tether is your air line. Clipped on, your air never runs out. Press T to unclip and you have 30 seconds of bailout air to reach the hatch.

A sea serpent hears loud. It hears your sonar ping, your harpoon, hull knocks and a hard-driven sub. You can hear it too. It circles at the edge of your lamp and taunts you twice, then it strikes. Turn your lamp on it and it backs off. Too slow, and it bites through the line.

The Captain works for the company. He never says a word, and he takes 40 percent of every haul. Dying is billed as a fee. The next diver is already on the way.

**The theme.** Every haul gets edited by the Captain's cut, and the cut tether is the final edit. TODO(Quinn): confirm this sentence, and that the theme's name appears only here (D-025).

## Controls

On foot, aboard:

| Key | Action |
|---|---|
| W A S D | Walk |
| Mouse | Look |
| E | Use (hatch, console, helm) |
| Esc | Menu |

At the helm:

| Key | Action |
|---|---|
| W / S | Step the throttle up or down, six notches |
| A / D | Rudder |
| Hold C / hold Space | Flood / blow the ballast lever. It stays where you leave it |
| X | All stop |
| E | Stand up |

In the water:

| Key | Action |
|---|---|
| W A S D | Swim |
| Space / C | Up / down |
| Shift | Sprint (noisy) |
| E / G | Grab / drop |
| Left click | Harpoon, one per dive |
| V | Sonar ping (the serpent hears it) |
| R (hold) | Winch home |
| F | Lamp, once bought |
| T | Unclip from the air line |
| Q (hold) | Ask to be cut loose |

## In the build

- Title, settings, credits, pause, quit, and a win screen with "dive again".
- A submarine dressed as a working boat, with a hatch, a settlement console and a helm with throttle and ballast levers.
- Three stops and a relic, to 600 m. A flashlight and a hull upgrade are bought at settlement.
- A calm screen. Text shows only while you need it.
- Camera shake on hits and bites. All sound is made in code; no voice or music files ship.

## How it was made

Quinn, Colby and Claude agents. The people chose the idea and every call on style and scope. The agents wrote the C++, tests, playtests and the dev log. One orchestrating agent wrote no game code. TODO(Quinn): confirm the start time to quote and Colby's role. The commit count and the final test result go in at the package.

Dev log: TODO(Quinn): link.

## Credits

- Unreal Engine 5.9 (MegaJam Preview), Epic Games.
- All sound generated in code.
- Dragon Cave, Denys Rutkovskyi, Fab personal licence (the cave rocks, the stones and the relic; the serpent is built in code). TODO(Quinn): confirm the seller and the licence.

Tools: Unreal Engine 5.9, Unreal MCP, Claude Code. TODO(Quinn): add the others (Obsidian, Google Docs).

## To run

Windows 10 or 11, DirectX 12. TODO(Quinn): minimum GPU and RAM, and the team name.
