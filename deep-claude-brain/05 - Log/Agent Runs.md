---
updated: 2026-11-14 21:45 EST
---
# Agent Runs

One row per subagent run. The orchestrator adds the row when the agent reports, using the numbers Claude Code gives it. This is the source for cost and effort figures in the dev log. Times are EST on Nov 14 2026 unless a date is shown, and are approximate to a minute or two.

| # | Start | Role | Model, effort | Task | Tokens | Tool calls | Duration | Result |
|---|---|---|---|---|---|---|---|---|
| 1 to 8 | 8:20 to 8:45 PM | research (web fetch) | small fetch model | Read the jam page, Epic's MCP docs, NVIDIA's DLSS pages, Claude Code docs, Epic forum posts. | about 305k in total | 27 | 12 s to 73 s each | Facts in [[Jam Brief]], [[Unreal MCP in UE 5.9]], [[NVIDIA RTX, DLSS and ACE]], [[Agent Orchestration]]. The rules PDF could not be read. |
| 9 | 8:52 PM | fork of the main session | session model | Write [[Validation Loop]] and [[Agent Orchestration]]. | 466k | 2 | 98 s | Done. |
| 10 | 8:52 PM | fork of the main session | session model | Write the NVIDIA, co-op and environment notes. | 467k | 3 | 116 s | Done. |
| 11 | 8:52 PM | fork of the main session | session model | Write the workflow notes, [[Decision Log]], [[Devlog]], [[Inbox]]. | 479k | 73 | 200 s | Done. |
| 12 | 9:16 PM | build-sheriff | session model | Harness script. | not reported | | 2 min | Stopped: restarted on a cheaper model before it wrote a file. |
| 13 | 9:16 PM | director-writer | session model | The Director's lines. | not reported | | 2 min | Stopped, same reason. |
| 14 | 9:21 PM | build-sheriff | Opus 5.5, inherited | `tools\ue.ps1`, All Toolsets, canary test, prove build and test. | 120k | 44 | 11 min | Green. Found and fixed a missing module registration. |
| 15 | 9:21 PM | director-writer | Opus 5.5, inherited | 138 lines and 24 fee labels as CSV. | 102k | 17 | 8 min | Done. Checked by the orchestrator. |
| 16 | 9:34 PM | systems-engineer | Opus 5.5, high | Rules with tests, then the playable grey box. | | | | Running. |
| 17 | 9:43 PM | build-sheriff | Sonnet 5.5, medium | Prove package, smoke and zip in its own checkout (`.wt\pkg`, branch `sheriff/package`); add the `shot` verb. | 67k | 15 | 10 min | Green, commit `dc33aae`, merged. Package 5 min 12 s, 729 MB; smoke pass; zip 449 MB. First attempt at 9:41 PM was refused: Claude Code's built-in worktree isolation rejects this repo because the session was opened as `deep-megajam` and the folder on disk is `Deep-MegaJam`. A worktree made by hand works. |

| 18 | 9:47 PM | writer | Sonnet 5.5, medium | Rewrite all 138 lines for the Captain after the rename (D-016); file becomes `CaptainLines.csv`. | 61k | 13 | 70 s | Done, commit `5021c7f`. Checked by the orchestrator: same ids and counts, ASCII, 10 lines say "Director's cut", no strong language. |

## What the numbers say so far

- Runs 9 to 11 cost about 1.41 million tokens for ten notes, because a fork carries the whole conversation. Runs 14 and 15 cost 222k for a working pipeline and the game's entire script. Fresh agents with tight briefs are about six times cheaper per run.
- Since run 14 every agent has a named model. Since run 16 every agent has a named effort level.
- Run 15 (Opus, inherited effort) wrote 138 lines in 8 minutes for 102k tokens. Run 18 (Sonnet, medium) rewrote all 138 in 70 seconds for 61k. For writing, the smaller model at medium effort was faster and cheaper with no visible loss.
