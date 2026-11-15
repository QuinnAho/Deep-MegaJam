---
updated: 2026-11-14 20:55 EST
verified: Engine plugin presence checked in the local UE 5.9 install. Third-party plugins were found by web search only. None has been installed or tested.
---
# Co-op, Steam and Voice

Status: **proposed to leave out of the jam build.** Tracked in [[Decision Log]]. Quinn and Colby have not confirmed yet.

## What was asked

Quinn's first notes list:

- "Co-op lethal company style"
- "Research plugins to make this plug and play"
- "proximity chat, steam support"

This note is that research, plus Claude's recommendation.

## Recommendation

Leave co-op, Steam and voice out of the jam build. Build a solo game that a judge can finish alone.

1. **Judges play alone and probably offline.** A game that needs a second player cannot show its loop. The jam page says nothing about multiplayer entries, so assume no one will set up a session.
2. **Networking multiplies the work on every system.** Each feature needs replication. Swimming and tether physics also need client prediction, or they feel wrong for the player who is not the host.
3. **Third-party plugins have no 5.9 Preview builds.** Each one would need compiling from source against a preview engine. "Plug and play" does not hold on a preview engine.
4. **Testing needs two machines and Steam.** Every test is a packaged build on both PCs.
5. **About 24 hours remain.**

## How the jam build keeps the door open

These cost little now and save a rewrite later.

| Practice | Why it helps co-op later |
|---|---|
| Rules as pure functions | The same rules run on a server unchanged. Nothing reads global state. |
| State on actors and components | Depth, cargo, tether length and the Director's meter live where they can be replicated later, not in the HUD. |
| No singletons | Code is handed the diver it acts on. Nothing assumes there is exactly one player. |
| Input separated from intent | Input produces an intent such as swim direction, reel or grab. Gameplay acts on intents. Later the intent is what a client sends to the server. |

## What exists for later

### In the engine install

Verified present in `C:\Program Files\Epic Games\UE_5.9\Engine\Plugins`.

| Plugin | Location | Use |
|---|---|---|
| OnlineSubsystemSteam | `Online\OnlineSubsystemSteam` | Steam sessions, friends, invites |
| SteamSockets | `Runtime\Steam\SteamSockets` | Steam networking transport |
| OnlineSubsystemEOS | `Online\OnlineSubsystemEOS` | Epic Online Services sessions |
| VoiceChat | `Online\VoiceChat\VoiceChat` | Voice chat interface |
| EOSVoiceChat | `Online\VoiceChat\EOSVoiceChat` | Voice through Epic Online Services |

Steam development normally uses app ID 480 until the game has its own.

### Third-party

From search results. Claims are the sellers' own and unverified.

| Option | What it offers | Link |
|---|---|---|
| Advanced Sessions | Free community plugin that exposes sessions to Blueprints. Needs a build per engine version. | (no link checked) |
| PloxTools "Steam Integration & Multiplayer" (Fab) | Hosting, finding and joining games, invites, global and proximity voice, push-to-talk. | https://www.fab.com/listings/a55fc08e-82ec-4332-8bed-dde44fff7847 |
| "Dynamic Voice Chat System (VoIP)" (Fab) | UE 5.5+. Runs over Unreal networking with no external voice service. | https://www.fab.com/listings/78769f70-76a5-4cfd-bac0-1fed7f99381a |
| "Advanced VoIP Voice Chat System" (Fab) | Voice chat plugin. | https://www.fab.com/listings/24a12063-0867-413c-83b2-4dc6e4fb3d4c |
| ODIN by 4Players | SDK with proximity voice, spatialisation and occlusion. | https://odin.4players.io/voice-chat/ |
| Epic community tutorial | Steam proximity and radio voice, free, more setup. | https://forums.unrealengine.com/t/community-tutorial-steam-voice-chat/2312204 |

Check before choosing any of them:

- Which engine versions it supports.
- Whether it works with Steam sessions, EOS sessions, or both.
- The license terms for a commercial release.

## The post-jam design this points to

Asymmetric two-player co-op:

- **Player one is the diver.** Out on the tether, collecting salvage, as in the jam build.
- **Player two is the captain.** Drives the sub, runs sonar and the winch, and holds the cut button.
- **In single-player the Director AI plays the captain.** The jam build is this mode.

It matches the "Two roles: Captain, Diver" split in Quinn's first notes. It also puts the theme in a player's hands: the second player decides when the Director's cut happens.

This is the pitch for the continuation grant. See [[GDD]].
