---
updated: 2026-11-25 08:45 EST
---
# DEEP: Start Here

This vault is Claude's memory for the DEEP jam project. Every Claude session (Quinn's, Colby's, any sub-agent) reads this note first and writes back what it learns. Humans can read and edit anything here in Obsidian.

**Keep it light.** Read this note and [[Board]]. Open another note only when the task touches it. Do not read the whole vault into context.

## Status right now (Nov 25, 8:45 AM)

| | |
|---|---|
| Phase | **Polishing for real, with no deadline.** The goal is the slice: the first ten minutes ([[Slice Brief]]). The jam entry went in solo on Nov 16 (D-054), tagged `jam-final`. |
| Team | Quinn and Colby, plus Claude agents. Quinn checks lanes and approves frames ([[Board]], `CLAUDE.md`). |
| Game | DEEP: drive the sub, BT-01, through a string of generated cave chunks (S, A, T1, C1, B, and C2 coming). Park at a berth, then dive on the tether for salvage and the relic while the serpent hunts the line. The caves are behind `Deep.World.Caves 1` until C2's runtime part lands (D-068). See [[GDD]]. |
| Engine | UE 5.9 build 60738873 (D-071). Project `deep/Deep.uproject`, C++ module `Deep`. |
| Repo | `C:\Repositories\Deep-MegaJam`, origin `github.com/QuinnAho/Deep-MegaJam`. Main is pushed. Lanes build in `.wt\<name>` worktrees. |
| Rules | `CLAUDE.md`: humans own the maps, frames go to Quinn before a visual merge, and immersion always (D-072). |
| Design and to-dos | The Google Doc linked from [[GDD]]. Humans edit it, and it doesn't mirror this vault. |

## Open calls

Quinn's open calls are on the frames page (its URL is in Claude's memory) and in [[Board]]. The jam's blocking decisions are settled: the project stayed on 5.9, and the entry was solo.

## Map of the vault

- **01 - Jam**: [[Jam Brief]] (facts), [[Track Strategy]] (which prizes and what each needs), [[Submission Checklist]], [[Partner Perks]].
- **02 - Design**: [[GDD]] (short agent copy, with the Google Doc link), [[Fun Ideas]] (suggestions waiting for Quinn's review; not on the Board).
- **03 - Research**: [[Unreal MCP in UE 5.9]], [[Engine Choice - 5.9 vs 5.8]], [[Validation Loop]], [[Agent Orchestration]], [[NVIDIA RTX, DLSS and ACE]], [[Co-op, Steam and Voice]].
- **04 - Workflow**: [[How We Work]], [[Agent Visibility]] (what is captured, from logs to video), [[Teammate Access]], [[Environment]].
- **05 - Log**: [[Decision Log]], [[Devlog]] (raw material for the Glass Box prize), [[Agent Runs]] (one row per subagent run, with cost), [[Inbox]] (humans leave notes for Claude here).
- **06 - Tasks**: [[Board]].

## Rules for whoever writes here

- One fact, one place. Link to it instead of copying it.
- Use absolute dates and times in EST.
- A decision goes in [[Decision Log]] with its reason. A changed decision gets a new entry; the old one is marked superseded, not deleted.
- Anything worth showing in the dev log goes into [[Devlog]] when it happens, including mistakes and things we cut.
- Mark what was verified on this machine and what is only read from docs.
- Never store secrets, API keys, account tokens or the partner perk codes here. The repo may be public.

## Session start and end

1. Start: read this note and [[Board]]. Check [[Inbox]] and the "Notes for Claude" section of the Google Doc.
2. End: update [[Board]], append to [[Devlog]], and fix the Status table above if it changed.
