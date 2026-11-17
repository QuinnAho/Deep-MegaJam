# Lane: sub inside, the room fitted to BT-01 (Nov 17)

Branch `build/sub-inside`, worktree `.wt/subs`. Brief: fit the code-built room (`ADeepSubRoom`) to BT-01's inside, keep it in code, re-lay the dressing, keep the airlock, console, helm and helm camera working. Style: [[Style Guide]].

## BT-01 measured

- `deep/Content/Deep/Room/measure_bt01_inside.py` (headless, read-only on `SM_BT01`) traces the inner hull by line casts.
- The cabin: x 40..476 cm, y ±90, headroom about 206 (161 in the cockpit), a free aisle of only 60-80 cm.
- **Not usable as a walk space.** Two 70 cm capsules cannot pass, and the curved floor snags a walker. Frame: `devlog/media/2026-11-17-inside-bt01-cabin.png`. The room stays a code-built box sized from BT-01's inner section.

## The fitted room

- 4.9 x 1.8 x 2.1 m inside (was about 8 x 4 m): X -100..390, Y ±90, deck -105, ceiling +105, walls 15 cm, in room-local cm with the round hatch's centre at the origin (lined up with the hull's hatch along X).
- New `Inside*` fields in `FDeepSubTuning`. `World/DeepRoomLayout` turns them into boxes (inside, console, helm panel, seat, levers, porthole, hatch lamp post) that the room, the dressing, the helm levers and the tests all share. `Room*` tuning now sizes the hull's box only.
- Round hatch, 1.1 m across: a non-solid cover plus rim ring and three dogs. The valve is at its centre.
- Stations (m): valve (0, 0), enter stand (0.95, -0.3), start (1.7, -0.15), console (1.2, 0.45) against the starboard wall, helm (3.4, 0) at the bow under the porthole, levers at (3.1, ±0.5).
- **E goes to the nearest station in reach** (`ADeepSubRoom::GetStationInReach`). In a room this small, the console and the valve are both within 1.8 m. The old fixed order (helm, console, valve) made E at the valve open the console, which `Deep.World.Still.Airlock` caught. The HUD prompts use the same rule.
- Dress re-laid for the room: 4 ribs, 3 port pipe runs, the valve drop, a gate valve, a starboard cable tray, gauges, 2 lockers on the aft wall (the bench is gone), the ladder to a ceiling scuttle, 5 caged fixtures and a port strip.
- New cheat `Deep.Room.Roof 0|1` hides the ceiling for a plan view.

## Tests

- `Deep.World.SubRoomLayout` was rewritten:
  - the hatch is at least 1 m across;
  - every station is off the hatch and inside the room, with the walker's radius (35 + 3 cm) to spare;
  - no walk between stations crosses the hatch;
  - start, enter, console and valve are within the use range of each other, and the helm is furthest forward;
  - **a pass scan:** the widest free strip across the room is 164 cm at x 12, against 152 needed for two walkers (D-057);
  - the eye (deck + 135) is below the ceiling by more than the near clip, and no dress piece comes within 10 cm of it.
- `RoomDress.ChecksBite` gains hatch-edge and lever checks.
- `Deep.World.Sub` checks that E at the valve is the valve's and E at the console is the console's.
- First run: `RoomDress.InsideHull` failed because the top pipe's flanges poked 0.5 cm into the ceiling (lowered 2 cm). `Still.Airlock` and `Still.RoomLook` failed on the E order (fixed as above).
- Green: SubRoomLayout 1, RoomDress 4, Still 7, MovingSub 6, World.Sub 12, World.Pilot 7. `Deep.Sim.Sub` matches no test, so `Deep.World.Sub` was run instead.
- Playtests: `S1-hatch` passes. `S12-sidehome` passes, coming home 5 of 5 times to the enter stand at (95, -30..10). Its first try failed only because the editor had not come up.

## Frames (1280x720, for Quinn)

`devlog/media/2026-11-17-inside-{start,console-helm,helm-aft,hatch,plan,bt01-cabin}.png`:
- start and console-to-helm: the low corridor to the helm panel under the porthole, with the levers either side;
- helm-aft: the console on the starboard side, the round hatch, the lockers and the ladder aft;
- hatch: the round cover, the dogs and the red lamp post;
- plan: `Deep.Room.Roof 0` from above. It is murky because the eye is outside the room's exposure volume.

## Open for the look pass

- The lockers and dress have no collision, so a capsule can push about 22 cm into a locker. The camera stays at least 13 cm off, so it never clips.
- The start transform puts the capsule 12 cm above the deck, and it drops on spawn.
- The pipe flanges overlap where the runs are close.
- No old-versus-new frame: the old room only exists at `fd465e4`.
