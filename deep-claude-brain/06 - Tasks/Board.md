---
updated: 2026-11-15 02:56 EST
---
# Board

The agents' work queue. Design rules are in [[GDD]], dispatch and testing rules in `.claude/loop.md`, every run in [[Agent Runs]]. Status words: `todo`, `doing`, `blocked`, `done`.

## Handoff for a fresh orchestrator session (written 2:56 AM Nov 15)

A new session needs only `CLAUDE.md`, this note and `.claude/loop.md`. **The polish pass has started (D-033, Quinn's go at about 2:47 AM).** Two Opus engineers are running, and their reports go to the session that started them, so a new session checks their branches: (a) `polish/water` in `.wt\dev`: the sea at each depth, the lamp, particles, the hatch pool, ray tracing; (b) `polish/monster` in `.wt\monster`: the serpent's body, skin, markings, eyes and motion. **Neither merges until Quinn has seen its frames** (`devlog/media/2026-11-15-water-*.png` and `2026-11-15-serpent-*.png`). When he approves: run `verify` in the branch's checkout, merge, push. Every polish brief links to `02 - Design/Style Guide.md` and `03 - Research/Polish - Start Here.md`. Next lanes, once one of these frees a checkout: the placement loader (plan WP2), then the room inside and the HUD. Waiting on Quinn: installing packs (Megascans rocks, Modular Ship Interior, Industrial Infrastructure, and Dragon Cave, which has realistic dragon meshes that could serve as the serpent's head and the 500 m site); the DLSS plugin download for the NVIDIA award (D-032); an OpenAI key for the Captain's voice.

1. **Polish research is in (D-029). Start with `03 - Research/Polish - Start Here.md`:** it lists the rules for the pass, what Quinn still has to choose, the six research notes, and the amendments the later research made to the plan. Quinn has a private visual board of the 61 asset picks (its URL is in Claude's memory, not in the repo). **Nothing visual is dispatched until Quinn has chosen the style and said go.** Once he picks the style, write `02 - Design/Style Guide.md` first: palette, materials, lighting, type sizes, animation timings and volume levels as numbers.
2. **When he says go:** dispatch from section 9 of the plan. WP1 (water look), WP2 (dressing loader) and WP6 (HUD) are code only and need no packs. Opus at high for those. The plan and the asset scout disagree on Corals and Atlantis Ruins: the style choice settles it.
3. **Still unconfirmed in the research:** the Official Rules PDF, seven sound needs with no candidate, most Freesound licences, the download size of every pack, and how long the first cook with pack materials takes.
4. **Scripted playtests are deferred until a checkpoint (D-031, Quinn at about 2:30 AM).** The fast-playtests agent (`.wt\pkg`) was told to stop, keep its work on `tools/regress` if it runs or on `parked/fast-playtests` if it is half-done, and close its editor. Check that no Unreal editor is left running and that `.wt\pkg` is clean. Do not dispatch playtest work until a package is about to be cut. Deferred with it: proof in real play that the diver gets home from the side (`0874fe5`), which Quinn can check by hand.
5. One Unreal editor at a time: the machine has 32 GB of RAM and about 6 GB free with no editor open.
6. **Polish is one planned pass (D-028), and a priority equal to gameplay (D-029).** Research and planning now. No HUD, look, effect or sound-finish work is dispatched until Quinn has chosen from the research and given the pass a start time.
7. The three later research notes (engine skills, outside tools, polish craft) are merged and folded into `Polish - Start Here.md`.

After merging a branch that adds dev log entries, remove any repeated `## Sunday Nov 15 2026` heading in [[Devlog]].

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

| Lane | Agent | Task | Started |
|---|---|---|---|
| Fix batch one | Opus, high, `.wt\dev` on `dev/feel1` | **Done 1:29 AM**, merged as `6f2e259`. Title key press and tether distance. 46 of 46 tests. The cut HUD and motion work is parked on `parked/motion-hud`. | 1:01 AM |
| Assets | Sonnet, medium, `.wt\monster` on `research/assets` | **Done 1:39 AM**, merged as `2e3eed5`. 61 picks, 25 owned and 36 free. | 1:32 AM |
| Sound, voice, fonts, references | Sonnet, medium, `.wt\text` on `research/polish-web` | **Done 1:36 AM**, merged as `c019cfb`. | 1:32 AM |
| The polish plan | Opus, high, `.wt\dev` on `research/polish-plan` | **Done 1:40 AM**, merged as `6ff4a2a`. Ten work packages in three lanes. | 1:32 AM |
| Real-input playthrough | Sonnet, medium, `.wt\pkg` on `tools/regress` | **Done 2:10 AM**, merged as `b8b4037`. `S8-keys` passes; two game problems found. | 1:39 AM |
| Fix batch two | Opus, high, `.wt\dev` on `dev/fix2` | **Done 2:27 AM**, merged as `0874fe5`. 52 of 52 tests. | 2:12 AM |
| Fast playtests | Sonnet, medium, `.wt\pkg` on `tools/regress` | **Stopped 2:30 AM** on Quinn's call (D-031). Its work is kept on a branch for the next checkpoint. | 2:12 AM |
| Engine skills | Sonnet, medium, `.wt\dev` on `research/engine-skills` | **Done 2:07 AM**, merged as `7256f6c`. | 2:01 AM |
| Outside tools | Sonnet, medium, `.wt\text` on `research/ext-tools` | **Done 2:04 AM**, merged as `7c15b45`. | 2:01 AM |
| Polish craft | Sonnet, medium, `.wt\monster` on `research/polish-craft` | **Done 2:03 AM**, merged as `e58a6e1`. | 2:01 AM |
| Dev log post one | Sonnet, medium, `.wt\text` on `devlog/post1` | **Done 1:03 AM**, merged as `c263c9a`. `devlog/01-saturday-night.md`: the first five hours, for the Glass Box prize. A file only; nothing published. | 1:01 AM |

## Next, in order

| # | Item | Owner | Status |
|---|---|---|---|
| 1 | Play the whole run by hand and say what is wrong with how it plays. | Quinn | todo |
| 1a | Prove the loop with simulated key and mouse input, and list where real input fails. | Sonnet agent | done, `b8b4037` |
| 1b | **Fast scripted playtests.** Parked on `parked/fast-playtests` (`4b710b8`). One in-process MCP session with polling takes S0 to S7 from 472 s to 174 s, all passing with the same end states. Left to do when playtests resume: `S8-keys` fails on the new script (a key-hold timing; a fix is written and untested), the `-Speed` option and the background-throttle flag are untested, and `S9-side-return` is not written. | Sonnet agent | deferred (D-031) |
| 2 | Fix batch two: what the playthrough found. | Opus engineer | done, `0874fe5`; proof in real play pending (`S9-side-return`) |
| 3 | Real sound files replace the generated ones: import under `/Game/Audio/`, put the path in that row of `Sounds.csv`, run `Deep.Audio.Reload`. Sources: Colby, or WeLoveIndies from the jam perks. Quinn owns no audio on Fab. | Colby, Quinn | todo |
| 4 | The Captain's voice. | writer | blocked on Quinn's choice |
| 5 | **The polish pass (D-028, D-029):** look, HUD, effects, sound and voice, in one planned pass to a professional standard. A priority equal to gameplay. The research and the plan are done ([[Polish - Plan and Techniques]]). | Quinn chooses, agents build | waiting on Quinn's choices |
| 6 | Water and rope feel (D-024): swim inertia, a tether and harpoon line that sag and snap, bubbles and silt. Part of the polish pass. | engineer, Quinn tunes | later |
| 7 | Ideas Quinn picks from [[Fun Ideas]]. Live Captain brain decision about 1 PM Sunday (D-018). | per idea | waiting on review |
| 8 | Sunday: feature freeze 3 PM, final package and video 5 PM, submitted 8 PM. **Run the scripted playtests before each package** (D-031); they are not run in between. The itch.io page says in one sentence how the game answers the theme (D-025). | all | todo |

## Waiting on Quinn

- Restart the editor, play the run, and send notes on how it plays. First: does a key press on the title start the game?
- **The polish choices:** the style, the packs to install (mark them on the look board), whether agents may create binary assets during the pass, the sound source, the Captain's voice, and whether the code-only packages start tonight.
- The Captain's voice: an OpenAI key, or a built-in Windows voice as a stand-in.
- Team name, and whether both are enrolled students.
- Which of the [[Fun Ideas]] to build.
- Where the public dev log lives (the draft is a file in `devlog/`).
- Share the Brief doc with Colby. It still says "the Director". Delete the stray notes in the vault root (`Welcome.md`, the empty `GDD.md`, the empty co-op note).

## For Colby

Models: submarine first, then salvage pieces, relic, serpent head. Sounds: breathing, winch, tether snap, serpent, ambience, one music loop; each replaces a generated sound by one row in `deep/Content/Data/Sounds.csv`. Dev log post, gameplay video and itch.io page on Sunday.

## Checkouts

The main checkout is Quinn's and he may keep the editor open on it (port 8000: never call it without his say). Agents build only in `.wt\dev`, `.wt\monster`, `.wt\pkg` and `.wt\text` and push branches. The orchestrator runs the merge gate in the branch's own checkout, merges it into `main`, and tells Quinn to restart the editor. Free now: all four, once the stopped agent has reported. One Unreal editor at a time: the machine has 32 GB of RAM.

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
