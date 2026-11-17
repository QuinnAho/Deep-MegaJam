---
updated: 2026-11-15
status: Research by Claude. Nothing here is decided until Quinn says so.
---
# Games to Learn From

Behaviour only, learned from play, video and published talks. Nothing is extracted or copied. "Observed" means seen in play or public footage. Pillars: [[Slice Brief]].

## Pillar 1: Drive the sub through the dark

| Game | Behaviour | Why it works | What DEEP does |
|---|---|---|---|
| Subnautica (Cyclops) | The big sub is slow and heavy. You steer it from a helm view and the sub's own sounds (engine, creaks, alarms) tell you its state. | The vehicle feels like a place you are in, and the player learns it by feel. Charlie Cleveland's [GDC 2019 talk](https://gdcvault.com/play/1026273) covers how the team built exploration, discovery and the unknown. | Give the sub weight: throttle and ballast lag, momentum carries into walls. Every state (hull, depth, ballast) has a sound before it has a gauge. |
| Barotrauma | A breach is a fast, local emergency: water moves room to room, the crew has to act. Flooding speed is tuned so players have time to react ([official patch notes](https://steamcommunity.com/app/602960/discussions/1/4536825080754434077)). | A failure that spreads gives the player a job, not a bar. | A scrape opens a leak at the contact point. The leak is audible and visible in that compartment, and the player has to leave the helm or accept it. |
| Iron Lung | A blind sub, a porthole camera, and coordinates read from a dial. Navigation is slow and the dark does the scaring. Observed. | Limited information turns each small instrument change into an event. | The slice's tight tunnel and one squeeze should be driven by the camera feed alone. Keep the sub's lamp short, so the player creeps. |

## Pillar 2: Hunted by sound

| Game | Behaviour | Why it works | What DEEP does |
|---|---|---|---|
| Outer Wilds (Dark Bramble) | Anglerfish react to noise. Gentle thrust near them lets you slip past, loud thrust draws them ([guide that describes it](https://twinfinite.net/?p=459764)). | The player makes the danger: every input has a cost, and the player decides how loud to be. | The serpent hears sub noise by state: engine thrust, ballast flooding, scrapes, the diver's ping. Coasting with the engine off is the stealth option. |
| Alien: Isolation | Two layers: a director that points the alien at broad areas, and the alien's own senses for moment-to-moment behaviour. The alien never knows where you are, it hunts near you ([Game Developer article](https://www.gamedeveloper.com/design/the-perfect-organism-the-ai-of-alien-isolation), [revisit](https://www.gamedeveloper.com/design/revisiting-the-ai-of-alien-isolation)). | It feels fair and uncanny: the creature seems smart but cannot cheat, and the director keeps tension rising and falling. | A serpent director sets a hunt zone and pacing (taunt, circle, commit, retreat). The serpent's own hearing runs inside that zone. Never give it the player's exact position. |
| Alien: Isolation (motion tracker) | A handheld device pings and shows blips, but the beep is loud and the screen draws your eye away from the world. Observed. | The tool is an information-for-risk trade. | Keep the diver's ping (D-043, D-049), but make it noisy: a ping that finds salvage also tells the serpent where you are. |
| Subnautica (leviathans) | Roars carry from far off in the water, with long silence between. The sound arrives before the animal does ([Inven on the sound](https://www.invenglobal.com/articles/21514/may-15-the-1-steam-wishlist-title-arrives-subnautica-2)). | The player has time to be afraid. The fear is the gap. | Beat 4 in the brief: a sound first, a shape only at the lamp's edge. Space the calls with long quiet and vary them by distance (muffled, then close and bright). |
| Hellblade (Senua's Sacrifice and Hellblade 2) | Almost all audio is built from binaural recordings, with voices placed around the head ([Vice](https://www.vice.com/en/article/playing-hellblade-senuas-sacrifice-with-newly-restored-hearing/), [Xbox Wire](https://news.xbox.com/en-us/2024/05/17/hellblade-2-audio-design-is-like-nothing-youve-ever-heard/)). | Precise direction in headphones makes sound the main sense. | Use HRTF or binaural spatial audio for the serpent's knocks and calls, so the player can point at it with their ears. |

## Pillar 3: Immersion first (a bare HUD)

| Game | Behaviour | Why it works | What DEEP does |
|---|---|---|---|
| Dead Space | Health, stasis and ammo live on the suit and on holograms in the world, not on a screen overlay. ([GDC Vault, Dino Ignacio](https://gdcvault.com/play/1018819/Crafting-Destruction-The-Evolution-of)). | Everything the player needs is looked at in the world, so the world stays the frame. | Hull, depth and ballast are gauges on the helm, and the diver's air is on the suit or wrist. Only a reticle and prompts are screen UI. |
| SOMA | Underwater walking with a heavy suit, a camera that blurs and degrades near danger, and few interface elements. Observed. | Failure of the equipment is the signal. | The diver's gear gives feedback by failing: the lamp dims, the breath gets louder, the visor fogs, before any number shows. |

## Pillar 4: Seen through a camera

| Game | Behaviour | Why it works | What DEEP does |
|---|---|---|---|
| Unrecord | The "bodycam" look is built from lens behaviour: exposure, rolling shutter, lens bloom, compression, handheld motion. Observed in public trailers and devlogs. | The picture feels recorded, so what is on it feels real. | Build the sub feed from a few lens traits (exposure hunting, bloom on lamps, compression blocks, slight latency). See [[Camera - Footage Look and Sonar Pulse Recipe]]. |
| Content Warning | A shared camera whose image breaks when a monster is near, and which players read as a warning. Observed. | The picture itself is the alarm. | D-041: interference rises with the serpent's closeness and attention. It starts before the serpent is visible and never gives a direction. |

## Caves that feel real

| Game | Behaviour | Why it works | What DEEP does |
|---|---|---|---|
| Deep Rock Galactic | Caves are lit only by what the player carries and throws, so flares are a place-and-return tool ([Epic interview](https://www.unrealengine.com/developer-interviews/guns-gold-and-glory-in-the-caverns-of-deep-rock-galactic), which notes the caves are generated and cannot use baked light). | Darkness is a resource, and cave shape reads through shadow. | Give the diver a limited number of placeable lights. The player marks the way back, and the serpent notices them. |
| Abzu | Underwater light comes as shafts and caustics from above, and colour moves with depth. Observed. | Light direction tells the player where up is and how deep they are. | Light only near the surface and shafts, then nothing past a depth. Below that, the sub's lamp is the only light. |

## The five to do first

1. **Noise-driven serpent hearing** (Outer Wilds), with sources tied to the sub's own states. It makes pillars 1 and 2 one system.
2. **Director above senses** (Alien: Isolation): pacing beats from the slice brief on top of the serpent's local hearing.
3. **Sound before shape, with long silence** (Subnautica), spatialised for headphones (Hellblade).
4. **Failure shown in the world** (Dead Space, Barotrauma): gauges on the helm, leaks you can hear, no HUD bars.
5. **Interference as the alarm** (Content Warning), tied to the serpent's attention and built from lens traits (Unrecord).
