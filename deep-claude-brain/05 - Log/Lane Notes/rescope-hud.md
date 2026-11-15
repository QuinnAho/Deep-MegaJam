# Lane note: rescope-hud (the calm HUD)

Branch `rescope/hud`, worktree `.wt/text`, Nov 15 afternoon. Quinn at 3:50 PM: "the UI is still too cluttered and really takes away from the immersion" and "the captain is still useless too".

## What changed

One new pure layer, `UI/DeepHudCalm.h/.cpp` (`FDeepHudCalm::Apply`), thins the frame `ADeepHUD` gathers before `SDeepHud` paints it. `ADeepHUD` (Game/) was not touched. Its numbers are in `FDeepHudCalmTuning`.

## Cut, and why

- **The Captain's subtitle** in every play mode (Aboard, Helm, Water, Cut, Dead, Settlement). `Deep.Captain.Radio` now defaults to 0: no radio audio. `Deep.Captain.Radio 1` brings the voice back, but not the subtitle.
- **The patience bar**: gone from every mode. The tether lane stops the timer. The enum bit stays, and no mode lists it.
- **The helm gauge column** (a 400 px column with stops, the limit band, SUB and LIMIT labels, the status and the "between stops" text). It is replaced by a slim readout at the bottom right: DEPTH, the number, and NEXT STOP N m, or LIMIT N m in alert when the next stop is past the limit. The sonar is the instrument.
- **The 72 px water depth**: now a small panel, 200 x 88, with a label and a 36 px number. The strain line sits at its foot.
- **Every prompt but one**: only the nearest usable thing in reach is shown (it has a key; anchored prompts are ranked by distance to the screen centre). Keyless far tags are dropped: HELM, BANK CARGO, HATCH LOCKED, DIVE, TAKE IT HOME.
- **The warning line**: it now shows Danger only (the end of the line, the Captain about to cut). "Too heavy" lives on the cargo panel.
- **Feedback cues**: only critical ones remain: the tether cut or bitten, hull hit, hull critical, hull breach, air low, a bite. Grabbed, Banked, Parked, Harpoon away, fees and the rest are gone.

## What stays, per mode

| Mode | Panels (shown) |
|---|---|
| Aboard | one object prompt, critical cues, the controls hint until first use |
| Helm | sonar, slim depth readout with the next stop, hull (3 s after a change, always below 30%), critical cues, the helm hint until the sub moves |
| Water | small depth, cargo (3 s after a change, or while too heavy), reticle, threat edge, one prompt, bite alarm, hatch marker when carrying, the hint until used |
| Cut | as Water plus the bailout air and the hatch marker; AIR LOW once below 30% |
| Pause | the card plus the full CONTROLS list of the last play mode |

- **The controls hint** shows until its controls are used once, then never again this run: Water after 3 m of movement, any cargo or line paid out; Helm after 1 m of sub depth or a stop change; any mode once the player leaves it. It stays at least 2.5 s so it can be read. A new run (the title) resets it.
- **The helm hint keys** follow the helm lane: W/S throttle, A/D rudder, SPACE/C ballast (hold to rise or sink), X all stop, E stand up.
- **Fades**: the hint, hull and cargo ease in and out over 0.4 s. HUD A's colours are unchanged.

## Tests

- New: `Deep.UI.HudCalm.Panels`, `.Hint`, `.Hull`, `.Filter`.
- Updated:
  - `Deep.UI.Hud.Panels`: water patience is now off, and there is no subtitle or patience in any play mode. The old assert said water shows patience.
  - `Deep.UI.Hud.Layout`: the depth panel is compact instead of 280 x 128. The strain-line-Y check is replaced by "depth clear of cargo".
  - `Deep.UI.Hud.GaugeLabels`: it now tests on a local copy of the old 400 px column, because `HelmGauge()` returns the slim readout. The SUB and LIMIT rule is kept in case the gauge comes back.
- Results: see the commit and report.

## Frames

`devlog/media/2026-11-15-rescope-hud-helm.png`, `-aboard.png`, `-water.png` (1280x720).

## Open

- The title still shows the Captain's "press any key" subtitle. Title is not a play mode.
- The pause card grows to 600 px wide when it has a controls list. The 7-row water list was not checked at 720p for fit.
