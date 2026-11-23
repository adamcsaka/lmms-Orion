# Universal AI Agent Guidelines for LMMS Orion

> This file (`AGENTS.md`) is a cross-platform configuration file automatically recognized by AI coding assistants (including OpenCode, Antigravity, Claude Code, Cursor, and Aider).

## 1. Project Context & Build Architecture
- **Repository Path:** `C:\lmms\lmms-repo`
- **Master Guide:** Follow `C:\lmms\lmms-repo\LMMS_ORION_MASTER_GUIDE.md` for toolchain paths, local build commands, and deploy scripts.
- **Git Push Invariant:** Always pass `-c credential.helper=` on git commands (`& 'C:\lmms\git\cmd\git.exe' -C 'C:\lmms\lmms-repo' -c credential.helper= push origin <branch>`) to prevent Windows credential helper popup hangs.
- **Local Compilation:** Always execute `build-local.ps1` or run ninja reserving 3–4 CPU cores free (`[Math]::Max(1, $env:NUMBER_OF_PROCESSORS - 4)`). Never pin 100% of CPU cores.
- **Portable Deployment:** Run `deploy-v7.ps1` to update `C:\Users\ahsoka\Downloads\LMMS-Orion-v7`.
- **Deployment Isolation (Anti-Locking Rule):** Never overwrite an actively opened deployment directory. If the destination is locked by a running process, deploy into an incremented versioned directory or timestamped folder.
- **Changelog Tracking:** Always update `CHANGELOG.md` with every commit or milestone, detailing what was changed and why.

## 2. Multi-Session & Git Awareness
- Always inspect `git log -n 5` and `git status` before starting analysis or edits. Another concurrent session may have already resolved or started work on the issue.

## 3. LMMS Audio & Architectural Invariants
- **MIDI Fractional Step Boundaries:** Calculate note end ticks with `note->pos() + ticksPerStep`, not legacy `pos() + 1`. Maintain dynamic two-way adaptation using `m_userSteps`.
- **Envelope Rendering in Synths:** Internal ADSR synths must NOT call `applyRelease(workingBuffer, n)` in `playNote()`.
- **Effect Rack Clearing:** Always call `fxChain()->clear()`, never just GUI `clearViews()`.
- **Randomizer Safety:** Never mute tracks, never modify mixer routing (`fxChannel`), never change MIDI groups (`group`), and keep tuning/pitch harmonic. Always snapshot undo state (`Song::saveUndoState`).
- **Fail-Safe Pointer Checks:** Always guard dynamic pointers (`Track*`, `Instrument*`, `Effect*`, `Model*`) with null checks (`if (!ptr) { ... }`) before dereferencing.
