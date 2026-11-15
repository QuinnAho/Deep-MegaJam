# Sound Brief

This brief covers two things: the Fab sound pack Quinn picks, and the music and serpent sounds his brother makes (D-040). The names and paths are fixed, because the game finds each sound by its file name. The look and sound rules are in [[Style Guide]], section 9. The Captain's lines are in [[Captain Script]].

## The sound of DEEP

DEEP is about a cheap, worn salvage sub in a black ocean. Everything should sound heavy, low, wet and metallic, and real rather than sci-fi: no laser zaps, nothing cartoonish, no bright synth leads. The music plays under everything, on a bus 18 dB down, so it should be texture and pressure rather than melody. The only voice is the Captain, who speaks over a radio.

## The Fab pack: what to look for

- Pick one pack of realistic sound effects that sound like one set. Add a second pack only to fill a gap.
- Search terms: underwater, submarine, deep sea, horror ambience, metal impacts, diving.
- What it must have, most important first:
  1. Metal hitting and scraping rock; hull groans, creaks and pressure pops. The cave shaft needs these most, for wall hits.
  2. Two or three underwater ambience loops that sound deep and distant.
  3. Sounds inside the sub: hum, electrics, ventilation, the motor and thrusters.
  4. The diver: regulator breathing (calm, fast and panicked), bubbles and a heartbeat.
  5. Machines: a winch and cable, a cable pulled taut and snapping, the hatch, and clanks of metal salvage.
  6. A harpoon firing and hitting.
  7. Electronics: a sonar ping, an alarm, and radio squelch and static.
- Three or more variations of every sound that repeats.
- WAV files, at 44.1 or 48 kHz.
- A licence that allows use in a released game; Fab's standard licence does. Write down the pack name and author for the credits.
- Add it with the launcher's "Add to project" into `deep/Content/`, or download it to any folder. Tell the orchestrator the folder name; it adds the folder to `.gitignore` and links it into the build checkouts.

## For Quinn's brother

### Music

All the music shares one key or tonal centre and one slow tempo. Aim for texture over melody: drones, low strings or pads, metal resonances.

| File | What it is | Length |
|---|---|---|
| `music_title` | The title screen: calm and ominous. | 60 to 90 s, loops |
| `music_depth1` | Under the first stop: uneasy. | 60 to 120 s, loops |
| `music_depth2` | Under the middle stops: darker and sparser. | 60 to 120 s, loops |
| `music_depth3` | Under the deepest stop: almost nothing but pressure. | 60 to 120 s, loops |
| `music_tension` | Plays over any of the depth beds while the serpent is near: a pulse and low hits, in the beds' tempo and key. | 30 s, loops |
| `music_sting_serpent` | The first time the serpent is seen. | 3 to 5 s |
| `music_win` | Back at the surface with the relic. | 10 to 20 s |
| `music_settlement` | Optional: detuned corporate hold music while the Captain takes his cut. | 30 to 60 s, loops |

### The serpent's voice

Make three variations of each sound, named `<file>_01.wav`, `<file>_02.wav` and `<file>_03.wav`. The serpent should sound like low, whale-like groans, creaks and resonances, not roars: big, slow and far away, until it is close.

| File | When it plays |
|---|---|
| `monster_first_heard` | The first time the diver hears it: a distant call. |
| `monster_near` | It is close: a rumble with a falling pitch. |
| `monster_repelled` | The lamp or a harpoon drives it off: a pained retreat. |
| `harpoon_hit_monster` | A harpoon hits it. |
| `death_monster` | It takes the diver. |
| `loop_serpent` | A seamless loop of it moving or breathing, played while it is near. |

### Optional: the Captain's voice

There are 74 short lines in [[Captain Script]], each with a voice note and a tone. The Captain is a smug salvage boss on a radio: smug when things are calm, clipped when they are tense, and cracking when he is scared. Record the lines dry, with no radio effect, because the game adds that. Make one file per line, named by its line id, for example `bank_good_01.wav`.

### Specs

- WAV, 48 kHz, 16-bit. 24-bit works too, but 16-bit keeps the repository small, and it has no large-file storage.
- Mono for the serpent and the voice; stereo for music.
- Loops must wrap without a seam: no fades, cut on a zero crossing, and no click at the wrap.
- Peaks at -1 dBFS. Don't squash the mix with a limiter; the game does the mixing.
- Keep any reverb on the serpent short, because the game places it in the world.
- Use the names exactly as given, in lower case.

### When and where (EST, Sunday Nov 15)

- First files by 2:00 PM: one music bed and one serpent sound, to test the path into the game.
- Everything else by 3:30 PM. Feature freeze is at 5:30 PM.
- Put the files in `C:\Repositories\Deep-MegaJam\audio-drop\` on Quinn's machine. Git ignores that folder.
- Who imports them is open. Either Quinn imports them in the editor, telling the orchestrator first because an open editor blocks the agents' builds, or the Captain-and-sound agent imports them headless into `deep/Content/DeepAudio/` once Quinn allows it.

### Credit

He can join the team (up to five people) or be credited as an outside contributor. Quinn decides which.
