# LMMS Orion - Changelog & Modification Ledger

All notable changes, architectural improvements, bug fixes, and safety rules for the **LMMS Orion** digital audio workstation are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and adheres to semantic versioning milestones.

---

## [v10.0.0] - Unreleased

> Continuation of the v9 development line. Focus: comprehensive undo coverage, gesture-based automation recording, global swing, and new piano-roll note transforms.

### Added
- **ModularSynth instrument (new plugin):** A native sound-design instrument with **unlimited oscillators** (up to 64, add/remove in the resizable GUI). Each oscillator has: on/off, waveform (sine/saw/square/triangle/noise — saw & square are polyBLEP band-limited), **frequency in Hz with no practical upper limit** (±1,000,000 Hz, double-precision phase, exact values typable via double-click), amplitude (0–4, headroom allowed), stereo pan, and **FM routing** — any oscillator can frequency-modulate any other by a precise Hz depth (one-sample-delayed modulator output keeps feedback-free cycles stable). Master section: volume, pan, and a full **ADSR envelope** (attack/decay/release in ms, sustain level — all live/automatable). Every parameter is a journallable model, so knobs/spinboxes work with undo, controller links, and the new gesture-based automation recording. Oscillator count, all parameters, and FM routing persist in project files. DSP caches all oscillator parameters once per audio period (no per-sample model lookups) and skips the render loop entirely for silent patches — cheap even with 64 oscillators.
- **Global Swing:** New "SWING" spinbox in the main toolbar (next to TEMPO/TIME SIG), backed by `Song::m_swingModel` (0–100%, persisted as the `swing` attribute in project files, reset with the song). During playback, notes falling on the off-16ths of each beat are delayed by up to half a 16th step — ~67% approximates a triplet/shuffle feel. Applies to both Song and Pattern playback in `InstrumentTrack::processNextBuffer()`.
- **Piano-Roll Note Transforms:** The note-tools toolbar gains three new actions operating on the current selection (or all notes when nothing is selected): **Duplicate Notes** (clones the selection tiled seamlessly after itself), **Double Note Lengths (x2)**, and **Halve Note Lengths (/2)** — implemented as `MidiClip::duplicateNotes()` / `MidiClip::scaleNoteLengths()`.
- **Gesture-Based Automation Recording:** `PatternStore::handleLiveAutomation()` now only records while the automatable model reports `isBeingManipulated()` — i.e. strictly during an active knob/slider drag. Programmatic value changes, armed-but-idle states, and post-release changes no longer write automation nodes or spawn empty automation clips. Step count still grows dynamically to fit recorded nodes.

### Changed
- **Undo/Redo — batch randomizations are now single-step:** `AutomatableModel::setValue()` pushes a journal checkpoint per parameter, so mass randomizations previously flooded the undo stack with hundreds of per-parameter entries (effectively breaking undo). `InstrumentTrack::randomizeParameters()` and `EffectRackView::randomizeEffects()` now suppress per-model journalling during the batch: the instrument randomize takes one track checkpoint (plus a separate `EffectChain` checkpoint, since its FX params live on the mixer channel outside the track state), and the rack randomize keeps its single chain checkpoint — each randomization is now exactly one undo step. The instrument randomize's nested FX-chain parameter randomization is covered by the extra chain checkpoint.
- **Theme "Randomize all colors" performance:** each `ThemeManager::setThemeColor()` call rebuilt the entire application stylesheet (`qApp->setStyleSheet`) and re-polished every widget — 15 full restyles per click. New `ThemeManager::setThemeColors()` batch API assigns all fields first and applies the theme exactly once; the dialog's "Randomize all colors" button uses it. Shared `assignThemeField`/`assignAccentFields` helpers keep single-color behavior (including accent derivation) identical.
- **Undo/Redo — Effect Rack coverage:** `EffectChain` is now a `JournallingObject` (its `saveSettings`/`loadSettings` already round-trip the full chain), so the whole FX rack state is journaled. `EffectRackView` calls `addJournalCheckPoint()` before every structural mutation: add/paste/load effect group, delete plugin, move up/down, randomize rack, clear all effects, and programmatic inserts (`addEffectByName`, e.g. mixer EQ quick-insert). FX deletions, rack randomization, and reordering are now undoable.
- **Undo/Redo — Clip transforms:** `MidiClip::duplicateContent()` ("Double content") and `MidiClip::scaleContent()` ("Halve content", percentage scaling) now take a journal checkpoint, making clip duplication and percentage transforms undoable.
- **Vestige knob automation fix:** `ManageVestigeInstrumentView`'s destructor no longer deletes the instrument-owned `knobFModel` array/elements (nor the Qt-child knobs it did not own). Previously, closing the plugin control window destroyed the automation backing models — breaking knob automation and risking a double-free when the instrument was destroyed. Vestige's per-parameter `FloatModel`s are `JournallingObject`s, so all synth knobs are now reliably automatable (drag = gesture recording, links persist in project files).

### Removed
- **ZynAddSubFX presets:** Deleted `data/presets/ZynAddSubFX` (962 `.xiz`/`.xpf` preset files) from the repository. The plugin binary was already excluded from portable packages (`zynaddsubfx.dll`, `RemoteZynAddSubFx.exe`) due to randomizer crashes; its preset library is now gone as well.
- **Vestige + Carla excluded from packages:** `vestige.dll`, `carlabase.dll`, `carlarack.dll`, `carlapatchbay.dll`, and `libcarla_native-plugin.dll` are removed from portable builds — Vestige is a VST2-only bridge and Carla proved unreliable on this target; the new native ModularSynth covers the sound-design use case without external plugin hosting.
- **Carla exclusion lifted (superseded):** `carlabase.dll`, `carlarack.dll`, `carlapatchbay.dll` were briefly re-included for VST3 hosting, then removed again in favor of ModularSynth.

---

## [v9.0.0] - Unreleased

> Development line branched from **v8-original** (`87949a18e`). Implements guide section 9: configurable pattern-length cap, multi-track selection/groups, and the workspace minimap port.

### Added
- **Configurable Pattern Length Cap (guide 9.2):** The hard-coded 32-bar LCM clamp in `PatternStore::lengthOfPattern()` is now driven by the shared `patterns/lengthcap` config key (bars, fractional values allowed). Non-positive values mean "unlimited", internally bounded by a hard safety cap (~5.2e5 bars) so pathological clip-length combinations cannot exhaust memory. The LCM accumulator was widened to 64-bit so huge/unlimited combinations cannot overflow `tick_t` before the cap check. New "Pattern length limit..." toolbar action in the Pattern Editor opens a dialog with an Unlimited checkbox and a bars spinbox; applies to both Pattern and Song Editor, and forces clip-view repaints on change.
- **64-bit-safe pattern strip rendering:** `PatternClipView::paintEvent()` now computes `pixelsPerPattern` in `qint64` and draws notes directly per visible repetition when the pattern strip would exceed 65536 px, preventing allocation failures and integer overflow for very long patterns.
- **Multi-Track Selection (guide 9.1):** Click a track head to select it, drag vertically across track heads for rubber-band range selection, Ctrl+click toggles tracks, Shift+click range-selects from the anchor (a drag still performs the legacy track-height resize). Works in both Song and Pattern Editors via the shared `TrackView`/`TrackContainerView` code. Selection is shown with a translucent accent overlay and a colored left bar; Escape clears it.
- **Batch Track Operations:** The track operations (grip) menu gains a "N tracks selected" submenu when multiple tracks are selected: clone/remove/clear selected tracks, move up/down, mute/unmute/solo, and set a shared track color.
- **Track Groups (guide 9.1):** Selected tracks can be grouped (with optional name), added to an existing group, ungrouped, renamed, and collapsed/expanded. Group membership (`trackgroup`/`trackgroupname` attributes) persists in project files and presets. Grouped tracks show a stable per-group tint; collapsing keeps the first member visible as the group header.
- **Per-Project Pattern Length Cap:** The pattern length limit is now stored inside the project file (`patternlengthcap` attribute in the patternstore state), so each project remembers the cap it was saved with; projects without the attribute fall back to the global config value.
- **Theme Color Randomizer:** The Theme & Appearance dialog now has a small "R" button next to every color target (the four main pickers and all extended keyed colors) that assigns a random color to just that target, plus a "Randomize all colors" button that randomizes every theme color at once.
- **Workspace Radar & Minimap + Workspace Zoom (guide 9.3):** Ported from the v8 `master` line (commit `1c5935b0e`). New "Workspace Radar & Minimap" window (Ctrl+9 / toolbar button) renders a live minimap of all MDI subwindows with click-drag panning and theme-aware colors. The workspace MDI area additionally supports Ctrl+wheel zoom (40%–250%, zoom-to-cursor) and double-click reset to 100%. The minimap window itself is pinned — it no longer moves with workspace panning, zooming, or radar navigation.

### Build & Packaging
- **`build-local.ps1`:** Added `-DWANT_SWH=OFF` to the CMake configure flags (the swh LADSPA pack cannot build on this machine — its Perl `List::MoreUtils`/`XML::Parser` modules are unavailable), matching the existing local build configuration.

---

## [v8.0.0-original] - 2026-11-27

> Continuation line branched from **v7.0.0** (`02626bedb`) plus the safe GUI-only theme commits (`8f248f074`, `4f3c18286`). The unstable v8 engine experiments (minimap, polyphony rework, arp DSP, cent detune) are intentionally excluded.

### Added
- **Per-Channel Mixer Panning (`MixerChannel::m_panModel`):** Every mixer channel (including Master) now has an automatable pan control (-100..+100, center-detented). Implemented as a post-fader gain stage in `MixerChannel::doProcessing()` with sample-exact `ValueBuffer` support for smooth automation, full project save/load (`<pan>` attribute), channel duplication support, and a dedicated `Pan` knob rendered next to the channel fader in `MixerChannelView`.
- **One-Click Parametric EQ on Mixer Channels:** New `EQ` button on every mixer channel strip and a `Parametric EQ...` context-menu entry. Inserts (or re-opens) the native 8-band parametric `eq` effect directly into that channel's FX chain via the new `EffectRackView::addEffectByName()` / `editEffectControls()` / `findEffectByPluginName()` helpers. All EQ parameters are `FloatModel`-based and fully automatable through the standard automation-track drag & drop workflow.
- **Extended Precise Theme Color Customization:** The Theme & Appearance dialog now exposes 11 additional precise color targets in a scrollable section — Accent Hover, Text on Accent, General Text, Grid Lines, Beat Lines, Bar Lines, Note, Selection, Input Fields, Sliders & Scrollbars, and Headers & Group Boxes — on top of the existing Accent/Base/Border/Window pickers.
- **New Theme Palette Fields (`fieldColor`, `sliderColor`, `headerColor`):** `ThemePreset` gained three new colors driving generated QSS for input fields (line edits, spin boxes, combo boxes with accent focus borders), sliders/scrollbars (themed troughs + accent handles), and headers (group boxes, dock titles, tooltips). All five factory presets ship tuned values; missing values in older `.lmms-theme` presets and `<projecttheme>` XML blocks are auto-derived from the base color for backward compatibility.
- **Global Text & Widget QSS Coverage:** The dynamic theme stylesheet now recolors general foreground text (`QLabel`, `QCheckBox`, `QGroupBox`, item views), input fields, scrollbars, sliders, group boxes, dock titles and tooltips — so custom themes visibly propagate across the whole interface instead of only a handful of widgets.

### Build & Packaging
- **`deploy-v8.ps1`:** New portable deployment script producing `C:\Users\ahsoka\Downloads\LMMS-Orion-v8-original` plus a compressed `LMMS-Orion-v8-original.zip` archive.
- **`WANT_SWH=OFF` build configuration:** The optional swh LADSPA pack stays disabled (it was never built on this machine — its Perl `List::MoreUtils`/`XML::Parser` modules are unavailable); this matches all previous local builds.
- **`build-only.ps1`:** Lightweight incremental ninja wrapper reserving 3-4 CPU cores (`ProcessorCount - 4`).

---

## [v7.0.0] - 2026-11-23

### Added
- **Multi-Platform Automated Cloud Release Pipeline (`.github/workflows/release.yml`):** Added a GitHub Actions release automation workflow capable of building and publishing complete release assets for Windows (x64 Portable ZIP & NSIS Installer), Linux (Ubuntu 24.04 AppImage), and macOS (Apple Silicon / Universal DMG) in the cloud with zero host CPU load. Supports one-click manual triggers (`workflow_dispatch`) and automated publishing upon tag pushes (`v*`), attaching all platform binaries directly to GitHub Releases with auto-generated release notes.
- **Dynamic Theme, Appearance & Color Palette Customization (`ThemeManager` & `ThemeDialog`):** Introduced a zero-overhead, 100% native Qt styling and theme management engine.
  - **Dedicated Theme Dialog:** Accessible via the main toolbar (colorize palette icon, Ctrl+8) and `Edit -> Theme & Appearance...`. Allows live color customization of Accent (LEDs, active buttons, knobs, selections), Base Panel (toolbars, docks, sidebar, menus), Window Borders & Frames, and Workspace background.
  - **Comprehensive Subwindow & Frame Theming (`lmms--gui--SubWindow`):** Eliminated hardcoded legacy gray subwindow styling. Window borders (1px outline), active titlebar gradients (accent-tinted), inactive titlebars (harmonious base palette), and window labels dynamically match the chosen theme or user's custom border color.
  - **Global Interface Styling:** Dynamically recolors `SideBar`, `QMenuBar`, `QMenu`, `QStatusBar`, `QDockWidget`, `TrackLabelButton`, and `MixerChannelView` according to base and accent theme parameters.
  - **5 Factory Presets:** Built-in curated palettes (*Orion Obsidian Gold*, *Cyber Neon Cyan*, *Studio Dark Lime*, *Solarized Amber*, and *Classic LMMS*) with instant live switching without restart, plus user preset saving and loading (`.lmms-theme` JSON files).
  - **Workspace Wallpaper Management:** Supports custom background images (`.png`, `.jpg`, `.svg`) with automatic brush painting on the QMdiArea canvas, browse and clear actions.
  - **Project-Specific Theme Persistence (`<projecttheme>` XML):** Projects can save their chosen colors, preset name, border color, and wallpaper directly into `.mmp` / `.mmpz` files, restoring the project's unique visual identity on load.
  - **Global Override Protection:** Added *"Apply project-specific themes on load"* toggle in both `Settings (Display)` and the `Theme & Appearance` dialog. When unchecked, user's global theme remains strictly enforced across all opened projects. Reset-to-default ensures new projects (`Ctrl+N`) clean back to global user preferences.
- **CI / Submodule Checkout & Build Fixes:**
  - **Upstream Submodule Reference Alignment (`plugins/ZynAddSubFx/zynaddsubfx`):** Restored submodule git pointer to upstream commit `66a42ef`, preventing GitHub Actions runner checkout failures (`fatal: upload-pack: not our ref`).
  - **Automated CMake DSP Patching:** Integrated automatic configure-time patching for `SVFilter.cpp` directly in `plugins/ZynAddSubFx/CMakeLists.txt` to guarantee null pointer crash prevention across all platforms without dirtying upstream submodule commits.
  - **Fixed GitHub Actions Checkout Actions:** Restored `actions/checkout@v7` typo back to valid `actions/checkout@v4` in `build.yml`.
- **Upstream Stability & UI Acceleration Integrations:**
  - **Vestige, Carla & VstEffect Subwindow Double-Deletion Protection (`5ba6744e5`):** Removed redundant `setAttribute(Qt::WA_DeleteOnClose)` and `close()` calls immediately before explicit deletion of `m_subWindow` in `Vestige.cpp`, `Carla.cpp`, and `VstEffectControls.cpp`, eliminating heap double-free crashes when closing VST, Vestige, and Carla plugin windows.
  - **AudioFileProcessor Empty Sample Buffer Protection (`16dcfe966`):** Hardened `AudioFileProcessorWaveView::updateGraph()` with empty and null buffer guards (`!m_sample || !m_sample->buffer() || m_sample->sampleSize() <= 0`), preventing buffer overrun and null pointer crashes when opening or viewing empty audio files.
  - **Song Editor Beat Preview Batch Drawing Acceleration (`39c5654ab`):** Optimized `PatternClipView::paintEvent()` by drawing pattern notes into a transparent batching pixmap (`midiPatternPixmap`) once and rendering repeated pattern clips with `p.drawTiledPixmap()`. Eliminates heavy per-note fill loop overhead and substantially smooths timeline scrolling across large multi-track projects.
- **ZynAddSubFX SVFilter Null Pointer Dereference Fix (0xc0000005 Crash at 0x16c79):** Hardened `SVFilter` constructor, `settype()`, and `singlefilterout()` in `plugins/ZynAddSubFx/zynaddsubfx/src/DSP/SVFilter.cpp` against out-of-range filter types (`type > 3`). Clamped filter type strictly to `[0, 3]` and added a safe fallback (`out = &x.low`) in the `switch (type)` default branch. Prevents fatal access violations when loading analog/special filter presets or when randomizing musical parameters.
- **SlicerT GUI Signal-Slot Type Mismatch & 0x0 Crash Fix:** Fixed invalid member function pointer casts in `SlicerTView.cpp` where `QPushButton` instances (`m_midiExportButton`, `m_resetButton`) were bound to `&PixmapButton::clicked`. Connected to `&QPushButton::clicked`, eliminating the invalid vtable jump to `0x0000000000000000`.
- **SlicerT Audio-to-GUI Thread Decoupling & Paint Safety:** Bound `SlicerT::isPlaying` to `SlicerTWaveform::isPlaying` using `Qt::QueuedConnection` to ensure waveform redraws and playhead updates execute exclusively on the main GUI thread event loop instead of running synchronously within the real-time audio engine rendering thread.
- **SlicerT & FileBrowser Null Pointer Lifetime Protection:** Added defensive pointer checks in `SlicerT::playNote()`, `deleteNotePluginData()` (`handle->m_pluginData = nullptr`), and `FileBrowserTreeWidget::openInSlicerT()`, preventing crashes when dragging or loading samples into uninitialized tracks.
- **Automation Editor Selection Tool & Precision Multi-Node Editing:** Added dedicated `Select` tool (`edit_select` icon, Shift+S), interactive rubber-band rectangle selection on canvas, golden glowing selection outlines around active nodes, and simultaneous multi-node mouse dragging in time (ticks) and level (clamped to parameter bounds). Holding Ctrl enables smooth unquantized movement, while releasing snaps to the quantization grid. Micro-stepping keyboard navigation: Shift/Ctrl + Arrow keys allow ultra-fine 1-tick temporal adjustment and 1% level adjustment.
- **Automation Editor Step/Length Controls & Content Multipliers:** Added `Length / Step controls` toolbar to Automation Editor featuring a high-precision `Steps:` numeric spinbox, `-1 Bar` (remove 16 steps), `+1 Bar` (add 16 steps), `x2` (Double Content - duplicate points offset by clip length), and `/2` (Halve Content).
- **Piano Roll Step Controls, Content Duplication & Resize Actions:** Added `Remove steps (-1 Bar)`, `Add steps (+1 Bar)`, `Double content (x2)`, and `Halve content (/2)` action buttons directly to the Piano Roll toolbar next to the step spinbox. Users can now double, halve, expand, or shrink pattern lengths and note content directly from within Piano Roll without having to navigate back to the Pattern Editor.
- **Thread Affinity & Cross-Thread GUI Creation Fix (ZynAddSubFX Crash Fix):** Resolved fatal `0xc0000005` access violation in `Qt6Core.dll` when loading or drag-and-dropping ZynAddSubFX or other instruments. Bound `InstrumentLoaderThread::finished` signals to receiver context (`this`) in `TrackContainerView::dropEvent` and `PluginBrowser::openInTrackContainer`, ensuring window and widget instantiation (`InstrumentTrackWindow`) executes strictly on the main GUI thread rather than executing as a direct connection on secondary worker threads. Added automatic thread reclamation via `deleteLater` to prevent thread leaks.
- **Null Pointer Model Guards in InstrumentTrackWindow & ZynAddSubFxView:** Guarded model binding across `m_muteBtn`, `m_soloBtn`, `m_noteStackingView`, `m_arpeggioView`, `m_effectView`, `m_tuningView`, and `ZynAddSubFxView::modelChanged()`, preventing offset pointer dereferences on uninitialized tracks.
- **Native Windows SEH Hardware Crash Interceptor:** Integrated a top-level Win32 structured exception filter (`SetUnhandledExceptionFilter`) with a native modal message dialog (`MessageBoxW`) in `src/core/main.cpp`. Any unexpected CPU-level exception (e.g. faulty 3rd-party VSTs or driver faults) now displays a clear diagnostic alert with the faulting instruction address and hex exception code instead of silently terminating the application.
- **Fail-Safe Pointer Protection in NotePlayHandle (0xc0000005 Access Violation Fix):** Added comprehensive `nullptr` guards for `instrumentTrack()` across `NotePlayHandle::framesLeft()`, `setVolume()`, `midiKey()`, `play()`, `noteOff()`, `actualReleaseFramesToDo()`, and destructor. Prevents null-pointer dereferences when orphaned, preview, or released notes are polled by the audio engine play handle cycle.
- **Hardware Zero-Division Protection in Arpeggiator & Chord Stacking (0xc0000094 Fix):** Hardened `InstrumentFunctionArpeggio::processNote()` and `InstrumentFunctionNoteStacking::processNote()` against zero-divisor faults. Enforced minimum bounds (`repeats = std::max(1, ...)`, `range = std::max(1, ...)`, `arp_frames > 0`, `cycleRange = std::max(1, range / repeats)`), clamped down-direction index calculation (`std::max(1, range - 1)`), and added chord index validation with empty play handle fallbacks.
- **Multi-Node Automation Selection & Sideways Dragging:** Added rubberband box selection (Shift+drag on canvas or dedicated `Select` tool via Shift+S), Shift-click toggle selection, `Ctrl+A` select-all, `Delete`/`Backspace` node removal, and multi-node simultaneous dragging horizontally in time (with quantization snap) and vertically in value.
- **Infinite Drag Screen-Wrapping for BPM & Time Signature:** Added monitor edge cursor wrapping (`QGuiApplication::screenAt` + `QCursor::setPos` in `LcdFloatSpinBox` and `LcdSpinBox`), allowing users to smoothly adjust BPM or Time Signature continuously upwards or downwards without getting stuck at the top or bottom monitor edges.
- **Automation Decimal Range Clamping & High-Resolution Ruler Fix:** Replaced `roundf` level clamping with `std::clamp` in `AutomationEditor::getLevel` and `updateTopBottomLevels`, resolving the bug where parameters with non-integer upper limits (such as LB302 VCF Resonance = 1.25 and Cutoff = 1.5) were artificially capped at 1.0. Added fractional subdivisions to the Y ruler so values like 0.0, 0.31, 0.63, 0.94, 1.25 are clearly visible and drawn accurately.
- **Real-Time Playhead Tracking in Automation Editor:** Integrated `PositionLine` tracking synchronized with Song, Pattern, and AutomationClip playback modes, accompanied by a dynamic glowing playhead dot moving live along the automation curve.
- **Controller Rack Preset Management ("LFO Presets"):** Added Save Preset, Load Preset, and Presets quick menu to Controller Rack View. Presets are saved and loaded from `<UserDir>/presets/LFO Presets` with support for appending or replacing controllers and direct folder opening.
- **60 FPS GUI Modulation Throttling & Audio Thread Decoupling:** Decoupled the real-time audio thread from flooding Qt's main event queue. `Controller::triggerFrameCounter()` now marks dirty state via an atomic flag (`s_guiUpdateNeeded`), while `MainWindow::timerEvent` dispatches `Controller::updateGuiControllers()` at a steady 60 FPS directly within the main GUI thread. Eliminates thousands of cross-thread `QMetaCallEvent` allocations per second and eradicates interface lag and mouse stutter during active LFO, automation, and controller modulations.
- **Thread-Safe Period Tracking & Pointer Lifecycle Safety:** Converted `Controller::s_periods` to `std::atomic<long>` to eradicate audio-to-GUI data races. Hardened `Controller::updateGuiControllers()` with vector snapshot iteration, fail-safe pointer checks, and container membership validation to prevent iterator invalidation or use-after-free when controllers are dynamically created or destroyed during signal dispatch.
- **Knob Drawing Buffer Reuse & Bounds-Safe Resizing:** Optimized `Knob::drawKnob()` to reuse existing `QImage` backing buffer (`m_cache`) across frames rather than allocating a heap-backed `QImage` on every single repaint. Enforced `m_cache.size() == size()` cache validation to ensure proper crisp redraws on widget resize and high-DPI scaling, with early empty-size guards.
- **Hidden Widget Update Optimization:** Enhanced `FloatModelEditorBase::friendlyUpdate()` to skip string formatting, floating text updates, and update requests for invisible/closed widgets (`!isVisible()`), drastically reducing GUI overhead on large projects with dozens of background tracks.
- **Dynamic Controller Linking & Stopped-State Responsiveness:** Added `Controller::requestGuiUpdate()` hooks on `ControllerConnection::setController()` and `LfoController` parameter updates, ensuring knobs immediately update their visual state upon connection or parameter adjustment even when playback is stopped.
- **Smooth 60 FPS Knob Motion:** Cleaned obsolete modulo-1024 audio frame skips in `FloatModelEditorBase::friendlyUpdate()`, ensuring silky-smooth 60 FPS knob animation synchronized with display refresh rates without skipping or jumping.
- **Dedicated Isolated Deployment Pipeline (`deploy-v7.ps1`):** Automated deployment into versioned destination folder (`LMMS-Orion-v7`) to prevent Windows OS file lock conflicts (`EBUSY` / `UnauthorizedAccessException`) with existing running instances. Automatically prunes intermediate compiler artifacts (`.pdb`, `.obj`, `.ilk`, `CMakeFiles`), reducing overall package footprint from 1.2 GB down to ~150 MB.

### Planned & In-Progress
- **AVX2 SIMD Mixing & Dynamic Polyphony:** High-performance real-time mixing pipeline with AVX2 SIMD vectorization, runtime CPUID detection, and SSE fallback.
- **Voice Stealing Engine:** Intelligent audible voice-stealing preventing release freezes and CPU overload on high-polyphony tracks.

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
