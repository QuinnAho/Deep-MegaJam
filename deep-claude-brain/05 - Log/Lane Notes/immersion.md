# Lane: immersion, first pass (D-072, from Nov 25)

Quinn, Nov 25, going to sleep with the lanes running: "always be thinking about immersion dont be afraid to add polish". This lane adds signs of life and pressure to the cave run. It starts from `build/drive` once D1 has reported, because its hooks sit in the new drive code.

Every item follows the [[Style Guide]] (light from sources, no decorative glow, no saturated surfaces). Each one also gets the rule in CLAUDE.md: look up Epic's skill or documentation for an effect first, write the recipe down, then build it once. Quinn sees frames before anything merges, and sound changes come with the paths to the sounds. The audio is generated in code until Colby's files arrive. Every sound is one row in `deep/Content/Data/Sounds.csv`, so a real file can replace it later.

## I1
1. **The hull speaks.** A steel pressure hull creaks and groans.
   - Short creaks (a `hull_creak` event with three variations, each with pitch jitter) come more often and pitch lower the deeper the sub is. They also come on hard turns and while the depth changes, so the pressure reads.
   - A long, low groan plays once when the sub first passes each 100 m of read depth.
   - A soft flow noise (a loop) rises with speed and falls to nothing at rest. It sits under the idle hum the drive lane adds.
   - They play on the pilot's machine and in the room, through the existing buses.
   - The creaks are rarer in the room at the surface stop, so they don't wear thin.
2. **Silt.** In flooded caves, silt is the thing that steals your sight.
   - The hull lifts it when it runs within about 1.5 m of the floor, or scrapes rock. The diver lifts a little when kicking near the floor.
   - The clouds rise slowly, spread, hang for 10 to 20 s, and drift. They catch the floods and the lamp, and cut the view for a while.
   - They never appear inside the hull or through rock.
   - Use Niagara if it can be authored reliably from script. Otherwise use code-driven instanced puffs like the marine snow in `ADeepEnvironment`, which the cook already handles.
   - The GPU budget is +0.5 ms with several clouds up, and the packaged build must draw them.
3. **The sonar's echo.** The ping's sound comes back as an echo.
   - Its delay and level follow the distance to the walls the sonar's traces hit: short and dry in a tunnel, long and faint in a cavern. A cavern sounds big before the lights show it.
   - Use the sonar's existing traces, and add no new traces per frame.

Not in I1:
- creatures and other life;
- propeller spin (BT-01 is one mesh);
- bubbles (parked since the jam);
- the HUD's look.
These are candidates for I2, once Quinn has heard and seen I1.

**Tests:**
- Creak timing follows depth (more often deeper) and is never closer together than a minimum gap.
- The groan plays once per band.
- Silt rises only near the floor or on a scrape, never inside the hull's box, and its count stays bounded.
- The echo's delay grows with the traced distance.
- With the caves off, the shaft game is unchanged.

**Frames and sounds:**
- frames of a silt cloud from the chase view, lifting and settling over about 10 s;
- the same view before and after in T1's squeeze;
- WAV exports of the creak, the groan, the flow noise and the echo in a tunnel and in a cavern, using `Deep.Audio.Export <builtin> <wav>`.

## Silt: the recipe (read before building, Nov 25)
Sources read:
- Epic's Niagara skills `fundamentals.py` and `renderers.py` (`Engine\Plugins\Experimental\Toolsets\NiagaraToolsets\Content\Python\skills`): Niagara is authored from templates through the editor's toolsets, and "Opening the editor is necessary for the errors to surface". Not reliably authored and verified from a headless script, so the lane note's fallback applies: code-driven instanced puffs, cooked with `/Game/Deep`. The same skill's material rules still hold: prefer the minimal usage flags, translucent only where soft blending is needed.
- Epic, [Lit Translucency](https://dev.epicgames.com/documentation/en-us/unreal-engine/lit-translucency-in-unreal-engine) and [ETranslucencyLightingMode](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/ETranslucencyLightingMode): Volumetric NonDirectional is the mode for smoke and dust, lit from the translucency lighting volume, which takes shadowed direct light from every movable light. So the lamp and floods light the silt and nothing else does (style guide 2).
- Epic, [Depth material expressions](https://dev.epicgames.com/documentation/unreal-engine/depth-material-expressions-in-unreal-engine): Depth Fade hides the seam where a translucent mesh meets opaque geometry.
- Breakdown: [pseudo-volumetric smoke from sphere meshes (80.lv)](https://80.lv/articles/unity-made-pseudo-volumetric-smoke-optimized-for-low-end-mobile-devices): spheres, inverted Fresnel for a soft edge, depth fade, scrolling noise. Epic's old [Fog Volumes](https://docs.unrealengine.com/udk/Three/FogVolumes.html) note gives the same soft-edged sphere.
- Engine source: `LocalFogVolumeComponent.h` and `LocalFogVolumeRendering.cpp` (local fog volumes start 10 m from the camera by default and cap at 32 a view): ruled out for clouds next to the chase camera.

Recipe as built: each puff is one instance of the engine sphere (ISM, no collision, no shadow) in `M_SiltPuff` (`Content/Deep/Immersion`, built by `build_immersion_materials.py`): translucent, Volumetric NonDirectional, opacity = facing^EdgePower x slow world-space lumps x the instance's own opacity (PerInstanceCustomData 0, from its age) x Density x a fade within 1.5 m of the eye, then Depth Fade 90 cm into rock. The albedo is grey-brown at 0.44 (value under 70%, saturation about 20%). An A/B with Surface ForwardShading (`M_SiltPuffFwd`, kept for comparison, `Deep.Silt.Material fwd`) drew hard round balls and caught ambient light in the dark, so the volume mode ships.

## Log
- Nov 25, about 4:10 AM: lane planned by the orchestrator. It waits for D1's report.
- Nov 25, about 5:00 AM: `build/immersion` cut from `build/drive` with main merged.
- Step 1, `282d102`: rules, sounds, silt and echo, tests `Deep.Sim.Immersion.*` (6) and `Deep.World.Immersion.*` (3). The ping is dry now (`sonar_ping_dry`); its returns are `sonar_echo`, timed by the walls the sweep traced.
- Step 2, `e8ecbbc`: the first frames showed no silt. The cave chunks have complex collision only, so the probes trace complex now. Lifted puffs are thrown aft by the wash. The A/B picked the translucency-volume puff, made brighter.
- Step 3, `bf41290`: in T1 the hull knocked the walls but shed nothing, because the probes looked out from the middles of the box's faces. Now they go round the section at bow, middle and stern. A hull pinned on rock with its thrusters pushing still blows silt off.
- Measured: 20 minutes still gives 46 creaks at 100 m and 172 at 600 m, never closer than 2.5 s. The echo off T1's walls is 1.9 m at 0.09 s and 8.2 m at 0.17 s. C1's cavern at its berth gives 18 m at 0.32 s and 21 m at 0.36 s, and S's berth 18 m and 30 m (0.31 and 0.48 s).
- Nov 25, about 6:15 AM: gates on `aba5f70` (main merged). verify 232/232. Playtests: S1-hatch and S13-caves passed; S7-run passed on its second run (the first editor never answered on port 8015). Package 1151.8 MB, smoke clean, zip 863.4 MB, no usage-flag warnings. The packaged game draws the silt (`packaged-T1-outside.png`). GPU A/B (`Deep.Silt.Hide`, 1280x720, side view of the cloud at S): 5.56 ms hidden against 5.92 ms with about 35 puffs on screen, so +0.36 ms. Whole frame 6.26 against 7.65 ms; the extra 1.4 ms was not traced. Frames and WAVs are in `deep\Saved\Harness\immersion\`.
- For Quinn: in the chase view the silt shows mostly in squeezes and when you turn back into your own trail. The floods face ahead, away from what the hull lifts. From outside, the clouds read clearly. Up close the puffs can still look like soft balls. A cavern echo comes back at about 0.32 s at C1's berth; `Echo.DelayPerM` makes caverns sound bigger.
