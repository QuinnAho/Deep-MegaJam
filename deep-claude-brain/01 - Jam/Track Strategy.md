---
updated: 2026-11-14 20:50 EST
---
# Track Strategy

Facts are in [[Jam Brief]]. This note is Claude's read of which prizes are worth effort with about 24 hours left and two people. Odds are judgement, not data.

| Track | What it needs from us | Extra effort | Claude's read |
|---|---|---|---|
| Best Overall (UE) | A finished, fun build that fits the theme. | None beyond making the game. | Enter. Long odds against teams that had four days, but everything else depends on the same build. |
| Best Team | Two or more people on the team. | None. | Enter. Automatic. |
| Best Student Project | Current enrolment at a higher-education institution. | Proof of enrolment if asked. | **Enter if both are enrolled.** Smaller pool than Best Overall, same prize money. Needs Quinn and Colby to confirm, and someone to read the Official Rules. |
| Glass Box | A dev log link in the submission. | Low. Agents draft it from [[Devlog]], commits and screenshots. | **Best value per hour.** An agent pipeline with a real validation loop and an honest cut list is a strong story. Start capturing now. |
| Render Royalty | The best-looking game. | High to win. Medium to look good. | Do the cheap, high-impact look work (fog, light shafts, particles, a colour script per depth zone). Do not chase it at the cost of gameplay. |
| NVIDIA RTX Award | DLSS, ACE or NvRTX in the build, and top rank among those Render Royalty entries. | Medium, and blocked on 5.9. | **At risk.** No DLSS plugin build exists for 5.9. See [[NVIDIA RTX, DLSS and ACE]] and [[Engine Choice - 5.9 vs 5.8]]. |
| OpenAI Awards | OpenAI models or products used in development or gameplay, and a written answer at submission. | Low. | Generate the Director's voice with OpenAI text-to-speech. Needs an API key from Quinn. Prize is API credits, so do not spend much on it. |
| Hyper3D cash pool | Use Rodin for at least one asset, list it under tools, and win any category. | Very low. | Generate one hero asset (the relic or the monster) with Rodin. Redeem the perk by Nov 16. |

## What follows from this

1. The game itself is the priority. Six of the eight tracks only pay if the build is good.
2. The dev log is the one track where our process is the entry. Every session appends to [[Devlog]].
3. Audio and progression are judged. Budget time for both.
4. The submission form has free-text answers for NVIDIA and OpenAI. Keep notes on how each was used so the answers take five minutes. See [[Submission Checklist]].

## Challenge to the original plan

Quinn's first table listed "Render Royalty + NVIDIA RTX" as "the underwater look, plus ACE (DLSS as backup)". Two corrections from the research:

- ACE is not practical for this jam. The on-device speech and language models are several gigabytes against a 1 GB build limit, need an RTX GPU on the judge's machine, and the plugins target UE 5.7.
- DLSS is the practical route, and it currently stops at UE 5.8.
