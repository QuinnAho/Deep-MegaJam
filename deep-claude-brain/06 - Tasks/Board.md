---
updated: 2026-11-15 01:15 EST
---
# Board

The agents' work queue. Design rules are in [[GDD]], dispatch and testing rules in `.claude/loop.md`, every run in [[Agent Runs]]. Status words: `todo`, `doing`, `blocked`, `done`.

## Handoff for a fresh orchestrator session (written 1:05 AM Nov 15)

A new session needs only `CLAUDE.md`, this note and `.claude/loop.md`. One agent is running (item 1). Its report goes to the session that started it, so a new session checks its branch instead:

1. **Fix batch one** (`dev/feel1`, built in `.wt\dev`, Opus at high, started 1:01 AM). Cut down at 1:12 AM by D-028 (no half-done polish): it keeps the title key press and the tether distance after a cut, and drops the motion cues and the HUD fixes. It may push dropped work to `parked/motion-hud`; leave that branch alone. Its report should list anything else that stops a real player finishing the loop with keyboard and mouse. Finished when `origin/dev/feel1` has commits and the last one merges `origin/main`. It is C++, so the gate is `.wt\dev\tools\ue.ps1 verify`. Then merge into `main`, push, and tell Quinn to restart the editor.
2. **Dev log post one** is merged (`c263c9a`): `devlog/01-saturday-night.md`, a draft file covering 8 PM to 1 AM. It is not published anywhere; Quinn reads it and decides where it goes. Post two should cover Sunday from 1 AM.
3. **Next, while Quinn has not played:** prove the loop with real key and mouse input, simulated through the editor's input tools (Sonnet at medium, `.wt\pkg`). The scripted playtests use console commands, so walking into the hatch, pressing E at the console and helm, and aiming the harpoon have never been exercised. Whatever fails is function and goes to an engineer. Then fix batch two: Quinn's notes on how the loop plays.
4. **Polish is one planned pass, later (D-028).** Do not dispatch HUD, look, effect or sound-finish work until Quinn gives it a start time.

After merging a branch that adds dev log entries, remove any repeated `## Sunday Nov 15 2026` heading in [[Devlog]].

## State of the game

`main` is at `844cbee`. 44 tests passing. Nobody has played it by hand yet, and nobody has heard it.

- **The run, start to finish:** title, then standing in the submarine's one room. Drop through the hatch to dive: grab, weight, tether, winch, the Captain's Patience, the cut, bailout air, the harpoon. Back up through the hatch. Bank at the console and buy the gates there. Drive from the helm between stops at 100, 300 and 500 m. The relic at stop 3, then the win screen with a performance review. Pause and restart work.
- **Layer gates (D-023):** the helm stops at 200 m without the flashlight (50 cr) and at 400 m without the hull upgrade (200 cr). The serpent appears in layer 2 and is aggressive in layer 3.
- **Sound (D-026):** every sound is generated in code. 43 events play one, and seven loops fade in and out (sea, room, breathing, fast breathing, winch, serpent, helm). One row each in `deep/Content/Data/Sounds.csv`. Commands: `Deep.Audio.List`, `Deep.Audio.Play <event>`, `Deep.Audio.Mute 0|1`, `Deep.Audio.Reload`.
- **Plain on purpose:** grey boxes, even light, no fog (D-020). No "Director" in the game's text (D-025).
- **Scripted playtests:** eight scenarios in `tools\playtest.ps1`: S0-title, S1-hatch, S2-gate, S3-serpent, S4-cut, S5-harpoon, S6-bank, S7-run. All eight passed twice before the sound merge; S1 (which checks a sound fired) and S7 passed after it.
- **Last package:** 707 MB on disk, 426.5 MB zipped, built from `ce9c4c8` with the sound layer in it. Cook clean, `Sounds.csv` inside, smoke run clean. The limit is 1 GB zipped.
- **Parked, not in `main`:** the stopped look pass, on `look/pass1`.

## Known rough edges

Function, being fixed in batch one:

- **The title may not start on a key press.** A scripted playtest's synthetic key press did not leave the title; the script uses `Deep.Start`. A real key press is unverified. This is the first thing a judge does.
- `tether_distance_m` reads 0.0 right after a cut.

Function, not yet looked at:

- Never verified: real keyboard and mouse anywhere, walking into the hatch with real physics, pressing E at the console and helm, aiming the harpoon by hand, anything in multiplayer.
- Every number is a guess: room size, walk speed, hatch size, harpoon speed and pull, serpent values.
- `Deep.Teleport` sometimes lands in the wrong place.

Finish, parked for the polish pass (D-028: do not patch these one by one):

- Driving does not feel like moving. The helm view is flat grey; only the gauge shows the descent.
- The HUD is a placeholder and looks it. Panels show in the wrong mode: the console panel at the helm, "LIMIT 400" over the depth gauge, the controls panel still up under the hatch, the status panel through the win screen.
- Every sound is generated in code and unheard. The final build ships none that Quinn has not listened to and kept.

## Running now

| Lane | Agent | Task | Started |
|---|---|---|---|
| Fix batch one | Opus, high, `.wt\dev` on `dev/feel1` | Title key press and the tether distance after a cut. Cut down at 1:12 AM (D-028): no motion cues, no HUD fixes. | 1:01 AM |
| Dev log post one | Sonnet, medium, `.wt\text` on `devlog/post1` | **Done 1:03 AM**, merged as `c263c9a`. `devlog/01-saturday-night.md`: the first five hours, for the Glass Box prize. A file only; nothing published. | 1:01 AM |

## Next, in order

| # | Item | Owner | Status |
|---|---|---|---|
| 1 | Play the whole run by hand and say what is wrong with how it plays. | Quinn | todo |
| 1a | Prove the loop with simulated key and mouse input, start to win, and list where real input fails. | Sonnet agent, `.wt\pkg` | after batch one |
| 2 | Fix batch two: Quinn's notes and what 1a finds, in one engineer run. Function only. | Opus engineer, `.wt\dev` | waiting on 1 and 1a |
| 3 | Real sound files replace the generated ones: import under `/Game/Audio/`, put the path in that row of `Sounds.csv`, run `Deep.Audio.Reload`. Sources: Colby, or WeLoveIndies from the jam perks. Quinn owns no audio on Fab. | Colby, Quinn | todo |
| 4 | The Captain's voice. | writer | blocked on Quinn's choice |
| 5 | **The polish pass (D-028, D-020):** look, HUD, effects, sound and voice, in one planned pass to a professional standard. Not started piece by piece. It needs a start time from Quinn and its inputs ready: Fab packs installed ([[Asset Library Shortlist]]), a source of real sound, the Captain's voice. | Quinn decides when | deferred |
| 6 | Water and rope feel (D-024): swim inertia, a tether and harpoon line that sag and snap, bubbles and silt. Part of the polish pass. | engineer, Quinn tunes | later |
| 7 | Ideas Quinn picks from [[Fun Ideas]]. Live Captain brain decision about 1 PM Sunday (D-018). | per idea | waiting on review |
| 8 | Sunday: feature freeze 3 PM, final package and video 5 PM, submitted 8 PM. The itch.io page says in one sentence how the game answers the theme (D-025). | all | todo |

## Waiting on Quinn

- Restart the editor, play the run, and send notes on how it plays. First: does a key press on the title start the game?
- When the polish pass starts, and its inputs: which Fab packs to install, where real sound comes from, the Captain's voice.
- The Captain's voice: an OpenAI key, or a built-in Windows voice as a stand-in.
- Team name, and whether both are enrolled students.
- Which of the [[Fun Ideas]] to build.
- Where the public dev log lives (the draft is a file in `devlog/`).
- Share the Brief doc with Colby. It still says "the Director". Delete the stray notes in the vault root (`Welcome.md`, the empty `GDD.md`, the empty co-op note).

## For Colby

Models: submarine first, then salvage pieces, relic, serpent head. Sounds: breathing, winch, tether snap, serpent, ambience, one music loop; each replaces a generated sound by one row in `deep/Content/Data/Sounds.csv`. Dev log post, gameplay video and itch.io page on Sunday.

## Checkouts

The main checkout is Quinn's and he may keep the editor open on it (port 8000: never call it without his say). Agents build only in `.wt\dev`, `.wt\monster`, `.wt\pkg` and `.wt\text` and push branches. The orchestrator runs the merge gate in the branch's own checkout, merges it into `main`, and tells Quinn to restart the editor. Free now: `.wt\monster`, `.wt\pkg`.

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
