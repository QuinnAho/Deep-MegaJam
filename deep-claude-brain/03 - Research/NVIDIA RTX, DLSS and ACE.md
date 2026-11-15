---
updated: 2026-11-14 20:55 EST
verified: Read from NVIDIA's developer pages and the itch.io jam page on Nov 14 2026. Nothing has been downloaded, installed or tested.
---
# NVIDIA RTX, DLSS and ACE

Status: **the NVIDIA RTX Award is at risk on UE 5.9.** No DLSS plugin build exists for it. The engine decision is in [[Engine Choice - 5.9 vs 5.8]] and tracked in [[Decision Log]].

## The award

- **Prize:** one GeForce RTX 5090.
- **Who gets it:** the team with the highest-ranked eligible Render Royalty entry built in Unreal Engine. It is a visuals award first. NVIDIA tech only makes an entry eligible.
- **What qualifies:** the game must use at least one of:
  - an NvRTX feature: RTX Mega Geometry, RTX Direct Illumination, RTX Indirect Illumination, RTX Path Tracing (the terms also name LSS Hair),
  - NVIDIA DLSS,
  - NVIDIA ACE.
- **How to claim:** answer the NVIDIA questions at submission. The form asks us to check the features used and describe how.
- Epic's judges decide who qualifies, with NVIDIA's technical help. If the top team does not qualify, the prize moves to the next qualifying entry.
- Wording differs in two places: the prize table says "DLSS 4.5 (Unreal Engine)", the terms say "NVIDIA DLSS".

## DLSS plugin

| | |
|---|---|
| Newest version | DLSS 4.5 plugin **8.9.0** |
| File date | 2026-10-16 |
| Engine versions | UE 5.5, 5.6, 5.7, 5.8 |
| Package contents | Streamline 2.14.16 / NGX 310.9.16 |
| Features | Super Resolution, DLAA, Ray Reconstruction, Multi Frame Generation, Dynamic Multi Frame Generation, Reflex, NVIDIA Image Scaling |
| **UE 5.9** | **No build on any NVIDIA page read** |

- NVIDIA's archive thread still shows 8.7.2 (July 21 2026) as its newest row. It was last edited before 8.9.0 came out, so use the get-started page.
- Not stated on the pages read: whether the download needs an NVIDIA developer login, whether the package includes source code, where to install it, and which console variables or Blueprint nodes turn Super Resolution on.

## NvRTX

- NVIDIA's own branch of Unreal Engine, hosted on GitHub. It carries the RTX features listed above.
- It is built from source. Jammers have reported using NvRTX 5.6 (2025 jam) and NvRTX 5.8 (this jam). The newest supported version is not confirmed from NVIDIA's page.
- Building an engine from source takes hours before any game work starts. Not realistic in a 24 hour jam.

## ACE

- The ACE Game Agent SDK went to beta on June 16 2026 with Unreal Engine 5 plugins:
  - **Speech recognition:** nemo-conformer-ctc-120m. English included, seven more languages downloadable.
  - **Small language model:** Qwen 3.5 4B from local GGUF files, with function calling.
  - **Text-to-speech:** Chatterbox Turbo 350M.
  - NVIDIA's Unreal page also lists Audio2Face-3D.
- NVIDIA's demo runs on UE 5.7. No list of supported engine versions was found.
- Everything runs on the player's RTX GPU, not in the cloud.

Why ACE is out for this jam:

1. The models are large and the build limit is 1 GB.
2. A judge without an RTX GPU could not run the feature.
3. The plugins target UE 5.7, not 5.9.
4. Integrating speech in, a language model and speech out is days of work, not hours.

The Director's voice uses scripted lines with pre-generated speech instead. See [[GDD]].

## Our options, in order

**1. A timeboxed 30 minute test of DLSS 8.9.0 on 5.9.** Outcome unknown.

1. Do it before any content is made. The project is empty today.
2. The project needs a C++ module for plugin source to compile. It has one: Quinn added `Deep` at 8:55 PM Nov 14.
3. Download "UE 5.8 DLSS Plugin" 8.9.0 from the get-started page.
4. Copy the plugin folders into `deep\Plugins\`.
5. With the editor closed, build the editor target. Then open the project and enable the plugin.
6. Pass: the editor opens with the plugin loaded, DLSS switches on in a test view, and a packaged build runs. Fail: the package has no source, or the compile errors are not a five minute fix.
7. Stop at 30 minutes whatever the result.

**2. Restart the project on UE 5.8**, where the plugin works as shipped. This gives up the agent toolsets that play and see the game. See [[Engine Choice - 5.9 vs 5.8]].

**3. Skip the NVIDIA award.** We stay eligible for Render Royalty itself.

Claude's read: option 1 only if this award matters to the team, otherwise option 3. Odds are discussed in [[Track Strategy]].

## If we qualify: notes for the submission answer

Write these down as they happen so the form takes five minutes.

- Plugin name and version, and the engine version.
- Which features are on in the shipped build and the default quality mode.
- Where the player can change it, and what the game does on non-RTX hardware.
- One or two frame-rate numbers with DLSS off and on, measured on the RTX 4080 laptop.
- A screenshot.
- Add "NVIDIA DLSS" to the tools list in [[Submission Checklist]].

## Unknowns

- Whether the 8.9.0 plugin compiles or loads on 5.9 Preview.
- Whether the download needs a login and whether it ships with source.
- Whether NVIDIA or Epic will publish a 5.9 build before the deadline.
- The newest engine version NvRTX supports.
- ACE plugin engine support and model download sizes.
- How judges weigh DLSS alone against entries using several RTX features. The jam page only says one feature is enough to be eligible.

## Sources

- https://developer.nvidia.com/rtx/dlss/get-started
- https://forums.developer.nvidia.com/t/nvidia-dlss-unreal-engine-plugin-archive/377564
- https://developer.nvidia.com/game-engines/unreal-engine
- https://developer.nvidia.com/blog/build-on-device-ai-companions-with-the-nvidia-ace-game-agent-sdk-and-unreal-engine-5-plugins/
- https://itch.io/jam/2026-epic-ai-megajam
