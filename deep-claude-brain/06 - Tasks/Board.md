---
updated: 2026-11-14 21:36 EST
---
# Board

The agents' work queue. Quinn said go at about 9:12 PM Nov 14. Rules for each item are in [[GDD]]. Status words: `todo`, `doing`, `blocked`, `done`.

## Running now

| Lane | Agent | Task | Started |
|---|---|---|---|
| Pipeline | build-sheriff | **Done 9:32 PM.** `tools\ue.ps1` exists; `build` (45 s cold, 5 s warm), `test` (17 s) and `verify` pass, and a failing test, an empty filter and a compile error each report FAIL. All Toolsets and Unreal MCP are in the `.uproject`. Not yet run: `package`, `zip`, `smoke`, `editor`. | 9:21 PM |
| Gameplay | systems-engineer (Opus, high) | Items 1 and 2 below: rules with tests, then the playable grey box. | 9:34 PM |
| Writing | director-writer | **Done 9:30 PM.** 138 lines across 41 events in `deep\Content\Data\DirectorLines.csv`, 24 fee labels in `SettlementFees.csv`. Checked: ASCII, no duplicate ids, longest line 15 words. | 9:21 PM |

| Packaging | build-sheriff (Sonnet, medium) | **Done 9:53 PM**, merged from `sheriff/package`. On the still-empty game: `package` passes in 5 min 12 s (729 MB on disk), `smoke` passes, `zip` is 449 MB. `shot` verb added; it needs game-side `-DeepShot` support before it can pass. The checkout `.wt\pkg` stays as the packaging lane: merge `main` into it and package there, so packaging never blocks the builder in the main checkout. | 9:43 PM |
| Writing | writer (Sonnet, medium) | **Done 9:49 PM**, commit `5021c7f`. All 138 lines rewritten for the Captain in `deep\Content\Data\CaptainLines.csv`; fee labels sharpened. Checked: same ids and counts, ASCII, 10 lines say "Director's cut". | 9:47 PM |

| MCP tooling | tools agent (Sonnet, medium) | In `.wt\pkg` on branch `tools/mcp`, against a live 5.9 editor on port 8012: a command-line MCP client `tools\mcp.ps1`, a live catalog of 5.9's toolsets, a proven PIE round trip with a screenshot, and how to write our own toolset. | 9:56 PM |

Models and effort follow `.claude/loop.md`. The submarine AI is now **the Captain**; the Director is its unseen boss. See [[GDD]] v4.

Only one agent builds or tests in the main checkout at a time: a running test or cook holds the module DLL and breaks a parallel build.

## Next, in order

| # | Item | Agent | Target (EST) | Status |
|---|---|---|---|---|
| 1 | Rules in `Sim/` with tests: depth and zones, tether, weight, Patience, settlement, fees, hull. Tuning in `UDeepTuningSettings`. | systems-engineer | Sat 10:15 PM | blocked on pipeline |
| 2 | Playable grey box: diver (swim, lamp, grab, drop), tether and winch, salvage and weight, Patience, the cut, bailout, a self-building arena, `Deep.*` cheats, `DEEPSTATE`, a plain HUD. | systems-engineer | Sat 11:45 PM | blocked on 1 |
| 3 | Bank, settlement screen, hull rating, descend by fade, re-tether, respawn counter. | systems-engineer | overnight | blocked on 2 |
| 4 | Packaged build and smoke run of the playable loop, in `.wt\pkg`. The pipeline itself is proven (9:53 PM). Watch the size: the empty game is already 729 MB on disk and 449 MB zipped, against a 1 GB upload limit. | build-sheriff | overnight | blocked on 3 |
| 5 | Subtitles: the bark system that plays the Captain's lines on events. Reads `CaptainLines.csv`, fills the `{n}`, `{gross}`, `{cut}`, `{net}`, `{depth}` tokens, never repeats a variant twice running. Voice is optional per line: play the audio for a line id if it exists, otherwise subtitle only. The 12 lines with tokens are subtitle only. | systems-engineer | Sun 10 AM | blocked on 2 |
| 5a | `DeepToolset`: our own MCP tools for the game. `GetState` (typed, replaces parsing the log), `GotoStop`, `GiveSalvage`, `SetPatience`, `ForceCut`, `SpawnMonster`, `ResetRun`. Editor-only module. Follows the how-to from the MCP tooling agent. | systems-engineer or a second builder in `.wt\pkg` | overnight | blocked on 2 and the MCP tooling agent |
| 5b | Scripted playtests: `tools\playtest.ps1 S1` drives PIE through `tools\mcp.ps1` and asserts on `GetState`, with a screenshot per scenario. No model tokens per run; an agent only reads the summary. | build-sheriff | Sun 9 AM | blocked on 5a |
| 6 | AI playtest: scenarios S1 to S6 scripted (5b), plus one exploratory "critic" pass by an agent. Works through `tools\mcp.ps1` on an editor the harness starts, so it does not depend on Claude's own MCP connection. | playtester | Sun 10 AM | blocked on 5b |
| 7 | Monster pressure cycle and tether bite. | systems-engineer, Quinn tunes | Sun noon | blocked on 3 |
| 8 | Title, controls card, win and lose screens, restart, quit. | systems-engineer | Sun 2 PM | blocked on 3 |
| 9 | Voice for the Director's lines. | director-writer | Sun 2 PM | blocked on Quinn's choice |
| 10 | Feature freeze, then fixes only. | all | Sun 3 PM | todo |
| 11 | Final package, second PC test, 30 to 60 second video. | Quinn + Colby | Sun 5 PM | todo |
| 12 | itch.io page, submission form, dev log link. Submitted. | Colby, Claude drafts | Sun 8 PM | todo |

## Waiting on Quinn

| Item | Why it matters |
|---|---|
| Open the editor after the pipeline agent finishes, then reconnect Claude with `/mcp`. | Agents cannot see or play the game until this session is connected to `unreal-mcp`. |
| May Claude commit at each green checkpoint? | Commits give rollback points and feed the dev log. Nothing has been committed yet. |
| The Director's voice: an OpenAI key, or a built-in Windows voice as a stand-in. | Decides item 9 and the OpenAI award. |
| Share [DEEP - Brief](https://docs.google.com/document/d/1s7-gTlxQ-urXaO8CnRa_atFVw1bGDQ9sgtFYwNMFdtM/edit) with Colby. | Claude cannot change sharing. |
| Delete the stray notes in the vault root (`Welcome.md`, empty `GDD.md`, empty `Co-op, Steam and Voice.md`) and the old long design doc. | The empty notes shadow the real ones in Obsidian. |

## For Colby

Models (submarine, salvage pieces, relic, monster), sounds (breathing, winch, tether snap, monster, ambience, one music loop), then the three light and fog settings once the arena exists. Dev log post, video and itch.io page on Sunday.

## Done

| Item | When | Evidence |
|---|---|---|
| Research and setup notes in the vault. | Nov 14, 9:00 PM | [[00 - Start Here]] |
| Hole pass and narrowed scope. | Nov 14, 9:10 PM | D-013 in [[Decision Log]] |
| Brief design doc in Google Docs. | Nov 14, 9:13 PM | [[GDD]] |
| Fun and winning pass; agent role files; `.mcp.json`. | Nov 14, 9:15 PM | D-014 in [[Decision Log]], `.claude/agents` |
