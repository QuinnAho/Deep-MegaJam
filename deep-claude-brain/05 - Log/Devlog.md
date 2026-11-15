---
updated: 2026-11-14 21:00 EST
---
# Devlog

Raw material for the Glass Box prize, which goes to the best dev log showing how the game was built. This note is the honest record: what we did, what went wrong and what we cut. The devlog-scribe agent turns it into posts. Nothing here is polished and nothing is left out because it is embarrassing.

## Entry format

Add entries as things happen, newest at the bottom of the day.

```
### 9:40 PM  Short title
- What happened: one or two sentences.
- Why it matters: what it changed, or what we learned.
- Evidence: a link, a screenshot path, a commit hash or a test result.
```

Times are EST. Mark a time "about" when it comes from memory and not from a timestamp.

## Saturday Nov 14 2026

### 6:43 PM  The first idea is written down
- What happened: The "Game Jammy" Google Doc is created with the first notes: a submarine horror game in the style of a co-op extraction game, a tether, a diver, a captain run by an AI, a monster that fears light and hears you.
- Why it matters: This is the starting point. The jam opened on Nov 12, so we began on the evening of day three of four.
- Evidence: The doc's creation time in Google Drive.

### 8:06 PM  The repo exists
- What happened: The git repo is created.
- Why it matters: Commits will feed this dev log.
- Evidence: The repo folder timestamp.

### 8:09 PM  Claude gets a brain
- What happened: An Obsidian vault is created inside the repo as Claude's memory.
- Why it matters: Every Claude session, on either brother's machine, reads the same notes and writes back to them.
- Evidence: This vault. See [[00 - Start Here]].

### About 8:10 PM  The engine finishes installing
- What happened: Unreal Engine 5.9 MegaJam Preview finishes installing.
- Why it matters: It is the build with the new agent tooling. It is also a preview that Epic calls unstable.
- Evidence: The engine folder timestamp.

### About 8:15 PM  Research changes the plan
- What happened: The Claude session starts and the first hour of research finds five things:
  - The deadline is about 24 hours away: Sunday 8:59 PM EST.
  - The theme is Director's Cut.
  - UE 5.9 ships Epic's own MCP server, and a new project has every toolset turned off, so an agent sees no tools until they are enabled.
  - The DLSS plugin stops at UE 5.8.
  - NVIDIA ACE is not practical for a jam build.
- Why it matters: The original plan assumed more time, co-op, and NVIDIA ACE. The theme gave the submarine AI a name and a job: the Director, who takes a cut and cuts the tether.
- Evidence: [[Jam Brief]], [[Unreal MCP in UE 5.9]], [[NVIDIA RTX, DLSS and ACE]], [[Track Strategy]].

### About 8:35 PM  Claude runs ahead and gets stopped
- What happened: Claude starts writing a C++ project scaffold: project file, build files, a rules library and tests. Quinn rejects the writes and says, in short: I'm going to make the project, don't code yet.
- Why it matters: The agent ran ahead of the humans. The idea was not settled and Quinn wanted to create the project himself. The fix was a standing rule: no game code and no scaffolding until Quinn says go. Claude removed the empty folders it had made.
- Evidence: D-002 in [[Decision Log]].

### 8:38 PM  Quinn creates the project
- What happened: Quinn creates `Deep.uproject` in UE 5.9. It is Blueprint-only and empty.
- Why it matters: There is now a real project to point the tools at.
- Evidence: The editor process start time.

### About 8:45 PM  A one-way door
- What happened: Quinn asks for specialised subagents and end-to-end validation with AI playing the game. He also says he is fine with 5.8 but wants to use 5.9's features for development and maybe move to 5.8 later. We learn that this cannot work: assets saved in 5.9 will not open in 5.8.
- Why it matters: The engine has to be chosen before any content exists. The toolsets that let an agent play and see the game are only in 5.9. The DLSS plugin is only in 5.8.
- Evidence: [[Engine Choice - 5.9 vs 5.8]], D-006 in [[Decision Log]].

### About 8:50 PM  The project moves into the repo, and the work fans out
- What happened: Quinn moves the project into the repo at `deep/` and tells Claude to use subagents to move faster. The rest of this vault is written by three parallel forks of the main session.
- Why it matters: This is the first use of parallel agents on the project, and it was used on notes before any code.
- Evidence: D-004 and D-005 in [[Decision Log]].

### 8:56 PM  The design doc goes to the cloud
- What happened: Claude writes the game design and a ranked to-do list, with a Quinn / Colby / Claude split, into a new Google Doc. Quinn's rule for it: design and to-dos only, not Claude's memory. Claude could create the doc but was not permitted to change its sharing, so Quinn shares it with Colby by hand.
- Why it matters: Humans get one editable page. Agents keep a short copy in [[GDD]] and re-read the doc at the start of each work block.
- Evidence: The doc link in [[GDD]].

### 9:00 PM  The brain is populated, and we learn what it cost
- What happened: The vault reaches 19 notes: jam facts, track strategy, engine research, the validation design, the agent roster, the decision log and this dev log. Checks run on the result: every wiki link resolves and no token or account detail appears in any note.
- Why it matters: Two lessons. First, each of the three forked subagents carried the whole research conversation and used about 470,000 tokens to write two to five notes. From now on subagents start fresh and are briefed with links to vault notes, which is the reason the vault exists. Second, Quinn asked for memory to stay light, so what loads every session is six one-line pointers and a short `CLAUDE.md`; everything else is read on demand.
- Evidence: [[00 - Start Here]], [[Agent Orchestration]].

### 9:00 PM  The dev log becomes a standing duty
- What happened: Quinn: "very important we need to keep a dev log". The rule is now in the repo's `CLAUDE.md`: every session appends here while it works, not at the end.
- Why it matters: The Glass Box prize is judged on this record, and moments such as a first playtest cannot be recreated afterwards.
- Evidence: `CLAUDE.md` at the repo root.

### 8:55 PM  The project becomes a C++ project
- What happened: Quinn adds a C++ module, `Deep`, to the project. The first build finishes a minute later.
- Why it matters: Agents write and test C++ far more reliably than Blueprint graphs, and C++ survives an engine change in either direction.
- Evidence: `deep/Source`, `deep/Binaries` timestamps.

### 9:10 PM  The first cut: ten holes and a shorter list
- What happened: Quinn asks for a briefer doc, the holes in the gameplay, and only the to-dos that finish in time. Claude finds ten holes in its own v1 design. The worst three: a player could run out of salvage and never afford to descend; the monster did not appear for five or more dives; and death was never defined. The fixes change prices, add respawn and re-tether rules, and make the relic too heavy to swim up.
- Why it matters: The to-do list drops from fifteen items to twelve must-ship items with a cut order. The gear upgrade shop, a unique third level and five smaller features are cut 23.8 hours before the deadline.
- Evidence: D-013 in [[Decision Log]], [[GDD]], [[Board]].

### About 9:12 PM  Go
- What happened: Quinn says go and asks one question before the building starts: what makes this fun, and what gives us the best shot at winning? Claude's answer is five principles. One tension only, greed against the cut. The cut is the best moment in the game and never ends the run alone. The Director's writing is the personality, and dying is a joke about replacement divers. The first 90 seconds must show the theme, the loop and the joke, because a judge gives a jam game about five minutes. The monster is a pressure cycle that is heard long before it is seen.
- Why it matters: The design changes again. The player can now ask to be cut loose to reach salvage beyond the tether, and the level builds itself from code so agents can test without a map.
- Evidence: D-014 in [[Decision Log]], [[GDD]].

### 9:16 PM  The first two agents start, then restart on a smaller model
- What happened: Two fresh subagents start in parallel, each briefed with a role file and links to vault notes: a build sheriff writing the build, test and package script, and a writer producing the Director's lines as data. The gameplay engineer waits for the pipeline, because two agents building in one checkout break each other's builds. Two minutes in, Quinn asks for subagents to run on Opus 5.5 to keep usage down. Neither agent had written a file yet, so both are stopped and restarted on Opus 5.5 at 9:21 PM.
- Why it matters: This is the pattern for the rest of the jam. The orchestrator hands out work and checks evidence; agents start small, read only what they need, and run on the cheaper model.
- Evidence: `.claude/agents` (each role file names `model: opus`), [[Board]].

### About 9:25 PM  A cost policy for the agents
- What happened: Quinn asks Claude to act as the orchestrator and get the best performance per token. The rules go into `.claude/loop.md`: the cheapest model that will pass the gate first time (Opus 5.5 for critical-path gameplay code, Sonnet for scripts, writing, playtests and this dev log, Haiku for running a command and summarising it); fresh agents with tight briefs; reports under 150 words; warm agents reused for the next task in the same code; a model raised only after a second failed gate. The gameplay engineer also keeps a 40-line code map so new agents do not read the whole source tree.
- Why it matters: The first three subagents cost about 470,000 tokens each. The work that most needs a large model is deciding what to build and checking evidence, not typing.
- Evidence: `.claude/loop.md`, `.claude/agents`.

### 9:27 PM  The Director gets a voice, as data
- What happened: The director-writer agent writes all of the Director's lines as two CSV files. There are 138 lines across 41 game events, from the title screen to the win, plus 24 joke fee labels for the settlement screen. No line is over 15 words, 117 are 12 words or fewer, and every line is plain ASCII. The runtime fills in tokens for diver number, depth and credits. Each line has a short note for the text-to-speech voice. Every line was checked against the GDD's Cut list, so none mentions the shop, sonar, saving, or the lamp and winch costing Patience. The cut moment has five variants in five different joke shapes, and variant 1 says "Director's cut".
- Why it matters: The Director's writing is the game's personality and its best chance with a judge who plays for five minutes. Because the lines are data, the voice can be tuned and rewritten without touching code. Three of the lines:
  - "Your scene did not make the Director's cut." (the tether is cut when Patience runs out)
  - "Please hold your breath. Your air is important to us." (also the cut)
  - "Of all my divers, you were easily the most recent." (the win)
- Evidence: `deep/Content/Data/DirectorLines.csv`, `deep/Content/Data/SettlementFees.csv`.

### 9:29 PM  The pipeline goes green
- What happened: The build sheriff writes `tools\ue.ps1`, one script that builds, tests, packages and smoke-runs the game, with one PASS or FAIL line per run and full logs in `deep\Saved\Harness`. The first build took 45 s and passed. The first test run failed in 28 s before any test ran: the editor refused to load the `Deep` module. The cause was the C++ class wizard, which had written a class called `Deep` over the module's own `Deep.cpp` and deleted the line that registers the module. It still compiled, so nobody had noticed. One new file, `DeepModule.cpp`, fixed it. The second surprise: the test filter `Deep` matched 27 tests, 26 of them Epic's own tests with "deep" in their names, so the script now matches from the start of the test name. A deliberately broken test and a deliberate compile error both came back FAIL with the file, line and message.
- Why it matters: Agents can now prove their work in about 25 s (build about 5 s, headless tests about 18 s), and the gameplay engineer is unblocked. The MCP toolsets are switched on in the project too (Epic's fix for known issue UE-412135).
- Evidence: `.\tools\ue.ps1 verify` gives `PASS verify (21.9s)` with 1 of 1 tests passed. Logs and report in `deep\Saved\Harness\test-20261114-212927-report\index.json`. See [[Validation Loop]].

### 9:37 PM  First commit, three hours in
- What happened: Quinn gives Claude full permission to commit and push. The first commit goes to GitHub: 50 files covering the project, the pipeline script, the Director's lines, the vault and the agent role files. It is taken at the exact state the pipeline agent proved green, before the gameplay engineer had written a file. The staged files are scanned for keys and perk codes before the push.
- Why it matters: From here every green checkpoint is a commit, so there is always a working build to fall back to, Colby can clone, and the history becomes part of this dev log.
- Evidence: commit `487d2da` on `main`.

### 9:43 PM  The rules exist and are tested
- What happened: The systems engineer writes every GDD rule as a pure static function in `Sim/DeepRules` (depth and zones, tether solve and winch, weight and swim, Patience, settlement, fees, hull rating, bailout air) with every number in one `UDeepTuningSettings` config class. Twelve new automation tests cover them, including edge cases: 40% of 1 credit rounds up to 1, settlement never goes below zero, a step that straddles the bailout slow window, a zero-length tether. One test checks the shipped tuning against the design itself: pearls must sit just out of reach on the stop-1 tether, one heavy piece must overload the diver, the winch must beat the sink speed. The first build and test run passed with no fixes.
- Why it matters: The numbers will change all weekend. A change is now a config edit, and the design invariants test catches a tuning change that breaks the first 90 seconds.
- Evidence: `.\tools\ue.ps1 verify` gives `PASS verify (51.5s)`, 13 of 13 tests passed. Report in `deep\Saved\Harness\test-20261114-214329-report\index.json`.

### 9:50 PM  The Director becomes the Captain, and gets nasty
- What happened: Quinn renamed the submarine AI from "the Director" to "the Captain": "sarcastic, kind of an asshole". The writer agent renamed DirectorLines.csv to CaptainLines.csv and rewrote all 138 lines and half of the 24 settlement fee labels. Ids, events and variant counts are unchanged.
- Why it matters: The phrase "Director's cut" stays in the game as the Captain's excuse: the Director is the unseen boss who takes the 40 percent and orders the tether cut, and the Captain blames him while plainly enjoying it. That keeps the jam theme audible in first salvage, bank, cut and win lines.
- Evidence: Best new lines. "Orders from the Director. Direct quote: Cut it. I am just following them. Happily." / "The pressure crushes lesser divers. Let us see which you are." / "Net is empty. Much like your skull." Commit in git log for CaptainLines.csv.

### 9:53 PM  The game can ship, 23 hours early
- What happened: A second build agent packages the game in its own checkout while the gameplay engineer keeps building in the main one. Package passes in 5 minutes 12 seconds, the packaged game starts and runs clean for 25 seconds, and the zip is 449 MB against the jam's 1 GB limit. Claude Code's built-in worktree isolation refused the repo, because the session was opened as `deep-megajam` and the folder on disk is `Deep-MegaJam`, so the orchestrator made the checkout by hand with `git worktree add`.
- Why it matters: The most common way to miss a jam is a game that was never packaged until the last hour. Ours has been packaged before it has any gameplay. One warning from it: an empty game is already 729 MB on disk, so size needs watching.
- Evidence: commit `dc33aae` merged into `main`; run 17 in [[Agent Runs]] (Sonnet at medium effort, 67k tokens, 10 minutes).

### 10:04 PM  The first playable slice, built from nothing
- What happened: All in C++ with no assets. Press Play on an empty map and the game spawns a submarine made of engine basic shapes, ten salvage pieces in a ring below it, an abyss floor, and water light and fog. The diver swims in first person and can grab and drop salvage. Weight drags the diver down, and one heavy piece is enough to stop the diver swimming up. The tether stops the diver at 40 m and shows as a stretched yellow cylinder. Holding R winches the diver home at 7 m/s, and F toggles the lamp. The HUD shows depth, zone, cargo, tether left and a grab prompt. Input is built in code with Enhanced Input. Console commands `Deep.State`, `Deep.GiveSalvage`, `Deep.GotoStop` and `Deep.Teleport` are there for tests. A world test builds the arena in a blank world and checks a full dive: the tether clamp, the winch, a grab and a drop, a stop change, and `DEEPSTATE` JSON that parses.
- Why it matters: The core loop of grab, get heavy and winch home exists and has tests, and every number is still config. Surprises about the preview engine: only one compile error in the whole slice (strict shadowing, C4458). The Entry map has a PlayerStart, which the game mode ignores and spawns under the airlock instead. A test world needs the game mode's `StartPlay` called by hand before actors begin play. The build script refused twice because the packaging worktree had its own editor open, even though that editor did not lock our DLL, which cost about 12 minutes of waiting.
- Evidence: `.\tools\ue.ps1 verify` gives `PASS verify (41.9s)`, 15 of 15 tests passed. Report in `deep\Saved\Harness\test-20261114-220341-report\index.json`. Code map in `deep\Source\Deep\ARCHITECTURE.md`. Nobody has played it yet: it is unplayed until someone presses Play.
### 10:05 PM  We can drive the editor from a script, and added our own tool
- What happened: UE 5.9's built-in MCP server answers on a port we pick (8012 here). `tools\mcp.ps1` is a small command-line client for it, so any agent can start Play In Editor, run a console command, read the log and take a screenshot without a Claude MCP connection. The live editor reports 106 toolsets (about 840 tools), including a Fab toolset. We then added `DeepToolset`, an editor-only C++ module with one tool, `GetState`, and called it through the same client. The editor warm-booted to a listening port in about 50 seconds.
- Why it matters: Playtests and "does it look right" checks can run from a script, and later gameplay state (oxygen, trust, depth) can be exposed to agents with a few lines per tool. Gotchas found: toolset names are `Module.Class`, and the viewport capture needs `"world": null` passed explicitly.
- Evidence: branch `tools/mcp`; screenshot `deep\Saved\Harness\mcp\CaptureGameViewport-20261114-215845-957.png` (not committed); note [[Unreal MCP Live Catalog]].

### 10:40 PM  The sea serpent exists, and hears you through Unreal's own AI
- What happened: The monster is a sea serpent built from about 20 instanced engine spheres that follow the path its head actually swam, tapering to the tail with a travelling side-to-side weave. It has two unlit glowing eyes, so you see a pair of points in the dark before you see the body. No animation assets. Halfway through, Quinn asked whether it could use Unreal's game AI. Now the serpent is a Pawn possessed by an `AAIController`, and it hears through the engine's perception system with the hearing sense: winching, sprinting and grabbing turn into engine noise events. The decision is still one pure, tested function with five states. Lurk circles beyond lamp range. Stalk spirals in on the tether when noise passes 60. Bite holds the line for a 5 s alarm. Recoil happens when the lamp is held on its head for 0.75 s. Hunt happens only if you have no tether. We kept it off Behavior Trees, StateTree assets and the navmesh: those are binary assets we cannot test headless or merge, and this is open 3D water. The work is purely additive under `Monster\` so it merges with the branch that adds the cut and death, which will bind to its events.
- Why it matters: The monster is fair by construction, and the tests prove it: it never severs a tether without the full 5 s alarm, the beam always breaks a bite at once, and silence always calms it. Gotchas found: a bare test world has no AI system (`World->CreateAISystem()`), and perception delivers a noise only once world time has moved past it, so the test advances `World->TimeSeconds` itself. The harness on `main` still refuses to build while Quinn's editor is open on another checkout. The fix is on `dev` (e374c6b), so this run used a temporary copy of that script.
- Evidence: `verify` gives 20 of 20 tests passed (5 new: `Deep.Sim.Monster.Noise/Beam/States/Steering`, `Deep.World.Monster`). Report in `deep\Saved\Harness\test-20261114-223925-report\index.json` on branch `dev/monster`. Nobody has seen it move yet: the weave, the eyes and the approach are unverified by eye.

## Cut list

The theme is Director's Cut, so we keep the list of what did not make the cut and why.

| Cut | Status | Why |
|---|---|---|
| Co-op, Steam and proximity voice | Proposed cut for the jam build | Judges play alone. Netcode multiplies the work on every system. The plugins have no builds for 5.9 Preview. See [[Co-op, Steam and Voice]]. |
| NVIDIA ACE | Cut | The on-device models are several gigabytes against a 1 GB build limit, they need an RTX GPU on the judge's machine, and the plugins target UE 5.7. |
| A live language model in the shipped build | Proposed cut | A judge would need a key and a connection, and a key inside the package is a leaked key. Scripted lines with pre-generated voice replace it. |
| Claude's first C++ scaffold | Rejected by Quinn | Written before the idea was settled and before Quinn had made the project. |
| Gear upgrade shop (five items) | Proposed cut, 9:10 PM | A whole screen of UI. The hull rating is bought automatically at settlement instead. |
| A unique third level | Proposed cut, 9:10 PM | One arena kit with three light and fog settings. The relic's weight makes stop 3 different. |
| Lamp and winch costing Patience | Proposed cut, 9:10 PM | Two more rules to build, tune and explain. The beam only works where it is pointed, which is limit enough. |
| Tether gun, sonar, DLSS, controller support, PCG terrain | Proposed cut, 9:10 PM | None is needed to finish the loop. |

## Numbers worth quoting

- 10.3k people had joined the jam and 155 entries were in at about 8:20 PM on Nov 14.
- About 26 hours from the first note in the Google Doc (6:43 PM Nov 14) to the deadline (8:59 PM Nov 15). About 24 and a half hours from the start of the Claude session.
- We started on the evening of day three of a four-day jam.

## Capture as we go

Take these when they happen. They cannot be recreated later.

- [ ] Screenshot of the first level built by an agent.
- [ ] Screenshot or recording of the first AI playtest.
- [ ] Screenshot of the first settlement screen, with the Director's cut on it.
- [ ] Before and after of the underwater look.

## Tools used so far

Keep this in step with the "Tools you used" list in [[Submission Checklist]].

- Unreal Engine 5.9 MegaJam Preview
- Claude Code
- Obsidian
- Google Docs
