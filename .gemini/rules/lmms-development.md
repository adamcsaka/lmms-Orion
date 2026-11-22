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

## 7. Multi-Session Awareness & Prior State Inspection
- **Git Repositories:** Before beginning any bug analysis, code changes, or computations, **ALWAYS** check recent git commits (`git log -n 5`) and `git status`. In multi-window/collaborative environments, another session or process may have already resolved or committed the fix.
- **Non-Git Projects:** Always check the local machine state: verify existing output files, build artifacts, recent edits, and active background processes before starting work.

## 8. "Ask First, Then Create" & Proactive Recommendations for Boost / Teamwork
- **NEVER** silently or automatically spawn heavy subagents (such as DeepCoder) or trigger long autonomous boost workflows.
- Always notify the user beforehand: ask clarifying questions, explain what needs to be done, and obtain the user's explicit alignment before initiating subagents.
- **Proactive Suggestions:** Whenever a task is complex, multi-modular, architectural, or would significantly benefit from `/boost` or `/teamwork`, PROACTIVELY recommend it to the user immediately and ask if they wish to activate it.
- Favor direct, transparent execution in the main conversation for fast turnaround.
