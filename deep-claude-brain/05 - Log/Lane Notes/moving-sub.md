# Moving sub: plan (undo D-022)

## Actors
- **`ADeepSubmarine`, the hull.** The server moves it and it replicates. It carries the shell collision, the dress, the floods, the bow lights, `HelmCamera` and the hatch with the tether anchor.
- **`ADeepSubRoom`, new, the fixed set.** It holds the deck, the console, the seat, the levers, and the room's dress and lights. Still, at `Sub.RoomSetLocationM` (300 m above the depth datum).
- **`ADeepStopPoint`, new, replaces `ADeepArenaRoot`.** Placeable: where the hull parks at stop N; that stop's bed, cavern shell and salvage hang from it. A map without one gets three, at Z = -depth. The shaft is placed once.

## Sim
- `StepMotion`, `StepNotch`, `StepBallast` and `HelmStep` do not change; they now work in world space. In each 0.05 s step the server sweeps the hull and clamps its Z to the gate limit.
- `SubDepthM`, `SubOffsetM` and `SubYawDeg` stay replicated but are only derived from the hull. Their ~200 readers stay.
- **Park:** near a stop point, the hull glides onto it.
- **Cheats:** `Deep.GotoStop` teleports the hull. `Deep.Drive` flies the generated cave's `CentreM`.
- **New pure rules** (`Deep.Sim.MovingSub.*`): `DepthFromZ`, `ResolveWallHit`, `ParkNear`, `RoomSway`, `AirlockPhase`.

## Collision
- **Shape:** the hull's root is a box of `GetSolidBoundsLocal`. It blocks WorldStatic and ignores pawns.
- **Turning:** a turn happens only if an overlap test at the new heading is clear. Otherwise the hull scrapes.
- **Damage:** `ResolveWallHit` gives the impact speed and the bounce. Existing `HitDamage`/`DentHull`, scrape and breach do the rest.

## Fixed room
- The room is on lighting channel 2, so the cave lights skip it.
- On each machine, `ADeepEnvironment` eases to a Room look (thin fog, no volumetrics, no snow) while the local view is in the room. Fades hide the switch.
- `RoomSway` fakes motion from the helm speed and the hull's acceleration: camera sway, swinging caged lights, hum, creaks, and a shake on a knock. The room itself never tilts.

## Airlock
- **Out:** E at the room's hatch plays a hiss (`airlock_cycle`) and fades out. The diver moves under the hull's hatch, `StartDive` runs, and the view fades in.
- **Back:** `ReachedHatch` runs the cycle in reverse into `EnterSub`.
- **Fade:** `StartCameraFade` on the client, timed by `FDeepAirlockTuning`. Cheat: `Deep.Airlock`.

## Files
- **Readers, unchanged:** Audio, Tether, HudCalm, HudFrame, SDeepHud.
- **Moved to `FindRoom`:** DeepHUD, GameMode, HelmLevers.
- **Writers, rewritten:** RunComponent, GameState, Cheats, StateReport.
- **Placement code removed:** Arena, ArenaRoot (deleted), Shaft, AbyssFloor (one per stop), Cavern, Dressing, Environment, SerpentController.
- **Split:** Submarine (loses the trench) and SubRoom.
- **Tuning:** CaveRules, TuningSettings, TuningTypes.
- **Tests:** Pilot, Cave, Sub, CameraHull and Cavern are rewritten. Others need path edits.

## Commits (each green and playable)
1. Pure rules, tuning and tests. Behaviour unchanged.
2. Room split and airlock, with D-022 still on.
3. The still world behind `Deep.World.Still` (off): stop points, beds at their depth, the swept hull.
4. Switch on, tests fixed, one playtest.
5. Delete D-022 code: the root, the moving `Follow`, `StepShaft`, the trench. Update ARCHITECTURE.md and log a decision.
6. Authored maps: placed stop points turn the generated cave off; a warning if one is off its tuned depth. The hull gets a `WorldPartitionStreamingSourceComponent`.

## Deferred
- Real openings through the beds. Until then a bed hides once the sub leaves its stop.
- A live window view.
- Client prediction for the hull.

## Risks
- The seated view may jitter on clients.
- Streaming must follow the hull, not the pawn.
- Procedural collision cooks asynchronously.
- About 13 test files assume the moving root.

**Size:** about 24 h.
