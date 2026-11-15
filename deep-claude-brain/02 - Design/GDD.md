---
updated: 2026-11-14 21:46 EST
---
# GDD (agent copy)

**Status: v4, in effect.** Quinn said go at about 9:12 PM Nov 14. At 9:45 PM he renamed the submarine AI: it is **the Captain**, "sarcastic, kind of an asshole" (D-016 in [[Decision Log]]). Numbers are start values and will be tuned by play.

**Names.** The voice in your ear is the Captain. The Director is the Captain's boss at head office and is never seen or heard. This keeps the jam theme, Director's Cut, in the game: the Director's cut is the 40 percent taken from every haul, and cutting the tether is a Director's order that the Captain enjoys carrying out. Notes written before 9:45 PM call the character "the Director".

Human docs. Claude can read them but cannot edit them in place:
- [DEEP - Brief](https://docs.google.com/document/d/1s7-gTlxQ-urXaO8CnRa_atFVw1bGDQ9sgtFYwNMFdtM/edit): the short design and to-dos. It still says "the Director" for the character.
- [Game Jammy](https://docs.google.com/document/d/1pacncyBHnF7a83vEYtB_EmQNcnsSREjv7sMqVBaqtbQ/edit): the team's live notes.

## One line

First-person tethered salvage dive into a black trench. The submarine's AI, the Captain, mocks you the whole way down, takes the Director's cut of every haul, and cuts your tether when its Patience runs out.

## What makes it fun

Build and tune toward these. If a feature does not serve one of them, it is cut.

1. **One decision, asked every 20 seconds: one more piece, or go home?** Greed buys time (salvage refills Patience) and costs speed (weight). Richer salvage is farther and lower.
2. **The cut is the best moment in the game, not the end of it.** A snap, the tether whips away, a red timer, the Captain's worst line, then a 30 second sprint. Tune it so players often make it with seconds to spare.
3. **The Captain is the personality.** It insults you, it is greedy, and it is funny. Dying is a joke: the diver is written off and replacement diver number two arrives. The settlement screen is the punchline of every dive.
4. **Danger is readable.** The player can always tell how much trouble they are in: the Patience bar, the Captain's tone, breathing, the tether's strain, the monster's sound.
5. **Darkness is the art budget.** A lamp beam in fog hides cheap models and looks expensive.

## The first 90 seconds

A judge gives a jam game about five minutes. This must happen without any explanation screen:

1. Title, then the airlock. The Captain states the job in one insult.
2. Light shafts, particles, salvage glinting below. Grab one. The Captain names the Director's cut.
3. Too heavy to swim up, so winch home.
4. Settlement screen. Hull rating bought. Descend into the dark.

The monster is heard by minute 3. Most players are cut by minute 4. A full run is about 9 minutes.

## Rules and start values

Every number is tuning and lives in config.

| System | Rule |
|---|---|
| Stops and depth | Three stops at 100 m, 300 m and 500 m of displayed depth, in the Sunlight (0 to 200 m), Twilight (200 to 400 m) and Midnight (400 to 600 m) zones. The relic sits at 600 m. Depth is what the HUD shows; the arenas need not be that far apart in the level. |
| Tether | 40 m at stop 1. Each new stop adds 20 m. Cannot swim past it. |
| Winch | Hold to be reeled home at 7 m/s. Loud. |
| Weight | First 10 kg free. Past that you sink and swim about 1% slower per kg. An overloaded diver cannot swim up and needs the winch. |
| Patience | The Captain's. Full at the start of each dive, about 100 s. Drains twice as fast while the monster hunts. Salvage refills it: 5%, 10% or 20% by size. The Captain warns at 50, 25 and 10%. |
| The cut | At zero Patience, when a tether bite is not stopped within 5 s, or when the player asks to be cut loose. The tether is cut and 30 s of bailout air starts. The last 5 s run slow so near misses happen. |
| Cut loose | The player may ask for the cut to reach salvage beyond the tether. The best light pieces sit just out of reach. |
| Re-tether | Reach the airlock on bailout air: new tether, 20 credit fee, the run continues. |
| Death | Bailout air runs out, or the monster touches an untethered diver. Respawn at the airlock as the next replacement diver, with a counter. Carried salvage is lost. 30 credit fee. Credits never go below zero. |
| Salvage | Respawns every dive. Scrap: 2 kg / 20 cr, 6 kg / 70 cr, 15 kg / 200 cr. Pearl: 1 kg / 120 cr, only beyond tether range. |
| Settlement | Gross minus the Director's cut of 40% (rounded up) minus fees. Never below zero. Fee lines are itemised and vary, as jokes. |
| Hull rating | Bought automatically at settlement when affordable: 50 cr to reach stop 2, 200 cr to reach stop 3. |
| Monster | A sea serpent (Quinn, Nov 14: a snake, because it needs no animation assets). Its body is a chain of segments that follow the head's path. Two points of light for eyes. Stops 2 and 3. A pressure cycle, not a patrol: noise (sprint, winch, grabs) brings it in from the dark toward the tether; the lamp beam held on it drives it back for a while. A bite on the tether starts a 5 s alarm, then the cut. It kills an untethered diver on contact. Heard long before it is seen. |
| Relic | Stop 3. About 60 kg: it cannot be swum up, only winched, which is loud. Banking it wins. |

## How it is built

- **The arena builds itself from code and config.** If a map has no submarine actor, the game spawns the sub, salvage and an abyss floor. The game is playable in an empty map and testable headless. Art replaces the stand-ins through config, with no code change.
- The three stops are one arena layout with three light and fog settings from config. Descending is a fade and a rolling depth counter.
- The sub hangs above. Salvage is below and outward, so the way home is always up. Keep the water open: the tether is a straight line.
- The Captain's lines are data in `deep\Content\Data\CaptainLines.csv`.

## Must ship

Diver, tether and winch, salvage and weight, Patience and the cut, re-tether and respawn, bank and settlement, hull rating and descent, the monster, the Captain's subtitle lines, a minimum sound set, title, controls card, win and lose screens, restart, a packaged Windows build.

If time runs short, cut in this order: asking to be cut loose, the monster, the Captain's voice.

## Cut

Gear upgrade shop. Lamp and winch costing Patience. Tether gun. Sonar. DLSS. Controller support. PCG terrain. Water plugin surface. Sub interior. A unique third arena. Co-op, Steam, voice chat. A live AI model or NVIDIA ACE in the build. Saving.
