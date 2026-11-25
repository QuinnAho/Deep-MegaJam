# Lane: drive like a game sub, and see (D-070, from Nov 25)

Quinn drove the caves on Nov 25 and said: "change the sub driving mechanics to be someting more like a traditional video game sub its super hard to see and control ... you barely can see anything". He said "go" to D1 below at about 2:05 AM. The research and the orchestrator's read are in [[Sub Driving and Seeing - Approaches]].

The branch is `build/drive` in `.wt\cave`, cut from main `06deef1`. To turn the caves on, use `Deep.World.Caves 1` (it's off by default until C2, D-068). Another lane is building the rock material at the same time on `build/cavestop3` in `.wt\cave3`. Don't touch `deep/Content/Deep/CaveGen/`.

## What exists (verified)
- **Helm rules:** `Sim/DeepPilotRules` has the throttle notches (`StepNotch`), the ballast lever (`StepBallast`), `StepMotion` (surge, sway, heave and yaw with inertia) and parking.
  - The tuning is `FDeepPilotTuning` (`Sim/DeepTuningTypes.h:2573`): 4 m/s ahead and surge drag 0.45/s, so it coasts for seconds. The rudder turns at 28°/s and builds at 1.6/s. Heave is 2.6 m/s, and a knock trips the throttle back to stop.
- **The run component** (`Diver/DeepRunComponent.cpp`):
  - `StepPilot` flies the sub from the replicated lever state.
  - `StepPilotCave` runs in the caves. Each substep sweeps the hull box (`ADeepCaveString::SweepHull`), and `UDeepMovingSubRules::ResolveWallHit` bounces it. The rest of the move slides, and a turn into rock is refused.
  - `StepRockToll` dents and scrapes. The depth move is clamped at `DepthLimitM` through the depth map, and `ParkNear` and `ParkInCave` park the sub at the berths.
- **Input:** `ADeepPlayerController` builds Enhanced Input with NewObject. The cheats are `Deep.Pilot.Throttle|Rudder|Ballast|AllStop|Yaw|Run`. The lever props are `World/DeepHelmLevers`.
  - The seated readout shows THROTTLE, BALLAST and HEADING (`DeepHudLayout::HelmLevers`), and the controls hint lists the old keys.
- **Camera:** `HelmCamera` on `ADeepSubmarine` (`World/DeepSubmarine.cpp`, around 300 and 580 to 670). It sits at `Sub.HelmCameraM` (5.2, 0, -2.6) m, pitched -32°, with a 95° field of view, plus sway, settle and jolt in `TickHelmLook`. The seated diver's view target is this camera.
- **Floods** (around line 541): two helm floods (`Look.HelmFlood*`) at 40000, a 44° cone and 30 m. The port one is pitched -30° and the starboard one -60°, with volumetric 0.15. They run at full strength while the helm is manned. The other outside lights dim with depth (`SetOutsideLights`).
- **The problem in frames:** `devlog/media/2026-11-18-m2b-hud-c1-helm.jpg` and `2026-11-18-m2a-s-ping-helm.jpg` show lit floor with black above it.

## D1: drive and see
1. **Direct drive.** This becomes the default. The lever model stays behind a switch (for example `Deep.Pilot.Mode levers`), for comparison only.
   - Keys are held, and nothing latches: W/S ahead and astern, A/D turn, Space up and C down, Q/E strafe. X can go.
   - Starting feel (tuning, not facts): full speed in under 2 s, and at rest within about 1.5 s of letting go. The turn rate builds in about 0.3 s, with a little drift so it feels heavy. Keep 4 m/s ahead unless driving shows it's too slow.
   - The hull stays level, with no pitch or roll in the sim.
   - With the option `Deep.Pilot.MouseSteer 1`, the sub turns and climbs toward where the camera looks while it thrusts.
   - Keep it server-authoritative: the pilot's input state replicates to the server, which steps the sub, as the levers do now (co-op, D-057).
2. **Rock (caves).**
   - The sub slides along rock.
   - Damage grows with the speed into the wall: a scrape costs little, a head-on knock a lot, and a breach stays possible.
   - A knock no longer stops the drive.
   - A turn into rock is no longer refused. The hull is pushed clear and keeps turning, slower if it must.
   - Keep the knock's shake and sound, scaled by impact.
3. **Camera.**
   - At the helm the default is a chase view, behind and above the stern. It follows the hull's own trail through the cave, so it sits in space the hull has just swept clear of rock. A probe covers turning in place and going astern.
   - It looks ahead along the travel, not at the nose.
   - The mouse looks around, and the view eases back behind after a moment.
   - It never shows the inside of rock or of the hull.
   - V switches to a cockpit view: level (0 to -5°), looking ahead, with mouse free-look within limits. Both views keep the jolt on a knock.
4. **Floods.** Aim them level and forward (0 to -5°).
   - Use a long, narrow throw (about 45 to 60 m) and a wide, dim fill for the walls.
   - Place them away from the camera's line, so the rock shows relief and the water shows less glare.
   - Make the beams visible (volumetric scattering).
   - They stay at full strength at every depth, as the pilot's lamp (D-070).
   - Change exposure or fog nowhere ([[Style Guide]]).
5. **Words on screen.** The controls hint and the seated readout show the new keys and what they need (speed, heading, depth). Change content only; the HUD's look waits for the look pass (D-028).
6. **The default game (caves off, the shaft) keeps working.** S1-hatch and S7-run must pass. Say in the report whether the direct drive also flies the shaft, or whether the shaft keeps the levers.

Not in D1: the centring assist, side proximity beeps, a sonar that lingers, the rock material (the other lane), and any map.

**Tests**, written first, by filter:
- **Direct drive** (a new `Deep.Sim.Pilot` prefix or similar): time to full speed, distance to rest after letting go, turn build-up, strafe, heave, and that nothing latches.
- **Caves** (`Deep.World.CaveDive`):
  - Sliding keeps most of the speed along the wall.
  - Damage grows with the speed into the wall.
  - A turn into rock isn't refused.
  - The lever mode's old tests still pass behind its switch.
- **Camera:** at sampled points along the cave path from S to E, with the sub on the path, the chase view is clear of rock and outside the hull. The cockpit view is level.

**Frames.** Take them in the game with the caves on, through `tools\ue.ps1 shot`. Use JPEG only for anything that goes into git (D-066).
- The same spots before (today's helm view) and after (chase and cockpit): S's berth, A's trench, T1's squeeze, C1's entry and its sink, and B.
- Then four frames of a drive through T1's squeeze from the chase view.
- Keep them under `Saved` and list their paths in the report.

**A build for Quinn.** Package once at the end. Copy it to `C:\Repositories\Deep-MegaJam\Builds\Drive-D1\` with a `Play-Caves.bat` like `Builds\CaveDive-M2b\Play-Caves.bat`.

## Log
- Nov 25, about 2:10 AM: the lane was planned, and the branch `build/drive` was cut from main `06deef1` in `.wt\cave`.
