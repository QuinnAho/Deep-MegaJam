---
updated: 2026-11-15 00:12 EST
---
# Board

The agents' work queue. Design rules are in [[GDD]], dispatch rules in `.claude/loop.md`, every run in [[Agent Runs]]. Status words: `todo`, `doing`, `blocked`, `done`.

## Handoff for a fresh orchestrator session (written 12:12 AM Nov 15)

A new session needs only `CLAUDE.md`, this note and `.claude/loop.md`. Two agents are running. Their reports go to the session that started them, so a new session checks their branches instead:

1. **Playtests and package** (`tools/regress`, built in `.wt\pkg`, Sonnet at medium, started 12:11 AM). Finished when `origin/tools/regress` has a commit after `0acf06d`. It changes scripts, frames and the dev log only, so merge it into `main` and push. Read its dev log entry for game bugs it found and add them to "Known rough edges".
2. **Sound layer** (`dev/audio`, built in `.wt\monster`, Opus at high, started 12:11 AM, D-026). Finished when `origin/dev/audio` has commits and the last one merges `origin/main`. Then run `.wt\monster\tools\ue.ps1 verify`, merge into `main`, push, and tell Quinn to restart the editor and listen. If the branch stalls, the brief is: sounds generated in code, one CSV row per sound, new files in `deep\Source\Deep\Audio\`.
3. **Next engineer run** (`.wt\dev`, Opus at high) is one feel-fix batch: Quinn's notes from playing by hand plus "Known rough edges" below. Wait for his notes so it is one run, not two.

After merging a branch that adds dev log entries, remove any repeated `## Sunday Nov 15 2026` heading in [[Devlog]].

## State of the game

`main` is at `0acf06d`. 39 tests passing. Nobody has played it by hand yet.

- **The run, start to finish:** title, then standing in the submarine's one room. Drop through the hatch to dive: grab, weight, tether, winch, the Captain's Patience, the cut, bailout air, the harpoon. Back up through the hatch. Bank at the console and buy the gates there. Drive from the helm between stops at 100, 300 and 500 m. The relic at stop 3, then the win screen with a performance review. Pause and restart work.
- **Layer gates (D-023):** the helm stops at 200 m without the flashlight (50 cr) and at 400 m without the hull upgrade (200 cr). The serpent appears in layer 2 and is aggressive in layer 3.
- **Plain on purpose:** grey boxes, even light, no fog (D-020). No "Director" in the game's text (D-025).
- **Silent** until the sound layer merges.
- **Last package:** 707 MB on disk, 426 MB zipped, at `d0eb723`, before the harpoon and the interior. A new one is being built.
- **Parked, not in `main`:** the stopped look pass, on `look/pass1`.

## Known rough edges (input for the feel-fix batch)

- **Driving does not feel like moving.** At the helm the view is flat grey with three specks; only the gauge shows the descent (`devlog/media/2026-11-14-sub-helm-descent.png`). It needs grey-box motion cues, such as shapes streaming past the window and the light dimming with depth. This is feedback for a mechanic, not a look pass.
- The settlement panel overlapped the left HUD box, and the HUD still said "TETHER 31 m" while settling. Seen before banking moved to the console; the playtest agent is re-checking.
- `Deep.Teleport` sometimes lands in the wrong place. `tether_distance_m` reads 0.0 right after a cut.
- Every number is a guess: room size, walk speed, hatch size, harpoon speed and pull, serpent values.
- Never verified: walking into the hatch with real physics, key presses at the helm, aiming the harpoon by hand, anything in multiplayer.

## Running now

| Lane | Agent | Task | Started |
|---|---|---|---|
| Playtests and package | Sonnet, medium, `.wt\pkg` on `tools/regress` | Rewrite the scripted scenarios for the hatch, console, helm, gates and harpoon; run the set twice; package, smoke and zip. No game code. | 12:11 AM |
| Sound | Opus, high, `.wt\monster` on `dev/audio` | The sound layer (D-026): generated sounds for events and loops, `Sounds.csv`, settings, `Deep.Audio.*` commands, a `DEEPAUDIO` log line so playtests can check what fired. | 12:11 AM |

## Next, in order

| # | Item | Owner | Status |
|---|---|---|---|
| 1 | Play the whole run by hand and say what feels wrong. | Quinn | todo |
| 2 | Feel-fix batch: Quinn's notes plus the rough edges above, in one engineer run. | Opus engineer, `.wt\dev` | waiting on 1 |
| 3 | Real sound files replace the generated ones, one CSV row each. Sources: Colby, or WeLoveIndies from the jam perks. Quinn owns no audio on Fab. | Colby, Quinn | todo |
| 4 | The Captain's voice. | writer | blocked on Quinn's choice |
| 5 | Look pass, when Quinn says the gameplay is right (D-020). His own assets that fit are in [[Asset Library Shortlist]]. | look engineer | deferred |
| 6 | Water and rope feel (D-024): swim inertia, a tether and harpoon line that sag and snap, bubbles and silt. | engineer, Quinn tunes | later |
| 7 | Ideas Quinn picks from [[Fun Ideas]]. Live Captain brain decision about 1 PM Sunday (D-018). | per idea | waiting on review |
| 8 | Sunday: feature freeze 3 PM, final package and video 5 PM, submitted 8 PM. The itch.io page says in one sentence how the game answers the theme (D-025). | all | todo |

## Waiting on Quinn

- Restart the editor, play the run, and send notes.
- The Captain's voice: an OpenAI key, or a built-in Windows voice as a stand-in.
- Team name, and whether both are enrolled students.
- Which of the [[Fun Ideas]] to build.
- Share the Brief doc with Colby. It still says "the Director". Delete the stray notes in the vault root (`Welcome.md`, the empty `GDD.md`, the empty co-op note).

## For Colby

Models: submarine first, then salvage pieces, relic, serpent head. Sounds: breathing, winch, tether snap, serpent, ambience, one music loop; each replaces a generated sound by one row in `deep/Content/Data/Sounds.csv`. Dev log post, gameplay video and itch.io page on Sunday.

## Checkouts

The main checkout is Quinn's and he may keep the editor open on it (port 8000: never call it without his say). Agents build only in `.wt\dev`, `.wt\monster`, `.wt\pkg` and `.wt\text` and push branches. The orchestrator verifies a branch in its own checkout, merges it into `main`, and tells Quinn to restart the editor. Free now: `.wt\dev`, `.wt\text`.

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
