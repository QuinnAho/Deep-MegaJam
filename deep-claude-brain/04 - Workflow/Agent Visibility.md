---
updated: 2026-11-14 21:45 EST
---
# Agent Visibility

Quinn asked for the agents' work to be visible through the whole process, from logs to video. Two audiences: the team, who need to see what is happening now, and the Glass Box judges, who score how the game was built.

## What is captured, and where

| Layer | What | Where | Captured by |
|---|---|---|---|
| Who did what | One row per agent run: role, model, effort, task, tokens, tool calls, duration, result. | [[Agent Runs]] | Orchestrator, when each agent reports |
| What is happening now | Running agents and the queue. | [[Board]], "Running now" | Orchestrator |
| Why | Decisions with reasons, and what was cut. | [[Decision Log]], the cut list in [[Devlog]] | Orchestrator |
| The story | Timestamped entries, including mistakes. | [[Devlog]] | Every agent appends its own |
| Proof | Build and test output, test reports. | `deep\Saved\Harness\` (not committed) | `tools\ue.ps1` |
| History | One commit per green checkpoint, written to be read. | `git log` | Each agent, when its gates pass |
| What it looks like | A screenshot of the running game at each milestone. | `devlog\media\` (committed) | `tools\ue.ps1 shot`, once the game supports it |
| Agents playing | Recordings of AI playtests. | `devlog\media\` | The playtester, through Unreal MCP viewport recording |
| Full transcripts | Every agent's complete session. | `%USERPROFILE%\.claude\projects\` on Quinn's PC | Claude Code, automatically |

## What only a human can capture

These cannot be recreated later. Use Win+Alt+R (Xbox Game Bar) or OBS.

- [ ] A screen recording of the orchestrator handing out work, with several agents running.
- [ ] The editor being driven by an agent through MCP: actors appearing with no hands on the mouse.
- [ ] The first time a human plays the grey box, with the reaction.
- [ ] The final 30 to 60 second gameplay video. A human should play this one.

## How it becomes the dev log

On Sunday the devlog-scribe turns the layers above into the published dev log: the timeline from [[Devlog]], the org chart of roles, the table of runs with cost per feature from [[Agent Runs]], the cut list, a screenshot from each milestone, and a clip of an agent playing the game. Every claim in it must trace to one of these sources.

## Ideas, not yet decided

- A **Director's commentary mode** in the game itself: the dev log told by the Director while you play. See [[Fun Ideas]].
- The **credits** list each agent role beside the humans, with real numbers from [[Agent Runs]].
- A one-page **dashboard** generated from [[Agent Runs]] and the commit history, linked from the itch.io page.
- **Hooks** that log every subagent start and stop automatically. Not set up: a broken hook can stall a session, and the manual table already works.

## Rules

- Never publish a transcript, a log or a screenshot without checking it for keys, account names and the perk codes.
- Numbers in the dev log come from [[Agent Runs]] or `git`, never from memory.
- Clock times come from tool output, not from estimates. Claude's estimates ran about eight minutes fast on Nov 14 and had to be corrected.
