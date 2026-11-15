---
updated: 2026-11-15 12:50 EST
---
# Board

The agents' work queue. Design rules are in [[GDD]], dispatch and testing rules in `.claude/loop.md`, every run in [[Agent Runs]]. Status words: `todo`, `doing`, `blocked`, `done`.

## Handoff for a fresh orchestrator session (rewritten 7:00 AM Nov 15, updated 1:50 PM)

A new session needs only `CLAUDE.md`, this note and `.claude/loop.md`. The history of the night is in [[Devlog]] and [[Agent Runs]]. This section holds only what is true now.

**Where things are**

- `main`: code as of `0874fe5` (52 tests), plus notes. Quinn has not merged the polish pass yet.
- `polish/integration` on origin at `59af289` (1:41 PM: the cave shaft and the caverns; verify 107 of 107, a package with 0 usage-flag lines, smoke, a 762.6 MB zip, and S2, S7, S8, S9, S10, S11 and S12 passed; before it `6aeafc8` at 12:56 PM: the Captain lane, 94 tests; before it `ea29122` at 12:33 PM, the sonar; and `a2e696d` at 12:06 PM: the first-five-minutes pass and the level view aboard merged; verify 79 of 79, package and smoke green, zip 762 MB, 0 usage-flag lines; all 14 scripted playthroughs passed on `ad644df` in 745 s, and S1, S4, S5, S6, S7 again on `a2e696d`): everything built since. Water, the room, hull and salvage, the driving view, the serpent with the dragon's head (two passes: the join, the bite staged on the line, the shoulder thinned), the HUD, settings, credits and a title menu, the fast playtest script (S0 to S12), the ending fix, two passes of rock and ruin (the placement loader, the cook hook, floor rock at every stop, the 500 m well with the relic as a carved stone sphere, real rock on the trench wall, a carved gate at each stop), the tether and the harpoon line as ropes that sag, go taut and visibly part when cut, a hull for the submarine built from basic shapes, a texture limit for the build, and dev log posts two and three (`devlog/02-sunday-before-dawn.md`, `devlog/03-sunday-morning.md`). It also has the lamp that samples its whole cone, a flat floor in the relic's pit, rebuilt silt, and the serpent kept inside the well on the last ascent. Full suite 78 of 78; packaged and smoke-tested at `b7755a1` (11:18 AM), zip 762 MB, no usage-flag lines. All 14 scripted playthroughs passed at `a942e1c`; the ending set has not run on `b7755a1`, because the machine went down as it started. It is checked out in `.wt\monster`, the orchestrator's merge checkout, and the package is in `.wt\monster\Builds`.
- **Local, not pushed and not gated:** `.wt\monster` holds integration at `340332f`. That is the cavern lane's serpent clamp (`ef84d37`: the serpent keeps 3 m off the rock) and the camera lane's fix (`c19f142`: the ping never lights the sub, and wall hits shake the view by speed). The lower roof (`0e14e21` on `polish/cavern`) joins them, and then one gate runs on all three.
- The packaged game matches the editor. The orchestrator took frames of the dragon's head, a pillar at the well and the bottom of the well from the package, and its log has no `missing usage flag` line.
- **Nothing visual merges into `main` until Quinn has seen frames and said "merge".** The frames page URL is in Claude's memory, as is the URL of his board of 61 asset picks. When he says it: merge `polish/integration` into `main` with the gate in `.claude/loop.md`, push, and tell him to restart his editor.

**Now (1:50 PM): the scope changed (D-038 to D-042)**

- **Freeze is 5:30 PM** (D-038). The final playthroughs and package are at 6 PM, the video and itch page run 6 to 7:30 PM, and submission is by 8 PM.
- **Quinn's new scope (D-039):** drive the sub down a winding cave shaft, with a sonar, into a cavern at every stop. Three opus engineers started at 12:02 PM, each in its own checkout. They share one contract: the passage is centred on the parked sub with 14 m radius at every stop, every cave surface blocks `ECC_Visibility` and Pawn, and each part has a switch that gives back today's game.
  - `polish/shaft` in `.wt\pkg`: **done** (`59af289`).
    - It covers steering, the tube, hull hits and breach, `GetHullIntegrity01()`, the Captain hearing the hull, and a plain hull bar above the sonar.
    - It also gave each procedural rock mesh its own collision cook key (`World/DeepCookKey.h`), because UE 5.9 caches procedural collision by simple geometry only.
    - Gated and pushed at 1:41 PM.
    - Left:
      - squeezes close under the 1.5 m/s hit speed, so they only scrape;
      - `Deep.Cave.Goto cavern` stops at the 200 m limit;
      - the SUB marker sits on LIMIT 200 (finish, for the HUD pass).
  - `polish/cavern` in `.wt\dev`:
    - walls and a roof round each stop, gated with the shaft;
    - the serpent kept 3 m off the rock (`ef84d37`, merged locally);
    - the lower roof (D-042), committed as `0e14e21`. Frames, and a check that the descent costs no hull, are still to come.
  - `polish/sonar` in `.wt\text`: **done** at about 12:25 PM (`d8c2f53`). It merged into integration as `ea29122`, passed the gate and the S0, S2, S6 and S7 playthroughs, and was pushed at 12:33 PM. The hull bar it was to build next came with the shaft lane (`8ac8efc`).
  - Every `tools\ue.ps1` call by an engineer holds the `Global\DeepUnrealSlot` mutex. The orchestrator's playtest script also waits for any other `UnrealEditor.exe` to close. One Unreal process at a time, since the 11:19 crash had two editors up.
  - When they report, expect conflicts in `DeepArena.cpp` (shaft: `ApplySubDepth`; cavern: `EnsureArena`), `Sim/DeepTuningTypes.h` (three new tuning structs) and the CSVs (rows appended at the end). After each merge, run the scripted playthroughs that drive or swim (`S2-gate`, `S7-run`, `S9-floor`, `S10-relic`, `S11-pass`, `S12-sidehome`). Quinn sees frames before any part is called done.
- **Started after the scope change (D-040, D-041):**
  - `polish/captain` in `.wt\voice`: **done** at about 12:43 PM, all four steps (`0063051`). It covers the radio voice with squelch and ducking, the pacing, the danger tone, music slots by depth, breathing that follows the danger, and up to three variations per sound. Merged into integration as `6aeafc8` and pushed at 12:56 PM. The gate is green (94 tests, a package with no usage-flag lines, smoke, a 762.5 MB zip), and S0, S6, S7 and S10 passed. The names and paths in D-040 are fixed, because Quinn's brother records to them ([[Sound Brief]]). Waiting on files:
    - point the six serpent rows in `Sounds.csv` at the files, with pitch back to 1.0;
    - map the Fab pack's rows;
    - import `audio-drop/`, if Quinn allows it.
  - `polish/camera` in `.wt\camera`: **done** (`c19f142`), merged locally as `340332f`.
    - Shakes are on, now also on wall hits, scaled by impact speed.
    - The footage look, the sonar pulse and interference stay behind their switches until Quinn decides.
    - The ping skips a box round the sub, so it never lights the hull (0.03 ms on the GPU).
    - The recipe is in `03 - Research/Camera - Footage Look and Sonar Pulse Recipe.md` on that branch.
    - Plan: turn the pulse on by default unless Quinn says no.
  - Both were told at 12:35 PM to merge `origin/polish/integration` (`ea29122`, with sonar). At 12:57 PM the shaft, cavern and camera lanes were told to merge `6aeafc8` before they report.
  - The new lines are in [[Captain Script]]: 74 of them, with no numbers in the voice. The Captain lane copies the CSV block.
  - The HUD pass: one planned pass (D-028), in the look Quinn approves.
    - The mockup is done: [[HUD Mockup Spec]], with the pictures on a private page for Quinn.
    - It offers A, the instrument HUD, or B, the diver's camera display, which costs about an hour more to build.
    - An Opus engineer builds his pick.
  - The brother's files land in `C:\Repositories\Deep-MegaJam\audio-drop\`, which git ignores. When the Fab pack lands, add its folder to `.gitignore` and link it into `.wt\monster`, `.wt\voice` and `.wt\camera`, the way DragonCave is linked.
- `polish/aboard` (`2f27da3`, pushed): coming aboard levels the view (`Arena.AboardPitchDeg`, test `Deep.World.SubAboardPitch`, `Deep.World.Sub` 9 of 9). Merged at 12:06 PM (`a2e696d`).
- Bubbles are parked. The fourth try, half-built at the crash, is committed unfinished on `polish/fx-wip4` (`f7a3fce`). `polish/fx` has all effects off by default.
- The crash (11:19:45 AM, Kernel-Power 41, no bugcheck): the orchestrator and all three engineers died. Every lane that had committed work survived, and `polish/wellserpent` was merged and pushed (`b7755a1`).
- The frames page is current: version 16 at 1:45 PM. It has the opening, the well, the camera, the shaft and the caverns, and the 2:15 asks. The camera fix frames go on next. The generator is in the old session's scratch folder (`C:\Users\quinn\AppData\Local\Temp\claude\4cf960466880eab9\f3fa3667-c758-4612-8d67-d2bda7e089c8\scratchpad\frames_gen\`; Python is `py`).
- Left from the opening: the walls by the start spot are near black, the serpent's first sight is faint, and the boulders at the 300 m gate are pale grey.

**How a lane is merged**

- When one reports: look at its frames yourself, write the verdicts, merge into `polish/integration`, run the gate **and a package**, read the packaged log, take a frame from the packaged game, push, and add the frames to the page. A lane whose frames fail goes back to its engineer with a written review. Working parts may merge; a failed frame is never called done.
- Scripts for this are in the session's scratch folder and are worth rewriting if lost: one runs `verify`, then `package` and `smoke` inside the Unreal slot and `zip` after it; one takes a frame from the packaged game with `-DeepShot`.
- A resumed engineer with a long history costs more per minute than a fresh one (691k tokens for the 42-minute second pass on the well, 492k for the 66-minute first). Resume for a review of the same work; start fresh for new work.

**Scripted playtests on the merged build (9:06 AM, `5ae5f0b`)**

- `S7-run` passes: title to win. `S10-relic` passes: down the shaft by keys, the relic taken, winched home in 21 s with the lamp aimed for 1.3 s, one repel, no bites, a win.
- A first try of `S10-relic` failed only because the editor was still loading. Run a warm-up scenario first (`S0-title`), and give the playtest its own port (8013), not the agents' 8012.
- The playtest overwrites tracked screenshots in `devlog/media`. Put them back with `git checkout -- devlog/media` before the next merge.
- **The playtest leaves its editor open** when a later call finds the port already answering. At 9:11 AM that editor held two material files and a merge in `.wt\monster` failed half-way; the gate then refused to build ("an editor is running on this project"), so nothing wrong was built. After a playtest, check for an `UnrealEditor.exe` on that checkout and port and stop it before merging.

**The lamp and the ending as played (the `polish/ascent` lane, merged)**

- Merged and pushed (`a942e1c`). Gate 77 of 77, package and smoke green, zip 762 MB, and **all 14 scripted playthroughs pass** (13 in the set at 10:34 AM, and `S3-serpent` alone at 10:37 AM after the script learned to wait for a busy editor).
- `S10-cutdrop` now passes. It had failed 2 of 2: a diver cut at the very bottom of the shaft could not swim out. With the pit's flat floor the diver drops the relic and gets home on bailout air. That settles the third of Quinn's three calls on the ending, by script; a try by hand is still worth it.
- Checked in the packaged game with ray tracing switched off (`Deep.RayTracing 0`): the relic in the lamp and the hull at the hatch both look right, so a machine without ray tracing gets an acceptable picture.
- The lamp samples its whole cone and no longer burns the serpent, the pit floor or the hoard. Left from that lane: the serpent in the shaft never caught in a frame, the hard edge of the beam up the shaft, a dim lamp pool at 100 m, the pit wall nearly black at 1 m. The bite with the lamp off went to the `polish/opening` lane.
- For Quinn: the view now tips down at least 45 degrees as the diver drops through the hatch (`Arena.HatchDropPitchDeg`). It makes the bed and the well's glow the first thing seen. It is a change to the camera at every dive.

**An open editor blocks every build (found 10:14 AM)**

- An Unreal editor starts Live Coding by itself, and while it runs UnrealBuildTool refuses every build that uses the same installed engine, in every checkout: "Unable to build while Live Coding is active". A package failed on it while another lane had an editor open for asset work.
- `tools/ue.ps1 editor` now passes `-LiveCoding=0` (integration, `5a5f8ab`). Lanes cut before that must add it by hand; the two engineers running at 10:15 AM were told.
- **Quinn's own editor does the same.** When he opens the main checkout in Unreal 5.9, every agent build and the merge gate stop until he closes it or turns Live Coding off (Editor Preferences, Live Coding, untick Enable; or start the editor with `-LiveCoding=0`).
- To see who holds it: list `UnrealEditor.exe` processes with their command lines. Never stop one you did not start.
- Port 8012 is shared by the agents' editors, and at 10:13 AM one engineer's calls went to another's editor because it did not re-check the port. Give each lane that needs an editor its own port in the brief (8012, 8014, ...; 8013 is the playtests'), and say to check the port before every batch of calls.

**Left over from the well pass (small)**

- A fine smear across the trench rock while the submarine moves. **Accepted as it is.** The rock writes correct velocity; the smear is the upscaler resampling its history while fine rock slides past, and the settings that govern it are project-wide. If time is left at the end, the one local thing to try is a temporal-responsiveness output on `M_DressSilt`.
- The beam up the relic's shaft has a hard edge and starts at a small bright square.
- The gate at 500 m was not checked in a frame. The gate rows were added after that lane's last test run; the merge gate has run since.
- `T_StoneDebris01_ORM` does not fit the skin's sampler and is left out, with a warning in the log.
- Pack pieces have no collision. They are kept 1.5 m clear of every diver's path, but a diver who strays can swim through a pillar.

**Not yet met for the submission, by risk**

No 30 to 60 s gameplay video (OBS is installed on Quinn's PC; the machine has no ffmpeg, so a scripted recording has no encoder). No itch.io page (draft in `devlog/itch-page.md`). Nobody has played by hand. No run on a second machine. The Official Rules PDF is unread. Team name and Epic account emails. No public dev log link, which Glass Box needs. The full list is in `01 - Jam/Submission Checklist.md`. No checkout is free.

**The build's size (the limit is 1 GB zipped)**

- `deep/Config/DefaultDeviceProfiles.ini` limits plain world and character textures to 2048 and drops the top mip of every virtual texture (`LODBias_VT=1`). Without it the zip was 1,318 MB. With it: 762 MB with the dragon's head and both rock passes.
- If more packs arrive and the zip nears the limit, `LODBias_VT=2` is the next step. Never use a flat `MaxLODSize_VT`: the pack's big textures are grids of 4096 tiles and it shrinks each tile to 256 pixels.
- `tools/jam.json` still has the placeholder team name, so the zip is called `TeamName_DEEP.zip`.

**The Dragon Cave pack (D-036)**

- Quinn installed it at 3:31 AM through his own import project (`C:\Repositories\DeepImport`, Unreal 5.7). That editor is still open and is his: leave it alone. The pack is at `deep/Content/DragonCave/` in the main checkout: 4.8 GB, 333 files, kept outside git by a line in `.gitignore`.
- It holds four static poses of a whole dragon (no skeleton, no separate head), cave rocks and large rocks, stone walls, columns, arches and stairs, skeletons, skulls, treasure, old timber and dead trees.
- Each agent checkout sees it through a link at `.wt\<name>\deep\Content\DragonCave`. **Before removing a worktree, remove the link first (`cmd /c rmdir <link>`), and never run `git clean` in a worktree.** Either could delete Quinn's files through the link.
- Only what the game uses may be cooked. The limit is 1 GB zipped, and the last zip was 427 MB. `deep/Content/Data/PackAssets.csv` lists the pack assets that code loads by path; the cook hook that reads it is part of the dressing lane. **Cut a package only from a checkout that has the link**, or the pack's meshes are missing from the build.

**The ending (D-035)**

- The 5:50 AM alarm, "the game cannot be won", was half wrong. The scripted player winched in bursts, and that is why it was cut. A player who simply held the winch won every time and never needed the lamp, because the winch was as fast as the serpent. So the ending was not impossible. It was trivial, or, for a diver cut in the shaft, a trap.
- Fixed on `dev/win` and merged. With the relic the winch runs at 4 m/s, not 7. The relic's shaft is straight under the hatch (`Arena.RelicOutM` 8 to 0) and wider (`Look.SeabedPitRadiusM` 2.5 to 4.5).
- Scripted results: with the lamp, 5 of 5 win after a 21 s ascent. Without it, 0 of 3 win, and all three drop the relic and get home alive. The full-run scenario now winches the relic home and passes.
- **Quinn decides:** (1) the relic straight under the hatch is a layout change; (2) whether the serpent should hunt the moment it cuts the line, so a careless diver dies and does not only lose the relic; (3) a diver cut at the very bottom of the shaft could not swim out in the script (`S10-cutdrop` fails 2 of 2), which needs a try by hand.

**Next checkpoint: when both second passes are in**

1. Merge each with the gate and a package, as above. Read the packaged log for `missing usage flag`, and take one frame of the rock and one of the dragon's head from the packaged game.
2. The scripted playtest set, once (D-031 allows it at a checkpoint).
3. Update the frames page and tell Quinn.
4. A scripted run recorded as a fallback 30 to 60 s gameplay video.

**Waiting on Quinn**

- His look at the frames page and the word "merge". The three ending decisions above. D-037: the relic becomes a real object and the well holds its light.
- The Fab listing for Dragon Cave says "personal" licence. He should check that it covers a public jam entry. The pack is credited in `Credits.csv` and is not in git.
- More packs, if he wants them: Megascans rocks and a silt or sand surface, Modular Ship Interior, Industrial Infrastructure. The room and the hull are still boxes.
- The DLSS plugin download for the NVIDIA award (D-032). An OpenAI key for the Captain's voice.
- By hand: a key press on the title of a packaged build, listening to the sound, volume and mouse feel, and whether settings survive a restart. The newest packaged build is `.wt\monster\Builds\Windows\Deep.exe` (from `6746147`: the first rock pass and the dragon's head; in it the rock still wears the default grey material).
- Team name, student status, Fun Ideas picks, and where the public dev log is hosted.

**Standing reminders**

- Polish rules: `03 - Research/Polish - Start Here.md` and `02 - Design/Style Guide.md`. Every polish brief links to both. No half-done polish (D-028), one style (D-030).
- Every Unreal launch goes through the machine-wide slot. The snippet is in `.claude/loop.md`, and every brief must carry it, because the role files do not.
- Scripted playtests and packages run only at checkpoints (D-031).
- Sunday: feature freeze 5:30 PM (D-038), final playthroughs and package 6 PM, video and itch page 6 to 7:30 PM, submitted by 8 PM. The deadline is 8:59 PM.
- Still unconfirmed from the research: the Official Rules PDF, seven sound needs with no candidate, and most Freesound licences.
- After merging a branch that adds dev log entries, remove any repeated `## Sunday Nov 15 2026` heading in [[Devlog]].
- In a packaged build the start command is ignored for about the first four seconds after launch.

## State of the game

`main` is at `0874fe5`. 52 tests passing. Nobody has played it by hand yet, and nobody has heard it.

- **The run, start to finish:** title, then standing in the submarine's one room. Drop through the hatch to dive: grab, weight, tether, winch, the Captain's Patience, the cut, bailout air, the harpoon. Back up through the hatch. Bank at the console and buy the gates there. Drive from the helm between stops at 100, 300 and 500 m. The relic at stop 3, then the win screen with a performance review. Pause and restart work.
- **Layer gates (D-023):** the helm stops at 200 m without the flashlight (50 cr) and at 400 m without the hull upgrade (200 cr). The serpent appears in layer 2 and is aggressive in layer 3.
- **Sound (D-026):** every sound is generated in code. 43 events play one, and seven loops fade in and out (sea, room, breathing, fast breathing, winch, serpent, helm). One row each in `deep/Content/Data/Sounds.csv`. Commands: `Deep.Audio.List`, `Deep.Audio.Play <event>`, `Deep.Audio.Mute 0|1`, `Deep.Audio.Reload`.
- **Plain on purpose:** grey boxes, even light, no fog (D-020). No "Director" in the game's text (D-025).
- **Scripted playtests:** eight scenarios in `tools\playtest.ps1`: S0-title, S1-hatch, S2-gate, S3-serpent, S4-cut, S5-harpoon, S6-bank, S7-run. All eight passed twice before the sound merge; S1 (which checks a sound fired) and S7 passed after it.
- **Last package:** 707 MB on disk, 426.5 MB zipped, built from `ce9c4c8` with the sound layer in it. Cook clean, `Sounds.csv` inside, smoke run clean. The limit is 1 GB zipped.
- **Parked, not in `main`:** the stopped look pass, on `look/pass1`.

## Known rough edges

Function, fixed in batch one (merged 1:29 AM as `6f2e259`):

- The title: a real key press already worked through the viewport; the playtest tool's simulated key took another path. Both now start the run, the text says PRESS ANY KEY OR CLICK TO START, and a test presses seven keys and buttons. Still unverified: a physical keyboard.
- `tether_distance_m` is right after a cut.

Function, from the playthrough on simulated keyboard and mouse (merged 2:10 AM as `b8b4037`):

- Returning to the hatch from the side: fixed in code and merged (`0874fe5`). The diver now slides down and under the hull, and the winch routes round it. **Not yet seen working in real play.** Quinn checks by hand, or the playtests do at the next checkpoint (D-031).
- The hatch on the console-to-helm walk: fixed and merged (`0874fe5`). The start position, the console stand and the helm moved to the console's side of the room.
- Works on simulated input (scenario `S8-keys`): the title key, walking to and through the hatch, E to grab, the return from below, E at the console, the settlement screen, E at the helm, holding the dive key and parking at 300 m, the lamp, the winch, the cut, the harpoon.
- Still needs a human: the Esc pause menu (the input tool cannot reach it), how the mouse feels (the code gives 0.175 degrees per count, which is normal; the tool's odd reading was the tool), and anything in multiplayer.
- E on the settlement screen does not use the console twice (checked on simulated input).
- No gamepad support. In the editor the game window may need a click before keys arrive.
- Every number is a guess: room size, walk speed, hatch size, harpoon speed and pull, serpent values.
- `Deep.Teleport` sometimes lands in the wrong place.

Finish, parked for the polish pass (D-028: do not patch these one by one):

- Driving does not feel like moving. The helm view is flat grey; only the gauge shows the descent.
- The HUD is a placeholder and looks it. Panels show in the wrong mode: the console panel at the helm, "LIMIT 400" over the depth gauge, the controls panel still up under the hatch, the status panel through the win screen.
- Every sound is generated in code and unheard. The final build ships none that Quinn has not listened to and kept.

## Running now

The overnight runs are in [[Agent Runs]].

| Lane | Agent | Task | Started |
|---|---|---|---|
| Cave shaft | Opus, high, `.wt\pkg` on `polish/shaft` | **Done** (`59af289`): steering, hull hits, the cook key and a hull bar. Gated and pushed at 1:41 PM. | 12:02 PM |
| Caverns | Opus, high, `.wt\dev` on `polish/cavern` | Walls and roof gated at 1:41 PM; the serpent clamp is `ef84d37`. Now: the lower roof (D-042), committed as `0e14e21`, with frames due. | 12:02 PM |
| Sonar | Opus, high, `.wt\text` on `polish/sonar` | **Done**, `d8c2f53`. Merged as `ea29122` and pushed at 12:33 PM. | 12:02 PM |
| Captain and sound | Opus, high, `.wt\voice` on `polish/captain` | **Done** at about 12:43 PM (`0063051`): radio voice, pacing, danger tone, music slots, breathing by danger, variations. Merged as `6aeafc8`, gate green, pushed at 12:56 PM. Waits for sound files. | 12:17 PM |
| Captain lines | Sonnet, medium (writer) | **Done**: 74 lines, no numbers in the voice. [[Captain Script]] | 12:17 PM |
| Camera | Opus, high, `.wt\camera` on `polish/camera` | **Done** (`c19f142`). First frames came at 1:13 PM; then the fix: the ping skips the sub, and wall hits shake by speed. Merged locally as `340332f`; the gate is next. | 12:25 PM |
| HUD mockup | Sonnet, high, scratchpad only | **Done** at 1:11 PM: [[HUD Mockup Spec]] and a private page with both directions. Waiting for Quinn's pick of A or B. | 12:49 PM |

## Next, in order

| # | Item | Owner | Status |
|---|---|---|---|
| 1 | Play the whole run by hand and say what is wrong with how it plays. | Quinn | todo |
| 1a | Prove the loop with simulated key and mouse input, and list where real input fails. | Sonnet agent | done, `b8b4037` |
| 1b | **Fast scripted playtests.** Parked on `parked/fast-playtests` (`4b710b8`). One in-process MCP session with polling takes S0 to S7 from 472 s to 174 s, all passing with the same end states. Left to do when playtests resume: `S8-keys` fails on the new script (a key-hold timing; a fix is written and untested), the `-Speed` option and the background-throttle flag are untested, and `S9-side-return` is not written. | Sonnet agent | deferred (D-031) |
| 2 | Fix batch two: what the playthrough found. | Opus engineer | done, `0874fe5`; proof in real play pending (`S9-side-return`) |
| 3 | Real sound replaces the generated sound (D-040): Quinn's Fab pack for the world, and his brother's music and serpent voice, imported under `/Game/DeepAudio/` with the names in [[Sound Brief]]. The Captain-and-sound lane wires them in. | Quinn, his brother, Captain lane | doing |
| 4 | The Captain's voice. The script is written ([[Captain Script]], 74 lines). Still to choose who records it: Quinn's brother, OpenAI text to speech, or nobody (text only). | Quinn | waiting on Quinn |
| 5 | **The polish pass (D-028, D-029):** look, HUD, effects, sound and voice, in one planned pass to a professional standard. A priority equal to gameplay. The research and the plan are done ([[Polish - Plan and Techniques]]). | Quinn chooses, agents build | waiting on Quinn's choices |
| 6 | Water and rope feel (D-024): swim inertia, a tether and harpoon line that sag and snap, bubbles and silt. Part of the polish pass. | engineer, Quinn tunes | later |
| 7 | Ideas Quinn picks from [[Fun Ideas]]. Live Captain brain decision about 1 PM Sunday (D-018). | per idea | waiting on review |
| 8 | Sunday: feature freeze 5:30 PM (D-038), final package 6 PM, video and itch page by 7:30 PM, submitted 8 PM. **Run the scripted playtests before each package** (D-031); they are not run in between. The itch.io page says in one sentence how the game answers the theme (D-025). | all | todo |

## Waiting on Quinn

**Now (1:50 PM):**

- The Fab sound pack: add it, and tell the orchestrator its folder name. What to look for is in [[Sound Brief]].
- His brother's music and serpent sounds go into `audio-drop\`: the first files by 2 PM, the rest by 3:30 PM ([[Sound Brief]]).
- Two questions for Quinn:
  - May the Captain-and-sound agent import those files headless into `deep/Content/DeepAudio/`? That would be a new exception to the binary-asset rule. If not, Quinn imports them himself, telling the orchestrator first, because an open editor blocks the builds.
  - Who voices the Captain: his brother, OpenAI text to speech (with Quinn's key, kept out of the repo), or text only?
- **At about 2:15 PM, two calls:**
  - the footage look, yes or no;
  - the HUD, A or B.
  Both are on his frames page and the HUD mockup page. Advice: B if the footage look is yes, A if it is no. With no answer by about 2:30, build A, which costs an hour less.
- Tell Colby that the brother is making the serpent and the music, so the two of them don't both make those sounds.

**Older, still open:**

- Restart the editor, play the run, and send notes on how it plays. First: does a key press on the title start the game?
- **The polish choices:** the style, the packs to install (mark them on the look board), whether agents may create binary assets during the pass, the sound source, the Captain's voice, and whether the code-only packages start tonight.
- The Captain's voice: an OpenAI key, or a built-in Windows voice as a stand-in.
- Team name, and whether both are enrolled students. The zip is still named from the placeholder `TeamName` in `tools/jam.json`.
- Which of the [[Fun Ideas]] to build.
- Where the public dev log lives (the draft is a file in `devlog/`).
- Share the Brief doc with Colby. It still says "the Director". Delete the stray notes in the vault root (`Welcome.md`, the empty `GDD.md`, the empty co-op note).

## For Colby

Models: submarine first, then salvage pieces, relic, serpent head. Sounds: breathing, winch, tether snap, serpent, ambience, one music loop; each replaces a generated sound by one row in `deep/Content/Data/Sounds.csv`. Dev log post, gameplay video and itch.io page on Sunday.

## Checkouts

The main checkout is Quinn's and he may keep the editor open on it (port 8000: never call it without his say). Agents build only in the `.wt\` checkouts (`dev`, `pkg`, `text`, `voice` and `camera`) and push branches. `.wt\monster` is the orchestrator's merge checkout. The orchestrator runs the merge gate in the branch's own checkout, merges it into `main`, and tells Quinn to restart the editor. In use at 1:50 PM: `.wt\dev` (the caverns). Free: `.wt\pkg`, `.wt\text`, `.wt\voice` and `.wt\camera`. One Unreal editor at a time: the machine has 32 GB of RAM.

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
| The title starts on any key or click; tether distance fixed | 1:29 AM | `6f2e259` |
| Polish research: assets, sound and voice, the plan; a visual board for Quinn | 1:44 AM | `6ff4a2a` |
| Six research notes for the polish pass, with one index note | 2:10 AM | `7256f6c` |
| The first stop played on simulated keyboard and mouse (`S8-keys`) | 2:10 AM | `b8b4037` |
| The diver can return to the hatch from any side; the hatch is off the walking line | 2:27 AM | `0874fe5` |
| The style guide: dark realism led by light, as numbers | 2:46 AM | `design/style-guide` merge |
