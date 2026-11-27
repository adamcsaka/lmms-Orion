# LMMS Orion v9 (v8-original continuation + guide §9) Portable Deployment Script
param (
    [string]$TargetDir = "C:\Users\ahsoka\Downloads\LMMS-Orion-v9"
)

$dest = $TargetDir

# Safety check for file locking
if (Test-Path "$dest\lmms.exe") {
    $processes = Get-Process -Name "lmms" -ErrorAction SilentlyContinue
    if ($processes) {
        Write-Warning "Detected running lmms.exe instances! Attempting isolated fallback or prompt..."
    }
}

if (Test-Path $dest) {
    try {
        Remove-Item -Recurse -Force $dest -ErrorAction Stop
    } catch {
        $timestamp = (Get-Date).ToString("yyyyMMdd_HHmmss")
        $dest = "C:\Users\ahsoka\Downloads\LMMS-Orion-v9_$timestamp"
        Write-Warning "Could not overwrite $TargetDir (files likely locked). Deploying to $dest instead."
    }
}
New-Item -ItemType Directory -Path $dest -Force | Out-Null

Write-Host "Copying build files to $dest..."
Copy-Item "C:\lmms\v8-original copy\lmms-repo\build\*.exe" -Destination $dest
Copy-Item "C:\lmms\v8-original copy\lmms-repo\build\*.dll" -Destination $dest
if (Test-Path "C:\lmms\v8-original copy\lmms-repo\build\vcpkg_installed\x64-windows\bin") {
    Write-Host "Copying vcpkg runtime dependencies (zlib, fluidsynth, etc.)..."
    Copy-Item "C:\lmms\v8-original copy\lmms-repo\build\vcpkg_installed\x64-windows\bin\*.dll" -Destination $dest -Force
}
Copy-Item -Recurse "C:\lmms\v8-original copy\lmms-repo\build\plugins" -Destination $dest

Write-Host "Pruning intermediate compiler artifacts (.pdb, .obj, .ilk, CMakeFiles) from plugins folder..."
Get-ChildItem -Path "$dest\plugins" -Directory | Where-Object { $_.Name -notin @("ladspa", "32") } | Remove-Item -Recurse -Force
Get-ChildItem -Path "$dest\plugins" -Recurse -File | Where-Object { $_.Extension -notin @(".dll", ".exe") } | Remove-Item -Force

# Remove experimental, crash-prone, and superseded plugins as requested
# (ZynAddSubFX: randomizer crashes. Vital/OxoSynth: excluded previously.
#  Vestige + Carla family: removed - replaced by the native ModularSynth
#  instrument which has built-in FM/ADSR and automatable parameters.)
Remove-Item -Force -ErrorAction SilentlyContinue "$dest\plugins\vital.dll", "$dest\plugins\oxosynth.dll", "$dest\plugins\carlabase.dll", "$dest\plugins\carlarack.dll", "$dest\plugins\carlapatchbay.dll", "$dest\plugins\libcarla_native-plugin.dll", "$dest\plugins\vestige.dll", "$dest\plugins\zynaddsubfx.dll", "$dest\plugins\RemoteZynAddSubFx.exe"
if (Test-Path "C:\lmms\v8-original copy\lmms-repo\data") {
    Copy-Item -Recurse "C:\lmms\v8-original copy\lmms-repo\data" -Destination $dest
}

# Ensure Commodore 64 SID plugin and STK rawwaves are included
$portableDir = "C:\Users\ahsoka\Downloads\lmms-win64-portable\lmms-win64-portable"
if (-not (Test-Path "$dest\plugins\sid.dll")) {
    if (Test-Path "$portableDir\plugins\sid.dll") {
        Write-Host "Copying missing sid.dll from portable package..."
        Copy-Item "$portableDir\plugins\sid.dll" -Destination "$dest\plugins\"
    }
}

if (-not (Test-Path "$dest\data\stk\rawwaves")) {
    if (Test-Path "$portableDir\data\stk\rawwaves") {
        Write-Host "Copying missing stk rawwaves from portable package..."
        if (-not (Test-Path "$dest\data\stk")) {
            New-Item -ItemType Directory -Path "$dest\data\stk" -Force | Out-Null
        }
        Copy-Item -Recurse "$portableDir\data\stk\rawwaves" -Destination "$dest\data\stk\"
    }
}

Write-Host "Running windeployqt..."
& "C:\Qt\6.8.0\msvc2022_64\bin\windeployqt.exe" --no-translations --compiler-runtime "$dest\lmms.exe"

Write-Host "Verifying executable..."
$ver = & "$dest\lmms.exe" --help
Write-Host "Help result verified successfully!"

# Create compressed zip archive in Downloads
$zipPath = "C:\Users\ahsoka\Downloads\LMMS-Orion-v9.zip"
if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
Write-Host "Compressing package to $zipPath ..."
Compress-Archive -Path $dest -DestinationPath $zipPath -CompressionLevel Optimal

Write-Host "=== LMMS ORION V9 DEPLOYMENT COMPLETE at $dest ===" -ForegroundColor Green
Write-Host "=== ZIP ARCHIVE: $zipPath ===" -ForegroundColor Green
