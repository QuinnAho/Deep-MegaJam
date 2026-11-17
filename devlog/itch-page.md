# DEEP: itch.io submission pack

Checked Nov 15 against tag `jam-final` (`b37b1c8`, game integration `d2a53bf`), the build in `Builds\NoTag-2127`. Deadline 10 PM PST (1 AM EST).

## 1. Blanks only Quinn can fill

Solo entry (Quinn, Nov 15: Colby didn't have time to work on it). Section 2 is ready to paste as it is. Change it only if you want to:

- Tagline and theme sentence: written in; edit them if you like. The theme's name appears only in the theme sentence (D-025).
- Dragon Cave: says "Fab personal licence"; change it if you bought another tier.
- For the jam form: the video link and the dev log link (`https://<you>.itch.io/deep/devlog`).
- Zip name: `<your entrant name>_DEEP.zip`.

Paste tip: itch.io's editor is rich text, so raw Markdown shows its `#` and `|` symbols. Open this file in a Markdown preview (VS Code: Ctrl+Shift+V) and copy from the preview. Images don't carry over; add them by hand or skip them.

## 2. Page copy (paste from the line below to "End of page copy")

# DEEP

*Cheapest submarine on the market. Something down there can hear it.*

## The game

You are a salvage diver on a tether. You pilot a cheap submarine down a winding, generated cave. Every stop is a cavern. Hit the rock hard and the hull takes damage. Breach it and the diver aboard dies.

At each stop, drop through the hatch, swim out, grab salvage, and get back up the line. Past 10 kg you sink. At 15 kg you cannot swim up, and the loud winch is the only way home.

The tether is your air line. Clipped on, your air never runs out. Press T to unclip and you have 30 seconds of bailout air to reach the hatch.

A sea serpent hears loud. It hears your sonar ping, your harpoon, hull knocks and a hard-driven sub. It circles at the edge of your lamp and taunts you twice, then it strikes. Turn your lamp on it and it backs off. Too slow, and it bites through the line.

The Captain works for the company. He never says a word, and he takes 40 percent of every haul. Dying is billed as a fee. The next diver is already on the way.

**The theme.** Every haul gets edited by the Captain's cut, and the cut tether is the final edit.

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
| W / S | Throttle up or down: two astern, stop, three ahead |
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
- Three stops, at 100, 300 and 500 m, and a relic at 600 m. A flashlight and a hull upgrade are bought at settlement.
- A calm screen. Text shows only while you need it, and the helm has no sonar panel.
- Camera shake on hits and bites. All sound is made in code; no voice or music files ship.

## How it was made

Made solo by Quinn, with Claude agents. Quinn chose the idea and made every call on style and scope. The agents wrote the C++, the tests, the playtests and the dev log; one orchestrating agent planned and reviewed and wrote no game code. This build passes all 164 of its automated tests.

Dev log: see the Devlog posts on this page.

## Credits

- Unreal Engine 5.9 (MegaJam Preview), Epic Games.
- All sound generated in code.
- Dragon Cave, Denys Rutkovskyi, Fab personal licence (the rock and stone meshes and textures in the cave walls and caverns, and the relic's mesh and textures; the serpent is built in code).
Tools: Unreal Engine 5.9, Unreal MCP, Claude Code, Obsidian (notes) and Google Docs (design).

## To run

Windows 10 or 11 (64-bit), DirectX 12. The look is built for hardware ray tracing (Lumen ray-traced lighting, reflections and shadows), so a GPU with ray tracing support is recommended. Ray tracing can be switched off in Settings.

<!-- End of page copy -->

## 3. Jam form answers

- **Tools used:** Unreal Engine 5.9 (MegaJam Preview), Unreal MCP, Claude Code (Claude agents), Obsidian, Google Docs.
- **Third-party assets and licences** (from `Credits.csv`):
  - Unreal Engine 5.9, Epic Games, Unreal Engine EULA (MegaJam Preview build).
  - All sound: generated in code, original work (the game's own synthesiser; no recordings).
  - Dragon Cave (cave rocks, stones and the relic), Denys Rutkovskyi, Fab personal licence.
- **OpenAI use:** None.
- **NVIDIA features:** None claimed. (The build uses Unreal's hardware ray tracing, not an NVIDIA-specific feature; do not claim DLSS or similar.)
- **Theme sentence:** Every haul gets edited by the Captain's cut, and the cut tether is the final edit.
- **Entry:** solo, Quinn, with Claude agents. Best Team needs two people, so it doesn't apply.
- **Minimum spec:** Windows 10 or 11, DirectX 12, a GPU with hardware ray tracing recommended. Ray tracing can be switched off in Settings (the menu has a RAY TRACING row). Not tested: how it runs on a GPU without ray tracing.

## 4. Video shot list (45 to 60 s, 6 beats)

From `deep-claude-brain/01 - Jam/Video Shot List.md`, corrected: the helm sonar panel is off, so the helm shot shows the lit cave through the window and the calm HUD only (throttle, ballast, heading, depth, hull bar). The V ping is the diver's, in the water. The aim label shows only a distance.

Record on `Builds\NoTag-2127`. It is a Development build, so the console (the ` key) works: `Deep.BuyGate free` buys the flashlight, `Deep.GotoStop 2` jumps to stop 2. Try them once before recording. Do one rehearsal.

| # | Sec | On screen |
|---|---|---|
| 1 | 0-10 | At the helm: W for throttle, A/D rudder, hold C to sink. The generated cave scrolls past the window. No sonar panel. Best look first. |
| 2 | 10-17 | Knock a wall on purpose: the view jolts, the hull bar drops. Then X for all stop. |
| 3 | 17-24 | Park at a cavern, E to stand up, walk to the hatch and drop through. |
| 4 | 24-34 | In the water: F lamp, V once (outlines the walls), E grabs salvage. |
| 5 | 34-46 | The serpent circles at the edge of the lamp and taunts. Lamp on it and it backs off. |
| 6 | 46-56 | Theme beat: T to unclip, the line goes slack and bailout air counts down; back aboard, the settlement receipt with the Captain's 40 percent cut. End on DEEP. TODO(Quinn): title card, or leave it out. |

Beat 5 is the hardest to get on demand; `Deep.Monster.Taunt` is a cheat if it misbehaves. Skip the menus, dead time and any taskbar frame. OBS: 1920x1080, 60 fps, MP4. Upload to YouTube Unlisted and test the link in a private window; start by 9 PM PST at the latest.

## 5. Dev log posts to paste (`devlog\`)

Post two, as the submission checklist planned:

1. `01-saturday-night.md`: Saturday night.
2. `02-sunday.md`: all of Sunday up to the freeze.

Skip `02-sunday-before-dawn.md` and `03-sunday-morning.md`. They are earlier drafts, and `02-sunday.md` covers the same hours. Both posts now say Quinn made the game solo, and the Colby section is gone. Leave their 8:59 PM EST deadline as it is: that was the deadline when they were written.
