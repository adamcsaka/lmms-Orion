# LMMS Orion v7 Portable Deployment Script
param (
    [string]$TargetDir = "C:\Users\ahsoka\Downloads\LMMS-Orion-v7"
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
        $dest = "C:\Users\ahsoka\Downloads\LMMS-Orion-v7_$timestamp"
        Write-Warning "Could not overwrite $TargetDir (files likely locked). Deploying to $dest instead."
    }
}
New-Item -ItemType Directory -Path $dest -Force | Out-Null

Write-Host "Copying build files to $dest..."
Copy-Item "C:\lmms\lmms-repo\build\*.exe" -Destination $dest
Copy-Item "C:\lmms\lmms-repo\build\*.dll" -Destination $dest
if (Test-Path "C:\lmms\lmms-repo\build\vcpkg_installed\x64-windows\bin") {
    Write-Host "Copying vcpkg runtime dependencies (zlib, fluidsynth, etc.)..."
    Copy-Item "C:\lmms\lmms-repo\build\vcpkg_installed\x64-windows\bin\*.dll" -Destination $dest -Force
}
Copy-Item -Recurse "C:\lmms\lmms-repo\build\plugins" -Destination $dest

Write-Host "Pruning intermediate compiler artifacts (.pdb, .obj, .ilk, CMakeFiles) from plugins folder..."
Get-ChildItem -Path "$dest\plugins" -Directory | Where-Object { $_.Name -notin @("ladspa", "32") } | Remove-Item -Recurse -Force
Get-ChildItem -Path "$dest\plugins" -Recurse -File | Where-Object { $_.Extension -notin @(".dll", ".exe") } | Remove-Item -Force

# Remove experimental and crash-prone plugins as requested
Remove-Item -Force -ErrorAction SilentlyContinue "$dest\plugins\vital.dll", "$dest\plugins\oxosynth.dll", "$dest\plugins\carlabase.dll", "$dest\plugins\carlarack.dll", "$dest\plugins\carlapatchbay.dll"
if (Test-Path "C:\lmms\lmms-repo\data") {
    Copy-Item -Recurse "C:\lmms\lmms-repo\data" -Destination $dest
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

# Ensure ReShade integration is preserved if available
$vcmiReShade = "C:\Users\ahsoka\Documents\Alapértelmezett projekt\heroes3mód\vcmi\build\bin\Release"
if (-not (Test-Path "$dest\dxgi.dll") -and (Test-Path "$vcmiReShade\dxgi.dll")) {
    Write-Host "Integrating ReShade and shaders from VCMI directory..."
    Copy-Item "$vcmiReShade\dxgi.dll" -Destination $dest -Force
    Copy-Item "$vcmiReShade\ReShade64.dll" -Destination $dest -Force
    if (Test-Path "$vcmiReShade\ReShade64.json") { Copy-Item "$vcmiReShade\ReShade64.json" -Destination $dest -Force }
    Copy-Item "$vcmiReShade\ReShade.ini" -Destination $dest -Force
    if (Test-Path "$vcmiReShade\ReShadePreset.ini") { Copy-Item "$vcmiReShade\ReShadePreset.ini" -Destination $dest -Force }
    Get-ChildItem -Path $vcmiReShade -Filter "*.ini" | ForEach-Object { Copy-Item $_.FullName -Destination $dest -Force }
    Copy-Item -Recurse "$vcmiReShade\reshade-shaders" -Destination $dest -Force
    if (Test-Path "$dest\ReShade.ini") {
        (Get-Content "$dest\ReShade.ini") | ForEach-Object {
            $_ -replace 'IntermediateCachePath=.*', 'IntermediateCachePath=.\reshade-shaders'
        } | Set-Content "$dest\ReShade.ini"
    }
}

# Create a convenient ReShade launcher batch script
$launcherContent = @"
@echo off
set QT_WIDGETS_RHI=1
set QSG_RHI_BACKEND=d3d11
start "" "%~dp0lmms.exe" %*
"@
Set-Content -Path "$dest\launch-reshade.bat" -Value $launcherContent -Encoding ASCII

Write-Host "Running windeployqt..."
& "C:\Qt\6.8.0\msvc2022_64\bin\windeployqt.exe" --no-translations --compiler-runtime "$dest\lmms.exe"

Write-Host "Verifying executable..."
$ver = & "$dest\lmms.exe" --help
Write-Host "Help result verified successfully!"
Write-Host "=== LMMS ORION V7 DEPLOYMENT COMPLETE at $dest ===" -ForegroundColor Green
