---
description: Critical architectural guidelines, invariant patterns, and build conventions for LMMS Orion development.
globs: **/*
---

# LMMS Orion Development Rules & Invariants

## 1. MIDI Step Count Calculation
- When calculating fractional or decimal step boundaries and note ends in `MidiClip`, always compute note end ticks using `note->pos() + ticksPerStep` (or equivalent step tick duration), rather than legacy `note->pos() + 1`. Using `pos() + 1` truncates step counts and cuts off the last step in playback and rendering.
- Maintain a baseline `m_userSteps`: dynamic step count adaptation must expand when notes extend beyond the baseline (`std::max(m_userSteps, neededSteps)`) and contract back down to `m_userSteps` when notes are deleted.

## 2. Polyphonic Synthesizer Audio Rendering
- Instruments equipped with their own internal envelope generators (ADSR) must NOT call `applyRelease(workingBuffer, n)` in `playNote()`. Calling `applyRelease()` applies an external linear ramp to the shared working buffer, which crushes concurrent polyphonic voices and cuts off delay/reverb decay tails.
- Large voice buffers (such as delay or reverb lines) must be lazily allocated or preallocated to avoid heap allocation jitter on the real-time audio thread.

## 3. Effect Chain & Rack Lifecycle Management
- When clearing, replacing, or randomizing effects in `EffectRackView`, always call `fxChain()->clear()`. Calling GUI-only methods like `clearViews()` leaves underlying DSP `Effect` instances running in memory, causing memory leaks and audio duplication upon view refresh.

## 4. Build, Git & Toolchain Guidelines
- Always follow `C:\lmms\lmms-repo\LMMS_ORION_MASTER_GUIDE.md`.
- Git commands must always pass `-c credential.helper=` to prevent credential popup hangs on Windows.
- Local compilation with Ninja/MSVC is prioritized over remote runners for rapid turnaround.

## 5. Build Concurrency & CPU Core Reservation
- **MANDATORY:** Always leave 3-4 CPU cores/threads free when initiating compile/build or ninja tasks (e.g. `ninja -j 8` or `cmake --build build --parallel ([Math]::Max(1, $env:NUMBER_OF_PROCESSORS - 4))`). Never saturate 100% of all CPU threads so that the user's system remains smoothly responsive during builds.

## 6. Randomization & Routing Invariants
- When randomizing instruments or tracks (`randomizeParameters`, random synths generation, reordering):
  - NEVER modify track MIDI group numbers (`m_midiGroupModel`).
  - NEVER modify mixer channel assignments or routing bonds (`m_mixerChannelModel`).
  - NEVER mute instruments (`m_mutedModel`). Instruments must remain audible and properly routed to their assigned mixer channels and MIDI groups.
