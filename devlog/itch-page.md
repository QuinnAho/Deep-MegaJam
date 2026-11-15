# DEEP

*Cheapest submarine on the market. Loudest opinion of you.*

<!-- Draft for itch.io. Body under 500 words. Every TODO(Quinn) is a fact the repo cannot confirm. -->

## The game

You are a salvage diver on a tether, lowered from a submarine into a trench that gets darker the further you go. Swim down, grab what is lying on the seabed, and get back up the line. Everything you carry is heavy. Past ten kilograms you sink, and an overloaded diver can only be winched home, which is loud.

Something in the water hears loud. A sea serpent, a dragon-headed thing, comes in toward the tether when you make noise and backs off when you hold your lamp on it. You hear it long before you see it. A bite on the tether starts a five-second alarm.

The submarine's AI is the Captain, and it works for the company. It mocks you the whole way down, takes 40 percent of every haul, and cuts your tether when its patience runs out. Dying is billed as a fee. The next diver is already on the way.

**The theme.** Nobody says the theme in the game, so here: every haul gets edited by the Captain's cut, and the cut tether is the final edit. TODO(Quinn): confirm you want this sentence, and the theme's name, kept in this one place.

## Controls

Keyboard and mouse. There is no controller support.

| Key | Action |
|---|---|
| W A S D | Swim (walk inside the submarine) |
| Mouse | Look |
| Space / C or Left Ctrl | Swim up / down |
| Left Shift | Sprint (noisy) |
| E | Grab salvage; use the console and helm |
| G | Drop what you carry |
| Left click | Harpoon, one per dive |
| R (hold) | Winch home, loud |
| F | Lamp, once you have bought the flashlight |
| Q (hold) | Ask to be cut loose |
| Esc or P | Menu |

## What is in the build

- A title menu, settings (volume, look sensitivity, invert look, ray tracing), credits, a pause menu and quit.
- A walkable submarine with a hatch to the sea, a console that banks salvage and shows the settlement screen, and a helm that drives the submarine down.
- Three depth layers: Sunlight, Twilight and Midnight, to 600 m. The flashlight and a hull upgrade are bought at settlement.
- The tether, winch, weight, harpoon, the Captain's patience and the cut, with a bailout run on the cut.
- The sea serpent. Its head uses a dragon model from a third-party pack (see credits).
- About 180 lines for the Captain, shown as subtitles, and a set of sounds made in code. TODO(Quinn): confirm no voice recordings or real sound files are in the final build.
- A win screen with the choice to dive again.

## How it was made

Two people, Quinn and Colby, and Claude agents, in about a day. The jam was three days old when we started. TODO(Quinn): confirm the start time you want quoted and whether Colby wants his role described.

The people chose the idea, the Captain's voice and every call on style and scope. They approved or rejected frames. The agents wrote the C++, the tests, the scripted playtests, the Captain's lines and the dev log entries, and ran the build and packaging. One orchestrating agent dispatched the others and did not write game code.

- 157 commits on the integration branch at the time of writing. TODO(Quinn): refresh the number at submission.
- 67 automated test cases in the source. TODO(Quinn): refresh.
- About 185 lines of Captain dialogue, written as data.

Dev log: TODO(Quinn): link. The drafts are `devlog/01-saturday-night.md` and `devlog/02-sunday-before-dawn.md`.

## Credits and third-party assets

| Item | Author | Licence |
|---|---|---|
| Unreal Engine 5.9 (MegaJam Preview) | Epic Games | Unreal Engine EULA |
| All sound | Generated in code | Original work |
| Dragon Cave | TODO(Quinn): seller on Fab | TODO(Quinn): Fab licence |
| Megascans / Quixel assets | Quixel | TODO(Quinn): list only the ones that shipped |

Tools used: Unreal Engine 5.9 MegaJam Preview, Unreal MCP, Claude Code. TODO(Quinn): add every other tool actually used (Obsidian and Google Docs were used for notes).

## What it needs to run

Windows 10 or 11, DirectX 12. Ray tracing can be switched off in the settings. TODO(Quinn): minimum GPU and RAM, once someone has tested on a second machine.

## Team

TODO(Quinn): team name, and anything about who made it.
