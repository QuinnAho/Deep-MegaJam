---
updated: 2026-11-14 21:00 EST
---
# DEEP: Start Here

This vault is Claude's memory for the DEEP jam project. Every Claude session (Quinn's, Colby's, any sub-agent) reads this note first and writes back what it learns. Humans can read and edit anything here in Obsidian.

**Keep it light.** Read this note and [[Board]]. Open another note only when the task touches it. Do not read the whole vault into context.

## Status right now

| | |
|---|---|
| Jam | 2026 Epic AI MegaJam. Theme: **DIRECTOR'S CUT**. See [[Jam Brief]]. |
| Deadline | **Sunday Nov 15 2026, 8:59 PM EST** (5:59 PM PST). Late entries are disqualified. |
| Team | Quinn and Colby (brothers), plus Claude agents. |
| Game | DEEP: first-person tethered salvage dive, run by a corporate submarine AI called the Director. See [[GDD]]. |
| Phase | **Building.** Quinn said go at about 9:12 PM Nov 14. Agents are working through [[Board]] in order. Design is [[GDD]] v3. |
| Project | `deep/Deep.uproject` inside this repo. UE 5.9 MegaJam Preview. Quinn added a C++ module, `Deep`, at 8:55 PM Nov 14. No content yet. All Toolsets is not enabled yet. |
| Git repo | `C:\Repositories\Deep-MegaJam`, origin `github.com/QuinnAho/Deep-MegaJam`. No commits yet. |
| Design and to-dos | The Google Doc linked from [[GDD]]. Humans edit it. It does not mirror this vault. |

## Blocking decisions

Open in [[Decision Log]]. Nothing gets built until the first two are answered.

1. **Engine: stay on 5.9 Preview or restart on 5.8?** Assets cannot move from 5.9 back to 5.8. See [[Engine Choice - 5.9 vs 5.8]].
2. **Solo-playable jam build with co-op after the jam?** Claude recommends yes.
3. Student track eligibility, team name, and whether the Director's voice uses OpenAI text-to-speech.

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
