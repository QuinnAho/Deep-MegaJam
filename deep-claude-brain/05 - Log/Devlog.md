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

### 1:24 AM  Why the title ignored the playtest's key, and a cut halfway through the task
- What happened: The title already took any key, but only through `ADeepPlayerController::InputKey`, the path a real key takes (game viewport to controller). The input tool sends its key straight to `PlayerInput`, past that function, so its Space reached Enhanced Input, which had nothing for the title. Now an Enhanced Input action mapped to `AnyKey` also leaves the title, and both paths call one function, `TryStartFromTitle`. The title text says "PRESS ANY KEY OR CLICK TO START". `tether_distance_m` read 0 after a cut because the tether solve stops updating; the state report now measures the distance to the anchor. Halfway through, Quinn cut the helm motion cues (specks, shaft walls, a speed readout) and the HUD-per-mode rework: the HUD and the look will be redone properly later, and placeholder polish in between is wasted. That code built and is parked on `parked/motion-hud`, not merged.
- Why it matters: The first thing a judge does is press a key on the title, and now the scripted playtest proves it without the `Deep.Start` cheat. A first version of the new test failed for a test reason: a test world spawns the game mode as a plain actor, so `GetAuthGameMode` is empty. The controller now finds the game mode the same way the cheats do.
- Evidence: `Deep.World.TitleKey` (K, Space, Enter, LMB, RMB, Esc, a gamepad button through `InputKey`; mouse movement does not start), `Deep.World.TetherDistanceAfterCut`, `Deep.World.FullRunCommands` all pass; `playtest.ps1 S0-title` PASS with no fallback note; frame `devlog/media/2026-11-15-feel-title.png`. A physical keyboard is still unchecked.

### 1:32 AM  Looks get the same weight as gameplay
- What happened: Quinn said looks are as important as gameplay and that polish is "a VERY big deal", and asked to see what the team owns and what is online. Three research agents started: one reads his Fab library and free listings through an editor, one searches the web for sound, a voice for the Captain, fonts and look techniques with their licences, and one writes the Unreal 5.9 plan with work packages.
- Why it matters: at 1:10 AM the rule was no half-done polish. This is the other half of that rule: prepare the whole pass first, with its assets, sounds and techniques chosen, so that when it starts it is one piece of work. Nothing visual is being built yet.
- Evidence: D-029 in the decision log; branches `research/assets`, `research/polish-web`, `research/polish-plan`.

### UI feedback from Quinn: too busy, not polished
- What happened: Quinn looked at the playtest frames and said the UI is far too busy with unnecessary information and is not polished at all.
- Why it matters: The settlement card, the always-on left HUD box (depth, cargo, tether, credits, hull, stop, diver number), the key-hint strip and the Captain subtitle all show at once, and the settlement panel overlaps the left HUD. A polish pass should cut to the few things a player needs at each moment (for example air and Patience while diving, the net and one button on settlement) and hide the rest.
- Evidence: `devlog\media\2026-11-14-loop-S6-bank.png`, `devlog\media\2026-11-14-loop-S5-cut-bailout.png`. Not fixed yet; needs a UI task on the Board.

### 2:09 AM  First real-input playtest of the main loop (S8-keys)
- What happened: A new scenario, S8-keys, plays stop 1 on simulated keys only: any key on the title, walk to the hatch and fall through, swim to the nearest salvage and grab with E, swim back up through the hatch, E at the console, E on the settlement screen, walk to the helm, E, hold C 12 s to stop 2, release (parked at 300 m). Then F lamp, R winch, Q cut, LMB harpoon on the cut diver, Esc. All of it works on simulated input except pause. Three snags found on the way: (1) swimming straight at the hatch from the side stalls about 5 m out against the hull, the Captain's patience ran out, the line was cut and the diver died; the diver has to get under the hatch and rise (script does that); (2) the open hatch lies on the straight line from console to helm, so walking it drops the diver into the sea; (3) mouse look turns only weakly on a simulated push (0 to 14 degrees for x=40, once via Mouse2D, once via MouseX): the input tool, not the game, so real mouse look is still unverified by a person. E on the settlement screen leaves it once and does not use the console again (credits stayed 15). Esc cannot be driven: the tool skips PlayerController::InputKey where the menu keys are read, so pause is still untested on real keys.
- Why it matters: A real player can finish the first stop on keys and mouse. The two things only a human can still check are real mouse look and Esc pause.
- Evidence: tools\playtest.ps1 S8-keys PASS in 303 s; setup commands used were Deep.Credits 60 (afford the flashlight), Deep.Look (aim), Deep.Teleport and Deep.Harpoon.Reset (extras); screenshots in devlog\media\2026-11-15-play-S8-keys.png and S8-1..6.

### 2:25 AM  Home to the hatch from any side, and the walks kept off the hatch (dev/fix2)
- What happened: The S8 stall was not an invisible volume: the hull's collision is exactly its visible cubes (a new test traces each side and meets the drawn face). The cause was geometry plus sliding. The hatch is in the floor, only 0.2 m above the hull's underside, and the diver is 1.6 m tall, so a straight swim at it from the side always meets the side wall first; the engine slides the push up that wall to the hatch's height and the diver pins there, 5.05 m out (4.7 m half-length plus a 0.35 m capsule). Fix: pushing into the hull's side now slides the diver down it and under it (`HullSlideVelocity`), and the winch routes round and under the hull toward the point below the hatch instead of dragging the diver through it (`WinchStepAroundHull`). The room's start, enter stand and helm moved to the +Y side next to the console, so no straight walk between any two of them crosses the hatch; walking into the hatch still dives. Mouse look checked, not changed: 0.175 degrees per mouse count (0.07 axis sensitivity x 2.5 legacy yaw scale x 1.0 LookSensitivity), on the fast side of normal, so the tool's 0 to 14 degrees for 40 counts (7 expected) was the tool.
- Why it matters: A player who swims straight back at the hatch from any side now gets home instead of being cut and killed. That was the one blocker in the main loop on real input.
- Evidence: new tests `Deep.Sim.Rules.WinchRoute`, `Deep.Sim.Rules.SwimHullSlide`, `Deep.World.SubReturnWinch` (7 points round the sub, heights -6 m to +7 m, aboard within 6 s, the capsule never inside the hull), `Deep.World.SubReturnSwim` (forward held, aimed at the hatch from behind, beside and a high corner: aboard within 15 s), `Deep.World.SubHullShape`, `Deep.World.SubRoomLayout`. `Deep.World.Sub*` + `Deep.Sim.Rules.Winch*` + `Deep.Sim.Rules.Swim*`: 11 of 12 on the first run; the swim test failed because a bare `World->Tick` in the test world moved nothing, so it now steps the diver, its movement, tether and run by hand, then passed 1 of 1. RunLoop, RelicWin, FullRunCommands, Harpoon and TetherDistanceAfterCut: 7 of 7.
### 4:55 AM  The orchestrator pushed a red merge, for ninety seconds
- What happened: the HUD branch and the monster branch each built on their own. Merged, they did not: the serpent had a small helper called `EaseOutCubic` and so did the HUD, and once the build tool compiled the files together the HUD's calls could not choose. The orchestrator's check should have stopped the push. It did not, because twenty minutes earlier the orchestrator had wrapped the check in a machine-wide lock so that agents would not start Unreal at the same time, and that wrapper threw the exit code away. The output said FAIL and the script said 0.
- Why it matters: this is the same class of mistake the orchestrator caught an engineer making at 12:31 AM, a green that was not green, made by the checker itself. It was caught by reading the output instead of the exit code, fixed by renaming one helper, re-verified at 59 of 59 tests, and pushed green. It never reached the main branch, because nothing merges there until Quinn has seen frames. The gate now requires both a zero exit code and the PASS line, and the corrected command is written into the orchestrator's rules.
- Evidence: `polish/integration` `53b045c` (red) and `c562939` (green); `.claude/loop.md`, "How to run the gate".

### 7:00 AM  The alarm was half wrong, the pack was already here, and two lanes start on it (orchestrator)
- What happened: The ending engineer reported at 6:45 AM. The orchestrator read the diff against the report (three numbers and one small rule, as stated), ran the gate on the branch head (build and 66 of 66 tests, 51 s, exit code 0 and the `PASS verify` line) and pushed it to the integration branch at 6:54 AM. While checking the main checkout it found two things nobody had mentioned. First, an empty stray `playtest.ps1` from 1:46 AM, removed. Second, `deep/Content/DragonCave/`: 4.8 GB, 333 files, written at 3:31 AM. Quinn had installed the dragon pack half an hour after choosing it, through a separate Unreal 5.7 project, and gone quiet. For three and a half hours the Board said "waiting on Quinn to install Dragon Cave" while the pack sat on the disk.
- Why it matters: Two corrections to the record. (1) At 5:50 AM the orchestrator told Quinn the game could not be won. The playtest script was the bug: it winched in bursts. A player who held the winch won every time and never needed the lamp, which is a different fault, and the reverse of the one reported. A scripted player is a witness, not a judge: its failures need the same checking as an agent's successes. (2) The orchestrator only reads what agents report and what Quinn types. Neither covers what a human does on the disk without saying so. A look at `git status` in the main checkout at each tick would have found the pack at once.
- What was done: The pack stays outside git by a line in `.gitignore` and is linked into the three agent checkouts (D-036). Two Opus engineers started at 6:57 AM: `polish/dragon` (the dragon's head on the serpent; the join is the test) and `polish/dressing` (the placement loader, a cook hook so that only used pack assets are cooked, then rock and ruin at the dive sites). The Board's handoff section, which had grown into a history of the night, was rewritten to hold only what is true now.
- Evidence: gate log in the session's scratch folder, test report `.wt\monster\deep\Saved\Harness\test-20261115-065316-report\index.json`; commits `20c77c5` (the fix), `ab814a6` (ignore rule), `4f860e9` (integration head). Decisions D-035 and D-036.

### 8:40 AM  The first build with the pack in it: 1.3 GB, grey rock, and a game that would not compile (orchestrator)
- What happened: Both polish lanes reported within twenty minutes of each other, and the orchestrator looked at every frame. Rock and ruin: the sea floors at 100 m and 300 m work. Every frame of the 500 m site is faint or fails, and the bottom of the relic's shaft reads as a debug view: a white ring with 12,000 units of light in a yellow-green pit. The dragon's head: from the side in plain light it is one animal, and at 6 m in the lamp it is teeth and one eye where the old head was a small fish. The bite still cannot be read and the head burns white at arm's length. Both engineers went back with a written review (D-037 for the relic). Then the orchestrator cut a package, without waiting for the planned checkpoint.
- Why it matters: The package found three faults that 71 green tests and two dozen good-looking editor frames could not.
  1. The zip was 1,318 MB against the jam's 1 GB limit. The 42 placed meshes are 7 MB; the 37 textures they reference are 1.9 GB of source.
  2. The packaged log said the rock's surface was "missing usage flag Nanite! Default Material will be used in game". The editor sets that flag on the fly and a cooked game cannot. The packaged frame still looked plausible, because the default material is grey, and so is silted rock in a lamp.
  3. With the dragon merged the game did not compile at all. A debug line called `UStaticMesh::IsNaniteEnabled`, which exists only in editor builds, and the merge gate builds only the editor.
- What was done: A texture limit in a new `deep/Config/DefaultDeviceProfiles.ini`. It took three tries, each measured with a real package. `MaxLODSize=2048` alone: 1,160 MB, because virtual textures ignore it. `MaxLODSize_VT=2048`: 530 MB, but the pack's big textures are grids of 4096 tiles (the dragon is 20480 by 8192), and a flat limit shrinks every tile to 256 pixels. `LODBias_VT=1`, which drops one mip from every virtual texture whatever its grid: 679 MB, and about 760 MB expected once the dragon's head is in. The answers came from reading `TextureLODSettings.cpp` in the installed engine and from a 60-line script that reads each texture's size and type out of its file header, not from memory of how the engine works. The flag and the compile error went to their engineers as first jobs, and the dragon merge is held back until the game builds. From now on a branch with C++, materials or pack content is packaged when it merges (`.claude/loop.md`).
- Evidence: package logs `.wt\monster\deep\Saved\Harness\package-20261115-08*.log`; commits `71eaaa8` (rock merged, gate 71 of 71), `b3a184c` (texture limit, packaged and smoke-tested); frames `devlog/media/2026-11-15-dressing-*.png` and `2026-11-15-dragon-*.png` on the lane branches; D-037.

### 9:00 AM  The well reads, the dragon's head is in, and the packaged game now matches the editor (orchestrator)
- What happened: The compile fix for the dragon's head came back ten minutes after the fault was reported, and the head merged at 8:47 AM (`6746147`). The second pass on the 500 m site came back at 8:53 AM and merged at 8:56 AM (`5ae5f0b`). Each merge ran the full gate and a package, and the orchestrator took frames from the packaged game, not only from the editor: the dragon's head in plain light, a carved pillar at the mouth of the well, and the bottom of the well.
- Why it matters: An hour ago the bottom of the relic's shaft was the worst frame in the project, and it is the one place every winning player reaches. Now the relic is a carved stone sphere among gold and bone, with its light rising up a well of cut stone, and from the hatch the glow is the beacon. The cause of the black pillars was not light at all: the surface had picked up the wrong textures. Asking "why is it dark" before adding a light saved a wrong fix.
- What was done: Verdicts written for every frame and published on the frames page. A third lane started at 8:58 AM on the tether, which is the game's central object and is still a stretched cylinder: a rope that sags, goes taut and visibly parts when it is cut, with bubbles and silt. The dragon lane is on the bite and the skin at arm's length.
- Numbers: zip 762 MB of 1,024 allowed. 71 of 71 tests. The rock costs about 0.5 ms a frame and the dragon's head about 0.8 ms. The second pass on the well cost 691k tokens in 42 minutes, because a resumed agent carries its whole history; the first pass cost 492k in 66.
- Evidence: gate and package logs under `.wt\monster\deep\Saved\Harness\`; frames `devlog/media/2026-11-15-well-*.png` and `2026-11-15-dragon-*.png` on the integration branch; packaged frames in the session's scratch folder and on the frames page.

### 10:40 AM  Five causes for one white disc, and three ways the agents got in each other's way (orchestrator)
- What happened: The lamp lane came back at 10:11 AM with five causes for the burn, where the brief had guessed at one. The fifth was nobody's guess: the relic's pit was a funnel, so a diver standing at the relic had its slope in his face, and the lamp was lighting a wall half a metre away. The pit now has a flat floor and the lamp samples its whole cone. The hull lane came back at 9:56 AM with a submarine that reads as a machine from below. The rope merged at 9:52 AM. After each merge the orchestrator ran the gate and a package, and at 10:21 AM the whole scripted playtest set.
- Why it matters: All 14 playthroughs pass, and one of them had never passed before. `S10-cutdrop`, a diver cut at the very bottom of the relic's shaft, failed 2 of 2 on Saturday's layout and was on Quinn's list as a question only he could answer by hand. The flat floor that fixed the lamp also let the diver swim out. A fix for how a place looks changed how it plays, for the better, and only the playtests showed it.
- What went wrong, three times, all between agents and none in the game:
  1. A package failed with "Unable to build while Live Coding is active". An editor starts Live Coding by itself, and UnrealBuildTool then refuses every build that uses the same installed engine, in every checkout. One lane's editor, open for asset work, had stopped every other lane's builds. The editor launch now passes `-LiveCoding=0`, read from the engine's own start-up code.
  2. An engineer closed its editor, another took the same port, and the first sent its next batch of calls into the second's editor: it started and stopped Play there. It reported the mistake itself. Each lane now gets its own port.
  3. The orchestrator left its playtest editor open on its merge checkout, and its own next merge failed half-way on two locked files. The build tool refused to run on the half-merged tree, so nothing wrong was built.
- What was done: The bubbles were tried a second way, as mirror spheres, and rejected by the engineer who built them; a third engineer is building them as particles. The smear on the trench rock is accepted: the engineer proved the rock writes correct velocity, withdrew its own earlier reading, and stopped at a project-wide setting as the brief said. A new lane is walking the first five minutes frame by frame, because a judge sees the opening first.
- Evidence: integration at `a942e1c` (gate 77 of 77, package at `5a5f8ab`, zip 762 MB); playtest log in the session's scratch folder (13 passed and 1 failed in 732 s, then `S3-serpent` alone, passed); frames `devlog/media/2026-11-15-ascent-*.png`, `-hull-*.png`, `-rope-*.png` on the integration branch.

### 11:19 AM  The serpent was never in the well, and then the machine went down (orchestrator)
- What happened: Three lanes reported within three minutes. The well lane found that during the last ascent the serpent had never once been in view: it did not know the well existed, so it circled 45 m out inside the solid rock of the sea bed, and the lamp test, which has no line of sight, was driving off a head nobody could see. The scripted ending had been passing against an invisible monster. Its steering now keeps it inside the shaft, coming up from below head first. The opening lane gave the bite in the dark its own light, a glow under the jaw in the colour of the serpent's spots, because the hatch light reaches only 3.8 m and a bite happens 8 m out. The effects lane built bubbles the right way this time (Niagara, a lit translucent rim) and still shipped nothing: a bubble breathed out at the mask is never inside the lamp's narrow cone, so no frame passed.
- Then, at 11:19:45 AM, the PC reset without shutting down (Kernel-Power 41, no bugcheck). The orchestrator and all three engineers went with it. The well merge had just passed its gate (78 of 78), its package, smoke run and zip (762 MB); the orchestrator's playtest editor had started 27 s earlier while the effects engineer, already sent on a fourth try, had its own editor open. The cause is not proved. Two Unreal editors at once breaks the board's own rule for a 32 GB machine, so it is the rule again, for the orchestrator too.
- Why it matters: Everything committed survived, which is the argument for committing at every green step. The green but unpushed merge was pushed by the next session at 11:31 AM. Lost: the three engineers' context, the ending playtests on the new build, and the effects engineer's half-built fourth try, which is on disk but uncommitted. A new session picked up the state from git, the board and the tail of the old session's log in about ten minutes.
- Evidence: integration `b7755a1`; `.wt\monster\deep\Saved\Harness\package-20261115-111700.log`; frames `devlog/media/2026-11-15-wellserpent-*.png`, `-opening-*.png`, `-fx-try3-*.png` on the lane branches; Windows System log, Kernel-Power 41 and EventLog 6008 ("previous system shutdown at 11:19:45 AM was unexpected").

### 12:02 PM  Quinn changes the game: drive the sub down caves by sonar (orchestrator)
- What happened: At 11:44 AM the first-five-minutes pass merged cleanly (one conflict, in a notes file): verify 78 of 78, package and smoke green, zip 762 MB. Then Quinn rethought the game: "what makes this game fun and driving the sub with a radar mechanic or something is part of it really also i think we must do caverns because if your swimming in an open ocean it really lessens it". Hitting walls is what should damage the hull.
- The orchestrator pushed back on part of it. Free driving through branching tunnels would rewrite how the sub, the tether, the stops and the ending work. Today the sub never moves; the whole world slides past it. It offered a version that keeps all of that: between stops the descent becomes a winding cave shaft that you steer through sideways; the sonar shows the walls around you and 20 m below; every stop becomes a cavern. Quinn also asked why features froze at 3 PM. They don't have to: that was our own buffer, and the jam's only deadline is 8:59 PM. The freeze moved to 5:30 PM (D-038, D-039).
- Three Opus engineers started at 12:02 PM, one each for the shaft, the caverns and the sonar, in three checkouts. Before writing a line they agreed one contract: at every stop the passage is centred on the parked sub with a 14 m radius; every cave surface answers the sonar's traces; each part has a switch that gives back today's game. Whatever isn't solid by 5:30 PM ships switched off.
- The machine crashed at 11:19 with two Unreal editors up, so four engineers and the orchestrator now take turns: every build, test and frame waits for one shared lock. At 11:47 AM a small sonnet fix (coming aboard, the view levels instead of staring at the ceiling) got its 10-second build and 23-second test into the gap between the orchestrator's package and its playthroughs.
- Cut to make room: bubbles. The fourth try was half-built when the machine went down. It is committed, unfinished, on its own branch.
- Why it matters: this is the biggest design change since the submarine room, made nine hours before the deadline. The switches are what make it a bet we can afford.
- Evidence: integration `ad644df` (`.wt\monster\deep\Saved\Harness\package-20261115-114533.log`, 0 usage-flag lines); `polish/aboard` `2f27da3`; `polish/fx-wip4` `f7a3fce`; D-038 and D-039 in [[Decision Log]].


### 12:40 PM  The Captain gets a radio and fewer lines, the sonar ships, the camera gets a body (orchestrator)
- **What happened:**
  - **Quinn's questions.** Just after noon Quinn asked two design questions. The first: "so what do you think we make the game fun look at game psychology ect how do we make this feel less hacky more professional and polished". The second: "the capitian feature doesnt really bring anything in here too how hsould we change that".
  - **What reads as hacky.** The orchestrator named three things:
    - every sound is synthesized in code;
    - the Captain is subtitles with no pacing;
    - the look still has leftovers.
  - **Keep the Captain, change how he talks.** He carries the theme, so the orchestrator argued for keeping him and changing how he talks. Quinn said "Captain direction yes", and offered a Fab sound pack and his brother's music and sounds.
  - **The Captain lane and the writer.** Two agents started at 12:17 PM: an Opus engineer for the Captain's radio, pacing, music and pack sounds (`polish/captain`), and a Sonnet writer.
    - The writer took 2.4 minutes to cut 185 lines to 74 (calm 50, tense 19, scared 5), with jokes only where the player is safe.
    - Its brief said 45 events. It counted 55 in the file, and it found that `{credits}` was never a token.
    - At 12:26 PM it took every number out of the voiced lines in 36 seconds, rewriting 25 lines. The receipt, the depth readout and the diver number already show those numbers on screen.
  - **The sonar landed first.** The sonar lane committed its first version at 12:14 PM and its last at 12:23 PM, an hour and a half before its first deadline. At the helm it draws a sweeping plan view of the walls round the sub, and it costs 0.069 ms a frame.
    - It cannot show walls yet, because the trench has no collision. The shaft and cavern lanes add walls the sonar can see.
    - It merged into integration at 12:26 PM. The gate then ran in five minutes: verify, a package with no usage-flag lines, smoke and a 799 MB zip.
    - Four scripted playthroughs passed: `S0-title` (13 s), `S2-gate` (35 s), `S6-bank` (14 s) and `S7-run` (49 s).
    - It was pushed at 12:33 PM.
  - **Quinn's next ideas.** Around 12:20 PM Quinn sent more: lidar beams to see in the dark, a HUD that matches the theme, camera shakes, breathing, post-processing and a found-footage camera.
    - The orchestrator folded the lidar into the sonar: the ping made visible in the world, not a second tool.
    - At 12:25 PM it started an Opus camera engineer for the shakes, the footage look and the pulse. Every part is behind a switch, off until Quinn approves frames.
    - The HUD pass waits until the sonar and shaft lanes merge, because both changed the HUD.
    - When Quinn said he might own a camera-shake pack, the orchestrator said it wasn't needed: Unreal shakes are a few numbers on Epic's built-in patterns. The one exception would be recorded handheld motion.
- **Why it matters:** Audio and Theme are two of the seven judged scores, and both were weak in the same place, the Captain. The camera idea makes the theme something the player sees: the Captain watches through your helmet camera and makes his cut.
- **Evidence:**
  - D-040 and D-041 in [[Decision Log]];
  - [[Captain Script]] and [[Sound Brief]];
  - integration `ea29122`, with the logs in `.wt\monster\deep\Saved\Harness\` (`test-20261115-122703`, `package-20261115-122726`, `smoke-20261115-122821` and `zip-20261115-122847`);
  - sonar frames `devlog/media/2026-11-15-sonar-parked300.png` and `devlog/media/2026-11-15-sonar-driving.png` on `polish/sonar`.

### 12:56 PM  The Captain lane finishes hours early, and the sound waits for real files (orchestrator)
- **What happened:**
  - **All four steps done.** The Captain-and-sound lane (Opus, high) reported at 12:43 PM, 26 minutes after it started. Its last step was due at 4:00 PM.
    - The Captain talks over a radio, with squelch, a band-pass and ducking. He holds his tongue while the diver is in the water, and his tone follows the danger.
    - Music has a slot for each depth, a tension layer while bailout air runs, a sting at the serpent's first sight, and title, settlement and win cues.
    - Breathing follows the danger: calm, then fast, then panic over a heartbeat.
    - A one-shot sound picks from up to three variations at ±3% pitch, and never plays the same one twice in a row.
  - **Silent until the files come.** The lane wrote no generated music, so a slot with no file stays silent. The serpent's rows still point at the synth, pitched down to 0.55 to 0.7; each needs a file path and pitch 1.0 when the files arrive. It imported nothing, because Quinn has not said whether agents may.
  - **The cook.** It takes `/Game/DeepAudio` whole, plus any pack sound `Sounds.csv` names. Today's package log says `DEEPCOOK added 0 pack sounds named by Sounds.csv`, which is right, since no pack is installed yet.
  - **A clash between lanes.** The lane merged the sonar work in before it reported. The sonar lane and the new script had each written the Captain's line for a sonar contact, in different column formats. It kept the script's six-column line and dropped the sonar lane's row.
  - **The merge.** The orchestrator merged the lane into integration with no conflicts (`6aeafc8`, 12:47 PM). The gate passed:
    - build in 18 s and 94 tests;
    - a package in 58 s with no usage-flag lines;
    - smoke in 26 s and a 762.5 MB zip.
  - **Playthroughs.** Four scripted playthroughs passed: `S0-title` (15 s), `S6-bank` (14 s), `S7-run` (49 s) and `S10-relic` (77 s). The merge was pushed at 12:56 PM, and the shaft, cavern and camera lanes were told to merge it before they report.
  - **The HUD mockup starts early.** At 12:49 PM the orchestrator started a Sonnet agent on a HUD mockup with two directions on real frames. Direction A is the style guide's instrument HUD done properly; direction B is the diver's helmet-camera display, with a battery bar that is the Captain's patience. It uses no Unreal, so it does not wait for the slot.
- **Why it matters:**
  - Sound is a judged score, and its code is ready before the files exist. A missing file leaves a slot silent instead of breaking the build, so the brother's files can arrive right up to the freeze.
  - The HUD mockup lets Quinn choose the HUD and the footage look together at 2:15 PM, instead of one decision after the other.
- **Evidence:**
  - integration `6aeafc8`, with logs in `.wt\monster\deep\Saved\Harness\` (`test-20261115-124837`, `package-20261115-124910`, `smoke-20261115-125008` and `zip-20261115-125034`);
  - the lane's own three entries in this note on `polish/captain`;
  - rows 79 and 82 in [[Agent Runs]].

### 1:13 PM  Four lanes report in twelve minutes, each an hour or more early (orchestrator)
- **What happened:**
  - **Four reports.** Between 1:01 and 1:13 PM four lanes reported, each an hour or more before its first deadline:
    - the cave shaft at 1:01 PM;
    - the caverns, merged into integration at 1:04 PM;
    - the HUD mockup at 1:11 PM;
    - the camera at 1:13 PM.
  - **The camera lane** built three looks, each behind its own switch. Only the shakes are on.
    - **The found-footage lens:** a slight barrel, darker corners and soft edges, for about 0.2 ms a frame. On its own it is subtle. It reads as a camera only with the HUD's direction B, which carries the record dot and the timecode.
    - **The sonar ping as light:** Quinn's lidar idea, folded into the sonar. An amber band runs out over the rock as the ping passes, for about 0.25 ms while a ping is out. In open water it shows nothing, because there is nothing to light.
    - **Interference:** snow, a colour split and a tear as the serpent closes, and fully in a bite, for about 0.3 ms.
    - **Two faults, both found in its own frames.**
      - The first interference drew diagonal stripes. A noise function lost precision on the GPU, and a different noise fixed it.
      - The ping lit the inside of the helm window before it reached the rock, so a brown frame flashed round the view. That fault went back to the engineer at 1:26 PM, along with a second job: hull hits must shake the view.
    - **Motion blur:** stills at 0 and at 0.2 looked the same, so it stays at 0.2, and only inside the footage look.
  - **The HUD mockup** drew two directions over four real frames:
    - A, the style guide's instrument HUD done properly;
    - B, the diver's helmet-camera display.
    B costs about an hour more to build. Quinn picks one when he decides on the footage look.
- **Why it matters:** the lanes ran in parallel, each behind its own switch, so four pieces were ready before the 2:15 PM decision point. Quinn can decide from frames instead of from descriptions.
- **Evidence:**
  - `polish/camera` `6be7d0b`, `3741010` and `36da416`, with the recipe and its sources in `03 - Research/Camera - Footage Look and Sonar Pulse Recipe.md` on that branch;
  - [[HUD Mockup Spec]];
  - rows 76, 77, 81 and 82 in [[Agent Runs]].

### 1:38 PM  The serpent stays in the cavern, the orchestrator raises a false alarm, and the roof comes down (orchestrator)
- **What happened:**
  - **The serpent could swim through rock.** With walls round each stop, nothing kept its body off them. The cavern engineer clamped its lurk circle and body to keep 3 m off the rock, plus 1.5 m of body reach. In four-minute runs at stops 2 and 3, none of 32,160 sampled body spheres came within 3 m of the rock (`e50e11a`).
  - **A false alarm.** Out of the hatch at stop 2 the frame was nearly black. At 1:28 PM the orchestrator told Quinn that "the sonar suggests the sea bed may be out of reach too". The engineer measured it instead:
    - the bed is 8 m under the hatch;
    - the farthest salvage is 44.6 m away on a 60 m tether;
    - at stop 3, the relic is 77 m straight down on an 80 m tether.
    The orchestrator corrected its message to Quinn and the frames page.
  - **The real problem is the look.** From the hatch the nearest wall is 60 m or more away, and the lamp lights 35 m. So the dive still looks like open water, which is what the caverns were for.
  - **The engineer stopped instead.** Asked to bring rock into view, it stopped without a commit, as its brief allowed, and explained why:
    - the walls can't come nearer than 47.6 m: the farthest salvage spot plus its margin;
    - a bell-shaped cave would pull the serpent's lurk circle into the lamp at the start of the dive;
    - pillars from the dressing data have no collision, so the tether would pass through them.
  - **The call:** a lower roof, so rock hangs about 15 m overhead from the hatch, while the serpent's first sighting stays in the dark (D-042). Building since 1:38 PM.
- **Why it matters:**
  - The engineer was told to stop if the change would break something, and it did, with numbers.
  - The orchestrator's mistake came from reading a dark frame; the engineer settled it by measuring.
- **Evidence:**
  - `polish/cavern` `e50e11a`, `c5607fa` and `ef84d37` (26 of 26 tests), and `DeepCavernSerpentTests.cpp`;
  - frames `devlog/media/2026-11-15-cavern-serpent-s2.png`, `devlog/media/2026-11-15-cavern-serpent-s3.png`, `devlog/media/2026-11-15-cave-join-hatch.png` and `devlog/media/2026-11-15-cavern-wall-s2-lamp.png`;
  - D-042 in [[Decision Log]].

### 1:41 PM  The shaft passes the gate, after an engine cache took the collision from its rock (orchestrator)
- **What happened:**
  - **The descent is now driving.** Between stops the sub follows a winding rock shaft. You steer sideways with WASD while the sonar reads the walls, and a run at the rock costs hull.
    - The Captain hears the hull at the helm: below half he turns tense, and below a quarter scared.
    - A plain hull bar sits above the sonar until the HUD pass.
  - **The rock lost its collision when the caverns merged in.** The shaft engineer traced it to the engine. UE 5.9 caches the cooked collision of a procedural mesh by its simple geometry alone, and `UProceduralMeshComponent` has no mesh id, so every rock mesh shared one cook. In the engineer's words: "The shaft's lost collision was an engine cache: UE 5.9 caches procedural collision by simple geometry only, so every rock mesh shared one cook. A 1 cm keyed tetrahedron per mesh fixed the shaft, the sea bed and the caverns in one helper."
  - **The gate.** The orchestrator fast-forwarded integration to the merged build (`59af289`) and ran the full gate. By 1:29 PM it had 107 of 107 tests, a package with no usage-flag lines, a smoke run and a 762.6 MB zip.
  - **Seven scripted playthroughs** then all passed: `S2-gate` (38 s), `S7-run` (49 s), `S8-keys` (110 s), `S9-floor` (147 s), `S10-relic` (78 s), `S11-pass` (110 s) and `S12-sidehome` (59 s). The orchestrator pushed at 1:41 PM.
  - **A size warning that is not a problem.** The harness warned that the packaged folder is 1,047 MB. The jam's limit is 1 GB zipped, and the zip is 762.6 MB.
  - **Left for later:**
    - Squeezes barely hurt. Steering into one closes at 0.9 m/s, under the 1.5 m/s that counts as a hit, so it scrapes one point every 2.5 s.
    - The `Deep.Cave.Goto cavern` cheat stops at the 200 m limit.
    - The SUB marker sits on the LIMIT 200 label. That is finish for the HUD pass.
- **Why it matters:** Quinn said that driving by sonar is what makes the game fun, and it now passes the whole gate. Without the cook key, the sub would have passed straight through the shaft's walls.
- **Evidence:**
  - `polish/shaft` `9ffc342`, `2f283ee`, `02eff74`, `8ac8efc` and `59af289`;
  - `deep/Source/Deep/World/DeepCookKey.h`;
  - logs in `.wt\monster\deep\Saved\Harness\`: `package-20261115-132632` (0 usage-flag lines) and `smoke-20261115-132754`;
  - frames `devlog/media/2026-11-15-cave-join-*.png` and `devlog/media/2026-11-15-shaft-*.png`.

### 1:50 PM  The sonar ping stops lighting the sub, and a wall hit now shakes the view (orchestrator)
- **What happened:**
  - **The ping fix.** The camera engineer fixed the ping that lit the inside of the helm window. Each frame, the game now sends the pulse material a box round the sub's parts, 0.3 m larger than they are, and the material skips every pixel inside it. The cost is three dot products a pixel, with no extra pass.
  - **The rejected fix.** The engineer turned down the usual fix, a stencil mask. It would have needed a depth pass over about 200 parts and would have clashed with the serpent's own mask. The pulse material now costs 0.030 ms a frame.
  - **The ping starts closer.** It used to start 6 m out and now starts 0.5 m out, so rock right beside the hull still lights.
  - **Hull hits shake the view.** Each hull hit now carries its impact speed, and the shake grows with it. A scrape gets a smaller judder.
  - **The orchestrator's check.** It looked at the frames itself:
    - at the shaft squeeze, an amber band runs along a rock ridge and the brown frame round the window is gone;
    - from the water at stop 2, the hull stays dark 0.35 s after a ping.
    The wall-hit frame is a still, which cannot show a shake, so that is checked by hand in play.
  - **Merged locally.** The orchestrator merged the lane into integration as `340332f`. The one conflict was in the code map, `ARCHITECTURE.md`, where the cavern and the camera lanes had each added a line, and both were kept. It goes through one gate with the lower cavern roof.
- **Why it matters:** the ping is how the player sees in the dark (Quinn's lidar idea), and it can now go on by default. The shake makes a hit cost something the player feels, not only a number on the hull bar.
- **Evidence:**
  - `polish/camera` `c19f142` and `325903e`, with tests Deep.Camera 6 of 6, Deep.World.Cave 10 of 10 and Deep.Sim.Cave 3 of 3;
  - frames `devlog/media/2026-11-15-camera-pulse-0.4s.png`, `devlog/media/2026-11-15-camera-pulse-sub.png` and `devlog/media/2026-11-15-camera-wall-hit.png` on that branch;
  - row 83 in [[Agent Runs]].

### 1:54 PM  A lower cavern roof changes nothing on screen, so the diver gets the sonar ping (orchestrator)
- **What happened:**
  - **The roof result.** The cavern engineer lowered the roof (from 10 m to 8.5 m over the sub) and hung three times as much rock from it. The build and all 26 cavern and serpent tests passed, and the sub lost no hull on the way down. Then it took its own frames from the hatch at stops 2 and 3 and reported plainly that they show no difference:
    - looking level, black, because the nearest rock is 73 m and 94 m away;
    - looking up, the sub's belly fills the view;
    - 12 m out, looking up, the aim finds rock 18 m away, but the lamp fades into the fog before it reaches it.
  - **Its verdict:** "Moving rock won't fix this; the lamp's falloff and the fog will. Merge 0e14e21 or skip it." The orchestrator skipped it, and D-042 is dropped.
  - **The new plan (D-043).** Quinn's own idea was lidar-like beams "to see whats in front of you when it gets dark". The camera lane already built a sonar pulse that draws a band across rock as the ping passes. So the same engineer now gives the diver a ping of their own, reaching about 90 m, which should outline the cavern walls for a moment. Frames are due by 3 PM.
  - **Quinn's HUD note (D-044).** Mid-turn, Quinn wrote: "i dont like the use of color it doesnt match the theme please note that when we get to it". Both HUD mockups use cyan numbers with amber and red warnings. The HUD pass now starts with a new palette, which Quinn sees before anything is built. The diver-ping brief was changed to make the pulse colour a tuning value, so it can follow.
  - **The gate.** The serpent clamp and the camera fix (`340332f`) went into a gate at 1:54 PM, without the roof: verify, package, smoke and seven scripted playthroughs, about 15 minutes.
- **Why it matters:** this is the second time the caverns have failed the view from the hatch, and the second time an agent has said so itself instead of shipping a change that does nothing. Moving geometry has been tried twice. The next try changes what the player can see: a tool, not the set.
- **Evidence:**
  - `polish/cavern` `0e14e21` (tuning) and `ff0b0b5` (frames `devlog/media/2026-11-15-cavern-read-*.png`), pushed, not merged;
  - frames page version 17, with the camera fix frames and this result;
  - rows 77 and 84 in [[Agent Runs]]; D-042, D-043 and D-044 in [[Decision Log]].

### 2:25 PM  S11 passed for the wrong reason, the diver's ping is in, and the orchestrator reads before it tests (orchestrator)
- **What happened:**
  - **The gate on `340332f`** (the serpent clamp and the camera fix): verify, package (0 usage-flag lines), smoke and zip passed, and six of seven playthroughs passed. S11-pass failed. At stop 2 the serpent made its first pass on its 20 s timer, went back to lurking, and then heard nothing (noise 0) while the script sprinted the diver for 20 s.
  - **A rerun "passed", for the wrong reason.** The editor log shows the Captain's patience running out and cutting the line (`cut_patience`), and only then the serpent hunting. The script counts any state other than "tethered" as "hunted", so a cut line passes it. The 1:41 PM pass of S11 took 110 s, about as long as the patience lasts, so it was probably the same false pass.
  - **The likely cause**, not yet proven: the diver's sprint makes no noise, perhaps because pushing into cavern rock keeps the speed under the sprint threshold. A fix agent (opus, high) is on it in `.wt\dev` on `fix/s11-noise`, to fix the cause and make S11 check the serpent itself; report by 3 PM. The integration branch is held, not pushed, until then.
  - **The diver's sonar ping is done** (camera agent, `ccf1f22` and `42fbd47`). V in the water, a 3 s cooldown, about 100 m. At stop 2 it outlines the cavern rock in a moving band and leaves a field of lidar dots for a moment. At stop 3 the far wall, 94 m away, shows only as a thin line at 1 s. `Deep.Camera` 8 of 8, with a world test: refused aboard, one ping in the water, a second refused during the cooldown, and the helm's ping does not cut the diver's short.
  - **The orchestrator's review, the first under D-045.** It looks like it works: the server checks the cooldown, then every machine draws the pulse, and the ping rides the event bus that already plays the ping sound. What it changes elsewhere: the pulse is now on in all play; the ping makes no noise for the serpent, because nothing listens for it (a design choice, not a bug); a second player's client was not checked; and the camera test that said "pulse off" now says "pulse on", which follows D-043 rather than weakening the test. Merged into integration as `bbeeb7e`, with one conflict in `ARCHITECTURE.md` (both sides kept). Its gate is running: verify, package, smoke, zip, S8-keys and S9-floor.
  - **Quinn's new rule (D-045, about 2:12 PM).** Toward the end, the orchestrator reviews each diff itself, then runs `verify`, then only the playthroughs for that area. The full run-through waits for the freeze at 5:30 PM.
  - **Quinn on the monster.** He resumed the camera agent from its transcript and told it the serpent "doesn't animate at all and looks like two models smooshed together; I think the older one was better". The agent's reading: the Dragon Cave head, a rigid mesh with no jaw, sits on our code-built body, and nothing bends the body from side to side. That fix is next.
- **Why it matters:** a scripted playthrough can pass for a reason nobody meant. S11 used "the line is no longer tethered" to stand for "the serpent hunts", and the Captain's timer satisfied it. The exit code said pass; the log said otherwise.
- **Evidence:**
  - failing run, `.wt\monster\deep\Saved\Logs\Deep-backup-2026.11.15-19.05.38.log`: "Lurk -> Pass (noise 0, tether 37.9 m)", "Pass -> Lurk", nothing after;
  - rerun, `Deep.log`: `DEEPEVENT cut_patience` at 2:11 PM, then "Lurk -> Hunt";
  - the hunted check in `tools/playtest.ps1` (S11);
  - frames `devlog/media/2026-11-15-camera-diverping-*.png` (10, on `polish/camera`);
  - rows 84 and 85 in [[Agent Runs]]; D-045 in [[Decision Log]].

### 2:50 PM  The diver's ping passes its gate, three HUD palettes, and the serpent's body is to move by physics (orchestrator)
- **What happened:**
  - **The gate on `bbeeb7e`** (the diver's ping on top of `340332f`) passed at 2:33 PM: build, 117 of 117 tests, verify, a 1047.5 MB package, smoke, a 762.7 MB zip and the S8-keys and S9-floor playthroughs. The zip step warned that the team name is still a placeholder. Integration stays unpushed until the S11 fix is in.
  - **HUD A started at 2:24 PM**, six minutes before the 2:30 PM default the orchestrator had given Quinn for choosing A or B. Saying B still switches it.
  - **Three palettes (D-044).** A sonnet agent drew them from the theme and put a palette switch on the HUD mockup page (version 2, 2:38 PM): Bone and oxide, the off-white and rust of a worn hull; Tonal water, cut from the water's own blue-grey, which it recommends; and Amber phosphor, the whole HUD as one console screen, which breaks the Style Guide's rule that amber never goes on HUD lines. It measured contrast over the real frames: all panel text clears 4.3, and danger red is never weaker than today's.
  - **Two problems in every palette, today's included.** Warnings and the machine screens (sonar, helm, the Captain's tag) share one amber, so the sonar walls look like a permanent warning. And the reticle, aim brackets and hatch marker have no panel, so over the lamp glow they drop to about 1 to 2 contrast. At 2:39 PM the HUD A engineer was told to make the two ambers two roles, outline those marks, and build in Tonal water until Quinn picks.
  - **A lane the orchestrator did not start.** Quinn resumed the camera agent himself with his note on the serpent. It made its own worktree, `.wt\swim` on `polish/serpent-look`, and the orchestrator found it with `git worktree list`. It went back to one model: the code-built head with its jaw is the default again, instead of the Dragon Cave head on a code-built body (`8b5b100`, frames `da36b1f`, pushed, not merged).
  - **Quinn on the slither (D-046), about 2:36 PM:** "instad of a slither mechanic we need eah spline to have physics so that it moves like an actual snake in 3d space", with "the controller is the head". An Opus engineer started at 2:39 PM on the same branch. The body becomes a chain of points the head drags through the water, integrated the way Epic's CableComponent and Jakobsen's "Advanced Character Physics" (GDC 2001) do it, with fixed spacing, a bend limit, water drag and the cave clearance. The sine weave goes. A small stroke at the head starts a wave, and the chain carries it down the body. Report by 4:00 PM with frames in motion, or stop at 3:45 PM if it does not look right.
- **Why it matters:** the serpent is the game's one creature, and Quinn has turned down two looks for it. The chain changes how it moves, not how it is drawn, so it costs no new art and keeps the harpoon hit segments. Quinn expects it to be easy, and the chain is. The work is keeping it stable at any frame rate and off the rock in a winding shaft, which is why the brief asks for the solver as a tested pure function. And the worktree list is how the orchestrator learns of work a human starts with an agent directly.
- **Evidence:**
  - integration `bbeeb7e` in `.wt\monster`, gate passed at 2:33 PM;
  - the HUD mockup page, version 2, and [[HUD Palettes]];
  - `polish/serpent-look` `8b5b100` and `da36b1f`, frames `devlog/media/2026-11-15-serpent-look-*.png` (4) on that branch;
  - rows 86 to 89 in [[Agent Runs]]; D-046 in [[Decision Log]].

### 3:00 PM  S11 is honest now, and HUD A is in the integration build (orchestrator)
- **What happened:**
  - **S11 was failing for the script's reasons, not the game's.** The S11 engineer (row 85) found that the old script's 1.5 s key holds never reached sprint speed (3.9 m/s against a 5.2 m/s threshold), and that its catch-up teleport put the diver at the end of the line, pinned and silent. The serpent never heard a thing. The earlier passes came from the Captain's patience cutting the line, which the old check counted as a hunt. The fix (`7eb79d8`) changes only the script: with the lamp off, the diver sprints in 5 s legs round four corners near the anchor, and the check passes only when the serpent reaches Stalk or bites. The engineer's first try at the fix failed and blamed itself correctly ("script at fault"): its holds were still too short.
  - **The orchestrator reviewed the diff before testing it (D-045).** It touches S11 only, turns the lamp back on at the end, and no longer counts a patience cut. One comment says x1.6 where the report says x1.3; both give 5.2 m/s. Integration had not touched the file, so the merge was clean (`1bd4031`). The change is script-only and the build was the one verified at 2:33 PM, so the gate was S11 once: PASS in 104 s, the serpent from Lurk to Stalk in 55 s of sprinting, noise 60.2, heard +588. Integration was pushed at 2:53 PM, with the camera fix and the diver's ping that had waited under it since 2:20 PM.
  - **A design finding for Quinn.** Sprinting alone never brings the serpent. Sprint noise counts only above 5.2 m/s and adds 15 a second against a decay of 10, so reaching Stalk at 60 takes about 12 s of unbroken sprinting, and a straight sprint reaches the end of a 60 m line in about 9.4 s, at about 43. Winching (30 a second) and grabs (30 each) do bring it. The orchestrator recommends keeping it: sprinting adds to the danger, and the core loop brings the serpent anyway.
  - **HUD A reported at 2:50 PM, 85 minutes early.** Direction A, the instrument, in Tonal water. Every colour is now a role in one file, `UI/DeepHudStyle.h`, the old canvas HUD and the harpoon overlay included, so another palette is a one-file change. New: a hull bar over the sonar whose fill eases down on a hit and leaves a ghost of the lost part that fades over 0.6 s; SUB and LIMIT placed by a tested function so they never touch; a 1 px dark edge on the reticle, aim brackets and hatch marker. The engineer's own calls: the harpoon's state rides on the aim brackets, the lamp's ON and OFF row is gone because the beam shows it, the water hint gains a V row for the ping, and the hint gives way to the bite band.
  - **Review, then the gate.** The orchestrator looked at all five frames and read the logic diff. The ping hint maps to V, nothing the scripted playthroughs read changed, and the fonts are the ones the 2:33 PM package already cooked, so no package is needed before the freeze. Merged as `f0b7fd0`; verify passed, 121 of 121 tests, in 80 s; pushed at 2:55 PM. The frames are on the frames page, version 19, beside the old cyan helm with SUB on top of LIMIT.
- **Why it matters:** a playthrough that passes for the wrong reason is worse than one that fails, because it hides the thing it was written to catch. S11 now fails if the serpent stops hearing. And the HUD was the last thing across the whole screen still in the old cyan, which Quinn said does not match the theme.
- **Evidence:**
  - `fix/s11-noise` `7eb79d8`; integration `1bd4031`; S11 on integration: PASS (104 s), Lurk > Stalk, noise max 60.2, heard +588.0, 6.1 m/s mean;
  - `polish/hud-a` `e11eca0`; integration `f0b7fd0`: PASS build (43.9 s), PASS test (35.6 s), 121 tests, PASS verify (79.5 s);
  - frames `devlog/media/2026-11-15-hud-a-{swim300,helm-limit,serpent-bite,aboard-bank,settlement}.png` on `polish/hud-a`; the frames page, version 19;
  - rows 85 and 86 in [[Agent Runs]].

### 3:10 PM  The submission text is rewritten for today's game, and review catches two shots that cannot be filmed (orchestrator)
- **What happened:**
  - **The itch page was eight hours old.** The draft from 7:05 AM described the game before the cave shaft: a trench, no submarine to drive, a Captain who mocks you the whole way down. The submission window, 6 to 7:30 PM, has room for the video and the form, not for writing. So at 3:03 PM a Sonnet writer (row 90) was sent to rewrite it from the integration code, write a shot list for the 30 to 60 s video Quinn records at about 6:15 PM, and update the checklist. It finished in about 2 minutes.
  - **It checked the old draft against the code** and fixed what the code disproved: 185 Captain lines (the data has 74), the 66-test count, a helm that only dives (W, A, S and D steer it in the shaft), the vague overload rule (you sink past 10 kg; at 15 kg you cannot swim up), and the missing ping and camera shake.
  - **The orchestrator then checked the writer against the code (D-045) and found three faults.**
    - It stamped its files 3:30, 3:40 and 3:45 PM when it was 3:03, probably from its 3:45 PM deadline.
    - The video's first shot was a sonar ping at the helm. The ping is the diver's and works only in the water (`DeepCameraRules::CanDiverPing` takes `IsInWater()`), so Quinn would have pressed V at the helm on camera and seen nothing. The ping moved to the shot where the diver swims out.
    - The lamp shots need the flashlight, which the first settlement sells (`HasFlashlight()` is `GatesBought() >= 1`, and `ToggleLamp` does nothing without it). The writer also thought the cheats would not work in the package. `tools/ue.ps1` packages a Development build, so the console should work: the shot list now says to try `Deep.BuyGate free` and `Deep.GotoStop 2` once before recording.
  - **What the orchestrator confirmed in code:** the 40 percent cut (`DirectorCutPercent`, an internal name the player never sees), the death fee, the 5 s bite alarm, a full-length bite that severs the line (`bSevered`), the lamp that always breaks a bite at once, the hull breach that drowns the diver at the helm, 30 s of bailout air, depth limits of 200, 400 and 600 m with the relic at 600, and the 3 s ping cooldown. The Dragon Cave seller's name comes from `Credits.csv` on integration.
- **Why it matters:** a video plan that opens on a key that does nothing would have cost a retake in the busiest hour of the jam. A writer that checks claims against code still needs the same check on its own work, and here the review cost six greps.
- **Evidence:** `devlog/itch-page.md`, [[Video Shot List]], [[Submission Checklist]]; row 90 in [[Agent Runs]]; the code read is integration `f0b7fd0` (`Diver/DeepDiver.cpp`, `Monster/DeepMonsterRules.cpp`, `Sim/DeepTuningTypes.h`, `Diver/DeepRunComponent.cpp`).

### 3:20 PM  The serpent's body moves by physics now, and it is in the integration build (orchestrator)
- **What happened:**
  - **The ask.** At 2:36 PM Quinn said the slither was not realistic: each part of the body should have physics, so that it moves like a real snake in 3D, with the controller as the head (D-046). At 2:39 PM an Opus engineer (row 89) started on `polish/serpent-look`, with a recipe from Epic's CableComponent and Jakobsen's GDC 2001 paper on Verlet physics, a report due at 4:00 PM, and an order to stop at 3:45 if it was not right. It reported at about 3:10 PM.
  - **What it built** (`f4ef2b0`). The body is a chain of points pinned behind the head, with no gravity. Each step moves every point by its own velocity, slowed by water drag: 1.2 a second along the body and 10 more across it, because water resists a snake moving sideways far more than along its length. The steps are fixed at 1/120 s, and a slow frame is clamped to 0.1 s so that a hitch cannot throw the body. Then a pass from head to tail puts every link back at its length and bends no joint more than 28 degrees, then each point is kept inside the cave. The sine weave is gone. The tube is drawn on a smooth curve through the points and banks into its turns. When the lamp drives the head back, a kick runs down the chain.
  - **Then it took the stroke out** (`35ddef2`). The first version kept a small swim stroke at the head. The agent reports that Quinn asked it for a plain zero-gravity rope, so the stroke is off: the body moves only where the head's path and the physics take it.
  - **The orchestrator reviewed the diff before testing it (D-045).** The AI's path, the bite's reach, the lamp and the cave clearance all read the head, and the head's motion did not change, so the scripted playthroughs test the same game. The cave check holds a weak pointer to the cavern and is cleared outside a cave stop. A NaN resets the chain straight. Nothing else on integration used the removed weave settings. Keeping each point inside the cave is arithmetic on the cave's radius, not a trace, so it costs little at 120 steps a second. The five new tests cover a straight swim, a turn in which the tail follows the head's path round the corner, the bend limit, hitches of 0 and 0.5 s, and the shipped settings.
  - **The gate.** Merged as `964c66e`. Verify passed with 126 of 126 tests in 66 s. S3 passed in 12 s. S11 passed in 101 s: after 55 s of sprinting the serpent went from Lurk to Stalk. Pushed at 3:17 PM.
  - **How it looks** (frames page, version 20). In the game's own dark it works, because the lamp shows only a band of wet scales sliding through the beam. In the agents' bright test light it reads as a hose: an even, glossy tube, and on a sharp turn the neck folds into a tight hook. The head, built in code with a jaw and a tooth row, reads as a predator. Quinn has three choices: keep it (the default), go back to `8b5b100`, or one tuning pass before the freeze: a body that tapers and a neck that bends more softly.
- **Why it matters:** the body now moves the way Quinn asked, by physics, and the gameplay could not change with it, because everything the player fights reads the head. A look change that touches no rule can merge on a short gate.
- **Evidence:**
  - `polish/serpent-look` `f4ef2b0`, `35ddef2`; integration `964c66e`: PASS build (29.6 s), PASS test (36.0 s), 126 tests, PASS verify (65.7 s); PASS S3-serpent (12 s); PASS S11-pass (101 s), Lurk > Stalk, noise max 60.3, heard +588.2;
  - frames `devlog/media/2026-11-15-serpent-physics-{side-0.0s,side-0.4s,side-0.8s,cavern,face}.png` on `polish/serpent-look`; the frames page, version 20;
  - row 89 in [[Agent Runs]].

### 3:30 PM  Quinn asks for a gun with knockback; the harpoon is that gun, so it gets the kick (orchestrator)
- **What happened:**
  - **The ask.** At about 3:22 PM Quinn asked for "the gun, where it gives you knockback when you shoot at the serpent on the tether", and said he was not sure it adds to the fun.
  - **What the code already had.** The orchestrator read the harpoon before answering. The gun has been in since D-021: one shot per dive, with a 25 m range. A bolt into the serpent drives it off as the lamp does, but loudly (noise 60), so it can come back. A bolt into the sub reels the diver home, which is the way back when the tether is cut. So the one shot is already a choice: save yourself now, or keep your way home. Firing shook the camera by 1 cm and 0.6 degrees for 0.18 s, and did nothing to the diver.
  - **The call.** Yes, as the harpoon's recoil, not as a second gun. The single shot gets weight. In water a shot should push the shooter back. And the shove opens a beat of distance from the serpent before the noise brings it back: tension, release, tension. More ammunition, or a gun that kills the serpent, was turned down, because plenty of shots turns a horror game into a shooter.
  - **The dispatch.** With the freeze at 5:30 PM, the orchestrator started the work at about 3:27 PM, before Quinn's yes, on its own branch (`polish/harpoon-kick` in `.wt\dev`, from integration `1bc6cd0`). One word from him stops it. An Opus engineer (row 91) writes a pure recoil rule first: 3.5 m/s opposite the aim, gone in half a second, about a metre. Then the hook, a firmer shake on the shot, and a new thud when a bolt strikes the serpent. The brief rules out `LaunchCharacter`, which would put the swimming diver into a falling mode, and asks that the kick end the moment a reel begins, so that a shot at the sub still reels the diver home in S5.
- **Why it matters:** the last two hours before the freeze go to feel. The change is small and sits on its own branch. At merge the gate is verify plus the scripted playthroughs that fire the harpoon (S5, S10).
- **Evidence:** row 91 in [[Agent Runs]]; D-047; `deep/Source/Deep/Harpoon/DeepHarpoonComponent.cpp` (the shot at about lines 268 to 318, the reel at about line 484); `deep/Source/Deep/Camera/DeepCameraRules.cpp` line 43 (the old shake).

### 3:40 PM  Eighteen minutes from Quinn's ask to a tested build: the harpoon kicks (orchestrator)
- **What happened:**
  - **The build.** The Opus engineer (row 91) reported at about 3:33 PM, six minutes after it started, with `e287d29`: nine files, 243 lines added. A pure rule gives the shove: 3.5 m/s straight back from the aim, falling evenly to zero at half a second, 0.875 m in all. Each frame the harpoon moves the diver by the rule's distance for that frame, as a swept move, so rock stops the shove and the diver slides along it, and the tether still holds the diver to the line's length. The shove ends the moment the bolt latches on the sub or the rock, and on a release, a reload, inside the sub, at death or on the winch. A client predicts its own kick, so the change adds no network call. The shot's camera kick went from 1 cm and 0.6 degrees for 0.18 s to 3 cm and 1.5 degrees for 0.3 s, and a bolt into the serpent now lands with a thud (5 cm, 1.8 degrees, 0.6 s), between a hull hit and a critical one.
  - **The tests.** `Deep.Sim.Harpoon.Recoil` checks the rule: full speed at once, half speed at half time, nothing after, 0.875 m in all, and the frames add up to the same. `Deep.World.Harpoon.Recoil` fires in a test world: the first frame moves the diver back more than 4 cm, the whole shove matches the rule within 8 cm and points straight back, the diver stops afterwards, an empty click gives no kick, a block of rock stops the shove, and a shot at the sub still reels the diver home.
  - **The review, before any test ran (D-045).** The orchestrator read the diff. The shove cannot fight a reel, because it ends before the reel starts, so S5 and S10, which reel the diver home, should still pass. It cannot carry the diver through rock, because the move is swept. Two notes, left as they are: the shot's shake (1.5 degrees) is now a little stronger than a hull hit's (1.4 degrees), though shorter, and those numbers came from the orchestrator's own brief; and a bolt into the serpent still plays the clank from `Sounds.csv`. No test covers how the kick feels, or a networked client.
  - **The gate.** Merged as `cee150a`. Verify passed with 128 of 128 tests in 58 s. S5-harpoon passed in 18 s, and S10-relic in 76 s. Pushed at 3:40 PM. The package took 1 min 31 s, the smoke run was clean, and the zip is 762.7 MB. Quinn's copy for hand-play is in `Builds\HandPlay-1540`, outside the integration checkout, so that a game left open cannot lock the files the freeze package overwrites.
- **Why it matters:** an idea at 3:22 PM was in the tested build at 3:40 PM. It went fast because the change was small and sat behind a pure rule: the rule's arithmetic was tested first, one world test showed the diver obeys it, and the review checked that nothing else had to change.
- **Evidence:**
  - `polish/harpoon-kick` `e287d29`; integration `cee150a`: PASS build (21.7 s), PASS test (36.4 s), 128 tests, PASS verify (58.2 s); PASS S5-harpoon (18 s); PASS S10-relic (76 s); PASS package (1 min 31 s, 1047.6 MB); PASS smoke (25.8 s); PASS zip (21.9 s, 762.7 MB);
  - `deep/Source/Deep/Harpoon/DeepHarpoonRules.cpp`, `deep/Source/Deep/Harpoon/DeepHarpoonComponent.cpp`, `deep/Source/Deep/Private/Tests/DeepHarpoonTests.cpp`;
  - row 91 in [[Agent Runs]]; D-047.

### 4:20 PM  Quinn says the game is not good enough; five lanes rebuild the helm, the cave, the HUD, the line and the room (orchestrator)
- **What happened:**
  - **The verdict.** At about 3:50 PM Quinn played the build and wrote that the game was not great. The sub could not be moved on every axis. "The UI is still too cluttered and really takes away from the immersion." "The interior is still not great", but the environment was what was really lacking. "The whole tether mechanic seems a little pointless as it stands", the Captain was still useless, and everything felt ad hoc and hacky, without real polish or immersion. He said that at this point he did not think the game was even worth submitting. He asked for real sub controls with a ballast lever, and for a cave generated by PCG or by a cave algorithm he could tweak.
  - **The challenge.** The orchestrator disagreed about not submitting. Integration `cee150a` is packaged and tested (verify 128 of 128, a 762.7 MB zip), so it is a floor the team can always ship. Not submitting scores zero for certain, and the dev log is judged on its own for the Glass Box prize.
  - **The plan.** Five lanes, one per complaint. Each is an Opus engineer on its own branch from `cee150a`, and all five started at about 4:15 PM (rows 92 to 96):
    - **Helm:** a throttle lever with notches (W/S), a rudder with inertia (A/D), a ballast lever with a neutral detent (Space blows, C floods), and X for all stop. The sub stays the fixed point and the world turns about it, so all the code that runs at a stop sees the frame it sees today. Levers stand by the seat.
    - **Cave:** one seeded noise function (ridged, with strata for ledges) sets the wall's radius for both the mesh and the collision, so what you see is what you hit. A wet-rock material from the Dragon Cave textures, and rocks sunk into the walls. Every number is live in Project Settings, with `Deep.Cave.Rebuild`.
    - **HUD:** a pass that only removes: no subtitles, hints that go after first use, one prompt at a time, the hull only when it changes or is low, and the helm down to the sonar and a depth readout.
    - **Tether:** the line becomes the air line. Clipped, air is unlimited. The serpent's head on the rope bites it through and leaves the diver on bailout air. The diver can unclip on purpose to go further. The Captain's patience no longer drains or cuts, and the Captain is off by default.
    - **Interior:** about 100 pieces of dressing (ribs, pipes with valves, cable trays, gauges, lockers, a ladder, caged lights) in worn painted steel, brass and rubber, with light only from real fittings.
  - **What was turned down.** The PCG plugin. It needs binary graph assets and a change to the project file, and it was on the Cut list. A C++ generator gives the same tweaking without either.
  - **How five lanes share one machine.** The shared files are split by function: the helm lane owns the helm functions in the run component, the tether lane the patience and cut code, and each lane adds its own settings struct. The cave shaft is split by a written contract: the shaft follows the arena root, which turns by minus the sub's yaw and is identity at yaw 0, so nothing moves differently until the sub turns. Every Unreal process takes the machine-wide slot.
  - **The schedule.** The 5:30 PM freeze (D-038) moves to 6:45 PM. Lanes merge as they land from about 5:15 PM, each after a diff review, verify and its own playthroughs (D-045). Quinn plays by hand at about 6:15. The full run-through and the package are at 6:45, then the video, the itch page and the form, and submission at about 8:30 PM, half an hour before the deadline.
- **Why it matters:** two hours before the freeze, the lead's verdict was that the game was not worth submitting. The answer was a floor that is already packaged and five parallel attempts to raise it, each able to fail alone.
- **Evidence:** D-048; rows 92 to 96 in [[Agent Runs]]; the helm screen Quinn called cluttered, `devlog/media/2026-11-15-hud-a-helm-limit.png`; branches `rescope/helm`, `rescope/cave`, `rescope/hud`, `rescope/tether` and `rescope/interior`, all from `cee150a`.

### 5:15 PM  Four of the five lanes land, the helm turns out to need flying, and Quinn's second round starts two more (orchestrator)
- **What happened:**
  - **Three lanes merged.** Between 4:31 and 4:51 PM three of the five re-scope lanes were reviewed and merged into integration:
    - **The air line** (row 95), `63830a0` at 4:31 PM, 16 minutes after it started. Clipped, the diver's air is unlimited. The serpent's head on the rope bites it through and leaves the diver on bailout air, T unclips on purpose, and the Captain's patience no longer drains. Two fixes followed the same road: S11-pass now counts a bite through the line as the serpent hunting (`0ec752a`), and the audio coverage test, which had gone red, learned the line's two new sound events (`2dd2cd2`).
    - **The calm HUD** (row 94), `03b9c72` at 4:48 PM. Text shows only while it is needed and then goes, one prompt at a time, critical cues only, and the Captain is quiet by default.
    - **The dressed room** (row 96), `5e530da` at 4:51 PM. 149 pieces of dressing (ribs, pipes with valve wheels, cable trays, gauges, lockers, a bench, a ladder, caged lights), drawn as 15 instanced meshes with no collision, in worn steel, tarnished brass, rubber and grimy glass. Five fittings light the room. `Deep.Room.Dress 0` hides it all for an A/B.
  - **One conflict, three times.** Each lane added its own settings struct at the end of `DeepTuningTypes.h`, so every merge stopped at the same line: a text merge cannot know that two appended structs are both wanted. The orchestrator kept both sides each time and counted `struct` and `};` against both parents to prove every struct was closed (33 and 36 at the base, 36 and 39 after the helm).
  - **The helm** (row 92) reported at about 4:55 PM, after 39 minutes. W/S step six throttle notches from 2 m/s astern to 4 m/s ahead. A/D hold a rudder that turns the sub at up to 28 degrees a second and comes in over about 0.6 s. C floods and Space blows a ballast lever that stays where it is left: 2.6 m/s at full, so 100 m takes about 40 s. X is all stop. The sub itself never moves; the world turns about it by the replicated `SubYawDeg`. A wall knock dents the hull and trips the throttle to stop. It was merged with integration as `ea92861` at 5:05 PM and waits on its own branch until the gate on `5e530da` finishes.
  - **What the helm lane found.** Diving at full flood without steering, the sub hit the walls at 1.6 to 5.8 m/s from 153 m down and breached the hull by 240 m. With real controls, the cave's bends must be flown. The call: keep the walls dangerous, and make the cave flyable and the controls readable. The cave lane caps how fast the passage's centre drifts sideways, at about 0.5 m per metre of depth where it is tight, with no steps, and proves it with a pure test; at full flood a bend then sweeps sideways at about 1.3 m/s, which slow ahead (1.33 m/s) can follow. The HUD lane adds a seated readout of the levers, which cannot be seen from the seat. Two ideas were dropped for time: a helm that centres itself in the passage, and a bot that drives the levers in the scripted playthroughs, which keep the old direct path.
  - **Free sub models** (row 97). At about 4:45 PM Quinn asked whether free submarine models were worth a search. A Sonnet researcher read Sketchfab's public search for 2.3 minutes and downloaded nothing. No free, worn, ready-made small research sub exists. The best base is the Atlantic Explorer (CC BY 4.0, about 120,000 triangles, the only free model with an interior), and a photographed scan of a beached old submarine could lie as a wreck in a cavern. It turned down the OceanGate Titan models, because a real, lost vessel is poor taste in a horror trailer, as well as cartoon and steampunk subs, warships and non-commercial licences. Quinn downloads; the credit lines go in the submission.
  - **Quinn's second round.** At about 5:00 PM, after playing, he wrote: "also the falshlight does not have a big enough cone you cant see anythign really, also i think we need a better way of controlling the sub also teh monster needs to be taunting the player in order to make iti good make sure you can here it and it can hear you better" Then: "also the landscape is sitill bad, im htinking the radar could be when your swimming instead of the sub also The light propagation withing the fog seems a littel weird i would like to make this better"
  - **The answer (D-049).** Two new Opus lanes from `ea92861` at about 5:10 PM. The light lane (row 98) widens the lamp's cone and reworks how light carries through the fog, after reading Epic's volumetric fog documentation. The serpent lane (row 99) makes the serpent heard before it is seen, makes it hear more and call back, and has it taunt two or three times at the edge of the lamp before it strikes. The serpent's seen-in-the-beam check reads the lamp's cone angle, so the serpent lane moves it onto its own angle, at today's value, before the lamp widens. On the controls: the helm Quinn played was the old one, so he judges the new one first. On the radar: the diver already has one, the ping on V (D-043). The orchestrator advised keeping the sonar at the helm until Quinn has seen the new light and cave, because an inertial sub flown through bends in the dark with no sonar may be a descent no one can finish. The HUD lane adds a switch, so hiding it is one line.
  - **One machine, short of memory.** At 5:10 PM four building lanes, one gate run and two editors shared 31.6 GB, with 5.1 GB free. Each lane is told to keep one editor at a time and close it when done.
- **Why it matters:** four of the five re-scope lanes were in by 5:05 PM, under an hour after they started, each through its own review and merge. The helm lane's own test found the design problem before Quinn did: real controls turn the cave into a course that must be flown, so the cave and the HUD changed to match.
- **Evidence:**
  - integration `5e530da`: verify passed at 4:54 PM; seven playthroughs (S0, S1, S3, S4, S10-cutdrop, S10-relic, S11) running at 5:15 PM;
  - `ea92861` on `rescope/hud-helm`;
  - frames `devlog/media/2026-11-15-rescope-hud-{helm,aboard,water}.png`, `devlog/media/2026-11-15-rescope-interior-{before,after}-{helm,hatch,aft}.png` and `devlog/media/2026-11-15-rescope-helm-levers.png`;
  - lane notes `rescope-tether`, `rescope-hud`, `rescope-interior` and `rescope-helm` in `05 - Log/Lane Notes/`;
  - rows 92 to 99 in [[Agent Runs]]; D-049; [[Polish - Sub Assets]].

### 5:55 PM  All seven re-scope lanes are in one build that passes, and the light is measured again on moving rock (orchestrator)
- **What happened:**
  - **The helm's gate passed** at 5:40 PM on `7027c72`, the helm lane merged with the HUD lane's seated readout: verify with 156 of 156 tests, then S1-hatch (20 s), S2-gate (41 s, parked at stop 2 at 300 m and seated), S8-keys (73 s, every helm key), S7-run (42 s) and S12-sidehome (61 s: the winch brought the diver home from 18 m below and 20 m to the side in 5 s).
  - **The seated readout** (row 94, `047b953`). Seated, the camera sits under the hull, so the helm's levers are out of view. A small instrument panel now stacks on the depth readout, at the helm only: THROTTLE with the telegraph's notches, BALLAST with a centre-zero bar, HEADING 000 to 359, in the existing palette. The hint says the ballast is a lever: "C floods, SPACE blows; it stays". `Deep.Hud.HelmSonar 0` hides the helm's sonar for Quinn's radar question; it stays on by default.
  - **Three more merges in ten minutes:** the cave (row 93) at 5:33 PM as `fe11bae`, the serpent (row 99) at 5:41 PM as `852d76c`, and the light (row 98) at 5:43 PM as `7698723`.
    - **The cave** is generated rock. One seeded noise rule, ridged with strata ledges and calmer at the cavern mouths, shapes both the shaft's mesh (128 sides, 249,000 triangles, built in about 0.4 s) and its collision, so the rock you see is the rock you hit. Every number is live in Project Settings, and `Deep.Cave.Rebuild` rebuilds the shaft while playing. 96 rocks from the Dragon Cave pack are half-buried in the walls, under a triplanar rock material with silt on the ledges. Where the passage is tight, its centre drifts at most 0.5 m per metre of depth, so the new helm can fly the bends. The lane's one known red, `Deep.World.Cave.Hit`, measured a knock as penetration per substep, which a shaped wall can turn into a graze; it passes on the merged build, where the helm lane's loop measures the real speed into the rock.
    - **The serpent** taunts: twice before it bites, it shows itself at the edge of the lamp, brushes past the line and withdraws into the dark, calling as it passes, and a taunt never bites. It hears the sonar ping, harpoon shots, hull knocks and a sub driven hard from up to 260 m, turns toward the sound and answers with a call. A drone that swells as it closes, clicks that quicken and three calls, all synthesised in code and placed in 3D on every machine.
    - **The light.** The lamp's cone is 18/38 degrees, was 7/21: a pool about 15 m across at 10 m. The fog scatters forward, so less milk comes back at the eye, and its temporal history is lighter, so the beam no longer trails when you turn, the fix Epic's volumetric fog page names. The rock casts shadows and the helm's floods cast fog shadows, so light stops at the rock instead of glowing through it.
  - **Order mattered.** The serpent went in before the light on purpose. Its seen-in-the-beam check read the lamp's cone, so a wider lamp would have changed the fight. The serpent lane moved the check to its own 21 degrees, and the light lane's run of the serpent's first-sight test went green only once that was in.
  - **A merge by character.** The light lane and the orchestrator's code map (`37722cb`) edited the same line of `ARCHITECTURE.md`, the World paragraph, and git stopped. A line merge cannot keep both. The orchestrator compared each side with the merge base character by character, found two insertions about 2,600 characters apart, and wrote a small three-way merge that keeps both when the edits do not overlap and stops when they do. It kept both.
  - **Measured, not assumed.** The light lane's first try at a finer fog grid looked better and cost 58 ms of GPU time a frame. It dropped it and kept the lighter history, the forward scatter and the shadows, at 12.6 ms against 12.2 before.
  - **The merged gate.** On `7698723`, verify passed at 5:45 PM with 164 of 164 tests in 1 min 42 s; then S7-run (48 s), S3-serpent (12 s), S11-pass (104 s), S10-relic (69 s), S2-gate (40 s) and S9-floor (149 s) passed. Integration is pushed.
  - **Measured again on moving rock.** The light lane's 12.6 ms was taken parked at stop 1, on the old smooth tube. The world turns about the sub, so while driving every rock mesh moves every frame, and since the merge the rock casts shadows; moving shadow casters can make virtual shadow maps redraw. So the light lane was resumed at 5:44 PM to measure on the merged build. Every reading is under the 16.7 ms of 60 frames a second, so nothing changed. At the helm at full throttle: GPU 4.2 ms at about 60 m, 10.0 at 150 m and 9.8 at 250 m. Diving at stop 2 with the lamp on: 9.5 to 11.9 ms. The closest is the sub's room at stop 2, 16.4 ms, where the room's lights take 10.8 ms of deferred lighting and the fog only 0.36. That room is the first place to cut if a slower machine stutters. The first run was wasted: the sub was still sinking, so the hatch refused at stop 2, and a second run took 43 s.
  - **Memory.** At about 5:26 PM a 13 GB process the orchestrator did not start shared the machine, and the gate scripts' memory floors were lowered: verify now waits for 3 GB free and skips under 1.5 GB, and a playthrough waits for 4 GB and skips under 2 GB. The process closed on its own by 5:39 PM, and the merged gate ran with 13 GB free.
- **Why it matters:** about 90 minutes after they started, all seven re-scope lanes (D-048, D-049) are in one build that passes every test and the playthroughs of every area they touched. Each went in on its own review and gate, so a bad one could have been left out alone. The freeze at 6:45 PM runs everything once more on this build.
- **Evidence:**
  - integration `7698723` (pushed); merges `7027c72`, `fe11bae`, `852d76c` and `7698723`;
  - frames `devlog/media/2026-11-15-rescope-hud-helm-readout.png`, `devlog/media/2026-11-15-rescope-cave-{060m,150m,250m-bright,stop2}.png`, `devlog/media/2026-11-15-rescope-light-{before,after}-{helm,water,deep,stat}.png` and `devlog/media/2026-11-15-rescope-monster-taunt.png`;
  - rows 92 to 99 in [[Agent Runs]]; D-049.

### 6:05 PM  The freeze run-through starts 40 minutes early, and the itch page describes the game as it is now (orchestrator)
- **What happened:**
  - **The frames page, version 21** (6:00 PM), puts the six re-scope areas first, with 19 frames and plain verdicts:
    - the helm's levers are plain sticks;
    - from the helm the cave rock barely reads except in the floods, and the first grade looked like crumpled paper at 250 m;
    - the wider lamp is still dim at 501 m;
    - the room is clearly better;
    - during a taunt the harpoon's reticle labels the serpent "SERPENT 8 m" in red, which breaks the calm screen.
  - Quinn's asks on it: play the build, keep or hide the helm's sonar, keep or drop the serpent's name tag, and "merge".
  - **The hand-play build** `Builds\HandPlay-1800` (from `7698723`, 1,048 MB) passed its smoke run at 5:59 PM.
  - **Started early, on purpose.** Every lane had merged by 5:43 PM and nothing is in flight, so waiting for 6:45 PM would only find a failure 40 minutes later. Main's notes merged into integration as `9ac970e` (no code), and at 6:05 PM the full run-through started there: verify, all 14 playthroughs, package, smoke and zip. Eight of the 14 have not run on the merged build yet: S0, S1, S4, S5, S6, S8, S10-cutdrop and S12. If Quinn asks for a change, only what it touches runs again.
  - **The itch page and the shot list** describe the re-scoped game (row 100, a Sonnet writer working from the lane notes). Before committing, the orchestrator checked them against the code:
    - the keys against the input map (T is bound by the tether, the harpoon by its own component);
    - Q still cuts the line, and the Captain's cut is 40 percent;
    - the lamp held on the serpent for 3 s drives it off;
    - parking needs the sub within 5 m of the stop, under 1.2 m/s, with the throttle at stop, the ballast neutral and the rudder centred.
  - Two fixes: the draft's time said 7 PM, and "silent by default" described a setting, so the page now says the Captain never says a word.
- **Why it matters:** a failure found at 6:30 PM can still be fixed before the video. One found at 7:15 PM costs the video or the page.
- **Evidence:** integration `9ac970e` (pushed); `devlog/itch-page.md`; [[Video Shot List]]; row 100 in [[Agent Runs]]; the frames page, version 21.

### 6:20 PM  The freeze run-through passes: every test, all 14 playthroughs, a package, a clean smoke run and a 763 MB zip (orchestrator)
- **What happened:**
  - On integration `9ac970e`, from 6:05 to 6:20 PM, in one Unreal slot:
    - verify: 164 of 164 tests (57 s);
    - all 14 playthroughs, in 12.5 minutes: S0-title 13 s, S1-hatch 15 s, S2-gate 36 s, S3-serpent 9 s, S4-cut 55 s, S5-harpoon 13 s, S6-bank 15 s, S7-run 46 s, S8-keys 96 s, S9-floor 147 s, S10-relic 78 s, S10-cutdrop 35 s, S11-pass 102 s, S12-sidehome 59 s;
    - the package (63 s, 1,048 MB), a clean 25 s smoke run, and the zip (31 s, 762.9 MB, under the 1 GB limit).
  - The eight playthroughs that had not yet run on the merged build all passed, so the re-scope broke none of the older paths: the title, the hatch, the cut, the harpoon, the bank, the keys, the cut-and-drop at the relic and the side trip home.
  - The build and its zip are copied to `Builds\Freeze-1820` (outside git), so a later package cannot overwrite them. The zip's SHA-256 starts `47B688DC739AC215`. It is still named `TeamName_DEEP.zip`, because the team name is not in `tools/jam.json` yet; re-zipping takes 31 s.
- **Why it matters:** the team has a tested, packaged entry 2 hours 39 minutes before the deadline, 25 minutes ahead of the plan. What is left is optional or quick: Quinn's calls, the video, the page and the form.
- **Evidence:** `verify=0 passline=True plays=[all 14 exit 0] package=0 smoke=0 zip=0 end 18:20:32` (the run's own summary); integration `9ac970e`.

### 6:30 PM  The credits named the pack for the wrong part, and the next package waits for the video (orchestrator)
- **What happened:**
  - The no-tag change came back at about 6:23 PM (`6946bcc` on `rescope/notag`, row 101). The orchestrator read the diff. In both HUD paths the reticle's label loses its "SERPENT " or "SUB " prefix and keeps the distance. The out-of-range text and the bracket colours are unchanged. It waits on Quinn's "no tag".
  - While checking the asset list the jam form asks for, the orchestrator found that the in-game credits still said the Dragon Cave pack made "the dragon head of the sea serpent". That head was dropped at 2:20 PM, and the serpent's head has been built in code since. The same agent corrected the row in 32 s (`dc19f58`; `Deep.UI.Settings`, 5 tests passed).
  - Before accepting the new wording, the orchestrator checked it two ways:
    - **In the code.** `M_DragonBody` samples the pack's dragon textures, but the serpent wears it only with the mesh head. The body's default skin, `M_SerpentSkin`, uses none of the pack's textures.
    - **In the freeze package's file index.** The cave rocks, the stones, the relic and their textures are there. No sound files are, because all sound is made in code.
  - The credits fix goes in with the next package, not now. Quinn may be recording the video on this machine, and a cook would make the capture stutter.
  - The public dev log stopped at 1 AM, with post 1. A writer is drafting post 2 from this log: Sunday, up to the freeze.
- **Why it matters:** the credits are the one place in the game that names someone else's work, and a judge can read them. The video is worth more than four minutes of packaging.
- **Evidence:**
  - `6946bcc` and `dc19f58`, pushed, not merged;
  - a search of `Builds\Freeze-1820\Windows\Deep\Content\Paks\Deep-Windows.utoc` for `SM_CaveRock01`, `SM_StoneworksStone01`, `SM_StoneworksSphere01`, `T_CaveRocks01_BC` and `.wav`;
  - `DeepSerpent.cpp`, around line 504.

### 6:40 PM  Post 2 of the public dev log is written and checked against the log (orchestrator)
- **What happened:**
  - A Sonnet writer drafted `devlog/02-sunday.md` from this log in 2.6 minutes: Sunday from 1 AM to the freeze, 2,573 words.
  - The orchestrator read it against the log. The events, times, SHAs and quotes held. It fixed six things:
    - the intro still had a sarcastic Captain who talks; the Captain has been silent by default since D-040;
    - the post said the log did not record how the look was chosen; the night's rules do (D-028, D-030, D-033, and the style guide at 2:40 AM);
    - the run counts: 101 runs, not 100, and 28 at Sonnet medium, not 27. One table row covers the eight fetches of Saturday evening;
    - a 2:09 AM greybox frame was captioned as the room Quinn judged at 3:50 PM. It moved to the night shift, and the interior lane's before and after frames took its place;
    - the video and submission times, which predated the freeze;
    - every frame was greybox, from 12:58 to 2:09 AM. Four frames from the 6 PM frames page were added: the room before and after, the new helm and the serpent's taunt. Each was viewed first. The cave frames were left out, because they show the helm HUD from before the helm lane and a Captain's subtitle.
  - It also added the 2:23 PM change that took the pack's dragon head off the serpent (`8b5b100`), so the 9 AM merge does not read as the serpent that ships.
- **Why it matters:** the dev log is judged on its own for Glass Box. A post that contradicts the build or the log costs more than a short one.
- **Evidence:** `devlog/02-sunday.md` and `devlog/media/2026-11-15-rescope-*.jpg`; Agent Runs row 102.
- **Left for Quinn:** Colby's part (a TODO in the post) and where both posts are published.

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
- 18 minutes from Quinn's ask for a gun with knockback (about 3:22 PM Nov 15) to a tested integration build with the harpoon's kick in it (3:40 PM). The agent's part took six.

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
