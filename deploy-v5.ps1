$dest = "C:\Users\ahsoka\Downloads\LMMS-Orion-v5"
if (Test-Path $dest) {
    Remove-Item -Recurse -Force $dest
}
New-Item -ItemType Directory -Path $dest -Force | Out-Null

Write-Host "Copying build files..."
Copy-Item "C:\lmms\lmms-repo\build\*.exe" -Destination $dest
Copy-Item "C:\lmms\lmms-repo\build\*.dll" -Destination $dest
Copy-Item -Recurse "C:\lmms\lmms-repo\build\plugins" -Destination $dest
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

Write-Host "Running windeployqt..."
& "C:\Qt\6.8.0\msvc2022_64\bin\windeployqt.exe" --no-translations --compiler-runtime "$dest\lmms.exe"

Write-Host "Verifying executable..."
$ver = & "$dest\lmms.exe" --dump
Write-Host "Dump result: $ver"
Write-Host "=== LMMS ORION V5 DEPLOYMENT COMPLETE ===" -ForegroundColor Green
