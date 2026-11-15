---
updated: 2026-11-15 02:10 EST
---
# Polish - Start Here

The one note a polish brief links to. The pass has not started. It waits on Quinn's choices below.

## Rules for the pass

- **One planned pass to a professional standard.** No patching of placeholders (D-028). Looks have the same weight as gameplay (D-029).
- **One style for everything seen and heard** (D-030). The style is written in `02 - Design/Style Guide.md` once Quinn picks it. Until that note exists, no visual work starts.
- **Numbers, not taste.** Type sizes, colours, animation timings and volume levels come from the style guide, so agents working in parallel cannot drift apart. That drift is the commonest way a jam game looks amateur.
- **Quinn approves frames at each gate.** An agent looks at its own frames before it reports.
- **One Unreal editor at a time.** The machine has 32 GB of RAM.

## Waiting on Quinn

1. The style: dark, worn and realistic (recommended by the asset scout and by Claude), or stylised.
2. Which packs he installs. He marks them on the look board (its URL is in Claude's memory, not in the repo).
3. Whether agents may create assets through the editor, each in its own folder, with no agent touching a map (the plan, section 6).
4. Whether the code-only work packages start before 10 AM.
5. The Captain's voice: OpenAI text-to-speech (needs a key, and the AI voice must be disclosed) or a local voice.
6. Sound: who finds the files, and whether he wants generation tools, which need keys or accounts from him (Meshy, OpenAI, fal, the Rodin perk by Nov 16).

## The notes

| Note | Use it for |
|---|---|
| [[Polish - Plan and Techniques]] | The master plan: the look at each depth, driving, the interior, the serpent, the HUD, the asset rule, budgets, work packages WP1 to WP10 and the cut order. |
| [[Polish - Assets]] | 61 picks, owned and free; the set that reads as one style; install order; gaps. |
| [[Polish - Engine Skills and Toolsets]] | Which of Epic's built-in agent skills and toolsets to use for each work package, and how to load a skill. |
| [[Polish - Craft Checklist and Skills]] | What "polished" means as checkable items; the tells of an amateur jam game; an outline for our own skill. |
| [[Polish - Sound, Voice, Fonts and References]] | Sound and music candidates with licences, voice options, HUD typefaces, the jam's rules on third-party assets. |
| [[Polish - Outside Tools]] | Generation tools for the gaps, what each needs and what must be disclosed. No third-party Unreal MCP server is recommended. |

## Amendments to the plan from the later research

- **Pack list.** The plan's WP0 lists Corals and Atlantis Ruins. The asset scout says both clash with the realistic set. The style choice settles it; if realistic, use the scout's install order.
- **Marine snow (WP1).** Start from Epic's `HangingParticulates` Niagara emitter template. `CreateNiagaraSystem` needs a template and cannot make a system from nothing. The plan's instanced spheres become the fallback.
- **HUD (WP6).** Keep native C++ widgets with the motion written in C++. Epic's widget-animation, MVVM and UMG skills only edit Widget Blueprints and do not apply.
- **Video (WP10).** Record with `SceneCaptureToolset.RecordViewport`, with resolution and frame rate raised from its defaults (1280 px, 15 fps). Movie Render Queue needs a tool our catalog lacks and a Level Sequence we do not have.
- **Lighting.** Do not load Epic's outdoor-lighting skill: it adds a sky and a sun. Use `material_basics` for materials.
- **Sound.** Epic's MetaSound recipes cover layered random ambience. Build from a fresh builder, not an attached one, which Epic names as the failure case.
- **Say it in the brief.** Epic's skills tell agents not to author new effects or render unless asked. Every polish brief must ask for it in so many words.
- **Add from the checklist:** a feedback table (what the player sees, hears and feels within a tenth of a second for grab, winch, cut, harpoon, bank, buy and drive); audio buses with the radio voice ducking the rest; settings for volume and mouse sensitivity; a credits and licences screen.

## Not confirmed anywhere

The Official Rules PDF. The download size of any pack. How long the first cook with pack materials takes. Seven sounds with no candidate. Prices and output licences of several generation tools. Whether any auto-rigger handles a serpent. The asset scout judged style from text, not from the pictures.
