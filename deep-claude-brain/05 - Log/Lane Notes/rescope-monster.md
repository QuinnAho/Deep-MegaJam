# Lane note: rescope-monster (lane G: it taunts, you hear it, it hears you)

Branch `rescope/monster`, worktree `.wt/dev`, Nov 15, 5 PM. Quinn: "teh monster needs to be taunting the player in order to make iti good make sure you can here it and it can hear you better".

## You hear it
- Three new `DeepSynth` built-ins, no assets: `serpent_drone` (6 s loop, two low sines beating under dark noise), `serpent_click` (a speeding click train), `serpent_call_a/b/c` (growl-to-moan glides, three contours). `Deep.Sim.Audio.Generators` covers them (length, peak under 0.6, loop seam).
- `ADeepSerpent` plays them **spatialised at the head** (`UDeepAudioSubsystem::PlaySpatial`: linear falloff, inner `VoiceInnerM`, falloff `DroneFalloffM` / `CallFalloffM` / `ClickFalloffM`), on every non-dedicated machine, from replicated state: the drone by `MoodThreat` (calm 0.6 to near 1.4), clicks every `ClickEveryS` (4 s calm to 1.3 s close), calls on each new replicated `CallSerial`. A different call each time, 3% pitch jitter, creature bus gain (mute, master, duck). Log: `DEEPMONSTER_VOICE {what, sound, distance_m, volume, at_ear, pitch}` per call.
- The old non-spatial `loop_serpent` still plays under it.

## It hears you
- `HearingRangeM` 150 to 260 m.
- The brain (server) turns bus events into hearing stimuli where they happen: `sonar_ping` 40, `harpoon_fired` 45, `hull_hit` 40 + 10 per m/s. The helm at full throttle adds 14 per second at the sub. Winch, sprint and grab are unchanged.
- A sound of `AnswerNoise` (20) or more: it turns toward it for `HeardTurnS` (3 s, while lurking) and answers with a call (`AnswerCooldownS` 6 s). Log: `DEEPMONSTER_CALL {why, kind, state, distance_m, volume}`.

## It taunts
- New state `Taunt` (appended last in the enum, so the replicated bytes do not shift). From Stalk, with taunts left, within `TauntStartM` (16 m) of the line: it goes to the edge of the light (9 m ahead and to one side of the diver's view), brushes past the line (never inside `BiteReachM` + 1 m of the bite point; the brush beat calls), and withdraws 26 m into the dark behind the far side. Then Stalk, with noise held at least at `StalkNoise`, so after the last taunt it strikes.
- `TauntCount` 2 per dive; after a recoil `TauntsAfterRecoil` 1 more. The lamp on it for 3 s drives it off (Recoil). Untethered: Hunt. Max 14 s per taunt.
- Taunt beats are diver-relative offsets in the arena root's frame, so they turn with the world while the sub is piloted.
- Gated like the first sight: player-controlled divers and the brain's own spawn only. Tests and cheat spawns keep the old rules.
- The air line bites only in Stalk and Bite (`StateBitesLine`), so a taunt cannot cut it. Log: `DEEPMONSTER_TAUNT {beat: start|edge|brush|withdraw|gone, taunt, left, distance_m, tether_m, drone_volume}`.
- The beam checks use `Behaviour.BeamHalfAngleDeg` (21, today's lamp value) instead of the lamp's cone.

## Cheats
`Deep.Monster.Taunt [n]`, `Deep.Monster.Call [0|1|2]`.

## Gates
Build PASS. Deep.Sim.Monster 14/14 (new: `.Taunt`, `.Hearing`), Deep.Sim.Audio 8/8, Deep.World.Monster 3/3. S3-serpent PASS on port 8027 on the second run (serpent present at stop 2, Lurk). The first run failed before the serpent: the game ran at about 5 fps with 1.5 GB free, and no diver appeared within 25 s. S11-pass and S10-relic were not run (slot time).

## Not verified
Not listened to by anyone (no agent can hear). Levels are numbers only. Playtests and the taunt frame: see the report.

## Frame
`devlog/media/2026-11-15-rescope-monster-taunt.png`: stop 2, lamp on, the serpent placed 9 m out and 3.5 m right with `Deep.Monster.Place`. Its body lies along the upper edge of the beam. This is a staged pose. A live taunt was not caught on camera, and the calls and taunt beats were not seen in a live run (no `DEEPMONSTER_TAUNT` or `DEEPMONSTER_CALL` in a game log yet).
