# DEEP (Epic AI MegaJam 2026)

Two-person jam team, Quinn and Colby, plus Claude agents. The build is due **Sunday Nov 15 2026 at 8:59 PM EST**.

## Memory

Claude's memory for this project is the Obsidian vault `deep-claude-brain/`.

- Start with `deep-claude-brain/00 - Start Here.md`, then `deep-claude-brain/06 - Tasks/Board.md`.
- Open any other note only when the task touches it. Do not read the whole vault.
- Write back what you learn: decisions to `05 - Log/Decision Log.md`, events to `05 - Log/Devlog.md`, task status to `06 - Tasks/Board.md`.

The Google Doc linked from `02 - Design/GDD.md` holds the game design and the to-do list for the humans. It is not a copy of this memory.

## Dev log

**Keep the dev log. It is a prize entry (Glass Box), not paperwork.** Append to `deep-claude-brain/05 - Log/Devlog.md` while you work, not at the end: decisions, failures, things cut, first-times, test results and screenshot paths. Follow the entry format at the top of that note.

## Standing rules

- Quinn said go at about 9:12 PM Nov 14: code is allowed. Build what `06 - Tasks/Board.md` lists, in its order, and nothing from the Cut list in the GDD.
- The Unreal project is `deep/Deep.uproject` (UE 5.9 MegaJam Preview, C++ module `Deep`). Quinn created it. Do not restructure it.
- Build, test and package only through `tools/ue.ps1`. Only one agent builds or tests in the main checkout at a time.
- Agents create no binary assets. Text data goes in `deep/Content/Data`. Never touch a map a human has open.
- Never put secrets, tokens or partner perk codes in the repo or the vault.
- **Commit and push are allowed** (Quinn, Nov 14, 9:35 PM: "full permission to commit and push"). Commit at each green checkpoint, on `main`, and push. Never commit a red build. Write messages a dev log reader can follow: what changed and why. Check for secrets before every push.
- A task is done only when its validation gates pass and the evidence is in the report. See `03 - Research/Validation Loop.md`.
- Ask before anything irreversible: engine version changes, deleting human work, sharing or publishing.
- The main session is the orchestrator: it plans, dispatches, checks evidence and writes no game code. Its tick procedure and dispatch rules are in `.claude/loop.md`.
- Subagents run on the cheapest model that will pass the gate first time, and never above **Opus 5.5**: `opus` for critical-path gameplay code, `sonnet` for scripts, writing, playtests and the dev log, `haiku` for running a command and summarising it. Quinn asked for this on Nov 14. Set effort on every dispatch: `low` for haiku runners, `medium` for sonnet routine work, `high` for opus critical-path code. Start agents fresh with a tight brief and one vault link. Do not fork a long session.
- Quinn wants to be challenged when a plan hurts the team's chances. Say so, with the reason.
