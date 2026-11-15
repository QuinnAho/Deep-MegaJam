---
updated: 2026-11-14 21:30 EST
verified: 2026-11-14 21:29 EST, build-sheriff ran tools\ue.ps1 build, test and verify on this machine (all PASS, plus a deliberate failing test and a deliberate compile error that both reported FAIL). package, zip, smoke and editor are written but not yet run. Tiers T2 to T5 and the MCP tool behaviour are still unverified design.
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

- T0 and T1 on every code task.
- T3 regression before merging gameplay changes.
- T4 every three hours or so, and at feature freeze.
- T5 at each milestone.

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
- `package`, `zip`, `smoke` and `editor` are written but not yet run.
