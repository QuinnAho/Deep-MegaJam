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

## Log
- Nov 25, about 4:10 AM: lane planned by the orchestrator. It waits for D1's report.
