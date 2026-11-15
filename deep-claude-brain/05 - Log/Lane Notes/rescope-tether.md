# Lane note: rescope-tether (the line is the air line)

Branch `rescope/tether`, Nov 15, about 4:15 to 4:35 PM EST. Brief: Quinn at 3:50 PM, "the whole tether mechanic seems a little pointless as it stands" and "the captain is still useless".

## New rules and why

- **Patience no longer drains.** Clipped on, the diver has unlimited air and the dive never ends by time. The drain was a timer on the player that the line itself did not explain. Kept as data: `AirLine.bPatienceDrains` (false) brings the old rule back. `Deep.SetPatience 0` still cuts with `cut_patience` (the S4-cut playtest relies on it).
- **The line is the air line.** Off the line (bitten, cut, unclipped) the existing bailout timer (`Bailout.AirSeconds`, 30 s) runs and the diver dies if not back at the hatch in time.
- **The serpent bites the line.** An attacking serpent (Stalk or Bite) whose head comes within `AirLine.BiteTouchM` (0.6 m) of any joined link of the simulated rope cuts it once, through `CutTetherFor`, event `tether_bitten`: a snap sound row, a hard camera tug (shake row: 18 cm, 6 deg, 9 Hz, 0.7 s) and two Captain lines. Only attacking states bite (`AirLine.bOnlyAttackingBites`), so a lurk or a first-sight pass brushing the rope does not end the dive without warning. The old timed bite (5 s alarm, `cut_bite`) stays as the second path. So the lamp and the harpoon now protect the line: driving the serpent off is keeping the air.
- **Unclip by choice: T.** Free while swimming (WASD, Space, C, Ctrl, Shift, E, G, R, F, V, Q and the left button are taken). Bound from the tether component with its own mapping context `IMC_AirLine` made in C++. The diver goes off the line onto bailout air; the line stays whole, hanging from the hatch. Re-clipping at the hatch is free (`AirLine.UnclipReclipFee` = 0) and fires the plain `enter_sub`; a bitten or cut line still pays the re-tether fee. Q-hold cut is unchanged.
- Rule code is pure in `Sim/DeepAirLineRules` (`PatienceAfterStep`, `StateBitesLine`, `TouchedLink`, `ReclipFee`). Cheats in `Diver/DeepAirLineCheats.cpp`: `Deep.Unclip`, `Deep.AirLine.Bite [alpha]`.

## Old tests changed (because the rule changed what they assert)

- `Deep.World.RunLoop`: asserted `patience_50` fires after a 1 s step from 50.5%. Now asserts Patience holds and no warning fires.
- `Deep.World.MonsterLoop`: asserted Patience drains twice as fast during a bite. Now asserts it holds.
- `Deep.World.SubHatch`: asserted Patience runs in the water. Now asserts it holds.

## Tests

Build green (24 s). One gated run, 53 of 53 passed: `Deep.Sim.AirLine`, `Deep.World.AirLine` (6 new), `Deep.Sim.Rope`, `Deep.Sim.Rules`, `Deep.Sim.Monster`, `Deep.World.TetherDistanceAfterCut`, `Deep.World.SubReturn`, `Deep.World.RunLoop`, `Deep.World.Monster`, `Deep.World.Harpoon`, `Deep.World.SubHatch`.

## For other lanes

- Story: `Story/DeepCaptain.cpp` urgency table should rank `tether_bitten` 3 and `tether_unclip` 2 (lane could not touch it; both default for now). Lines are in `CaptainLines.csv`.
- UI: the HUD has no key hint for T yet, and the Patience bar should now hide or read as "air line" while clipped on.

## Not verified

Multiplayer (the unclip RPC and `bUnclipped` replication are written but not run with two clients), the input key in a live session, the sound and shake in play, playtests.
