# LMMS Orion Edition - v7.0.0 Release

<div align="center">
  <h3>Modernized, High-Precision, Producer-Focused Digital Audio Workstation</h3>
  <p><strong>Available for Windows, Linux, and macOS</strong></p>
</div>

---

## 🇭🇺 Magyar Részletes Verzióinformáció (Changelog v7.0.0)

Az **LMMS Orion v7.0.0** egy átfogó, kifejezetten zenei producerek számára kifejlesztett modernizált kiadás. Ez a változat megszünteti a klasszikus LMMS stabilitási és grafikai szűk keresztmetszeteit, modern 64-bites többmagos környezetre optimalizál, és professzionális szekvenszer-, hangszer- és megjelenés-testreszabási eszközöket biztosít.

### 🎨 1. Dinamikus Téma- és Felületszínező Motor (`ThemeManager` & `ThemeDialog`)
* **Azonnali, natív Qt stíluskezelő:** Újraindítás nélküli, valós idejű szín- és palettaváltás a `Theme & Appearance...` ablakban (Toolbar színpaletta ikon vagy **Ctrl+8**).
* **5 Hivatalos Gyári Téma:**
  * **Orion Obsidian Gold:** Elegáns prémium fekete-arany studiópaletta.
  * **Cyber Neon Cyan:** Ragyogó futurisztikus neonkék / cián szintetizátor téma.
  * **Studio Dark Lime:** Modern zöld akcentusú, szemkímélő sötétszürke felület.
  * **Solarized Amber:** Meleg analóg borostyán-sárga paletta.
  * **Classic LMMS:** A hagyományos zöld-szürke felület kedvelőinek.
* **Ablakkeretek és Fejlécek Színezése:** Minden belső ablak (Song Editor, Piano Roll, Mixer, VST és szintetizátorok) éles, 1px-es elegáns keretet kap, az aktív ablak fejléce pedig lágy színátmenettel emeli ki a fókuszt.
* **Munkaterület Háttérképek (Wallpapers):** Egyedi képek (`.png`, `.jpg`, `.svg`) betöltése a munkaterület mögé sima méretezéssel.
* **Projektenkénti Témamentés (`<projecttheme>` XML):** Minden zeneszám elmentheti a saját egyedi színeit és háttérképét a `.mmp` / `.mmpz` fájlba.
* **Globális Védelem:** A beállításokban kikapcsolható a mások projektjeiből érkező témák betöltése, így a saját kedvenc színeid mindig megmaradnak.

### 🛡️ 2. Motorstabilitás és Fagyásvédelem (Crash Prevention)
* **ZynAddSubFX SVFilter Null Pointer Fix (0xc0000005):** Kijavítva a véletlenszerűsítéskor és analóg szűrőváltáskor fellépő végzetes memóriahiba.
* **SlicerT Audio-GUI Szálfüggetlenítés:** A hullámforma-kirajzolás és gombkötések leválasztva a valós idejű audio renderelő szálról, megszűntetve a kattogásokat és a fagyásokat.
* **Carla & Vestige Dupla Törlés Védelem:** Megszűnt a VST ablakok bezárásakor és sávtörléskor előforduló heap crash.
* **AudioFileProcessor Puffer Védelem:** Üres vagy hibás hangfájlok betöltésekor nem lép ki a program.
* **Hardveres Windows SEH Kivételkezelő:** Bármilyen váratlan processzor-szintű hiba esetén azonnali diagnosztikai ablak jelenik meg a hibacímmel, nem záródik be némán a program.

### 🎛️ 3. Precíziós Automatizáció és Szekvenszer Vezérlők
* **Többpontos Kijelölés és Mozgatás:** Rubber-band keretes kijelölés, csoportos idősíkbeli és értékszintű mozgatás rácshoz igazítva vagy finom mozgással (Ctrl gombbal).
* **Időzítési Szorzók:** `-1 Bar`, `+1 Bar`, `x2` (Duplázás) és `/2` (Felezés) gombok az Automation Editor és Piano Roll eszköztárán.
* **Dinamikus MIDI Lépéshossz Adaptáció:** A clip-ek hossza automatikusan alkalmazkodik a leütött hangok valós hosszához.
* **Polymetric Szekvenszer:** Sávonként független lépéshosszok (3, 5, 7 lépés) igazi poliritmikus zenék készítéséhez.

### ⚡ 4. Zenei Hangszer- és Effekt Véletlenszerűsítő
* **Zenei Hangnem Védelem:** A pitch és transzpozíció védve van, így a hangszerek sosem hamisodnak el.
* **Routing és Mute Védelem:** A mixer-csatornák és MIDI csoportok nem keverednek össze, a sávok nem némulnak le.
* **Azonnali Ctrl+Z Visszavonás:** Minden átalakítás azonnal visszavonható az előzményekből.
* **Hallásvédő Rezonancia-korlát:** Safe 70% Q limit a szűrőkön, kizárva a hangszórót és fület terhelő gerjedéseket.

---

## 🇬🇧 English Detailed Changelog (v7.0.0 Milestone)

**LMMS Orion v7.0.0** is an advanced, production-grade fork of LMMS built to eliminate workflow friction, resolve long-standing stability bottlenecks, and empower audio producers with modern styling, polymetric sequencing, and non-destructive sound design tools.

### 🎨 1. Dynamic Theming & Appearance Engine
* **Zero-Restart Live Palette Switching:** Instant Qt styling via the `Theme & Appearance` modal (**Ctrl+8** or palette icon).
* **5 Curated Factory Presets:** *Orion Obsidian Gold*, *Cyber Neon Cyan*, *Studio Dark Lime*, *Solarized Amber*, and *Classic LMMS*.
* **Custom Window Frames & Titlebars:** All subwindows (Song Editor, Piano Roll, Mixer, Instrument/VST windows) feature color-coded 1px borders and dynamic active/inactive titlebar gradients.
* **Custom Wallpaper Canvas:** Smooth scaling of custom background artwork (`.png`, `.jpg`, `.svg`) behind the workspace.
* **Per-Project Visual Identity (`<projecttheme>` XML):** Projects store unique palettes and wallpapers directly in project files.
* **Global Theme Override:** Preference toggle to enforce personal theme defaults across external projects.

### 🛡️ 2. Core Engine Stability & Crash Prevention
* **ZynAddSubFX SVFilter Crash Fix (0xc0000005):** Hardened filter state against out-of-bounds filter indices during rapid switching and randomization.
* **SlicerT Audio-to-GUI Thread Decoupling:** Redraw signals are strictly queued to the main GUI thread, eliminating audio thread stalls and vtable jump errors.
* **Vestige & Carla Subwindow Double-Free Fix:** Guarded window deletion preventing double-free heap corruption when closing plugins or removing tracks.
* **AudioFileProcessor Null Buffer Guard:** Safe guards preventing host termination when loading corrupted or zero-length audio files.
* **Native Windows SEH Hardware Interceptor:** Top-level Win32 exception filter providing actionable diagnostics instead of silent app closures.

### 🎛️ 3. Precision Automation & Sequencing
* **Multi-Node Automation Selection & Dragging:** Rubber-band canvas selection, simultaneous multi-node translation across time and parameter bounds with quantization toggle.
* **Quick Length Modifiers:** Instant `-1 Bar`, `+1 Bar`, `x2` (Double Content), and `/2` (Halve Content) buttons on both Automation Editor and Piano Roll toolbars.
* **Dynamic MIDI Step Auto-Adaptation:** MidiClips seamlessly expand and contract based on actual note positions.
* **Polymetric Sequencer:** Independent step lengths per track for true polyrhythmic beatmaking.

### ⚡ 4. Smart Musical Randomizers & Sound Design
* **Harmonic Integrity:** Shields transpose and coarse detuning to keep sound design musically harmonious.
* **Routing & Mute Protection:** Preserves mixer channel routing (`fxChannel`), MIDI bonds, and track audibility.
* **Instant Ctrl+Z Undo:** Journal checkpointing before transformations ensures immediate non-destructive revert.
* **Acoustic Q Safety Limit:** Enforces a 70% resonance ceiling to safeguard hearing and studio monitors from destructive feedback spikes.

---

## 📦 Downloads & Installation

| Platform | Package | Type |
| :--- | :--- | :--- |
| **Windows x64** | `lmms-7.0.0-msvc2022-win64.exe` | Official Setup Installer |
| **Windows x64** | `LMMS-Orion-v7-win64-portable.zip` | Standalone Portable ZIP |
| **Linux x86_64** | `lmms-7.0.0-linux-x86_64.AppImage` | Portable AppImage |
| **macOS x86_64** | `lmms-7.0.0-macOS.dmg` | Disk Image Installer |

*Built with Microsoft Visual C++ 2022, Qt 6.8, GCC 13, and Ninja.*
