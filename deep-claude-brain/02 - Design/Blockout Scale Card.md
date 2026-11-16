---
updated: 2026-11-15 22:55 EST
source: deep/Source/Deep/Sim/DeepTuningTypes.h and DeepTuningSettings.cpp (the tuning defaults on main 62c65b3)
---
# Blockout Scale Card

Sizes and speeds for blocking out the slice map. The numbers come from the game's tuning. Where the hull's size is a guess, it says so.

## The map
- New level: **Empty Open World**, so World Partition and one file per actor are on. Suggested path: `/Game/Maps/L_Slice`.
- Block out with Modeling Mode's **CubeGrid**. Use a 1 m grid for detail and 4 m for big masses.
- Put a **12 × 5 × 4 m box** where the sub starts and name it `SubScaleProxy`. It stands in for the sub until the moving sub lands.
- Depth comes from height (Z) once the moving sub lands, so build downward for real. 1 Unreal unit is 1 cm.

## The sub
- **Hull:** plan for **12 m long, 5 m wide, 4 m tall**.
  - Today's placeholder is about 10 × 5 × 3.5 m (the room is 9 × 4.5 × 3 m).
  - The sub kit Quinn picks will set the final size.
  - Proposed, waiting on Quinn: BT-01 at ×2 is **12.3 m long, 3.3 m wide, 5.0 m tall**, taller and narrower than this plan. Squeezes would then need to be 7 m tall. See [[Cave Pipeline]].
- **Speeds:**

  | Move | Speed |
  |---|---|
  | Full ahead (3 notches) | 4 m/s |
  | Astern | 2 m/s |
  | Up and down (ballast) | 2.6 m/s |
  | Turn | 28° a second |

- **Tightest bend:** about 8 m radius at full speed, 4 m at half speed.
- **Times:**

  | Distance | Time |
  |---|---|
  | 100 m of tunnel at full ahead | 25 s |
  | A 60 m drop | 23 s |
  | Crossing a 60 m cavern | 15 s |

- **Hull damage:** a bump faster than **1.5 m/s** damages the hull, 0.5 point per m/s over that, at most 3 points a hit. Scraping costs a point every 2.5 s.

## Spaces

| Space | Width | Height | Notes |
|---|---|---|---|
| Squeeze | 7 m | 6 m | Scrapes likely. A skill test; use one or two, not more. |
| Normal tunnel | 12 to 16 m | 8 to 10 m | Bow lights (18 m) reach both walls. |
| Turning room | 20 m across | 10 m | The sub can turn in place without touching. |
| Cavern stop | about 60 m across | roof 10 m or more above the parked sub | The floor sits 6 to 10 m below the sub. Lights don't reach the far wall: keep it that way. |
| Serpent space | 20 m or more | | Room for it to pass and circle. |

## The diver
- **Swim speed:** 4 m/s. Grab reach: 3 m.
- **Hatch:** 1.6 m square.
- **Lamp:** 35 m range, with an 18° bright core inside a 38° cone.
- **Radar ping:** reaches 100 m.
- **Tether:** 40 m at the first stop, if it stays (see [[Slice Brief]]).
- **Harpoon:** about 25 m.

## Light, for judging darkness
| Light | Range | Cone |
|---|---|---|
| Sub bow lights | 18 m | 28° |
| Helm flood (seated view) | 30 m | 44° |
| Lights on the hull | 5 m | 60° |

## Depths today
- Stops at **100, 300 and 500 m**, and the relic at 600 m.
- The slice only needs the start and the first stop. A 60 to 120 m drop through 150 to 250 m of tunnel is about 2 to 4 minutes of driving.
