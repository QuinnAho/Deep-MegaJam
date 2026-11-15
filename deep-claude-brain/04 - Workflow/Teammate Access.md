---
updated: 2026-11-14 20:55 EST
verified: Options read from Epic's and Claude Code's documentation on Nov 14 2026. None of them has been set up or tested yet.
---
# Teammate Access

Quinn asked for a way for Colby to talk to Claude too.

There is no single Claude to share. Each session starts fresh and becomes "the same Claude" by loading the same context: the repo's `CLAUDE.md` and this vault. So the goal is that any session Colby opens reads the same notes and writes back to them.

## Options

| # | Option | What it needs | What it gives |
|---|---|---|---|
| 1 | **Claude Code on Colby's machine**, in his own clone of the repo. | His own Claude account, Claude Code installed, and access to the GitHub repo. | A full session with the same context. His session and Quinn's stay in sync by committing and pulling the vault. |
| 2 | **Unreal Assistant panel** in the UE 5.9 editor, running Claude Code. | His own Claude account. Node.js. The Claude Code CLI signed in with `claude login`. The adapter: `npm install -g @agentclientprotocol/claude-agent-acp`. Restart the editor, then Editor Preferences > General > Unreal Assistant > External Agent: choose Claude Code and enable Use External Agent. | Chat inside the editor, with the editor's tools. Use **Auto** or **Accept edits**, because Manual mode is known to hang. Only exists in 5.9. See [[Unreal MCP in UE 5.9]]. |
| 3 | **The Google Doc as a mailbox.** Colby writes under a "Notes for Claude" heading. | Nothing new for reading. | Claude reads the doc at the start of each work block and acts on it. Writing a reply into that doc needs the Google Docs connector turned on in claude.ai connector settings. Today Claude can read Drive files and create new docs, but cannot edit an existing one. |
| 4 | **[[Inbox]]** in this vault. | The repo, and Obsidian or any text editor. | A dated note that Claude picks up at the start of each work block and answers in place. |
| 5 | **Remote Control.** | Quinn's own account on a phone or browser. | Lets Quinn reach his own session away from the laptop. It is tied to one account and is not a way to share a session with another person. |
| 6 | **Claude in Slack.** | Not researched. | It exists. Nothing more is known here. |

Do not share one Claude account between two people. Each person signs in as themselves.

## Recommendation

- **Option 3 in every case.** It costs nothing and works today for reading. Turn on the Google Docs connector if Colby wants answers written back into the doc.
- **Option 1 or 2 if Colby has a Claude account.** Option 2 if he works mostly in the editor, option 1 if he works in code.

## Things to know before setting it up

- Cross-session messaging only links one person's own sessions. Colby's Claude and Quinn's Claude coordinate through this vault, so both sides pull before working and push after.
- Each editor runs its own MCP server. Colby's Claude drives Colby's editor and does not collide with Quinn's. Binary assets can still collide, so the ownership rules in [[How We Work]] apply.
- Option 2 depends on the engine decision. If the project moves to 5.8, the panel cannot run Claude Code there. See [[Engine Choice - 5.9 vs 5.8]].

## What Claude still needs to know

1. Does Colby have a Claude account, and is Claude Code installed on his machine?
2. Is Colby coding, or doing art, design and the dev log?
