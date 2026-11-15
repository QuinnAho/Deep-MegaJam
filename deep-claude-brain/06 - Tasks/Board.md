---
updated: 2026-11-14 22:52 EST
---
# Board

The agents' work queue. Design rules are in [[GDD]], dispatch rules in `.claude/loop.md`, every run in [[Agent Runs]]. Status words: `todo`, `doing`, `blocked`, `done`.

## State of the game

`main` is at `12d026a`. 51 source files, about 6,900 lines, 21 tests passing.

- **Playable loop:** dive, grab, weight, tether, winch, the Captain's Patience, the cut, bailout air, re-tether, death and respawn, bank, settlement screen, hull rating, descent to stop 2, the Captain's subtitles. A test plays stops 1 and 2 start to finish.
- **Plain on purpose:** the grey box is evenly lit with no fog, at Quinn's request (D-020). No atmosphere work until the gameplay is right.
- **Pipeline:** `tools\ue.ps1` (build, test, verify, package, smoke, zip, shot, editor), `tools\mcp.ps1`, `tools\playtest.ps1` (S0 to S4), and our own `DeepToolset` MCP tools.
- **Built but not in `main`:** the sea serpent (`dev/monster`, `1ab0f29`). The stopped look pass is parked on `look/pass1`.
- **Not built:** the relic and the win, stop 3 as a finale, title and menus, sound, voice, models.

## Running now

| Lane | Agent | Task | Started |
|---|---|---|---|
| Monster | systems-engineer (Opus, high), `.wt\monster` on `dev/monster` | Merge `main` in, connect the serpent to the loop (noise, bite to the cut, caught to death, threat on the HUD, the Captain's monster lines), and look at it for the first time. | 10:52 PM |
| Regression | build-sheriff (Sonnet, medium), `.wt\pkg` on `tools/regress` | Bring the scripted playtests up to the new loop (S4 from the side, S5 cut, S6 bank, S7 descend, S8 full run), then package, smoke and zip the loop build. | 10:52 PM |

## Next, in order

| # | Item | Owner | Status |
|---|---|---|---|
| 1 | Play the loop and tune the feel: swim, sink, tether, winch, Patience. | Quinn | todo |
| 2 | Merge the serpent once wired; tune it. | orchestrator, Quinn | blocked on the monster lane |
| 3 | Finish the game's shape: the relic at stop 3 and the win, lose and restart, title and controls card, quit. | systems-engineer | todo |
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
