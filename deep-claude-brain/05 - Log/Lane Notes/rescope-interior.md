# Lane E: rescope, interior (Nov 15, 4:15 to 5:30 PM)

Branch `rescope/interior` (from integration `cee150a`), worktree `.wt/camera`. Quinn at 3:50 PM: "the interior is still not great", "ad hoc and hacky, lacking real polish and immersion". The goal is a cramped, worn, working submarine. Style: [[Style Guide]].

## What was built and why

- `World/DeepRoomDress.h/.cpp`: the room's dressing as a pure part list (`DeepRoomDress::Pieces`, `Lights`), built from engine basic shapes and drawn as 15 instanced static meshes (shape x material) with no collision. `ADeepSubmarine` calls `Create` once in its constructor and `Skin` in `ApplyLook`. The room's own collision is unchanged.
- **149 pieces:**
  - 9 hull ribs (ceiling bar plus wall legs; a leg is skipped behind the lockers, the console and beside the helm seat);
  - 3 port pipe runs in 4 segments each with flanges, a valve drop to the deck with a brass wheel, a gate valve forward;
  - a starboard cable tray with 3 cables and drops to the console and the helm;
  - 7 gauges (brass bezel, glass face);
  - 3 lockers with handles and vents;
  - a fold-down bench on struts;
  - a ladder up the port wall to a ceiling scuttle with a hand wheel;
  - 5 caged fixtures (base, bulb, 4 bars, ring);
  - a low strip housing.
- **5 lights, from sources only:**
  - 2 amber practicals (#FFB45A), one over the console and one over the helm;
  - 2 red emergency lights (#D8271C), aft and forward port;
  - 1 cool low strip (#BFE3EA), a rect light under the pipes facing in and down.

  Strong falloff (radius 260-300 cm), no shadows, no emissive on any piece. The hatch's existing red light gets a cage on a deck post, not a new light. All numbers are in `UDeepTuningSettings::RoomDress` (`FDeepRoomDressTuning`).
- **Kept clear** (tested): every station-to-station walk and each station to the hatch (walker radius 35 + 3 cm, below 2 m), the hatch plus 50 cm, and 1 m round the helm seat for the helm lane's levers.
- **The ladder:** the brief said "a ladder to the hatch". The hatch is in the floor and its use must stay clear, so the ladder goes up the port wall to a ceiling escape scuttle beside it.
- **Cheat:** `Deep.Room.Dress 0|1` hides or shows the dressing and its lights, for A/B frames and frame time.

## Materials: recipe and sources

1. Epic's material skill (`EditorToolset/.../skills/material_basics.py`) says to instance before making a new material. So all five room materials are instances of the existing `M_WornSteel`, which already has fbm chips, rust streaks, grime, bump and roughness from object-relative 3D noise and needs no UVs.
2. **Edge wear.** The usual recipe is a geometry edge or curvature mask times noise, thresholded, used as the blend from paint to bare metal. Sources:
   - [Painted Edges shader, UE forum](https://forums.unrealengine.com/t/rafasc-painted-edges-shader/2833029): no UVs or bake, a raw 0-1 edge mask.
   - [Smart Edge Wear breakdown, UE forum](https://forums.unrealengine.com/t/paulareb-smart-edge-wear-procedural-mask-educational-breakdown/2728716).
   - [Procedural edge wear, noise x edge mask](https://www.strayspark.studio/learn/tutorials/procedural-edge-wear-blender).
   - Adobe's Metal Edge Wear page returned 403.

   Without baked curvature, the edge comes from `LocalPosition` (instance origin, `MaterialExpressionLocalPosition.h`): a basic shape spans -50..50 on each axis, so being near two faces at once means an edge or a corner. That distance goes through a smoothstep with `EdgeWidth` (percent of size), times fbm, then a threshold set by `EdgeWear`. It is maxed into the chip mask, so colour, roughness and metal all turn bare on the edges. `ObjectBounds` was rejected because it is per component, not per instance.
3. `M_WornSteel` gains `EdgeWear` (default 0, so every existing user looks the same), `EdgeWidth`, and `used_with_instanced_static_meshes`.
4. **Instances** (linear colour, saturation at most 35%, roughness 0.4-0.9 except glass):

   | Instance | Look | Wear / Rust / Edge | Roughness |
   |---|---|---|---|
   | `MI_FrameSteel` | grey-green paint | 0.28 / 0.4 / 0.45 | 0.66 |
   | `MI_PipeSteel` | warmer, rustier | 0.3 / 0.55 / 0.3 | 0.6 |
   | `MI_DullBrass` | tarnished brass, rubbed bright on edges, metal 1 | edge 0.5 | 0.55 / 0.4 |
   | `MI_Rubber` | black rubber | edge 0 | 0.85 |
   | `MI_GrimyGlass` | near-black glass, opaque (small pieces seen dark) | edge 0 | 0.18 |

5. `deep/Content/Deep/Room/build_room_materials.py` builds all of it headless (`build_instances()`). The run gave: M_WornSteel 24 expressions, 0 errors; five MIs saved. M_Screen and M_Glass were rebuilt by the script but are unchanged, so they were restored and not committed.

## Tests (headless, through `tools\ue.ps1`)

- `Deep.World.RoomDress` (new), 4 of 4 passing:
  - `Count`: 149 pieces, every material used, lights 2 amber / 2 red / 1 strip.
  - `InsideHull`.
  - `ClearZones`: walks, hatch, helm.
  - `ChecksBite`: a crate in each zone is caught, and one overhead is not.
- `Deep.World.Sub` and `Deep.World.GreyBoxLight`: 9 of 9 passing.
- First run: `ClearZones` caught 3 pieces. A rib leg sat 71 cm from the helm seat, and the console cage hung 2 cm under head height. The leg is now skipped and the cage raised.
- A unity-build clash: file-local `AlongX` in my .cpp hid a name in `DeepSalvage.cpp`. All file-local names now carry an `Rd` prefix.

## Frames (1280x720)

- Before (`cee150a`): `devlog/media/2026-11-15-rescope-interior-before-{helm,hatch,aft}.png`.
- After: `devlog/media/2026-11-15-rescope-interior-after-{helm,hatch,aft}.png`. The first aft shot stalled at engine start (another lane's editor was using 5.6 GB) and passed on retry.
- In the frames: ribs and pipe runs on both walls, valve wheels, gauges over the console, lockers and the bench aft, the ladder, caged amber and red fixtures, the hatch clear. Glass gauge faces read as dark discs; the strip shows as a cool wash on the port side.

## Cut / not done

- No true curvature: cylinders get wear only on box-like corners, so pipes and flanges rely on chips and rust.
- No per-instance colour variation (`PerInstanceCustomData`).
- No sound.
- Light shadows are off for cost. The amber practicals could cast shadows if frame time allows.
