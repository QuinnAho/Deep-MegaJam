---
updated: 2026-11-14 21:15 EST
---
# GDD (agent copy)

**Status: v3, in effect.** Quinn said go at about 9:12 PM Nov 14 after the hole pass (D-013) and asked for a design tuned for fun and for winning (D-014 in [[Decision Log]]). Numbers are start values and will be tuned by play.

Human docs. Claude can read them but cannot edit them in place:
- [DEEP - Brief](https://docs.google.com/document/d/1s7-gTlxQ-urXaO8CnRa_atFVw1bGDQ9sgtFYwNMFdtM/edit): the short design and to-dos. Current. Created 9:13 PM Nov 14.
- The longer v1 doc from 8:56 PM is superseded and can be deleted.
- [Game Jammy](https://docs.google.com/document/d/1pacncyBHnF7a83vEYtB_EmQNcnsSREjv7sMqVBaqtbQ/edit): the team's live notes.

## One line

First-person tethered salvage dive into a black trench. A corporate submarine AI, the Director, takes a cut of every haul and cuts your tether when its Patience runs out.

## What makes it fun

Build and tune toward these. If a feature does not serve one of them, it is cut.

1. **One decision, asked every 20 seconds: one more piece, or go home?** Greed buys time (salvage refills Patience) and costs speed (weight). Richer salvage is farther and lower.
2. **The cut is the best moment in the game, not the end of it.** A snap, the tether whips away, a red timer, the Director's worst line, then a 30 second sprint. Tune it so players often make it with seconds to spare.
3. **The Director makes everything a joke about money.** Dying is funny: the diver is written off and replacement diver number two arrives. The settlement screen is the punchline of every dive.
4. **Danger is readable.** The player can always tell how much trouble they are in: the Patience bar, the Director's tone, breathing, the tether's strain, the monster's sound.
5. **Darkness is the art budget.** A lamp beam in fog hides cheap models and looks expensive.

## The first 90 seconds

A judge gives a jam game about five minutes. This must happen without any explanation screen:

1. Title, then the airlock. The Director states the job in one line.
2. Light shafts, particles, salvage glinting below. Grab one. "Forty percent of that is already mine."
3. Too heavy to swim up, so winch home.
4. Settlement screen. Hull rating bought. Descend into the dark.

The monster is heard by minute 3. Most players are cut by minute 4. A full run is about 9 minutes.

## Rules and start values

Every number is tuning and lives in config.

| System | Rule |
|---|---|
| Tether | 40 m at stop 1. Each new stop adds 20 m. Cannot swim past it. |
| Winch | Hold to be reeled home at 7 m/s. Loud. |
| Weight | First 10 kg free. Past that you sink and swim about 1% slower per kg. An overloaded diver cannot swim up and needs the winch. |
| Patience | Full at the start of each dive, about 100 s. Drains twice as fast while the monster hunts. Salvage refills it: 5%, 10% or 20% by size. The Director warns at 50, 25 and 10%. |
| The cut | At zero Patience, when a tether bite is not stopped within 5 s, or when the player asks to be cut loose. The tether is cut and 30 s of bailout air starts. The last 5 s run slow so near misses happen. |
| Cut loose | The player may ask for the cut to reach salvage beyond the tether. The best light pieces sit just out of reach. |
| Re-tether | Reach the airlock on bailout air: new tether, 20 credit fee, the run continues. |
| Death | Bailout air runs out, or the monster touches an untethered diver. Respawn at the airlock as the next replacement diver, with a counter. Carried salvage is lost. 30 credit fee. Credits never go below zero. |
| Salvage | Respawns every dive. Scrap: 2 kg / 20 cr, 6 kg / 70 cr, 15 kg / 200 cr. Pearl: 1 kg / 120 cr, only beyond tether range. |
| Settlement | Gross minus 40% (rounded up) minus fees. Never below zero. Fee lines are itemised and vary, as jokes. |
| Hull rating | Bought automatically at settlement when affordable: 50 cr to reach stop 2, 200 cr to reach stop 3. |
| Monster | Stops 2 and 3. A pressure cycle, not a patrol: noise (sprint, winch, grabs) brings it in from the dark toward the tether; the lamp beam held on it drives it back for a while. A bite on the tether starts a 5 s alarm, then the cut. It kills an untethered diver on contact. Heard long before it is seen. |
| Relic | Stop 3. About 60 kg: it cannot be swum up, only winched, which is loud. Banking it wins. |

## How it is built

- **The arena builds itself from code and config.** If a map has no submarine actor, the game spawns the sub, salvage and an abyss floor. The game is playable in an empty map and testable headless. Art replaces the stand-ins through config, with no code change.
- The three stops are one arena layout with three light and fog settings from config. Descending is a fade and a rolling depth counter.
- The sub hangs above. Salvage is below and outward, so the way home is always up. Keep the water open: the tether is a straight line.

## Must ship

Diver, tether and winch, salvage and weight, Patience and the cut, re-tether and respawn, bank and settlement, hull rating and descent, the monster, the Director's subtitle lines, a minimum sound set, title, controls card, win and lose screens, restart, a packaged Windows build.

If time runs short, cut in this order: asking to be cut loose, the monster, the Director's voice.

## Cut

Gear upgrade shop. Lamp and winch costing Patience. Tether gun. Sonar. DLSS. Controller support. PCG terrain. Water plugin surface. Sub interior. A unique third arena. Co-op, Steam, voice chat. A live AI model or NVIDIA ACE in the build. Saving.
