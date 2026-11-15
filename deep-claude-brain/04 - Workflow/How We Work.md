---
updated: 2026-11-14 20:55 EST
---
# How We Work

Two people and a set of Claude agents share one project until Sunday 8:59 PM EST. This note is the working agreement. The split of work is **proposed** until Quinn and Colby confirm it in the Google Doc.

## Who does what (proposed)

| Who | Owns | Notes |
|---|---|---|
| Quinn | The Unreal project and the repo, engine and scope decisions, directing the Claude agents, player feel, the final package and the submission. | Says when coding starts. |
| Colby | To be confirmed. Claude's guess: level blockout and dressing, the underwater look, audio, generated 3D assets, the dev log, the gameplay video and the itch.io page. | The first plan had Colby running the dev log. Whether he codes is not known yet. See [[Teammate Access]]. |
| Claude orchestrator | The main Claude session. Reads [[Board]], hands tasks to subagents, checks their evidence before accepting work, and keeps this vault current. | One orchestrator at a time. |
| Claude subagents | systems-engineer, editor-driver, playtester, build-sheriff, director-writer, devlog-scribe. | Roles and limits are in [[Agent Orchestration]]. |

The ranked to-do list and the final Quinn and Colby split live in the Google Doc. [[Board]] is Claude's working copy of it.

## Where each kind of truth lives

| Place | Holds | Who edits |
|---|---|---|
| Google Doc (link in [[GDD]]) | The game design doc and the to-do list for humans. | Quinn and Colby. Claude can read it. Editing it in place needs the Google Docs connector, see [[Teammate Access]]. |
| This vault | Claude's memory: research, decisions, the dev log, the task board and the agent-readable copy of the design. | Claude, and any human who wants to. |
| The repo | Code, config and assets. The Unreal project is in `deep/`. | Everyone, under the rules below. |

The Google Doc does not show Claude's memory, and the vault does not replace the Google Doc. When the two disagree about design, the Google Doc wins and [[GDD]] gets updated. See D-001 in [[Decision Log]].

## Standing rules

1. **No game code and no project scaffolding until Quinn says go.** Research, notes and planning are fine. See D-002 in [[Decision Log]].
2. **Design for change.** The idea is still moving, so the code shape proposed in D-010 is:
   - Numbers a designer might change live in config, not in code.
   - Game rules are pure functions with tests. They take inputs and return results, with no world, no actors and no globals.
   - Actors and components stay thin: gather inputs, call a rule, apply the result.
   - Nothing may block co-op later. No logic that assumes exactly one player, and no global object holding player state.
3. **One agent drives the editor at a time.** The editor's MCP server runs one tool call at a time. See [[Unreal MCP in UE 5.9]].
4. **Say what was verified.** Notes state what was tested on this machine and what was only read in docs.

## Definition of done

A task is done when all three are true:

- The gates in [[Validation Loop]] that apply to it pass (T0 Compile through T5 Human feel).
- The evidence is attached: test output, a screenshot, or a log excerpt. "It should work" is not evidence.
- [[Board]] and [[Devlog]] are updated.

An agent that cannot get a gate to pass reports that, with the output. It does not mark the task done.

## Asset ownership

Unreal assets are binary and cannot be merged. Two people saving the same file means one of them loses work.

- One writer per binary asset at a time.
- Humans own the maps. Agents do not save a map a human has open.
- Agents save only under an agreed folder. The folder is not chosen yet. Pick it before the first agent touches content.
- Anything an agent builds for testing goes in its own map, never in a human's map.

## Git habits

State at 8:55 PM on Nov 14: no commits yet, and no `.gitignore` or `.gitattributes`. Both are needed before the first commit.

- Small commits with clear messages. The dev log is drafted from them.
- Never commit `Saved/`, `Intermediate/` or `DerivedDataCache/`.
- Git LFS for `.uasset` and `.umap`.
- Claude commits and pushes only when asked.
- Pull before starting work and push when a task is done, so the other person and their Claude see the same vault.

## Secrets

- No API keys, account tokens or partner perk codes in the repo or in this vault. The repo may be public.
- No API keys inside the shipped build.
- Keys that agents need stay in environment variables or an ignored local file.

See D-003 in [[Decision Log]].

## When Claude stops and asks

Claude works without asking on research, notes and tasks already on [[Board]]. It stops and asks before:

- Anything that cannot be undone.
- Changing the engine version or converting the project.
- Deleting or overwriting work a human made.
- Publishing or sharing anything outside the team.

## How to reach Claude

- Leave a dated line in [[Inbox]].
- Write in the Google Doc. Claude reads it at the start of each work block.
- Talk to a Claude session directly. The options for Colby are in [[Teammate Access]].
