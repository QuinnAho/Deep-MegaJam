# Lane: caves into the dive (Nov 17)

Branch `build/cavedive`, worktree `.wt\cave`, from main after the caves merged (`06ba3f1`). Board row: "Caves into the dive".

Quinn's OK (about 6:30 AM Nov 17): "The caves merge after their own tests and packaged build. Then the next lane puts the caves into the dive: you start in S and drive the sub through with steering and hull hits. The old shaft stays behind a switch." On the squeeze: keep 119 cm, drive it first, tighten it later if it does not feel tight.

Decisions: D-051 (the sub moves through a still world), D-055 (baked chunks strung per dive), D-058 (tunnels flown level and nose first; caverns by ballast), D-059 (joint types), D-061 (S is stop 1's cavern), D-064 (this lane takes moving-sub step 6's collision; the switch stays off until M2), D-065 (depth in the caves follows the stops).

## What exists

- **Chunks:** `deep/Content/Deep/CaveGen/Meshes/SM_CaveChunk_{S,A,T1,C1,B,E}`. Collision is complex-as-simple, Nanite is off, so the collision is the render triangles. Never edit them; report a contact instead.
- **Tables** in `deep/Content/Data`:
  - `CaveChunks.csv`: per chunk, the exit transform and the bounds, in cm and degrees. The entry frame has the joint centre at the origin, +X into the chunk, Z up.
  - `CaveChunkSockets.csv`: Park, Salvage1 to 5 and Serpent. S's Park is its origin; C1's is (3000, 0, -2400).
- **The string tool:** `deep/Content/Deep/CaveGen/Python/cavestring.py`, editor Python only.
  - `STRING = S, A, T1, C1, B, E`; `string_places` composes the chunks.
  - `dive_path` gives world samples (x, y, z, yaw, region) of the sub's origin, the hatch. Regions are neck, tunnel, squeeze and cavern.
  - `sweep` box-traces the hull (`HULL_HALF` 615 × 165 × 250 cm, centred `HULL_AFT` 94 cm aft of the origin) against the placed chunks: 0 contacts in 1168 steps.
- **The composed string**, in cm with S's berth at the origin:

  | Chunk | Placed at | Yaw |
  |---|---|---|
  | A | (4200, 0, -250) | 0 |
  | T1 | (7200, 0, -250) | 0 |
  | C1 | (12500, 4800, -850) | 90 |
  | B | (15271, 9400, -3250) | 30 |
  | E | (17869, 10900, -3250) | 30 |

  C1's berth is (12500, 7800, -3250), 32.5 m below S's.
- **The still world** (moving-sub step 4):
  - `Deep.World.Still` (`World/DeepArena.cpp`) is latched at arena build.
  - `ADeepSubmarine::PlaceStill` teleports the hull from `SubOffsetM`, `SubDepthM` and `SubYawDeg`.
  - The frames come from `UDeepMovingSubRules::StillStopFrame` and `StillHull`.
- **The helm:**
  - `UDeepRunComponent::StepPilot` (`DeepRunComponent.cpp`, about lines 775 to 950) steps `UDeepPilotRules::StepMotion` in 0.05 s substeps. It pushes out of `UDeepCaveRules::Contact`, the analytic shaft, using the turn-free box `TurnFreeBoxM`.
  - `StepShaft` (about line 507) is the old straight drive.
  - The shiver in `ADeepSubmarine::TickHelmLook` (about line 646) uses the same contact.
  - `UDeepMovingSubRules::ResolveWallHit` and `ParkNear` exist, but only tests call them.
- **What reads depth:**
  - `UDeepPilotRules::ShouldPark`
  - `UDeepRules::HelmParkStop`, `NearestStop` and `RelicOffsetCm`
  - `StillBedShows`, `DeepDressing` and `DeepAbyssFloor`
  - in the run component, `Top = Stops[0]` and `Limit = DepthLimitM()`
- **The sonar** traces `ECC_Visibility` (`DeepSonarComponent.cpp`), so it sees the chunks if they block Visibility.
- **Packaging:** `/Game/Deep` is always cooked, and `Data` is staged as NonUFS (`DefaultGame.ini`). So the meshes and the CSVs reach the package.

## M1: drive it

1. **Switch:** a new CVar, `Deep.World.Caves`.
   - 0 is the shaft and the code-built caverns, as now. 1 is the chunk string.
   - It is latched at arena build like `Deep.World.Still`, and it needs Still = 1.
   - The default stays 0 in M1. Tests pin it with the scope in `Private/Tests/DeepTestCVar.h`.
2. **Placement:**
   - With Caves = 1, each machine spawns the string locally: transient static-mesh actors, not replicated, and no map edits.
   - The chunks are composed from `CaveChunks.csv` as `string_places` does, with S's Park socket at stop 1's `StillStopFrame`.
   - DeepShaft, DeepCavern and the abyss beds are not spawned.
   - The run starts parked at S's berth.
3. **Contact:**
   - In cave mode, StepPilot sweeps the real oriented hull box (`GetSolidBoundsLocal`, not `TurnFreeBoxM`) at each substep, against the chunks only.
   - `ResolveWallHit` gives the bounce and the impact. DentHull, the knock and the scrape work as now.
   - A turn applies only if the box at the new yaw is clear; otherwise the yaw holds and the hull scrapes.
   - The shiver uses the same contact.
4. **Parking and limits:**
   - The depth-based park (`ShouldPark` and the glide) must never pull the sub toward S through rock.
   - Parking uses `ParkNear` at the Park sockets, in S and C1.
   - The gate limit must not stop the drive from S to E in M1.
5. **The path:**
   - A new script in `deep/Content/Deep/CaveDive/` imports `cavestring` and exports `dive_path` to `deep/Content/Data/CaveChunkPath.csv`, as chunk-local points with their regions.
   - `Deep.Cave.Goto` puts the sub on the path, by region or by metres, for frames.
6. **What stays:**
   - The squeeze stays at 119 cm, and the chunks stay as they are.
   - New assets go only in `deep/Content/Deep/CaveDive/`. No map edits.
   - Never commit `Scratch7/` or `__pycache__`.

**Tests**, run with `tools\ue.ps1 test -Filter <prefix>`:
- The composition: the joints meet and the joint types chain.
- The hull spawns clear of rock at S's berth.
- The real hull box, flown along the exported path from S to E, touches no rock. This must agree with cavestring's sweep; if it does not, find out why.
- A shove into a wall bounces, dents the hull and scrapes.
- With Caves = 0, the existing suites still pass.

**Frames**, from the helm view unless noted:
- S's berth
- the S→A joint
- A
- the T1 squeeze
- C1's sink and park
- B
- E
- a knock against a wall
- one view from outside the hull

Package once at the end, and take one packaged frame in cave mode.

## M2a: depth, gates, salvage and the hatch (from Nov 18)

Quinn merged M1 (about 1:09 AM Nov 18, `5659e71`) and named M2: salvage, the serpent, the relic, the hatch and tether at C1, and the gates, all in the caves. Depth follows the stops (D-065). `build/cavedive` continues: fast-forward it to `origin/main` first.

1. **Read depth (D-065):**
   - A new table `deep/Content/Data/CaveDepthMap.csv` with columns PhysicalM and ReadM, piecewise linear and strictly increasing. True depth down to the top of C1's sink, then the sink stretched so C1's berth reads 300 m, then slope 1 below. Today that is (100, 100), (108.5, 108.5), (132.5, 300); take the two knots from the composed string (C1's entry joint and C1's Park socket), not from these numbers.
   - In caves, the game state's `SubDepthM` holds the read depth. The state-to-pose mapping in `DeepCaveStringRules` converts through the table both ways. Then everything that reads depth follows the stops unchanged: the readout, the zones, the look, `Top`, the limit, `NearestStop`.
   - Anything else that turns a world Z into a depth in caves (the diver, salvage, the sonar if it labels depth) uses the same table.
2. **Gates:** in caves the limit (`DepthLimitM`, from the gates bought) holds the sub at the read limit. With no gates bought it stops partway down C1's sink, about 120 m physical. M1's bypass of the limit goes. The limit's feedback at the helm and on the HUD works as in the shaft.
3. **The stops:** S's berth is stop 1 (100 m), C1's berth is stop 2 (300 m). `NearestStop`, `HelmParkStop` and the park at each berth agree with them.
4. **Salvage:** the shaft's salvage for stop 1 spawns at S's Salvage1 to 4 sockets and stop 2's at C1's Salvage1 to 5, keeping the shaft's counts, kinds and values. If the shaft has more items than a cavern has sockets, report it; don't invent places. No item sits inside rock or floats far from it.
5. **The hatch and the tether:** parked at either berth, the diver leaves through the hatch, reaches every salvage socket of that cavern on the tether, and gets back in. The diver collides with the chunks' rock. If a socket is out of the tether's reach, report the distance; don't move sockets or change the tether without asking.
6. **Waits for M2b:** the serpent at its sockets (S and C1 have one each), the relic, a full cave playthrough and the default flip. Stop 3 (500 m) needs a deeper cavern, so where the relic goes until then is Quinn's call at M2b.

**Tests**, by filter: the map (knots, round trip, strictly increasing); with no gates the limit holds the sub in C1's sink; with a gate bought it sinks to C1's berth, parks, and reads 300 m; salvage spawns at the sockets clear of rock; from each berth the diver reaches each socket on the tether, or the report lists which can't; with Caves = 0 the suites pass as before.

**Frames**, from the helm unless noted: S's berth reading 100 m; held at the limit in C1's sink, reading 200 m; parked at C1's berth, reading 300 m; S's salvage from the helm and from the diver; the diver at C1 on the tether by a salvage item. Package once at the end, with one packaged frame in caves.

## M2b: salvage in piles, the serpent, the relic in C1 (from Nov 18)

Quinn merged M2a (about 2:10 AM Nov 18, `7ac9d5b`) and chose: the caves lay 10 salvage pieces a stop in piles (D-067), and the relic waits in C1 until a stop-3 cavern exists, with the caves off by default until then (D-068). `build/cavedive` continues from `origin/main`.

1. **Salvage in piles (D-067):** each stop lays the shaft's 10 pieces, with the shaft's kinds and values, in piles of 2 or 3 at the cavern's salvage sockets: S lays 3, 3, 2 and 2 over its four, C1 lays 2 at each of its five. A pile's pieces rest on the rock within about 2 m of the socket, clear of rock and of each other, and every piece is within the tether's reach from that cavern's berth.
2. **The serpent:** it behaves as in the shaft (the same states, noise and per-stop tuning), but in the caves it lurks around the Serpent socket of the cavern the sub is parked in, and its drawn body keeps clear of the chunks' rock in every state, as it already does in `ADeepCavern` (the clearance at `DeepMonsterTypes.h:387`). Where a shaft distance doesn't fit a cavern (the 45 m lurk radius, the pass line, the stalk's arcs), fit it to the cavern's open space and report the numbers. It must still reach the diver and the line anywhere the tether reaches.
3. **The relic in C1 (D-068):** in the caves the relic's stop is 2. It spawns once in C1 at a new `Relic` socket in `CaveChunkSockets.csv`: on the floor, clear of rock, within the 60 m tether from C1's berth, and apart from the salvage piles, so finding it is its own goal. Winching it in and banking it wins the run as in the shaft. In the shaft the relic stays at stop 3. Report the socket and why it sits there.
4. **The HUD's next stop:** in the caves the HUD names only stops the cave string has. At C1, the last stop until stop 3 exists, it shows no next stop rather than 500 m.
5. **A full cave run:** a playtest scenario with `Deep.World.Caves 1`: start parked in S, dive and bank salvage, buy the first gate, sink to C1, fetch the relic and bank it, ending in the win. Use the harness's commands; add what the run lacks.
6. **The default stays off** (D-068) until stop 3's cavern exists and Quinn says so. With Caves = 0 the game is unchanged.

**Tests**, by filter: 10 pieces a stop in piles of 2 or 3, clear of rock and in reach; the serpent's lurk at each cavern's socket and its body clear of rock over sampled lurk, stalk and pass paths; the relic at C1's socket, in reach, and the win; the HUD's next stop in the caves; with Caves = 0 the suites pass as before.

**Frames**, from the diver unless noted: a pile in S; a pile in C1; the serpent in S and in C1, within sight; the relic in C1; the HUD parked at C1 (from the helm); the win at the end of the cave run. Package once at the end, with one packaged frame in caves.

## Stop 3's cavern, C2 (D-069, from Nov 18)

Quinn approved the plan at 2:37 AM Nov 18. C2 is a new last chunk joined to B's exit. E leaves the string; its mesh and its row in `CaveChunks.csv` stay. Part 1, the generator, starts now beside M2b on `build/cavestop3` in `.wt\cave3`. Part 2, the runtime, follows on the same branch once M2b has merged and main is merged in.

**The shape**, in C2's frame (the entry joint's centre at the origin, +X in, Z up):
- The entry is a bed joint, conformed to B's exit.
- A tunnel drops about 8.5 m to the pit's lip, where a `SinkTop` socket sits on the path. B's floor reaches about 7.1 m below its joints, so the lip lies below all of B.
- The pit is a vertical shaft like C1's sink: a clear radius of at least 9.5 m, and path bends of at least 10.5 m radius. After about 12 m it opens into the roof of the chamber.
- The chamber is the run's biggest and darkest: a long hall, about 50 by 35 m, wider than it is tall. The shaft comes down at one end. The sub sinks through open water and berths about 24 m below SinkTop, a few metres above the floor, facing down the hall. The roof is about 12 m above the berth.
- There is no exit: the chunk closes in rock (Type `stop`, InType `bed`, no OutType).

**Depths:** SinkTop reads 308.5 m and the berth 500 m. That is the slope of C1's sink: 8.5 m read true, then 191.5 m of gauge over 24 m. The second gate's 400 m limit falls about 11.5 m below SinkTop, so until the hull upgrade is bought the sub hangs at the foot of the shaft, over the dark hall. In the hall the gauge reads from about 400 m at the roof to about 505 m at the floor, as C1's does over its sink.

**Around the berth** (the stop-3 tether is 80 m: 40 m plus 20 m a stop):
- Salvage: five sockets, for 10 pieces in piles of 2. They lie on open floor and ledges along the hall, each with 2 m clear around it, between about 10 and 45 m from the berth. Some are out of sight from the hatch, so the diver has to look.
- The serpent's bay is a side pocket off the middle of the hall. Its `Serpent` socket is in the void, 3.8 to 5 m from rock.
- The relic: a `Relic` socket on the floor of an alcove at the hall's far end, 35 to 55 m from the berth by an open swim, and at least 8 m from any salvage socket. The swim to it passes the serpent's bay.

**Part 1, the generator (now):**
- Build C2 in `cavegen.py` and `cavestring.py`, and bake `SM_CaveChunk_C2` in `deep/Content/Deep/CaveGen/`. Don't re-bake or change any other chunk.
- `STRING` ends `B, C2`.
- Add C2's row to `CaveChunks.csv`.
- Add its sockets to `CaveChunkSockets.csv`: Park, SinkTop, Salvage1 to Salvage5, Serpent and Relic.
- Add its path to `CaveChunkPath.csv` through `export_cave_path.py`.
- Add frame cameras for C2.
- Place C2 in L_CaveSpike and take E's actor out.
- Every other chunk's rows in the three tables stay byte-identical.
- The string's checks pass: joints, lips, overlap, the hull sweep, the shaft's column, the tether and the cameras.
- With the C++ unchanged, the game is unchanged: the runtime ignores rows for a chunk outside its string (`PathPoses` skips them).
- **Frames** (editor captures, as in round 8c):
  - the string from above, with C2
  - a cut along C2's path through the tunnel, the shaft and the hall
  - the helm at the lip, looking down the shaft
  - the view from the 400 m limit into the hall
  - the hall from the berth's hatch
  - the hall from the relic's end, looking back at the berth
  - the serpent's bay
  - the relic's alcove

**Part 2, the runtime (after M2b):**
1. `String = {S, A, T1, C1, B, C2}` and `Berths = {S, C1, C2}`.
2. C2's salvage: the shaft's 10 pieces, 2 at each of its five sockets.
3. The depth map: a later berth's chunk reads true down to its `SinkTop` socket if it has one, otherwise down to its entry joint. Below that it stretches so its berth reads the next stop's depth. `CaveDepthMap.csv` gains C2's two knots, about (141, 308.5) and (165, 500). C1's knots stay.
4. The relic sits at the last berth's `Relic` socket, in C2. M2b derives the relic's stop from the string.
5. The HUD at C1 names 500 m as the next stop. At C2 it names none.
6. The hull upgrade's limit holds the sub at 400 m in C2's shaft until it is bought. Buying it sinks the sub to C2's berth.
7. The serpent lurks at C2's socket while the sub is parked there.
8. The cave run goes S, the flashlight, C1, the hull upgrade, C2, the relic and the win.

Then frames, then Quinn's OK. After that the default flips to caves (D-068), and moving-sub step 5 removes D-022's code.

## Log

- Nov 17, about noon: lane started on M1.
- Nov 17, 12:50 PM: M1 built and pushed (`132a907`, `400b49d`). What it found, for M2:
  - The real hull box (the model's solid bounds) is half (579.5, 156.2, 236.6) cm, its centre 89 cm aft and 68.7 cm up, against cavestring's half (615, 165, 250) centred 94 cm aft. It clears the path: 0 contacts in 1168 steps over 284 m. The squeeze stays as built until Quinn has driven it.
  - A straight sink 20 m into C1 hits a ledge at 112 m.
  - E ends in a wall, which holds the sub.
  - In S, C1, B and E the lamps light only the floor ahead: from the helm they read as grey floor. Finding the way in the caverns may lean on the soft ping; judge it when Quinn drives.
  - Setting `Deep.World.Caves` in a running game restarts the level so the arena latches it.
  - The unzipped build is 1.2 GB; `ue.ps1` warns about the old 1 GB jam limit, which no longer applies.
  - Play build for Quinn: `Builds\CaveDive-1251\Play-Caves.bat` (caves on from `-ExecCmds`).
- Nov 17, 1:02 PM: M1's merge gate green on `400b49d` (192 tests, S1-hatch and S7-run, package, smoke, two packaged frames, zip 911.1 MB).
- Nov 18, about 1:10 AM: Quinn merged M1 (`5659e71`) and chose depth that follows the stops (D-065). M2a starts.
- Nov 18, about 1:50 AM: M2a built and pushed (`7b90e9f`, `2e0784e`), its tests green. What it found:
  - The shaft lays 10 salvage pieces a stop; S has 4 sockets and C1 has 5, so the caves lay 4 and 5, about half the pay. Quinn's call (page v40); recommended: 10 a stop, in piles of 2 or 3 around each socket.
  - Every socket is within the tether: S 11.7 to 19.5 m of 40 m, C1 9.6 to 25.3 m of 60 m.
  - C1's 24 m drop reads 108 to 300 m, so in C1 the diver's gauge moves about 8 m per metre swum, and the fog and light change from roof to floor.
  - Between pings the caverns are black, and the diver's lamp needs the flashlight. The soft ping is a post-process drawn from depth, so it draws the cave rock: the helm's ping spreads to 50 m, the diver's to 100 m, and the helm's scope shows 60 m. Quinn judges by driving whether that is enough.
  - The HUD's next stop reads 500 m in the caves (M2b).
- Nov 18, about 1:55 AM: main merged into `build/cavedive` (`e2c9997`); M2a's merge gate started.
- Nov 18, 2:01 AM: M2a's merge gate passed on `e2c9997`: 198 tests, both playthroughs with the caves off (`devlog/media` untouched), the package (1.2 GB, zip 911 MB), smoke, and packaged frames of the room start and of the squeeze with the caves on. The build to drive is `Builds/CaveDive-M2a/Play-Caves.bat`. Page v41.
- Nov 18, about 2:10 AM: Quinn merged M2a (`7ac9d5b`; its nine frames are in `devlog/media` as `2026-11-18-m2a-*.jpg`) and chose salvage in piles, 10 a stop (D-067), and the relic in C1 until stop 3's cavern exists, with the caves off by default until then (D-068). M2b starts.
- Nov 18, 2:37 AM: Quinn approved stop 3's cavern, C2 (D-069). Its generator part starts beside M2b on `build/cavestop3` in `.wt\cave3`.
- Nov 18, about 3:35 AM: M2b built and pushed (`f83033b` to `f477eee`): salvage in piles of 2 or 3, 10 a stop; the serpent at each cavern's socket, clear of rock, first at stop 2; the relic in C1, 23 m from the berth; the HUD names only the cave string's stops, so at C1 it names none; and S13-caves, a cave run from S to the win. The caves have no autopilot, so S13-caves moves the sub between berths with `Deep.Cave.Goto`. M2b's own packaged-shot script deadlocked and held the Unreal slot for 31 minutes; the orchestrator checked it and stopped it.
- Nov 18, 4:16 AM: C2's shape, round 2, pushed on `build/cavestop3` (`4888633`; 38 cave tests pass). The ledges read as layers of the rock. The hall is about 58 m long, 35 m across and 18 to 21 m high; C1's room is 44 m, 36 to 42 m and 22 m. `section.py` labels the entry joint "true 132.5 m (reads 241 m)", but it will read 300 m: the page's copy is fixed, and part 2 fixes `section.py`. The serpent-bay frame burns out from a near wall.
- Nov 18, 4:27 AM: M2b's merge gate passed on `291d1ed` (main merged in): 207 tests, S1-hatch and S7-run with the caves off, S13-caves, the package (1.2 GB, zip 911 MB), smoke, and packaged frames of the room start and the relic. Play build: `Builds\CaveDive-M2b\Play-Caves.bat`. Page v43 asks Quinn to OK M2b's merge and C2's shape.
