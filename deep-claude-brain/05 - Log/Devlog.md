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
### 10:25 PM  First look at the game, by a script
- What happened: The tools agent gave `DeepToolset` three real tools (`GetState`, `Exec`, `GetArena`) and wrote `tools\playtest.ps1`, which starts Play In Editor, presses keys, asserts on the state JSON and screenshots the viewport. All five scenarios (boot, grab, overload, tether, winch) pass in about 2 minutes. This is the first time anyone has seen the game running, and the viewer was a Claude agent looking at the PNGs. It is bright flat teal fog with no visible submarine or salvage, a glowing vertical tether rod, and a readable but tiny HUD. It does not look like 300 m down.
- Why it matters: The loop (build, play, assert, look) now runs with no human and no model tokens, so every later task can be gated on it. It also found a likely game bug: winching from beside the sub at anchor height stalls 7.4 m out instead of reaching 3 m; from below it arrives in 8 s. Gotchas: PowerShell 5.1 strips quotes passing JSON to a child process, and the state JSON has no world positions, hence `GetArena`.
- Evidence: branch `tools/playtest`; screenshots `devlog\media\2026-11-14-greybox-S0-boot.png` and the four siblings; notes [[Validation Loop]] and [[Unreal MCP Live Catalog]].

### 10:47 PM  Lights on: the grey box you can actually see
- What happened: The engineer fixing the playtest findings on `dev/loop` was stopped by accident mid-task. A fresh one found its work uncommitted but finished in the working tree: a winch that reels to a point just under the airlock (and drags through the hull if a sweep is blocked), a HUD 1.7x bigger on dark panels with a flashing warning banner, and a thinner, darker tether. It built and passed, so it was kept and committed as is. Then, on Quinn's "no environment yet, just raw gameplay": fog shafts off, fixed manual exposure, a key light from above plus a fill from below, a mid-grey floor, motion blur off, and only a mild light tint per stop. The old water look stays behind one switch (`Arena.bGreyBox`). New cheat `Deep.Look <pitch> [yaw]` aims the camera for scripted shots.
- Why it matters: Every salvage piece, the sub, the tether and the floor now read at a glance, so the loop can be judged on play, not on haze. The run-loop test now also winches home from alongside the hull and from above the sub, and plays a whole dive at stop 2 (grab, cut, home with cargo, settle, dive again, die, next diver stays at stop 2). Gotcha: `FLinearColor::GetMax` counts alpha, which silently turned the per-stop tint into black until a test caught it.
- Evidence: commits `8cae423` (leftover fixes), `8cb383b` (grey box); `verify` 21/21 tests; shots in `deep\Saved\Harness\shots\` (`down-20261114-224245.png`, `up-20261114-224305.png`, `settle-…`, `cut-…`).

### 11:04 PM  The serpent meets the loop, and we see it for the first time
- What happened: The serpent branch merged `main` with no conflicts, then got wired into the run loop. The run component now reports noise for the winch, sprinting and grabs, and the brain stopped watching divers itself, so nothing is counted twice. A bite puts up a full-width red band with a countdown ("IT HAS YOUR TETHER, LAMP ON IT [F], CUT IN 4.2 s"). If you do nothing, it runs the same Cut that Patience uses (`cut_bite`). A catch runs the same death and respawn ("Taken by the serpent"). A red edge pulse and a SOMETHING NEAR bar follow the threat value, so you can tell it is closing with the sound off. Patience drains twice as fast while it stalks or bites you. The Captain speaks `monster_first_heard`, `monster_near`, `monster_repelled`, `bite_alarm`, `cut_bite` and `death_monster`. Then came the first look, through five scripted frames. It is plainly a sea serpent: a tapered, segmented, dark green body that curves into an S as it turns, with a jaw and glowing eyes. One frame was a bite we had planned to capture, but the lamp broke it, as the rules say it must: the helmet lamp was on and aimed up the tether at it.
- Why it matters: Fairness lives in code and tests, not just in tuning. A sever with no alarm on screen is refused, and the beam always breaks a bite. What we fixed after looking: the eyes were a faint glint at 7 m, so they are now half the head radius. New tooling: `DeepShot` takes `wait <s>` between commands, and there are new cheats `Deep.Monster.Place <m> [deg]` and `Deep.Lamp <0|1>`. Not checked by eye: the red edge pulse looks more pink than red on the bright grey box. Sound is not wired.
- Evidence: `verify` 27/27 tests passed. The new `Deep.World.MonsterLoop` checks four things: an unbroken bite gives exactly one Cut, the lamp releases it and the tether stays, a caught diver dies and comes back as the next diver number, and there is no serpent at stop 1 or after a reset. Frames: `devlog\media\2026-11-14-serpent-1.png` (close side view) and `devlog\media\2026-11-14-serpent-2.png` (bite alarm). Also in `deep\Saved\Harness\shots\`: `serpent-reveal-20261114-230003.png`, `serpent-stalk-20261114-230056.png` and `serpent-bite-20261114-230155.png` (the bite the lamp broke). Branch `dev/monster`.

### 11:27 PM  The game gets a beginning and an end
- What happened: Stop 3 now holds the relic. It is built from basic shapes: two gold cones point to point around a pale core, four teal shards circle it, and it has its own bright light. It weighs 60 kg, so it can only be winched. Banking it wins the run and opens a performance review: divers expended, total gross, the Director's cut, fees, what the diver kept, the time and the Captain's one-word rating, which tops out at ADEQUATE. Then you choose dive again or quit. The game now opens on a title screen (DEEP, "The Director's Cut", a Captain title line, press any key). The first dive shows a big controls card. Esc or P opens a pause menu with resume, restart the run and quit. Restart resets everything, the serpent included.
- Why it matters: A judge can now play from the title to the win. One honest gap: the GDD says the relic is at 600 m, but the stop-3 tether is 80 m from an anchor at 500 m. So the relic sits as deep as the line allows, about 577 m. The fixes we made after looking: the controls bar offered the winch and the cut after the tether was gone, so now it shows only what you can do. The airlock is marked while you carry cargo. The first controls card covered the salvage in the middle of the view and its last line spilled out of the box, so it moved to the right. Menus read raw key presses, so they work while the game is paused.
- Evidence: commit `e742dc9` on `dev/ending`; `verify` 31/31 (new: `Deep.Sim.Rules.ReviewAndRelic`, `Deep.World.RelicWin`, `Deep.World.RestartResets`, `Deep.World.FullRunCommands`, which plays the title to the win using only console commands). Shots: `devlog\media\2026-11-14-title.png`, `devlog\media\2026-11-14-relic.png`, `devlog\media\2026-11-14-win.png`.
### 11:25 PM  The loop has regression tests: nine scripted scenarios
- What happened: `tools\playtest.ps1` gained S5-cut (Patience to the cut, bailout air counting down, re-tether with the 20 fee, then air running out: death fee, respawn as diver 2, empty cargo), S6-bank (gross minus the 40 percent cut minus fees, clamped at zero), S7-descend (hull bought, stop 2, depth 304 m, salvage back) and S8-run (swim, grab, winch home from the side, bank, Space, stop 2 in about 45 s). S4 now winches from the side at anchor height and passes (25.0 m to 3.0 m in 8 s), so the old 7.4 m stall is fixed. Five scenarios flaked while I wrote them and every one was a script fault: a poll missed the 2.5 s descent, a poll missed the 3 s death screen, `Deep.Teleport` sometimes lands wrong so the script now checks and retries, the salvage ring respawns after `ResetRun` so the script asks the arena again. No game code changed. The same hour I found `ue.ps1 package` red (see [[Validation Loop]]): the MCP plugins broke the cook; one `DirectoriesToNeverCook` line in `DefaultGame.ini` fixed it (package 27 s, 706.8 MB; zip 426.4 MB; the Captain's 138 lines load from the packaged build).
- Why it matters: Any agent can now prove the whole loop still works in about 5.5 minutes with no model tokens, and the money rules are checked against the live game, not only the unit tests.
- Evidence: `playtest.ps1 all`: 9 of 9 passed (322 s); frames `devlog\media\2026-11-14-loop-S*.png` (945x632, the editor viewport's shape: the capture tool only takes a maximum size, not a 16:9 crop). In the settlement frame the panel covers the right end of the left HUD box ("SUNLIGHT" is clipped). Note [[Validation Loop]].

### 11:29 PM  The harpoon: the way home, and it costs you
- What happened: Quinn and his brother's idea went in on `dev/harpoon`. The diver carries one harpoon shot per dive. Left mouse fires a visible red bolt on a grey line along the view: 25 m range, 60 m/s, so it is quick, but you can miss. If it hits the submarine it latches and reels you to the airlock at 10 m/s, tethered or not, and the existing re-tether and bank rules take over there. If it hits anything else solid it pulls you to that point; releasing or pressing again lets go. If it hits the serpent, the serpent recoils as if the lamp had driven it off, and the hit counts as loud noise. Every hit on the sub adds one dent to a new `HullDamage` count on the game state, plus a "Hull repair (you shot my submarine)" line of 40 cr on the next settlement. Repairs the haul cannot cover come out of the account, which never goes below zero. The HUD shows ready or spent, crosshair brackets when a target is in range (grey with the distance when it is not), and a message on a latch, a miss, out of range, a serpent hit or a refused shot. Left mouse no longer grabs (E still does). The rule of the build: the harpoon must not make the cut stop being frightening, so you get one shot, limited range and a real fee.
- Why it matters: It gives a cut diver a skill-based way home with a price, without touching the serpent's rules or the Captain's lines. The six `harpoon_*` events fire on the bus now and do nothing until their lines exist. All of it lives in new files under `Harpoon/`, with small hooks in the diver, the HUD, the settlement and the game state, because the ending is being built on those same files at the same time. First frame: the line glowed white where it passed the helmet lamp, so it now uses the tether's lighting channel. Not checked by hand yet: how it feels to aim with a mouse, and multiplayer.
- Evidence: `verify` 31/31 tests passed. The new tests are `Deep.Sim.Harpoon.Rules`, `Deep.World.Harpoon.SubReelAndFee` (a cut diver is reeled home, re-tethered and banked, the fee is itemised, the dent stays), `Deep.World.Harpoon.OneShotPerDive` and `Deep.World.Harpoon.SerpentRecoils`. After merging the ending: 35/35. Frame, taken on the merged build: `devlog\media\2026-11-14-harpoon.png` (cut diver, bailout 28.9 s, line latched on the hull, "LATCHED TO THE SUB - REELING IN", "hull dents 1"). Cheats: `Deep.Harpoon.Fire`, `Deep.Harpoon.AimSub [fire]`, `Deep.Harpoon.Reset`.

### 11:51 PM  The submarine stands still and the sea moves
- What happened: Quinn's two decisions from 11:20 PM (D-022 the sub interior, D-023 the layer gates) went in as one change on `dev/sub`. The submarine never moves now. Salvage, the relic and the floor hang from one arena root, and driving moves that root the other way. The hull became a box that is also the one room. It has a deck with a yellow-rimmed hatch over the tether anchor, a console with a green screen, and a helm at the nose. Every run and every replacement diver starts on the deck, walking. Walking into the hatch drops you into the water, tethered, and Patience starts only then. Swimming up through the hatch, or being winched home under it, puts you back on the deck. Banking moved to the console. The two hull ratings became the flashlight (50 cr, and nobody starts with a lamp) and the hull upgrade (200 cr). The helm drives at 8 m of depth per second and stops at 200, 400 or 600 m by gate, and it says why. If you let go within 5 m of a stop, the sub parks there and that stop's arena comes in. Between stops a red cover locks the hatch. The descent fade is gone.
- Why it matters: This is the biggest change so far, and the old tests carried it. RunLoop, RelicWin, RestartResets, FullRunCommands and one monster test were rewritten for the new flow, not deleted. Their failures found the one real surprise. Dropping through the hatch leaves you inside the "home" radius, so a cut right under the sub counts as getting home. We kept that (it is forgiving), and the tests now swim away before they die. Nobody stands inside a moving object, so there is no jitter to fight.
- Evidence: commit `073c67a` on `dev/sub`; `verify` 35/35 (new: `Deep.Sim.Rules.Helm`, `Deep.World.SubHatch`, which checks walking and swimming both ways, `Deep.World.SubHelmLimits`, which checks each limit, and `Deep.World.SubFixedWorldMoves`: after 5 s of driving the sub has moved 0 cm and the root and salvage have risen 40 m. `FullRunCommands` now runs title, hatch, console, flashlight, helm, hull, helm, relic, console). At about 12:04 AM `dev/sub` merged `main` with the harpoon. A harpoon latched on the sub now reels the diver up into the hatch, where the run brings them aboard (a cut diver pays the re-tether fee). Two harpoon tests were adapted: the console banks, and a drop through the hatch reloads. `verify` 39/39.

### 11:57 PM  First look inside, through a Claude agent's eyes
- What happened: Three scripted shots. The first one inside the room showed only orange wall: the diver stood in the right place but faced a wall. So the run start now puts the diver on the deck looking at the hatch. The nose sphere bulged into the room through the front wall, so it moved outside. The yellow hatch rim now goes through the deck so it can be seen from the water too. After the fixes the room reads at a glance: the yellow hatch labelled "walk in to dive", the helm at the far end, the console screen on the right, and one line saying what to do next. From below, the hatch is a lit yellow frame with the tether running up into it. The helm view shows the depth, a gauge with the three stops and a red limit line, the limit and its reason in words, and "between stops: the hatch is locked".
- Why it matters: A new player can tell where they are and what to do from the room and the hatch shots alone. The helm shot is clear only because of the HUD. The camera under the hull sees mostly grey floor part-way down, so the descent does not yet feel like motion.
- Evidence: `devlog\media\2026-11-14-sub-room.png`, `devlog\media\2026-11-14-sub-helm-descent.png` (172 m, between stops, limit 400), `devlog\media\2026-11-14-sub-hatch-from-below.png`.

### 11:55 PM  Most of our usage was one long conversation
- What happened: Quinn's usage panel tied 83 percent of the last day's usage to not starting fresh, 66 percent to max effort and 33 percent to subagent model choice (the shares overlap). The orchestrator session had run for four hours at max effort, so every one-line board update re-read the whole night.
- Why it matters: the agents were already cheap, with a named model and effort since about 9:20 PM. The orchestrator was the expensive part. The fix was a handoff section at the top of the board so a new session can take over from git branches, project settings that start sessions at medium effort and compact at 200k tokens, and compacting this session. An orchestrator needs a short memory more than a long one.
- Evidence: `84287a7` (handoff), `bc392e5` (settings).

## Sunday Nov 15 2026

### 12:07 AM  You can walk around inside the submarine
- What happened: the interior branch passed a second verification (39 of 39 tests) and merged. One room, a hatch in the floor, a console to bank at, and a helm that drives while the submarine stays still and the sea moves. Depth is gated by the flashlight, then the hull upgrade.
- Why it matters: this was the biggest design change of the night, asked for at 11:20 PM and merged 47 minutes later. It was also the most expensive run so far: 37 minutes and 406k tokens. The orchestrator looked at two frames. The room reads clearly. The helm view does not: flat grey with three specks, so driving does not feel like moving. That is logged as the first item for the next feel pass and is not fixed.
- Evidence: `0acf06d`, `devlog/media/2026-11-14-sub-room.png`, `devlog/media/2026-11-14-sub-helm-descent.png`.

### 12:28 AM  The game makes sound, and an agent that cannot hear wrote every sound
- What happened: The game was silent. Now 43 events and 7 state loops each have a row in `deep/Content/Data/Sounds.csv`, and each row points to one of 15 sounds synthesised in C++ (`Audio/DeepSynth`): ping, snap, alarm, clank, bubbles, click, harpoon, a two-note "paid" chime, and loops for the drone outside (lower and darker with depth), the hum inside the sub, calm breathing, fast breathing when bailout air is under 12 s, the winch, a serpent growl that follows the brain's threat value, and the helm drive hum. Loops fade in over 0.8 s and out over 1.2 s, and are only stopped once they are silent. `title`, `idle_inside` and `harpoon_missed` are silent on purpose, and a test names them.
- Why it matters: Audio is one of the seven judged scores. The agent that wrote these sounds cannot hear them, so it relied on rules a test can check instead of taste. Every buffer goes through one safety pass: NaN to zero, the peak scaled to 45 percent of full scale (30 percent for loops) with a hard limit of 60 percent, a fade-in and fade-out at the ends of every one-shot, and a crossfaded seam for every loop. Nothing is a raw square wave. Final volume is row x group x master (0.8), clamped to 1. Each sound start logs `DEEPAUDIO {json}` even with no audio device, and DEEPSTATE gains `audio.loops`, so a playtest can check what played without hearing it. The first build failed at link: `USoundWaveProcedural` needs the engine's `AudioExtensions` module. A human still has to listen. Nobody knows yet whether the clank sounds like metal or the breathing sounds like a regulator. Replacing a sound means one CSV row: import the file under `/Game/Audio/` and put its path in the row.
- Evidence: branch `dev/audio`; `verify` 44/44 (new: `Deep.Sim.Audio.Generators` (length, peak, silent ends, loop seam, NaN), `Deep.Sim.Audio.Table`, `Deep.Sim.Audio.Coverage` (every event the game can fire has a row or is deliberately silent), `Deep.Sim.Audio.Mix`, `Deep.World.Audio` (aboard to water crossfade, winch, bus one-shots, DEEPSTATE, mute, headless with `-NoSound`)). Headless log: 120 DEEPAUDIO lines, 0 errors. Cheats: `Deep.Audio.List`, `Deep.Audio.Play <event>`, `Deep.Audio.Mute <0|1>`, `Deep.Audio.Reload`.

### 12:36 AM  Green that was not green: the build harness now builds like a committed tree
- What happened: The sound-layer engineer reported a green build. After the commit, the orchestrator's second check went red: `DeepEventBus.cpp` and the new `DeepAudioSubsystem.cpp` both defined `MaxHistory` in an anonymous namespace, and Unreal's build tool joined them into one combined ("unity") file. The cause is adaptive unity: files git sees as modified or untracked compile alone, so the clash was hidden while the audio files were uncommitted. Another engineer hit the same thing an hour earlier. We confirmed it both ways: with a trivial uncommitted edit in the audio file the build passed, and with `-DisableAdaptiveUnity` it failed with the same error.
- Why it matters: "green" meant nothing until the tree was committed, so agents were reporting results the next build could not reproduce. `tools/ue.ps1 build` now passes `-DisableAdaptiveUnity` (confirmed in the engine's `TargetRules.cs`), so every build groups files the way a committed tree does and this class of error shows up before the commit. The audio constant is now `MaxAudioHistory`; the story file was not touched.
- Evidence: failing log `deep/Saved/Harness/build-20261115-003244.log`; `verify` 44/44 on the clean committed tree.

### About 12:50 AM  Fewer tests, in the right place
- What happened: Quinn said we run too many tests on changes that do not need them, and that all tests can run at merge. He is right. One engineer ran the full suite at least three times in a single task, and the orchestrator's last brief asked for the five-minute scripted playtest set three times over.
- Why it matters: the check that caught a red build at 12:31 AM was the one at merge, so that one stays and the repeats go. Agents now build and run only their own area's tests. The full suite runs once per branch, at merge. Notes need nothing. The full suite takes about 30 seconds, so the saving is mostly in how often it runs and in the long checks: the scripted set takes five minutes and a package five to eight.
- Evidence: D-027 in the decision log, `.claude/loop.md` ("What to run").
### 12:58 AM  The scripted playtests are rewritten for the submarine, and the package of the sound layer works
- What happened: Of the nine old scenarios (S0 to S8), eight failed on the new game (the first run was also partly spoiled by an editor still loading, so some of those failures were the editor and not the game). Rewrote them as eight scenarios that assert on game state: `S0-title`, `S1-hatch`, `S2-gate`, `S3-serpent`, `S4-cut`, `S5-harpoon`, `S6-bank`, `S7-run`. Two full passes before merging `origin/main`: 8 of 8 both times (about 7.5 to 8 minutes each). After merging the sound layer only `S1-hatch` (now also asserts `audio.last` and a playing loop: `enter_sub`, `loop_drone_in`) and `S7-run` were re-run, both pass. Packaged the merged tree: 707 MB on disk, 426.5 MB zipped, smoke clean.
- Why it matters: Three of my own test bugs, none a game bug: a diver cut at the hatch is re-tethered at once (so cut tests must swim away first); `Deep.GiveSalvage 40 ...` is typed as the relic because 40 kg matches the relic's mass, so it won the game at stop 2; and a PowerShell precedence slip. Found while testing: `Deep.Start` is needed because a key press sent through the input tool does not leave the title (not checked with a real keyboard). The frames show things for the next feel pass: the helm frame shows "LIMIT 400" overlapping the depth gauge bar, the first-dive CONTROLS panel is still up at the hatch, the console panel shows while looking at the helm, and the win screen shows the status panel faintly through it.
- Evidence: `tools/playtest.ps1`, frames `devlog/media/2026-11-15-play-*.png`, package log `deep/Saved/Harness/package-20261115-005426.log`.

### About 1:10 AM  No half-done polish
- What happened: the orchestrator had just started a batch that patched the placeholder HUD and added grey-box motion cues for driving. Quinn stopped it. The gameplay loop is the work, and polish waits until it can be done properly, to a professional standard. Two of the batch's four items were cut mid-run. The title key press and one state bug stayed, because they are function.
- Why it matters: it is the second time tonight the orchestrator reached for visuals before being asked. The first was the atmosphere pass at 10:28 PM. Patching a HUD that will be replaced is work thrown away. The rule is now written down for every agent: fix function, not finish.
- Evidence: D-028 in the decision log, `CLAUDE.md`.

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
