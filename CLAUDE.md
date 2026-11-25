# DEEP

DEEP started as Quinn and Colby's entry for the Epic AI MegaJam 2026. The deadline passed on Nov 15 2026 without a submission. Since then it is a game they are polishing for real, with no deadline, built the traditional way: humans build maps and judge the feel, and Claude agents write code and tools. The jam's last build is tagged `jam-final`.

## Memory

Claude's memory for this project is the Obsidian vault `deep-claude-brain/`.

- Start with `deep-claude-brain/00 - Start Here.md`, then `deep-claude-brain/06 - Tasks/Board.md`.
- Open any other note only when the task touches it. Do not read the whole vault.
- Write back what you learn: decisions to `05 - Log/Decision Log.md`, events to `05 - Log/Devlog.md`, task status to `06 - Tasks/Board.md`.

The Google Doc linked from `02 - Design/GDD.md` holds the game design and the to-do list for the humans. It is not a copy of this memory. The slice is planned in `02 - Design/Slice Brief.md`.

## Dev log

Keep it light (Quinn, Nov 15 at about 9:30 PM): a few lines in `05 - Log/Devlog.md` per milestone, covering what changed, what was decided and what failed. No more Agent Runs rows.

## Human in the loop

- Check with Quinn before starting a lane or changing direction. Quinn wants to be challenged when a plan hurts the game: say so, with the reason.
- Nothing visual merges until Quinn has seen frames or a short clip and said OK.
- Ask before anything irreversible: engine version changes, deleting human work, sharing or publishing.

## Maps and assets

- **Humans own the maps.** No agent creates, saves or changes a map unless Quinn says so for that map. Never touch a map a human has open.
- **Agents make assets only in their lane's folder,** `deep/Content/Deep/<Lane>/`, with one branch and one worktree per lane. Assets include materials, particle systems, data assets and Blueprints. Nothing an agent saves goes outside its folder.
- **Third-party packs are read-only.** Each sits in its own folder under `deep/Content/` and is listed in `.gitignore`, so it stays outside git. Each person installs it from Fab at the same path. No agent saves, moves or renames anything inside a pack.
- Text data goes in `deep/Content/Data`.
- **Binary files go through Git LFS** (`.gitattributes`): `.uasset`, `.umap`, source art and audio.
- **Look it up before building an effect or a material.** Read Epic's built-in skill for it (`03 - Research/Polish - Engine Skills and Toolsets.md`), then official documentation or a breakdown. Write the recipe and its sources down, then build it once.
- **Use the editor's tools for real results:** PCG, Geometry Script, Modeling Mode, Blueprints and Data Assets. Don't rebuild them in C++.

## Style and polish

- **One style** (D-030). Everything the player sees and hears follows `02 - Design/Style Guide.md`. A good asset that clashes is left out, and Quinn approves frames.
- **Function first, immersion always** (D-028, loosened by D-072). Fix what does not work at any time. Quinn (Nov 25): "always be thinking about immersion dont be afraid to add polish". Any lane may add polish and immersion: feel, sound, light, camera, and signs of life. It has to meet a professional standard and follow the style guide. No quick patches that a later pass must undo.

## Code, builds and tests

- The Unreal project is `deep/Deep.uproject` (UE 5.9, C++ module `Deep`). Quinn created it. Do not restructure it.
- Build, test and package only through `tools/ue.ps1`. Only one Unreal process runs at a time, shared through the `Global\DeepUnrealSlot` mutex.
- Test adaptively. Agents run only the tests for their own area (`tools\ue.ps1 test -Filter <prefix>`). The full suite runs once per branch when the orchestrator merges it. Playtests and packaging run at milestones. The table is in `.claude/loop.md` under "What to run".
- A task is done only when its checks pass and the evidence is in the report. See `03 - Research/Validation Loop.md`.
- **Commit and push are allowed** (Quinn, Nov 14). Commit at each green checkpoint on `main` and push. Never commit a red build.
  - Write messages a reader can follow: what changed and why.
  - Check for secrets before every push.
  - Never put secrets, tokens or partner perk codes in the repo or the vault.

## Agents

- The main session orchestrates: it plans, dispatches, reviews, merges and keeps the vault. It writes no game code.
- Subagents run on the cheapest model that will pass first time, and never above **Opus 5.5**:

  | Model | Effort | Work |
  |---|---|---|
  | `opus` | `high` | Gameplay code on the critical path |
  | `sonnet` | `medium` | Scripts, research, writing and playtests |
  | `haiku` | `low` | Running a command and summarising it |

- Start each agent fresh, with a tight brief and one vault link. Do not fork a long session.
