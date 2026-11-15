---
updated: 2026-11-15 14:50 EST
---
# HUD Palettes

Three palettes for the HUD (D-044), drawn by a sonnet agent at 2:31 PM Nov 15 (row 87 in [[Agent Runs]]) and shown as a switch on the private HUD mockup page, version 2. Quinn picks one. Until then HUD A builds in Tonal water. The layout is in [[HUD Mockup Spec]], the rules in [[Style Guide]].

Six roles: primary, secondary, warn, danger, panel fill, and machine amber for the sonar, the helm and SUB marker and the Captain's tag. Machine amber is kept apart from warn, so a palette can keep the console glow without spending the warning colour. In code they are constants in `deep/Source/Deep/UI/DeepHudStyle.h`, so a palette is a one-file swap.

## Bone and oxide

Why it fits: dirty off-white and rust are what a cheap, old, worn hull looks like, and amber stays as the console glow.

| Role | Hex | Alpha | Used for |
|---|---|---|---|
| Primary | #DDD6C3 | 100% (hairline 60%) | depth, cargo, helm depth numbers, panel hairlines, hull fill, hatch marker, terrain aim brackets |
| Secondary | #A89F8D | 100% | labels, body text, subtitle text, readouts; tracks 18%, ticks 45%, rings 16%; reticle 90% |
| Warn | #FFB45A | 100% | patience fill at 50% or below, patience pips |
| Danger | #E84E36 | 100% (edge vignette 3 to 11%) | threat edge, bite band, serpent bracket, helm limit zone (18%) |
| Panel fill | #0B0907 | 60% | all panels, bite band, card, card dim |
| Machine | #FFB45A | 100% | sonar, helm and SUB marker, Captain tag |

## Tonal water

Why it fits: it is cut from the water's own blue-grey, so the HUD reads as part of the sea and drains with it. Amber and red stay as the only warm notes.

| Role | Hex | Alpha | Used for |
|---|---|---|---|
| Primary | #A9C5D0 | 100% (hairline 60%) | depth, cargo, helm depth numbers, panel hairlines, hull fill, hatch marker, terrain aim brackets |
| Secondary | #7F9AA4 | 100% | labels, body text, subtitle text, readouts; tracks 18%, ticks 45%, rings 16%; reticle 90% |
| Warn | #FFB45A | 100% | patience fill at 50% or below, patience pips |
| Danger | #EC5650 | 100% (edge vignette 3 to 11%) | threat edge, bite band, serpent bracket, helm limit zone (18%) |
| Panel fill | #04121A | 60% | all panels, bite band, card, card dim (a near-black taken from Water 300 m, #071A28) |
| Machine | #FFB45A | 100% | sonar, helm and SUB marker, Captain tag |

## Amber phosphor

Why it fits: the whole HUD becomes one console screen, the same dim amber as the helm and sonar practicals. A warning is the screen driven hot, pale cream.

Caveat: the Style Guide says amber is "never on HUD lines" and cyan is the HUD colour. This palette breaks that rule on purpose, so it needs Quinn's say-so and an edit to section 3.

| Role | Hex | Alpha | Used for |
|---|---|---|---|
| Primary | #D39A4C | 100% (hairline 60%) | depth, cargo, helm depth numbers, panel hairlines, hull fill, hatch marker, terrain aim brackets |
| Secondary | #B38B54 | 100% | labels, body text, subtitle text, readouts; tracks 18%, ticks 45%, rings 16%; reticle 90% |
| Warn | #FFF0C2 | 100% | patience fill at 50% or below, patience pips |
| Danger | #EE3F36 | 100% (edge vignette 3 to 11%) | threat edge, bite band, serpent bracket, helm limit zone (18%) |
| Panel fill | #0B0703 | 60% | all panels, bite band, card, card dim |
| Machine | #D39A4C | 100% | sonar, helm and SUB marker, Captain tag (same as primary) |

## Today, for reference

Primary #58E1D6, secondary #E0E6E6 at 88%, warn and machine #FFB45A, danger #E5484D, panel #000000 at 55%.

## The three tests

Contrast ratio against the brightest 5% of the panel area under each element, taken from `bg/*.jpg` with the panel fill composited over it. Rows: helm panel (s2, rock), subtitle (s3, lamp glow, hardest panel zone), bite band (s3), depth panel (s1). Text 18 px and up needs 4.5 for body, 3 for large.

| Palette | Primary | Secondary | Warn | Danger | Danger vs warn (dE) | Warn vs primary (dE) |
|---|---|---|---|---|---|---|
| Today | 8.5 to 13.2 | 8.7 to 12.7 | 7.7 to 11.9 | 3.5 to 5.4 | 54 | 85 |
| Bone and oxide | 9.3 to 14.0 | 5.2 to 7.7 | 7.7 to 11.5 | 3.6 to 5.4 | 47 | 50 |
| Tonal water | 7.0 to 11.0 | 4.3 to 6.7 | 7.2 to 11.3 | 3.6 to 5.7 | 49 | 69 |
| Amber phosphor | 5.6 to 8.3 | 4.4 to 6.6 | 12.1 to 18.0 | 3.5 to 5.3 | 82 | 40 |

- Danger: red against a dark panel is never below Today's, and it is the only red on the screen in all three. In Bone and oxide it is also the only orange-red, 47 dE from amber, so it separates by hue and by lightness. Danger text is weakest over the lamp glow (3.5 to 3.6), the same as Today. Keep it at 24 px and bold or larger there.
- Warn: in Bone and oxide and Tonal water, amber is a different hue from every readout, so a warning is obvious. In Amber phosphor it is weakest, at 40 dE: it differs mainly by brightness (cream against tan). It should also blink, as the spec already does for the pips, or it can be missed.
- Readability: all primary and secondary text clears 4.3 on panels. Without a panel (aim brackets, reticle, hatch marker) over the brightest lamp glow, every palette including Today falls to about 1 to 2. The real build needs a dark text outline there, as the mockup's `.ol` class does for labels. The secondary reticle dot is the weakest on glow in Bone and oxide.

## Recommendation

Tonal water: it is the only one that comes from the world's own colours, so it matches "drains toward blue-green with depth" and keeps amber and red as the only warm marks, which is what the Style Guide asks for.
