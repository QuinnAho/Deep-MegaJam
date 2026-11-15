---
updated: 2026-11-15 13:15 EST
by: camera lane (branch polish/camera)
---
# Camera: footage look and sonar pulse recipe

How the camera lane builds its post-process effects, and the sources each step came from. The footage look, the sonar pulse and the feed interference ship off until Quinn approves frames (switches below). Style: [[Style Guide]] (amber #FFB45A only on machines that work; darkness is the design; no overlays).

## Sources read first
1. Epic's built-in skill `material_basics` (`Engine/Plugins/Experimental/Toolsets/EditorToolset/Content/Python/editor_toolset/skills/material_basics.py`): reuse before you create, expose per-instance parameters, never guess node property names (list them; pins are wired, not set). No existing material in the project or the engine does either effect, so two new ones.
2. Epic, Post Process Materials: https://dev.epicgames.com/documentation/unreal-engine/post-process-materials-in-unreal-engine . After Tonemapping is the preferred blendable location for simple effects (cheapest, display resolution). Blendable Priority orders materials at one location. Scene Texture node gives SceneDepth, CustomDepth, WorldNormal, PostProcessInput0. World-position effects can alias with temporal upscaling. Custom depth is per primitive ("Render CustomDepth Pass").
3. Engine header `Runtime/Engine/Classes/Engine/BlendableInterface.h` (5.9): `BL_SceneColorAfterTonemapping` runs at display resolution on display-encoded colour; `BL_SceneColorBeforeDOF` is linear, at rendering resolution.
4. deepspacebanana, post-process scan effect: https://deepspacebanana.github.io/blog/shader/art/unreal%20engine/Post-Process-Scan-Effect . The classic scanner: distance from an origin to the pixel's world position (rebuilt from depth) compared with a growing radius; a sharp front with a trail behind; a grid from world position for the "data" pattern.
5. Unreal Possibilities, scan pulse experiment: https://unrealpossibilities.blogspot.com/2026/04/unreal-engine-experiments-creating-scan.html . Drive the radius from code through a dynamic material instance, not a timeline in the material; set weight 0 between pulses.
6. Engine `Scene.h` (5.9): `FPostProcessSettings` grain (`FilmGrainIntensity`, `FilmGrainIntensityShadows`), `VignetteIntensity`, `SceneFringeIntensity`, `ChromaticAberrationStartOffset`, `MotionBlurAmount`, and `WeightedBlendables` (`FWeightedBlendable{Weight, Object}`).

## Footage look (`Deep.Look.Footage`, default 0)
- Stacks on the depth look in `ADeepEnvironment::ApplyLook`. Grain adds 0.25 at 100 m to 0.55 at 500 m (`FootageGrainAdd`), with shadow grain 1.2 to 1.8 so it lives in the dark. Vignette +0.2. Fringe +0.9, starting at 0.45 of the way to the edge, so the centre stays clean. Motion blur 0.2.
- No exposure, fog or brightness change, and no text, REC dot or other overlay.
- Lens: `M_FootageLens`, after the tonemapper, priority 10 (runs after the pulse so the overlay bends with the picture). ViewportUV goes through `0.5 + c*(1 + K*r^2)/(1 + K*0.5)` with K 0.06 (`FootageBarrel`), then samples PostProcessInput0. Dividing by the corner value pins the corners, so no black border appears. It is one texture read.
- Sway: the step-1 Perlin shake `Sway`, constant while the switch is on, under a degree.
- Cost: 1707x1067 at 100 m, GPU 7.95 ms off and 8.04 ms on; at the helm while driving, 8.14 ms off and 8.37 ms on. The look costs about 0.1 to 0.2 ms.

## Sonar pulse (`Deep.Sonar.Pulse`, default 0)
- Each `sonar_ping` on the bus (the sonar broadcasts it, with Source as the sub and DepthM as the range) starts a pulse. `UDeepCameraSubsystem` drives a dynamic instance of `M_SonarPulse` and hangs it on the environment's unbound volume through `SetCameraBlendable`. The weight is 0 between pings.
- In the material, after the tonemapper, priority 0, one Custom node:
  - `d = |WorldPosition - Origin|`. Sky (depth over 1e6), beyond the range, or within `PulseStartM` 6 m of the sub (the room itself) is returned untouched.
  - Band: in front of the radius, a hard edge over 0.35 × band. Behind it, `exp(ahead/BandCm)` with a 0.6 m band. It fades out over 0.3 s once the shell reaches range at `PulseSeconds` 1.5 s.
  - Dots: a world grid at 0.35 m on the surface's dominant axis (triplanar from WorldNormal). The dot radius grows with depth so the dots stay about a pixel wide far away. Each dot lights as the shell passes, fades over `PulseDotFadeS` 2 s, and fades with distance from half of `PulseDotFarM` 40 m to all of it.
  - Serpent: the camera sets Render CustomDepth on `ADeepSerpent`'s primitives. Where custom depth is the nearest surface the dots are masked, so only the band crosses it.
  - Out: `scene + amber × (band×0.9 + dots×0.35)`. Added on top of the colour, so it lights nothing: no light, no GI, no exposure.
- Colour: after the tonemapper the buffer is display-encoded, so the amber goes in as its raw sRGB value (`FColor(FF,B4,5A)` reinterpreted, not converted to linear).
- Cheat: `Deep.Camera.TestPing [rangeM]` draws a pulse from the sub even with the switch off.
- Cost: at the helm at 300 m (1707x1067), GPU 8.23 ms with the pulse off and 8.47 ms with a pulse every 2.5 s. About 0.25 ms while a pulse is live, 0 between pulses (weight 0).
- Seen in frames: the cavern wall around the sub lights as one ring when the shell meets it, because the wall is about the same distance away all round. Dots on a flat floor at a grazing angle make faint moire lines; a larger `PulseDotSpacingM` or a smaller `PulseDotFarM` thins them.

## Feed interference (`Deep.Look.Interference`, default 0)
Sources:
- Epic community tutorial, VHS post process: https://dev.epicgames.com/community/learning/tutorials/6Vk8/unreal-engine-vhs-post-process (forum thread https://forums.unrealengine.com/t/community-tutorial-vhs-post-process/667290). Scanline offset by noise per line, per-channel offsets, noise mixed into the scene texture.
- 80.lv, making a VHS effect in Unreal: https://80.lv/articles/tutorial-making-a-vhs-effect-in-unreal-engine
- Forum, SOMA-style glitch: https://forums.unrealengine.com/t/how-can-i-replicate-this-glitch-effect-from-soma-in-a-post-processing-material/1720406 . The glitch is UV manipulation, and the chroma split is per-channel UV offsets.
- Forum, Retro Vision VHS/CRT: https://forums.unrealengine.com/t/bricedev-retro-vision-pro-vhs-crt-vfx/2685942 . Tape jitter is horizontal jitter by scanline.
- Engine `MaterialTemplate.ush`: the SceneTexture UVs input takes a viewport UV and converts it (`ViewportUVToSceneTextureUV`) and clamps it, so torn UVs never read outside the view.

Recipe:
- Strength S (0..1) comes from `DeepCameraRules::InterferenceTarget`: 0 at `InterferenceFarNearM.X` 30 m, rising to `InterferenceNearMax` 0.6 at 4 m (the nearest serpent head to the camera); 1 in a monster-brain Bite; at least `InterferenceBurst` 0.8 for `InterferenceBurstS` 0.5 s after `hull_hit`, `hull_critical` or `hull_breach`.
- `InterferenceStep` eases toward it at `InterferenceEasePerS` 4 per second falling and 16 per second rising. The danger lands at once and leaves slowly.
- The material is after the tonemapper at priority 20, so it runs last and breaks the pulse and the lens with the picture.
  - FeedUV: bands of `Bands` 70 rows. Each band hashes per 1/24 s and some tear sideways, by up to `Tear` 0.03 of the width; more bands tear as S rises. A rolling band wobbles. The shift scales with S², so it is calm at low strength.
  - Red and blue are read at ±`SplitUV` (`InterferenceSplit` 0.006 × S), green at the torn UV: three reads.
  - Snow at 480x270 per 1/30 s: `lerp(c, c*n*2 + n*0.03, S*Static)`. It multiplies, so black stays black. Scanlines darken by up to 20%. About one dim dropout line a frame at full strength.
- Hash: Hoskins (`frac(p*0.1031)`, `dot` with `yzx+33.33`). The `frac(sin(dot))` hash streaked diagonally across the snow at large inputs (seen in the first frames).
- Weight 1 only while S > 0.001, and 0 otherwise.
- Cost: at the helm at 300 m (1707x1067), GPU 8.15 ms idle and 8.45 ms at S = 1, about 0.3 ms while live.
- Not covered: the Monster Brain runs on the server, so a client sees only the distance part, never the bite maximum. `hull_hit` is not broadcast by game code yet.

## Rebuild
`deep/Content/Deep/Camera/build_camera_materials.py`, run headless inside the slot:
`UnrealEditor-Cmd.exe deep\Deep.uproject -run=pythonscript -Script=<abs path> -unattended -nopause -nosplash`.
The log prefix is `DEEPCAMMAT`. The first build gave M_SonarPulse 21 expressions and M_FootageLens 4, with 0 compile errors.

## Risks
- With TSR and upscaling, the world-position band can shimmer on thin geometry (Epic's warning). The band is 0.6 m wide, which is wide enough not to.
- A custom-depth pass is added only while the serpent is in the world. Its cost was not measured separately.
