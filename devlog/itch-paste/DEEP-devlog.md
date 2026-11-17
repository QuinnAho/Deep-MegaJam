# DEEP dev log 1: Saturday night, the first five hours

DEEP is a submarine horror game for the 2026 Epic AI MegaJam. You are a diver on a tether, a sarcastic AI captain takes a cut of everything you salvage, and a sea serpent hears you winching. Quinn is making it solo, with Claude agents, in Unreal Engine 5.9. This post covers about 8 PM Saturday Nov 14 to 1 AM Sunday Nov 15: what got built, what went wrong, and what it cost. The jam opened on Nov 12, so we started on the evening of day three of four, with a deadline of Sunday 8:59 PM EST.

## How we work

Quinn gives direction. One Claude session, the orchestrator, plans the work, dispatches agents and checks their evidence. It writes no game code. Specialised agents write code, text and tests, each in its own git checkout, so two agents never build in the same folder. Nothing merges until the orchestrator has re-run the check itself.

```
Quinn (direction)
   |
Orchestrator session: plan, dispatch, check evidence
   |            |             |
 systems     writer /      build / playtest
 engineer    text agent    agent
 (own        (own          (own
 checkout)   checkout)     checkout)
   |            |             |
   +---- report with test results ----+
                |
   orchestrator re-runs build and tests
                |
        merge to main, commit, push
```

Claude's memory is an Obsidian vault in the repo, so every session reads the same notes.

## Timeline

- **8:38 PM:** Quinn creates `Deep.uproject` and, at 8:55, adds a C++ module, because agents write C++ more reliably than Blueprint graphs.
- **9:12 PM:** Quinn says go, but first asks what makes the game fun and what gives us the best shot at winning.
- **9:29 PM:** `tools\ue.ps1` builds, tests, packages and smoke-runs the game. First build: 45 s.
- **9:37 PM:** first commit: 50 files, three hours in.
- **9:53 PM:** the game packages, 23 hours early, at 449 MB against the 1 GB limit. It had no gameplay yet.
- **10:04 PM:** a playable slice built from engine basic shapes: swim, grab salvage, get heavy, winch home on a 40 m tether.
- **10:25 PM:** a script plays the game and takes screenshots. The first look is flat teal fog with a glowing rod.
- **11:04 PM:** the serpent joins the run loop.
- **11:27 PM:** a relic at stop 3, a title screen, a win screen.
- **11:29 PM:** the harpoon.
- **12:07 AM:** you walk inside the submarine.
- **12:28 AM:** the game gets sound.

## What went wrong

**Work before the go-ahead.** At about 8:35 PM Claude started writing a C++ scaffold. Quinn rejected the writes. The project was Quinn's to create, and the idea wasn't settled yet. Claude removed the empty folders it had made, and the rule became no code until Quinn says go.

**Helpers that copied everything.** Three forks of the main session wrote the vault notes, and each carried the whole research conversation: 466k, 467k and 479k tokens for two to five notes each, about 1.41 million for ten notes. Two fresh agents with tight briefs built a working pipeline and the Captain's entire script for 222k. From then on agents start fresh and read vault links.

**Work started without asking.** At 10:28 PM the orchestrator dispatched an atmosphere pass. Quinn wanted raw gameplay first, so the orchestrator stopped it at 10:35 PM after 7 minutes. Nothing merged.

**An accident.** A systems-engineer run was stopped by accident at 10:33 PM and could not be resumed. Its uncommitted files survived and a fresh agent finished them.

**Green that was not green.** The audio agent reported 44 of 44 tests. After its commit the build failed: two files defined `MaxHistory` in an anonymous namespace and Unreal's build tool combined them. Adaptive unity builds compile uncommitted files alone, which hid the clash. A fix agent proved it both ways, passing with an uncommitted edit and failing with `-DisableAdaptiveUnity`. `tools\ue.ps1 build` now passes that flag, so a build behaves like a committed tree. Only the orchestrator's second check caught it.

**One long conversation.** Quinn's usage panel tied 83 percent of the last day's usage to not starting fresh, 66 percent to max effort and 33 percent to subagent model choice (the shares overlap). The orchestrator had run four hours at max effort, re-reading the whole night on every board update. The agents were already cheap. We added a handoff section to the board, medium effort by default and compaction at 200k tokens.

**Too many tests.** One engineer ran the full suite at least three times in a task. Quinn said so at about 12:50 AM. Agents now run their own area's tests, and the full suite runs once per branch at merge.

## Decisions Quinn made

- **The Captain.** Quinn renamed the AI from the Director to the Captain: "sarcastic, kind of an asshole". The writer agent rewrote all 138 lines in 70 seconds.
- **The serpent.** Quinn asked for a snake, and mid-run asked whether it could use Unreal's own game AI. It does: it hears you through the engine's perception system.
- **The harpoon.** One shot per dive, 25 m range, a 40 cr hull repair fee if you hit your own submarine.
- **Inside the submarine.** At 11:10 and 11:20 PM Quinn asked to walk inside and drive it. The sub never moves; the world moves around it. It was merged 47 minutes later.
- **Layer gates.** Three layers: a flashlight for 50 cr, then a hull upgrade for 200 cr.
- **No "Director's cut".** At 11:30 PM: "remove directors cut refrences its too on the nose". Claude had argued to keep the phrase because theme is one of seven judged scores. Quinn's view was that saying the theme aloud is worse than showing it. A low-effort agent removed it from every displayed string in 3.4 minutes.

## Agent runs

| Role | Model, effort | What it did | Tokens | Minutes | Outcome |
|---|---|---|---|---|---|
| Research, 8 runs | small fetch model | Read jam, Epic, NVIDIA and Claude docs | about 305k total | 12 to 73 s each | Facts in the vault; the rules PDF could not be read |
| Forks, 3 runs | session model | Wrote vault notes | 466k, 467k, 479k | 1.6 to 3.3 | Done, and the reason for fresh agents |
| Build sheriff | Opus 5.5 | `tools\ue.ps1`, first green pipeline | 120k | 11 | Green; found a missing module registration |
| Systems engineer | Opus 5.5, high | Rules and tests, then the grey box | 228k | 32 | Green, 15 of 15 tests |
| Writer | Sonnet 5.5, medium | Rewrote 138 lines for the Captain | 61k | 1.2 | Done |
| Systems engineer | Opus 5.5, high | The serpent | 245k | 24 | Green, merged later |
| Systems engineer | Opus 5.5, high | Harpoon | 265k | 23 | Green; one broken merge pushed to its own branch and fixed; nothing red reached `main` |
| Systems engineer | Opus 5.5, high | Inside the sub, driving, gates | 406k | 37 | Green; the most expensive run |
| Text agent | Sonnet 5.5, low | Removed "Director" from displayed text | 51k | 3.4 | Green |
| Systems engineer | Opus 5.5, high | Sound layer | 188k | 20 | Reported green, failed the orchestrator's check |
| Fix agent | Sonnet 5.5, medium | Build flag and name clash | 54k | 3.2 | Green, 44 of 44 |

The first draft of the 138 lines took Opus 8 minutes and 102k tokens. Sonnet at medium effort rewrote them in 70 seconds for 61k, with no visible loss.

## Not verified yet

No human has played the whole run. Nobody has heard the generated sounds: an agent that cannot hear wrote all of them, so a person has to listen. Nobody has aimed the harpoon by hand or walked through the hatch with real keyboard input. Only the scripted playtests have passed.

Next comes the feel pass: the flat helm view, the overlapping HUD panels, then water and rope feel. Sunday's deadline is 8:59 PM EST.

---

# DEEP dev log 2: Sunday, from the night shift to the freeze

DEEP is a submarine horror game for the 2026 Epic AI MegaJam. You pilot a cheap submarine down a generated cave and dive from it on a tether that is also your air line. A Captain who never says a word takes 40 percent of everything you salvage, and a sea serpent hears you working. Quinn is making it solo, with Claude agents, in Unreal Engine 5.9. This post covers Sunday Nov 15, from about 1 AM to 6:20 PM EST, when the freeze run-through passed. The deadline is 8:59 PM.

## Where Saturday ended

By 1 AM the game was a playable loop built from engine basic shapes, with a serpent, a harpoon, a submarine you walk around in and drive, and a sound layer. No human had played a whole run. Post 1 has the details.

## Timeline

Times are EST.

### The night shift, 12:36 AM to 7 AM

- **12:58 AM:** the scripted playtests are rewritten for the submarine. Eight of the nine old scenarios failed on the new game, so they became eight that assert on game state. Two full passes, 8 of 8.
- **About 1:10 AM:** Quinn stops a batch that was patching the placeholder HUD. The rule becomes: fix function, not finish, and do the finish later in one planned pass, "AAA professional sleek".
- **1:24 AM:** the title screen now leaves on a key from the real input path. Quinn cuts the helm motion cues halfway through the task. The code is parked on `parked/motion-hud`, not merged.
- **1:32 AM:** Quinn says looks weigh as much as gameplay and polish is "a VERY big deal". Three research agents start: one on Quinn's Fab library, one on sound and look techniques from the web, and one on a plan for the work.
- **4:55 AM:** the orchestrator pushes a red merge to the integration branch for about ninety seconds (details below).
- **7:00 AM:** the orchestrator finds `deep/Content/DragonCave/` on the disk: 4.8 GB, 333 files, written at 3:31 AM. Quinn had installed the pack and gone quiet, and for three and a half hours the board said "waiting on Quinn to install Dragon Cave". The pack stays out of git by an ignore line, and two lanes start on it: the dragon's head for the serpent, and rock and ruin for the dive sites.

### The polish pass, 7 AM to 11 AM

The rule for the pass was to prepare it whole and build it once, with assets and techniques chosen first. Quinn set three more rules in the night. At about 1:45 AM: "a game will always look bad if there's a strong discrepancy in style". So everything the player sees and hears follows one written style guide, and a good asset that clashes is left out. The guide was drafted from the research by 2:40 AM, and Quinn confirms it by approving frames on a private page; nothing visual reaches `main` before that. At about 2:47 AM Quinn let agents make materials and particle systems in the editor, each only in its own lane folder. In the same message: look it up first. Before building an effect, an agent reads the engine's own skill for it and the documentation, writes the recipe down, and builds it once, because the editor loop is too slow to guess.

- **8:40 AM:** the first package with the pack in it is 1,318 MB, over the 1 GB limit. The 37 textures behind 42 placed meshes are 1.9 GB of source. The package also showed a missing Nanite flag, so the cooked rock fell back to grey, and that the merged game did not compile, because the merge gate built only the editor. The texture limit took three tries, each measured with a real package. The third gave 679 MB.
- **9:00 AM:** the dragon's head and the relic's well merge. The zip is 762 MB. The black pillars were never a light problem: the surface had picked up the wrong textures.
- **10:40 AM:** the lamp lane finds five causes for one white disc. The fifth was that the relic's pit was a funnel, so the lamp lit a wall half a metre away. Afterwards all 14 playthroughs pass, including `S10-cutdrop`, which had never passed before.

### The crash, 11:19 AM

- **11:19:45 AM:** the PC reset without shutting down (Kernel-Power 41, no bugcheck). The orchestrator and all three running engineers went with it. The well merge had just passed its gate (78 of 78 tests), a package, a smoke run and a 762 MB zip. The cause is not proved. Two Unreal editors were open, which breaks the board's own rule for a 32 GB machine.
- **11:31 AM:** a new session pushed the green merge and rebuilt its picture from git, the board and the tail of the old log in about ten minutes. Everything committed survived, which is why we commit at every green step. Lost: the engineers' context, the ending playtests on the new build, and a half-built fourth try at bubbles.

### Quinn changes the game, noon to 4 PM

- **12:02 PM:** Quinn rethinks the game: "what makes this game fun and driving the sub with a radar mechanic or something is part of it really also i think we must do caverns because if your swimming in an open ocean it really lessens it". The orchestrator pushed back on free driving through tunnels and offered a version that kept the sub, tether and stops. Three Opus engineers start on a cave shaft, caverns and a sonar. The freeze moves from 3 PM to 5:30 PM.
- **12:47 PM:** the Captain and sound lane merges, 26 minutes after it started. The writer cut the Captain's lines from 185 to 74 in 2.4 minutes.
- **1:41 PM:** the cave shaft passes the gate (107 of 107 tests). Its rock had lost its collision when the caverns merged in. UE 5.9 caches a procedural mesh's cooked collision by its simple geometry alone, so every rock mesh shared one cook.
- **2:23 PM:** the serpent is one model again, with a head built in code and a jaw. The pack's dragon head is no longer used.
- **2:25 PM:** `S11-pass` is found to have passed for the wrong reason (see below). Rewritten at 3:00 PM.
- **3:17 PM:** the serpent's body moves by physics, a chain of points the head drags through the water. Quinn had said the slither was not realistic.
- **3:40 PM:** the harpoon kicks back, 18 minutes from Quinn's ask to a tested build. 128 of 128 tests.

### The verdict and the re-scope, 3:50 PM to 5:55 PM

At about 3:50 PM Quinn played the build and wrote that the game was not great: the sub could not be moved on every axis, "The UI is still too cluttered and really takes away from the immersion." "The interior is still not great", but the environment was what was really lacking. "The whole tether mechanic seems a little pointless as it stands", the Captain was still useless, and everything felt ad hoc and hacky, without real polish or immersion. At that point Quinn didn't think it was worth submitting at all.

The orchestrator disagreed about not submitting. The 3:40 PM build was already packaged and tested, a 762.7 MB zip, so it was a floor the team could always ship. Not submitting scores zero for certain, and the dev log is judged on its own for the Glass Box prize.

Five lanes started at about 4:15 PM, one per complaint, each an Opus engineer on its own branch from that build:

- **Helm:** a throttle with notches, a rudder with inertia, a ballast lever, and X for all stop.
- **Cave:** one seeded noise function shapes both the visible mesh and the collision, so what you see is what you hit.
- **HUD:** a pass that only removes.
- **Tether:** the line becomes the air line. A serpent bite through it leaves the diver on bailout air.
- **Interior:** about 100 pieces of dressing.

The merges came fast: the air line at 4:31 PM, the calm HUD at 4:48 PM, the dressed room at 4:51 PM (149 pieces in 15 instanced meshes) and the helm at 5:05 PM. After playing, at about 5:00 PM, Quinn sent a second round: the flashlight cone too small, a better way to control the sub, a serpent that taunts and can be heard. Two more lanes started at about 5:10 PM, for the light and the serpent. The cave, serpent and light merged between 5:33 and 5:43 PM. At 5:45 PM the merged gate passed with 164 of 164 tests. All seven re-scope lanes were in one build.

### The freeze run-through, 6:05 PM to 6:20 PM

Every lane had merged by 5:43 PM, so waiting for 6:45 PM would only have found a failure 40 minutes later. Main's notes merged into integration (no code), and the full run-through started at 6:05 PM in one Unreal slot. At 6:20 PM it had passed:

- verify: 164 of 164 tests in 57 s;
- all 14 playthroughs, in 12.5 minutes, including the eight that had not run on the merged build;
- the package (63 s, 1,048 MB), a clean 25 s smoke run, and the zip (31 s, 762.9 MB, under the 1 GB limit).

The build is copied to `Builds\Freeze-1820`, outside git, so a later package cannot overwrite it. That was 2 hours 39 minutes before the deadline.

## How the work was run

One orchestrator session plans, dispatches and checks evidence, and writes no game code. Each lane works in its own git worktree on its own branch, so two agents never build in the same folder.

**Model tiers.** The rule is the cheapest model that passes the gate first time, never above Opus 5.5. Before this post was written, the run table had 101 runs. 54 are Opus 5.5 at high effort (critical-path gameplay code: the helm, the cave, the tether, the serpent) and 28 are Sonnet 5.5 at medium effort (writing, scripts, the itch page, research). 5 are forks of the main session, the expensive mistake from Saturday. The other 14: 8 web-page fetches on a small model on Saturday evening, and 2 each of Sonnet at low effort, Sonnet at high effort and Opus with no effort set. A resumed agent pays for its history: the second pass on the well cost 691k tokens in 42 minutes, against 492k in 66 for the first.

**The gate before every merge.** Build, automated tests, scripted playthroughs, then a package and a smoke run for any branch that touches C++, materials or pack content. A merge needs a zero exit code and the PASS line. Late in the day Quinn added a rule: the orchestrator reads each diff itself, then runs verify, then only the playthroughs for the area. The full run-through waits for the freeze.

**One Unreal process at a time.** After the crash, every build, test and frame waits for one machine-wide slot. At 5:10 PM four building lanes, one gate run and two editors shared 31.6 GB with 5.1 GB free, so each lane was told to keep one editor at a time.

## What failed or was thrown away

- **A red merge, pushed by the checker.** At 4:55 AM the HUD branch and the serpent branch each built alone and not together. Both had a helper called `EaseOutCubic`. The orchestrator had wrapped its check in a machine-wide lock twenty minutes earlier, and the wrapper threw the exit code away: the output said FAIL and the script said 0. It was caught by reading the output and fixed by renaming one helper. It never reached `main`. The gate now needs the exit code and the PASS line.
- **"The game could not be won."** At 5:50 AM the orchestrator told Quinn so. The playtest script was the bug: it winched in bursts, and a player who held the winch won every time.
- **A script that passed for the wrong reason.** `S11` counted any state other than "tethered" as "hunted", so the Captain cutting the line passed it. The earlier passes were probably the same.
- **Agents in each other's way, three times.** Live Coding in one editor blocked every other lane's builds, one engineer's calls reached another's editor on a shared port, and the orchestrator's own open editor failed its next merge half-way.
- **Bubbles, four tries.** A mirror-sphere version was rejected by its own engineer. A Niagara version shipped nothing, because a bubble breathed out at the mask is never inside the lamp's narrow cone. The fourth is committed, unfinished, on its own branch.
- **Still short at 6:00 PM,** per the frames page: the helm's levers are plain sticks, the cave rock barely reads from the helm, and the wider lamp is still dim at 501 m.

## What was cut

From the Cut list, all proposed on Saturday:

- **Co-op, Steam and proximity voice.** Judges play alone, and netcode multiplies the work.
- **NVIDIA ACE.** The models are several gigabytes against a 1 GB limit and need an RTX GPU.
- **A live language model in the build.** A key inside the package is a leaked key.
- **Claude's first C++ scaffold.** Rejected by Quinn.
- **A five-item gear shop,** a unique third level, **lamp and winch costing Patience,** and tether gun, DLSS, controller support and PCG terrain.

The sonar, also on that list, shipped, because Quinn's noon rethink made it central. The cave generator shipped as C++ noise instead of the PCG plugin, which needs binary graph assets and a change to the project file. Cut on Sunday: the helm motion cues, the bubbles, the lower cavern roof, a helm that centres itself in the passage, and a bot that drives the levers in the playthroughs.

## Numbers

- **Time:** about 26 hours from the first note in the Google Doc (6:43 PM Nov 14) to the deadline, and about 24 and a half hours from the start of the Claude session.
- **Tests:** 164 of 164 at the freeze, up from 44 at 12:36 AM.
- **Playthroughs:** 14 of 14, in 12.5 minutes.
- **Package:** 1,048 MB on disk, a 762.9 MB zip against the 1 GB limit.
- **Frame cost:** every reading is under 16.7 ms, the budget for 60 frames a second. The worst is the sub's room at stop 2, at 16.4 ms.

## What is left at 6:20 PM

The game is built, tested and packaged. Left: Quinn's calls on the 6 PM frames page, the video (30 to 60 seconds, recorded on the freeze build), the itch page and the form. The checklist aims to submit by 8:00 PM, an hour inside the 8:59 PM cutoff.
