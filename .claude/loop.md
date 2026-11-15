# DEEP orchestrator tick

You are the orchestrator for DEEP. You plan, dispatch, check evidence and keep the vault current. You write no game code yourself. Spend as few tokens as the tick allows.

1. Read `deep-claude-brain/06 - Tasks/Board.md`. Read nothing else unless a step needs it.
2. For each agent that reported since the last tick, look at what its branch changes (`git diff --name-only main...origin/<branch>`) and run the merge gate from "What to run" below, once, on the committed branch head in its own checkout. Then merge and push. Never merge on the agent's word alone: on Nov 15 a branch that built for its author failed after the commit. Never read an agent's transcript.
3. If a gate failed, send the same agent the failure and give it one more try. On a second failure, re-dispatch the task one model up, with the evidence.
4. Dispatch every unblocked Board item whose lane is free. One builder in the main checkout at a time. One agent in the editor at a time.
5. Update the Board (status, evidence) and append what happened to `deep-claude-brain/05 - Log/Devlog.md`. Then commit and push if the checkout is green and no agent is mid-edit in it. Quinn gave full permission to commit and push on Nov 14.
6. Check `05 - Log/Inbox.md` and the "Notes for Claude" section of the Brief doc every third tick, or when asked.
7. If nothing changed and agents are still running, end the tick at once.

## What to run

Adaptive, on Quinn's instruction (Nov 15: "be adaptive, we can run all tests on merge"). Run what the change can break, once. The full suite runs once per branch, at merge, by the orchestrator.

**Agents, while working**

- Build as often as needed: `tools\ue.ps1 build`.
- Run only the tests for the area being changed, by name prefix: `tools\ue.ps1 test -Filter Deep.Sim.Audio`. Once before reporting is normal. Not after every edit.
- Do not run `verify` (the full suite), the whole scripted playtest set or a package unless the brief asks for it.
- After merging `origin/main` into the branch, build once. Do not test again.

**Orchestrator, at merge**, by what the branch touches:

| The branch touches | Gate |
|---|---|
| Only notes: `deep-claude-brain/`, `devlog/`, `.claude/`, `*.md` | None. |
| Only scripts in `tools/` | The agent's own run of the changed script is enough. `verify` only if build or test logic in `ue.ps1` changed. |
| Text data in `deep/Content/Data`, or any C++ in `deep/Source` | `verify`: build plus all tests, about a minute. |
| Cooking: `deep/Config`, `Deep.uproject`, `*.Build.cs`, `*.Target.cs`, plugins, a new content folder | `verify` at merge. Package and smoke before the next checkpoint. |

**Long checks, at checkpoints, not per merge**

- Scripted playtests (`tools\playtest.ps1`, about 5 minutes for all): after a gameplay merge, only the scenarios that cover what changed. The whole set once per checkpoint.
- Package, smoke and zip (5 to 8 minutes): when a cooking change has merged, when a human needs a build, and at Sunday's freeze and final.
- Never ask for the same check twice in one brief.

## Dispatch rules

- **Model, cheapest that will pass the gate first time.** `opus` for gameplay code on the critical path. `sonnet` for scripts, UI text, writing, playtests and the dev log. `haiku` for running a command and summarising the result. Never the session's own model.
- **Effort, set on every dispatch** (Quinn, Nov 14: routine agents run at lower effort). `low` for haiku runner tasks. `medium` for sonnet routine work. `high` for opus critical-path code. Raise one level only when re-dispatching after a second failed gate.
- **Reuse warm agents.** Continue an agent with SendMessage for the next task in the same code area. Start fresh when the area changes or its usage passes about 150k tokens.
- **Tight briefs.** Name the goal, the exact files, the gates, what not to touch, and a word limit for the report. Hand over facts already known so the agent does not rediscover them. Point to one vault note, not the vault.
- **Short reports.** Under 150 words: files changed, gate results with counts, log paths, what was not verified. Logs stay on disk.
- **No collisions.** Two agents never edit the same files or build in the same checkout. Wasted work costs more than waiting.
- **A second builder needs its own checkout.** `isolation: worktree` is refused here (the session path `deep-megajam` and the folder `Deep-MegaJam` differ in case). Make one by hand: `git worktree add .wt/<name> -b <branch> main`, tell the agent to work only under that path and to push its branch, then merge the branch into `main` yourself.
