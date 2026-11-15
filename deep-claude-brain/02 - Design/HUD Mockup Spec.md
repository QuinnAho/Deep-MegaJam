---
updated: 2026-11-15 13:15 EST
---
# DEEP HUD spec (two directions)

> **Quinn, 1:54 PM (D-044): the colours do not match the theme.** "i dont like the use of color it doesnt match the theme please note that when we get to it". The colours below (cyan, amber, red) are not final. The HUD pass reworks the palette first, shows Quinn frames, and only then builds. The layouts stand.

Copied at 1:15 PM from the mockup agent's scratchpad so it survives a reboot. The pictures (both directions, four states, over real frames) are on Quinn's private HUD mockup page; the orchestrator has the link. Quinn picks A or B; the HUD pass lane builds from this note and [[Style Guide]] section 8.

Companion to `index.html` (open it for the pictures, the per-element number badges, and the Today view that shows the old HUD). Quinn picks A or B at about 2:15 PM. Then the engineer builds the chosen one in `SDeepHud` (Slate), with text, boxes, lines and images only, in the engine's own fonts. Source of truth for values: Style Guide sections 3 and 8.

## Global rules (both directions)

- **Canvas** 1920 x 1080. All sizes below are px at 1080p. Slate font size in points = px x 0.75 (18 px = 13.5 pt, 24 = 18, 36 = 27, 72 = 54). Scale everything by viewport height / 1080.
- **Grid** 8 px. Margin from the screen edge 48 px. Panel padding 16 px. Anchors are TL, TR, BL, BR, TC, BC, CC. The offset is in screen axes (+x right, +y down), measured after the 48 px margin is applied. "TL, offset (-48, -48)" means the whole screen.
- **Faces** (confirmed in `C:\Program Files\Epic Games\UE_5.9\Engine\Content\Slate\Fonts`, mapped in `LegacySlateFontInfoCache.cpp`, drawn through `FCoreStyle::GetDefaultFontStyle(name, size)`):
  - `"Light"` is `Roboto-Light.ttf`. Numbers and body text.
  - `"BoldCondensed"` is `Roboto-BoldCondensed.ttf`. Labels, caps, letter spacing 120.
  - `"Mono"` is `DroidSansMono.ttf`. Changing readouts. Droid Sans Mono is wider than the Roboto Mono used in the HTML, so allow about 8% more width on mono strings.
- **Colours** primary `#58E1D6`, secondary `#E0E6E6` at 88%, alert `#E5484D` (danger only), amber `#FFB45A`, panel `#000000` at 55% with a 1 px `#58E1D6` hairline (the code draws the hairline at 60% alpha, as today).
- **Fill colours for bars** use today's patience thresholds: primary above 50%, amber at 50% or below, alert at 25% or below. I read the style guide's "never amber on HUD lines" as hairlines and outlines, not bar fills. **Quinn: confirm.** If he disagrees, bars stay primary and only go alert at 25%.
- **Motion** ease-out cubic, no bounce. Panel open 200 ms, close 120 ms, bar lerp 250 ms, number tween 450 ms.
- **Shared elements.** The reticle dot, aim brackets, hatch chevron, threat edge, Captain subtitle, helm gauge, sonar and card dim are the same in both directions. They appear in both tables so each table is complete. Build them once.
- **State 2 (helm)** uses today's helm gauge and sonar unchanged in both directions. The only new element there is the hull bar.
- **Frame used for the helm state** has no sub hull in it. The sonar is drawn from the data model, not from the picture.

## Direction A: Instrument

Aboard there is no HUD, only object-anchored prompts. In the water: depth, cargo, patience and reticle, plus the conditional elements below. Panels, hairlines, square corners, no shadow. This is Style Guide section 8 done as written.

### A: elements

| # | What it is | When it shows | Font | Size at 1080p | Colour | Anchor and offset |
|---|---|---|---|---|---|---|
| 1 | **Depth panel** (A.depth). Tether strain line (2 px, y 122, x 16 to 264) only above 50% strain: #58E1D6 to 75%, then #E5484D at taut. No zone name, no tether text. | Water, Cut | Label: Roboto Bold Condensed caps, spacing 120. Number: Roboto Light. Unit: Roboto Light | 18 / 72 / 36 (unit) | Number #58E1D6; unit #58E1D6 at 70%; label #E0E6E6 at 88%; panel #000000 at 55% with 1 px #58E1D6 hairline (draw at 60% alpha as today) | BL, offset (0, 0), 280 x 128 |
| 2 | **Cargo panel** (A.cargo).  | Water, Cut | Label: Roboto Bold Condensed caps. Values: Roboto Light. Status: Roboto Bold Condensed caps | 18 / 36 / 18 | kg #58E1D6; cr #E0E6E6 at 88%; label #E0E6E6 at 88%; TOO HEAVY #FFB45A (row 3, only when overloaded); panel as depth | BR, offset (0, 0), 280 x 128 |
| 3 | **The Captain's patience** (A.patience). No panel, as today. The +N% refill text (today, right of the bar) is unchanged and not drawn here. | Water | Roboto Bold Condensed caps, spacing 120 | 18 (label), bar 4 px tall | Label #E0E6E6 at 88%; track #E0E6E6 at 18%; fill #58E1D6 above 50%, #FFB45A at 50% or below, #E5484D at 25% or below; ticks #E0E6E6 at 45%, 12 px tall | TC, offset (0, 0), 480 x 44 |
| 4 | **Reticle dot** (A.reticle).  | Water, Cut | - | 4 x 4 px | #E0E6E6 at 80% | CC, offset (0, 0), 4 x 4 |
| 5 | **Harpoon aim brackets + label** (A.aim).  | Water, when the aim ray hits something | Label: Roboto Mono | half-size 18 px, arm 6 px, label 18 px at centre + 32 px | Serpent #E5484D; terrain or sub #58E1D6; far #E0E6E6 at 45% | CC, offset (0, 0), 160 x 112 |
| 6 | **Hatch marker (off-screen chevron)** (A.hatch). Position is the screen-edge projection of the hatch direction, inset 48 px. Drawn here at the top edge, right of the patience bar. On screen it is the 20/8 px bracket with the label 28 px below (unchanged). | Water when carrying; Cut | Roboto Mono | 18 label; chevron arms 14 px, 2 px stroke | #58E1D6 | TL, offset (1280, 16), 160 x 56 |
| 7 | **Threat edge (red vignette strips)** (A.threat). Unchanged from today; shown at threat 0.8. | Water, Cut, when threat is above 0 | - | edge width 64 px lerp to 240 px by threat | #E5484D, alpha = T^2 x (0.30 + 0.20 x pulse) x falloff^2 x 0.35, pulse 0.5 to 1.6 Hz | TL, offset (-48, -48), 1920 x 1080 |
| 8 | **Bite alarm band** (A.bite). Unchanged from today (band at y 120; hairlines expand from the centre in 200 ms). | Water, Cut, while the serpent holds the tether | Heading: Roboto Bold Condensed caps 36. Prompt row: key cap Mono, verb Bold Condensed caps, detail Light | 36 / 18 / 24 | Text #E5484D; rules #E5484D, 1 px; band #000000 at 55% | TC, offset (0, 72), 1920 x 104 |
| 9 | **Captain subtitle** (A.subtitle).  | Any mode with a Captain line | Label: Roboto Bold Condensed caps. Body: Roboto Light | 18 / 24 | Name tag and left bar #FFB45A (2 px); body #E0E6E6 at 88%; panel #000000 at 55% | BC, offset (0, 0), 960 x 96 |
| 10 | **Helm sonar (plan view)** (A.sonar). Today's element, unchanged. Scope radius 168 px, centre (200, 224) inside the panel. Rim turns #E5484D and pulses with proximity. | Helm, when the sonar is on | Header: Roboto Bold Condensed caps. Readouts: Roboto Mono | 18 | Header and walls #FFB45A (upper 2 px; lower slice 1.5 px at 38%); rim #FFB45A at 45%; rings #E0E6E6 at 16%, every 20 m; hull outline #E0E6E6; salvage square #58E1D6 10 px; serpent diamond #E5484D | BL, offset (0, 0), 400 x 488 |
| 11 | **Hull integrity bar (NEW)** (A.hull). Fills the HullSlot that DeepHudLayout already reserves (48, 488, 400 x 48). Reads GetHullIntegrity01(). On a wall hit: bar eases 250 ms, a ghost segment of the lost amount fades over 600 ms, the hairline flashes #E5484D for 120 ms. At 0: bar replaced by the red label HULL BREACH. | Helm | Label: Roboto Bold Condensed caps. Value: Roboto Mono | 18; bar 8 px tall | Label #E0E6E6 at 88%; track #E0E6E6 at 18%; fill #58E1D6 above 50%, #FFB45A at 50% or below, #E5484D at 25% or below; ghost #E0E6E6 at 35%; ticks #E0E6E6 at 45%; panel as depth | BL, offset (0, -496), 400 x 48 |
| 12 | **Helm depth gauge** (A.helm). Unchanged from today. Gauge 0 to 500 m at 1.28 px per metre; line at panel x 136; stops as 16 px ticks. | Helm | Header: Roboto Bold Condensed caps. Depth: Roboto Light. Ticks: Roboto Mono | 18 / 72 / 36 / 18 | HELM, SUB and status #FFB45A (status #58E1D6 when parked); line and ticks #E0E6E6; limit zone #E5484D at 18%, limit line and label #E5484D; panel as depth | TR, offset (0, 0), 400 x 984 |
| 13 | **Settlement receipt card** (A.card). Same 720 wide card and full-screen dim as today. Net becomes the one 72 px number. Rows count up over 450 ms. | Settlement | Title: Roboto Bold Condensed caps 36. Rows: Roboto Light 24. Net: Roboto Light 72. Footer: Mono key, Bold Condensed caps | 36 / 24 / 72 / 18 | Title and Net #58E1D6; rows #E0E6E6 at 88%; fee rows #E0E6E6 at 60%; rules #58E1D6 at 60% and #E0E6E6 at 30%; panel #000000 at 55% over a full-screen #000000 at 55% dim | CC, offset (0, 0), 720 x 488 |
| 14 | **Card dim** (A.dim).  | Any card | - | full screen | #000000 at 55% | TL, offset (-48, -48), 1920 x 1080 |


### A: today's elements dropped or changed

- Zone name: dropped.
- Tether "left / length m" text: dropped. The strain is the 2 px line under the depth number, shown only above 50% strain.
- Lamp state row (ON, OFF, NOT OWNED): dropped. The lamp is visible; a missing lamp is a prompt at the object.
- Harpoon state row (READY, IN FLIGHT, LATCHED, SPENT): dropped. **Not placed.** The aim bracket carries it: ready is a primary bracket; latched is filled corners; spent is secondary at 45% plus a "NO HARPOON" cue. Needs a decision.
- Controls hint in the water: dropped (prompts only).
- Patience "+N% refill" text: unchanged, not drawn in the mockup.
- Feedback ping ring: unchanged, not drawn.
- Settlement card: net is promoted to one 72 px number; the gate row stays.

## Direction B: Diver cam (found footage)

The HUD is the diver's helmet-cam on-screen display. A live feed has a REC dot, a timecode, a channel label, depth, cargo, and a battery bar that is the Captain's patience. As the serpent closes in the feed degrades: signal bars drop, static and tear bands rise, then the Captain cuts the feed. There are no panels. Text is white-ish with a 1 px black outline (`FFontOutlineSettings`), so it reads on any frame.

### B: elements

Rows 1 to 8 are the shared elements; rows 9 to 20 are specific to B.

| # | What it is | When it shows | Font | Size at 1080p | Colour | Anchor and offset |
|---|---|---|---|---|---|---|
| 1 | **Reticle dot** (A.reticle).  | Water, Cut | - | 4 x 4 px | #E0E6E6 at 80% | CC, offset (0, 0), 4 x 4 |
| 2 | **Harpoon aim brackets + label** (A.aim).  | Water, when the aim ray hits something | Label: Roboto Mono | half-size 18 px, arm 6 px, label 18 px at centre + 32 px | Serpent #E5484D; terrain or sub #58E1D6; far #E0E6E6 at 45% | CC, offset (0, 0), 160 x 112 |
| 3 | **Hatch marker (off-screen chevron)** (A.hatch). Position is the screen-edge projection of the hatch direction, inset 48 px. Drawn here at the top edge, right of the patience bar. On screen it is the 20/8 px bracket with the label 28 px below (unchanged). | Water when carrying; Cut | Roboto Mono | 18 label; chevron arms 14 px, 2 px stroke | #58E1D6 | TL, offset (1280, 16), 160 x 56 |
| 4 | **Threat edge (red vignette strips)** (A.threat). Unchanged from today; shown at threat 0.8. | Water, Cut, when threat is above 0 | - | edge width 64 px lerp to 240 px by threat | #E5484D, alpha = T^2 x (0.30 + 0.20 x pulse) x falloff^2 x 0.35, pulse 0.5 to 1.6 Hz | TL, offset (-48, -48), 1920 x 1080 |
| 5 | **Captain subtitle** (A.subtitle).  | Any mode with a Captain line | Label: Roboto Bold Condensed caps. Body: Roboto Light | 18 / 24 | Name tag and left bar #FFB45A (2 px); body #E0E6E6 at 88%; panel #000000 at 55% | BC, offset (0, 0), 960 x 96 |
| 6 | **Helm sonar (plan view)** (A.sonar). Today's element, unchanged. Scope radius 168 px, centre (200, 224) inside the panel. Rim turns #E5484D and pulses with proximity. | Helm, when the sonar is on | Header: Roboto Bold Condensed caps. Readouts: Roboto Mono | 18 | Header and walls #FFB45A (upper 2 px; lower slice 1.5 px at 38%); rim #FFB45A at 45%; rings #E0E6E6 at 16%, every 20 m; hull outline #E0E6E6; salvage square #58E1D6 10 px; serpent diamond #E5484D | BL, offset (0, 0), 400 x 488 |
| 7 | **Helm depth gauge** (A.helm). Unchanged from today. Gauge 0 to 500 m at 1.28 px per metre; line at panel x 136; stops as 16 px ticks. | Helm | Header: Roboto Bold Condensed caps. Depth: Roboto Light. Ticks: Roboto Mono | 18 / 72 / 36 / 18 | HELM, SUB and status #FFB45A (status #58E1D6 when parked); line and ticks #E0E6E6; limit zone #E5484D at 18%, limit line and label #E5484D; panel as depth | TR, offset (0, 0), 400 x 984 |
| 8 | **Card dim** (A.dim).  | Any card | - | full screen | #000000 at 55% | TL, offset (-48, -48), 1920 x 1080 |
| 9 | **REC block (dot, timecode, channel)** (B.rec). The dot blinks at 1 Hz (500 ms on, 500 ms off) while calm, goes steady once threat is above 0.5, and is removed when the Captain cuts the feed. Timecode counts the dive in HH:MM:SS:FF at 24 fps, resets each dive. | Water, Cut, Helm, always while a feed is live | REC: Roboto Bold Condensed caps. Timecode: Roboto Mono. Channel: Roboto Bold Condensed caps | 24 / 24 / 18 | Dot #E5484D (breaks the red rule, see spec); REC and timecode #E0E6E6 at 88%; channel #E0E6E6 at 60%; black 1 px outline on all text | TL, offset (0, 0), 400 x 64 |
| 10 | **Viewfinder corners** (B.corners).  | Water, Cut, Helm | - | arm 32 px, 1 px, inset 24 px from the screen edge | #58E1D6 at 60% | TL, offset (-48, -48), 1920 x 1080 |
| 11 | **Battery = the Captain's patience, with signal bars** (B.battery). Segments = ceil(patience x 5). Signal bars (4) show 1 minus threat, rounded up; at one bar they turn #E5484D; at zero they are replaced by NO SIGNAL. | Water, Cut | Value: Roboto Mono. Label: Roboto Bold Condensed caps | 24 / 18; icon 96 x 32 | Segments #58E1D6 above 50%, #FFB45A at 50% or below (blinks 1 Hz), #E5484D at 25% or below (blinks 2 Hz); outline #E0E6E6 at 88%; signal bars #58E1D6 lit, #E0E6E6 at 18% dark; text has a 1 px black outline | TR, offset (0, 0), 304 x 64 |
| 12 | **Depth OSD** (B.depth).  | Water, Cut | Label: Roboto Bold Condensed caps. Number: Roboto Light | 18 / 72 / 36 (unit) | Number #58E1D6; label #E0E6E6 at 88%; no panel; 1 px black outline on all text | BL, offset (0, 0), 280 x 128 |
| 13 | **Cargo OSD** (B.cargo).  | Water, Cut | Label: Roboto Bold Condensed caps. Values: Roboto Light | 18 / 36 | kg #58E1D6; cr #E0E6E6 at 88%; TOO HEAVY #FFB45A; no panel; 1 px black outline | BR, offset (0, 0), 280 x 128 |
| 14 | **Camera reticle + focus corners** (B.reticle). While the harpoon aim ray hits something, the four focus corners hide and A.aim (the aim brackets) is drawn instead; same brackets in both directions. | Water, Cut | - | ticks 16 px with 16 px gap, 1 px; focus corners arm 16 px at half-size 80 px | Ticks and dot #E0E6E6 at 80%; focus corners #E0E6E6 at 45% | CC, offset (0, 0), 160 x 160 |
| 15 | **Scanlines and signal-loss static** (B.static). Slate can draw these with FSlateDrawElement::MakeBox and MakeLines, no texture. Film grain and the barrel vignette belong to the post-process pass (Style Guide section 5), not to Slate. | Scanlines always at 6%; tear bands from threat 0.3 | - | scanlines 1 px every 4 px; tear bands 4 to 24 px tall | Scanlines #000000 at 10%; tear bands #E0E6E6 at 6 to 12%, re-rolled every 80 ms, count = 1 + 4 x threat | TL, offset (-48, -48), 1920 x 1080 |
| 16 | **Signal warning text** (B.signal).  | Threat above 0.6 | Roboto Mono | 24 | #E5484D, blinks 2 Hz, black outline | CC, offset (0, 232), 480 x 32 |
| 17 | **Bite alarm OSD** (B.bite). Same data and timings as today's bite alarm; the panel band is dropped so the feed is not covered. | Water, Cut, while the serpent holds the tether | Heading: Roboto Bold Condensed caps 36. Prompt row: key cap Mono, verb Bold Condensed caps, detail Light | 36 / 18 / 24 | Text #E5484D; rules #E5484D 1 px, 480 px each side of the heading; no band fill; black outline | TC, offset (0, 72), 960 x 104 |
| 18 | **Hull integrity, 10 segments (NEW)** (B.hullseg). Same data as A.hull (GetHullIntegrity01()); segments echo the battery. | Helm | Label: Roboto Bold Condensed caps. Value: Roboto Mono | 18; segments 20 x 16 px, 4 px gap | Segments #58E1D6 above 50%, #FFB45A at 50% or below, #E5484D at 25% or below; unlit #E0E6E6 at 18%; just-lost segment #E0E6E6 at 35%, fades in 600 ms; panel as A.depth | BL, offset (0, -496), 400 x 48 |
| 19 | **Dive log receipt card** (B.card). Same rows as the settlement today. The sub-line needs two values the HUD frame does not carry yet: dive time and deepest depth. | Settlement | As A.card, plus a Roboto Mono 18 sub-line | 36 / 24 / 72 / 18 | As A.card; sub-line #E0E6E6 at 60% | CC, offset (0, 0), 720 x 520 |
| 20 | **Playback stamp** (B.play).  | Settlement | PLAY: Roboto Bold Condensed caps 24. Timecode: Roboto Mono 24. Channel: Roboto Bold Condensed caps 18 | 24 / 24 / 18 | Triangle and text #E0E6E6 at 88% (no red: nothing is recording) | TL, offset (0, 0), 400 x 64 |


### B: breaks from the style guide, with reasons

1. **REC dot is alert red `#E5484D`**, which breaks "alert = danger only". Reason: a red REC dot is the one thing every viewer reads as "recording" at a glance. It blinks 1 Hz while calm, goes steady above threat 0.5, and is removed when the feed is cut. Fallback if the rule stays intact: a `#58E1D6` dot. The cost is that it reads less like a camera.
2. **The dot is round**, which needs `FSlateRoundedBoxBrush`. Panels stay square-cornered. It is the only round element.
3. **No panels**: text uses a black outline instead of the 55% black fill. This replaces "panels: black 55%". Hairlines in primary are still used for the viewfinder corners.
4. Scanlines and tear bands are Slate boxes and lines (no texture). Film grain and the barrel vignette are a post-process job (Style Guide section 5), not Slate.

### B: new data the engineer needs

- `GetHullIntegrity01()` for the hull bar (both directions).
- Dive timecode (HH:MM:SS:FF at 24 fps), reset each dive.
- Dive time and deepest depth for the "FEED ENDED" sub-line on the receipt. These are not in the HUD frame today. If they cost too much, drop the sub-line and keep the card.

### B: today's elements dropped or changed

- Everything A drops (zone name, tether text, lamp row, harpoon row), plus the following.
- Panels on depth, cargo and patience: replaced by bare outlined text and the battery.
- Patience bar: replaced by the 5-segment battery.
- Bite alarm band: the OSD stays, the band fill is dropped.
- Controls hint in the water: dropped; if kept it sits under the REC block.
- Bailout air state (72 px seconds plus the draining red ring): **not placed.** The battery shows NO SIGNAL; the ring needs a decision.
- Settlement card: retitled "FEED ENDED" with a playback stamp.

## Not drawn in the mockup, still needed in either direction

Title, Dead, Review, Pause, Settings, Credits and ConfirmQuit cards stay as today until the pass reaches them. They should adopt the same panel (A) or the same outline-text and corner style (B). The Cut state is covered by state 3 (the cut warning). The ping ring is unchanged.

## Today's settlement receipt, for reference

A 720 px wide card on a full-screen 55% black dim. Rows: Gross haul; The Captain's cut (40%) with a minus; fee rows, dim, with a minus; Net, large, with a rule above; Credits; a gate row such as "Flashlight needs 50 cr". Footer "[Space] Back to the deck" and a Captain subtitle. Real values from the capture `opening-06b`: gross 70, cut -28, fees -5 ("Listening to you breathe") and -7 ("Captain's commentary"), net 30, credits 30. Rows count up over 450 ms.
