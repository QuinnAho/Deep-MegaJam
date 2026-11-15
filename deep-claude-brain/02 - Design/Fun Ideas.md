---
updated: 2026-11-14 21:45 EST
---
# Fun Ideas (for Quinn to review)

Claude's suggestions for making DEEP more fun and more likely to win. **None of these are on the [[Board]] yet.** Tick the ones you want, cross out the rest, or tell Claude. Cost is agent time: S is under an hour, M is one to two hours, L is more.

The test for every idea: does it make "one more piece, or go home?" sharper, make the cut land harder, or make the Captain funnier? If not, it is not worth the hours.

Since 9:45 PM the voice is the Captain, and the Director is its unseen boss (see [[GDD]]). Where an idea below says "the Director" speaks, read "the Captain". The commentary mode keeps its film name: the Captain is made to read the Director's commentary and resents it.

## Claude's top five

- [ ] **1. Director's commentary mode.** Cost S to M. A "Director's Cut" edition of a film ships with a commentary track. Ours is a menu toggle: while you play, the Director comments on how the game was made by AI agents in 24 hours. "This tether was written at 9:52 PM by the cheapest agent available." It is on theme, it is the Glass Box dev log inside the game, and this is the AI MegaJam. Needs about 25 extra lines and a dozen triggers. **This is the one idea I would add even if nothing else is.**
- [ ] **2. Dead divers stay where they died.** Cost S to M. Each replacement diver finds the last one's lamp still glowing in the dark, with the lost haul beside it. A corpse run: go back for it, or write it off? The lamps become landmarks and the trench fills with your own mistakes. Dark, funny, and it makes dying add to the level instead of resetting it.
- [ ] **3. Negotiate your own cut.** Cost M. At each new stop the Director offers a contract, two buttons, no shop: keep 40 percent, or take a worse split for a longer tether, or a better split with less patience. The theme becomes a choice the player makes. Replaces the upgrade shop we cut.
- [ ] **4. Film credits and a performance review.** Cost S. The run ends with "Divers expended: 4. Gross: 1,240. Director's cut: 496. Rating: Adequate." Then credits roll like a film: "Directed by the Director", then the real credits. The jam requires us to list third-party assets and tools anyway, so this is where they go.
- [ ] **5. The monster is a sound before it is a shape.** Cost S once the monster exists. A click that speeds up as it closes in, like a motion tracker. Players will remember the sound, and it lets the creature model be simple.

## Small touches worth having

- [ ] Hold to be cut loose, about 1.5 seconds, while the Director says "Are you sure? ... Splendid." Prevents accidents and builds the moment. S.
- [ ] The winch has a kick: field of view widens, the cable whines, and you swing if the tether goes taut at speed. S.
- [ ] Settlement screen counts down line by line with a sound for each fee. Skippable. S.
- [ ] Photo finish: reach the airlock with under 3 seconds of air and the Director awards a bonus of zero credits. S.
- [ ] The Director's praise rises with your haul, then turns the moment you are in trouble. Lines exist; needs triggers. S.
- [ ] Marine snow: drifting specks in the lamp beam, made from instanced meshes so it needs no assets. M.
- [ ] Title screen: "Press any key to waive your rights." Line exists. S.

## A live Captain (OpenAI), decide Sunday about 1 PM

Quinn is weighing a live AI brain for the OpenAI awards. It is not needed to be eligible: the award text is "uses OpenAI models or products, during development, gameplay, or both", so voice generated with OpenAI ahead of time already qualifies. A live brain makes a stronger case for "best use", and the prize is API credits, not cash. The scripted Captain stays as the default either way, because a judge may be offline and no key can ship inside the build.

It plugs into the event bus the engineer is building, so nothing built now is wasted. Three sizes:

- [ ] **A. Live lines.** Cost M. Each game event plus the game state goes to an OpenAI model, which returns a fresh insult as a subtitle. Falls back to the written lines on any failure or with no key.
- [ ] **B. Live voice.** Cost M more. The same line spoken through OpenAI text-to-speech at runtime.
- [ ] **C. Haggle with the Captain.** Cost L, the risky one. Hold a key and plead out loud; the model decides whether to grant more Patience, raise the Director's cut, or cut you now, by calling game functions. This is the "Jarvis" idea from the first notes and the strongest use of a live model, because the model changes the game instead of decorating it.

Open question for any of them: whose key? Either the player pastes their own (judges mostly will not), or a small relay holds ours with a spending cap, which needs about half an hour of setup and an internet connection on the judge's side.

## Bigger swings, only if we are ahead at noon Sunday

- [ ] The Director turns kind at stop 3. "Take your time." It is more unsettling than threats. Lines only. S.
- [ ] At stop 3 the monster ignores the lamp for the first two seconds. Tuning only. S.
- [ ] The hull creaks and loses rating the longer you stay at stop 3, a soft timer for the finale. M.

## Not worth the hours

Procedural terrain, more creature types, an inventory screen, a map, saving, difficulty modes, controller support.

## Build for the 30-second video

The submission needs a 30 to 60 second gameplay video, and many judges will watch it before they play. Its beats, in order: title joke, first grab with the Director's line, the settlement screen, the dark and the first click, the cut, the sprint, a photo finish. Anything on that list is worth polishing first.
