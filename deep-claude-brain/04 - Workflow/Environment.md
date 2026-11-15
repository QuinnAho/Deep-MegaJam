---
updated: 2026-11-14 20:55 EST
verified: Read from Quinn's machine on Nov 14 2026 with read-only commands. Colby's machine has not been looked at.
---
# Environment

## Quinn's machine

| | |
|---|---|
| OS | Windows 11 Pro 10.0.26200 |
| CPU | Intel Core i9-14900HX, 24 cores, 32 threads |
| RAM | 32 GB |
| GPU | NVIDIA GeForce RTX 4080 Laptop GPU, 12 GB, driver 596.49 |
| Disk | About 1.1 TB free on C: |

## Unreal Engine installs

All under `C:\Program Files\Epic Games`.

| Version | Folder | Notes |
|---|---|---|
| 5.7.4 | `UE_5.7` | |
| 5.8.3 | `UE_5.8` | The DLSS plugin has a build for this version. |
| 5.9.0 MegaJam Preview | `UE_5.9` | Changelist 60351404, branch `++UE5+Release-5.9`. Finished installing about 8:10 PM on Nov 14. Found under "Other builds" in the launcher. |

## Tooling present

| Tool | Version | Note |
|---|---|---|
| Visual Studio 2022 Community | 17.14 | MSVC toolsets 14.38 and 14.44. Windows SDK 10.0.22621. |
| git | 2.54 | |
| git-lfs | 3.7.1 | |
| Node.js | 24.15 | Needed for the Claude Code adapter in the Unreal Assistant panel. |
| Python | 3.12 | Use the `py` launcher. Plain `python` is the Microsoft Store alias and fails. |
| .NET SDK | 9.0.316 | |
| Claude Code | 2.1.325 | |
| Steam | installed | |

**Not installed:** GitHub CLI (`gh`), `uv`, CMake. Without `gh`, pull requests cannot be opened from the command line. Plain `git push` still works.

## The project as created

- **Path:** `C:\Repositories\deep-megajam\deep\Deep.uproject`. Quinn created it at 8:38 PM on Nov 14 and moved it into the repo about ten minutes later.
- **Engine association:** a GUID, not "5.9". The preview is registered as a custom build on this machine. Expect Unreal to ask which engine to use when the project is opened on another PC.
- **C++ project since 8:55 PM Nov 14.** Quinn added a module named `Deep` (`deep/Source`) and it built. It was Blueprint-only when first created.
- **Plugins:** the only entry is ModelingToolsEditorMode. **All Toolsets is not enabled yet**, so agents have no level, asset or Blueprint tools. See [[Unreal MCP in UE 5.9]].
- **Content:** empty apart from the `Collections` and `Developers` folders.
- **`DefaultEngine.ini` highlights:**
  - Lumen global illumination and reflections (`r.DynamicGlobalIlluminationMethod=1`, `r.ReflectionMethod=1`).
  - `r.RayTracing=True`.
  - Substrate on (`r.Substrate=True`).
  - DirectX 12.
  - Default map is the engine's OpenWorld template (`/Engine/Maps/Templates/OpenWorld`).
- **MCP:** port 8000 was listening while the editor was open, which matches the MCP server. No tool call has been made yet.
- The editor was closed when last checked at 8:51 PM.

## Repo state

- **Folder:** `C:\Repositories\Deep-MegaJam`. Windows ignores case, so `deep-megajam` is the same folder.
- **Origin:** https://github.com/QuinnAho/Deep-MegaJam.git
- **No commits yet.** `deep/` and `deep-claude-brain/` are both untracked.
- **No `.gitignore` or `.gitattributes` yet.** Before the first commit the repo needs ignore rules for Unreal's generated folders (`Binaries`, `Intermediate`, `Saved`, `DerivedDataCache`) and Git LFS rules for `.uasset` and `.umap` files.
- **An older copy of the project still exists at `C:\Repositories\Deep`.** Do not open it by mistake. Work saved there never reaches the repo.

## Colby's machine

Unknown. What we need to learn:

- GPU and how much video memory.
- Which Unreal version is installed. The 5.9 preview is under "Other builds" in the launcher.
- Whether he has Claude Code and his own Claude account. See [[Teammate Access]].
- Whether git and git-lfs are installed.
- Whether Visual Studio 2022 is installed, if the project gains C++.
- Free disk space.

His PC is also the second machine for the clean-install test in [[Submission Checklist]].

## Connectors available to Claude in this session

- **Google Drive:** read, create and share files. It **cannot edit an existing Google Doc in place**, because the Google Docs editor connector is not turned on. Turning it on in the claude.ai connector settings would let Claude edit the team's doc directly.
- Web search and page fetch.
- Gmail, Google Calendar, Slack, Notion and Linear connectors exist but are not signed in.
