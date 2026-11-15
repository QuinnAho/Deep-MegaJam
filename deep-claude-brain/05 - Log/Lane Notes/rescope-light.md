# Lane F: rescope/light (the lamp and the light in the water)

Branch `rescope/light`, worktree `.wt/voice`, from integration `ea92861`. Port 8026.

## The ask

Quinn played the build: "the flashlight does not have a big enough cone, you can't see anything really" and "the light propagation within the fog seems a little weird, I would like to make this better".

Goal: the diver sees the cave in the lamp. Light in the water reads as murky deep water in an AAA horror game: a soft forward-scattering beam with a wide pool, backscatter that does not white out the eye, falloff into real darkness, no light through rock.

## What was wrong (read from the code)

1. **Pencil beam.** Lamp cone 7 degrees inner, 21 outer: at 10 m the full-strength spot is 2.5 m across.
2. **Trails.** Epic's Volumetric Fog page: the fog's "heavy temporal reprojection filter" leaves "lighting trails" behind fast-changing lights, flashlights named. The lamp turns with the head, so its beam in the fog lags and smears. The engine's history weight is 0.9.
3. **Milk at the eye.** Phase g 0.5 and fog starting 0.5 m from the eye: a wide cone would light the water round the eye and send a fair share back.
4. **Light through rock.** The descent shaft's rock (`ADeepShaft` tubes) cast no shadow, and the bow lights (the helm floods) cast none: both the downlight and the floods lit the fog on the far side of the rock.

## Recipe (sources below)

| Setting | Was | Now | Why |
|---|---|---|---|
| Lamp inner / outer cone | 7 / 21 deg | 18 / 38 deg | A usable pool, about 15 m across at 10 m, soft edge (engine squared ramp). Unitless intensity, so the centre does not dim. |
| Lamp volumetric scattering | 1.2 | 0.8 | The wider cone lights about three times the water; the beam reads as murk, not a solid. |
| Fog scattering distribution (HG g) | 0.5 | 0.7 | Epic: near 1 scatters "predominantly in the direction of the light". Sea water is about 0.9 (Petzold). Halves the backscatter at a viewer behind the lamp; shafts seen toward the light get stronger. |
| Volumetric fog start | 0.5 m | 1.0 m | No lit blob round the eye. |
| Volumetric distance 100/300/500 m | 80/60/40 m | 64/48/40 m | The same Z slices over less water: finer slices along the 35 m beam. Beyond it the height fog carries the water colour. |
| r.VolumetricFog.HistoryWeight | 0.9 (engine) | 0.7 | The beam follows the head instead of trailing. CVar help: "a tradeoff between visible jittering and responsiveness". |
| r.VolumetricFog.GridPixelSize / GridSizeZ | scalability | scalability (tuning 0) | Tried 8 / 128 (Epic level 3): GPU 58 ms against 12 ms at stop 1, so it went back to scalability. The tuning fields stay for a later pass. |
| r.VolumetricFog.HistoryMissSupersampleCount | 4 | 8 | Less noise on a fast turn (CVar help: "reduces noise when panning"). |
| Shaft rock casts shadows | off | on | Stops the downlight and floods lighting fog through rock. |
| Bow lights (helm floods) shadows and fog shadows | off | on | Their lit water stops at the rock. Epic: volumetric shadows cost about 3x for a spot light. |
| Helm flood fog scattering (`HelmFloodFog`) | 0.35 | 0.15 | The floods milked the helm view grey and hid the wall 9 m ahead. |

All numbers are DEEP Tuning (`FDeepLampTuning`, `FDeepLookTuning`, presets in `UDeepTuningSettings`). The CVars are set once by `ADeepEnvironment::ApplyFogQuality` with `ECVF_SetByCode`; 0 in tuning keeps the engine's value.

Sources:
- Epic, "Volumetric Fog in Unreal Engine" (UE 5.9 docs): https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-fog-in-unreal-engine (scattering distribution, albedo, extinction, per-light Volumetric Scattering Intensity, Cast Volumetric Shadow cost, GridSizeZ, the temporal "lighting trails").
- Engine source, `Renderer/Private/VolumetricFog.cpp` (CVar names, defaults and help text) and `Engine/Config/BaseScalability.ini` (levels 2 to 4: 16/64, 8/128, 4/128).
- Engine source, `SpotLightComponent.cpp`: unitless spot intensity is not remapped by the cone (lumens are), so the wider cone keeps its centre.
- `03 - Research/Polish - Engine Skills and Toolsets.md`: no look skill exists; the look stays in C++ (do not load the outdoor-lighting skill).

## Not touched

The serpent (Lane G reads the lamp's outer cone for its seen-in-beam check and is moving it to its own angle; until then the wider cone makes the serpent count as lit at a wider angle), the HUD, the room lights, the cave mesh shape.
