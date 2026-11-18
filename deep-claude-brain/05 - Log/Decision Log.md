---
updated: 2026-11-18 01:17 EST
---
# Decision Log

One entry per decision: what, who, why, and what follows. A changed decision gets a new entry and the old one is marked superseded, not deleted. Entries are grouped by what needs attention first: Open, then Proposed (waiting for Quinn and Colby), then Decided.

## At a glance

| ID | Decision | Status |
|---|---|---|
| D-001 | The vault is Claude's memory; the Google Doc is the design doc and to-dos | Decided |
| D-002 | Quinn creates the project; no game code until told | Lifted about 9:12 PM Nov 14: Quinn said go |
| D-003 | No secrets, tokens or perk codes in the repo or vault | Decided |
| D-004 | Use subagents for parallel work | Decided |
| D-005 | The project lives at `deep/` inside the git repo | Decided |
| D-006 | Engine version: 5.9 Preview or 5.8 | **Open** |
| D-007 | Solo-playable jam build; co-op after the jam | Proposed |
| D-008 | Theme reading: the Director, the cut, and Patience as the main clock | Proposed |
| D-009 | The Director speaks in scripted lines with pre-generated voice | Proposed |
| D-010 | Code shape: pure tested rules, thin actors, tuning in config | Proposed |
| D-011 | Tracks to target | Proposed |
| D-012 | Tiered validation T0 to T5, including AI playtesting | Proposed |
| D-013 | Gameplay holes fixed and scope narrowed to twelve must-ship items | In effect since Quinn's go |
| D-014 | Design for fun and for winning: five principles, the first 90 seconds, a self-building arena | In effect since Quinn's go |
| D-015 | Agents: Claude orchestrates; models and effort tiered by task; commit and push allowed | Decided |
| D-016 | The submarine AI is the Captain, a sarcastic asshole; the Director is its unseen boss | Decided (name and voice by Quinn; the Director as boss is Claude's proposal) |
| D-017 | Build our own MCP tooling: a command-line client, a `DeepToolset`, and scripted playtests | Decided (Quinn asked; the shape is Claude's) |
| D-018 | A live AI brain for the Captain: later, as an add-on over the scripted lines | Deferred to about 1 PM Sunday (Quinn agrees it comes later) |
| D-019 | The monster is a sea serpent with a procedural body and a five-state brain | Decided (snake by Quinn; the behaviour is Claude's first version, all tunable) |
| D-020 | Raw gameplay first: no environment or atmosphere work until the loop plays | Decided (Quinn, 10:35 PM Nov 14) |
| D-021 | The harpoon is in | Decided (Quinn, 11:10 PM Nov 14) |
| D-022 | The player goes inside the submarine and drives it | Decided (Quinn, 11:10 and 11:20 PM): the sub is static and the world moves. That half is superseded by D-051 (Quinn, 9:39 PM Nov 15) |
| D-023 | Three layers, each with one gate: flashlight, then hull upgrade; serpent at layer 2, aggressive at layer 3 | Decided (Quinn, 11:25 PM Nov 14) |
| D-024 | Water and rope feel matter for immersion, and come after the gameplay | Decided (Quinn, 11:25 PM Nov 14) |
| D-025 | No "Director" or "Director's cut" in the game's text | Decided (Quinn, 11:30 PM Nov 14); supersedes the Director half of D-016 |
| D-026 | First sounds are generated in code; real files replace them one CSV row at a time | Decided by Claude (12:11 AM Nov 15); Quinn can overrule |
| D-027 | Test adaptively: an agent runs its own area's tests, and the full suite runs once, at merge | Decided (Quinn, about 12:50 AM Nov 15) |
| D-028 | No half-done polish: the gameplay loop first, then polish in one pass to a professional standard | Decided (Quinn, about 1:10 AM Nov 15) |
| D-029 | Looks are as important as gameplay: research and plan the polish pass now | Decided (Quinn, about 1:25 AM Nov 15) |
| D-030 | One style for everything the player sees and hears | Decided (Quinn, about 1:45 AM Nov 15); the direction itself is still open |
| D-031 | Scripted playtests wait until a checkpoint; none run in between | Decided (Quinn, about 2:30 AM Nov 15) |
| D-032 | The style: dark, worn, realistic, led by light and reflection, built on Megascans | Quinn leaning (about 2:40 AM Nov 15); he confirms by approving frames |
| D-033 | Go for the polish pass: water and the monster first; agents may create materials and particle systems; look it up before building | Decided (Quinn, about 2:47 AM Nov 15) |
| D-034 | The monster is built on the dragon from Quinn's Dragon Cave pack; any Megascans asset may be used | Decided (Quinn, about 2:56 AM Nov 15) |
| D-035 | The last ascent: the winch is slower with the relic, and the relic's shaft is straight under the hatch | In the integration branch (engineer, 6:45 AM Nov 15); Quinn can overrule |
| D-036 | Packs stay where they are installed, agents link to them, and the cook takes only what is used | Decided (orchestrator, 6:55 AM Nov 15) |
| D-037 | The relic becomes a real object, and the well holds its light | In the integration branch (8:56 AM Nov 15); Quinn can overrule |
| D-038 | Feature freeze moves from 3 PM to 5:30 PM | Quinn, 11:58 AM Nov 15; the 5:30 PM freeze is replaced by D-048 (6:45 PM) |
| D-039 | Drive the sub down a winding cave shaft with a sonar; every stop is a cavern | Quinn, 11:55 AM Nov 15; the shaft and caverns gated and pushed at 1:41 PM |
| D-040 | The Captain becomes a voice on the radio who says less; real sound replaces the synth | Quinn, about 12:15 PM Nov 15; built, waiting on sound files |
| D-041 | Camera feel: shakes, a found-footage look, and the sonar made visible | Shakes decided; the rest behind switches until Quinn approves frames; the pulse's look changes in D-060 |
| D-042 | The cavern reads from the hatch by a lower roof, not nearer walls | Orchestrator, 1:38 PM Nov 15; built, made no visible difference, dropped at 1:54 PM for D-043 |
| D-043 | The diver sends the sonar ping, so the cavern walls show from the water | Orchestrator, 1:54 PM Nov 15; replaces D-042; Quinn can overrule |
| D-044 | The HUD palette changes: Quinn says the colours do not match the theme | Quinn, 1:54 PM Nov 15; for the HUD pass |
| D-045 | Toward the end: review each diff, test only what it touches; the full run-through at the freeze | Quinn, about 2:12 PM Nov 15; in `.claude/loop.md` |
| D-046 | The serpent is one model again, and its body moves by physics, dragged by the head | Quinn, about 2:36 PM Nov 15; being built on `polish/serpent-look` |
| D-047 | The harpoon kicks the diver back when it fires | Quinn's ask, about 3:22 PM Nov 15; in the integration build since 3:40 PM (`cee150a`); one word from him takes it out |
| D-048 | The re-scope: the sub is piloted on every axis, the cave is generated, the screen goes quiet, the line is the air, the room is dressed; the freeze moves to 6:45 PM | Quinn's ask, about 3:50 PM Nov 15; five lanes building since about 4:15 PM; replaces D-038's 5:30 PM freeze |
| D-049 | Quinn's second round: a lamp you can see by and better light in the fog, a serpent that taunts, is heard and hears more, a readable helm, bends you can fly; the helm keeps its sonar for now | Quinn's asks, about 5:00 PM Nov 15; two lanes building since about 5:10 PM |
| D-050 | The jam is over; DEEP becomes a polished game made the traditional way: humans own the maps, agents write code and tools | Decided (Quinn, 9:11 to 9:39 PM Nov 15) |
| D-051 | The sub moves through a still world; the room is a fixed set; leaving is an airlock cycle | Decided (Quinn, 9:39 PM Nov 15); building on `build/moving-sub`; supersedes D-022's moving world |
| D-052 | PCG and Geometry Script are on; caves are made with the editor's tools | Decided (Quinn, 9:43 and 10:04 PM Nov 15); `6917b6d` |
| D-053 | The cave pipeline: a drawn skeleton, a Geometry Script shell, PCG dressing, a hand pass | Decided (Quinn, about 1:05 AM Nov 16), with one change: the output is a chunk library strung together per dive (D-055). The spike is approved: the scratch map `L_CaveSpike` and the beta PCG Geometry Script Interop plugin. |
| D-054 | The jam entry went in solo | Decided (Quinn, about 1 AM EST Nov 16) |
| D-055 | Caves are baked chunks, strung in a new order every dive | Decided (Quinn, about 1:05 AM Nov 16) |
| D-056 | The Atlantic Explorer is wreckage | Decided (Quinn, about 1:10 AM Nov 16); the dive's goal and the upgrade loop are open |
| D-057 | Two players: one drives the sub, one dives on the tether | Decided (Quinn, Nov 16), after the cave work; plugins open |
| D-058 | The dive is tunnels that open into caverns | Decided (Quinn, Nov 16); replaces the vertical descent passage in [[Cave Gen - Chunk Assembly]] |
| D-059 | Joints come in two or three types that must match | Decided (Quinn, about 11:45 PM Nov 16); changes D-055 and D-058 |
| D-060 | The helm ping stays automatic, but soft and low-key | Decided (Quinn, about 11:45 PM Nov 16); Quinn OK'd the look and the sound about 3:45 AM Nov 17; merged as the default 4:15 AM Nov 17 (`4367b3a`) |
| D-061 | The run starts in a calm start cavern | Decided (Quinn, about 11:45 PM Nov 16); chunk S in round 5 |
| D-062 | BT-01 is the player's sub | Decided (Quinn, about 11:45 PM Nov 16); merged 3:24 AM Nov 17 (`6ed6929`) |
| D-063 | The serpent turns to each soft helm ping, but pings alone do not make it hunt | Decided (Quinn, about 3:45 AM Nov 17); merged with the soft ping 4:15 AM Nov 17 (`4367b3a`) |
| D-064 | The caves lane brings the hull's real collision; the old shaft stays the default until M2 | Decided (Claude, inside Quinn's OK for the lane, about 12:02 PM Nov 17); M1 merged 1:10 AM Nov 18 (`5659e71`) |
| D-065 | Depth in the caves follows the stops: C1's sink reads 100 to 300 m | Decided (Quinn, about 1:09 AM Nov 18) |
| D-066 | Frames go into git as JPEG only; the test harness's frames stay out of git | Decided (Quinn, about 1:09 AM Nov 18) |

## Open

### D-006 Engine version: 5.9 Preview or 5.8
- **Status:** Open. Needed before any content is made.
- **Date:** 2026-11-14
- **Who:** Quinn and Colby decide. Claude recommends.
- **Decision:** Not made. Quinn is fine with 5.8 but wants 5.9's development features.
- **Why it is hard:** Assets saved in 5.9 cannot be opened in 5.8, so "develop in 5.9 and move to 5.8 later" does not work for maps, Blueprints or materials. 5.9 has the toolsets that let agents play and see the game. 5.8 has the DLSS plugin, which is the practical route to the NVIDIA RTX Award.
- **Claude recommends:** Stay on 5.9 with safeguards: gameplay in C++ and config, frequent commits and packaged builds, and an optional 30-minute test of the DLSS plugin against 5.9 before content exists.
- **What follows:** See [[Engine Choice - 5.9 vs 5.8]]. The project is empty today, so switching now costs nothing.

## Proposed

### D-053 The cave pipeline: draw the skeleton, generate the rock, dress it with PCG, finish by hand
- **Status:** Decided (Quinn, about 1:05 AM Nov 16), with one change: the output is a chunk library strung together per dive (D-055). The spike is approved: the scratch map `L_CaveSpike` and the beta PCG Geometry Script Interop plugin.
- **Date:** 2026-11-15, 10:55 PM EST
- **Who:** Quinn asked for the approach. The pipeline and the spike are Claude's.
- **Quinn's words:** 10:04 PM: "we really should think through a pcg followed with genuiine cave generation to think of the best approach here to give use truley realistic immersive results" Then 10:07 PM: "also note in the plan if theres behavior from games we wnat to replicate or learn form we can use reverse enginerer anythign too, but continue"
- **What it changes:** caves are made in the editor and saved, not generated while the game runs. A human draws splines and volumes. Geometry Script turns them into one rock shell, shaped by the rules real caves follow, and bakes it into static meshes in 40 to 60 m chunks. PCG dresses the rock, and a human finishes by hand. A 60 m spike proves it first. The plan is [[Cave Pipeline]].
- **Why:** the runtime noise caves look like noise, not rock cut by water, and can't put a squeeze before a reveal. A karst simulation can't keep a route drawn for pacing. Voxel Plugin 2 has no confirmed 5.9 support, and Houdini costs a seat each. PCG alone can't make the rock: its Spline To Mesh extrudes a flat curve.
- **Challenged:** reading "genuine cave generation" as simulating how caves form. A player can't tell a simulated route from a drawn one, but can tell the surface, so the realism goes there: tube shapes, scallops, breakdown, silt and light.
- **Games and reverse engineering:** behaviour is learned from play, footage, talks and published breakdowns, and measured from our own recordings ([[Games to Learn From]]). No decompiling, no datamining, and nothing pulled out of a game's files.
- **Not done, on purpose:** nothing bought; nothing generated at runtime; the beta PCG Geometry Script Interop plugin stays off until the spike.
- **Fallback:** a Voxel Plugin 2 trial or Houdini if the spike fails. Until the new caves exist, the runtime generator stays as the stand-in.

### D-049 Quinn's second round: a lamp you can see by, a serpent you can hear that taunts and listens, a helm you can read, bends you can fly
- **Status:** Quinn's asks, about 5:00 PM Nov 15. Two lanes started at about 5:10 PM from `ea92861`: the light on `rescope/light` and the serpent on `rescope/monster` (rows 98 and 99 in [[Agent Runs]]). The HUD lane resumed for the helm readout, and the cave lane got the limit on the bends. Each merges after a diff review, verify and its own playthroughs (D-045). The freeze stays at 6:45 PM. **All four parts merged by 5:43 PM:** the readout in `7027c72`, the bends in `fe11bae`, the serpent in `852d76c`, the light in `7698723`; verify passed with 164 of 164 tests, and S7-run (48 s), S3-serpent (12 s), S11-pass (104 s), S10-relic (69 s), S2-gate (40 s) and S9-floor (149 s) passed. Driving the merged rock, every GPU reading is under 16.7 ms; the closest is the sub's room at stop 2 (16.4 ms, mostly its own lights).
- **Date:** 2026-11-15, about 5:00 PM EST
- **Who:** Quinn asked. The lanes, the bend limit and the radar default are the orchestrator's; one word from him changes any of them.
- **His words:** "also the falshlight does not have a big enough cone you cant see anythign really, also i think we need a better way of controlling the sub also teh monster needs to be taunting the player in order to make iti good make sure you can here it and it can hear you better" Then: "also the landscape is sitill bad, im htinking the radar could be when your swimming instead of the sub also The light propagation withing the fog seems a littel weird i would like to make this better"
- **What it changes:**
  - **Lamp and fog:** a wider cone with a usable pool of light, falloff into real dark, forward scatter in the water without glare at the eye, and no light through rock.
  - **Serpent:** heard before it is seen, from where it really is, louder as it closes. It hears the ping, the harpoon, fast swimming, knocks, the winch and a sub at high throttle from further away, answers with a call and turns toward the noise. It shows itself and withdraws two or three times before it strikes, and a taunt never bites. Its seen-in-the-beam check gets its own angle, at today's value, so the wider lamp does not change when it counts as lit.
  - **Helm:** a seated readout of the throttle notch, the ballast and the heading, because the levers cannot be seen from the seat, and a hint that says the ballast is a lever.
  - **Cave:** where the passage is tight, its centre drifts at most about 0.5 m per metre of depth, with no steps. At full flood (2.6 m/s) a bend then sweeps sideways at about 1.3 m/s, which slow ahead (1.33 m/s) can follow.
  - **Radar:** the diver's ping (V, D-043) is already the swimmer's radar. The helm's sonar stays on by default; `Deep.Hud.HelmSonar 0` hides it.
- **Why:** Quinn could not see by the lamp, the light in the fog looked wrong, the serpent did not frighten because it never taunted and could not be heard, and the landscape was still bad. The helm lane showed that a full-flood dive with no steering breaches the hull by 240 m, so the bends must be flyable and the levers readable.
- **Challenged:** moving the radar off the sub now. An inertial sub flown through bends in the dark, with no sonar, may be a descent no one can finish. It waits until Quinn has seen the new light and cave; then it is one switch.
- **Not done, on purpose:** a helm that centres itself in the passage, and a bot that drives the levers in the scripted playthroughs, which keep the old direct path (no time); sound files (the serpent stays synthesised in code); a taunt that bites.
- **Fallback:** a lane that misses its gate is left out or switched off. The packaged floor is still `cee150a`.

### D-048 The re-scope: the sub is piloted, the cave is generated, the screen goes quiet, the line is the air
- **Status:** Quinn's ask, about 3:50 PM Nov 15. Five lanes started at about 4:15 PM, each an Opus engineer on its own branch from integration `cee150a` (rows 92 to 96 in [[Agent Runs]]). Each lane merges into integration only after a diff review, verify and its own playthroughs (D-045); nothing reaches `main` without his "merge". It replaces the 5:30 PM freeze of D-038.
- **Date:** 2026-11-15, about 3:50 PM EST
- **Who:** Quinn asked. The five lanes, their defaults and the schedule are the orchestrator's; one word from him changes any of them.
- **His words:** "so as it stands this game is not great the controls on teh sub you need to be able to move around all accesses in teh submarine, and at this point i think its not even worth submitting, the UI is still too cluttered and really takes away from the immersion, we need to actually be able to piot the sub around all access, on idea to make it more fun is to actualy have real dub controls with a ballast lever, also the interior is still not great but what really is lacking is the evnrioment i feel liek we need to generate it wether thats with pcg or just som cave gen alogrithm that you can tweak it needs to be better than this, also the whole tether mechanic seems a little pointless as it stands and it needs some tweaiing the captian is still useless too and everythign fees very adhoc and hacky lacking real poish and immersino, so think about how best to use editor tools to get real reseulsts continue"
- **What it changes:**
  - **Helm:** real sub controls. A throttle lever with notches (W/S), a rudder with inertia (A/D), a ballast lever with a neutral detent (Space blows, C floods), all stop (X). The sub moves along its heading, turns and changes depth. It is still the fixed point: the world turns about it, and a parked sub glides back to the stop's frame, so the code that runs at a stop sees what it sees today.
  - **Cave:** generated in C++. One seeded noise function sets the wall for both the mesh and the collision. The numbers are live in Project Settings > DEEP Tuning > Cave, and `Deep.Cave.Rebuild` rebuilds while playing. A wet-rock material, and rocks sunk into the walls.
  - **HUD:** most of it goes. What stays shows only when it is needed.
  - **Tether:** the line is the diver's air. The serpent can bite it through; the diver can unclip on purpose; the Captain's patience no longer cuts it.
  - **Captain:** off by default; `Deep.Captain.Radio 1` brings him back.
  - **Interior:** about 100 pieces of dressing and worn materials, light only from real fittings.
  - **Schedule:** lanes merge 5:15 to 6:30 PM; Quinn hand-plays at about 6:15 PM; the freeze run-through and package at 6:45 PM; video, itch page and form after; submit at about 8:30 PM.
- **Why:** Quinn played the build and found it ad hoc: the sub could not be flown, the screen pulled him out of the dive, the cave read as a pipe, and the line and the Captain did nothing for the game. Each lane answers one complaint, and each can fail alone without touching the floor build.
- **Challenged:** "not even worth submitting". `cee150a` is packaged and tested, so the team always has a build to submit; not submitting scores zero for certain, and the dev log is judged on its own for the Glass Box prize.
- **Not done, on purpose:** the PCG plugin (binary graph assets, a change to the project file, and on the Cut list); restyling the HUD (this pass only removes); new meshes (the dressing is engine shapes and pack rocks with new materials).
- **Fallback:** integration `cee150a`, packaged at 3:40 PM. A lane that misses its gate is left out.

### D-047 The harpoon kicks the diver back when it fires
- **Status:** Quinn's ask, about 3:22 PM Nov 15. Started at about 3:27 PM on `polish/harpoon-kick` (row 91 in [[Agent Runs]]), before his yes, because the freeze is at 5:30 PM. Built by about 3:33 PM (`e287d29`), reviewed, and in the integration build since 3:40 PM (`cee150a`: verify 128 of 128, S5 and S10-relic passed). One word from him takes it out, and nothing merges into `main` without his "merge".
- **Date:** 2026-11-15, about 3:22 PM EST
- **Who:** Quinn asked. The shape is the orchestrator's.
- **His words:** "so something that i was thinkif of adding is the gun, where it gives you knockback when you shoot at the serpent on the tether but not sure  feel like it adds to the fun factor"
- **What it changes:** no second gun. The harpoon (D-021) is the gun, with one shot per dive. A shot now shoves the diver back, opposite the aim: 3.5 m/s at first, gone in half a second, about a metre in all. The camera kicks harder on the shot, and a bolt into the serpent lands with a thud. The shove ends the moment a reel starts, so a shot at the sub still reels the diver home.
- **Why:** the single shot gets weight. In water a shot should push the shooter back. And the shove opens a beat of distance from the serpent before the shot's noise brings it back.
- **Not done, on purpose:** more shots, or a gun that kills the serpent. Plenty of ammunition turns a horror game into a shooter, and the lamp stays the main defence.
- **Fallback:** if the gate fails or Quinn dislikes the feel, the branch is not merged and integration is unchanged.

### D-046 The serpent is one model again, and its body moves by physics, dragged by the head
- **Status:** Quinn's direction, about 2:36 PM Nov 15. Being built on `polish/serpent-look` (row 89 in [[Agent Runs]]); frames before any merge.
- **Date:** 2026-11-15, about 2:36 PM EST
- **Who:** Quinn.
- **His words:** first, to the camera agent before 2:25 PM, as it relayed them: the serpent "doesn't animate at all and looks like two models smooshed together; I think the older one was better". Then to the orchestrator: "so instad of a slither mechanic we need eah spline to have physics so that it moves like an actual snake in 3d space undesrand the slither is not realistic, i feel like this hsoul dbe pretty easy the controller is the head"
- **What it changes:** the serpent is one model again: the code-built head with its jaw on the code-built body (`8b5b100`), not the Dragon Cave head on our body. Its body stops being laid along the head's path with a sine swing added on top. It becomes a chain of points the head drags through the water: Verlet integration with fixed spacing, a bend limit, water drag and the cave clearance. A small stroke at the head starts the wave, and the chain carries it down the body. The harpoon hit segments and the tube are built from the chain, so the AI, the bite and the harpoon do not change.
- **Why:** the weave was a formula, a sine wave added to the path, so the body swung without being pulled, and Quinn calls it not realistic. A chain that lags and follows gives an eel's motion with no new art.
- **His "pretty easy":** the chain itself is short. The work is keeping it stable at any frame rate and off the rock in a winding shaft, so the brief asks for the solver as a pure function with tests.
- **Fallback:** if the chain is not right by 3:45 PM, merge only the one-model fix (`8b5b100`).

### D-045 Toward the end: the orchestrator reviews each diff, then tests only what it touches
- **Status:** Quinn's direction, about 2:12 PM Nov 15. In force; `.claude/loop.md` ("What to run") says how.
- **Date:** 2026-11-15, about 2:12 PM EST
- **Who:** Quinn.
- **His words:** "i do think as we move twoards nearing the end we should move twoards more orchestrator code review (does it look like it works implications) then e2e tests. and we should wait until the feature lock to really run through"
- **What it changes:** at each merge the orchestrator reads the diff first. Does it look like it works? What does it change elsewhere: a state machine, a tuning number another system reads, an assumption a scripted playthrough makes? Then `verify`, then only the scripted playthroughs for the area the diff touches. New materials and pack content are still packaged at once. Every playthrough, the package, smoke, zip and a play by hand wait for the feature freeze at 5:30 PM.
- **Why it fits:** the same hour, a scripted playthrough (S11) passed for a reason nobody meant, and only reading its log showed it.

### D-044 The HUD palette changes: the colours do not match the theme
- **Status:** Quinn's direction, 1:54 PM Nov 15. For the HUD pass; nothing is built yet.
- **Date:** 2026-11-15, 1:54 PM EST
- **Who:** Quinn.
- **His words:** "not in the hud i dont like the use of color it doesnt match the theme please note that when we get to it"
- **What it changes:** both mockups use the style guide's HUD colours: cyan `#58E1D6` for numbers and hairlines, amber `#FFB45A` for warnings and red `#E5484D` for danger. The HUD pass reworks that palette whichever layout Quinn picks, and Quinn sees the new palette on frames before it goes in. [[Style Guide]] section 3 changes with it.
- **Open:** whether "not in the hud" also means no found-footage marks (a recording dot, a timecode) on the HUD. Asked at about 2 PM.

### D-043 The diver sends the sonar ping, so the cavern walls show from the water
- **Status:** Decided by the orchestrator at 1:54 PM Nov 15. Replaces D-042. Quinn can overrule. Building on `polish/camera`.
- **Date:** 2026-11-15, 1:54 PM EST
- **Who:** the orchestrator.
- **Why:** the lower roof (D-042) was built and measured, and it made no visible difference. From the hatch at stops 2 and 3, a level view is black (rock 73 m and 94 m away) and an upward view is the sub's belly. From 12 m out, looking up, the reticle finds rock at 18 m, but the lamp fades into the fog before it gets there. The cavern engineer's verdict: moving rock won't fix it; only the lamp's falloff and the fog will. Quinn asked for lidar-like beams "to see whats in front of you when it gets dark" (D-041), and the camera lane already has a sonar pulse that draws a band on rock as the ping passes.
- **Decision:** the diver gets a ping of their own: a key, a cooldown of about 3 s and a reach of about 90 m. For a moment it outlines the cavern. `Deep.Sonar.Pulse` goes on by default (Quinn was told at 1:46 PM that it would unless he said no). The pulse colour becomes a tuning value, because of D-044.
- **Not merged:** `0e14e21` (the roof tuning) and `ff0b0b5` (its frames) stay on `polish/cavern`.
- **Risk:** if the pulse cannot read through the fog at 60 to 95 m without a big change, the engineer stops and says why.

### D-042 The cavern reads from the hatch by a lower roof, not nearer walls
- **Status:** Dropped at 1:54 PM Nov 15: built (`0e14e21`) and measured, it made no visible difference. Replaced by D-043. It was decided by the orchestrator at 1:38 PM, on the cavern engineer's measurements.
- **Date:** 2026-11-15, 1:38 PM EST
- **Who:** the orchestrator.
- **The problem:** from the hatch at stops 2 and 3 the nearest wall is 60 m or more away, and the lamp lights to 35 m (`FDeepLampTuning.RangeM`) through heavy fog. So the dive still looks like open water, which is what Quinn added caverns to avoid ("if your swimming in an open ocean it really lessens it"). Reach is not the problem: the bed is 8 m under the hatch, and the farthest salvage spot is 44.6 m away on a 60 m tether.
- **What the engineer measured:**
  - **Nearer walls by tuning:** not possible. The farthest salvage spot plus its 3 m margin holds the wall at 47.6 m or more, and level rock shows only within about 15 to 20 m.
  - **A bell-shaped cave** (walls 15 to 18 m out from level upward, opening out below -30°) keeps every spot inside. But the serpent's containment would pull its 75 m lurk circle to 15 to 25 m from the hatch, so it would circle inside the lamp at the start of the dive. It would also break three tests in `DeepCavernTests.cpp`.
  - **Pillars from the dressing data:** they have no collision, so the tether would pass through them.
  - **A lower roof:** `RoofAboveAnchorM` 10 to 7.5, `HangTipClearM` 7 to 4 and `HangMinOutM` 19 to 15 put hanging rock about 15 m away looking up. Tuning only.
- **Decision:** the lower roof. The serpent's first sighting in the dark is the game's best horror beat, and it is worth more than nearer walls.
- **Checks:**
  - the sub's descent through the roof opening costs no hull;
  - the tether never passes through rock;
  - everything stays in reach;
  - the cavern and serpent tests stay green.

### D-041 Camera feel: shakes, a found-footage look, and the sonar made visible
- **Status:** Shakes are decided; Quinn asked for them. The footage look, the sonar pulse and the interference are built behind switches and stay off until Quinn approves frames. The HUD pass waits for the sonar and shaft merges. Lane `polish/camera` started 12:25 PM.
- **Date:** 2026-11-15, about 12:20 PM EST
- **Who:** The ideas are Quinn's; the orchestrator shaped them.
- **Quinn's words:** "i thinking of having a mechanic where you send out almost lidar tye beams to see whats in front of you when it gets dark, not sure if thats too much something diffrent, als not that the HUD doesnt match the theme and we need to do a pass there too, also we should add like camera shakes, your right more audio breathing ect any post porcessing motion blur ect, also had the idea like your looing through a camera and we have camera elements to it to give it that found footage look, but idk". Then, at 12:34 PM: "i might have camera shake in my library".
- **Decision:**
  - **The lidar:** not a second tool. The sonar ping becomes visible in the world. A shell expands from the sub over the sonar's range and outlines the walls for about 1.5 s in the sonar's amber, leaving lidar dots that fade. It is one mechanic, seen on the panel and in the world. It is a sensor overlay and lights nothing.
  - **Camera shakes** on hull hits, bites, the cut, the harpoon, the hatch, the descents and the serpent's approach. All of them are tuning values in C++. Quinn's shake pack is not needed: Unreal shakes are a few numbers on Epic's built-in patterns. It is worth adding only if it has recorded handheld motion, which would beat a noise sway for the footage look.
  - **The footage look:** the player sees through the diver's helmet camera, which the Captain watches from the sub. He cuts the footage as well as the line, which is the theme.
    - Grain rises with depth, like a camera pushing its gain.
    - Vignette, fringe at the edges, a slight lens distortion and a handheld sway.
    - Static before shape: the feed breaks up as the serpent closes.
  - **Motion blur:** frames with it off and at 0.2 decide it. The cave has to stay readable while driving.
  - **Breathing** follows the danger tier: calm, then fast, then panic with a heartbeat. The Captain-and-sound lane builds it.
  - **HUD:** one planned pass (D-028), after the sonar and shaft merges, since both change the HUD. It uses whichever look Quinn approves, and a mockup comes first.
- **Pushback:** a separate lidar tool would compete with the sonar panel for the player's attention and double the work. Five lanes already share one Unreal slot.
- **Safety:** `Deep.Camera.Shake` is on. `Deep.Look.Footage`, `Deep.Sonar.Pulse` and `Deep.Look.Interference` are off until approved. With all of them at 0, the game is today's.

### D-040 The Captain becomes a voice on the radio who says less; real sound replaces the synth
- **Status:** Decided by Quinn at about 12:15 PM, Nov 15. Lane `polish/captain` and a writer started at 12:17 PM. The lines were revised at 12:26 PM.
- **Date:** 2026-11-15, about 12:15 PM EST
- **Who:** Quinn, on the orchestrator's proposal.
- **Quinn's words:** "the capitian feature doesnt really bring anything in here too how hsould we change that", then "Captain direction yes, I'll add a Fab pack  - tell me what you are looking ffor my brother can make sounds or music too".
- **Decision:**
  - **Keep the Captain.** He carries the theme: he takes a cut of every haul and holds the tether cutter.
  - **Change how he talks.** He is a voice on the sub's radio. In the water, and while the sub is being driven, he says only what the diver needs:
    - chatter is silent;
    - a notable line waits at least 30 s after the last one;
    - when the serpent is close, only life-or-death lines get through.
    That silence is designed.
  - **His tone follows the danger:** smug when calm, clipped when tense, cracking when scared. His jokes move to the safe moments: the title, aboard, death and the settlement.
  - **Fewer lines:** 185 lines on 55 events become 74 on 59, each with a tone ([[Captain Script]]). The voice never says a number. Numbers stay on screen: the receipt, the depth readout and the diver number.
  - **The radio:** squelch on and off, and a voice file per line id when one exists. The other buses duck under him (Style Guide section 9).
  - **Real sound where we have it.** A Fab pack from Quinn covers the world. Quinn's brother makes the music and the serpent's voice, and perhaps the Captain's voice. What to get and how is in [[Sound Brief]].
  - **Fixed names, so his files drop in:**
    - music events `music_depth1`, `music_depth2`, `music_depth3`, `music_sting_serpent`, `music_tension`, `music_title`, `music_win` and `music_settlement`;
    - our own audio under `/Game/DeepAudio/`: `Captain/<line id>`, `Music/<event>`, and `Serpent/<event>_01` to `_03`.
- **Open:**
  - Who voices the Captain: Quinn's brother, OpenAI text to speech, or text only.
  - Whether an agent may import the brother's files headless into `deep/Content/DeepAudio/`. That would be a new exception to the binary-asset rule.
- **Safety:** with `Deep.Captain.Radio` and `Deep.Music` at 0, the game is today's.

### D-039 Drive the sub down a winding cave shaft with a sonar; every stop is a cavern
- **Status:** Decided by Quinn (11:55 AM Nov 15). Three lanes building: `polish/shaft`, `polish/cavern`, `polish/sonar`.
- **Date:** 2026-11-15, 11:55 AM EST
- **Who:** Quinn. The orchestrator challenged the full version and offered a scoped one.
- **Quinn's words:** "what makes this game fun and driving the sub with a radar mechanic or something is part of it really also i think we must do caverns because if your swimming in an open ocean it really lessens it". And earlier: bumping into walls is what damages the hull.
- **Decision:**
  - Between stops the helm descent becomes a cave shaft that bends, opens into caverns and pinches into squeezes. The player steers sideways to follow it. A wall hit jolts the view, scrapes, makes the Captain sneer and damages the hull. A breach kills the diver aboard, through the existing death and respawn path.
  - A sonar at the helm shows the walls round the sub and 20 m below, plus salvage and the serpent.
  - Every stop's arena is enclosed: rock walls and a roof, the sea bed as the floor, and a 14 m opening over the sub where the shaft comes in.
  - This un-cuts **Sonar** from the GDD's Cut list. It is Quinn's call.
- **Not done:** free driving through branching tunnels. The sub is the fixed point that the tether, hatch, stops, serpent and ending hang from (the world slides past it), so free driving would mean rewriting them under freeze. The shaft keeps that: at every stop the passage is centred on the parked sub.
- **Safety:** each part is behind its own switch (`Deep.Cave.Shaft`, `Deep.Cave.Cavern`, `Deep.Sonar`). Set to 0, a switch gives back today's game exactly, so any part that is not solid by 5:30 PM ships switched off.
- **Cost:** the fourth bubbles try (FX lane, half-built at the 11:19 crash) is parked on `polish/fx-wip4`, unfinished.

### D-038 Feature freeze moves from 3 PM to 5:30 PM
- **Replaced:** at about 4:15 PM Nov 15 by D-048, which moves the freeze to 6:45 PM.
- **Status:** Decided by Quinn (11:58 AM Nov 15).
- **Why:** the 3 PM freeze was the team's own buffer, set on the Board; the jam only requires the entry by 8:59 PM EST. The new plan is: features until 5:30 PM, the final scripted playthroughs and package at 6 PM, the video, itch page and form from 6 to 7:30 PM, and submission by 8 PM. That still leaves an hour for a crash or a bad merge (the machine went down at 11:19 AM).

### D-037 The relic becomes a real object, and the well holds its light
- **Status:** Built, in the integration branch (`5ae5f0b`, 8:56 AM Nov 15). Quinn can overrule.
- **Date:** 2026-11-15, 8:10 AM EST
- **Who:** The orchestrator, after looking at the dressing lane's frames of the 500 m site.
- **What was wrong:** The relic is a white glowing ring with a light of 12,000 units and a reach of 25 m (`Arena.RelicLightIntensity`, `RelicLightRadiusM`). Those numbers were set on Saturday night so that a player could find it across the sea floor. Since D-035 it lies in a shaft straight under the hatch. Two metres from it the light burns the pit to yellow-green and the frame reads as a debug view (`devlog/media/2026-11-15-dressing-500-bottom.png`). Every winning player comes here.
- **Decision:**
  - The relic becomes a real object from the Dragon Cave pack.
  - The shaft holds its light. The light rises out of the mouth as a glow, and that glow is the beacon.
  - At the bottom the relic is the brightest thing and does not burn the frame. The locked exposure stays.
  - How the relic plays does not change: its weight, its grab range, its place under the hatch and a clear winch path up the shaft.
- **Why:** D-029 puts looks level with gameplay, and this is the climax frame. The old job of the light, finding the relic across the floor, ended with D-035.
- **What follows:** For its second pass the dressing lane owns the whole 500 m site, including the relic's light numbers, its mesh and the pit's surface.
- **As built:** The relic is the pack's carved stone sphere (`SM_StoneworksSphere01`). Its light is 1,200 units over 7 m, amber, down from 12,000 over 25 m. A narrow shadowed spot sends a beam up the shaft, which is lined with cut stone. Weight, grab and winch path are unchanged; `Deep.Sim.Monster` (with `LastAscent`) and `Deep.Sim.Rules.ReviewAndRelic` pass. Frames: `devlog/media/2026-11-15-well-*.png`.

### D-035 The last ascent: a slower winch with the relic, and a shaft under the hatch
- **Status:** In the integration branch. Quinn has not seen it and can overrule any number.
- **Date:** 2026-11-15, 6:45 AM EST (engineer's entry), merged 6:54 AM
- **Who:** A systems engineer, under the orchestrator's brief "make the win reachable without changing the design". Nobody human has decided this yet.
- **What was wrong:** Not what the 5:50 AM alarm said. With the old numbers a scripted player who held the winch won 5 of 5 with the lamp and 3 of 3 without it. The winch (7 m/s) was as fast as the serpent's stalk at stop 3, so the serpent never reached the line and the lamp was never needed. The earlier "cut in 3 of 3" came from the playtest script winching in 1.5 s bursts. There was also a real trap: the relic sat 8 m out from the hatch, so the winch line ran through the wall of its shaft, and a diver cut there ended up under the sea bed with no way out.
- **Decision:** Three numbers, nothing else.
  - `Tether.RelicWinchSpeedMps`: new, 4 m/s (any other load stays at 7). The ascent with the relic takes about 21 s, so the serpent reaches the line and the lamp decides the ending.
  - `Arena.RelicOutM`: 8 to 0 m. The shaft is straight under the hatch, so the line runs up open water.
  - `Look.SeabedPitRadiusM`: 2.5 to 4.5 m. A wider shaft. At 6 m the sea floor test failed, because the bed sank under a salvage spot.
- **Evidence:** Scripted, on the shipped values: with the lamp 5 of 5 win; without it 0 of 3 win, each cut 35 m out, and all three drop the relic and reach the hatch alive. The full-run scenario now winches the relic home. New test `Deep.Sim.Monster.LastAscent`. Suite 66 of 66.
- **Open for Quinn:**
  1. The relic straight under the hatch is a layout change. The search for it is gone; the descent and the ascent are the set piece.
  2. Should the serpent hunt the moment it cuts the line? Today it flees first, so a careless diver who drops the relic gets home. The GDD says a careless one dies. The orchestrator's recommendation is to leave it: losing the relic and paying for the tether is already the punishment, and a judge who fails once can try again in the same run.
  3. A diver cut at the very bottom of the shaft could not swim out in the script (`S10-cutdrop` fails 2 of 2). It needs a try by hand before anyone calls it a bug.

### D-007 Solo-playable jam build; co-op, Steam and voice after the jam
- **Status:** Proposed
- **Date:** 2026-11-14
- **Who:** Claude
- **Decision:** The jam build is fully playable by one person. Co-op, Steam and proximity voice wait until after the jam.
- **Why:** Judges play alone. Netcode multiplies the work on every system. The Steam and voice plugins have no builds for 5.9 Preview. The Director fills the captain role from the original notes.
- **What follows:** Nothing in the code may block co-op later (D-010). See [[Co-op, Steam and Voice]].

### D-008 Theme reading: the Director, the cut, and Patience
- **Status:** Proposed
- **Date:** 2026-11-14
- **Who:** Claude
- **Decision:** The submarine AI is called the Director. It takes a cut of every haul, and it cuts the tether when its Patience runs out. Patience is the main clock of a dive.
- **Why:** The theme sits in the mechanics, where a judge meets it in the first two minutes. One meter is easier to read and to build than several.
- **What follows:** Details in [[GDD]].

### D-009 The Director speaks in scripted lines with pre-generated voice
- **Status:** Proposed
- **Date:** 2026-11-14
- **Who:** Claude
- **Decision:** The Director's lines are written ahead of time and triggered by game events, with pre-generated voice and subtitles. No live AI calls in the shipped build.
- **Why:** A judge needs no key and no connection. No secret ships in the package (D-003). Generating the voice with OpenAI text-to-speech counts as using OpenAI during development, which is what the OpenAI awards ask for.
- **What follows:** Needs an OpenAI API key from Quinn, or another voice tool. Subtitles ship either way.

### D-010 Code shape: pure tested rules, thin actors, tuning in config
- **Status:** Proposed
- **Date:** 2026-11-14
- **Who:** Claude
- **Decision:** Game rules are pure C++ functions with tests. Actors and components stay thin. Design numbers live in config. Blueprints are for glue.
- **Why:** Text can be diffed, merged and tested without the editor, and it survives an engine change in either direction. A design change becomes a data change.
- **What follows:** Quinn added a C++ module, `Deep`, at 8:55 PM Nov 14, so the project can take C++ now. Writing gameplay code still waits for Quinn's go (D-002).

### D-011 Tracks to target
- **Status:** Proposed
- **Date:** 2026-11-14
- **Who:** Claude
- **Decision:** Enter Best Overall and Best Team. Treat Glass Box as the best value per hour. Get cheap eligibility for the OpenAI awards and the Hyper3D pool. Enter Best Student Project if both are enrolled. Do the inexpensive look work for Render Royalty. Treat the NVIDIA RTX Award as at risk.
- **Why:** Most tracks pay only if the build is good, so the game comes first. The dev log is the one track where our process is the entry.
- **What follows:** See [[Track Strategy]]. Needs answers on student enrolment and a team name.

### D-012 Tiered validation T0 to T5, including AI playtesting
- **Status:** Proposed. The direction was requested by Quinn.
- **Date:** 2026-11-14
- **Who:** Claude
- **Decision:** Work passes through tiers: T0 Compile, T1 Rules, T2 In-editor functional, T3 AI playtest, T4 Package and smoke, T5 Human feel.
- **Why:** Quinn asked for specialised subagents and a solid end-to-end validation loop with AI playing the game. Agents can catch breakage, soft locks and unreadable screens. A human still judges whether it is fun.
- **What follows:** See [[Validation Loop]] and [[Agent Orchestration]]. T3 leans on toolsets that exist only in 5.9, which ties this to D-006.

### D-013 Gameplay holes fixed and scope narrowed
- **Status:** Proposed. Quinn asked for a briefer doc, the holes in the gameplay, and a to-do list cut to what finishes in time.
- **Date:** 2026-11-14, 9:10 PM EST
- **Who:** Claude
- **Decision:** The design in [[GDD]] changes as follows, and [[Board]] drops to twelve must-ship items.
- **Holes found in v1, and the fix:**
  1. Money soft lock. Salvage was finite while the Director took 40% plus fees. Fix: salvage respawns every dive and credits never go below zero.
  2. The monster arrived too late. Hull prices of 250 and 600 meant five or more dives with no threat. Fix: 50 and 200, so the monster is on screen by minute 3 and a run is about 9 minutes.
  3. Death was undefined, and a full restart late in a run would end a judge's session. Fix: respawn at the airlock as the replacement diver, lose the carried salvage, pay a fee.
  4. Nothing said what happens after a cut. Fix: reach the airlock on bailout air and be re-tethered for a fee.
  5. Sinking had no consequence on a tether. Fix: salvage sits below the sub, so an overloaded diver cannot swim home and must use the loud winch.
  6. The lamp beat the monster for free. Fix: only the beam, where it is pointed, drives the monster back. The lamp and winch no longer cost Patience.
  7. A tether touch causing an instant cut was unreadable. Fix: a bite starts a 5 second alarm that the beam can stop.
  8. The third stop had no idea of its own. Fix: the relic is too heavy to swim up and has to be winched.
  9. Patience had four inputs. Fix: time, a hunting monster, and salvage refills.
  10. Sound and a way to learn the controls were optional. Fix: a minimum sound set and a controls card are must-ship.
- **Cut to make time:** the gear upgrade shop (the hull rating is bought automatically), a unique third arena (one kit, three light settings), tether gun, sonar, DLSS, controller support, PCG terrain, the Water plugin surface.
- **What follows:** Needs Quinn and Colby to confirm. If time runs short the monster and then the Director's lines are cut, in that order.

### D-014 Design for fun and for winning
- **Status:** In effect. Quinn said "go" at about 9:12 PM Nov 14 and asked Claude to work out what makes the game fun and gives the best shot at winning, then start building.
- **Date:** 2026-11-14, 9:15 PM EST
- **Who:** Claude proposes, Quinn's go puts it in effect. Any part can still be overruled.
- **Decision:**
  1. One tension only: greed against the cut. Anything that does not feed "one more piece, or go home?" is cut.
  2. The cut is the best moment in the game and never ends the run by itself. It can also be requested, to reach rich salvage beyond the tether.
  3. The Director's writing is the personality. Dying is a joke about replacement divers. The settlement screen is the punchline of every dive.
  4. The first 90 seconds show the theme, the loop and the joke with no explanation screen, because a judge gives a jam game about five minutes.
  5. The monster is a pressure cycle driven by noise and pushed back by the lamp beam, heard long before it is seen. It is cheaper and scarier than a patrolling creature.
  6. The arena builds itself from code and config, so the game is playable in an empty map and testable headless. Art replaces stand-ins through config.
- **Why:** Jam judges score seven criteria after a few minutes of play. A small game that is readable, funny and finished scores above a large rough one. Darkness and fog hide cheap art. Audio and progression are judged and often skipped by other entries.
- **What follows:** [[GDD]] v3 and [[Board]]. Colby's work moves from blocking out a level to models, sound and tuning light settings. D-002 is lifted: code is allowed.

### D-018 A live AI brain for the Captain
- **Status:** Deferred. Quinn is deciding whether to make the Captain a live AI brain for an award track, and agrees it should come after the core game.
- **Date:** 2026-11-14, about 10:15 PM EST
- **Who:** Quinn decides. Claude recommends.
- **Facts:** The OpenAI awards do not require it. The jam page says "uses OpenAI models or products, during development, gameplay, or both", so voice generated ahead of time with OpenAI qualifies. NVIDIA ACE, the other live-AI route, is impractical here (see [[NVIDIA RTX, DLSS and ACE]]). The prize for the OpenAI awards is API credits.
- **Claude recommends:** Ship the scripted Captain as the default. Revisit at about 1 PM Sunday if the loop, the monster and sound are in. If yes, add it over the event bus with a fall-back to the written lines, and never put a key in the build.
- **What follows:** The options and their costs are in [[Fun Ideas]] under "A live Captain".

## Decided

### D-068 The relic waits in C1 until a stop-3 cavern exists; the caves stay off by default until then
- **Status:** Decided.
- **Date:** 2026-11-18, about 2:10 AM EST
- **Who:** Quinn.
- **Quinn's words:** "C1 now, stop 3 next", to: "M2b puts the relic in C1 so the whole cave run (S, a gate, C1, the relic) can be played now. The caves stay off by default until a deeper cavern for stop 3 is built past B, so the default game keeps its third stop."
- **Decided:** In the caves the relic's stop is 2: it waits in C1 at 300 m. The caves stay off by default until a stop-3 cavern exists: a new chunk past B that sinks from 300 to 500 m. Then the relic moves there, and the default flips on Quinn's OK. In the shaft the relic stays at stop 3.
- **Why:** The caves end at C1, and the chambers past it, B and E, are no deeper. A relic in C1 makes a whole cave run playable now, and keeping the shaft as the default keeps the game's third stop.
- **What follows:** M2b places the relic in C1 and adds a cave run to the win. Stop 3's cavern is the next cave job after M2b.

### D-067 The caves lay 10 salvage pieces a stop, in piles of 2 or 3
- **Status:** Decided.
- **Date:** 2026-11-18, about 2:10 AM EST
- **Who:** Quinn.
- **Quinn's words:** "10 a stop, in piles", to: "Keep 10 pieces a stop, 2 or 3 in a pile at each place, so each place is a small wreck worth the swim and the caves pay like the shaft."
- **Decided:** Each cave stop lays the shaft's 10 salvage pieces, with the shaft's kinds and values, in piles of 2 or 3 at the cavern's salvage sockets: S lays 3, 3, 2 and 2 over its four sockets, C1 lays 2 at each of its five.
- **Why:** S has 4 salvage sockets and C1 has 5, so one piece a socket paid about half the shaft, and each gate took about twice as many dives.
- **What follows:** M2b lays the piles. A new cavern gets enough sockets for its stop's pieces to fall in piles of 2 or 3.

### D-066 Frames go into git as JPEG only; the test harness's frames stay out of git
- **Status:** Decided.
- **Date:** 2026-11-18, about 1:09 AM EST
- **Who:** Quinn.
- **Quinn's words:** "JPEG only", to: "From now on, commit JPEG copies only, about an eighth of the size, and keep the test harness's own frames out of git. Files already committed stay."
- **Decided:** Frames committed to `devlog/media` from now on are JPEG copies, 1280 wide at quality 82. `tools/playtest.ps1` stops copying its frames into `devlog/media`; they stay in `deep/Saved/Harness/`, which git ignores. The PNGs already committed stay.
- **Why:** `devlog/media` held 483 MB of PNGs in plain git (560 files), and each playtest rewrote tracked PNGs, so every lane grew the history.
- **What follows:** a small tools change: playtest.ps1 writes no frames into git, and a converter goes into `tools/`.

### D-065 Depth in the caves follows the stops: C1's sink reads 100 to 300 m
- **Status:** Decided. M2a of [[cave-dive]] builds it.
- **Date:** 2026-11-18, about 1:09 AM EST
- **Who:** Quinn chose; Claude chose how.
- **Quinn's words:** "Follow the stops", to: "The depth readout, fog and light follow the stops, so the sink in C1 reads as 100 to 300 m. Stop 3 (500 m) needs one more deep cavern later."
- **Decided:** In caves the game's depth (`SubDepthM`, and so the readout, the zones, the look, the limit and the stops) is a read depth, mapped from the physical depth by a piecewise-linear table in `Content/Data`. It is true depth down to the top of C1's sink, then C1's sink is stretched so C1's berth reads 300 m (stop 2), then true slope below. The map keys off physical depth, not progress along the path, so driving level never moves the gauge. B and E read about 300 m.
- **Why:** The run's rules (stops, gates, zones, fog, light, the serpent's depth rules) all key off depth, so mapping one number keeps them working without moving the stops. True depth would squeeze the zones into 32 m; deeper rock would mean minutes of sinking.
- **What follows:** Stop 3 (500 m) and the relic's depth wait for a deeper cavern, a later cave round. Where the relic goes until then is Quinn's call at M2b.

### D-064 The caves lane brings the hull's real collision; the old shaft stays the default until M2
- **Status:** Decided. The lane `build/cavedive` has started.
- **Date:** 2026-11-17, about 12:02 PM EST
- **Who:** Claude, inside Quinn's OK for the lane.
- **Quinn's words:** "OK, merge and go", to: "Then the next lane puts the caves into the dive: you start in S and drive the sub through with steering and hull hits. The old shaft stays behind a switch."
- **Decided:** The lane that puts the caves into the dive also does moving-sub step 6's collision: the hull's real box sweeps against the baked chunks, and `UDeepCaveRules::Contact` retires with the shaft. The switch `Deep.World.Caves` stays 0 (the old shaft) through M1 and flips to 1 at the end of M2, once salvage, the stops and the gates work in the caves.
- **Why:** Driving the caves needs hits against the chunks' rock, which was step 6's job; doing it twice wastes a lane. Flipping the default before the run loop works in the caves would break every run test at once.
- **What follows:** [[cave-dive]] holds the plan. Moving-sub step 5 (removing D-022's code) comes after this lane.

### D-063 The serpent turns to each soft ping, but pings alone do not make it hunt
- **Status:** Decided and merged with the soft ping (D-060), 4:15 AM Nov 17 (`4367b3a`).
- **Date:** 2026-11-17, about 3:45 AM EST
- **Who:** Quinn, answering the merge question.
- **Quinn's words:** "Merge it", to: the serpent answers each ping but only hunts when you make other noise too.
- **Decided:** The helm ping still counts as noise, and the serpent still turns to each one. At the soft cadence, one ping every 6 s, pings alone no longer stack up to Stalk; it takes other noise on top. `Deep.Sonar.Style 0` brings back the jam's ping every 2.5 s sweep.
- **What follows:** Merged 4:15 AM Nov 17 (`4367b3a`) after frames from the packaged game showed the band at 0.5 s and 1.2 s. Its gate passed at 3:56 AM.

### D-062 BT-01 is the player's sub
- **Status:** Decided. The model lane has started.
- **Date:** 2026-11-16, about 11:45 PM EST
- **Who:** Quinn.
- **Quinn's words:** "also we should use the sub model"
- **Decided:** BT-01 replaces the basic-shapes dress on our sub, at the 12.3 m hull length the cave rules use (×2), inside the 12.3 × 3.3 × 5.0 m hull box. This settles D-056's working assumption; Quinn judges the scale in frames. The Atlantic Explorer stays the wreck (D-056). Credit: BT-01 by Oliv1e, Sketchfab, CC BY 4.0.
- **What follows:** the lane `build/sub-model`, behind `Deep.Sub.Model`; frames before it merges.

### D-061 The run starts in a calm start cavern
- **Status:** Decided. Chunk S is built in round 5 of the chunk tool.
- **Date:** 2026-11-16, about 11:45 PM EST
- **Who:** Quinn asked; Claude proposed the shape.
- **Quinn's words:** "i also would like to have the starting area a bit better maybe its just a cavern"
- **Decided:** a start chunk S replaces stop 1's old code-built cavern as the place a run begins: one calm cavern, the sub at a berth, the exit joint 25–40 m ahead of the bow, and a chimney overhead where the sub came down.
- **What follows:** the assembler lane puts the chunk string into the game and starts runs in S.

### D-060 The helm ping stays automatic, but soft and low-key
- **Status:** Decided and merged (4:15 AM Nov 17, `4367b3a`). The soft ping is the default; `Deep.Sonar.Style 0` brings back the jam's ping.
- **Date:** 2026-11-16, about 11:45 PM EST
- **Who:** Quinn decided; Claude set the starting values.
- **Quinn's words:** "i feel like the radar is a bit oto invasive", and asked how: "softer more minaml and modern lowkey still gives that horror vibe"
- **Decided:** the helm keeps pinging by itself. The wave goes thin and faint and reaches only nearby rock, the dots are sparse and brief, there is about one ping every 6 s, and the sound is softer and lower with a long dim tail. The diver's V ping (D-043) gets the same look, a little stronger.
- **Claude's call, for Quinn to check in frames:** cool #BFE3EA instead of amber, because the Style Guide keeps amber off rock and water.
- **What it changes:** the pulse's look from D-041 and D-043.

### D-059 Joints come in two or three types that must match
- **Status:** Decided. Round 5 of the chunk tool builds them.
- **Date:** 2026-11-16, about 11:45 PM EST
- **Who:** Quinn decided. Claude had recommended one shared rock outline with loose rocks over each joint.
- **Quinn's answer:** "2–3 joint types", after frames page v24 showed all four 9 m round joints reading as tubes from the helm.
- **Decided:** two or three irregular joint outlines drawn from real passage shapes, such as a bedding-plane slot, a rift or a keyhole. Every socket carries a type, and chunks join only where the types match, on the identical vertex ring. Each type holds the hull's 3.3 × 5.0 m section with 1 m clear all round. The chunk library must always let a string reach E.
- **Why:** one round profile at every joint reads as a tube. Matching types give variety, at the cost of a larger chunk library.
- **What it changes:** D-055's single joint profile and D-058's 9 m round joints. The assembler must match types.

### D-058 The dive is tunnels that open into caverns
- **Status:** Decided. The chunk tool builds a stop cavern and a turning tunnel next.
- **Date:** 2026-11-16
- **Who:** Quinn decided.
- **Quinn's words:** "im thining itsi like tunnels with caverns closer to the first option"
- **Decided:**
  - **Tunnels:** the sub flies level and nose first through the 9 m round joints. Slopes stay gentle (about 11° at most near a joint), and 7 × 7 m squeezes sit inside chunks.
  - **Caverns:** the sub sinks on ballast from the entry joint (high) to the exit joint (lower). A cavern keeps a clear column 16 m or more across, so the hull can turn in place.
  - Stops are in caverns. The wreck of the Atlantic Explorer (D-056) is the cavern at the bottom.
- **Why:**
  - A level hull 12.3 m long can't pass a 9 m joint going down, so the descent happens in caverns rather than in vertical passages.
  - It also suits two players (D-057): the diver can swim ahead through a squeeze and call the pilot through.
- **What it changes:** it replaces the "descent passages, mostly vertical" chunk type in [[Cave Gen - Chunk Assembly]]. The [[Slice Brief]] is rewritten once the first cavern and tunnel exist.
- **Changed by D-059:** the joints are two or three matching types, no longer 9 m circles.

### D-057 Two players: one drives the sub, one dives on the tether
- **Status:** Decided that DEEP is two-player co-op. The plugins are open.
- **Date:** 2026-11-16
- **Who:** Quinn decided; Claude proposes the voice idea.
- **Quinn's words:** "then I do want this to be a game, where ones driving ones on the tether, so we should look into that, im thinking that theres some plugin that allows easy steam support proximity chat ect, i might =even have it saved but continue with the cave"
- **Decided:** DEEP is a two-player co-op game, one player at the helm and one on the tether. It comes after the cave work.
- **Open:** the session and voice plugins ([[Co-op - Steam and Voice]]); which plugin Quinn has saved; solo play, assumed to stay as the jam build plays, one person doing both jobs.
- **Claude's proposal, not decided:** the tether carries the voice. It is clear over the line while the diver is clipped on, and close and muffled once they unclip or the line is cut. So a cut line also cuts the voice.

### D-056 The Atlantic Explorer is wreckage
- **Status:** Decided that the model is a wreck. The dive's goal and the upgrade loop are open.
- **Date:** 2026-11-16, about 1:10 AM EST
- **Who:** Quinn decided the wreck; Claude proposes the loop.
- **Quinn's words:** "we can have it as wreckage honesltly maybe the move is that we are recoving stuff from the wreck or maybe we can upgrade the ship? not too sure"
- **Decided:** the Atlantic Explorer model is a wreck in the cave, not the player's sub.
- **Open, Quinn unsure:** the dive's goal is to recover salvage from wrecks, and the earnings upgrade the sub. Claude proposes the two as one loop: wrecks are what each dive goes down to, salvage from them pays the Company, and what's left buys sub upgrades. This goes into the [[Slice Brief]] once Quinn decides.
- **Working assumption:** BT-01 at ×2 is the player's sub. Quinn hasn't confirmed the scale.
- **Settled by D-062:** BT-01 is the player's sub.

### D-055 Caves are baked chunks, strung in a new order every dive
- **Status:** Decided. The assembler is open; the research is [[Cave Gen - Chunk Assembly]].
- **Date:** 2026-11-16, about 1:05 AM EST
- **Who:** Quinn.
- **Quinn's words:** "im thinking lethal company stylegenerate caves so yes chunks"
- **What it changes:** the pipeline bakes a library of chunks with one standard joint profile, and a runtime assembler strings them together per dive from one replicated seed. The 60 m spike becomes two chunks joined both ways.
- **Why:** every dive is different, and the rock still looks hand-shaped.
- **Challenged:** D-053 had rejected modular kits because seams and repeats show. The answer: joints at restrictions with one fixed profile, rocks over each joint, and enough chunks. The spike tests the seam.
- **Open:** the assembler; see [[Cave Gen - Chunk Assembly]].
- **Changed by D-059:** two or three joint types that must match, instead of one standard profile.

### D-054 The jam entry went in solo
- **Status:** Decided. Done. D-050's post-jam rules still stand; only its "passed without a submission" wording no longer holds.
- **Date:** 2026-11-16, about 1 AM EST
- **Who:** Quinn.
- **What happened:** Quinn submitted DEEP to the 2026 Epic AI MegaJam as a solo entry, team name QuinnAho, because Colby did not have time to work on it. The cutoff was 2:00 AM EST (11 PM PST), later than the 8:59 PM EST the vault planned around. The build is `BuildsNoTag-2127` (tag `jam-final`), zipped as `QuinnAho_DEEP.zip`. The video is on YouTube. The dev log is a link-shared Google Doc with 15 screenshots; its text is in `devlog/itch-paste/DEEP-devlog.md`, with commit hashes and decision codes removed for outside readers. Modifiers: Solo Driver and Glass Box.

### D-052 PCG and Geometry Script are on; caves are made with the editor's tools
- **Status:** Decided. Committed in `6917b6d` at 10:19 PM: the build passed, the editor mounts both plugins, and the sim tests pass 64 of 64. Quinn restarts the editor to load them.
- **Date:** 2026-11-15, 9:43 and 10:04 PM EST
- **Who:** Quinn.
- **Quinn's words:** 9:43 PM: "yes good we want to use the editor tools lets continue" Then 10:04 PM: "the two plygins should be on"
- **What it reverses:** D-048's "not done, on purpose: the PCG plugin", and PCG terrain on the Devlog's Cut list.
- **What follows:** the beta PCG Geometry Script Interop plugin stays off until the cave spike needs it (D-053). Read Epic's PCG skills before building any PCG graph.

### D-051 The sub moves through a still world; the room is a fixed set; leaving is an airlock cycle
- **Status:** Decided. Being built on `build/moving-sub` in six commits, each green. The plan is that branch's `05 - Log/Lane Notes/moving-sub.md`. Steps 1 and 2 are running: the pure rules, then the room split and the airlock.
- **Date:** 2026-11-15, 9:39 PM EST
- **Who:** Quinn agreed to the orchestrator's proposal.
- **Quinn's words:** "yes that is an issue and it really messes with immersion and yes i agree with eerything here everythign should move twoards more traitional game dev"
- **What it supersedes:** D-022's moving world, in which the sub stood still and the world moved past it. Going inside the sub and driving it stays.
- **Why:** a world that moves fights real collision, World Partition streaming and maps built by hand.
- **What follows:** depth comes from height; the hull collides with real rock; the room you walk in is a fixed set; the stops become actors placed in the map.

### D-050 The jam is over; DEEP becomes a polished game, made the traditional way
- **Status:** Decided. The rules are in `CLAUDE.md` (`e7ed09f`). The jam's last build is tagged `jam-final`.
- **Date:** 2026-11-15, 9:11 to 9:39 PM EST
- **Who:** Quinn.
- **Quinn's words:** 9:11 PM: "looks like we are giong to miss it which is fine so focusing on building this out right is what we want" 9:29 PM: "ok now that we are notoing gunning for the game jam we can focus on making this a polished game instead, I think we ahve to have a make that is not data, thoughts?" 9:32 PM: "we dont need a super detailed devlog anymroe and we can move a little faster on that front"
- **What it changes:** a real map built by hand in the editor; humans own the maps; agents write code and tools, and make assets only in their lane's folder; binary files in Git LFS (`62c65b3`); Quinn's OK before each lane; a light dev log, with no new Agent Runs rows.

### D-036 Packs stay where they are installed; agents link to them; the cook takes only what is used
- **Status:** Decided.
- **Date:** 2026-11-15, 6:55 AM EST
- **Who:** The orchestrator, following D-033 and D-034, after finding that Quinn had installed Dragon Cave at 3:31 AM.
- **What happened:** Fab has no build of the pack for the 5.9 preview, so Quinn made a separate Unreal 5.7 project (`C:\Repositories\DeepImport`), added the pack there and brought it across. It landed in `deep/Content/DragonCave/`, not in the `deep/Content/Packs/` folder that D-033 had named. He did not say so in the chat; the orchestrator noticed the folder three hours later.
- **Decision:**
  - A pack stays where Fab or a migrate puts it. Moving it would break its own references. Each pack gets one line in `.gitignore`.
  - Each agent checkout gets a link to Quinn's copy. Packs are read-only: no agent saves, moves or renames anything inside one.
  - The cook includes only the pack assets the game uses. `deep/Content/Data/PackAssets.csv` names the ones that code loads by path, and the placement data names the rest. The pack's demo maps are never cooked.
- **Why:** The pack is 4.8 GB and the submission limit is 1 GB zipped. A pack in git would also break the push.
- **What follows:** Two lanes started at 6:57 AM: the dragon's head on the serpent, and rock and ruin for the dive sites. A package must be cut from a checkout that has the link. Before a worktree is removed its link is removed first, and `git clean` is never run in a worktree, because either could delete Quinn's files through the link.

### D-034 The monster uses the Dragon Cave dragon
- **Status:** Decided.
- **Date:** 2026-11-15, about 2:56 AM EST
- **Who:** Quinn, on seeing the pack's picture: "yes that dragon is perfect I think we should def use that or some varient of that for sure also rember all of quixels assets are free so we can use any version of thoes too".
- **Decision:** The sea serpent becomes a dragon-headed serpent: the realistic dragon from his Dragon Cave pack for the head, on the code-driven body, wearing the dragon's own skin so that head and body read as one animal. It is regraded near-black and wet to fit the style guide.
- **Megascans:** any Quixel Megascans asset may be used, not only the ones the asset scout listed: rocks, surfaces, decals, plants.
- **What it needs:** the pack installed in the project, which only Quinn's Fab account can do. Until then nobody has seen what meshes it holds: whether the dragon is one mesh or pieces, static or rigged. The creature engineer is building the body with an attach point, a tunable neck and a slot for the dragon's material, so the head drops in when it arrives.
- **Also worth using from the same pack:** its gothic stone arches, for the 500 m relic site as a drowned temple to the thing that hunts the diver. That ties the monster to the place.
- **Risk Quinn raised at about 3:00 AM:** "im worried about this not looking cood we maybe need to move it to some ai mesh enditor but we can cross that bridge when we get there". The weak points are the neck join, a skin that does not match between a detailed head and a code-built body, and a static head with no jaw. The test is the first frame of the join, taken as soon as the pack is installed. If it reads as two things stuck together, the fallbacks in order are: hide the join (darkness, a frill or fins at the neck); use more of the dragon mesh; make one serpent mesh with an AI mesh tool from the dragon as reference (Rodin, which also counts for the Hyper3D prize, or Meshy), which needs a key or perk from Quinn.

### D-033 Go for the polish pass
- **Status:** Decided.
- **Date:** 2026-11-15, about 2:47 AM EST
- **Who:** Quinn: "yes go, look at any monsters that we might have like a dragon or somethign that could be used idk yet, and do number 2 yes when you create particle effects since the feedback loop is slow rather tahn infer i would like you to look onliune for things or tutrials then do it, rember i want the bets possiblenviroment AAA horror game feel".
- **Decision:** The pass starts now, seven hours earlier than planned, with the water look and the monster. Agents may create materials and particle systems through the editor, each only in its own folder under `deep/Content/Deep/`, never a map. Before building an effect or a material an agent looks up how it is done and writes the recipe down.
- **How it is run:** each lane works on its own branch and nothing merges until Quinn has seen its frames. Two lanes at first, because the machine has 32 GB of RAM and Quinn's own editor is open. `*.uasset` is marked binary in git; packs stay outside git in `deep/Content/Packs/`.
- **Monsters he owns:** a title search of his 208 non-Megascans packs found no realistic sea creature. The dragons he owns are stylised and would break the one-style rule (D-030).
- **Supersedes:** the 10 AM start in the plan, and the blanket rule that agents create no binary assets.

### D-032 The style: dark realism, led by light
- **Status:** Quinn is leaning this way. He confirms it by approving frames.
- **Date:** 2026-11-15, about 2:40 AM EST
- **Who:** Quinn: "im leaning twoard realism wtih all the quixel assets, we really jsut need good water effects and a good monster", and a few minutes later: "this is horror after all maybe we could do some nice path tracing some reflective things off teh water falashlight".
- **Decision:** Dark, worn and realistic, built on Quixel Megascans. The look is led by light and reflection, not by the number of assets: the diver's lamp, wet reflective surfaces, the black water in the hatch, eyes that shine back in the beam. Water effects and the monster come first.
- **What the engine allows:** the project already has Lumen with hardware ray tracing on (DX12, `r.RayTracing=True`), so ray-traced reflections and shadows are available in the game now. Unreal's Path Tracer is an offline renderer and is not for gameplay; real-time path tracing exists only in NVIDIA's engine fork.
- **Still open:** the player's character (Quinn is thinking of a simplified one); the route to the monster; NVIDIA award eligibility, which needs DLSS, an NvRTX feature or ACE in the build. Ray tracing in stock Unreal does not make an entry eligible by itself. The engine ships only NVIDIA Reflex.
- **What Claude saw in the pack pictures:** the Sci-Fi Creatures Research Lab is clean, bright and teal and does not read as a cheap worn submarine; Modular Ship Interior and Industrial Infrastructure fit better; Atlantis Ruins could serve the 500 m site if graded dark.
- **Blocked:** a time-boxed test of NVIDIA's DLSS plugin on 5.9 was refused by this session's permission system at 2:42 AM, before it started. It needs Quinn: either he downloads the plugin himself, or he allows the test.

### D-031 Scripted playtests wait until a checkpoint
- **Status:** Decided.
- **Date:** 2026-11-15, about 2:30 AM EST
- **Who:** Quinn: "we are taking to long to run the play test i feel ask if w e can ignore and do laTer".
- **Decision:** No scripted playtest work runs between checkpoints. The agent that was speeding the playtests up and adding a side-return scenario was told to stop, keep its work on a branch and close its editor. The playtests run again before a package is cut.
- **What still guards every merge:** the build and the full automated test suite, about 30 seconds, run by the orchestrator on the committed branch (D-027). Playtests never blocked a merge.
- **What is deferred with it:** proof in real play that the diver now gets home from the side (`0874fe5`). The code and its tests are merged; Quinn can check it by hand in half a minute by swimming at the submarine from the side. Also deferred: making the playtest script fast (most of its time is the script starting a process per call, not the game).
- **Cost to bear in mind:** the scripted playthrough is what found the hatch stall. Skipping it until a checkpoint means bugs of that kind are found later, so the checkpoint before packaging must not be skipped.

### D-030 One style for everything
- **Status:** Decided. Which style is still open.
- **Date:** 2026-11-15, about 1:45 AM EST
- **Who:** Quinn: "its impoartant that everythign follows the same stule theme, a game will wlayes look bad if theres a strong discrepancy in style".
- **Decision:** Assets, HUD, effects, type and sound all follow one style. A good asset that clashes with it is left out.
- **How it is enforced with several agents working at once:** Quinn picks the direction from the look board (the asset scout sorted every pick into dark, worn and realistic, or stylised). That direction is then written down once, in `02 - Design/Style Guide.md`: palette, materials, lighting, type, sound. Every polish brief links to it, and Quinn approves frames at each gate.
- **Known conflict to settle by that choice:** the plan lists Corals and Atlantis Ruins for installing, and the asset scout says both clash with the realistic set.

### D-029 Looks are as important as gameplay
- **Status:** Decided.
- **Date:** 2026-11-15, about 1:25 AM EST
- **Who:** Quinn: "i would like to look at our assets, or look online or make it a priority looks is just as imporatnt than gameplay polish is a VERY big deal research if you need ect".
- **Decision:** The polish pass is a priority equal to gameplay. Research and planning start now, so that the pass itself is one piece of work with its inputs ready (D-028). Use what Quinn owns on Fab, what is free online, and what the engine ships with.
- **What changed at once:** Three research agents started at 1:32 AM: assets (owned and free), sound, voice, fonts and references from the web, and a technique plan for Unreal 5.9 with work packages.
- **Still Quinn's to choose:** the style, which packs to install, where real sound comes from, the Captain's voice, whether agents may create binary assets during the pass, and when it starts.
- **Relation to earlier decisions:** D-020 (no environment until the gameplay is right) still holds for building. This decision is about preparing, so that nothing is half-done.

### D-028 No half-done polish
- **Status:** Decided.
- **Date:** 2026-11-15, about 1:10 AM EST
- **Who:** Quinn: "i dont want to get too carried away with hud since it looks pretty bad" and "i geuss in want to wait on the polish and not half do it, or get it good enough think AAA prtofesional sleek ect".
- **Decision:** The main gameplay loop is the work now. HUD, look, effects and other finish wait. When they are done, they are done in one planned pass to a professional standard, not patched piece by piece on top of placeholders.
- **What changed at once:** Fix batch one lost two of its four items mid-run: the grey-box motion cues for driving and the HUD panel fixes. It keeps the title key press and a state report bug, which are function.
- **What it covers that Quinn did not name:** The sounds generated in code (D-026) are placeholders by the same rule. They stay switchable (`Deep.Audio.Mute 1`), and the final build ships none that Quinn has not heard and kept.
- **Claude's caution:** Visuals, audio and level design are three of the seven judged scores, and the deadline is 8:59 PM today. Waiting is right only if the polish pass has a start time and its inputs are ready: asset packs installed, a source of real sound, the Captain's voice. Otherwise the pass gets squeezed and is half-done anyway.
- **What follows:** Agents fix function, not finish. The orchestrator spends the wait on proving the loop with real key and mouse input.

### D-027 Test adaptively; all tests at merge
- **Status:** Decided.
- **Date:** 2026-11-15, about 12:50 AM EST
- **Who:** Quinn: "we are running too many tests on changes that dont need them be adaptive we can run all tests on merge".
- **Decision:** Run what a change can break, once. Agents build and run only their own area's tests. The orchestrator runs the full suite once per branch, at merge, on the committed branch head. A change to notes needs no tests. Scripted playtests run for the changed area; the whole set and a package run only at checkpoints.
- **Why Claude agrees:** The check that caught a red build at 12:31 AM was the one at merge, and that stays. What goes is the repetition before it. One engineer ran the full suite at least three times in one task, and the orchestrator's own last brief asked for the five-minute scripted set three times over.
- **What it costs:** A failure outside an agent's own area now shows up at merge and not in the agent's report, so fixing it costs one more round trip.
- **What follows:** The rule and its table are in `.claude/loop.md` under "What to run", and in the agent role files, `CLAUDE.md` and [[Validation Loop]].

### D-026 First sounds are generated in code
- **Status:** Decided by Claude as orchestrator. Quinn has not been asked and can overrule it.
- **Date:** 2026-11-15, 12:11 AM EST
- **Decision:** The sound layer ships with about ten sounds synthesised in code (ping, snap, alarm, thud, bubbles, click, harpoon, a paid chime, and loops for ambience, breathing, winch, serpent and driving). Each is one row in `deep/Content/Data/Sounds.csv`. A real file replaces a generated sound by importing it and filling in that row's asset path.
- **Why:** Audio is one of seven judged scores and the game is silent. Quinn owns no audio on Fab, Colby has not delivered files, and agents may not create binary assets. Generated sounds mean the game is never silent, whatever happens to the files.
- **What it costs:** Nobody on the agent side can hear. The sounds are kept low, soft and short, with tests on their shape, and `Deep.Audio.Mute 1` turns them off. Quinn has to listen and say which are bad.
- **What follows:** Sound was item 4 on the board, after the ending, the harpoon and the interior, which are all merged. Real files are still wanted for the final build.

### D-025 No "Director's cut" in the game's text
- **Status:** Decided. Supersedes the second half of D-016, where Claude kept the Director as the Captain's unseen boss.
- **Date:** 2026-11-14, 11:30 PM EST
- **Who:** Quinn: "remove directors cut refrences its too on the nose".
- **Decision:** The word "Director" and the phrase "Director's cut" come out of everything a player sees: the title screen subtitle, the settlement and review screens, 15 of the Captain's lines and one fee label. The Captain takes the cut himself and blames "the company".
- **What it costs:** Claude had argued for keeping the phrase because theme is one of seven judged scores. Quinn's call is that saying the theme aloud is worse than showing it. The theme is now carried by what happens: a cut of every haul, and a cut tether. The itch.io page should say how the game answers the theme in one sentence, since the game no longer does.
- **What follows:** Code identifiers keep their names, to avoid colliding with the engineers working now. A low-effort agent is making the text change on `text/no-director`.

### D-024 Water and rope feel, later
- **Status:** Decided
- **Date:** 2026-11-14, 11:25 PM EST
- **Who:** Quinn: "it will be importatn for this games immersie at the water phsyics are good, so think wit hthe harpoon rope and tether and effects that come from it, btu thats alter down the line".
- **Decision:** A pass on how the water, the tether and the harpoon line feel is planned, after the gameplay is right. It is not started.
- **What follows:** The scope is in the "Water and rope feel" section of [[GDD]]. Until then the tether and the harpoon line stay simple, and nothing built now should make a real rope harder to add.

### D-023 Three layers and their gates
- **Status:** Decided
- **Date:** 2026-11-14, 11:25 PM EST
- **Who:** Quinn: "for layer one you need to get a flashlight to see farthere down layer two, your hull needs upgrading, with this layer two the monster is introduced layer three the monetser is aggressive".
- **Decision:** Layer 1: no lamp and no monster; buying the flashlight is the gate to go deeper. Layer 2: the serpent is introduced; buying the hull upgrade is the gate. Layer 3: the serpent is aggressive; the relic.
- **Already true before this:** the serpent starts at layer 2 and is meaner at layer 3; there are two purchases at 50 and 200 credits.
- **What changes:** the diver no longer starts with a lamp, and the first purchase is the flashlight, not a hull rating. The flashlight is also the serpent's counter, so the player buys the tool one layer before meeting the thing it works on.
- **A note for now:** the scene is deliberately lit with no darkness (D-020), so "you need it to see" is not yet true on screen. Until there is darkness the gate is enforced at the helm: no flashlight, no deeper than 200 m.
- **What follows:** built as part of the submarine interior and driving task, which already reworks banking and descent. The table is in [[GDD]].

### D-022 Go inside the submarine and drive it
- **Status:** Direction decided by Quinn. The shape and the order are Claude's proposal and still open.
- **Superseded in part by D-051** (Quinn, 9:39 PM Nov 15): the sub now moves through a still world, and the room is a fixed set. Going inside and driving stays.
- **Date:** 2026-11-14, 11:10 PM EST
- **Who:** Quinn: "we will need to go inside the submarie and we will be able to drive it thats part of this".
- **What it reverses:** the submarine interior was on the cut list, and descending was a fade.
- **Claude's warning:** this is the largest addition so far, about 22 hours before the deadline, on a game no human has played yet and that has no sound, models, voice or look. The riskiest part is a character moving about inside a moving vehicle, which is a classic source of jitter and bugs.
- **Claude's proposed shape, to keep it cheap:**
  1. One room, entered through the airlock. Banking moves to a console inside.
  2. The room is flooded, so the diver swims inside as outside and no walking mode is needed. It also fits a submarine "held together by duct tape".
  3. Driving happens seated at a helm with a fixed view, and only while seated, so nobody moves around inside a moving hull.
  4. Driving is how the player descends to the next stop, replacing the fade. The hull rating becomes a depth limit: go deeper than rated and the hull takes damage.
  5. Hull damage from the harpoon and the serpent shows as leaks inside that the player patches by holding a key.
- **Proposed order:** the ending (running), then the harpoon (running), then this, and only after Quinn has played the current build by hand.
- **What follows:** waiting on Quinn for two answers: flooded room or walking, and whether driving is for descending only or for moving around freely. The harpoon already keeps hull damage as real state so this can use it.
- **Quinn's answer, 11:20 PM:** "once we are inside the sub is static and the world moves around use ... this is common game pattern and we will return salveage to a console inside the sub, inside the sub we are able to drive it, this make sense because we will need to sub to go deeper". This settles the shape and removes Claude's main worry: the submarine is the fixed point and the outside world moves, so nobody stands inside a moving object. Claude's reading of the rest, stated back to Quinn: a dry room you walk in, entered through a hatch in the floor; a console for banking; a helm for driving; driving goes down and up first, sideways later. The full spec is the "Inside the submarine" section of [[GDD]].
- **Order:** built after the ending and the harpoon are merged, because it changes the same files as both.

### D-021 The harpoon is in
- **Status:** Decided
- **Date:** 2026-11-14, 11:10 PM EST
- **Who:** Quinn: "i want the harpoon".
- **Decision:** One shot per dive. It latches onto the submarine and reels the diver home, which is what makes being cut survivable by skill. It also latches onto anything else solid, so it will work on terrain when there is some, and it drives the serpent back. Each hit on the submarine damages the hull and adds a repair fee to the next settlement.
- **Why this shape:** see [[Fun Ideas]]. The limits (one shot, about 25 m, a fee above the re-tether fee) keep the cut frightening.
- **What follows:** being built in `.wt\monster` on `dev/harpoon`, in parallel with the ending. Six new Captain events need lines.

### D-020 Raw gameplay first, no environment yet
- **Status:** Decided
- **Date:** 2026-11-14, 10:35 PM EST
- **Who:** Quinn: "i dont want any enviroment yet jsut raw gameplay workiung".
- **Decision:** No fog, light or atmosphere work until the loop plays start to finish. The grey box should be plainly visible instead: fog off, fixed exposure, even light, so the submarine, salvage and tether can be seen and the gameplay judged.
- **Why:** The first screenshots were a washed-out haze that hid the submarine and the salvage. Claude's response was to start a look pass on Opus. Quinn's call is the cheaper and faster one: a grey box only has to be visible, and tuning atmosphere before the gameplay exists is wasted if the gameplay changes.
- **What follows:** The look agent (run 23) was stopped after seven minutes with nothing merged. The core-loop engineer owns the environment files again and is making the scene plain. The look pass, and the live tuning commands planned with it, move to after the loop is playable. This overrides the "darkness is the art budget" work in [[GDD]] for now; the principle stands for later.

### D-019 The monster is a sea serpent
- **Status:** Decided for the creature. The behaviour is a first version for Quinn to shape.
- **Date:** 2026-11-14, 10:16 PM EST
- **Who:** Quinn: "im thinking snake since animation is easiest but we will need to work on the game ai for that".
- **Decision:** A sea serpent. Its body is a chain of segments that follow the path the head travelled, so it needs no rig and no animation assets. Two lights for eyes.
- **Behaviour, version 1:** five states. Lurk: circles in the dark beyond lamp range. Stalk: noise passed a threshold, so it closes on the middle of the tether in an arc. Recoil: the beam was held on its head, so it flees and waits out a cooldown. Bite: it reached the tether and holds on for a 5 second alarm; the beam releases it, otherwise the tether is severed. Hunt: the diver has no tether, so it chases the diver, and contact kills.
- **Why a snake fits:** the tether is a line and so is the serpent, so "it comes for your tether" reads at a glance. Open water needs no path-finding. The eyes show before the body does, which is where the fear comes from.
- **Rule for the behaviour:** readable and fair. It never severs the tether without the alarm, the lamp always works if aimed, and silence always calms it.
- **What follows:** A third builder is making it in `.wt\monster` on `dev/monster`, as new files only, so it merges cleanly with the core loop. Four wires are connected after both merge: noise sources, bite to the cut, caught to death, threat to sound and HUD. Every number is in `UDeepMonsterSettings` for Quinn to tune.

### D-017 Build our own MCP tooling
- **Status:** Decided. Quinn: "we should be building out our mcp tools as well make sure your updated with 5.9 feature". The three parts below are Claude's proposal.
- **Date:** 2026-11-14, 9:56 PM EST
- **Who:** Quinn asked; Claude shaped it.
- **Decision:**
  1. `tools\mcp.ps1`, a command-line client for Unreal's MCP server, so any agent or script can call editor tools without a Claude MCP connection.
  2. `DeepToolset`, our own tools registered with Unreal's toolset registry: typed game state and the test set-up actions.
  3. Scripted playtests that drive Play-In-Editor through the client and assert on game state, with a screenshot each.
- **Why:** Claude's own MCP connection is made at session start, so it breaks whenever the editor restarts and subagents may not have it. A script has no such limit. A scripted scenario also costs no model tokens to run, which is the best performance per token available for regression testing. The live catalog keeps us honest about what 5.9 really ships, as opposed to what was announced.
- **What follows:** A tools agent is building the client and the catalog against a live editor on port 8012. `DeepToolset` waits for the grey box, because it exposes game state. See [[Board]] items 5a, 5b and 6, and [[Unreal MCP in UE 5.9]].

### D-016 The Captain, not the Director
- **Status:** Decided for the name and the voice. The second half is Claude's proposal and can be overruled.
- **Date:** 2026-11-14, 9:45 PM EST
- **Who:** Quinn: "lets call it the captian not the director - should be saccastic kinda an assshole".
- **Decision:** The AI that talks to the player is the Captain. It is sarcastic, rude and greedy, and it mocks the diver. This matches the first notes, which had a Captain role run by an AI.
- **Claude's proposal to protect the theme:** The jam theme is Director's Cut, and theme is one of seven judged scores. Dropping the word "Director" would lose the pun the design was built on. So the Director stays as the Captain's boss at head office, never seen or heard. The 40 percent is still called the Director's cut on the settlement screen, and cutting the tether is a Director's order that the Captain enjoys carrying out.
- **Why this works:** A foul-mouthed middle manager blaming head office is funnier than a polite one, and the phrase "Director's cut" is still said aloud every dive.
- **What follows:** All 138 lines are being rewritten in the new voice as `CaptainLines.csv`. [[GDD]] is v4. The code is unaffected: it names the cut and Patience, not the character. The Brief doc in Google Docs still says "the Director" until it is reissued. If Quinn wants the Director gone entirely, about fifteen lines and the settlement label change.

### D-015 How the agents run
- **Status:** Decided
- **Date:** 2026-11-14, between 9:12 and 9:35 PM EST
- **Who:** Quinn
- **Decision:** Claude's main session is the orchestrator and writes no game code. Subagents run on the cheapest model that passes the gate first time, never above Opus 5.5, with effort tiered: `low` for Haiku runners, `medium` for Sonnet routine work, `high` for Opus critical-path code. Claude has full permission to commit and push.
- **Why:** Quinn's words: "use opus 5.5 models", "act as the orchestration agent ... best token to performance ratio", "routine agents should have lower effort", "you have full permission to commit and push".
- **What follows:** The rules live in `.claude/loop.md` and each role file. Commits happen at green checkpoints on `main`, and each push is checked for secrets first. Commits also feed the dev log.

### D-005 The project lives at `deep/` inside the git repo
- **Status:** Decided
- **Date:** 2026-11-14, about 8:50 PM EST
- **Who:** Quinn
- **Decision:** Quinn moved the Unreal project into the repo. It is at `C:\Repositories\deep-megajam\deep\Deep.uproject`.
- **Why:** One tree for code, assets, the vault and Claude's config, so everything is committed and shared together.
- **What follows:**
  - A `.gitignore` is in place (added 9:00 PM Nov 14). Still to decide before the first commit: whether to track `.uasset` and `.umap` with Git LFS (`.gitattributes`).
  - An old copy remains at `C:\Repositories\Deep`. Quinn removes it once the moved project opens cleanly. Claude will not delete it.
  - The editor writes `.mcp.json` into the project folder, and Claude Code reads it from the folder it is started in. Make sure the two match. See [[Unreal MCP in UE 5.9]].

### D-004 Use subagents for parallel work
- **Status:** Decided
- **Date:** 2026-11-14
- **Who:** Quinn
- **Decision:** Claude uses subagents so work moves faster.
- **Why:** There are about 24 hours and two people.
- **What follows:** The orchestrator hands out tasks and checks the evidence. Parallel agents multiply token use. See [[Agent Orchestration]].

### D-003 No secrets, tokens or perk codes in the repo or vault
- **Status:** Decided
- **Date:** 2026-11-14
- **Who:** Claude
- **Decision:** API keys, account tokens and the partner perk codes are never written into the repo or this vault, and no API key ships in the build.
- **Why:** The repo may be public. The jam email asks jammers not to share the codes. A key inside a packaged build is a leaked key.
- **What follows:** The codes stay in Quinn's email. See [[Partner Perks]] and the secrets rules in [[How We Work]].

### D-002 Quinn creates the project; no game code until told
- **Status:** Decided
- **Date:** 2026-11-14, about 8:35 PM EST
- **Who:** Quinn
- **Decision:** Quinn creates the Unreal project himself. Claude writes no game code and no project scaffolding until Quinn says go.
- **Why:** Claude had started writing a C++ project scaffold on its own, and Quinn stopped it. He wanted the research, memory and doc structure in order first.
- **What follows:** Research, notes and planning continue. The empty scaffold folders Claude had made were removed. Claude asks before creating anything under `deep/`.

### D-001 The vault is Claude's memory; the Google Doc is the design doc and to-dos
- **Status:** Decided
- **Date:** 2026-11-14
- **Who:** Quinn
- **Decision:** This vault is Claude's memory. The Google Doc holds the game design doc and the to-do list for humans. It does not mirror Claude's memory.
- **Why:** Colby needs a doc he can open and edit without the repo. Claude needs a place for research, decisions and the dev log that every session reads.
- **What follows:** Research, decisions and the dev log stay here. [[GDD]] is the copy of the design that agents read. Claude re-reads the Google Doc at the start of each work block. See [[How We Work]].
