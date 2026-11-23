<div align="center">
	<h1>
	<img src="https://raw.githubusercontent.com/LMMS/artwork/master/Icon%20%26%20Mimetypes/lmms-64x64.svg" alt="LMMS Logo"><br>LMMS Orion Edition
	</h1>
	<p><strong>A Modernized, High-Precision, Producer-Focused Digital Audio Workstation</strong></p>
	<p>
		<a href="https://github.com/adamcsaka/lmms-Orion">LMMS Orion Repository</a>
		⦁︎
		<a href="https://lmms.io/">Upstream LMMS Website</a>
		⦁︎
		<a href="https://lmms.io/documentation">User Manual</a>
	</p>
</div>

---

## What is LMMS Orion?

**LMMS Orion** is an enhanced, production-ready fork of LMMS (Linux MultiMedia Studio) designed to overcome legacy workflow limitations, modernize the core architecture for 64-bit multi-core Windows environments, and provide professional sequencing, automation, and sound design tools.

---

## Key Differences & Enhancements from Upstream LMMS (v1 – v7)

Below is the complete summary of all architectural and feature enhancements introduced across the Orion development milestones:

### 🎨 Milestone v7: Theme Customization, Engine Stability & Cloud Releases
* **Dynamic Theme & Appearance Engine (`ThemeManager` & `ThemeDialog`):**
  * Instant, zero-overhead Qt palette and styling system with real-time live preview.
  * **5 Factory Presets:** *Orion Obsidian Gold*, *Cyber Neon Cyan*, *Studio Dark Lime*, *Solarized Amber*, and *Classic LMMS*.
  * **Live Customization:** Configurable Accent color (knobs, LED meters, active buttons, selection outlines), Base panel color (toolbars, sidebars), and Workspace background.
  * **Custom Wallpaper Support:** Load custom artwork/wallpapers (`.png`, `.jpg`, `.svg`) drawn smoothly across the main MDI workspace canvas.
  * **Project-Specific Theme Persistence (`<projecttheme>` XML):** Projects can save their unique colors and wallpaper directly into `.mmp` / `.mmpz` files, restoring the track's visual theme on load.
  * **Global Override Protection:** *"Apply project-specific themes on load"* toggle in both Settings and Theme Dialog allows users to permanently enforce their own favorite theme.
* **Upstream Engine Stability & Hardware Crash Prevention:**
  * **ZynAddSubFX SVFilter Null Pointer Fix (0xc0000005):** Resolved access violation in `SVFilter::singlefilterout()` during fast preset switching and parameter randomization.
  * **SlicerT Thread-Safety & Decoupling:** Decoupled waveform rendering and Qt signal-slot bindings from the real-time audio callback (`SlicerT::play()`) using queued connections to eradicate audio glitches and UI freezes.
  * **Vestige, Carla & VstEffect Double-Deletion Fix:** Eliminated heap double-free crashes when closing plugin windows or deleting tracks.
  * **AudioFileProcessor Empty Buffer Guard:** Prevented host crashes when loading empty or zero-byte audio files.
  * **PatternClipView Batch Pixmap Rendering:** Accelerated Song Editor timeline scrolling in large arrangements via tiled pixmap caching.
  * **Native Windows Hardware SEH Interceptor:** Integrated top-level Win32 structured exception handling with diagnostic modal alerts showing instruction addresses and fault codes instead of silent app crashes.
* **Precision Sequencing & Modulation Controls:**
  * **Automation Editor Multi-Node Selection & Editing:** Rubber-band box selection, multi-node dragging in time and value, unquantized Ctrl-drag, keyboard micro-stepping (Shift/Ctrl + Arrows), and step multipliers (`-1 Bar`, `+1 Bar`, `x2`, `/2`).
  * **Piano Roll Quick Length Controls:** Dedicated buttons on the toolbar to add/remove bars and double/halve pattern content directly from Piano Roll.
  * **Controller Rack Presets ("LFO Presets"):** Save and load complex multi-controller LFO chains with one click.
  * **60 FPS GUI Modulation Throttling:** Decoupled audio DSP calculations from Qt GUI paint queues, eliminating mouse stutter during high-speed LFO modulations.
* **Multi-Platform Automated Cloud Releases (GitHub Actions):**
  * Automated cloud pipeline producing official release binaries for **Windows x64** (portable ZIP & setup EXE), **Linux x86_64** (.AppImage), and **macOS** (.dmg) with zero local CPU load.

### 🚀 Milestone v1: Modern Architecture & Toolchain Modernization
* **Qt 6.8 x64 & MSVC 2022 Integration:** Fully migrated from legacy Qt 5 / MinGW toolchains to native Microsoft Visual C++ 2022 and modern Qt 6.8.
* **vcpkg Dependency Management:** Replaced fragmented external libraries with 40+ unified, cached vcpkg packages for high-fidelity audio processing.
* **High-DPI & Multi-Core Optimization:** Enhanced UI responsiveness on high-resolution displays and optimized multi-threaded CPU scheduling.

### 🎛️ Milestone v2: Enhanced Track & Pattern Management
* **Seamless Container Synchronization:** Re-architected interaction between Song Editor and Pattern Editor (Beat/Bassline) tracks.
* **Robust Track Container Operations:** Streamlined track duplicating, channel routing, and safe lifecycle handling for instruments and automation.

### 🔁 Milestone v3: Pattern Sequencing & Looping Refinements
* **Zero-Jitter Bar Alignment:** Fixed playback loop boundaries to eliminate sample dropouts and micro-stutters during continuous looping.
* **Step Sequencer Playback Integrity:** Ensured exact tick synchronization between looping MIDI clips and host transport.

### 🎼 Milestone v4: Polymetric Sequencing & Advanced Automation
* **Independent Track Lengths (Polymetric Sequencer):** Allows individual tracks in the Pattern Editor to have independent step lengths (e.g. 3, 5, 7, 11 steps), unlocking true polyrhythmic and polymetric composition with automatic Least Common Multiple (LCM) looping.
* **Unused Automation Purge:** One-click clean-up action in Song Editor and Pattern Editor to detect and remove orphaned or unassigned automation tracks.
* **Automatic Linear Curve Mode:** Real-time automation recording now defaults to smooth Linear Curves (`Curves` mode) instead of discrete stair-stepped discrete events.
* **Direct Pattern Export:** Streamlined exporting of isolated patterns directly to audio.

### 🔍 Milestone v5: Precision Controls & Production Workflow
* **Ultra-Fine Zoom & Smooth Scrolling:**
  * **Piano Roll:** 24 fine-grain zoom levels ranging from 3.125% to 1600%.
  * **Song Editor:** Precision zoom scaling from 1 to 1000 px/bar (500 discrete steps) with non-jumping horizontal scroll mechanics.
* **Fractional & Decimal Step Sequencing:** Native support for 1, 2, 3, and decimal (e.g. 2.5) step counts in 4/4 time by eliminating the legacy 1-bar minimum clamp.
* **Melody Track Step Length Preservation:** Pattern Editor context menus now retain custom step length configurations for melodic tracks.
* **Dedicated Piano Roll Step Toolbar:** Relocated the step count controller to a dedicated toolbar so it is never hidden inside overflow dropdown menus.
* **Loop-Safe Automation Recording:** Automation points are modulo-clamped to pattern length, eliminating 60–70 step displacement glitches during loop recording.
* **Automation Nudging (Shift):** Added context menu actions to nudge/shift automation points left or right by 1 step or arbitrary step counts.
* **Portable FX Chain Group Save & Load:** Save and restore complete effect chains (`.lmms-fxgroup` / `.xml`) interchangeably between mixer channels and instrument effect racks.
* **Built-in Commodore 64 SID & STK Rawwaves:** Fully integrated the SID physical emulation plugin and STK physical modeling acoustic rawwaves into the standard portable bundle.

### ⚡ Milestone v6: Auto-Adapting Steps, Smart Musical Randomizers & CPU Guard
* **Auto-Adapting MIDI Step Count (Dynamic Sizing):**
  * Step counts now dynamically adapt to the actual bounds of MIDI notes.
  * Resolved the decimal step calculation bug: note end boundaries are computed with exact step tick duration (`note->pos() + ticksPerStep`), preventing boundary truncation.
  * Two-way dynamic adaptation: expands automatically when notes are placed beyond the baseline and contracts back down to the user's configured baseline (`m_userSteps`) when notes are removed.
* **Smart Musical Instrument Parameter Randomizer:**
  * Sáv- és hangszer-szintű véletlenszerűsítés (`Randomize this instrument` és `Randomize All`).
  * Recursively randomizes internal synthesizer parameters (oscillators, waveforms, filters, envelopes, and internal effects).
  * **Musical Harmony & Routing Invariants:**
    * Preserves pitch, tuning, and semitone transposition (`semi`, `pitch`, `coarse`) to maintain song key harmony.
    * Strictly shields mixer channel bindings (`fxChannel`) and MIDI group assignments (`midigroup`).
    * Shields track mute state (`m_mutedModel`), ensuring instruments remain audible and never silenced.
* **Configurable Random Instrumentation & Effects Generator:**
  * Dedicated **"Random Synths..."** button on both Song Editor and Pattern Editor toolbars.
  * Configurable number of instruments (1–32) and DSP effects per track (0–10).
  * **Persistent Dialog Memory:** Configured quantities are permanently remembered across application restarts via `ConfigManager`.
* **Mixer & Effect Rack Randomizer:**
  * **"Random FX"** action on Effect Rack views allowing the user to generate a specified number of DSP insert effects (1–10) with automatic randomized parameter configurations.
* **Hardware Voice Limit & Voice Stealing (Max Polyphony):**
  * Added hardware-protective polyphony control (`POLY` LCD spinbox on Instrument Track window) with intelligent voice stealing in `NotePlayHandle` to prevent CPU overloads during dense passages.
* **Local Build Concurrency Guard:**
  * Local build toolchain (`build-local.ps1`) dynamically detects available CPU threads and reserves 3–4 cores (`[Math]::Max(1, $totalProcessors - 4)`), guaranteeing system responsiveness during local compilation.

---

## Building LMMS Orion (Windows x64)

### Prerequisites
* Visual Studio 2022 (MSVC v143, x64)
* Qt 6.8.0 MSVC 2022 64-bit (`C:\Qt\6.8.0\msvc2022_64`)
* Ninja Build System & CMake
* vcpkg installed with required audio packages (`C:\vcpkg`)

### Local Incremental Build
```powershell
powershell -ExecutionPolicy Bypass -File C:\lmms\lmms-repo\build-local.ps1
```

### Portable Package Generation
```powershell
powershell -ExecutionPolicy Bypass -File C:\lmms\lmms-repo\deploy-v7.ps1
```
The deploy script outputs a self-contained portable distribution to:
`C:\Users\ahsoka\Downloads\LMMS-Orion-v7`

---

## License

LMMS Orion is released under the GNU General Public License (GPL) version 2 or later, following upstream LMMS.
