---
updated: 2026-11-14 20:50 EST
verified: Toolset lists compared from the two engine installs on this machine. DLSS versions read from NVIDIA's pages on Nov 14 2026. Preview status read from Epic's announcement.
---
# Engine Choice: 5.9 Preview vs 5.8

Status: **open**. Tracked in [[Decision Log]]. It has to be decided before any content is made.

## The constraint that drives everything

Unreal cannot open an asset saved by a newer engine version. A map, Blueprint or material saved in 5.9 will not load in 5.8. Going from 5.8 to 5.9 works.

So "develop in 5.9, then move to 5.8" is not possible for binary assets. Only text moves both ways: C++ source, config files, and source files such as PNG, WAV, FBX and CSV.

The project is empty today, so switching now costs nothing. After the first evening of level work it costs that work.

## Side by side

| | UE 5.9 MegaJam Preview | UE 5.8.3 |
|---|---|---|
| Stability | Preview. Epic: "about a week of development and a shorter QA pass than usual", "should be considered as unstable". | Third patch release. Stable. |
| MCP server | Auto-starts. | Present, does not auto-start by default. |
| Agents build levels, Blueprints, materials | Yes, C++ and Python toolsets. | Yes, Python toolsets (actor, asset, blueprint, material, scene). |
| Agents run tests, compile | Yes. | Yes (automation tests, Live Coding compile). |
| **Agents play the game** (press inputs, call cheats, read logs, run console commands) | **Yes, built in**: Input, Cheats, Console, Logs toolsets. | **No.** Those toolsets do not exist in 5.8. We would have to build our own harness. |
| **Agents see the game** (viewport capture, recording) | **Yes**: Scene Capture toolset. | Only a Slate UI screenshot tool. |
| Performance, NavMesh, MetaSound, file tools | Yes. | No. |
| Claude Code inside the editor panel | Yes. | No (0 references to external agents in the 5.8 Assistant source). |
| Fab and Quixel Bridge plugins | Present in the install. | Present in the install. |
| **DLSS plugin (NVIDIA RTX Award)** | **No build.** Newest is 8.9.0 (Oct 16 2026) for UE 5.5 to 5.8. | **Yes**, DLSS 4.5 plugin 8.9.0. |
| Marketplace code plugins (voice, Steam) | No 5.9 builds. Would need compiling from source. | Builds exist. |

## What each choice costs

**Stay on 5.9:** we get the built-in "AI plays and sees the game" loop that Quinn asked for, and Claude Code in the editor for Colby. We accept preview instability and we most likely give up the NVIDIA RTX Award.

**Move to 5.8:** we get stability and the NVIDIA route. We lose the play-and-see toolsets and have to write our own test harness (debug autopilot, state dump, screenshot commands) to get a weaker version of the same loop.

## Claude's recommendation

**Stay on 5.9 Preview**, with three safeguards:

1. Keep gameplay in C++ and config. Those survive an engine change either way. Keep maps simple and few.
2. Commit often and package a build every few hours, starting tonight, so a preview bug is found early.
3. If the NVIDIA award matters to the team, spend a timeboxed 30 minutes on a test **before making content**: download the DLSS 8.9.0 plugin for 5.8, put it in the project's `Plugins` folder, and see whether it compiles against 5.9. If it does, we have both. If it does not, choose then: 5.9 without NVIDIA, or restart on 5.8.

Reasons: the agent validation loop is what lets two people ship more in 24 hours, and it is the story for the Glass Box dev log. The NVIDIA award is one GPU for the single best eligible entry, which is long odds for a one-day build.

Unknowns in this recommendation: the DLSS plugin has not been tested against 5.9, and 5.9's stability on this machine is unproven beyond the editor opening.

## If the answer is 5.8

Recreate the project in 5.8 now. The MCP server needs starting by hand or by flag. The validation design in [[Validation Loop]] still holds, but tier 3 uses our own cheat and state-dump commands driven from automation tests instead of the Input and Scene Capture toolsets.

## Sources

- https://forums.unrealengine.com/t/unreal-engine-5-9-megajam-preview/2852840
- https://developer.nvidia.com/rtx/dlss/get-started
- Local installs: `C:\Program Files\Epic Games\UE_5.8` and `UE_5.9`.
