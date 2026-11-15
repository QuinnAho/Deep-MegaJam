# Lane: rescope/cave (generated cave)

Quinn (Nov 15): "what really is lacking is the environment ... we need to generate it whether that's with pcg or just some cave gen algorithm that you can tweak". Runtime C++ generation, not the PCG plugin: the shaft already rebuilds from tuning, and one rule can feed both the mesh and the collision.

## What was built
- **One shape rule, used by both mesh and collision.** `UDeepCaveRules::WallRadiusM(depth, angle)` gives the wall's radius at a depth and angle. Both `ADeepShaft::Build` (the mesh) and `UDeepCaveRules::Contact` (the helm's collision; signature unchanged) read it, so the rock you see is the rock you hit. It is built from:
  - Its own noise: a seeded integer hash, Perlin gradients and a quintic fade (`Noise3`). No engine noise is used, so a seed is the same cave in every build.
  - 5-octave fBm blended 60% toward ridged noise (`RockFbm`), sampled on the wall's cylinder. Depth is stretched x0.55, so ribs run along the shaft.
  - Strata: sawtooth ledges every 4.5 m, 1.8 m deep, warped by noise and broken up so they come and go.
  - Weight: full mid-stretch, 30% at the cavern mouths, none above the first stop.
  - A smooth max against the floor, `MinRadiusM + RockFloorMarginM` (0.6 m), so the rock never comes nearer the passage's centre than the sub needs.
- **Contact** samples the padded box's cross-section outline every `ContactStepM` (1 m, corners first) on 2..32 levels over its height, and returns the worst point with a radial normal.
- **Mesh**: 128 sides, 0.5 m rows (were 72 and 0.75 m). That is 249,344 triangles in 21 pieces, built in about 370-425 ms (mesh about 365 ms). The log line is `DEEPSHAFT built ...`.
- **Live tuning**: every number is in `FDeepCaveTuning` (Cave|Rock), with defaults in C++ initialisers.
  - `Deep.Cave.Rebuild [seed]` rebuilds the shaft and the caverns (in `World/DeepCaveCheats.cpp`; logs `DEEPCAVE rebuilt ...`, then DEEPSTATE).
  - `Deep.Cave.AutoRebuild 1` (the default) rebuilds by itself when a CRC fingerprint of the Cave numbers changes; it checks every 0.5 s in the shaft's Tick, so Project Settings edits show up while playing. A rebuild takes about 760 ms (shaft and caverns, synchronous cook).
- **Scatter**: 96 Dragon Cave rocks (`SM_CaveRock01-04`, `SM_LargeRock01-07`, 70 per 100 m, 2-6 m), seeded, half-buried.
  - Each rock's centre is set from the deepest wall over its span, so it stands out at most `ScatterProtrudeM` (0.45 m, clamped to HullPadM 0.6). The sub's padded box never reaches it; this is visual only, with no collision.
  - They are instanced static meshes attached to the shaft actor and hidden with their stretch.
- **Material** `M_CaveRock` (recipe below) is used on the shaft, the scatter and the caverns: `Cave.RockMaterial` comes first in `DeepCavern.cpp`'s material pick.
- **Flyable bends** (coordinator, 5:07 PM, for Lane A's inertial helm: full flood 2.6 m/s, rudder 28 deg/s): `CentreM` scales each stretch's bend so the centre drifts at most `BendMaxDriftPerM` (0.5 m per m of depth, so a bend sweeps at most 1.3 m/s at full flood). The scale is one number per stretch, so the centre stays smooth. It is measured on the raw curve every metre, aimed at 95% of the cap, and cached per thread. `RadiusM` adds `BendRoomM` (1.5 m) times drift/cap: more room where it bends. Proved by `Deep.Sim.CaveShape.Drift` (numbers under Numbers and tests).
- **Caverns**: build 4, noise on the cavern shells, was cut for time. The cavern shells already had outward-only relief, and now share the rock material and rebuild live.

## Failures and fixes
- HISM scatter crashed the test run on rebuild with `InstanceReorderTable.Num() == PerInstanceSMData.Num()` (an async tree build racing a component recreated under the same name). Two fixes:
  - Plain ISM: GPU-instanced in UE5, with no tree to race, and fine for 96 rocks.
  - `MakeUniqueObjectName` for rebuilt tube, scatter and cavern-shell components, so NewObject never reuses a destroyed one in place.
- `Deep.World.Cave.Hit` (an existing test) failed: the sub rammed the wall and scraped instead of knocking. A probe showed the cause is in the helm, not the rock:
  - `UDeepRunComponent` measures impact as `PenetrationM / Dt`. At 6 m/s the last substep before contact stopped 0.24 m short, the next one penetrated 0.04 m, and that read as 0.8 m/s against the 1.5 m/s knock threshold.
  - The round tube passed only because of where its substeps happened to land.
  - Fix (Diver/*, outside this lane): `ImpactMps = max(ImpactMps, max(Pen/Dt, dot(SteerVel, Normal)))` before the bounce. The orchestrator says Lane A's helm commit 1dcc0c9 on rescope/helm already does exactly this in its new pilot substep loop. The test drives the old StepShaft steer loop (SetHelmSteer then StepRun), which Lane A's loop replaces, so it is red on this branch alone and is to be rerun after A and B merge.

## Material recipe
`deep/Content/Deep/Cave/build_cave_materials.py` builds `/Game/Deep/Cave/M_CaveRock` headless (`UnrealEditor-Cmd ... -run=pythonscript -Script=<abs path>`, inside the slot).
- Result: 27 expressions and no compile errors. Readback shows tangent_space_normal=False, used_with_instanced_static_meshes=True and used_with_nanite=True.
- **Object-space triplanar.** The cave moves past the sub (the sub never moves, D-022), so the World Aligned functions would make the rock swim.
  - Position and normal: LocalPosition, and the vertex normal turned world to local with a Transform node.
  - Planes: X=pos.yz, Y=pos.xz, Z=pos.xy.
  - Weights: pow(abs(N), 4), normalised. One Custom node each for colour, normal and ORM.
- **Normals, UDN-style.** Each plane's tangent xy offsets the vertex normal along that plane's axes. The blend goes local to world (Transform node) into Normal, with tangent_space_normal off.
- **Tiling.** Two tile sizes (TileSmallM 4, TileLargeM 13) are mixed by a slow position mask, and T_CaveRocks01 and 02 by another. Textures: `/Game/DragonCave/Textures/T_CaveRocks01|02_BC|N|ORM`; none is virtual-textured, which the script checks.
- **Silt** on local-up faces (normal z plus wobble): albedo 0.095..0.165, roughness 0.9, normal flattened 80%.
- **Grade** (Style Guide 4): chroma x Saturation, then a hard cap at HSV saturation 0.35; value x 0.35, then at most 0.25 linear (first run used x0.8 and a 0.6 cap: at 4 m under the lamp the rock read almost white, frame `-250m-bright`; the silt bed is 0.09..0.16); roughness lerp(0.35, 0.55, ORM.g); AO = ORM.r.
- **Parameters** (group names in the instance editor): TileSmallM, TileLargeM, SiltAmount, SiltSharpness, Saturation, Value, RoughMin, RoughMax, NormalStrength, SiltDark, SiltLight.
- **Sources.**
  - Epic's `material_basics` skill (`Engine/Plugins/Experimental/Toolsets/EditorToolset/Content/Python/editor_toolset/skills/material_basics.py`): reuse material functions first, group parameters, list properties before setting them, neutral normal is (0,0,1). WorldAlignedTexture/Normal were the functions to reuse; they are world-space, hence the hand-written object-space version.
  - Engine `MaterialExpressionTransform.h`: confirmed TRANSFORMSOURCE_World / TRANSFORM_Local.
  - The dressing and look lanes' scripts (API usage, silt recipe).
  - Style Guide sec 1-4.
  - Not readable: Epic's "World Aligned Material Functions" page (dev.epicgames.com, renders by JavaScript, empty body) and Ben Golus's "Normal Mapping for a Triplanar Shader" (HTTP 403). The UDN blend follows Golus's article from memory.
- **Unverified.** The normal-map green-channel convention: if bumps look inverted in frames, negate the offsets.

## Numbers and tests
- Built for the CaveShape tests: 249,344 triangles, 21 pieces, 96 rocks, about 373 ms. Rebuilt with a new seed and amplitude in 760 ms.
- Deep.Sim.CaveShape.Floor: 203,616 samples, none inside the floor (11.14 m for the test box). Mid-shaft the rock stands in by up to 2.8 m and recedes by up to 2.5 m; at the mouths, 0.8 m.
- Deep.Sim.CaveShape.Contact: agrees with the rock at 60 of 60 points (30 cm either side). On the centreline the padded box stays at least 1.48 m from the rock all the way down.
- Deep.World.CaveShape.Mesh: 7,114 vertices within 0.000 m of the rule; 16 of 16 traces hit, worst gap 0.00 m.
- Filters Deep.Sim.CaveShape, Deep.World.CaveShape, Deep.Sim.Cave, Deep.World.Cave, Deep.World.Cavern, Deep.World.Dressing, Deep.UI.Sonar and Deep.World.Sonar: 34 run, 33 pass before the drift cap. The one failure was Deep.World.Cave.Hit (see above). With the cap it passes (the run that measured the cap: 35 run, 33 pass. The two failures were the room overshooting the cavern radius and my guessed 1.0 m radius-step bound; both are fixed). Final run, filters Deep.Sim.Cave, Deep.World.Cave, Deep.UI.Sonar, Deep.World.Sonar and Deep.World.Dressing: **35 run, 35 pass**, Deep.World.Cave.Hit included.
- Deep.Sim.CaveShape.Drift, over 90..610 m:
  - Max centre drift 0.480 m per m (cap 0.50), at 480 m. The centre still swings up to 7.2 m off the axis.
  - Max step between 0.5 m rows: the centre 0.240 m, RadiusM 1.016 m (the designed swell into a cavern, about 2 m per m, smooth), and the nearest rock round the outline 1.87 m (at 232.8 m).
  - The rock figure is a strata ledge lip, not a jump of the passage. It is reported, not asserted. If the helm knocks on ledges, lower `StrataDepthM` (1.8).

## Frames
`devlog/media/2026-11-15-rescope-cave-060m.png` (parked at 100 m, stop 1: dark water), `-150m.png` (shaped, textured wall), `-250m-bright.png` (first grade, too bright) and `-stop2.png` (the cavern dome from outside, first grade). Helm seat, via `tools\ue.ps1 shot -Port 8022`. The darker grade is rebuilt (CAVEMAT ok); its frames are pending the shared slot.
