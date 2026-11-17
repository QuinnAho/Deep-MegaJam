---
updated: 2026-11-16 01:45 EST
---
# Co-op: Steam and Voice

Checked Nov 16, 2026, by web search and page fetches. Quinn decided on two players (D-057): one drives the sub, one dives on the tether. He wants "some plugin that allows easy steam support proximity chat". Nothing was installed or tested; every "5.9" answer below is what the sources say, and most say nothing.

**State of the project:** no online subsystem is configured. There are no Steam, EOS or voice entries in `deep/Deep.uproject` or `deep/Config/*.ini`. The code already replicates the sub's depth, offset and yaw and the world-still flag, so a listen server (one player hosts, the other joins) is the likely model.

**The main finding:** no source I found names UE 5.9 for any option. The 5.9 [release notes](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-9-release-notes) (last updated Nov 9, 2026) say nothing on Online Subsystem, Steam or voice. 5.9 support is unconfirmed everywhere, so a spike has to prove it.

## 1. Sessions on Steam

| Option | Price, licence | Newest UE named | Blueprint or C++ | Gives | Install |
|---|---|---|---|---|---|
| Engine Online Subsystem Steam + Steam Sockets | Free, part of the engine | Ships with the engine, so 5.9 by definition | C++ and config; sessions are not exposed to Blueprint beyond the basic Create/Find/Join nodes | Sessions and lobbies, join in progress, Steam overlay invites; friends and invites need C++ or a plugin | Engine plugin, turned on in the project; needs `DefaultEngine.ini` entries and a Steam app ID |
| [Advanced Sessions (mordentral)](https://github.com/mordentral/AdvancedSessionsPlugin) | Free, MIT | Repo calls itself the UE4 plugin and shows only `master`; no 5.8 or 5.9 mention. Forum thread shows users on 5.6 in [Aug 2025](https://forums.unrealengine.com/t/advanced-sessions-plugin/30020?page=168) | Blueprint nodes over C++ | Extra session nodes and Steam friend, invite and avatar nodes (AdvancedSteamSessions module); it sits on top of the engine's OSS Steam | Project plugin; forum posters say to rebuild it for each engine version ([thread](https://forums.unrealengine.com/t/advanced-sessions-plugin/30020?page=161)) |
| [Steam Integration Kit (Betide)](https://github.com/betidestudio/SteamIntegrationKit) | Marketplace/Fab version paid (price not found); GitHub source MIT, compile yourself | "5.2 and above"; no 5.8 or 5.9 mention; last repo update Jan 2, 2026 | Blueprint and C++ | Steam interfaces: friends, matchmaking, parties, networking; [docs](https://sik.betide.studio) list Lobbies, Direct P2P Session, Friends List with invites, Voice Chat | Fab: pre-compiled project plugin. GitHub: compile; set `bUseEngineSteam` in the Build.cs so it uses the engine's Steam plugin |
| [EOS Integration Kit (Betide)](https://github.com/betidestudio/EOSIntegrationKit) | GitHub MIT; Marketplace version paid (price not found) | No version stated; repo updated Aug 6, 2026 | Blueprint and C++ | Lobbies, matchmaking sessions, voice chat (a separate EOS Voice Integration Kit with positional voice) | Plugin; "check the docs" for steps |
| Other credible options | PloxTools Steam and others came up in search, but I found no 5.9 evidence and did not assess them | | | | |

Notes:

- The Steam Integration Kit had a [security report](https://80.lv/articles/steam-integration-kit-plug-in-may-be-exposing-your-steam-password-to-players) (a Steam password exposed in packaged builds). Betide said it does not store credentials in `DefaultEngine.ini`. Date not checked; re-read before buying.
- Epic's [Online Services overview](https://dev.epicgames.com/documentation/unreal-engine/overview-of-online-services-in-unreal-engine) steers new long-lived projects to Online Services over the older Online Subsystem. Whether OSS Steam is deprecated in 5.9 is not confirmed here. The plugins above all build on OSS.

## 2. Proximity voice

| Option | Price, free tier | 5.9 | 3D voice | Per-player effects at runtime | Over Steam sessions |
|---|---|---|---|---|---|
| Engine VOIP: `UVOIPTalker`, attenuation, source effect chain, voice from the online subsystem | Free | Engine feature; no 5.9 note found | Yes: `FVoiceSettings` has a component to attach to, an attenuation setting and a [source effect chain](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/USoundEffectSourcePresetChain) | Chain entries can be added or bypassed from Blueprint ([Add Source Effect to Preset Chain](https://dev.epicgames.com/documentation/unreal-engine/BlueprintAPI/Audio/Effects/AddSourceEffecttoPresetChain), [Set Bypass](https://dev.epicgames.com/documentation/unreal-engine/BlueprintAPI/Audio/Effects/SetBypassSourceEffectChainEntry)); these affect only the chain instance. Forum users report runtime setting swaps [not taking effect](https://forums.unrealengine.com/t/voip-talker-change-setting-at-runtime/650593) | Yes: Steam's voice runs under the OSS. A dev reports a working proximity setup on 5.4 with Advanced Sessions at [app 480](https://forums.unrealengine.com/t/voiptalker-troublshooting-on-steamappid-480-missing-words/2662021), with a startup-order trap |
| EOS Voice (via [EOS Integration Kit](https://github.com/betidestudio/EOSIntegrationKit)) | Epic's RTC cost not found | Not named | Kit says positional voice; Epic spatial support not confirmed by me | Not found | Needs EOS lobbies; a forum report of a Steam client [crashing](https://forums.unrealengine.com/t/steam-client-crashes-when-joining-eos-voice-chat-with-epic-games-store-client/2656723) when joining EOS voice |
| [ODIN (4Players)](https://www.fab.com/listings/79d0a208-78e8-4bd7-9214-36f6cc190da2) | Plugin free. Voice free up to 25 peak concurrent users (PCU); then [Starter EUR 49/month (500 PCU, ODIN brand placement) or pay-as-you-go EUR 0.29 per PCU per month](https://odin.4players.io/pricing/) | Fab page lists no engine versions or update date; no 5.9 mention | Yes: spatial audio, occlusion, up to 64 3D channels per room | Fab page: custom audio effects can be inserted into encoder and decoder pipelines. Per-player runtime switching not confirmed | Yes in principle: it runs on its own servers, independent of the session service; room join is by token |
| Vivox | Unity Gaming Services; reported free up to 5,000 PCU ([comparison](https://tech-insider.org/vivox-vs-agora-vs-azure-acs-voice-chat-2026/), Unity [FAQ](https://support.unity.com/hc/en-us/articles/31045802890260) dated Nov 2024; check the current page) | Not found; I found no current Unreal plugin | Positional channels reported for Vivox generally; Unreal side unverified | Not found | Independent of sessions |
| Fab proximity-chat plugins | Not assessed; no named plugin found | | | | |

Engine VOIP caveats from forums: attenuation sometimes does not reach clients or [late joiners](https://forums.unrealengine.com/t/voip-attenuation-not-working-for-new-join-in-progress-players/467430), and several posters [bought a plugin instead](https://forums.unrealengine.com/t/voip-talker-not-attenuating-for-clients/2266357). Those are anecdotes, not engine bugs confirmed by Epic.

## 3. Is the tether twist feasible?

The idea: clipped on, both hear each other clearly like a radio (band-pass), at any distance. Unclipped or cut, they hear each other only up close and muffled (low-pass, short range).

| Option | Can switch a talker between the two modes at runtime? |
|---|---|
| Engine VOIP | Plausible and the best fit. Two settings differ between modes: attenuation (none versus short range) and the source effect chain (band-pass versus low-pass; Epic has a [filter source effect](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/Synthesis/USourceEffectFilterPreset) and an [EQ one](https://dev.epicgames.com/documentation/unreal-engine/API/Plugins/Synthesis/USourceEffectEQPreset)). The risk is the forum reports that runtime changes to a talker do not always apply. A fallback is two talkers per player (one radio, one water), muting one and unmuting the other. Untested |
| EOS Voice | Unknown. Not enough evidence |
| ODIN | Possible through its custom effects and per-room 3D audio, but the sources do not show a per-player runtime switch. Would need its docs or a test |
| Vivox | Unknown for Unreal |

Conclusion: feasible on paper with the engine's VOIP, unproven until a spike runs it.

## 4. Recommendation

1. Sessions: start with the engine's OSS Steam, Steam Sockets and Advanced Sessions (free, MIT, Blueprint nodes). It adds no cost, so a 5.9 failure costs only time; if it does not build on 5.9, the same source can be rebuilt as a project plugin.
2. Voice: try the engine's VOIP first, with `UVOIPTalker`, attenuation and a source effect chain. It is free, over Steam, and the two tether modes map onto its two settings.
3. Fallback voice: ODIN, if engine VOIP cannot switch modes reliably. Free to 25 PCU, fine for development and testing. Costs only begin when real players arrive.
4. The Betide kits are the paid fallback for sessions (Steam Integration Kit; EOS Integration Kit if voice moves to EOS). Check Quinn's saved plugin first.
5. Spike: one listen server and one client in PIE first (replication, talker setup, the two modes with a fake mic or the editor's voice test). Then two machines on Steam with test app 480: invite, join, voice, clip on and off.
6. Risks: (a) 5.9 support is unconfirmed for every plugin; (b) app 480 is shared, so sessions from other developers may show up, and a real app ID is needed to ship; (c) cost: engine route is free, ODIN is free to 25 PCU, then EUR 49 or more per month; Betide Fab prices not found.
7. The cave work stays first; this follows it.

Quinn may already have a plugin saved on Fab; check its name against this list.
