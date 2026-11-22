# LMMS Orion - Changelog & Modification Ledger

All notable changes, architectural improvements, bug fixes, and safety rules for the **LMMS Orion** digital audio workstation are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and adheres to semantic versioning milestones.

---

## [v7.0.0] - In Progress / Milestone

### Planned & In-Progress
- **GUI Modulation Stutter & Frame Rate Throttling:** Investigation and resolution of Qt GUI event-loop saturation caused by rapid parameter modulations (LFO, controllers, knobs, meters) to ensure 60 FPS smooth rendering regardless of modulation count.
- **AVX2 SIMD Mixing & Dynamic Polyphony:** High-performance real-time mixing pipeline with AVX2 SIMD vectorization, runtime CPUID detection, and SSE fallback.
- **Voice Stealing Engine:** Intelligent audible voice-stealing preventing release freezes and CPU overload on high-polyphony tracks.
- **Dedicated Isolated Deployment Pipeline (`deploy-v7.ps1`):** Prevention of Windows OS file locks (`EBUSY` / `UnauthorizedAccessException`) by automated deployment into versioned, isolated destination folders (`LMMS-Orion-v7`).

---

## [v6.0.0] - 2026-11-22

### Added
- **Instant Ctrl+Z Undo for Parameter Randomization:** Introduced `Song::saveUndoState` / `addJournalCheckPoint` before parameter randomization in `InstrumentTrack::randomizeParameters`, allowing immediate non-destructive revert via `Ctrl+Z` (`b453891a2`).
- **Acoustic Audio Ceiling Protection:** Enforced safe 70% Q resonance cap (`minVal + 0.70f * (maxVal - minVal)`) on filter resonance parameters (`resonance`, `filter q`, `res`, `q`) during randomization to protect ears and monitor speakers (`b453891a2`).
- **Dynamic MIDI Step Boundary Auto-Adaptation:** MidiClips automatically adapt their step count and bounding length to the actual extent of note data (`note->pos() + ticksPerStep`), shrinking back to `m_userSteps` on note deletion without clipping trailing steps (`cc3f4dfa7`).
- **Smart Musical Parameter Randomizer:** Independent and global randomization that protects pitch/detuning harmony, maintains mixer routing (`fxChannel`), preserves track MIDI group bonds, and strictly avoids muting tracks (`c14262c93`, `bedab042c`).
- **Configurable Random Synths Generator:** Dedicated "Random Synths..." dialog in Song Editor and Pattern Editor with configurable track count (1-32) and insert effect counts (0-10) per track (`d96ae621f`).
- **Persistent Dialog Memory:** Synced user count preferences with `ConfigManager` so randomizer configurations persist across application restarts (`e0da70cd0`).
- **Max Polyphony & Voice Stealing Controls:** Added `POLY` LCD spinbox on Instrument Track window with real-time dynamic voice stealing in `NotePlayHandle` (`ae9b196c6`).
- **ZynAddSubFX Randomization & Full Voice Stealing:** Fixed CPU release freeze and integrated deep synthesis parameter randomization for ZynAddSubFX (`abedcb577`).

### Fixed
- **Vibed Synth Identifer:** Corrected plugin identifier from `"vibed"` to `"vibedstrings"`, fixing empty synth track generation (`c24150620`).
- **Runtime Dependency Packaging:** Automatically copy vcpkg runtime DLLs (`zlib1.dll`, `fluidsynth.dll`) into deploy directories to ensure ZynAddSubFX and SoundFont player load reliably (`6b927abec`).
- **Effect Rack Memory Leak & Crash Prevention:** Cleaned effect racks using `fxChain()->clear()` instead of GUI-only `clearViews()` (`cc3f4dfa7`).
- **Build Concurrency CPU Guard:** Capped MSVC Ninja build parallelism to `[Math]::Max(1, $env:NUMBER_OF_PROCESSORS - 4)` to preserve OS smoothness (`ae34bb4da`).

---

## [v5.0.0] - 2026-11-15

### Added
- **Fine-Grained Zoom & Smooth Scrolling:** Extended Song Editor zoom range to 1–1000 px/bar (500 steps) and Piano Roll zoom to 24 steps (3.125% to 1600%) without jumping scroll coordinates (`aabf8149b`).
- **Fractional & Decimal Step Sequencing:** Step Sequencer support for 1, 2, 3, and decimal (e.g. 2.5) step counts in 4/4 time by eliminating artificial 1-bar constraints (`aabf8149b`).
- **Dedicated Piano Roll Step Toolbar:** Placed step count controls on a dedicated toolbar so they never get hidden in overflow menus (`aabf8149b`).
- **Automation Shifting:** Added Shift Left / Shift Right (1 step) and custom step shifts in automation clip context menus (`aabf8149b`).
- **FX Group Save & Load:** Interchangeable `.lmms-fxgroup` / `.xml` export and import between mixer channels and instrument chains (`aabf8149b`).
- **Random Instrumentation Prototype:** Quick 3–5 track random synth generator with DSP insert effects (`aabf8149b`).
- **Packaging Inclusions:** Bundled Commodore 64 SID plugin and STK rawwaves into distribution (`aabf8149b`).

---

## [v4.0.0] - 2026-11-08

### Added
- **Polymetric Track Lengths in Pattern Editor:** Individual track length customization per pattern calculated using Least Common Multiple (LCM) (`4a17e897b`).
- **Unused Automation Track Cleanup:** One-click deletion of unassigned/idle automation tracks from Song and Pattern Editors (`4a17e897b`).
- **Continuous Curve Recording:** Automatic linear curve generation for live automation recording (`4a17e897b`).
- **Pattern Audio Export:** Direct pattern-level audio export slot from MainWindow (`3f591a4e3`).

---

## [v3.0.0] - 2026-10-25
- Pattern loop range refinement and playback boundary synchronization.
- Step sequencer timing enhancements.

---

## [v2.0.0] - 2026-10-10
- Enhanced track container management and pattern lifecycle handling.
- Multi-track routing improvements.

---

## [v1.0.0] - 2026-09-20
- Ported toolchain to Qt 6.8.0 and Microsoft Visual C++ 2022 (MSVC x64).
- Cleaned legacy Qt 5 deprecated API references.
