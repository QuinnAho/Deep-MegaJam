# Deep module code map (read this instead of the source tree)

Folders (include from the module root, e.g. `#include "Sim/DeepRules.h"`):
- `Sim/` pure rules. `UDeepRules` = static functions (depth, zones, tether solve, winch, weight, swim, grab pick, Patience, settlement, fees, hull, bailout, salvage spots). `DeepTuningTypes.h` = USTRUCT tuning + result types. `UDeepTuningSettings` = every design number (Project Settings > Game > DEEP Tuning, `Config/DefaultGame.ini`). Defaults are in its constructor and the struct initialisers.
- `Game/` framework. `ADeepGameMode` (builds arena, spawns divers under the airlock, `GotoStop`), `ADeepGameState` (Stop, Credits, HullRating, DiverNumber; replicated), `ADeepPlayerController` (Enhanced Input built with NewObject, forwards to the diver; `KeyHint` for HUD text), `ADeepHUD` (canvas text).
- `Diver/` `ADeepDiver` (ACharacter in MOVE_Flying; cargo, lamp, grab/drop, winch flag; server RPCs) and `UDeepTetherComponent` (TG_PostPhysics: winch step, length clamp, line mesh).
- `World/` `DeepArena::EnsureArena` spawns whatever the map lacks: `ADeepSubmarine` (anchor = airlock), `ADeepSalvage` ring (kinds from tuning), `ADeepAbyssFloor`, `ADeepEnvironment` (light, fog, exposure per stop). `DeepShapes` = engine basic shapes + tint.
- `Debug/` `DeepCheats.cpp` console commands, `DeepStateReport` builds the `DEEPSTATE {json}` line.
- `Private/Tests/` automation tests (`Deep.Sim.*` rules, `Deep.World.*` world). `DeepModule.cpp` registers the module. `Deep.h/.cpp` are Quinn's wizard class: leave alone.

Units: names ending `M` are metres, `Cm` are Unreal units. Rules take metres for lengths and cm for world positions.

How a dive flows (stop-1 slice built so far):
1. Play on any map (default `/Engine/Maps/Entry`). `ADeepGameMode::RestartPlayer` calls `EnsureArena`, then spawns the diver at `ADeepSubmarine::GetDiverStartTransform`.
2. Controller input -> `ADeepDiver::SetMoveInput/SetVerticalInput/...`. Diver Tick: `SolveWeight(cargo kg)` -> `SwimVelocityMps` -> `AddMovementInput` (MaxFlySpeed covers the fastest swim, so scale = m/s).
3. `UDeepTetherComponent` after physics: if winching `WinchStep`; then `SolveTether` with `TetherLengthM(GameState.Stop)`; clamps position and outward velocity; positions the cylinder line.
4. Grab: `FindGrabTarget` -> `PickGrabTarget` over salvage actors -> cargo += `ADeepSalvage::ToCargo`, actor destroyed. Drop respawns it.
5. HUD reads diver depth (`DepthM` from stop depth + anchor Z), zone, cargo, tether solve.
Not built yet: Patience at runtime, cut, bailout, banking, settlement screen, monster, Director lines. Their rules already exist in `UDeepRules`.

Add a rule: declare a `static` UFUNCTION(BlueprintPure) in `UDeepRules`, numbers in a tuning struct (add a field + default), test in `Private/Tests/DeepRulesTests.cpp` with a locally built tuning struct. Design-wide checks go in `Deep.Sim.Tuning.DesignInvariants`.
Add a cheat: another `FAutoConsoleCommandWithWorldAndArgs` in `Debug/DeepCheats.cpp`, name `Deep.<Verb>`, finish with `DeepStateReport::LogState(World)`. Add a key to `DeepStateReport::BuildJson` (and its header comment) for new state.
Cheats: `Deep.State`, `Deep.GiveSalvage <kg> <value>`, `Deep.GotoStop <n>`, `Deep.Teleport <x> <y> <z>` (cm).
Gate: `.\tools\ue.ps1 verify` (editor closed). Tests must start with `Deep.`.
