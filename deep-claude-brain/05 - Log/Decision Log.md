---
updated: 2026-11-14 20:55 EST
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
| D-022 | The player goes inside the submarine and drives it | Decided (Quinn, 11:10 and 11:20 PM): the sub is static and the world moves |
| D-023 | Three layers, each with one gate: flashlight, then hull upgrade; serpent at layer 2, aggressive at layer 3 | Decided (Quinn, 11:25 PM Nov 14) |
| D-024 | Water and rope feel matter for immersion, and come after the gameplay | Decided (Quinn, 11:25 PM Nov 14) |
| D-025 | No "Director" or "Director's cut" in the game's text | Decided (Quinn, 11:30 PM Nov 14); supersedes the Director half of D-016 |
| D-026 | First sounds are generated in code; real files replace them one CSV row at a time | Decided by Claude (12:11 AM Nov 15); Quinn can overrule |
| D-027 | Test adaptively: an agent runs its own area's tests, and the full suite runs once, at merge | Decided (Quinn, about 12:50 AM Nov 15) |
| D-028 | No half-done polish: the gameplay loop first, then polish in one pass to a professional standard | Decided (Quinn, about 1:10 AM Nov 15) |
| D-029 | Looks are as important as gameplay: research and plan the polish pass now | Decided (Quinn, about 1:25 AM Nov 15) |

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
