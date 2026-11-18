---
updated: 2026-11-18 01:19 EST
verified: 2026-11-14 21:29 EST, build-sheriff ran tools\ue.ps1 build, test and verify on this machine (all PASS, plus a deliberate failing test and a deliberate compile error that both reported FAIL). package, zip and smoke were run for real at 21:50 EST on branch sheriff/package (all PASS: package 5m12s, 729.2 MB; zip 448.7 MB; smoke 25 s clean). shot is proven only on its failure path (no PNG, FAIL after 120 s) because the game-side -DeepShot support does not exist yet. editor is still unrun. Tiers T2 to T5 and the MCP tool behaviour are still unverified design.
---
# Validation Loop

How agents prove that work is done. This is the design; none of it is built. Tool names are explained in [[Unreal MCP in UE 5.9]]. The agents that run each tier are in [[Agent Orchestration]].

## The principle

A task is not done until its gates pass and the evidence is attached to the report. No output is not a pass: a test run with no report, a screenshot that was never taken, or a log nobody read all count as failures.

The project (`C:\Repositories\deep-megajam\deep\Deep.uproject`) has had a C++ module, `Deep`, since 8:55 PM Nov 14, but no gameplay code or tests yet. T0 can run now. T1 needs the first tests.

## The tiers

| Tier | Proves | How it is run | Who | Time | Required |
|---|---|---|---|---|---|
| **T0 Compile** | The code builds. | Editor closed: UBT `Build.bat`, headless. Editor open: `LiveCodingToolset` (EnableLiveCoding, poll IsLiveCodingReady, CompileLiveCoding). Live Coding patches function bodies only; new reflected types need an editor restart. | systems-engineer, build-sheriff | 1 to 3 min | Every code task. |
| **T1 Rules** | The pure C++ rule functions give the right numbers. | Headless `UnrealEditor-Cmd.exe` with `Automation RunTests Deep`, then read `index.json`. Or in the editor with `AutomationTestToolset`: DiscoverTests, RunTestsByFilter("StartsWith:Deep"), GetTestResults. | systems-engineer, build-sheriff | Under 2 min | Every code task. |
| **T2 In-editor functional tests** | Actors and components behave in a real world. | Functional test maps and spawn-and-assert tests, run through `AutomationTestToolset`. | editor-driver | 2 to 5 min | Any change to actors, components or Blueprints. |
| **T3 AI playtest** | The game plays end to end. | StartPIE, CallCheat to set up, ApplyInput to play, `LogsToolset.GetLogEntries` to read DEEPSTATE, CaptureGameViewport for screenshots, assert, StopPIE. | playtester | 3 to 10 min | Before merging gameplay changes. |
| **T4 Package and smoke** | The shipped build works. | RunUAT BuildCookRun for Win64. Run the packaged exe for N seconds. Scan the log for Fatal, assert and ensure. Check the size is under 1 GB and the zip name is right. Run it on a second machine. | build-sheriff | 15 to 40 min | Every three hours or so, and at feature freeze. |
| **T5 Human feel** | It is fun and readable. | Quinn and Colby play. Agents cannot judge fun. | Humans | 10 min | Each milestone. |

## What the game must expose so agents can test it

These are requirements for the first gameplay code, not extras added later.

- **Cheats named `Deep.*`**, so `CheatsToolset.ListCheats` finds them. Examples: `Deep.GotoStation`, `Deep.GiveSalvage`, `Deep.SetPatience`, `Deep.SpawnMonster`, `Deep.ForceCut`, `Deep.Win`.
- **A state dump.** One line of JSON written to the log with the fixed prefix `DEEPSTATE`, on demand and at key events. Fields: run state, depth in metres, zone, Patience from 0 to 1, cargo kg and value, tether length and strain, credits, hull rating, monster state and distance.
- **A fixed random seed option**, so a scenario replays the same way.
- **Time dilation** for fast runs.
- **All tuning in config**, so an agent changes a number in a text file and re-runs.

## T3 regression scenarios

Each is a short script the playtester runs. Every step stays under the 120 second call limit.

| # | Scenario | Steps | Expected DEEPSTATE | Screenshot |
|---|---|---|---|---|
| S1 | First dive and bank | Leave the airlock, grab one salvage, return, bank. | Credits rise by the net payout. Cargo is 0. | Settlement screen. |
| S2 | Overload sinks | Give salvage past the free allowance, release all input. | Depth increases with no input. | HUD cargo readout. |
| S3 | Tether limit holds | Swim away from the sub for 20 s. | Distance never exceeds tether length. Strain above 0. | Taut tether. |
| S4 | Reel-in returns to the sub | From full extension, hold reel. | Tether length falls to its minimum. | Diver at the airlock. |
| S5 | Patience, cut, bailout | Set Patience near zero and wait. | Run state changes to cut. Bailout air counts down from about 30 s. | The moment of the cut. |
| S6 | Buy hull, descend, zone changes | Give credits, buy the hull rating, descend. | Hull rating goes up. Zone changes. | New zone lighting. |
| S7 | Monster hears and retreats | Spawn the monster, sprint, then aim the lamp at it. | Monster state goes to hunting, then fleeing. | Monster in the lamp. |
| S8 | Full run to a win | Use cheats to clear every station in under 3 minutes. | Run state is won. | Win screen. |

## The critic pass

A second kind of T3. The playtester plays with no script and scores the build from 1 to 5 against the seven judging criteria in [[Jam Brief]]. It reports what a judge would see in the first two minutes: whether the goal is clear, whether the theme shows, and anything unreadable, too dark or silent. It reports findings and edits nothing.

## Evidence every report must carry

- The tools or commands that were run.
- Pass and fail counts.
- Failing test names and their messages.
- Screenshot paths (`Saved/ModelContextProtocol` when Dump Media To Disk is on).
- What was not verified, stated plainly.

## Constraints

- **One MCP call at a time.** The editor runs tool calls on the game thread.
- **About 120 seconds per call.** Start long work, then poll with `GetTestStatus`.
- **A headless editor run also auto-starts MCP on port 8000.** Give it `-ModelContextProtocolPort=N` so it cannot take the port from the open editor.
- **UBT cannot rebuild the game module while the editor has it loaded.** Use Live Coding, or close the editor.

## Cadence

Adaptive since Nov 15 (D-027): run what the change can break, once. The table of what to run is in `.claude/loop.md` under "What to run".

- **While an agent works:** T0 build as needed, and T1 only for its own area, by name prefix (`tools\ue.ps1 test -Filter Deep.Sim.Audio`).
- **At merge:** T0 and all of T1, once, by the orchestrator, on the committed branch head. A branch that changes only notes needs nothing.
- **T3 scripted playtests:** only the scenarios that cover a merged gameplay change. The whole set once per checkpoint.
- **T4 package and smoke:** when a cooking change has merged, when a human needs a build, and at Sunday's freeze and final.
- **T5:** whenever Quinn plays.

## How to build and test

Everything goes through `tools\ue.ps1`. Run it from the repo root as `.\tools\ue.ps1 <verb>`. If scripts are blocked, use `powershell -ExecutionPolicy Bypass -File tools\ue.ps1 <verb>`.

| Verb | Does | Timeout |
|---|---|---|
| `info` | Engine path and version, project path, editor running on the project, port 8000 listening, packaged build. | none |
| `build` | UBT `DeepEditor Win64 Development`. Fails fast if an editor has the project open. | 20 min |
| `test [-Filter X]` | Headless `UnrealEditor-Cmd`, `Automation RunTests`, reads `index.json`. Default filter `Deep`. | 15 min |
| `verify` | `build`, then `test` if the build passed. The gate for every code task. | 35 min |
| `package` | RunUAT BuildCookRun Win64 Development into `Builds\`. Reports exe path and size, warns at 1 GB. | 60 min |
| `zip` | `Builds\<teamName>_<gameName>.zip` from `tools\jam.json` (created on first use; warns while teamName is `TeamName`). | 15 min |
| `smoke` | Runs the packaged exe windowed for 25 s, then scans its log for fatal errors, asserts and ensures. | 25 s |
| `shot -Name n [-Delay 6] [-Cmds "a;b"]` | Launches `UnrealEditor.exe Deep.uproject -game -windowed` with the three `-DeepShot*` switches, waits for the PNG, stops the game, prints `deep\Saved\Harness\shots\<name>-<timestamp>.png`. FAILs with "no screenshot produced; game-side -DeepShot support may be missing" if none appears. | 120 s |
| `editor` | Starts the interactive editor with MCP at `http://127.0.0.1:8000/mcp`. Does not block. | none |
| `clean` | Deletes `deep\Binaries` and `deep\Intermediate` only. | none |

Exit code 0 means PASS, anything else is FAIL. Each run prints one `PASS` or `FAIL` line with the verb and duration, then the first compiler errors or the failing tests and their messages. Full logs go to `deep\Saved\Harness\<verb>-<timestamp>.log`, test reports to `deep\Saved\Harness\test-<timestamp>-report\index.json`. On timeout the whole process tree is killed and the run is a FAIL.

PASS:

```text
build: DeepEditor Win64 Development, timeout 20 min
PASS build (4.6s)
  Result: Succeeded
  log: C:\Repositories\deep-megajam\deep\Saved\Harness\build-20261114-212922.log
test: Automation RunTests ^Deep (headless, timeout 15 min)
PASS test (17.3s) - 1 tests passed
  tests: 1 run, 1 passed, 0 failed, filter "Deep"
PASS verify (21.9s)
```

FAIL (a deliberately broken test, then a deliberate compile error):

```text
FAIL test (18.3s) - 1 of 1 failed; editor exit code 255
  tests: 1 run, 0 passed, 1 failed, filter "Deep"
  FAIL Deep.Harness.Alive [Fail]
      Error: Expected 'Harness is alive' to be true. (DeepHarnessAliveTest.cpp:14)

FAIL build (2.1s) - exit code 6
  C:\Repositories\Deep-MegaJam\deep\Source\Deep\Private\Tests\DeepHarnessAliveTest.cpp(14,22): error C2065: 'UndeclaredHarnessSymbol': undeclared identifier
  Result: Failed (OtherCompilationError)
```

`Deep.Harness.Alive` (`deep\Source\Deep\Private\Tests\DeepHarnessAliveTest.cpp`) is a permanent canary so `test` always has something to run. Zero tests, a missing report or a crash is a FAIL.

Quirks found on this project with UE 5.9:

- **The test filter is a substring match.** `Automation RunTests Deep` ran 27 tests, 26 of them engine tests with "deep" in the name. `ue.ps1` anchors a bare filter to the start of the test path (`^Deep`). Prefix with `*` for a substring match. `^X`, `X$`, `StartsWith:X`, `Group:X` and `A+B` pass through unchanged.
- **The class wizard broke the module.** A C++ class named `Deep` overwrote the module's `Deep.h` and `Deep.cpp`, so the module lost `IMPLEMENT_PRIMARY_GAME_MODULE`. It still compiled, but the editor refused to load it ("The game module 'Deep' could not be successfully initialized after it was loaded"), so the headless editor quit before any test ran. Fixed by adding `deep\Source\Deep\DeepModule.cpp`. Never name a class after the module.
- **`UnrealEditor-Cmd` exits with 255 when a test fails**, and also when no test matches the filter ("No automation tests matched '^X'"). The harness reads the report and does not rely on the exit code alone.
- **The headless run starts its own MCP server.** With `-ModelContextProtocolPort=8011` the log shows "Starting MCP server on port 8011", so port 8000 stays free for the interactive editor.
- **PowerShell 5.1 strips Unreal's inner quotes** from `-ExecCmds="Automation RunTests ^Deep; Quit"` when a native exe is called with `&`. `ue.ps1` passes one hand-built argument string to `Start-Process`, and never uses `Start-Process -Wait`, which hangs on helper processes that outlive the editor.
- **Timings.** The first Development build took 45 s (8 actions; the wizard had only built DebugGame). Incremental builds take about 5 s. A headless test run takes 17 to 26 s.
- **Package, proven (Nov 14, 21:44 EST, worktree `.wt\pkg`, nearly empty game).** `package` PASS in 5m12s, 729.2 MB in `Builds\Windows\Deep.exe` (Development, pak, no debug info). It passed first try, no script fixes. Well under the 1 GB limit, but it will grow with content, so re-check after each content drop.
- **Zip, proven.** `zip` PASS in 17.6 s, `Builds\TeamName_DEEP.zip` 448.7 MB. It warns while `teamName` in `tools\jam.json` is still `TeamName`; fix that before the real submission.
- **Smoke, proven.** `smoke` PASS in 26 s: the packaged exe ran 25 s windowed, and the log had no fatal error, assertion, ensure or unhandled exception.
- **`shot` (new).** `ue.ps1 shot -Name <name> [-Delay <s, default 6>] [-Cmds "<console cmds separated by ;>"]` runs `UnrealEditor.exe "<uproject>" -game -windowed -ResX=1280 -ResY=720 -nosplash -unattended -log -abslog="<log>" -DeepShot="<abs png>" -DeepShotDelay=<s> -DeepShotCmds="<cmds>"`. The game must implement the three switches: `-DeepShot` (absolute PNG path to write), `-DeepShotDelay` (seconds to wait after start before capturing), `-DeepShotCmds` (console commands, `;` separated, run before capture). The verb waits up to 120 s for the PNG, kills the process if still running, and prints the path. Proven failure path: with no game-side support it ran 2m01s and printed `FAIL shot - no screenshot produced; game-side -DeepShot support may be missing`. The success path is not yet verified.
- **Timings.** Cold worktree build 65 s, package 5m12s, smoke 26 s, zip 18 s.
- **Package broke when the MCP plugins arrived, fixed Nov 14 11:30 PM.** `package` failed at cook: the editor-only `ToolsetRegistry` plugin ships `ToolsetManifestSeed` assets that import a class not available on Win64 (12 errors, exit 25). Fix in `deep\Config\DefaultGame.ini`: `+DirectoriesToNeverCook=(Path="/ToolsetRegistry/ToolsetManifestSeed")`. (Never-cooking the whole `/ToolsetRegistry` path trips an ensure about two asset domains and kills the cook.) After it: package 27 s warm, 706.8 MB on disk, smoke clean, zip 426.4 MB, `Content\Data\CaptainLines.csv` present and the smoke log says "Loaded 138 Captain lines for 41 events". `ConflictingInstance` (UBT mutex) means another agent is building: wait and re-run.

### Scripted playtests

`tools\playtest.ps1 <scenario|all> [-Port 8012] [-KeepEditor]` costs no model tokens. It starts the editor on the port if nothing answers, then per scenario: StartPIE, run steps through `tools\mcp.ps1` (`DeepToolset.Exec` for cheats, `InputToolset.ApplyInput` for held keys), assert on the `DeepToolset.GetState` JSON, capture the game viewport, StopPIE. One `PASS` or `FAIL <scenario>: <reason>` line each, then a `TOTAL` line; exit code 0 only if all pass. It closes the editor only if it started it (and not with `-KeepEditor`). Needs an editor built with the `DeepToolset` module. `playtest.ps1 all` took about 2.5 minutes for S0-S4 with the editor already up.

| Scenario | Asserts |
|---|---|
| `S0-boot` | a submarine and at least one salvage piece exist, diver present, stop 1, tether length above 0 and the diver inside it |
| `S1-grab` | teleport 80 cm beside salvage piece 0, tap E, `cargo_kg` rises above 0 |
| `S2-overload` | `Deep.GiveSalvage 30 300`, no input for 3 s, `depth_m` increases |
| `S3-tether` | hold W + Space + Shift for 15 s; max `tether_distance_m` stays within `tether_length_m` + 1 and the diver really moved |
| `S4-winch` | teleport 25 m out to the SIDE at anchor height, hold R (8 s, retried up to 24 s), diver ends within 5 m of the anchor; this is the regression test for the old 7.4 m stall (passes: 25.0 to 3.0 m in 8 s) |
| `S5-cut` | bank 100 (credits 45), `Deep.SetPatience 0.02`, run state becomes `cut` and `bailout_air_s` counts down; teleport home: back to `tethered`, credits drop by the 20 fee; cut again far out, air runs out: `dead`, credits drop by 30 (floor 0), then respawn with `diver_number` +1, cargo 0, diver at the airlock (about 90 s) |
| `S6-bank` | gross 5 (net clamps at 0, credits stay 0), then gross 100: cut 40 (ceil of 40 percent), fees 15, net 45, credits rise by exactly the net, cargo emptied; left on the settlement screen for the frame |
| `S7-descend` | gross 300: net 145, hull rating 2 bought for 50 (credits 95), Space on the settlement screen, descent ends at stop 2, depth about 300 m (reads 304), salvage back (10), cargo 0 |
| `S8-run` | like a player: swim out on W + Space + Shift, teleport to salvage 0 and grab with E, top up with `Deep.GiveSalvage`, winch home with R from the side, auto-bank, hull bought, Space, arrive at stop 2; must finish in under 3 minutes (about 45 s) |

Helpers: `Wait-State` polls `GetState` until a condition holds (real-time timeout); `Press-Continue` taps Space until the settlement screen closes (the 2.5 s descent can finish between two polls, so it accepts any state but `settling`). Scenario S5 also saves `S5-cut-bailout.png` mid-cut.

Screenshots: `deep\Saved\Harness\playtest\<scenario>.png` (not committed; the capture tool takes only `maxResolution` 1280, so the aspect ratio is the editor viewport's, not forced 16:9). Chosen frames go into `devlog\media\` as JPEG only (D-066): `powershell -ExecutionPolicy Bypass -File tools\jpeg.ps1 -Files a.png,b.png -Dst devlog\media -Prefix 2026-11-18-` (1280 wide, quality 82, about 40 KB each). `playtest.ps1 all` (9 scenarios) takes about 5.5 minutes with the editor up.
