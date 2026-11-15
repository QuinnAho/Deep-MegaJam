---
updated: 2026-11-14 23:07 EST
---
# Board

The agents' work queue. Design rules are in [[GDD]], dispatch rules in `.claude/loop.md`, every run in [[Agent Runs]]. Status words: `todo`, `doing`, `blocked`, `done`.

## State of the game

`main` is at `f9feeec`. 31 tests passing. The game runs from a title screen to a win screen.

- **Playable loop:** dive, grab, weight, tether, winch, the Captain's Patience, the cut, bailout air, re-tether, death and respawn, bank, settlement screen, hull rating, descent to stop 2, the Captain's subtitles. A test plays stops 1 and 2 start to finish.
- **Plain on purpose:** the grey box is evenly lit with no fog, at Quinn's request (D-020). No atmosphere work until the gameplay is right.
- **Pipeline:** `tools\ue.ps1` (build, test, verify, package, smoke, zip, shot, editor), `tools\mcp.ps1`, `tools\playtest.ps1` (S0 to S4), and our own `DeepToolset` MCP tools.
- **The sea serpent** is in from stop 2: it hears winching, sprinting and grabs, the lamp drives it back, a bite on the tether shows a 5 second alarm then cuts, and it kills an untethered diver. A red pulse and a bar show how close it is.
- **Parked, not in `main`:** the stopped look pass, on `look/pass1`.
- **Not built:** the relic and the win, stop 3 as a finale, title and menus, sound, voice, models.

## Running now

| Lane | Agent | Task | Started |
|---|---|---|---|
| Ending | systems-engineer (Opus, high), `.wt\dev` on `dev/ending` | **Done 11:27 PM**, merged as `f9feeec`. Item 3: title screen, stop 3 and the relic, the win with a performance review, pause, restart, quit. 31 of 31 tests. | 11:07 PM |
| Harpoon | systems-engineer (Opus, high), `.wt\monster` on `dev/harpoon` | Item 3a (D-021): one shot per dive, reels home off the sub, latches to anything solid, drives the serpent back, hull damage and a repair fee. | 11:13 PM |
| Submarine | systems-engineer (Opus, high), `.wt\dev` on `dev/sub` | Item 3c (D-022, D-023): the room, the hatch, the console, the helm, driving with the sub static and the world moving, and the flashlight and hull upgrade gates. Started before the harpoon landed to save time; it merges `main` at the end. | 11:28 PM |
| Regression | build-sheriff (Sonnet, medium), `.wt\pkg` on `tools/regress` | Bring the scripted playtests up to the new loop (S4 from the side, S5 cut, S6 bank, S7 descend, S8 full run), then package, smoke and zip the loop build. | 10:52 PM |

## Next, in order

| # | Item | Owner | Status |
|---|---|---|---|
| 1 | Play the loop and tune the feel: swim, sink, tether, winch, Patience. | Quinn | todo |
| 2 | Play against the serpent and tune it (Project Settings > Game > DEEP Monster). | Quinn | todo |
| 3 | Finish the game's shape: the relic at stop 3 and the win, restart, title, pause and quit. | systems-engineer | doing |
| 3a | The harpoon (D-021). Running in `.wt\monster` on `dev/harpoon` since 11:13 PM. | systems-engineer (Opus, high) | doing |
| 3b | Captain lines for the six harpoon events, and for the submarine interior and driving once that design is settled. | writer | todo |
| 3c | Go inside the submarine and drive it (D-022). Quinn settled the shape at 11:20 PM: the sub is static and the world moves; salvage goes to a console inside; a helm drives the sub deeper. Spec in the "Inside the submarine" section of [[GDD]]. Starts when the ending and the harpoon are merged, because it changes the same files. Includes the layer gates (D-023): no lamp at the start, the flashlight is the first purchase and the gate to layer 2, the hull upgrade is the gate to layer 3. | systems-engineer (Opus, high) | blocked on 3 and 3a |
| 3d | Water and rope feel (D-024): swim inertia and buoyancy, a tether and harpoon line that sag, snap taut and whip when cut, bubbles and silt. After the gameplay is right. | systems-engineer, Quinn tunes | later |
| 4 | Sound hooks and a minimum sound set. Needs files from Colby. | systems-engineer, Colby | blocked on files |
| 5 | Voice for the Captain's lines. | writer | blocked on Quinn's choice |
| 6 | Look pass, only after the gameplay is right (D-020). | look engineer | deferred |
| 7 | Ideas Quinn picks from [[Fun Ideas]]. | per idea | waiting on review |
| 8 | Sunday: feature freeze 3 PM, final package and video 5 PM, submitted 8 PM. | all | todo |

## Waiting on Quinn

- Close and reopen the editor to get the loop, then play it.
- The Captain's voice: an OpenAI key, or a built-in Windows voice as a stand-in.
- Team name, and whether both are enrolled students.
- Which of the [[Fun Ideas]] to build.
- Share the Brief doc with Colby. Delete the stray notes in the vault root.

## For Colby

Models: submarine first, then salvage pieces, relic, serpent head. Sounds: breathing, winch, tether snap, serpent, ambience, one music loop. Dev log post, gameplay video and itch.io page on Sunday.

## Checkouts

The main checkout is Quinn's and he may keep the editor open on it. Agents build only in `.wt\dev`, `.wt\monster` and `.wt\pkg` and push branches. The orchestrator verifies a branch in its own checkout, merges it into `main`, and tells Quinn to restart the editor.

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
