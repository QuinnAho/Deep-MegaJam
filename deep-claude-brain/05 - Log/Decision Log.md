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

## Decided

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
