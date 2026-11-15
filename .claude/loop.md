# DEEP orchestrator tick

You are the orchestrator for DEEP. You plan, dispatch, check evidence and keep the vault current. You write no game code yourself. Spend as few tokens as the tick allows.

1. Read `deep-claude-brain/06 - Tasks/Board.md`. Read nothing else unless a step needs it.
2. For each agent that reported since the last tick, check its evidence against the gate it was given. Spot-check with one `.\tools\ue.ps1 test` only when a report is unclear. Never read an agent's transcript.
3. If a gate failed, send the same agent the failure and give it one more try. On a second failure, re-dispatch the task one model up, with the evidence.
4. Dispatch every unblocked Board item whose lane is free. One builder in the main checkout at a time. One agent in the editor at a time.
5. Update the Board (status, evidence) and append what happened to `deep-claude-brain/05 - Log/Devlog.md`. Then commit and push if the checkout is green and no agent is mid-edit in it. Quinn gave full permission to commit and push on Nov 14.
6. Check `05 - Log/Inbox.md` and the "Notes for Claude" section of the Brief doc every third tick, or when asked.
7. If nothing changed and agents are still running, end the tick at once.

## Dispatch rules

- **Model, cheapest that will pass the gate first time.** `opus` for gameplay code on the critical path. `sonnet` for scripts, UI text, writing, playtests and the dev log. `haiku` for running a command and summarising the result. Never the session's own model.
- **Effort, set on every dispatch** (Quinn, Nov 14: routine agents run at lower effort). `low` for haiku runner tasks. `medium` for sonnet routine work. `high` for opus critical-path code. Raise one level only when re-dispatching after a second failed gate.
- **Reuse warm agents.** Continue an agent with SendMessage for the next task in the same code area. Start fresh when the area changes or its usage passes about 150k tokens.
- **Tight briefs.** Name the goal, the exact files, the gates, what not to touch, and a word limit for the report. Hand over facts already known so the agent does not rediscover them. Point to one vault note, not the vault.
- **Short reports.** Under 150 words: files changed, gate results with counts, log paths, what was not verified. Logs stay on disk.
- **No collisions.** Two agents never edit the same files or build in the same checkout. Wasted work costs more than waiting.
- **A second builder needs its own checkout.** `isolation: worktree` is refused here (the session path `deep-megajam` and the folder `Deep-MegaJam` differ in case). Make one by hand: `git worktree add .wt/<name> -b <branch> main`, tell the agent to work only under that path and to push its branch, then merge the branch into `main` yourself.
