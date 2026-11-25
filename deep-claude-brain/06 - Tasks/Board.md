---
updated: 2026-11-25 01:56 EST
---
# Board

The agents' work queue. Design rules are in [[GDD]], dispatch and testing rules in `.claude/loop.md`, the jam's runs in [[Agent Runs]] (no new rows since the jam). Status words: `todo`, `doing`, `blocked`, `done`.

## Now: the slice (from Nov 15, 10 PM)

The jam is over; DEEP is a polished game now (rules in `CLAUDE.md`). The goal is the slice: the first ten minutes in a real map built in the editor ([[Slice Brief]]). Everything below this section is the jam's history.

Jam entry submitted solo (D-054). The goal now is an enjoyable game. Caves are chunks strung per dive (D-055).

| What | Who | Status |
|---|---|---|
| Push main, tag `jam-final`, move Unreal assets to Git LFS | orchestrator | done (`62c65b3`) |
| Rewrite `CLAUDE.md` for the post-jam rules | orchestrator | done, for Quinn to read |
| Turn on the PCG and Geometry Script plugins (D-052) | orchestrator | done (`6917b6d`); Quinn restarts the editor to load them |
| Commit the `.uproject` EngineAssociation change ("5.9")? | Quinn decides | todo |
| Moving sub: undo D-022 (D-051: the sub moves through a still world; the room is a fixed set; airlock cycle; real collision). Plan in the branch's `Lane Notes/moving-sub.md`, six green commits | agent, opus, `.wt\swim` on `build/moving-sub` | Step 4: built. `5a40577` on `build/moving-sub` is pushed. The still world is on by default, and the room has its own look. Tests: 178 of 178 pass. The S7 run was won in 47 s. It waits for Quinn's OK on the frames (frames page v22). The orange lattice at stop 1 is not from this lane: the `jam-final` build shows it at the same view (`.wt\monster\deep\Saved\Harness\shots\jamfinal-stop1-helm-20261116-032115.png`). Turning off volumetric fog or the stop's cave shell removes it; shadows and props don't. Packaged for Quinn to play: `Builds\Step4-2309` (11:09 PM, Nov 16; package and smoke pass). **Merged** into main at 11:47 PM, Nov 16 (`ab8c1f0`, main merged into the branch first): verify 178 of 178, package 1050 MB and smoke pass. Next: step 5 (removing D-022's moving-world code; the airlock fade came in step 2), after the caves are in the dive. Step 6's cave collision moved into the caves lane (D-064) |
| Asset shortlist for the slice: sub exterior, interior kit, cave kit, with licences | agent, sonnet | done ([[Asset Shortlist - Slice]]) |
| Cave research: UE 5.9's tools, real caves and methods, games to learn from | agents | done ([[Cave Gen - Engine Tools]], [[Cave Gen - Real Caves and Methods]], [[Games to Learn From]]) |
| Cave pipeline: draw the skeleton, generate the rock, dress it with PCG, finish by hand (D-053) | orchestrator | decided with D-055 (chunks) ([[Cave Pipeline]]) |
| Cave spike: two 30 m chunks (tube into restriction, breakdown chamber) joined both ways in `L_CaveSpike`; output only in `deep/Content/Deep/CaveGen/` | agent, opus, `.wt\cave` on `build/cavegen` | done. `6db858d` on `build/cavegen` is pushed but not merged. The pipeline works: bakes take 3 to 4 s, the joints are exact, and a level hull passes with no contact. The look doesn't work yet: the shapes look like pipes, a borrowed rock texture stretches, and ray-traced shadows leave black patches. The chamber measured 18 ms on the GPU, from only 4 samples. The merge waits for round 8 and Quinn's OK on its frames, then a package check; the `EngineAssociation` line does not block it |
| Chunk tool step 2: a stop cavern (C1); a tunnel that turns and drops, with a squeeze (T1); exit and socket data in `deep/Content/Data/CaveChunks.csv` and `CaveChunkSockets.csv`; the shadow fix and a new GPU measurement; a string of chunks placed from the data in `L_CaveSpike` (D-058) | agent, opus, `.wt\cave` on `build/cavegen` | done. `d6bf75b` on `build/cavegen` is pushed. Four chunks (A, T1, C1, B) are placed from the CSVs. The joints meet within 0.31 cm, the hull sweep touches nothing (least clearance 34 cm, in T1's squeeze), C1's sink is 24 m across, and the GPU cost is about 5 ms with Nanite off (the 18 ms was the editor throttling in the background). The generator is editor Python driving Geometry Script now, not the PCG node. The look doesn't work yet: the tunnels read as pipes, with black specks at the mouths |
| Chunk tool round 3: rock shapes (canyons, keyholes, bedding ledges, joints, breakdown blocks), clearance built in around the hull's path, the specks fixed, the PCG spike and its plugin line removed | agent, opus, `.wt\cave` on `build/cavegen` | done. `bbb1a11` on `build/cavegen` is pushed. The hull's clear path is built into each chunk: 0 contacts, least clearance 92 cm in the squeeze (was 34). Both sides of each joint share one 720-point rim, so the specks are gone (gap 0.000 cm). Beds, cracks and fallen blocks read as rock. GPU about 4.4 ms with Nanite off. Still man-made: the 9 m round joints (ringed holes, a porthole), a doorway, a rectangular opening, fingers; no material |
| Chunk tool round 4: no fingers, doorways or round tubes, flared mouths where a joint meets a cavern, an end to the string, and the frame views kept in code. The joint stays the 9 m circle | agent, opus, `.wt\cave` on `build/cavegen` | done. `41d0bc3` on `build/cavegen` is pushed. The doorway, the mine door and most fingers are gone; a new end chunk E pinches shut, so no view sees out; the camera views are in `CaveCameras.csv`. Checks pass: 0 contacts, least room 87 cm, gap 0.000 cm, GPU about 4.1–4.2 ms. From the helm 20 m out, all four joints still read as round tubes. Left: a lintel in the squeeze, straight beds, clay-like rock at C1's entry, E's plain bays |
| The joint shape: one rock outline shared by every joint, with loose rocks over each joint (recommended), or two or three joint types that must match. Either changes D-055 and D-058 | Quinn | decided: two or three types that must match (D-059, about 11:45 PM Nov 16) |
| Chunk tool round 5: two or three joint types that must match (D-059) and a calm start cavern S (D-061) | agent, opus, `.wt\cave` on `build/cavegen` | done, `0e7c183` (pushed, not merged). Bed, rift and key joints; S with a berth, a chimney and the exit joint ahead; the string S, A, T1, C1, B, E. The agent's checks pass (re-run before merge). From the helm the joints read as a slot, a rift and a keyhole, not tubes, but a dark crease rings each rim, S's chimney looks drilled, and S's back reads as a box |
| Chunk tool round 6: walls that run through each joint rim with no crease at 20 m, a natural chimney and a rock back wall in S, then the lintels (A, S's mouth), straight beds, C1's entry rock and E's bays | agent, opus, `.wt\cave` on `build/cavegen` | done, `06770f2` (pushed, not merged). Fixed: S's chimney rings, the lumps around S's way out, the T1-C1 arch, B-E's lintel. Not yet: the keyhole, rim creases at three joints, E's bay (now crumpled), C1's entry rock (faceted); two frame cameras sit in rock |
| Chunk tool round 7: the keyhole and rim creases, E's and S's bays, C1's entry rock and column rings, holes and degenerate triangles, S's mouth (keeping S's size and layout), cameras that must sit 50 cm clear of rock, and the GPU for rounds 6 and 7 back to back | agent, opus, `.wt\cave` on `build/cavegen` | done, `4dfcb90` (pushed, not merged). Fixed: the frame cameras (on the dive path, all 22 at least 1.46 m from rock, a run fails under 50 cm), E's and S's back bays, the T1-C1 creases, holes (0 open edges); GPU unchanged. Not yet: the keyhole (its straight sides are the shape: Quinn's joint call), C1's column rings, E's flat end. Worse: three pale boulders at S's way out |
| Chunk tool round 7b: the three boulders at S come out, E closes in broken rock instead of a flat plug, and a written answer on why the joint rims still read as cut | agent, opus, `.wt\cave` on `build/cavegen` (round 7's agent, follow-up) | done, `20b0dea` (pushed, not merged). Checked on frames: the boulders are out, and E narrows into a choke of broken rock. Left: a few pale rounded blocks and flat roof patches at E, S's mouth walls plain again, E 2.75 m shorter. Rims: surface detail fades to 0 over 6.5 m at each joint plane (3.5 m for fine detail); the fix is shared detail in the joint's frame, which rebuilds all six pieces. Proposed to Quinn: judge the rims in the game first |
| Chunk tool round 8: the joint rims stop reading as cut (one displacement in each joint's own frame, shared by both sides and set by the joint type), the keyhole reshaped as a natural one, then S's mouth walls and E's pale blocks; the hull sweep's box moves 0.94 m aft for BT-01. S's size and layout, the exits and the joint types stay | agent, opus, `.wt\cave` on `build/cavegen` (main merged in first: `b9c5616`, clean) | **merged** `06ba3f1` (11:53 AM Nov 17). Built from about 3:52 AM Nov 17 (Quinn: "fix the rock first", before the caves go into the dive). Part 1 pushed, `debca83`: each joint type has its own rock relief shared by both chunks, a natural keyhole (a tube over a stepped, meandering trench), the sweep box 94 cm aft; every round 7b check holds (sweep 0 / 1168; T1's bend 93.8 cm with the box aft) and 0 degenerate triangles. Round 8b pushed, `7d14fc3`: a new check (`cavestring.lip_checks`, creases over 90° within 3 m of a rim) reads 0 on all five rims; T1's squeeze section leans and pinches (95.3 cm at the squeeze, against 87.5 in round 8); E's choke is tilted slabs under a scarred roof; every round 8 check holds and the GPU cost is equal. My read of the ten 7b-beside-8b pairs (frames page v34, 5:59 AM): the rims, the joints, A's trench and E work. Not yet: past the squeeze T1 is still a square duct (the core's rectangle, which is also the dark centre of Helm_A_T1), and S's bed undercuts read as torn paper and a grille at the mouth and as smears on the walls. Sent back about 5:58 AM for round 8c on those two. Round 8c pushed, `4f2f44a`: T1's irregular walls run its whole length (flat box wall 77% to 5% at the squeeze, 57% to 2% in the canyon); one softer bedding-notch try (Palmer 2007, Slabe 1995) read as a pipe, so S's mouth is back to round 7b's; lips 0, joint gap 0, sweep 0 / 1168, GPU equal, C1 untouched. The squeeze's tightest point is now 118.8 cm (95.3 in 8b, 87.5 in round 8); raising `T1_FLOOR[0]` tightens it but brings back some flat wall. My read (page v35, 6:20 AM): the eight changed views work; Joint_A_T1 keeps one fairly straight dark edge about 20 m in, to check in the dive's light. Quinn's OK about 6:30 AM ("OK, merge and go"; the squeeze: "Drive it first"). The merge gate passed at 11:51 AM on `9a58789` (main merged in): verify (181 tests), S1-hatch, S7-run, package, smoke and two packaged frames; the zip is 911.1 MB (814.1 MB at the soft ping's gate); no material errors in the packaged logs |
| Soft helm ping (D-060): a thin, faint wave on nearby rock only, about one ping every 6 s, a softer and lower sound, cool #BFE3EA instead of amber; `Deep.Sonar.Style` switches old and new for frames | agent, opus, `.wt\text` on `build/sonar-soft` | **merged** 4:15 AM Nov 17 (`4367b3a`, pushed). Quinn OK'd it about 3:45 AM, serpent included (D-063): the serpent turns to each ping but no longer stalks from pings alone. Gate on `0cb1de0`, 3:56 AM: verify 180 of 180, S3-serpent and S7-run pass, package 1101 MB with no missing usage flags, smoke clean, zip 814 MB. Packaged frames at the helm, 4:10 AM: the band lights the near rock at 0.5 s and outlines the far rocks at 1.2 s; the first frame, at 0.8 s, lit nothing because nothing in that view is about 27 m from the sub. The lane's editor frame labelled 0.8 s looks like the packaged one at 1.2 s: the same look at another moment (the lane's logs are gone, so why is unknown) |
| BT-01 on our sub (D-062): the model replaces the basic-shapes dress, fitted to the 12.3 × 3.3 × 5.0 m hull box, behind `Deep.Sub.Model`. First pass `b43e4cf`: the model shows, but the hull still collides as the old 4.8 m wide box, its lights sit inside the model, a square hatch plate shows, and SubHullShape fails. Second pass since 12:15 AM, Nov 17: collision from the model, the hull's box from the model, lights on its surface, the plate hidden. Second pass done, `5d78c5a` (pushed, not merged): the model is the diver's collider (2048 convex hulls; propellers, arms, drill and rope have none), lights on its skin, the plate hidden, its area's tests green. Checked on frames (page v29): reads well from 10 m out; up close the lamp washes the paint out and hard black shapes show on the hull. Follow-up done, `daf3a69` (pushed, not merged): the black shapes up close are shadows, not mesh faults (the unlit frames are clean). On the side they are the Downlight's hard shadows with no fill at depth; on the tower, the diver lamp's (it sits 35 cm right of the eye and 30 cm below). Softening them is look-pass work (D-028). Frames of the tower's side and the green starboard light. Cost with the diver against the hull: frame time unchanged at 6.5 to 6.8 ms (render-bound); game thread 6.6 against 3.7 ms (lamp rays 1.0, hull-slide sweep 0.9, movement 0.8); Chaos 0.18 ms either way. Fewer hulls would cut it later if needed. New cheats `Deep.Sub.Lamps`, `Deep.Swim`, `Deep.Perf`. Quinn OK'd the merge at about 2:50 AM. **Merged** into main at 3:24 AM, Nov 17 (`6ed6929`; main was merged into the branch first, so main fast-forwarded): verify 180 of 180; S1-hatch, S2-gate, S7-run and S12-sidehome pass; package 764 MB with no missing usage flags; smoke passes; zip 480 MB. Two frames from the packaged game look as in the editor (frames page v31) | agent, opus, `.wt\subs` on `build/sub-model` | done, merged |
| BT-01's inside: the room (the fixed set you stand in) changes to fit BT-01 (Quinn, about 2:50 AM Nov 17: "we need to change the interior too"). The room is still 9 × 4.5 × 3 m (`RoomHalfLengthM`, `RoomHalfWidthM`, `RoomHeightM` in `DeepTuningTypes.h`), wider than BT-01's whole 3.3 m hull, so it narrows by more than a metre, and the console, the helm, the hatch and room for two players (D-057) must still fit. Proposed: size and layout now, its finish in the look pass (D-028). It is spawned and dressed in code (`ADeepSubRoom` at `RoomSetLocationM`, `DeepRoomDress.cpp`), so no map is touched. The cook carries BT-01's cabin materials (`MI_BT01_Interior_cabin`, `_computer`, `_soft_furniture`, `MI_BT01_Bed_details`): check whether the mesh holds a cabin to start from | agent, opus, `.wt\subs` on `build/sub-inside` (new, from main `fd465e4`) | **merged** `c695b5e` (11:41 AM Nov 17). Built `a90ef50` (pushed). BT-01's own cabin is not walkable (4.4 x 1.8 m, aisle 60-80 cm, a curved floor), so the room stays code-built at 4.9 x 1.8 x 2.1 m (`Inside*` tuning, new `World/DeepRoomLayout`): helm at the bow, console on the starboard wall, round hatch aft; 164 cm free by the hatch for two to pass (152 needed, D-057). E acts on the nearest station in reach. The agent's tests are green (SubRoomLayout, RoomDress, Still, MovingSub, World.Sub, World.Pilot) and S1-hatch and S12-sidehome pass. Main merged in, `ef1510b` (clean, pushed). Frames on page v33 (5:14 AM) with the ask: OK to merge once three fixes are in. Sent back 5:12 AM for them and a readable plan frame. Fixed, `60ebfe6`: the patches were M_WornSteel's bare-steel chips (about 55 cm) on the bezel; the capsule spawns at `Sub.InsideStandHeightM`; the dress ISMs block Pawn only (new test `Deep.World.SubRoomStand`). The same chips tore the deck and a locker door, so `f9b40cb` caps all room wear at the new `Look.RoomMaxWear` (0.15); the lever bases keep 0.45. 38/38 on the agent's filters. Frames on page v35 (6:20 AM). Merge gate passed at 6:22 AM: verify (181 tests), S1-hatch, S12-sidehome, package, smoke, a packaged frame in the room that matches the editor, zip 480.5 MB (the same as BT-01's gate). The packaged log's one hit is M_CaveRock falling back to the default material: its textures come from the Dragon Cave pack, which `.wt\subs` lacks (BT-01's gate logged the same). Quinn's OK about 6:26 AM (page v36), then the merge |
| Caves into the dive: place the chunk string in the still world at runtime, replace DeepShaft and DeepCavern behind a switch, start runs in S, and fly the sub along the path with steering and hull hits. BT-01's centre sits about 0.94 m aft of the hull's origin (the hatch), so the cave tool's hull box shifts aft by that much | agent, opus, `.wt\cave` on `build/cavedive` | M1 merged about 1:10 AM Nov 18 (`5659e71`): the run starts parked in S and the helm drives the string S to E with the hull's real box. M2a merged about 2:10 AM Nov 18 (`7ac9d5b`; gate green on `e2c9997`: 198 tests, both playthroughs, package, smoke): depth follows the stops (D-065), the limit holds the sub at 200 m in C1's sink, a gate sinks it to C1's berth at 300 m, salvage lies at the sockets, and the hatch and tether work at both berths. Play build: `Builds\CaveDive-M2a\Play-Caves.bat`. M2b built by 3:35 AM Nov 18, and its merge gate passed at 4:27 AM on `291d1ed` (207 tests, both playthroughs with the caves off, the cave run S13-caves, package, smoke): salvage in piles, 10 a stop (D-067); the serpent at each cavern's socket, clear of rock; the relic in C1 (D-068); the HUD names only the cave string's stops; a cave run to the win. **Merged** about 1:45 AM Nov 25 (`a64d217`, pushed; the code is the gated `291d1ed`; Quinn: "fine with merging"). Play build: `Builds\CaveDive-M2b\Play-Caves.bat`. Stop 3's cavern, C2 (D-069): its shape is built on `build/cavestop3` (`4888633`, `.wt\cave3`) and waits for Quinn's OK (page v43); its runtime part follows. The caves stay off by default until then (D-068). Plan in [[cave-dive]] |
| The orange lattice at the helm: volumetric fog where it meets cave rock, in `jam-final` too. Find the cause and fix it | agent | step 4 is merged; waits for Quinn's OK |
| Drive like a game sub, and see where you go (D-070; Quinn, Nov 25: "super hard to see and control ... you barely can see anything"). Found: the helm camera looks 32° down and its floods 30° and 60° down (made for the jam's well), so tunnels show floor; exposure is locked | agent, opus, `.wt\cave` on `build/drive` | **doing** since about 2:10 AM Nov 25: D1 (direct drive, chase and cockpit views, sliding on rock, level floods), plan in [[drive]], research in [[Sub Driving and Seeing - Approaches]]. Quinn said go at about 2:05 AM; the floods stay strong at depth ([[Style Guide]]) |
| A rock material on the cave walls (D-070). Every chunk wears `M_CaveGrey`, a flat grey (`cavegen.py`); the jam's `M_CaveRock` (triplanar, Dragon Cave textures) was only on the old shaft. It runs on C2's branch, so C2 gets the rock too | agent, opus, `.wt\cave3` on `build/cavestop3` | **doing** since about 2:10 AM Nov 25: R1 (option B with Dragon Cave rock: triplanar, silt, cavity baked per vertex, every chunk re-baked), plan in [[cave-rock]]. Merges after Quinn's OK on C2's shape and on its frames |
| Repo size: `devlog/media` holds 483 MB of PNG frames in plain git (560 files at noon, Nov 17), outside LFS. Proposed: new frames as JPEG, a small fraction of the size, and the harness's own frames kept out of git | orchestrator | decided: JPEG only, and the harness's frames stay out of git (D-066, `673746d`) |
| Subs: use both (Quinn, 10:04 PM). Proposed: BT-01 is ours (×2, 12.3 m) and the Atlantic Explorer is the wreck. Both CC BY 4.0, unpacked in `asset-drop/subs/` and measured | orchestrator | Atlantic Explorer = wreckage (D-056); BT-01 is ours (D-062), lane started; open: wreck salvage and sub upgrades loop |
| Chunk assembly research: how Lethal Company-style interiors are built; UE 5.9 options | agent, sonnet | done ([[Cave Gen - Chunk Assembly]]) |
| Slice brief: pillars, keep or cut, the opening beat by beat | Quinn + orchestrator | draft ready; to do: rewrite it for tunnels with caverns (D-058) once C1 and T1 exist |
| Block out the slice map with CubeGrid, from [[Blockout Scale Card]]; settle the hull's size first ([[Cave Pipeline]]) | Quinn | todo |
| Sounds for the slice | Colby | later (Quinn, 10:04 PM: not yet) |
| Co-op: one at the helm, one on the tether (D-057); Steam sessions and proximity voice | agent, sonnet (research) | research done ([[Co-op - Steam and Voice]]); build after the cave |
| Immersion pass: camera shake that reacts to the world (knocks, current, the engine, rock close by) and grain underwater. Quinn, Nov 16: after the cave. Part of the one planned look pass (D-028); look each effect up first | later | after the cave |

On hold: `build/sight` (`.wt\voice`) and `build/helm` (`.wt\text`) wait for the still world and the sub kit. The runtime cave generator stays only as the stand-in until the map exists.

## Jam-era handoff (history) (rewritten 7:00 AM Nov 15, updated 4:20 PM)

A new session needs only `CLAUDE.md`, this note and `.claude/loop.md`. The history of the night is in [[Devlog]] and [[Agent Runs]]. This section holds only what is true now.

**Where things are**

- `main`: code as of `0874fe5` (52 tests), plus notes. Quinn has not merged the polish pass yet.
- `polish/integration` on origin at `cee150a` (3:40 PM: the harpoon's kick, D-047, verify 128 of 128, S5 and S10-relic passed, package and smoke green, zip 762.7 MB; under it `1bc6cd0` at 3:19 PM: `main` merged in, notes and text only. `main` has had vault commits since, so at the freeze merge `main` into integration again, text only, before the fast-forward; under it `964c66e` at 3:17 PM: the serpent's physics body, verify 126 of 126, S3 and S11 passed; under it `f0b7fd0` at 2:55 PM: HUD A, verify 121 of 121; under it `1bd4031` at 2:53 PM, the honest S11, and `bbeeb7e`, the diver's ping; then `59af289` at 1:41 PM: the cave shaft and the caverns; verify 107 of 107, a package with 0 usage-flag lines, smoke, a 762.6 MB zip, and S2, S7, S8, S9, S10, S11 and S12 passed; before it `6aeafc8` at 12:56 PM: the Captain lane, 94 tests; before it `ea29122` at 12:33 PM, the sonar; and `a2e696d` at 12:06 PM: the first-five-minutes pass and the level view aboard merged; verify 79 of 79, package and smoke green, zip 762 MB, 0 usage-flag lines; all 14 scripted playthroughs passed on `ad644df` in 745 s, and S1, S4, S5, S6, S7 again on `a2e696d`): everything built since. Water, the room, hull and salvage, the driving view, the serpent with the dragon's head (two passes: the join, the bite staged on the line, the shoulder thinned), the HUD, settings, credits and a title menu, the fast playtest script (S0 to S12), the ending fix, two passes of rock and ruin (the placement loader, the cook hook, floor rock at every stop, the 500 m well with the relic as a carved stone sphere, real rock on the trench wall, a carved gate at each stop), the tether and the harpoon line as ropes that sag, go taut and visibly part when cut, a hull for the submarine built from basic shapes, a texture limit for the build, and dev log posts two and three (`devlog/02-sunday-before-dawn.md`, `devlog/03-sunday-morning.md`). It also has the lamp that samples its whole cone, a flat floor in the relic's pit, rebuilt silt, and the serpent kept inside the well on the last ascent. Full suite 78 of 78; packaged and smoke-tested at `b7755a1` (11:18 AM), zip 762 MB, no usage-flag lines. All 14 scripted playthroughs passed at `a942e1c`; the ending set has not run on `b7755a1`, because the machine went down as it started. It is checked out in `.wt\monster`, the orchestrator's merge checkout, and the package is in `.wt\monster\Builds`.
- **Pushed at 2:53 PM:** `1bd4031` merges the S11 fix (`7eb79d8`, script only). S11 now passes only when the serpent hears a diver who really sprints, not when the Captain's patience cuts the line: on integration the serpent went from Lurk to Stalk in 55 s, noise 60.2, heard +588, PASS in 104 s. Under it are `bbeeb7e` (the diver's ping) and `340332f` (the serpent clamp `ef84d37` and the camera fix `c19f142`): the 1:54 PM gate on `340332f` passed S2, S3, S7, S9, S10 and S12, and the 2:33 PM gate on `bbeeb7e` passed the build, 117 tests, a 1047.5 MB package, smoke, a 762.7 MB zip, S8 and S9. **Pushed at 2:55 PM:** `f0b7fd0` merges HUD A (`e11eca0`); verify passed, 121 of 121. It loads the same engine fonts as before, so the next package is the 5:30 PM freeze run-through.
- **Toward the end (D-045, Quinn, 2:12 PM):** review each diff yourself, then `verify`, then only that area's playthroughs. The full run-through (every playthrough, package, smoke, zip, a hand play) waits for the 6:45 PM freeze (D-048). The lower roof (`0e14e21`) is **not** merged: it made no visible difference (D-043).
- The packaged game matches the editor. The orchestrator took frames of the dragon's head, a pillar at the well and the bottom of the well from the package, and its log has no `missing usage flag` line.
- **Nothing visual merges into `main` until Quinn has seen frames and said "merge".** The frames page URL is in Claude's memory, as is the URL of his board of 61 asset picks. When he says it: merge `polish/integration` into `main` with the gate in `.claude/loop.md`, push, and tell him to restart his editor.

**Now (4:20 PM): the re-scope (D-048)**

- **9:35 PM: sonar off and no tag are on integration `d2a53bf`, gated and pushed** (build in `Builds\NoTag-2127`). Main waits for Quinn's "merge". The jam is over; next is a polished game with a real map, more human in the loop. Lanes `build/sight`, `build/cave` and `build/helm` are on hold until Quinn sets the map direction.
- **6:40 PM: post 2 of the public dev log is committed** (`devlog/02-sunday.md`: Sunday from 1 AM to the freeze, with four frames of the re-scoped game), checked against the log. Left for Quinn: Colby's part (a TODO in the post) and where the posts go; itch.io devlog posts on the game page are what other entrants use.
- **6:30 PM: two small changes are reviewed and ready on `rescope/notag`**, not merged: the reticle without the serpent's name tag (`6946bcc`, waits on Quinn's "no tag") and the corrected credits (`dc19f58`: the Dragon Cave pack supplies the cave rocks, stones and relic; the serpent's head is built in code). The credits fix goes in with the next package. No Unreal process runs until Quinn's calls come in or the video is recorded, because a cook would make the capture stutter. Post 2 of the public dev log is being drafted (`devlog/02-sunday.md`). The [[Submission Checklist]] is up to date.
- **6:20 PM: the freeze run-through passed** on `9ac970e`: verify (164 of 164), all 14 playthroughs, package (1,048 MB), smoke, zip (762.9 MB). Copied to `Builds\Freeze-1820`; this is the entry unless Quinn's calls change it. Next: Quinn's calls ("no tag" is prepared on `rescope/notag`, not merged), the team name, the video on this build, the itch page and the form.
- **6:05 PM: the freeze run-through started early** on integration `9ac970e` (main's notes merged in, no code): verify, all 14 playthroughs, package, smoke and zip. Every lane had merged by 5:43 PM, so there was nothing to wait for. If Quinn asks for a change, only what it touches runs again. The itch page and [[Video Shot List]] now describe the re-scoped game (row 100), checked against the code.
- **5:55 PM: all seven lanes are in integration `7698723`, pushed.** Verify passed with 164 of 164 tests; on the merged build S7-run (48 s), S3-serpent (12 s), S11-pass (104 s), S10-relic (69 s), S2-gate (40 s) and S9-floor (149 s) passed. Driving the merged rock, every GPU reading is under 16.7 ms; the closest is the sub's room at stop 2 (16.4 ms, mostly its own lights). Next: a hand-play package for Quinn, `Builds\HandPlay-1800`, then the freeze at 6:45 PM.
- **Quinn's verdict at about 3:50 PM:** the game is not good enough, and maybe not worth submitting. The orchestrator disagreed about submitting: integration `cee150a` is packaged and tested, so it is the floor, and the team submits at least that.
- **Five lanes since about 4:15 PM**, each an Opus engineer on its own branch from `cee150a`, each reporting by 5:30 PM and stopping at 5:45 PM with what is green (rows 92 to 96 in [[Agent Runs]]):
  - `rescope/helm` in `.wt\dev` (port 8021): throttle notches, rudder, ballast lever, all stop; the world turns about the sub (`SubYawDeg`); levers by the seat. At merge: verify, S2, S7, S8.
  - `rescope/cave` in `.wt\swim` (port 8022): the generated cave (noise and strata shared by the mesh and `Contact`), `M_CaveRock`, noisy caverns, rocks in the walls, `Deep.Cave.Rebuild`. At merge: verify, S2, S3, S7.
  - `rescope/hud` in `.wt\text` (port 8023): the HUD stripped back; the Captain off by default. At merge: verify, S0, S1, S11.
  - `rescope/tether` in `.wt\voice` (port 8024): the line is the air line; a bite cuts it; unclip by choice; patience no longer cuts. At merge: verify, S4, S10-cutdrop.
  - `rescope/interior` in `.wt\camera` (port 8025): about 100 pieces of dressing and worn materials in `Content/Deep/Room/`. At merge: verify, S1, S11.
- **The shared seams.** The helm lane owns the helm functions in `DeepRunComponent` and the placement block in `ADeepShaft::Follow`; the tether lane owns the patience and cut code; the cave lane owns the rest of `DeepShaft.cpp` and the insides of `UDeepCaveRules::Contact`, whose signature stays. The contract: the shaft follows the arena root, whose rotation is a pure yaw of minus `SubYawDeg`, identity at yaw 0. A branch that conflicts at merge goes back to its agent; the orchestrator writes no game code.
- **As each lands**, in integration in `.wt\monster`: review the diff, build, verify through the slot, the lane's playthroughs, package if C++ or materials changed, push. Frames to Quinn on the frames page.
- **Schedule (replaces D-038):** lanes merge 5:15 to 6:30 PM; Quinn hand-plays about 6:15 to 6:40 PM; freeze at 6:45 PM: verify, every playthrough, package, smoke, and a zip under 1 GB; video 7:10 to 7:40 PM; itch page and form 7:40 to 8:15 PM; submit about 8:30 PM.
- After the re-scope, the itch page (`devlog/itch-page.md`) and [[Video Shot List]] needed the new helm keys. **Done** at 6:05 PM (row 100), checked against the code on `9ac970e`.

**Earlier (1:50 PM): the scope changed (D-038 to D-042)**

- The 5:30 PM freeze (D-038) is replaced by D-048's 6:45 PM freeze; see Now (4:20 PM) above.
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
    - the lower roof (D-042), committed as `0e14e21`: no hull lost on the descent, but no visible difference from the hatch either, so not merged. D-042 is dropped for D-043, the diver's sonar ping.
  - `polish/sonar` in `.wt\text`: **done** at about 12:25 PM (`d8c2f53`). It merged into integration as `ea29122`, passed the gate and the S0, S2, S6 and S7 playthroughs, and was pushed at 12:33 PM. The hull bar it was to build next came with the shaft lane (`8ac8efc`).
  - Every `tools\ue.ps1` call by an engineer holds the `Global\DeepUnrealSlot` mutex. The orchestrator's playtest script also waits for any other `UnrealEditor.exe` to close. One Unreal process at a time, since the 11:19 crash had two editors up.
  - When they report, expect conflicts in `DeepArena.cpp` (shaft: `ApplySubDepth`; cavern: `EnsureArena`), `Sim/DeepTuningTypes.h` (three new tuning structs) and the CSVs (rows appended at the end). After each merge, run the scripted playthroughs that drive or swim (`S2-gate`, `S7-run`, `S9-floor`, `S10-relic`, `S11-pass`, `S12-sidehome`). Quinn sees frames before any part is called done.
- **Started after the scope change (D-040, D-041):**
  - `polish/captain` in `.wt\voice`: **done** at about 12:43 PM, all four steps (`0063051`). It covers the radio voice with squelch and ducking, the pacing, the danger tone, music slots by depth, breathing that follows the danger, and up to three variations per sound. Merged into integration as `6aeafc8` and pushed at 12:56 PM. The gate is green (94 tests, a package with no usage-flag lines, smoke, a 762.5 MB zip), and S0, S6, S7 and S10 passed. The names and paths in D-040 are fixed, because Quinn's brother records to them ([[Sound Brief]]). Waiting on files:
    - point the six serpent rows in `Sounds.csv` at the files, with pitch back to 1.0;
    - map the Fab pack's rows;
    - import `audio-drop/`, if Quinn allows it.
  - `polish/camera` in `.wt\camera`: the fix is **done** (`c19f142`), merged locally as `340332f`. The diver's sonar ping (D-043) is **done** too (`ccf1f22`, frames `42fbd47`), merged as `bbeeb7e`: V in the water, 3 s cooldown, about 100 m, the pulse on by default, its colour as tuning.
    - Shakes are on, now also on wall hits, scaled by impact speed.
    - The footage look, the sonar pulse and interference stay behind their switches until Quinn decides.
    - The ping skips a box round the sub, so it never lights the hull (0.03 ms on the GPU).
    - The recipe is in `03 - Research/Camera - Footage Look and Sonar Pulse Recipe.md` on that branch.
    - Plan: turn the pulse on by default unless Quinn says no.
  - Both were told at 12:35 PM to merge `origin/polish/integration` (`ea29122`, with sonar). At 12:57 PM the shaft, cavern and camera lanes were told to merge `6aeafc8` before they report.
  - The new lines are in [[Captain Script]]: 74 of them, with no numbers in the voice. The Captain lane copies the CSV block.
  - The HUD pass: one planned pass (D-028), in the look Quinn approves. **Quinn, 1:54 PM (D-044): the colours do not match the theme.** The pass starts with a new palette and shows it to him before building.
    - The mockup is done: [[HUD Mockup Spec]], with the pictures on a private page for Quinn.
    - It offers A, the instrument HUD, or B, the diver's camera display, which costs about an hour more to build.
    - Quinn had not picked by 2:24 PM, so an Opus engineer built A in `.wt\text` on `polish/hud-a` (row 86 in [[Agent Runs]]). Done at 2:50 PM in Tonal water (`e11eca0`), merged into integration as `f0b7fd0`, frames on the frames page, version 19. Say B to switch.
    - The palettes are in [[HUD Palettes]] and on the HUD mockup page (version 2, 2:38 PM): Bone and oxide, Tonal water (recommended) and Amber phosphor. HUD A is built in Tonal water, and another palette is a one-file swap in `UI/DeepHudStyle.h`. Warn and the machine screens are two roles now. The reticle, aim brackets and hatch marker have a dark outline, because with no panel behind them they drop to about 1 to 2 contrast over the lamp glow.
  - **The serpent (D-046).** Before 2:25 PM Quinn told the camera agent the serpent "doesn't animate at all and looks like two models smooshed together". That agent went back to one model in `.wt\swim` on `polish/serpent-look`: the code-built head with its jaw is the default again (`8b5b100`, frames `da36b1f`, pushed, not merged). At about 2:36 PM Quinn asked for a body that moves by physics, with the controller as the head. An Opus engineer has built it on the same branch since 2:39 PM (row 89): a Verlet chain the head drags, with a bend limit, water drag and the cave clearance, and no sine weave. Report by 4:00 PM, frames first. If the chain is not right by 3:45 PM, merge only `8b5b100`.
  - The brother's files land in `C:\Repositories\Deep-MegaJam\audio-drop\`, which git ignores. When the Fab pack lands, add its folder to `.gitignore` and link it into `.wt\monster`, `.wt\voice` and `.wt\camera`, the way DragonCave is linked.
- `polish/aboard` (`2f27da3`, pushed): coming aboard levels the view (`Arena.AboardPitchDeg`, test `Deep.World.SubAboardPitch`, `Deep.World.Sub` 9 of 9). Merged at 12:06 PM (`a2e696d`).
- Bubbles are parked. The fourth try, half-built at the crash, is committed unfinished on `polish/fx-wip4` (`f7a3fce`). `polish/fx` has all effects off by default.
- The crash (11:19:45 AM, Kernel-Power 41, no bugcheck): the orchestrator and all three engineers died. Every lane that had committed work survived, and `polish/wellserpent` was merged and pushed (`b7755a1`).
- The frames page is current: version 17 at 1:56 PM. It has the opening, the well, the camera with its fix, the shaft and the caverns, the roof result, Quinn's HUD colour note and the 2:15 asks. The generator is in the old session's scratch folder (`C:\Users\quinn\AppData\Local\Temp\claude\4cf960466880eab9\f3fa3667-c758-4612-8d67-d2bda7e089c8\scratchpad\frames_gen\`; Python is `py`).
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
- By hand: a key press on the title of a packaged build, listening to the sound, volume and mouse feel, and whether settings survive a restart. The newest packaged build is integration `cee150a` (3:40 PM: the cave shaft, the sonar, HUD A, the serpent's physics body and the harpoon's kick). Quinn's copy for hand-play is `Builds\HandPlay-1540\Deep.exe` in the main checkout, so that his play cannot lock the files the freeze package overwrites in `.wt\monster\Builds\Windows`.
- Team name, student status, Fun Ideas picks, and where the public dev log is hosted.

**Standing reminders**

- Polish rules: `03 - Research/Polish - Start Here.md` and `02 - Design/Style Guide.md`. Every polish brief links to both. No half-done polish (D-028), one style (D-030).
- Every Unreal launch goes through the machine-wide slot. The snippet is in `.claude/loop.md`, and every brief must carry it, because the role files do not.
- Scripted playtests and packages run only at checkpoints (D-031).
- Sunday (D-048, replacing D-038's 5:30 PM freeze): lanes merge 5:15 to 6:30 PM, Quinn hand-plays about 6:15 PM, freeze run-through and package 6:45 PM, video 7:10 to 7:40 PM, itch page and form 7:40 to 8:15 PM, submitted about 8:30 PM. The deadline is 8:59 PM.
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
- **Last package:** 1047.6 MB on disk, 762.7 MB zipped, built from integration `cee150a` at 3:40 PM. Package 1 min 31 s, smoke run clean (25 s), zip 22 s. The limit is 1 GB zipped; the harness warns because the unzipped folder is over 1 GB, and the Official Rules PDF, still unread, is where to confirm which size counts.
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
| Caverns | Opus, high, `.wt\dev` on `polish/cavern` | Walls and roof gated at 1:41 PM; the serpent clamp is `ef84d37`. The lower roof (`0e14e21`, `ff0b0b5`) made no visible difference and is not merged. **Finished** at about 1:53 PM. | 12:02 PM |
| Sonar | Opus, high, `.wt\text` on `polish/sonar` | **Done**, `d8c2f53`. Merged as `ea29122` and pushed at 12:33 PM. | 12:02 PM |
| Captain and sound | Opus, high, `.wt\voice` on `polish/captain` | **Done** at about 12:43 PM (`0063051`): radio voice, pacing, danger tone, music slots, breathing by danger, variations. Merged as `6aeafc8`, gate green, pushed at 12:56 PM. Waits for sound files. | 12:17 PM |
| Captain lines | Sonnet, medium (writer) | **Done**: 74 lines, no numbers in the voice. [[Captain Script]] | 12:17 PM |
| Camera | Opus, high, `.wt\camera` on `polish/camera` | **Done** (`c19f142`). First frames came at 1:13 PM; then the fix: the ping skips the sub, and wall hits shake by speed. Merged locally as `340332f`. The diver's sonar ping (D-043) is **done**: `ccf1f22`, `42fbd47`, merged as `bbeeb7e` at 2:20 PM. | 12:25 PM |
| S11 fix | Opus, high, `.wt\dev` on `fix/s11-noise` | **Done** at about 2:48 PM (`7eb79d8`, script only): the old script never reached sprint speed, and its catch-up teleport pinned the diver, silent, at the end of the line. S11 now passes only when the serpent hears a real sprint. Merged as `1bd4031`; S11 passed on integration; pushed at 2:53 PM. | 2:15 PM |
| HUD mockup | Sonnet, high, scratchpad only | **Done** at 1:11 PM: [[HUD Mockup Spec]] and a private page with both directions. No pick by 2:24 PM, so A is being built. | 12:49 PM |
| HUD A | Opus, high, `.wt\text` on `polish/hud-a` | **Done** at 2:50 PM (`e11eca0`): direction A in Tonal water, every colour by role in `UI/DeepHudStyle.h`, a hull bar with a damage ghost, SUB and LIMIT apart, outlined marks. Merged as `f0b7fd0`, verify 121 of 121, pushed at 2:55 PM. Frames page version 19. | 2:24 PM |
| HUD palettes | Sonnet, high, scratchpad only | **Done** at 2:31 PM: three palettes ([[HUD Palettes]]) on the HUD mockup page, version 2. Tonal water recommended. | 2:24 PM |
| Serpent look | Opus, high (the camera agent, resumed by Quinn), `.wt\swim` on `polish/serpent-look` | **Done** at about 2:35 PM: one model again, the code-built head and jaw (`8b5b100`), frames `da36b1f`. Pushed, not merged. | about 2:16 PM |
| Serpent physics | Opus, high, `.wt\swim` on `polish/serpent-look` | **Done** at about 3:10 PM (`f4ef2b0`, `35ddef2`): the body is a chain the head drags through the water (D-046), no stroke. Reviewed, merged as `964c66e`, verify 126 of 126, S3 and S11 passed, pushed at 3:17 PM. Frames page version 20: works in the game's dark; in plain light it reads as a hose. | 2:39 PM |
| Harpoon recoil | Opus, high, `.wt\dev` on `polish/harpoon-kick` | **Done** at about 3:33 PM (`e287d29`), six minutes after it started: a shot shoves the diver back 0.875 m (3.5 m/s fading to zero in half a second), rock stops the shove, and a reel ends it; a firmer shake on the shot and a new thud for a bolt into the serpent. Reviewed, merged as `cee150a`, verify 128 of 128, S5 (18 s) and S10-relic (76 s) passed, pushed at 3:40 PM, packaged (D-047). | 3:27 PM |
| Submission text | Sonnet, medium, main checkout, text only | **Done** at 3:05 PM (row 90): `devlog/itch-page.md` rewritten for today's game, [[Video Shot List]], the checklist's status. Reviewed against the code, three faults fixed, committed. | 3:03 PM |
| Helm piloting | Opus, high, `.wt\dev` on `rescope/helm` | **Done** at about 4:55 PM (`1dcc0c9`, `658e7b9`): six throttle notches on W/S, a rudder on A/D, a ballast lever on C and Space that stays where it is left, X all stop; the world turns about the sub. Its finding: an unsteered full-flood dive breaches the hull by 240 m, so the bends must be flown (D-049). Merged with integration as `ea92861` on `rescope/hud-helm` at 5:05 PM, and into integration with the seated readout as `7027c72` at 5:24 PM; its gate passed at 5:40 PM (verify 156 of 156, S1, S2, S8, S7, S12). | 4:15 PM |
| Cave generation | Opus, high, `.wt\swim` on `rescope/cave` | **Done**, merged as `fe11bae` at 5:33 PM (`89c9b5e`, `1adde91`): one seeded noise rule shapes the rock you see and the rock you hit, live in Project Settings with `Deep.Cave.Rebuild`; `M_CaveRock`; 96 pack rocks in the walls; the bends drift at most 0.5 m per metre of depth where it is tight (D-049). S7-run passed on the merged build in 48 s. | 4:15 PM |
| HUD declutter | Opus, high, `.wt\text` on `rescope/hud` | **Done**, merged as `03b9c72` at 4:48 PM: text only while it is needed, one prompt at a time, critical cues only, the hull only on a change or below 30%, the Captain off by default. The follow-up (D-049) is done in `047b953`: a seated readout of the throttle notch, the ballast and the heading, a hint that says the ballast is a lever, and `Deep.Hud.HelmSonar 0` to hide the helm's sonar (on by default); merged with the helm as `7027c72` at 5:24 PM. | 4:15 PM |
| Tether air line | Opus, high, `.wt\voice` on `rescope/tether` | **Done**, merged as `63830a0` at 4:31 PM, with two fixes (`0ec752a`: S11 counts a bite through the line; `2dd2cd2`: the audio coverage test knows the line's two sounds). Clipped means unlimited air; the serpent bites the line through; T unclips; Patience no longer drains. | 4:15 PM |
| Interior | Opus, high, `.wt\camera` on `rescope/interior` | **Done**, merged as `5e530da` at 4:51 PM: 149 pieces of dressing as 15 instanced meshes with no collision, worn steel, brass, rubber and glass, five fittings for light; `Deep.Room.Dress 0` hides it. | 4:15 PM |
| Lamp and fog light | Opus, high, `.wt\voice` on `rescope/light` | **Done**, merged as `7698723` at 5:43 PM (`c4e9f89`, `674bb69`): the lamp's cone 18/38 degrees (was 7/21), forward scatter and a lighter fog history so the beam no longer trails, rock and floods that cast shadows; GPU 12.6 ms against 12.2, parked. Re-measured at 5:52 PM on the merged build while driving: every reading under 16.7 ms (the helm 4.2 to 10.0 ms, diving 9.5 to 11.9, the room at stop 2 16.4, mostly its own lights); nothing changed. | 5:10 PM |
| Serpent taunts and hearing | Opus, high, `.wt\dev` on `rescope/monster` | **Done**, merged as `852d76c` at 5:41 PM (`b01cbf8`, `7917e9f`): twice before it bites it shows itself at the lamp's edge and withdraws, calling, and a taunt never bites; it hears the ping, the harpoon, knocks and a hard-driven sub from 260 m and answers; a drone, clicks and calls in 3D. Quinn listens in the hand-play. | 5:10 PM |
| Free sub models | Sonnet, medium, search only | **Done** at about 4:50 PM (row 97): [[Polish - Sub Assets]]. The Atlantic Explorer as a base for the hull, the beached-submarine scan as a wreck; both CC BY 4.0 with a credit line. Waiting on Quinn's download into `asset-drop\`. | 4:48 PM |

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
| 8 | Sunday (D-048, replacing D-038): lanes merge 5:15 to 6:30 PM, freeze run-through and package 6:45 PM, video and itch page 7:10 to 8:15 PM, submitted about 8:30 PM. **Run the scripted playtests before each package** (D-031); they are not run in between. The itch.io page says in one sentence how the game answers the theme (D-025). | all | todo |

## Waiting on Quinn

**Now (4:20 PM):**

- **The re-scope's defaults (D-048); one word from him changes any of them:** the Captain is off by default (`Deep.Captain.Radio 1` brings him back); no PCG plugin, but a C++ generator tuned live in Project Settings > DEEP Tuning > Cave, with `Deep.Cave.Rebuild`; the tether is the air line, the serpent can bite it, and patience no longer cuts it.
- **Submit regardless.** At 3:50 PM he said the game might not be worth submitting. The orchestrator disagreed: `cee150a` is packaged and tested, so it is the floor, and not submitting scores zero for certain.
- The Fab sound pack: add it, and tell the orchestrator its folder name. What to look for is in [[Sound Brief]].
- His brother's music and serpent sounds go into `audio-drop\`: the first files by 2 PM, the rest by 3:30 PM ([[Sound Brief]]).
- Two questions for Quinn:
  - May the Captain-and-sound agent import those files headless into `deep/Content/DeepAudio/`? That would be a new exception to the binary-asset rule. If not, Quinn imports them himself, telling the orchestrator first, because an open editor blocks the builds.
  - Who voices the Captain: his brother, OpenAI text to speech (with Quinn's key, kept out of the repo), or text only?
- **Calls open since 2:15 PM:**
  - the footage look, yes or no (frames page);
  - the HUD: A is built in Tonal water and is in integration (frames page, version 19). Keep it, say B (about an hour more), or name another palette from [[HUD Palettes]] (a one-file change). Amber phosphor breaks the Style Guide's rule that amber never goes on HUD lines;
  - the serpent's body (asked at 3:20 PM, frames page version 20): it moves by physics and is in integration. Keep it (the default), say "fallback" for `8b5b100` (the sine swim he called unrealistic), or "tune" for one pass before the freeze: a body that tapers and a neck that bends more softly;
  - the harpoon's kick (his ask at about 3:22 PM, D-047): in the integration build since 3:40 PM (verify 128 of 128, S5 and S10-relic passed). Feel it in `Builds\HandPlay-1540\Deep.exe`; one word from him takes it out;
  - the serpent's hearing (asked at 3:00 PM): sprinting alone never brings it. Sprint noise adds 15 a second against a decay of 10, so reaching Stalk takes about 12 s of unbroken sprinting, and a straight sprint reaches the end of a 60 m line in about 9.4 s. Winching and grabs do bring it. (a) keep it (recommended, and the default), (b) raise sprint noise to about 25 a second or lower Stalk to about 40, (c) count noise at any speed above a swim.
  Asked at about 2 PM: does his "not in the hud" also mean no recording dot or timecode on the HUD?
- **The video** (required, 30 to 60 s, a link on the form): Quinn records it by hand with OBS on the 6:45 PM freeze package, at about 7:10 PM (D-048); the shot list and the itch page get the new helm keys once the helm lane lands. The shot list is in [[Video Shot List]]: it opens at the helm, and the lamp shots need the flashlight bought first (one dive, or the console cheat `Deep.BuyGate free`). The itch page draft has his TODOs in it (`devlog/itch-page.md`).
- **The Official Rules PDF has never been read.** It is on Box and needs a browser. Five minutes before the freeze, or tell the orchestrator it may read it in Chrome.
- **"Merge" into `main`.** Nothing visual reaches `main` until he has seen frames and says "merge". Ask before the 6:45 PM freeze (D-048), so the freeze run-through and its package are built from `main`.
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
