---
updated: 2026-11-15 02:15 EST
verified: Web searches and page fetches 2026-11-15 01:55 to 02:15 EST. Tripo and Meshy pricing pages returned 403/404, so their terms come from search snippets. Anything marked UNCONFIRMED was not seen on a primary page.
---
# Polish: outside tools

Nothing here was installed or signed up for. Related: [[Partner Perks]], [[Jam Brief]], [[Polish - Sound, Voice, Fonts and References]]. Time left matters more than features: every tool below costs setup minutes that a polish pass may not have.

## 1. Other Unreal MCP servers

Epic's own plugin is already in 5.9 Preview and "a bigger Unreal MCP" is in the 5.9 notes ([5.9 release notes](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-9-release-notes), [Creative Bloq](https://www.creativebloq.com/news/unreal-engine-5-9-divides-game-devs-with-an-ai-heavy-update)). I found **no evidence that any third-party Unreal MCP plugin supports 5.9**. Verdict for the whole group: **skip** (a binary plugin must match the engine minor version, and a preview build is the worst case).

| Tool | Maker, licence, price | Versions | Code plugin? | Adds over official | Verdict |
|---|---|---|---|---|---|
| ChiR24 Unreal_mcp | Community, MIT, free ([repo](https://github.com/ChiR24/Unreal_mcp)) | Lists 5.0 to 5.8, not 5.9 | Yes. Fab build or prebuilt binaries only work for the engine minor version they were built for; source build needs a C++ project (same page) | One gateway tool over 23 internal tools, "400+" actions. Mostly duplicates what we have | Skip |
| StraySpark Unreal MCP Server | Commercial, from $129.99, bundle from $479 ([vendor post](https://www.strayspark.studio/blog/epic-official-mcp-plugin-ue5-8-vs-third-party)) | Aimed at 5.8; 5.9 not mentioned | Not stated | 455+ tools, single-undo batch scripts, graph read-back. Vendor marketing | Skip |
| ivanmurzak Unreal-MCP | Community, open source ([listing](https://mcpservers.org/servers/ivanmurzak/unreal-mcp)) | Listings mention 5.7 | Yes (Fab plugin) | Nothing we lack | Skip |
| wellingfeng ue-mcp-for-all-versions | Community, standalone C++ server over Remote Control HTTP, claims 4.25 to 5.8 ([listing](https://awesome.ecosyste.ms/projects/github.com%2Fwellingfeng%2Fue-mcp-for-all-versions)) | Not 5.9 | No editor plugin | Remote Control API only, which our toolset already covers | Skip |
| Other single-repo plugins (5.5 to 5.7) | Various | Older | Yes | None | Skip |

## 2. 3D generation

Jam partners come first. Their perks and prize rules are on the [jam page](https://itch.io/jam/2026-epic-ai-megajam): Hyper3D Rodin (one year of Rodin Business per winning team member plus a $10,000 cash pool; "Be sure to list Rodin in the 'Tools you used' field"), Tripo (3,000 credits and 7 days of Pro, redeem by Nov 16; winners get Tripo Max or Teams), Meshy (one month Pro, 1,000 credits, credit card required, redeem by Nov 17; winners get a year of Pro).

| Tool | How an agent drives it | Output rights | Notes |
|---|---|---|---|
| **Meshy** | Official open-source MCP: `npx -y @meshy-ai/meshy-mcp-server` with `MESHY_API_KEY`; tools for text to 3D (preview then refine/texture), image to 3D, retexture, rig, animate, convert ([Meshy docs](https://docs.meshy.ai/api/ai.md)). Third-party listing says GLB, FBX, OBJ, USDZ ([Meshy MCP listing](https://glama.ai/mcp/servers/meshy-dev/meshy-mcp-server)) | Free plan: CC BY 4.0, attribution required (credit line "Model created with Meshy"). Paid plans: private, commercial, no attribution. Models made while paid keep paid rights ([Meshy docs](https://docs.meshy.ai/pricing), [help](https://help.meshy.ai/en/articles/9992001-can-i-use-my-generated-assets-for-commercial-projects)) | Rig tool is described as humanoid only by one third-party doc ([3DAI Studio](https://docs.3daistudio.com/processing/rigging)); Meshy's own claim conflicts. Credit costs per task not found. Quality and seconds per model UNCONFIRMED |
| **Tripo** | Official MCP is alpha and only talks to Tripo's Blender add-on ([repo](https://github.com/VAST-AI-Research/tripo-mcp)). Better: Tripo API key, or the Blender route below | Paid plans: commercial. Free plan: sources conflict (no commercial use per Tripo FAQ, CC BY per Tripo blog). Our 7 days of Pro perk counts as paid, UNCONFIRMED whether it covers models made during the trial after it ends. Save the terms page ([Tripo help](https://www.tripo3d.ai/help/privacy-policy/can-i-use-models-commercially), 403 to my fetch) | Its rigger lists serpentine and aquatic creatures (v2.5) ([Tripo rigging](https://developers.tripo3d.com/en/models/rig)). Vendor claim, untested on a long legless body |
| **Hyper3D Rodin** | No official MCP found. Gen-2 API (submit, status, download) and an MIT community skill ([Rodin3D Skills](https://mcpservers.org/zh-TW/servers/deemostech/rodin3d-skills)); also reachable inside the Blender MCP below | Third parties say free-tier downloads are commercial; conflicting. UNCONFIRMED | About 2 to 4 minutes for a high-poly mesh, third-party figure ([Dupple](https://dupple.com/tools/rodin-ai)). Credits are spent at download. Best for the relic hero prop and the only one tied to a cash pool |
| fal | $30 perk credit; hosts Rodin v2.5 ([fal](https://fal.ai/models/fal-ai/hyper3d/rodin/v2.5/api)). No fal MCP found | Per model, UNCONFIRMED | Backup route to Rodin by plain HTTP |

Quality, honestly: hard-surface props (harpoon gun, relic, salvage) are the strong case for every tool. A creature mesh is plausible as a static or rigid-segment mesh. A **swimming serpent needs a rig**, and no source tested a many-segment legless body. Treat any auto-rig as a gamble.

## 3. Blender through MCP

The community Blender MCP (`ahujasid`, MIT, add-on plus `uvx` server) exposes nine tools: `execute_blender_code` (arbitrary Python: modelling, materials, animation, rigging), `look` (viewport screenshots), `get_scene_info`, `generate_3d` (Tripo, Hunyuan3D, Rodin, Meshy), asset search and import (Poly Haven CC0, Sketchfab, Poly Pizza) ([README](https://github.com/ahujasid/blender-mcp)). Needs Blender 3.0+, Python 3.10+, `uv`. Security: the socket has no authentication; run localhost only; `BLENDER_MCP_SAFE_MODE=1` blocks file and network access; telemetry on by default (`DISABLE_TELEMETRY=true`). It has no dedicated FBX tool, but Python can call Blender's own FBX exporter (inference, not tested). Our team has not confirmed Blender is installed.

Realistic for a serpent in two hours? **Partly.** Agents writing Blender Python can build a segmented tube with a head, assign an armature chain and export FBX, and the screenshot tool lets the agent check its work. That is a stylised serpent, not a realistic one, and the swim animation still needs authoring (sine wave on bone chain, doable in a script). Generating a mesh with Tripo or Rodin then rigging it by a script is the same effort plus a quality gamble. Expect an hour to a working placeholder, two to something that looks intended. Do it only if the asset guard-rail (agents create no binary assets) is waived for FBX; flag that to Quinn.

## 4. Sound and voice

| Tool | Access | Output rights | Fit |
|---|---|---|---|
| **ElevenLabs** sound effects and TTS | Official MCP exists; community MIT servers too ([Docker listing](https://hub.docker.com/mcp/server/elevenlabs/overview)). Via fal: $0.002 per second ([fal](https://fal.ai/models/fal-ai/elevenlabs/sound-effects/llms.txt)). Key needed | Free plan is non-commercial only, with a credit to elevenlabs.io when shared (secondary sources: [oakgen](https://oakgen.ai/blog/elevenlabs-free-commercial-use)). Paid from Starter, about $6 a month. Sound effects terms: commercial use "only where your plan permits"; output may only be used combined with other material, never as isolated files or a library ([terms](https://elevenlabs.io/sound-effects-terms)) | Best fit for the seven missing effects. For a jam entry that may be judged commercially, use via fal credit (paid) or a Starter month. Free tier is safe only if the team treats the entry as non-commercial; ask Quinn |
| **fal** audio models | One API key, $30 credit. MMAudio V2 text to audio $0.001 per second; Stable Audio SFX variants about $0.02 per clip ([search listing](https://modelslab.com/sound-effects-api-pricing.md), aggregator, UNCONFIRMED against fal) | Per model, UNCONFIRMED | Gives SFX and TTS without another account. Quality per sound unverified |
| **OpenAI TTS** | API key. Already covered, with its AI-voice disclosure duty | See [[Polish - Sound, Voice, Fonts and References]] | 185 Captain lines: cheapest and fastest voice route |
| **WeLoveIndies** | Partner perk, sound and music | Non-commercial only; commercial needs a licence ([jam page](https://itch.io/jam/2026-epic-ai-megajam)) | Fill ambience and music loops; log each file |

I did not find a verified, no-key, commercial-safe text-to-sound generator. Freesound CC0 and Sonniss remain the safe libraries (see the existing note).

## 5. Images, textures, icons

- **PBRgen** (partner, valid Nov 7 to 15, $500 Render Royalty prize): I could not find its site, formats or licence. UNCONFIRMED. If Quinn already has access it fits hull metal, rock and silt materials, so claim it before today ends, then list it in "Tools you used".
- **fal** image models for HUD icons, decals, console screens and a title image, via the same API key (model list and licence per model UNCONFIRMED).
- **Meshy retexture and Tripo texture** keep props consistent with their meshes.
- **Style control for all of it**: put one fixed style paragraph (dark, worn, realistic, desaturated, palette hex values) at the start of every prompt, generate at 1024 and grade every output with one shared post-process material. Use the same seed or reference image where the tool allows it. The tools do not guarantee this; reviewing is on us.

## 6. Video

Use the engine: record a short Level Sequence of the real game camera and render with Movie Render Queue (or the Movie Render Graph, out of beta in 5.8, [forum tutorial](https://forums.unrealengine.com/t/community-tutorial-movie-render-graph-in-5-8-quick-start/2723897)). It gives higher quality than real-time capture but blocks the editor while it renders ([Epic docs](https://dev.epicgames.com/documentation/unreal-engine/rendering-high-quality-frames-with-movie-render-queue-in-unreal-engine)). Avoid the legacy Movie Scene Capture, which used the wrong camera in 5.8 ([forum](https://forums.unrealengine.com/t/ue-5-8-movie-scene-capture-legacy-uses-the-wrong-camera-while-sequencer-and-mrq-use-the-correct-camera-cut/2744456)). Fastest safe path for a late jam: OBS or Win+G screen capture of a packaged build at 1080p, 30 to 60 seconds, edited in any editor. Use MRQ only if a cinematic fly-through is worth the setup.

## The shortlist

1. **Meshy MCP** (props: harpoon gun, relic, salvage, plus possibly the serpent). Gap: props and a creature mesh. Setup 10 minutes. Lead provides: a Meshy account (card required, cancel after) and the API key. Risk: free-plan models need attribution; rigging unproven for a serpent.
2. **OpenAI TTS for the Captain's 185 lines**. Gap: voice. Setup 15 minutes with a script. Lead provides: an API key (not in the repo). Risk: must disclose AI voice; line count means batching.
3. **fal key** (SFX, images, backup Rodin). Gap: the seven sounds, icons, title image. Setup 10 minutes. Lead provides: the key from the $30 perk. Risk: output licences per model unconfirmed; sounds need a listening pass.
4. **Rodin** (hero relic). Gap: one high-quality prop and partner prize eligibility. Setup 15 minutes via fal or the Blender MCP. Lead provides: redeemed Rodin Business perk (by Nov 16) and a key. Risk: downloads cost credits; no official MCP.
5. **Blender MCP + Tripo** (only if the serpent is still missing after 2 hours). Gap: a rigged serpent. Setup 30 to 45 minutes. Lead provides: Blender install, Tripo account and key, permission for an agent-made FBX. Risk: highest. Unauthenticated socket, rigging unproven, binary asset rule.

## Skip list

- ChiR24 Unreal_mcp: no 5.9 support found, and a binary plugin must match the engine version.
- StraySpark Unreal MCP: $130 for tools we already have, no 5.9 evidence.
- Any other third-party Unreal MCP: same reasons.
- Tripo's official MCP: alpha, Blender add-on only.
- Reallusion iClone, Rokoko: no time to pay back in a one-creature game.
- Hunyuan3D via Blender: needs a cloud account; licence not checked, skip.
- Movie Render Graph for the trailer: only if screen capture fails.

## Disclosure

- **Jam submission**: list every AI and partner tool in "Tools you used". **Rodin must be listed** to count for the Hyper3D prize; Tripo, Meshy, fal and PBRgen too, since partner prizes read that field ([jam page](https://itch.io/jam/2026-epic-ai-megajam)). The general rules need no AI disclosure, but the Official Rules PDF was unreadable and governs; a human should read it.
- **Players**: OpenAI requires a clear disclosure that a TTS voice is AI-generated ([guide](https://developers.openai.com/api/docs/guides/text-to-speech)). Put it in the credits and itch page.
- **Attribution**: Meshy free-plan models (CC BY 4.0) need a credit line; ElevenLabs free output needs a credit and is non-commercial; Tripo free-plan terms are unclear. Prefer paid-tier generation, or avoid these, so no credit is owed.
- Keep a list of every generated file and which plan made it.
