---
updated: 2026-11-14 20:55 EST
verified: Mechanics read from the Claude Code docs on Nov 14 2026 and from `claude --help` for v2.1.325 on this machine. None of it has been exercised on this project yet.
---
# Agent Orchestration

How one long-running Claude session directs specialised agents for the jam. The gates those agents must pass are in [[Validation Loop]].

## Status

**None of this is built yet.** No agent files, no `loop.md` and no hooks exist. They get created when Quinn says go.

## The mechanisms available

- **Subagents.** Workers inside one session that return a summary. Started from the main conversation they run in the background by default. Up to 20 at once, nesting three layers deep. Custom definitions live in `.claude/agents/`. `isolation: worktree` gives one its own checkout.
- **Background sessions and agent view.** `claude --bg "prompt"` starts one and `claude agents` lists and monitors them. Research preview. A supervisor daemon runs them, so they survive a closed terminal and sleep, but not a reboot. Each moves into its own worktree before editing unless `worktree.bgIsolation` is `"none"`. When finished they commit without asking and push a branch if a remote exists. They never push main.
- **Dynamic workflows.** Triggered by the word `ultracode` in a prompt, or `/effort ultracode` for the whole session. Claude writes a script that fans out subagents and cross-checks their results, and only the final answer enters context. 16 concurrent agents by default. No mid-run input. A run does not continue if the session is backgrounded.
- **Agent teams.** Experimental and off by default. Teammates message each other directly. There is no worktree isolation, so the files must be partitioned by hand.
- **`/loop`.** A fixed interval (`/loop 5m ...`) or self-paced, where Claude picks a delay from 1 minute to 1 hour. It fires only while the session is idle. Recurring tasks expire after 7 days. A self-paced loop is not restored on resume. A bare `/loop` runs `.claude/loop.md` in place of the default prompt.
- **Worktrees.** Created at `.claude/worktrees/<name>` on branch `worktree-<name>`. `worktree.baseRef` is `fresh` (the remote default branch) or `head`; with no remote it falls back to local HEAD. With a global Git LFS install a worktree materialises every asset. `.worktreeinclude` copies gitignored files in. `worktree.sparsePaths` exists but its exact behaviour is not confirmed.
- **Hooks.** `Stop`, `SubagentStop` and `TaskCompleted` hooks can block completion with exit code 2. That is how "tests must pass" becomes a rule the harness enforces.

## What we will use for this jam

- **One orchestrator session** at the repo root (`C:\Repositories\deep-megajam`).
- **A self-paced `/loop`** whose tick procedure lives in `.claude/loop.md`.
- **Specialised subagents** defined in `.claude/agents/`.
- **[[Board]] as the queue.**
- **Workflows only for wide fan-out jobs**, such as writing tests across every rule or an audit before feature freeze.
- **No agent teams.** They are experimental and give no file isolation.
- **Background sessions are optional**, for long isolated code tasks.

Why: it has the fewest moving parts, the plan lives in one place, and the editor can only serve one agent at a time anyway.

## The roster

The orchestrator is the main session. It plans, dispatches, checks evidence and merges. It writes no game code itself.

| Agent | Job | Lane | Tools it may use | Gate | Never |
|---|---|---|---|---|---|
| **systems-engineer** | C++ rules and thin components, written test-first. | Code | File tools, build and test commands. | T0 + T1 | Touches Content or the editor. |
| **editor-driver** | The only builder inside the live editor: levels, Blueprints, materials, Niagara, data assets. | Editor, through MCP | `unreal-mcp` level, asset, Blueprint, material, Niagara and data tools. | T2 plus screenshots | Edits a map a human has open. |
| **playtester** | Plays the game and reports bugs with evidence. | Editor, through MCP | StartPIE, ApplyInput, CallCheat, logs, viewport capture. | T3 | Edits anything. |
| **build-sheriff** | Keeps main green, bisects red builds, packages. | Build | Build, test and package commands, git history. | T0, T1, T4 | Adds features. |
| **director-writer** | Writes the Director's lines as data, generates voice files, imports audio, adds subtitles. | Content pipeline | File tools, a text-to-speech service, `SoundWaveToolset.ImportFile`. | Lines load and play in PIE | Changes gameplay rules. |
| **devlog-scribe** | Turns [[Devlog]], commits and screenshots into posts for the Glass Box prize. | Docs | File tools, git log. | Every claim traces to the log | Invents events. |

## Unreal-specific concurrency rules

- **Exactly one agent holds the editor at a time**: editor-driver or playtester. The orchestrator hands it out. director-writer's audio imports go through the same lock.
- **Code agents can run two or three in parallel**, but UBT serialises builds, so more than that only queues.
- **One writer per binary asset.** Maps, Blueprints and materials cannot be merged.
- **Agents save assets only under an agreed folder** (proposed `Content/Deep/Agent`). Humans own the maps.

## The orchestrator tick

1. Read [[00 - Start Here]], [[Board]], [[Inbox]] and the Google Doc.
2. Pick the highest-priority unblocked task.
3. Dispatch it to the right agent with a brief.
4. When it returns, check the evidence against the gates in [[Validation Loop]].
5. Merge it, or bounce it back with the failure.
6. Update [[Board]] and [[Devlog]].
7. Schedule the next tick.

## Brief template for a subagent

```text
Goal:        one sentence, the outcome wanted.
Context:     links to the GDD section and the notes that apply.
Constraints: its lane, the folders it may write, what it must not touch.
Done means:  the gates that must pass (T0 to T4).
Return:      what changed, the evidence, and what was not verified.
```

## Context and cost

- The orchestrator delegates anything with long output (builds, test logs, searches) and keeps only the summary.
- State lives in this vault, not in the conversation, so a fresh session can pick the work up.
- Parallel agents multiply usage. Ten agents use quota about ten times as fast as one, and each background session draws quota independently.
- A subagent uses the parent's model unless its definition names another.
- **Measured on Nov 14:** three subagents forked from the long research session each used about 470,000 tokens to write two to five notes, because a fork carries the whole conversation. Start fresh agents instead and brief them with links to vault notes.

## Sources

- https://code.claude.com/docs/en/agents
- https://code.claude.com/docs/en/agent-view
- https://code.claude.com/docs/en/workflows
- https://code.claude.com/docs/en/agent-teams
- https://code.claude.com/docs/en/sub-agents
- https://code.claude.com/docs/en/worktrees
- https://code.claude.com/docs/en/scheduled-tasks
