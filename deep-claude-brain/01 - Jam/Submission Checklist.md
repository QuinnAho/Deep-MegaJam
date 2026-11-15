---
updated: 2026-11-15 (writer, status pass; sources named per line)
verified: Requirements from the itch.io jam page (Nov 14), status from the repo and from what the orchestrator reported on Nov 15. The Official Rules PDF has still not been read.
---
# Submission Checklist

Target: **submitted by 8:00 PM EST on Sunday Nov 15**, one hour before the 8:59 PM cutoff. Uploads fail and forms time out. Late is disqualified.

Status key: DONE, MISSING (nobody has done it), WAITING (only Quinn or Colby can do or confirm it).

## The build

- MISSING Packaged for Windows 10/11. A package exists but predates the latest branch; repackage the final build. Source: orchestrator report.
- DONE (rehearsal) The last packaged build zipped to 427.2 MB against the 1 GB limit. Source: orchestrator report. Recheck after the Dragon Cave pack and Megascans go in.
- WAITING Zip named `TeamName_GameName.zip`. Team name is not chosen. Source: this list, Nov 14.
- MISSING Runs on a second machine that has never had the project on it (Colby's PC). Source: no record in the Devlog.
- MISSING Played start to finish by a human with real keys. Nobody has played the build by hand. Scripted playtests exist; they are not a hand play. Source: orchestrator report.
- DONE (in code) A way to quit, and restart after a win. Title, pause and win menus have quit; the win menu has "dive again". Source: `deep/Source/Deep/Game/DeepPlayerController.h`. Not yet confirmed by hand.
- DONE No API keys or secrets in the repo. No live AI model is in the build (cut in the GDD). Source: GDD Cut list. Re-scan the package before upload.

## The itch.io page and jam form

- MISSING The page does not exist yet. Draft copy is in `devlog/itch-page.md`. Source: orchestrator report.
- MISSING Game uploaded to our own itch.io account, then submitted to the jam.
- WAITING Team name, and each member's name plus the email tied to their Epic Games account. Only Quinn and Colby have these.
- DONE Game name: DEEP. Source: `CLAUDE.md`.
- MISSING Link to a **30 to 60 second gameplay video**. No gameplay video has been recorded. Source: orchestrator report. Stills from the polish pass are in `devlog/media/`; none of it is video. An agent can record a scripted run as a fallback (`SceneCaptureToolset.RecordViewport`), but a hand-played clip is better.
- MISSING Dev log link. Required for Glass Box. Two drafts exist in `devlog/`; nothing is published. Decide where (itch.io devlog posts are what other entrants use).
- WAITING "Tools you used" field. Current list: Unreal Engine 5.9 MegaJam Preview, Unreal MCP, Claude Code, plus Obsidian and Google Docs for notes. Quinn confirms the final list.
- WAITING List of third-party assets and licences. `Credits.csv` lists the engine and in-code sound only. Dragon Cave (Fab) is being added; its seller and licence text are to-dos. Any Megascans used must be added. Source: `deep/Content/Data/Credits.csv`, D-034.
- WAITING NVIDIA questions, if we qualify. The build uses hardware ray tracing, which is not by itself an NvRTX, DLSS or ACE feature. DLSS has no build for 5.9; one try with the 5.8 plugin waits on Quinn's download (D-032). Without it, not eligible. Source: GDD Cut list, [[Track Strategy]].
- DONE (n/a) OpenAI question. No OpenAI model is used in the game or, as far as the repo shows, in its making. Answer "none" unless Quinn used one. The Captain's voice, not started and waiting on Quinn's key, would change this answer. Source: `Credits.csv`, [[Track Strategy]].
- WAITING Attribution text where a tool or asset requires it. Depends on the Dragon Cave and Megascans licence terms.

## Requirements the old list did not have

- WAITING **Student eligibility.** Best Student Project needs enrolment, and whether all members must be enrolled is in the unread Official Rules. Quinn and Colby confirm; someone reads the Rules on Box in a browser. Source: [[Jam Brief]], [[Track Strategy]].
- WAITING **Age 18+ and an active Epic Games account** for every member. Source: [[Jam Brief]].
- MISSING **Read the Official Rules PDF.** Never read. Source: [[Jam Brief]], "Not yet known".
- DONE Nothing built before Nov 12, so nothing to declare. Source: [[Jam Brief]].
- DONE Gameplay goes beyond the starter templates (own C++ module). Source: repo.
- DONE Engine rule: Unreal only, 5.9 MegaJam Preview. Source: `CLAUDE.md`.
- MISSING **Theme on the page.** The game no longer says the theme (D-025), so the page must, in one sentence. Drafted in `devlog/itch-page.md`; Quinn approves. Source: D-025.
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

Package and zip a build long before the deadline, and repeat. A build that has never been packaged is the most common way to miss a jam. The last rehearsal build was 427.2 MB.
