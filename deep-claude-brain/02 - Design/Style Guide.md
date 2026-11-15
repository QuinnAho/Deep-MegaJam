---
updated: 2026-11-15 02:44 EST
---
# Style Guide

Working guide. Quinn is leaning this way as of 2:40 AM Nov 15; he confirms it by approving frames.

Everything seen and heard copies from here (D-030). "(chosen here)" marks a value the research did not give; the rest is from [[Polish - Plan and Techniques]].

## 1. The look
A cheap, old, worn submarine over a black ocean. Surfaces look photographed: scanned rock, rusted steel, wet concrete, dull and dirty. Light is scarce and has to be bought: dim blue-green water with shafts at 100 m, near black at 300 m where only the lamp and the sub's lights show, pure black at 500 m. Colour is muted and drains toward blue-green with depth. Warm amber appears only where a person or machine is working. The look is led by light and reflection, not by asset count: the diver's lamp is the main subject (a visible beam in the murk, hard shadows, particles lighting up in the cone, darkness everywhere else), and wet surfaces are where it lands. The eye lands on the lamp, the water and the serpent.

## 2. Do and do not
Do:
- Light from a few local sources with strong falloff; let the rest go dark.
- Use physically based materials with roughness variation, grime and edge wear.
- Fade far geometry into fog, never into a visible clip edge.
- Grade every pack asset to section 4 before it goes in.
- Reveal the monster partly, at the edge of lamp range, sound first.
- Use one type family, four sizes, two weights.
- Give every action a visual, sound and feel cue within 100 ms.
- Ease every motion; send every sound through a bus.
Do not:
- Put saturated colour on a surface. Only lights and the HUD may be saturated.
- Use clean, bright or glossy-new assets (clean sci-fi, glowing fantasy, cartoon).
- Use pure white, flat grey or default engine content (cubes, mannequin, sky).
- Raise exposure or fog to "make it visible". Darkness is the design.
- Let any screen, light or sound pick its own colour, size, time or volume.
- Use emissive glow as decoration. Light must come from a source.
- Bounce, overshoot or spin UI.
- Play a sound with no variation or no bus.

## 3. Colour (sRGB hex)
| Name | Hex | Use |
|---|---|---|
| Water 100 m | #193C45 | far fog colour; downlight #A1D1ED |
| Water 300 m | #071A28 | far fog colour |
| Water 500 m | #000000 | far fog colour; directional scatter always black |
| Fog albedo | #40A6BF | volumetric albedo, all depths |
| Diver lamp | #F2F4EE (chosen here) | cool neutral white, never yellow |
| Sub ambient strips | #BFE3EA (chosen here) | cool, low |
| Sub practicals (console, helm) | #FFB45A (chosen here) | the only warm room light |
| Sub emergency | #D8271C (chosen here) | hatch rim, pulses on hull damage |
| Sub markers | #FF2A1A red, #22D96A green (chosen here) | blink 1 Hz |
| HUD primary | #58E1D6 | numbers, hairlines |
| HUD secondary | #E0E6E6 at 88% | body text, labels |
| HUD alert | #E5484D (chosen here) | danger only: cut, bite, threat edge |
| Panel fill | #000000 at 55% | all panels |
| Accent: amber | #FFB45A (chosen here) | below |
The single accent is amber. Allowed only on: console and helm screens and practicals, salvage glints, the Captain's name tag, warnings. Never on water, rock, the monster or HUD lines. Cyan is the HUD instrument colour, not an accent.

## 4. Materials
- Physically based only: base colour, roughness, normal. Roughness 0.4 to 0.9 on worn surfaces; below 0.25 only on the serpent's wet skin and glass.
- Worn: edge wear, rust, grime on every metal. No clean steel; paint is chipped, not glossy.
- Wet: submerged and sub-interior steel get roughness minus 0.1 (floor 0.3 to 0.5, wall steel 0.4 to 0.6, wet rock 0.35 to 0.55) with roughness varied by a grime mask so reflections break up; silt in crevices; puddles and streaks near the hatch at roughness 0.1 to 0.2 (chosen here). Dry, dusty surfaces stay above 0.7 so wet ones stand out. Reflections only work on a surface that has a normal map and variation; flat glossy planes are rejected.
- The hatch water is a near-black, low-roughness (0.05, chosen here) surface that must reflect the lit room.
- Never saturated: surface saturation at most 35%, value at most 70%. Colour comes from light.
- Grading scans together: pack material saturation 0.7, multiply by the depth tint, textures capped at 2048, reject any albedo brighter than HUD secondary.
- Use: Megascans rocks and harbour props, Industrial Infrastructure, Modular Ship Interior, Modular Underwater Exploration Base. Atlantis Ruins only graded dark.

## 5. Light at each depth
| | 100 m | 300 m | 500 m |
|---|---|---|---|
| Downlight lux | 2.0 | 0.04 | 0 |
| Fog density | 0.02 | 0.035 | 0.05 |
| Exposure bias (manual) | 0.0 | +1.5 | +2.0 |
| Bloom / vignette / grain | 0.4 / 0.35 / 0.10 | 0.5 / 0.45 / 0.18 | 0.6 / 0.55 / 0.25 |
| Saturation R,G,B,All | 0.75, 0.95, 1, 0.9 | 0.5, 0.9, 1, 0.8 | 0.4, 0.85, 1, 0.75 |
| Contrast / fringe | 1.05 / 0 | 1.08 / 0.2 | 1.1 / 0.35 |
| Volumetric distance | 80 m | 60 m | 40 m |
| Unlit visibility | 40 m silhouettes | 4 m near sub lights | 0 m |
Exposure locked, local exposure contrast 1.0, volumetric start 1.5 m. Lamp 40,000, cone 12 to 24 degrees, range 35 m, offset 25 cm right and 15 cm up, wobble 2% at 0.3 Hz, no flicker. Belly flood 100,000, 35 degrees, 60 m. Salvage glint: 100% at 100 m, 30% at 300 m, 15% at 500 m.

## 5b. Ray tracing
The project has Lumen GI and reflections with hardware ray tracing on (DX12). Use that; the Path Tracer is offline only and is not for the game.
- With it on: the lamp casts hard shadows; the hatch water reflects the lit room; the helm window shows a faint reflection of the room over the dark sea; wet steel and rock reflect the lamp and sub lights.
- With it off (no RTX card) the game must still look acceptable: the lamp beam, shadows and fog are unchanged, and the hatch and window fall back to screen-space or a fixed faint tint at the same colours. Check frames both ways before approval.

## 6. The monster
- Long, dark, wet: near-black skin, roughness 0.25, 0.9 m thick behind the head tapering to 0.1 m, a ridge along the back.
- Seen only by light: a cyan-green rim and two rows of small pulsing spots that speed up as threat rises. No other colour.
- Not shown in the first minute of a dive. First sight is a partial silhouette at the edge of lamp range, after a sound.
- Sound before shape: a low rumble and a pitch drop on the tether mean it is near.
- It retreats from the lamp, visibly and audibly, so the rule is learned.
- It answers the lamp: wet skin that glints where the beam hits, and eyes that shine back only while the beam is on them (retro-reflect, off otherwise).
- Contact: 0.15 s hit-stop (chosen here), heavy sound, camera lunge, then the consequence.
- Route open: code-driven body, owned or free mesh, or a generated head, if it passes section 11.

## 7. The character (NOT SETTLED)
First person shows arms and gear only. A simplified character fits this world if the arms are a diving-suit sleeve and glove in worn dark rubber and canvas, in the section 4 materials with the same wear as the steel; no bare skin, no outlines or flat colour; gear (lamp, harpoon, tether clip) uses the sub's metal. Keep detail low and let the dark hide it. An arm that looks cartoon is rejected.

## 8. HUD and type
- Faces: engine Roboto Light for numbers, Bold Condensed caps (letter spacing 120) for labels, Mono for changing readouts. No font asset needed. B612 Mono replaces Mono only if Quinn adds it.
- Sizes at 1080p (chosen here): 18 px label, 24 px body and subtitles, 36 px readout, 72 px depth. Weights: Light and Bold Condensed only.
- Panels: black 55%, 1 px hairline in HUD primary, 8 px grid, 16 px padding (chosen here), square corners, no shadow.
- Motion, ease-out cubic, no bounce (chosen here): panel open 200 ms, close 120 ms, fade or slide 200 ms, prompt fade 150 ms, bar lerp 250 ms, number tween 450 ms, scene fade 600 ms (range 300 to 1500).
- Aboard: no HUD, prompts anchored to objects. Water: depth, cargo, patience, reticle only.

## 9. Sound
Buses: Master, Voice, Effects, Ambience, Music. Levels relative to Master 0 dB (chosen here): Voice -3, Effects -6, Ambience -12, Music -18.
- The Captain's radio ducks Music 9 dB, Ambience 6 dB, Effects 3 dB (chosen here), attack 50 ms, release 500 ms.
- Radio chain: high-pass 450 Hz, low-pass 3200 Hz, light crush, hiss bed, squelch before and after.
- Ambience crossfades between depths over 3 s. Inside the sub, outside sounds low-passed at 1.5 kHz (chosen here).
- Each repeated sound has 3 or more variations and 3% pitch jitter. At least two planned near-silences per run. No clipping; fix by bus gain.

## 10. The numbers table
| Item | Value |
|---|---|
| Accent / HUD primary / secondary / alert | #FFB45A / #58E1D6 / #E0E6E6 at 88% / #E5484D |
| Water 100, 300, 500 m | #193C45, #071A28, #000000 |
| Lamp / sub strips / practical / emergency | #F2F4EE / #BFE3EA / #FFB45A / #D8271C |
| Type sizes at 1080p | 18, 24, 36, 72 px |
| Panel open, close, fade, number tween | 200, 120, 200, 450 ms |
| Buses | Voice -3, Effects -6, Ambience -12, Music -18 dB |
| Duck (Music, Ambience, Effects), attack, release | 9, 6, 3 dB, 50 ms, 500 ms |
| Surface saturation max, texture cap | 35%, 2048 px |
Depth values: section 5.

## 11. Acceptance test (five yes answers or it stays out)
1. Dark, worn and unsaturated at its depth, with no clean or glossy surface?
2. Only section 3 colours, with amber only where allowed?
3. Graded: saturation at most 35%, textures at most 2048, no albedo brighter than HUD secondary?
4. Still reads with the lamp as the only light and the HUD hidden, and acceptable with ray tracing off?
5. Sound: on a bus, at its level, with variations. UI: section 8 sizes and timings.

## 12. Still open
- Quinn's approval of this direction (by frames).
- The character: simplified or realistic arms.
- The monster route and look.
- Which packs he installs; NVIDIA award eligibility (checked separately).
- The Captain's voice, and whether to add the B612 Mono font.
