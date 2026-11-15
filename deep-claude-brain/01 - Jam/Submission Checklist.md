---
updated: 2026-11-15 6:30 PM (freeze package done and saved; itch draft, shot list and credits checked against the build; dev log post 2 being drafted)
verified: Requirements from the itch.io jam page (Nov 14), status from the repo and from what the orchestrator reported on Nov 15. The Official Rules PDF has still not been read.
---
# Submission Checklist

Target: **submitted by 8:00 PM EST on Sunday Nov 15**, one hour before the 8:59 PM cutoff. Uploads fail and forms time out. Late is disqualified.

Status key: DONE, MISSING (nobody has done it), WAITING (only Quinn or Colby can do or confirm it).

## The build

- DONE Packaged for Windows 10/11. The freeze run-through passed at 6:20 PM Nov 15 on integration `9ac970e`: verify 164 of 164, all 14 scripted playthroughs, package 1,048 MB, a clean smoke run, zip 762.9 MB. Kept in `Builds\Freeze-1820` (outside git); the zip's SHA-256 starts `47B688DC739AC215`. If Quinn's calls change the build, only what they touch runs again, and the new build goes in a new `Builds\Freeze-<time>` folder. Source: the run's own summary, Devlog 6:20 PM.
- DONE (rechecked at the freeze, 6:20 PM Nov 15) The zip is 762.9 MB against the 1 GB upload limit, with the Dragon Cave rocks, stones and relic in it. The unzipped folder is 1,048 MB; the limit is on the upload, the zip ("1 GB zipped" in [[Polish - Plan and Techniques]]). Without the texture limit in `deep/Config/DefaultDeviceProfiles.ini` the zip was 1,318 MB (8:47 AM), over the limit. Source: package and zip logs. Recheck at every package.
- WAITING Zip named `TeamName_GameName.zip`. Team name is not chosen. Source: this list, Nov 14.
- MISSING Runs on a second machine that has never had the project on it (Colby's PC). Source: no record in the Devlog.
- MISSING Played start to finish by a human with real keys. No hand play has been reported. Two builds are ready for one: `Builds\HandPlay-1800` (5:59 PM) and the freeze build `Builds\Freeze-1820`. Scripted playtests exist; they are not a hand play. Source: orchestrator report.
- DONE (in code) A way to quit, and restart after a win. Title, pause and win menus have quit; the win menu has "dive again". Source: `deep/Source/Deep/Game/DeepPlayerController.h`. Not yet confirmed by hand.
- DONE No API keys or secrets in the repo. No live AI model is in the build (cut in the GDD). Source: GDD Cut list. Re-scan the package before upload.

## The itch.io page and jam form

- MISSING The page does not exist yet. Draft copy refreshed at about 6:05 PM Nov 15 for the re-scoped game (D-048, D-049) and checked against integration `9ac970e`; at 6:30 PM the asset and sound lines were settled from the build. TODOs for Quinn remain: the theme sentence, the start time, Colby's part, the dev log link, the Fab licence, the tools list, the minimum spec and the team name. Source: `devlog/itch-page.md`.
- MISSING Game uploaded to our own itch.io account, then submitted to the jam.
- WAITING Team name, and each member's name plus the email tied to their Epic Games account. Only Quinn and Colby have these.
- DONE Game name: DEEP. Source: `CLAUDE.md`.
- MISSING Link to a **30 to 60 second gameplay video**. Not recorded yet. The shot list was rewritten for the re-scoped game at 6:05 PM ([[Video Shot List]]). Record it by hand in OBS on `Builds\Freeze-1820\Windows\Deep.exe`, and start the upload by 7:45 PM. Stills are in `devlog/media/`; none of it is video. The machine has no ffmpeg, so a scripted recording has no encoder. Source: Devlog 6:20 PM.
- MISSING Dev log link. Required for Glass Box. Post 1 (`devlog/01-saturday-night.md`, Saturday night) is written. Post 2 (`devlog/02-sunday.md`, Sunday up to the freeze) is being drafted at 6:30 PM. Nothing is published. Quinn decides where; itch.io devlog posts on the game page are what other entrants use.
- WAITING "Tools you used" field. Current list: Unreal Engine 5.9 MegaJam Preview, Unreal MCP, Claude Code, plus Obsidian and Google Docs for notes. Quinn confirms the final list.
- WAITING List of third-party assets and licences. One pack is in the build: Dragon Cave (Fab) by Denys Rutkovskyi, listed in `Credits.csv` as "Fab, personal licence". Since 2:20 PM it supplies the cave rocks, the stones and the relic, and the serpent's head is built in code. The in-game row is corrected on `rescope/notag` (`dc19f58`) and goes in with the next package. No Megascans and no recorded sound are in the build; all sound is made in code. Quinn confirms which Fab licence was bought. Source: `deep/Content/Data/Credits.csv`, the freeze package's file index, D-034.
- WAITING NVIDIA questions, if we qualify. The build uses hardware ray tracing, which is not by itself an NvRTX, DLSS or ACE feature. DLSS has no build for 5.9; one try with the 5.8 plugin waits on Quinn's download (D-032). Without it, not eligible. Source: GDD Cut list, [[Track Strategy]].
- DONE (n/a) OpenAI question. No OpenAI model is used in the game or, as far as the repo shows, in its making. Answer "none" unless Quinn used one. The Captain's voice, not started and waiting on Quinn's key, would change this answer. Source: `Credits.csv`, [[Track Strategy]].
- WAITING Attribution text where a tool or asset requires it. Depends on the Dragon Cave licence terms. No Megascans are used.

## Requirements the old list did not have

- WAITING **Student eligibility.** Best Student Project needs enrolment, and whether all members must be enrolled is in the unread Official Rules. Quinn and Colby confirm; someone reads the Rules on Box in a browser. Source: [[Jam Brief]], [[Track Strategy]].
- WAITING **Age 18+ and an active Epic Games account** for every member. Source: [[Jam Brief]].
- MISSING **Read the Official Rules PDF.** Never read. Source: [[Jam Brief]], "Not yet known".
- DONE Nothing built before Nov 12, so nothing to declare. Source: [[Jam Brief]].
- DONE Gameplay goes beyond the starter templates (own C++ module). Source: repo.
- DONE Engine rule: Unreal only, 5.9 MegaJam Preview. Source: `CLAUDE.md`.
- MISSING **Theme on the page.** The game no longer says the theme (D-025), so the page must, in one sentence. Redrafted in `devlog/itch-page.md` (Nov 15); Quinn approves. Source: D-025.
- MISSING **Best Team** is automatic with two people, but the team must be entered as a team on the form. Source: [[Track Strategy]].
- DONE (n/a) Hyper3D cash pool: needs Rodin. Not used, so nothing to redeem. Source: [[Track Strategy]]; nothing in the repo or Devlog mentions Rodin.
- WAITING **Check the judged criteria on the first two minutes**: theme, loop and look must show without explanation. Needs a human hand-play. Source: [[Jam Brief]].
- WAITING Entrants keep ownership; no action, but do not upload under a licence that says otherwise. Source: [[Jam Brief]].

## Things to collect as we go

- **Tools used:** see above.
- **Third-party assets:** Dragon Cave (Fab), seller TODO; Megascans if used.
- **OpenAI use:** none.
- **NVIDIA use:** none claimed.
- **Dev log:** raw entries in [[Devlog]]; drafts in `devlog/`.

## Rehearsal

Package and zip a build long before the deadline, and repeat. A build that has never been packaged is the most common way to miss a jam. The freeze zip is 762.9 MB (6:20 PM Nov 15, integration `9ac970e`).
