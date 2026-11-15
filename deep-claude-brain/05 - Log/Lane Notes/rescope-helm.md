# Lane A: rescope/helm (real sub controls)

Branch `rescope/helm`, from `cee150a`. Quinn's ask: "pilot the sub around all axes... real sub controls with a ballast lever".

## What was built and why

- **Seated controls** (`Diver/DeepDiver.cpp`):
  - W/S step a six-notch throttle (full astern, half astern, stop, slow, half, full ahead). One notch per press, and the lever stays where it is left. A held key is one press, so the player can count notches.
  - A/D hold the rudder. The turn rate eases in and out over about 0.6 s.
  - Holding C floods the ballast lever and holding Space blows it. The lever stays where it is left and holds 0.35 s in a neutral detent as it passes centre.
  - X is all stop: throttle to stop and ballast to neutral. E still stands up.
- **Pure rules** (`Sim/DeepPilotRules`): StepNotch, StepBallast (with the detent), StepMotion and the park check.
  - StepMotion: surge eases toward the notch speed, sway decays fast (low drag along the heading, high sideways), heave eases toward ballast × HeaveSpeed, and the yaw rate eases toward rudder × rate.
  - The rest: ShouldPark, GlideStep, GlideYawStep, TurnFreeBoxM and HangLocationCm.
- **The world turns, the sub does not.**
  - The replicated `ADeepGameState::SubYawDeg` drives the arena root's rotation: a pure yaw of -SubYawDeg, identity at 0.
  - The shaft's `if (bOn)` placement block turns the same way.
  - Both are proven identical to the old placement at yaw 0.
  - Caverns and dressing are attached to the root, so they turn with it. The sonar reads root-relative positions, so it is heading-up.
  - The serpent is not attached to the root and is left as it is.
- **Walls**: `UDeepCaveRules::Contact` is unchanged.
  - It is fed `TurnFreeBoxM`, a square whose corners lie on the hull box's circumcircle. That makes the contact the same at any heading, and a turn in a tight bend never pushes the sub into rock.
  - Impact speed is the larger of the velocity into the wall and the penetration rate, measured before the bounce.
  - A knock dents the hull (HitDamage/DentHull, with the cooldown), trips the throttle to stop and keeps half the along-wall speed.
  - The old direct loop (still used by the autopilot, Deep.Drive and tests) now measures impact the same way. This was the coordinator's ask, after Lane B's rock test read 0.8 m/s for a 6 m/s ram.
- **Parking**: the sub parks near a stop (within 5 m) when it is slow (under 1.2 m/s), the throttle is at stop, the ballast is neutral and the rudder is centred.
  - `Park(Stop, true)` keeps all of Park's events.
  - `StepParkGlide` then glides offset, depth and yaw to the stop frame in about 1-2 s.
  - The hatch opens only once the sub has settled.
  - Ballast cannot take the sub past `DepthLimitM()`, the gate limit, and `helm_depth_limit` fires on the rising edge.
  - Leaving the helm is all stop, and it parks if the sub is close enough.
- **Levers** (`World/DeepHelmLevers`): a runtime component on the sub, created on every machine by `DeepHelmLevers::Ensure` from the game state tick.
  - Two levers stand beside the seat: throttle inboard and ballast outboard. Each is a base block, an arm and a knob, built from engine shapes in `DeepShapes::Steel`.
  - The arms follow the replicated lever state. A notch change, or the ballast crossing neutral, clicks (`helm_take`).
- **Kept**: the autopilot, Deep.Drive and Deep.GotoStop (the direct path runs whenever any of them is active).
- **Cheats**:
  - `Deep.Pilot.Throttle <+1|-1|set n>`, `Deep.Pilot.Ballast <v>`, `Deep.Pilot.Rudder <v>`
  - `Deep.Pilot.AllStop`, `Deep.Pilot.Yaw <deg>`, `Deep.Pilot.Run <s>`
- **DEEPSTATE keys**: `sub_yaw_deg`, `throttle_notch`, `ballast`, `helm_speed_mps`.

## Numbers (FDeepPilotTuning, under Pilot in UDeepTuningSettings)

| Group | Values |
|---|---|
| Notch speeds | astern -2.0 / -1.0, stop 0, ahead 1.33 / 2.67 / 4.0 m/s (even fractions of full) |
| Surge and sway drag | surge 0.45/s, sway 2.5/s |
| Rudder | 28 deg/s at full rudder, response 1.6/s (two seconds of rudder: about 40 deg) |
| Ballast lever | 1.2 per s, detent 0.35 s, snap 0.12 |
| Heave | 2.6 m/s at a full lever, drag 0.55/s: 100 m at full flood takes **40.3 s** (tested; the brief asked 40-60) |
| Park | near 5 m, under 1.2 m/s, glide 2.5/s (measured 1.10-1.85 s) |

## Failures and fixes

- The Park and Ballast world tests started at stop 2 and mid stop 1/2. Without a gate, the limit is 200 m, so the clamp stopped the sub short. I moved them to stop 1 and to stop 1 + 50 m.
- The yaw-wrap test first asserted the wrong side of -180. It now checks that 350 deg glides up to 360.
- No python on this machine: edits went through perl scripts, and unix2dos for the CRLF files.

## Cuts

- No helm UI beyond the levers. The HUD is off-limits to this lane.
- The serpent does not turn with the world (it is not attached to the root).

## Gates

- Build: PASS.
- Tests, filters Deep.Sim.Pilot + Deep.World.Pilot + Deep.World.Sub + Deep.World.Helm + Deep.Sim.Cave + Deep.Sim.Rules + Deep.UI.Sonar + Deep.World.Sonar: **53 run, 53 pass**.
  - The first run was 52/53; the yaw-wrap test was fixed and the pilot tests rerun at 13/13.
  - No old test needed a change.
- After the impact fix and the X action, the same 8 filters ran again: **53/53**.
- Playtest S8-keys (port 8021): **PASS** (61 s).
  - Every helm key was checked once: C floods the lever (1.00) and it stays, which gives 13 m of descent in 6 s at 2.54 m/s; X brings the ballast and throttle back to 0; W, W, S, S steps the throttle 1, 2, 0; holding D for 2 s turns the sub 48.7 deg.
  - The descent to stop 2 is then `Deep.GotoStop 2` (setup), and the sub parks at 300.0 m.
  - The extras pass as before. The Esc pause finding is old: the input tool skips InputKey.
- Frame: `devlog/media/2026-11-15-rescope-helm-levers.png`. Both levers stand beside the helm seat, throttle inboard (pale knob) and ballast outboard (red knob).
  - It was taken standing (`Deep.Teleport 230 40 -68`, `Deep.Look -30 41`). Seated, the view is the helm camera under the hull, so the levers cannot be seen from the seat.

## Findings for the coordinator

- **An unsteered descent wrecks the sub.** In the first S8 run, C was held from stop 1 with no steering. The shaft bends swept into the hull: knocks at 1.6-5.8 m/s from 153 m down, and hull damage hit 10 by 240 m.
  - With real controls the bends must be flown. The old S8, "hold C and release at 296 m", cannot pass any more, so S8 now checks each key and leaves the descent to the autopilot.
  - Before the 90-second test, decide one of: a sonar or HUD cue for the bend ahead, a gentler impact from the wall sweeping past (the penetration rate), or a light auto-centre while the throttle is at stop.
- **Seated, the player cannot see the levers.** The helm camera is under the hull. Lever state needs a HUD readout (UI lane), or the levers need to be in the helm camera's view.
- **X was not seen.** The seated code read `IsInputKeyDown(X)`, and keys injected by the input tool never reached it. X is now an Enhanced Input action (`IA_AllStop`) in `ADeepPlayerController`, like every other key. This is a minimal change outside the lane.
- **The old direct loop now measures impact the same way as the pilot loop**, as max(velocity into the wall, penetration rate), before the bounce. The coordinator asked for this after Lane B's rock probe.
