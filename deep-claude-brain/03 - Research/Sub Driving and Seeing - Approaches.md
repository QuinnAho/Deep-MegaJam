---
updated: 2026-11-25 01:56 EST
---
# Sub Driving and Seeing - Approaches

Quinn, after driving: the sub is hard to see and control, and "you barely can see anything". This note compares fixes. Style limits are in [[Style Guide]] ("Darkness is the design", "Do not raise exposure or fog to make it visible").

Legend: **[Code]** = verified in our repo by the brief. **[Read]** = from a source I read about. **[Unsure]** = anecdotal or not confirmed.

## 1. What we have [Code]
- Helm camera sits 5.2 m ahead of the sub's origin and 2.6 m down, pitched 32 degrees down, FOV 95 (`DeepSubmarine.cpp`). It was built for the vertical well of the jam version.
- Throttle notches, 4 m/s ahead, surge drag 0.45/s (long coast), rudder at 28 deg/s building at 1.6/s, ballast lever that stays where left, no pitch or roll.
- Cave collision sweeps the real 12.3 x 3.3 x 5.0 m box, bounces, slides, and refuses turns into rock (`StepPilotCave`).
- Squeeze clearance is 0.9 to 1.2 m round the hull.
- Helm flood 40000, 44 degrees, 30 m, pitched -30 degrees. Fog density about 0.02, exposure locked.

Likely causes of "can't see, can't control" (my read of those numbers, not yet measured):
1. A camera 32 degrees down in a level tunnel looks at the floor, not along the path.
2. Floods pitched down light the floor close in and leave the tunnel ahead black.
3. A first-person view cannot show the hull edges, so a 1 m gap on each side is a guess. A 12 m hull cannot be judged from the nose.
4. Delayed control (throttle notch + drag + rudder build) plus a refused turn feels like fighting the sub.

## 2. What other games do [Read]
| Game | Finding | Source |
|---|---|---|
| Subnautica Seamoth | Most praised vehicle. Full 6-axis movement, called "too simple to use" by one critic. | [search summary of reviews](https://rpgcodex.net/forums/threads/what-game-are-you-wasting-time-on.50372/post-5952644) |
| Subnautica Cyclops | Players call it clunky, ask for strafing and remapping. Others say it is "weighted differently". Anecdotal. | [forum](https://forums.unknownworlds.com/discussion/155568), [Backloggd review](https://backloggd.com/u/Lucca202/review/587772/) |
| Below Zero Seatruck | One player calls it a good middle between Seamoth and Cyclops. Anecdotal. | same Backloggd page |
| Prawn suit | "Unwieldy", snags on small obstacles. | [TechRaptor](https://techraptor.net/gaming/reviews/subnautica-review-serviceable-survival) (summary only) |
| Under the Waves | Sub controls called not intuitive, partly "peculiar" camera tilt; "janky, never game-breaking". Hitting rocks can destroy the sub. | [GamesRadar](https://www.gamesradar.com/under-the-waves-review), [Sixth Axis](https://thesixthaxis.com/2023/09/19/under-the-waves-review) (via search; unverified quotes) |
| Barotrauma | Crew split: captain steers with helm and sonar. Active sonar eases steering but draws monsters. Passive sonar sees only close terrain. | [Barotrauma wiki: Sonar](https://www.barotraumagame.com/wiki/Sonar) |
| Iron Lung | Deliberate opposite: no windows, clumsy controls, sonar and photographs. Praised for tension. | [itch page](https://ironlungdev.itch.io/ironlung), [GG Recon](https://www.ggrecon.com/reviews/iron-lung-review/) |

[Unsure] I could not source Diluvion or GTA V submersibles in this pass. Do not cite them until read.

**Lesson [Read, partly inferred]:** the loved vehicle is the small, free-flying one with simple input. Complaints land on large, heavy or oddly-cameraed ones. Our 12 m sub is Cyclops-sized, which is the weak spot. Iron Lung shows low control and low sight can work, but only when that is the whole game. DEEP also asks the player to drive and then dive, so it cannot afford it.

## 3. Camera options
General rule from [Nesky's GDC talk "50 Game Camera Mistakes"](https://gdcvault.com/play/1020460/50-Camera): dynamic third-person cameras are hardest to design, and the camera must move the way the player wants or frustration follows ([Vice summary](https://www.vice.com/en/article/lights-camera-distraction-the-problem-with-virtual-camera-systems), full video on [Game Developer](https://gamedeveloper.com/design/video-50-common-game-camera-mistakes----and-how-to-fix-them)). I read summaries only, not the talk.

Unreal's [Spring Arm](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/USpringArmComponent) has a collision probe (`bDoCollisionTest`, `ProbeSize`, `ProbeChannel`) that shortens the arm when something blocks it. Forum reports say a large probe in tight rooms makes the camera snap in ([forum](https://forums.unrealengine.com/t/how-do-i-fix-this-camera-problem/768858)). That is the cave problem: a 12 m sub in a 4 m tunnel will drag a chase camera into the hull or the rock.

| View | Good | Bad |
|---|---|---|
| Chase (behind/above) | See hull and both walls, judge the squeeze | Arm fights tunnel walls; hull hides the view ahead; loses "inside a sub" |
| Cockpit (today) | Fits fear and co-op (helm player is in the room) | Cannot judge a 12 m hull; sees only the nose |
| Hybrid: cockpit default, hold or toggle for a chase "drone" view | Both, cheap to try | Two things to tune; chase can make the game too easy |

Chase notes: the arm should be short (about 12 to 16 m behind, 4 m up) with a probe about 1 m, pull in smoothly, and fade the hull when the camera is inside it. Look-ahead (aim at a point on the path ahead, not the sub's nose) is the biggest fix in either mode.

## 4. Handling
No single source gives numbers for this; treat these as starting values to test, not facts.
- **Arcade-leaning feel:** time to full speed under 2 s, stop under 1.5 s. Today's drag of 0.45/s means about 2.2 s to 63 % of the speed change and a long coast [Code; arithmetic mine].
- **Direct steering:** velocity follows input. Keep a little lag for weight, but remove the notch throttle. Hold W to go, release to stop.
- **Strafe and vertical:** A/D strafe, Space/Ctrl (or C) for up and down, mouse yaw/pitch. Free movement is what made the Seamoth easy [Read, see table]. A true 6-axis sub does not match "never pitches or rolls" yet, so start with yaw plus strafe plus heave, and add pitch only if tunnels need it.
- **Gamepad:** left stick move/strafe, right stick look, triggers for heave. Mouse steer and stick steer need the same "aim at where I look" rule.
- **Collision:** slide along rock, scale damage by impact speed along the normal (scrape = small, head-on = big), and cut speed only on the hit, not stop the throttle. Today a knock trips the throttle to stop [Code]. This punishes, and it also stops the sub after each scrape.
- **Assists (all toggles):**
  - Centreline nudge toward the path in `deep/Content/Data/CaveChunkPath.csv`, only when no input is held and only in the squeeze. Keep it soft (under 0.5 m/s) so it feels like water, not rails.
  - Auto-brake before a hit above a speed threshold.
  - Proximity ping or audio tone for each side of the hull in the squeeze (the Iron Lung proximity idea, see [Destructoid](https://www.destructoid.com/review-iron-lung-pc-horror-game/)).

## 5. Seeing without losing the horror
Existing rule: keep it dark. The question is what the light *shows*.

- **Headlights.** Put them forward, level, and at the camera's height or just above. Aim along the travel axis (0 to -5 degrees, not -30). A long narrow cone for distance plus a wide dim fill for the walls. DRG is a cave game built on this: flares and lamps are the reading light, and a dev-tracker post says shadows on them were removed to stop chaos ([unverified relay](https://devtrackers.gg/deep-rock-galactic/p/0bd9f650-all-my-issues-with-drg-flashlight-short-video-comparison-and-analysis)).
- **Volumetric beams.** Each light has its own Volumetric Scattering Intensity, and the fog must be on and scattering set near forward ([Epic: Volumetric Fog](https://dev.epicgames.com/documentation/unreal-engine/volumetric-fog-in-unreal-engine), [Local Fog Volumes](https://dev.epicgames.com/documentation/unreal-engine/local-fog-volumes-in-unreal-engine)). A visible beam in murk is what makes rock silhouettes read and fits the style guide's "the beam is the subject".
- **Fog.** Subnautica's team tuned visibility range and fog falloff "constantly", per biome ([Fandom interview](https://www.fandom.com/articles/subnautica-charlie-cleveland-interview)). They treat limited sight as a mechanic ([Mein-MMO](https://mein-mmo.de/en/exclusive-why-we-hate-underwater-levels-but-love-subnautica-2-heres-what-6-of-the-game-designers-say,1570132/)). For us: raise visibility at the lamp's range only (say 25 to 30 m), keep it falling off beyond.
- **Exposure.** Ours is locked [Code]. Epic's modes include Manual and Histogram with Min/Max and speed-up/down ([Epic: Auto Exposure](https://dev.epicgames.com/documentation/unreal-engine/auto-exposure-in-unreal-engine)). A locked value cannot fix a sudden bright flare or a dark squeeze. Option: narrow histogram range (about 1 EV) with slow speed-down. Test, as the style guide's bias row says "exposure bias manual" by depth.
- **Fill and rim.** A faint cool rim on rock edges (the sub strip colour #BFE3EA, low) lets the tunnel shape read without lighting the room. Keep it local to the sub so it stays "light that is bought".
- **Wayfinding.** Man-made: dim markers, cable or buoys on the path, placed from the same path data. Flares that the player drops (DRG model) light a chosen spot and show where they have been. Bioluminescence is allowed only if Quinn approves it against the colour table (no new colours on rock).
- **Lingering sonar.** Today's ping is a thin band every 6 s [Code]. A version that leaves faint dots for 3 to 6 s (point-cloud reveal) gives shape without lighting. Barotrauma is a precedent for sonar that trades risk for sight ([wiki](https://www.barotraumagame.com/wiki/Sonar)); we can let loud ping wake the serpent.
- **Material.** Flat grey rock reflects the same everywhere. Wet specular and albedo variation (style guide section 4: wet rock roughness 0.35 to 0.55) give the beam something to catch. That is the other agent's lane; the point here is that it matters for readability.

## 6. Options for DEEP

### A. Keep the helm, fix the view (smallest)
Tilt the camera level, look-ahead, re-aim floods forward, add the volumetric beam, soft slide and speed-scaled damage. Keep notches and rudder.
- **Code:** `DeepSubmarine.cpp` camera and light angles; `DeepTuningTypes.h` light values; `StepPilotCave` response; `DeepPilotRules.h` unchanged.
- **Co-op:** unchanged.
- **Risk:** may still be "hard to control"; Quinn asked for something more traditional.
- **Judge:** Quinn reads the next squeeze without help; 3 test drivers each pass the squeeze in 3 tries or fewer.

### B. Arcade sub, cockpit view with chase toggle (recommended)
Direct input (W/S, A/D strafe, mouse yaw, heave), light inertia, slide collisions, centreline nudge and auto-brake as toggles, plus A's view and lighting fixes. Hold a key for a pulled-back chase view. Keep the lever model as a "Sim" mode setting.
- **Code:** new `FDeepPilotTuning` block and `DeepPilotRules.h` branch for direct mode; `UDeepRunComponent::StepPilotCave` takes a path-nudge term and a sliding impact; a Spring Arm (probe about 1 m) on `ADeepSubmarine` for the toggle; input mapping and a settings entry.
- **Co-op:** the helm player keeps the cockpit. The diver player can see the sub in third person and call gaps. The chase view should be helm-only, so the tether partner does not get a free camera.
- **Risk:** arcade handling might flatten the dread. Keep dread through darkness and sound, not through bad controls. The chase view might remove the squeeze's tension.
- **Judge:** (1) from start to the C1 relic, fewer than 5 hull hits for a new driver; (2) average speed in tunnels at least 60 % of top speed; (3) squeeze passable first try by 2 of 3 testers; (4) in a dark-room frame with HUD off, can Quinn point out the next turn at 20 m.

### C. Full chase-camera sub (traditional video game sub)
Third-person always, free 6-axis flight, auto-levelling.
- **Code:** all of B plus pitch and roll in the sim and a spring-arm camera replacing the helm view; hull fade; the helm interior loses purpose.
- **Co-op:** weakest. The helm room becomes a menu.
- **Risk:** large; loses the cockpit identity that the whole "tether and berth" loop leans on; chase camera in a 4 m tunnel is the hardest camera case [Read: Nesky].
- **Judge:** same as B, plus no camera clip in the squeeze across 20 runs.

## 7. Recommendation
**B.** Do it in two steps so Quinn can look in between:
1. Light and camera pass (A): level the view, forward floods, volumetric beam, lingering sonar. Needs frames, per project rule.
2. Direct-input mode with a `Sim` fallback, then the chase toggle only if step 1 and 2 still leave the squeeze hard.

I challenge one thing: do not widen visibility by raising fog or exposure globally. Raising local light range and aim, and adding player-placed light (flares), hurts the style least.

## Gaps
- Diluvion, GTA V submersibles and Subnautica developer talks were not sourced.
- Control numbers in section 4 are my starting values, not from a source.
- No tests or play were run for this note.

## Orchestrator's read (Nov 25, 1:56 AM)
- **Agree:**
  - Direct controls replace the notches and the latched ballast.
  - The sub slides along rock, with damage by impact speed and no throttle trip.
  - The camera is level and looks ahead, and the floods aim forward with visible beams.
  - Assists are toggles, and there's no global exposure or fog change.
- **Differ: chase camera by default, cockpit on a key.**
  - The hull is 12.3 m long with about 1 m to spare in the squeeze. The pilot must see it against the rock to fly it fairly (section 1, point 3), and the cockpit can't show that.
  - No passage is narrower than the hull itself (3.3 × 5.0 m, plus the 0.9 m or more of clearance the generator builds round the hull's path), so the "4 m tunnel" above doesn't apply.
  - A camera that follows the hull's own trail sits in space the hull has just swept clear of rock. A probe only has to handle turns in place and reversing.
  - Co-op is unchanged, because only the pilot's view switches. The view can read as the sub's tail camera on the helm screen.
  - Quinn judges both views in one play build.
- **Inputs:** keys steer by default: W/S thrust, A/D turn, Space/C up and down, Q/E strafe, with the mouse free to look. "Drive where you look" (the mouse steers) is an option in the same build. Quinn picks after driving both.
- **Light:** the floods are the pilot's lamp, so they stay strong and level. The Style Guide's depth cut (40% at 300 m, 8% at 500 m) then applies only to the outside lights the diver sees. That split needs Quinn's OK (D-030).
- **Wayfinding:** an old cave-diving guideline along the route, with arrows that point the way out and catch the floods. It's real cave-diving practice and needs no emissive. The rope renderer already exists (`UDeepRopeComponent`). I prefer it to the material note's painted route band.
- **Order:**
  1. "See and steer": camera, controls, collisions and floods, with frames and a play build for Quinn.
  2. The caves' look pass: the material, the guideline and a sonar that lingers.
