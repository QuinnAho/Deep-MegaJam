---
updated: 2026-11-15 01:45 EST
verified: Web pages read 2026-11-15 01:35 to 01:45 EST. Items marked UNCONFIRMED came from search snippets only (opengameart.org pages returned 403, kenney.nl pages gave no file format, Google Fonts pages need JavaScript so font licences were read from the google/fonts repo). The Official Rules PDF was not read.
---
# Polish: sound, voice, fonts and references

Branch `research/polish-web`. Humans download; nothing here was downloaded. Judges score Audio and Visuals, so this is cheap score ([[Jam Brief]]).

## 1. The rules

- Third-party assets are allowed "as long as you have the right license"; list them in the submission; include attribution the way the rights owner describes. Source: [jam page](https://itch.io/jam/2026-epic-ai-megajam).
- AI tools are allowed for any asset, audio included, and use is unlimited. Same page. No AI-specific disclosure rule appears there; "Tools you used" is optional but decides partner prizes (Hyper3D, OpenAI, NVIDIA). Same page.
- Do not include other people's IP. Declare anything made before Nov 12 (we have none). Same page.
- WeLoveIndies audio is free for non-commercial use only; commercial release needs a paid licence. Same page. Log each file used.
- Not confirmed: the Official Rules (Box PDF, linked in [[Jam Brief]]) were unreadable and they govern. A human should read the third-party and AI sections.
- Separate duty, not the jam's: OpenAI requires a clear disclosure that a TTS voice is AI-generated ([TTS guide](https://developers.openai.com/api/docs/guides/text-to-speech), [service terms](https://openai.com/policies/service-terms/)). Put it in the credits, the itch page and the submission.

## 2. Sound effects

Account needed: **Freesound yes** ([FAQ](https://freesound.org/help/faq/)). **Sonniss no** (direct zips, torrent, Drive mirror; account optional) ([gdc.sonniss.com](https://gdc.sonniss.com)). **Pixabay no** per its [FAQ](https://pixabay.com/service/faq/) and guides (one older library guide disagrees; UNCONFIRMED for WAV). Kenney and OpenGameArt: none found, UNCONFIRMED. Fab: Epic account, assumed.

Sources and licences:
- **Kenney**, CC0: [UI Audio](https://kenney.nl/assets/ui-audio) (50 files), [Impact Sounds](https://kenney.nl/assets/impact-sounds) (130 files). [Interface Sounds](https://kenney.nl/assets/interface-sounds) and [Sci-fi Sounds](https://kenney.nl/assets/sci-fi-sounds): licence not read, expect CC0 and check. File format not stated on the pages.
- **Sonniss GDC 2026 bundle**: 7.47 GB, 347+ files, royalty free, commercial use, no attribution; use for AI/ML training is prohibited and files may not be resold as sound effects ([source](https://gdc.sonniss.com)). Fine for us. Its track list was not read, so UNCONFIRMED that it covers our needs. Older bundles had regulator breathing ([2017 dive list](https://sonniss.com/wp-content/uploads/edd/2017/11/Track_list_dive.pdf)). Cost is download time.
- **Pixabay**: Pixabay Content License, commercial use, no attribution, no standalone resale ([summary](https://pixabay.com/service/license-summary/)).
- **Freesound**: licence per sound: CC0, CC-BY or CC-BY-NC. NC is not usable for us; use the CC0 filter ([FAQ](https://freesound.org/help/faq/)).
- **Fab, Outta This World Audio**: 33 free game sounds at $0 on both licences; the jam allows Fab assets if noted in the submission ([forum post](https://forums.unrealengine.com/t/free-on-fab-33-game-sound-effects-you-can-use-in-the-megajam/2851872), [listing](https://www.fab.com/listings/41e52f11-7620-464b-b0d6-490260eec914)). Contents not listed.
- **WeLoveIndies** (the jam's audio partner, see [[Partner Perks]]): own terms not found by me; only the jam page's non-commercial note.

Candidates. "Lic" is what a search result said; **check every licence on the page** (Freesound pages need login from here). I could not confirm a CC0 Freesound hit for most needs, so each row has a fallback.

| Need | Candidate | Lic / format | Fallback |
|---|---|---|---|
| Deep ambience | [DCSFX Underwater Loop AMB](https://freesound.org/people/DCSFX/sounds/366159/), 4 min WAV 48k | uploader says public domain; verify | [Verberne](https://freesound.org/s/482167), [akemov](https://freesound.org/people/akemov/sounds/255597/), lic UNCONFIRMED |
| Breathing calm and panic | [klankbeeld regulator, 10 m](https://freesound.org/people/klankbeeld/sounds/734301/), [31007](https://freesound.org/s/31007) | UNCONFIRMED | Sonniss dive pack; speed and pitch up the calm loop for panic |
| Bubbles | none found | | Freesound CC0 "bubbles"; Sonniss water |
| Electric winch | none found | | Kenney Sci-fi Sounds servo (UNCONFIRMED); a looped motor from the Fab sampler |
| Cable snap | none found | | Kenney metal impact plus a short noise burst |
| Hull creak | [vflefevre metal creak](https://freesound.org/people/vflefevre/sounds/240258/) | UNCONFIRMED | Sonniss; pitch a ship creak down |
| Sonar ping | [OpenGameArt Sonar Ping](https://opengameart.org/content/sonar-ping) | CC0 (snippet) | [SamsterBirdies](https://freesound.org/people/SamsterBirdies/sounds/539957/), UNCONFIRMED |
| Alarm | none found | | Kenney Sci-fi Sounds, or keep our coded two-tone |
| Metal impacts | [Kenney Impact Sounds](https://kenney.nl/assets/impact-sounds) | CC0 | Sonniss |
| Creature growl | [silent beast growls4](https://opengameart.org/content/silent-beast-growls4), [monster snarls](https://opengameart.org/content/monster-snarls) | CC0 (snippet) | [Morphonaut pack, 103 sounds](https://morphonaut.itch.io/monster-vocalization-sound-pack), CC0 (snippet) |
| Creature in water | none found | | Layer a pitched-down growl, low-pass, and the ambience |
| Harpoon shot | none found | | Air hiss plus Kenney impact; Sonniss |
| UI clicks | [Kenney UI Audio](https://kenney.nl/assets/ui-audio) | CC0 | [Interface Sounds](https://kenney.nl/assets/interface-sounds) |
| Hatch | none found | | Kenney impact layered with a creak |

## 3. Music

All OpenGameArt, CC0 per search snippets, format unconfirmed (pages returned 403). Read each licence line before use; one uploader changed old licences to CC0 and a readme conflict was reported on Underwater Theme II.
- Tense, outside: [Nautilus](https://opengameart.org/content/nautilus) (poinl), "gloomy"; [Aquaria](https://opengameart.org/content/aquaria) (cynicmusic), sample-accurate loop point in the WAV; [Submarine game music](https://opengameart.org/content/submarine-game-music) (Scribe), slow, made for a submarine game.
- Calm, inside: [Underwater Ambient Pad](https://opengameart.org/content/underwater-ambient-pad) (isaiah658), loop; [Underwater Theme](https://opengameart.org/content/underwater-theme) (Spring Spring), OGG; [Exploration Theme](https://opengameart.org/content/exploration-theme) (Cleyton Kauffman), loop.
- Avoid: Underwater Exploration and Eyes of the Ocean are CC-BY 4.0; Dark Ambience lists both CC-BY and CC0.

## 4. The Captain's voice (185 lines, about 13,000 characters)

| Option | Licence and terms | Cost, quality, setup |
|---|---|---|
| **OpenAI TTS** | Disclose AI voice; do not strip AI-origin indicators ([guide](https://developers.openai.com/api/docs/guides/text-to-speech), [terms](https://openai.com/policies/service-terms/)). Output ownership not found. | 13 voices: alloy, ash, ballad, coral, echo, fable, nova, onyx, sage, shimmer, verse, marin, cedar; guide recommends marin or cedar. `gpt-4o-mini-tts`, `tts-1`, `tts-1-hd` shut down **Jan 6 2027**: fine, render once and ship WAVs. tts-1 is $15 per 1M characters, so under $0.25 for all lines; tts-1-hd $30 ([pricing](https://developers.openai.com/api/docs/pricing)). gpt-4o-mini-tts price not on that page. Key stays on the dev machine. About 15 min. |
| **Kokoro-82M** local | Apache-2.0 weights; some training data CC BY (Koniwa, SIWIS), credit them ([model card](https://huggingface.co/hexgrad/Kokoro-82M)). espeak-ng is GPL but we ship only audio. | 54 voices, 8 languages; card claims quality near larger models. Windows: `pip install kokoro-onnx soundfile` avoids espeak-ng per a third-party [guide](https://pykokoro.readthedocs.io/en/stable/installation.html). Setup 30 to 60 min, my estimate. |
| **Piper** local | Engine GPL-3.0-or-later. Voices carry their own licences and some are restrictive; lessac-based voices exclude commercial use ([LICENSING](https://github.com/OHF-Voice/piper1-gpl/blob/main/docs/LICENSING.md), [VOICES](https://github.com/OHF-Voice/piper1-gpl/blob/main/docs/VOICES.md)). | `pip install piper-tts`. Checking each voice card costs time; skip. |
| **Windows voices** | No Microsoft text on redistributing output found. A Microsoft Q&A answer says distributing audio needs a commercial licence ([Q&A](https://learn.microsoft.com/en-us/answers/questions/326449/using-microsoft-tts-for-commercial-use)); community answer, not official. | Do not ship. |
| **ElevenLabs free** | Free plan has no commercial licence; attribution wording rests on secondary sources ([help](https://elevenlabs.io/docs/help-center/legal/can-i-publish-the-content-i-generate-on-the-platform)). 10,000 credits a month ([pricing](https://elevenlabs.io/pricing)). | Do not use. |

**Primary: OpenAI TTS** (marin or cedar, or onyx for dry), rendered once to WAV, with a visible AI-voice disclosure. It also feeds the OpenAI award question. **Fallback: Kokoro**, with its CC BY data credited. I tested neither.

Intercom chain (standard ffmpeg filters, not run by me): `ffmpeg -i in.wav -af "highpass=f=450,lowpass=f=3200,acrusher=bits=10:mode=log:mix=0.3,acompressor=threshold=-18dB:ratio=6,volume=1.4" out.wav`. Squelch: `ffmpeg -f lavfi -i "anoisesrc=d=0.12:c=white:a=0.4,bandpass=f=1800:w=1200" squelch.wav`, played before each line and a shorter, quieter one after. Add a faint hiss bed under the line.

## 5. Fonts (all SIL OFL 1.1; licence read from the [google/fonts repo](https://github.com/google/fonts/tree/main/ofl))

1. **B612 Mono + B612**: [B612 Mono](https://fonts.google.com/specimen/B612+Mono), [B612](https://fonts.google.com/specimen/B612). Airbus-led, made for cockpit screens, legible when tired or in glare ([source](https://kottke.org/23/09/0043069-b612-is-an-highly-legible)). Mono numerals and a fitting instrument-panel story.
2. **Chakra Petch + IBM Plex Mono**: [Chakra Petch](https://fonts.google.com/specimen/Chakra+Petch) for headings and the Captain's name, [IBM Plex Mono](https://fonts.google.com/specimen/IBM+Plex+Mono) for gauges. Angular, technical, calm digits.
3. **Barlow Condensed + Share Tech Mono**: [Barlow Condensed](https://fonts.google.com/specimen/Barlow+Condensed) for labels, [Share Tech Mono](https://fonts.google.com/specimen/Share+Tech+Mono) for numbers. Narrow, so labels fit small gauges.

The reasons beyond B612's purpose are my judgement. Use the mono face for every changing number so digits do not jitter.

## 6. What makes small underwater and horror games look professional

General techniques; sources are talks, articles and docs, not a look to copy.
1. **Light is the subject.** Subnautica's team said lighting was the biggest factor in making water read as water ([Game Developer](https://gamedeveloper.com/design/how-i-subnautica-i-plunges-deeper-into-rendering-realistic-water)). Few lights, strong falloff, dark elsewhere.
2. **Visibility is a rule.** Fog distance is a design choice: too short is claustrophobic and monotonous ([Unknown Worlds forum](https://forums.unknownworlds.com/discussion/comment/2187138)). Make light reach and depth fog part of play.
3. **Local fog, not global.** Epic advises low global density, raised locally with volume particles; cost follows volume resolution ([Epic docs](https://dev.epicgames.com/documentation/unreal-engine/volumetric-fog-in-unreal-engine)).
4. **Limited palette, one accent.** Dominant neutrals, support colours, one accent reserved for what matters ([article](https://bugnet.io/blog/color-palettes-for-indie-games)). Blue-black plus one warning orange for the console, alarms and harpoon.
5. **Particles for scale and motion.** Drifting particles read as water and add depth, with blur for distance ([Thrive devlog 3](https://www.revolutionarygamesstudio.com/mini-update-3-underwater-sensation-pt-2), [devlog 2](https://www.revolutionarygamesstudio.com/mini-update-2-underwater-sensation-pt-1)). Keep them fixed in world space, not following the camera ([forum complaint](https://forums.unknownworlds.com/discussion/156365/below-zero-bz-polish-not-very-underwater-y)).
6. **Interface built into the world.** Dead Space put nearly all its UI in the fiction ([GDC Vault](https://gdcvault.com/play/1018819/Crafting-Destruction-The-Evolution-of)); one analysis says keep diegetic items to 3 or 4 at once ([HUDs and GUIs](https://www.hudsandguis.com/2012/08/22/dead-space-2-diegetic-interface-design)). Put depth, air and tether on the wrist and console.
7. **Helmet framing.** A mask can carry what a HUD would, through cues and sound ([GameGrin](https://gamegrin.com/articles/the-power-of-the-metro-gas-mask)); screen-glued visor UI can feel stuck to the face ([O'Reilly](https://www.oreilly.com/library/view/unity-virtual-reality/9781783988556/ch05s02.html), a VR source). Use a subtle rim, breath fog and a few droplets, not a full overlay.
8. **Restraint in the HUD.** Keep the screen clean and read information from in-world objects ([Exodus review](https://gamersdecide.com/articles/why-metro-exodus-provides-immersive-experience)). Show numbers only when they matter.
9. **Sound-led dread.** Alien: Isolation used ambiguous sound and a real-time adaptive mix ([GDC Vault](https://gdcvault.com/play/1021852/Building-Fear-in-Alien), [Sound Architect](https://thesoundarchitect.co.uk/?p=2402)). Our serpent hunts by sound, so make the player's own noise audible and meaningful.
10. **Dynamic range and silence.** Plan loud and quiet as a curve ([Bridgett](https://gamedeveloper.com/audio/dynamics-of-narrative)); duck ambience under the Captain ([Wwise HDR](https://www.audiokinetic.com/en/blog/wwise-hdr-overview-and-best-practices-for-game-mixing)). Near-silence with sharp cues beats constant noise.

## 7. Recommendation

Three decisions. **Sound and music source:** Kenney CC0 (UI, impacts), OpenGameArt CC0 (sonar, growls, one music loop per space: Aquaria or Nautilus outside, Underwater Ambient Pad inside) and the Sonniss GDC 2026 bundle for water, regulator and creaks, using Freesound only where CC0 shows on the page; one human gets 60 minutes to hunt, logging every file and licence for the submission. Skip WeLoveIndies unless the non-commercial limit is accepted. **Voice:** OpenAI TTS (marin or cedar) rendered once, disclosed as AI, with Kokoro as the fallback, both through the intercom chain. **Fonts:** B612 Mono with B612 on the HUD. Also have Quinn read the Official Rules for third-party and AI terms.
