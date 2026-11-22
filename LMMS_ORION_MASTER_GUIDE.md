# LMMS Orion - Master Handover & Build Architecture Guide

Ez a dokumentum az **LMMS Orion** teljes fejlesztési, felépítési és konfigurációs állapotát rögzíti, hogy egy teljesen új konverzációban vagy új gépen is azonnal, zökkenőmentesen lehessen folytatni a munkát.

---

## 1. Helyi környezet és elérési utak (Windows x64)

* **Repository gyökérkönyvtár:** `C:\lmms\lmms-repo`
* **Git parancs:** Mindig explicit elérési úttal:
  `& 'C:\lmms\git\cmd\git.exe' -C 'C:\lmms\lmms-repo'`
* **GitHub távoli tárhely (Remote origin):**
  `https://github.com/adamcsaka/lmms-Orion.git`
  > **KRITIKUS GIT BEÁLLÍTÁS:** Git parancsoknál (különösen `git push` és `git ls-remote`) **KÖTELEZŐ** a `-c credential.helper=` opció használata, ellenkező esetben a Windows `helper-selector` dialógusa felugró ablakra vagy interakcióra vár a háttérben, és a parancs lefagy.
  > Helyes forma: `& 'C:\lmms\git\cmd\git.exe' -C 'C:\lmms\lmms-repo' -c credential.helper= push origin <branch>`

### Fordítási eszközök (Build Toolchain):
* **MSVC vcvars64 környezet:** `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat`
* **CMake:** `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
* **Ninja:** `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe`
* **Qt 6.8.0 x64:** `C:\Qt\6.8.0\msvc2022_64`
* **windeployqt:** `C:\Qt\6.8.0\msvc2022_64\bin\windeployqt.exe`
* **vcpkg (42 könyvtár telepítve és gyorsítótárazva):**
  - Gyökér: `C:\vcpkg`
  - Toolchain fájl: `C:\vcpkg\scripts\buildsystems\vcpkg.cmake`
  - Triplet: `x64-windows`

---

## 2. Felépítési (Build) és Telepítési (Deploy) folyamat

1. **Helyi inkrementális fordítás (másodpercek alatt a 12 szálon):**
   ```powershell
   powershell -ExecutionPolicy Bypass -File C:\lmms\lmms-repo\build-local.ps1
   ```
   A script beállítja a `vcvars64.bat`-ot, a Ninja párhuzamos szálaival fordít, a kimenet pedig: `C:\lmms\lmms-repo\build\lmms.exe`.

2. **Hordozható (Portable) csomagolás:**
   ```powershell
   powershell -ExecutionPolicy Bypass -File C:\lmms\lmms-repo\deploy-v5.ps1
   ```
   A csomagolás menete:
   - Új mappát hoz létre a Letöltésekben: `C:\Users\ahsoka\Downloads\LMMS-Orion-vX`
   - Bemásolja a buildelt `lmms.exe`-t, `.dll`-eket és a `plugins/` mappát.
   - Bemásolja a forráskód `data/` mappáját (témák, minták, előbeállítások).
   - **Kritikus komponensek pótlása (ha a buildből hiányoznának):**
     - Commodore 64 SID plugin: `C:\Users\ahsoka\Downloads\lmms-win64-portable\lmms-win64-portable\plugins\sid.dll` -> `$dest\plugins\`
     - STK rawwaves (Mallets és fizikai modellezés): `C:\Users\ahsoka\Downloads\lmms-win64-portable\lmms-win64-portable\data\stk\rawwaves` -> `$dest\data\stk\rawwaves\`
   - Lefuttatja a `windeployqt.exe`-t az összes Qt 6 runtime fájl mellérakásához.
   - Ellenőrzi a binárist (`lmms.exe --dump` és `-v`).

---

## 3. A felhasználó alapvető elvárásai és szabályai

1. **Elkülönített verziók:** Minden verzió külön, tiszta mappába kerül a Letöltések mappában (`LMMS-Orion-v3`, `LMMS-Orion-v4`, `LMMS-Orion-v5` stb.), a korábbi verziók nem íródnak felül és nem vesznek el.
2. **Helyi tesztelés prioritása (Miért helyben fordítunk?):**
   - A felhasználó gépén egy gyors, 12 logikai szálas processzor fut, a buildkörnyezet (MSVC 2022, Ninja, Qt 6.8, vcpkg) teljesen be van állítva helyben.
   - A Ninja inkrementális fordítással csak a ténylegesen módosított C++ fájlokat fordítja újra, ami **másodpercek alatt elkészül**.
   - Nem szabad a teszteléshez a lassú GitHub runnerre várni (ahol 2 magon percekig/félóráig tartana egy teljes build), a fejlesztés és kipróbálás mindig helyben történik!
3. **Azonnali Git mentés:**
   - Amint elkészültek a módosítások és helyben lefordult/tesztelve lett a szoftver, a forráskódot azonnal el kell menteni (commit) és fel kell tölteni a GitHubra az aktuális verzió ágára (`v5`, `v6` stb.).
   - A GitHub a forráskód biztonságos felhős tárhelye és verziókövetője.
4. **GitHub Actions viselkedés és sorrend:**
   - A GitHub Actions felhős fordítás nem kötelező minden apró változtatás teszteléséhez.
   - Amikor felhős build fut, a **Windows zip/portable csomag elkészítése az abszolút elsődleges prioritás**, ezt kövesse a Linux AppImage. A felesleges vagy hibára hajlamos egyéb célokat (pl. régi macOS, 32-bites felesleges ágak) ki kell hagyni.
   - A felhős buildelés a felhasználó gépét nem lassítja, a háttérben fut. Ha kész, a letölthető csomagot le lehet tölteni és kicsomagolni a Letöltések mappába.

---

## 4. Eddigi fejlesztések áttekintése (v1 - v5)

* **v1:** Alapvető Qt 6 modernizáció és MSVC 2022 kompatibilitás.
* **v2:** Új sáv- és pattern-kezelési logikák.
* **v3:** Pattern loopolási és szekvenszer finomítások.
* **v4:**
  - Független sávhosszok a Pattern Editorban (polimetrikus szekvenszer, legkisebb közös többszörös / LCM számítás).
  - Nem használt automatika sávok egykattintásos törlése a Song Editorból és Pattern Editorból.
  - Automatikus lineáris görbe (Curves) mód élő automatika felvételnél.
  - Pattern export funkció.
* **v5:**
  - **Finomhangolt zoomolás és görgetés:** Song Editor határok (1-1000 px/bar, 500 lépés); Piano Roll 24 finom zoom-szint (3,125%-tól 1600%-ig); stabil, nem ugráló vízszintes görgetés.
  - **Tört és decimális lépésszám:** Step Sequencer 1, 2, 3 és decimális (pl. 2.5) lépés támogatása 4/4-ben a kényszerített 1 ütemes korlát eltávolításával.
  - **Melody sávok lépésszám-szerkesztése:** Pattern Editorban a dallamos sávok helyi menüje megőrzi a lépésszám-beállítást.
  - **Dedikált Piano Roll lépésszám eszköztár:** Külön toolbarra helyezve a lépésszám mező, így nem tűnik el a lenyíló `>>` menüben.
  - **Automatika felvétel javítás:** Pattern hosszra modulózva, nincs többé 60-70 lépéses ugrás looping lejátszásnál.
  - **Automatika mozgatás/eltolás:** Shift Left / Shift Right (1 step) és tetszőleges lépésű Shift opciók a helyi menüben.
  - **Effektcsoport (FX Chain) mentés és betöltés:** Keverő és hangszerek között felcserélhető `.lmms-fxgroup` / `.xml` mentés-betöltés.
  - **Véletlenszerű hangszerelés-generátor:** 3-5 véletlenszerű szintetizátorsáv DSP effektekkel való létrehozása egy gombnyomásra.
  - **Mester paraméter-randomizáló:** Minden hangszer potmétereinek, szűrőinek és pitch-ének (-12..+12 félhang) újrarandomizálása.
  - **Hiányzó elemek integrálása:** SID plugin és STK rawwaves beágyazása a csomagba.
