---
updated: 2026-11-15 01:33 EST
---
# Board

The agents' work queue. Design rules are in [[GDD]], dispatch and testing rules in `.claude/loop.md`, every run in [[Agent Runs]]. Status words: `todo`, `doing`, `blocked`, `done`.

## Handoff for a fresh orchestrator session (written 1:33 AM Nov 15)

A new session needs only `CLAUDE.md`, this note and `.claude/loop.md`. Three research agents are running for the polish pass (D-029). They write notes only, so no tests. Their reports go to the session that started them, so a new session checks their branches instead:

1. **Assets** (`research/assets`, in `.wt\monster`, Sonnet at medium, started 1:32 AM). It runs its own editor on port 8013 to read Fab. It writes `03 - Research/Polish - Assets.md` and `deep\Saved\Harness\polish\assets.json` (not in git; thumbnails and links for a visual board). Check afterwards that it closed its editor.
2. **Sound, voice, fonts, references** (`research/polish-web`, in `.wt\text`, Sonnet at medium, started 1:32 AM). It writes `03 - Research/Polish - Sound, Voice, Fonts and References.md`.
3. **The plan** (`research/polish-plan`, in `.wt\dev`, Opus at high, started 1:32 AM). It writes `03 - Research/Polish - Plan and Techniques.md`: the look at each depth, the interior, the serpent, the HUD, which parts need binary assets, and six to ten work packages for about five hours.
4. **When all three are in:** merge the notes, then build Quinn a visual board from `assets.json` (he asked to look at the assets) and give him the decisions to make: the style, which packs to install, the sound source, the voice, and whether agents may create binary assets during the pass.
5. **Function, after the asset agent frees the machine** (one editor at a time: 32 GB of RAM, 6 GB free at 1:31 AM): prove the loop with simulated key and mouse input (Sonnet at medium, `.wt\pkg`, port 8012). The scripted playtests use console commands, so walking into the hatch, pressing E at the console and helm, and aiming the harpoon have never been exercised. Known suspect: E is bound to both Use and Continue, so E on the settlement screen may also use the console.
6. **Polish is one planned pass (D-028), and a priority equal to gameplay (D-029).** Research and planning now. No HUD, look, effect or sound-finish work is dispatched until Quinn has chosen from the research and given the pass a start time.

After merging a branch that adds dev log entries, remove any repeated `## Sunday Nov 15 2026` heading in [[Devlog]].

## State of the game

`main` is at `6f2e259`. 46 tests passing. Nobody has played it by hand yet, and nobody has heard it.

- **The run, start to finish:** title, then standing in the submarine's one room. Drop through the hatch to dive: grab, weight, tether, winch, the Captain's Patience, the cut, bailout air, the harpoon. Back up through the hatch. Bank at the console and buy the gates there. Drive from the helm between stops at 100, 300 and 500 m. The relic at stop 3, then the win screen with a performance review. Pause and restart work.
- **Layer gates (D-023):** the helm stops at 200 m without the flashlight (50 cr) and at 400 m without the hull upgrade (200 cr). The serpent appears in layer 2 and is aggressive in layer 3.
- **Sound (D-026):** every sound is generated in code. 43 events play one, and seven loops fade in and out (sea, room, breathing, fast breathing, winch, serpent, helm). One row each in `deep/Content/Data/Sounds.csv`. Commands: `Deep.Audio.List`, `Deep.Audio.Play <event>`, `Deep.Audio.Mute 0|1`, `Deep.Audio.Reload`.
- **Plain on purpose:** grey boxes, even light, no fog (D-020). No "Director" in the game's text (D-025).
- **Scripted playtests:** eight scenarios in `tools\playtest.ps1`: S0-title, S1-hatch, S2-gate, S3-serpent, S4-cut, S5-harpoon, S6-bank, S7-run. All eight passed twice before the sound merge; S1 (which checks a sound fired) and S7 passed after it.
- **Last package:** 707 MB on disk, 426.5 MB zipped, built from `ce9c4c8` with the sound layer in it. Cook clean, `Sounds.csv` inside, smoke run clean. The limit is 1 GB zipped.
- **Parked, not in `main`:** the stopped look pass, on `look/pass1`.

## Known rough edges

Function, fixed in batch one (merged 1:29 AM as `6f2e259`):

- The title: a real key press already worked through the viewport; the playtest tool's simulated key took another path. Both now start the run, the text says PRESS ANY KEY OR CLICK TO START, and a test presses seven keys and buttons. Still unverified: a physical keyboard.
- `tether_distance_m` is right after a cut.

Function, not yet looked at:

- Never verified: real keyboard and mouse anywhere, walking into the hatch with real physics, pressing E at the console and helm, aiming the harpoon by hand, anything in multiplayer.
- E is bound to both Use and Continue, so E on the settlement screen may also use the console. Unconfirmed.
- No gamepad support. In the editor the game window may need a click before keys arrive.
- Every number is a guess: room size, walk speed, hatch size, harpoon speed and pull, serpent values.
- `Deep.Teleport` sometimes lands in the wrong place.

Finish, parked for the polish pass (D-028: do not patch these one by one):

- Driving does not feel like moving. The helm view is flat grey; only the gauge shows the descent.
- The HUD is a placeholder and looks it. Panels show in the wrong mode: the console panel at the helm, "LIMIT 400" over the depth gauge, the controls panel still up under the hatch, the status panel through the win screen.
- Every sound is generated in code and unheard. The final build ships none that Quinn has not listened to and kept.

## Running now

| Lane | Agent | Task | Started |
|---|---|---|---|
| Fix batch one | Opus, high, `.wt\dev` on `dev/feel1` | **Done 1:29 AM**, merged as `6f2e259`. Title key press and tether distance. 46 of 46 tests. The cut HUD and motion work is parked on `parked/motion-hud`. | 1:01 AM |
| Assets | Sonnet, medium, `.wt\monster` on `research/assets` | What Quinn owns and what is free on Fab, per need, with one coherent style. Its own editor on port 8013. | 1:32 AM |
| Sound, voice, fonts, references | Sonnet, medium, `.wt\text` on `research/polish-web` | Web research with licences, and the jam's rules on third-party assets. | 1:32 AM |
| The polish plan | Opus, high, `.wt\dev` on `research/polish-plan` | Techniques for Unreal 5.9 and six to ten work packages for the pass. | 1:32 AM |
| Dev log post one | Sonnet, medium, `.wt\text` on `devlog/post1` | **Done 1:03 AM**, merged as `c263c9a`. `devlog/01-saturday-night.md`: the first five hours, for the Glass Box prize. A file only; nothing published. | 1:01 AM |

## Next, in order

| # | Item | Owner | Status |
|---|---|---|---|
| 1 | Play the whole run by hand and say what is wrong with how it plays. | Quinn | todo |
| 1a | Prove the loop with simulated key and mouse input, start to win, and list where real input fails. | Sonnet agent, `.wt\pkg` | after the asset agent |
| 2 | Fix batch two: Quinn's notes and what 1a finds, in one engineer run. Function only. | Opus engineer, `.wt\dev` | waiting on 1 and 1a |
| 3 | Real sound files replace the generated ones: import under `/Game/Audio/`, put the path in that row of `Sounds.csv`, run `Deep.Audio.Reload`. Sources: Colby, or WeLoveIndies from the jam perks. Quinn owns no audio on Fab. | Colby, Quinn | todo |
| 4 | The Captain's voice. | writer | blocked on Quinn's choice |
| 5 | **The polish pass (D-028, D-029):** look, HUD, effects, sound and voice, in one planned pass to a professional standard. A priority equal to gameplay. Research is running now. It then needs Quinn's choices and a start time (about 10 AM proposed; freeze at 3 PM). | Quinn chooses, agents build | research doing |
| 6 | Water and rope feel (D-024): swim inertia, a tether and harpoon line that sag and snap, bubbles and silt. Part of the polish pass. | engineer, Quinn tunes | later |
| 7 | Ideas Quinn picks from [[Fun Ideas]]. Live Captain brain decision about 1 PM Sunday (D-018). | per idea | waiting on review |
| 8 | Sunday: feature freeze 3 PM, final package and video 5 PM, submitted 8 PM. The itch.io page says in one sentence how the game answers the theme (D-025). | all | todo |

## Waiting on Quinn

- Restart the editor, play the run, and send notes on how it plays. First: does a key press on the title start the game?
- For the polish pass, once the research is in: the style, which Fab packs to install, where real sound comes from, the Captain's voice, whether agents may create binary assets, and the start time.
- The Captain's voice: an OpenAI key, or a built-in Windows voice as a stand-in.
- Team name, and whether both are enrolled students.
- Which of the [[Fun Ideas]] to build.
- Where the public dev log lives (the draft is a file in `devlog/`).
- Share the Brief doc with Colby. It still says "the Director". Delete the stray notes in the vault root (`Welcome.md`, the empty `GDD.md`, the empty co-op note).

## For Colby

Models: submarine first, then salvage pieces, relic, serpent head. Sounds: breathing, winch, tether snap, serpent, ambience, one music loop; each replaces a generated sound by one row in `deep/Content/Data/Sounds.csv`. Dev log post, gameplay video and itch.io page on Sunday.

## Checkouts

The main checkout is Quinn's and he may keep the editor open on it (port 8000: never call it without his say). Agents build only in `.wt\dev`, `.wt\monster`, `.wt\pkg` and `.wt\text` and push branches. The orchestrator runs the merge gate in the branch's own checkout, merges it into `main`, and tells Quinn to restart the editor. Free now: `.wt\pkg`. One Unreal editor at a time: the machine has 32 GB of RAM.

## Done

| Item | When | Commit |
|---|---|---|
| Research and setup notes in the vault | Nov 14, 9:00 PM | `487d2da` |
| Build and test pipeline | 9:32 PM | `487d2da` |
| The Captain's 138 lines | 9:49 PM | `5021c7f` |
| Packaging proven on the empty game | 9:53 PM | `993155e` |
| Playable grey box | 10:05 PM | `a445002` |
| MCP client, live tool catalog, first custom toolset | 10:07 PM | `3dea698` |
| Game state over MCP, scripted playtests, first screenshots | 10:27 PM | `ce7058a` |
| The core loop, from dive to descent | 10:49 PM | `12d026a` |
| The sea serpent, wired into the loop | 11:06 PM | `bf58444` |
| 47 more Captain lines (harpoon, interior) | 11:17 PM | `bac783f` |
| Title, relic, win, pause, restart | 11:27 PM | `f9feeec` |
| Nine scripted playtests; packaging fixed | 11:31 PM | `d0eb723` |
| The Director leaves the game's text | 11:35 PM | `99b6d90` |
| The harpoon | 11:37 PM | `cb50c97` |
| Shortlist of Quinn's Fab library | 11:56 PM | `f3f7f54` |
| Inside the submarine: hatch, console, helm, layer gates | Nov 15, 12:07 AM | `0acf06d` |
| The sound layer; the build script now builds like a committed tree | 12:36 AM | `64f7838` |
| Adaptive testing rule (D-027) | 12:52 AM | `f95f903` |
| Scripted playtests for the new flow; package with sound proven | 12:59 AM | `844cbee` |
| The title starts on any key or click; tether distance fixed | 1:29 AM | `6f2e259` |
