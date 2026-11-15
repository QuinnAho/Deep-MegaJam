---
updated: 2026-11-15 02:15 EST
verified: 2026-11-15 02:15 EST. Web search plus fetch of the GitHub pages for Epic's plugin, quodsoler, maystudios and anthropics/skills (skill lists and licences read from the pages; commit dates not shown, so freshness unchecked). Talk and article existence confirmed by search listings only. I did not open the slides or videos, so the checklist items are engineering readings of the general techniques, not quotes. Nothing installed.
---

# Polish - Craft Checklist and Skills

Part 1 is what exists as agent skills. Part 2 is the checklist. Part 3 is the amateur tells. Part 4 is a proposed project skill. Style is a placeholder everywhere: Quinn has not chosen it. No item asks us to copy another game.

## 1. Skills that exist

Honest summary: nothing targets polish, game feel or horror. The useful ones are Unreal API references.

| Skill set | Contains | Licence | Fit |
|---|---|---|---|
| quodsoler/unreal-engine-skills (github.com/quodsoler/unreal-engine-skills) | 31 skills (README says 27): ue-audio-system (MetaSounds, submixes, attenuation), ue-ui-umg-slate (UMG, Common UI, widget animation), ue-materials-rendering, ue-niagara-effects, ue-gameplay-cameras (camera shakes), ue-input-system, ue-cpp-foundations | MIT | Best fit. C++ UE reference, so agents stop guessing UMG, submix and camera-shake APIs. Not polish guidance. Written for an older UE: check API names against 5.9. Only 2 commits shown. |
| EpicGames/unreal-engine-skills-for-claude-code-plugin | One skill, unreal-mcp, plus a session-start hook. Drives the editor over MCP | MIT | We already run the MCP route. Overlaps `Unreal MCP in UE 5.9.md`. Skip unless that note lacks something. |
| maystudios/claude-skills | 17 skills: unreal-best-practices (mentions CommonUI), unreal-blueprint-codegen (widget blueprints), unreal-gas, audio-to-midi, google-ai-studio-tts | MIT | Marginal. The author shipped a UE 5.7 horror game, but no horror or polish skill. Needs a Gemini key for some. Do not add. |
| Donchitos/Claude-Code-Game-Studios (search listings; I did not open the repo) | Template: about 49 agents, 70+ skills, hooks, studio hierarchy, UE5 agent set | MIT (per listings) | Process scaffolding, heavy. Would collide with our `.claude/loop.md`. Read for ideas only. |
| anthropics/skills | Document skills, MCP builder, web app testing, design and branding examples. I found no game, UE or audio skill | Mostly Apache 2.0, per-skill | Web-aimed. The frontend-design material is about HTML, not UMG. Only the SKILL.md format is useful to us. |

Recommendation: at most install quodsoler's ue-audio-system, ue-ui-umg-slate, ue-gameplay-cameras and ue-niagara-effects (copy those four folders, not the set; check MIT notice). Write our own deep-polish skill (part 4). Avoid unvetted aggregator sites (skillselion, claudeskills.info): they are indexes with unverified star counts.

## 2. The checklist
Each item: something an engineer does and a reviewer can check by playing.

### 2.1 Feedback on every action
Source: Swink, *Game Feel* (2008); Jonasson and Purho, "Juice It or Lose It" (GDC Vault 1016487); Nijman, "The Art of Screenshake". Principle: every input gets a visible, audible, felt response inside about 100 ms.
- Each of grab, winch, cut, harpoon, bank, buy and drive has a table row: visual cue, sound cue, camera or controller cue, all fired on the input frame.
- No action has a sound-only or text-only response.
- Interact prompts change state on hover and on press, and a failed press (cannot afford, harpoon spent) has its own distinct cue.
- Harpoon: fire gives recoil kick, muzzle or bubble burst and a sound; hit and miss are visibly different.
- Bank and buy: count animates, a confirm sound plays, the console reacts physically (light flash or screen change).
- Tether cut: the cut is shown at the cable, with a snap sound, a camera jolt and a hard change to the HUD.
- Every cue is under 0.5 s unless it is a story beat, and rapid repeats do not stack into noise (cap voices per cue).
- Reviewer test: mute the game, then close your eyes. Each action must still be identifiable in one of the two.

### 2.2 Camera and body
Source: Swink; Nijman talk (screenshake as impact); GDC and developer write-ups on first-person view bob are plentiful but I did not pin one primary source. Mark: unconfirmed.
- Walk and swim sway is a low-amplitude sinusoid, amplitude scaled by speed, and zero when still.
- A slow breathing drift (roughly 0.2 to 0.3 Hz) runs on camera pitch and roll while idle.
- Camera shake is a named asset set (small, medium, large) with one amplitude table, never ad-hoc values.
- Landing on the sea floor and hatch exit each have a one-time impact: dip, settle, and a sound.
- The hatch drop: input is gated, FOV or camera rolls during the fall, and control returns with a short ease, not a snap.
- Look input has no smoothing lag above one frame; sway never fights mouse look.
- Provide a "reduce camera motion" setting that scales sway and shake to zero.
- Reviewer test: film the screen with the mouse untouched for 10 s. Something must still move, gently.

### 2.3 Light and dark
Source: Dead Space and Alien: Isolation developer interviews on darkness and sound (links in 2.6); Epic UE docs on lighting (not fetched). Mark: lighting items are general technique.
- Depth sets ambient light, fog colour and fog density by a table of three rows (100, 300, 500 m), blended by depth, not by zone triggers.
- At 300 m and 500 m the player cannot read the scene without the lamp.
- Lamp has a cone, falloff, a slight flicker or wobble tied to power or the monster, and a hard limited reach.
- Lamp on, drive-off beat: the lamp spikes brighter and the serpent visibly recoils.
- Light something interesting every few metres: salvage gets a faint glint so dark is a choice, not a puzzle.
- Marine snow or particulates catch the lamp cone.
- Nothing shows its mesh edge: far geometry fades into fog, not into a visible clip plane.
- Reviewer test: screenshot at each depth. The three must be readable as different places without labels.

### 2.4 The monster
Source: Alien: Isolation audio reporting (PC Gamer "Seeing with your ears"; MCV on audio and emotion); Dead Space "fear emitters" (via Develop and PC Gamer).
- Presence before sight: a sound layer that grows with distance, one distinct sound per state (hunting, drawn to noise, repelled by lamp).
- A tether or camera cue fires when it is near: slight pitch drop, low rumble, HUD noise.
- First sighting is partial: a silhouette at the edge of lamp range, not a full model in view.
- Do not show it in the first minute of a dive.
- Contact has four parts: a freeze or hit-stop of 0.1 to 0.2 s, a heavy sound, a camera lunge, and a cut to the consequence.
- After contact the world reacts, whether Captain line, tether state or HUD.
- Retreat from the lamp is a readable animation and a sound, so the player learns the rule.
- Reviewer test: with the monitor off, a listener can say if it is near.

### 2.5 Interface
Source: Ignacio, "Crafting Destruction: The Evolution of the Dead Space User Interface" (GDC Vault 1017723; slides not opened, summary from articles); Giant Bomb "Markers" series for the caveat that diegetic is not always better.
- On screen at all times: crosshair dot and one tether-or-air indicator. Everything else shows on demand.
- In the world: depth on the diver's wrist or helmet, banked salvage at the console, upgrades on the console screen.
- Panels open in under 0.25 s with ease-out, close faster than they open, and never block look input without a stated reason.
- Numbers tick up over 0.3 to 0.6 s with a tick sound, and flash on change. No instant jumps for money.
- One typeface family, at most two weights, one size scale (for example 4 sizes) defined once in a style asset.
- Body text is readable at 1080p at 2 m from a monitor. Contrast is checked on the dark ocean, not a flat colour.
- Every prompt names its key or button from the Input mapping, not a typed constant.
- Reviewer test: screenshot with HUD hidden. Can you still tell what is going on from the world?

### 2.6 Sound
Sources: PC Gamer "Seeing with your ears: the audio of Alien: Isolation"; Develop/MCV interview with Sam Cooper; PC Gamer "The sound of silence". Ducking mechanics specifically: not confirmed in a primary source.
- Four buses: music, effects, ambience, voice, with one master. Volumes set from a table, with sliders on the first three plus master.
- Ambience bed (water, hull, depth tone) always present, one bed per depth, crossfaded 2 to 4 s.
- Voice ducks music and ambience by a set amount (for example 6 dB, attack 50 ms, release 500 ms) while the Captain speaks.
- Radio voice has its own filter chain (band-limit, small noise, squelch on and off) so it sounds like a radio, not narration.
- Planned silence: at least two moments per run where music and ambience drop to near nothing, and each is before a payoff.
- Each repeated sound has 3 or more variations with pitch jitter of a few percent; no cue plays identically twice in a row.
- Occlusion or low-pass applies inside versus outside the submarine, so the transition through the hatch is audible.
- No clipping: check the mix at max effects volume and fix by bus gain, not by per-asset guesses.

### 2.7 Transitions and pacing
Source: general practice; Swink on pace. No primary talk pinned. Mark: unconfirmed.
- Title to game, game to dive, dive to console and death to restart each use a fade or cut of defined length (0.3 to 1.5 s), never a hard pop.
- Loading or setup work is hidden behind a fade, not a frozen frame.
- Leaving the sub: hatch opens, sound, drop, then control, as one scripted beat under 6 s.
- Returning: a distinct surfacing beat (sound change, light change) before the console.
- Death: the cut tether or serpent hit ends with a short hold, a line from the Captain, then a restart within 5 s of that line.
- Respawn state is reset in one function, with a test that checks harpoon, tether and upgrades.
- Win: relic pickup triggers a slowdown, a held shot, a sound change, then credits. Not an immediate text box.
- Reviewer test: stopwatch each transition and compare to the table above.

### 2.8 First sixty seconds, last ten
Source: itch.io jam post-mortems and "5 common mistakes" (Game Developer); all anecdotal. Mark: no primary source.
- Launch lands on a title screen with the logo, one good looking frame behind it, and Play, Settings, Quit.
- First control prompt appears within 10 s and names the keys. Nothing reads like a manual.
- The first Captain line lands within 15 s and sets tone.
- First action by 30 s: the player moves, looks, or interacts with something that responds.
- By 60 s the player has seen the sea, the hatch and a reason to go.
- Last ten: the win beat (2.7) reads as a conclusion, and a credits screen with licences follows.
- Last-run state: dying at 499 m, winning, and quitting all end cleanly.
- Capture a trailer-safe screenshot for each of title, console, depth 100, 300, 500 and the win.

### 2.9 The floor of professionalism
Source: itch.io jam guides (potbanksoftware "Common Game Jam Pitfalls"; bugnet "common game jam mistakes"; Game Developer "5 common mistakes").
- Settings: master, music, effects, voice sliders and mouse sensitivity, saved and applied before the next frame.
- Pause menu on Esc with Resume, Settings, Quit to title. Time and audio pause.
- Quit exits to desktop without a crash dialog. Test the packaged build.
- Grep the build for "TODO", "test", "placeholder", "lorem" and default engine names or textures.
- Wording is consistent: one name per mechanic, one casing rule, one tense.
- Credits list people, tools and all asset licences (Fab, CC-BY credit lines).
- Window title and icon are the game's, not the engine's. Cursor shows in menus, hides in game.
- Run the packaged build on a clean machine or account before submitting (overscope and untested builds are the top jam failures per those guides).

## 3. What looks amateur (tells and fixes)
Sources: the itch.io and Game Developer pieces in 2.8 and 2.9, plus Jonasson and Purho and Nijman (flat versus juiced). Mostly anecdotal.
1. Default engine content (mannequin, default skybox, grey cubes). Fix: replace or hide; swap before anything else.
2. Silent or flat interaction. Fix: part 2.1 table.
3. Text sizes, fonts and colours that differ between screens. Fix: one style asset (2.5).
4. Instant everything: no easing on panels, numbers or fades. Fix: timing table (2.5, 2.7).
5. Loud audio with no mix; music over voice. Fix: buses and ducking (2.6).
6. Uniform lighting at all depths. Fix: depth table (2.3).
7. No menu, no pause, no sensitivity setting. Fix: 2.9.
8. Controls unexplained. Fix: first-10-second prompt (2.8).
9. Weak first screen or store cover; the cover is often a menu screenshot with the buttons cropped. Fix: dedicated screenshot list (2.8).
10. Over-scoped, untested submission. Fix: freeze content and test the package.
We are most at risk of 3, 4 and 5: many agents in parallel will each choose their own fonts, timings and volumes.

## 4. Proposed skill: .claude/skills/deep-polish/SKILL.md
```
---
name: deep-polish
description: Use when changing anything the player sees, hears or feels in DEEP
---
# Style
(PLACEHOLDER: Quinn picks the look. Palette, typeface, motion feel go in `Style.md`; read it first.)
# Rules
1. One source of truth: colours, fonts, sizes, timings, volumes live in one data asset; never hard-code.
2. Every player action uses the feedback table (see Polish - Craft Checklist 2.1).
3. Timings: panel 0.25 s, fade 0.3 to 1.5 s, number tick 0.3 to 0.6 s.
4. Audio goes through the four buses; voice ducks others.
5. No binary assets except those a human added; use approved Fab picks only.
6. Wording: use the Glossary; no placeholder text.
7. Do not touch a map a human has open.
# Procedure
1. Read Style.md, the Board row, the checklist group for your task.
2. Make the change in data first, then code.
3. Play it: launch via tools/ue.ps1, screenshot into the evidence folder.
# Before reporting
- [ ] Row in the feedback table filled (visual, sound, feel).
- [ ] Screenshot or clip path given.
- [ ] Mute test and no-HUD test run.
- [ ] No new hard-coded colour, font, timing or volume.
- [ ] Wording grep clean (TODO, placeholder).
- [ ] Devlog entry written.
```
