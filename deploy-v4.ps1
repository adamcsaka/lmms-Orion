$dest = "C:\Users\ahsoka\Downloads\LMMS-Orion-v4"
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

Write-Host "Running windeployqt..."
& "C:\Qt\6.8.0\msvc2022_64\bin\windeployqt.exe" --no-translations --compiler-runtime "$dest\lmms.exe"

Write-Host "Verifying executable..."
$ver = & "$dest\lmms.exe" --dump
Write-Host "Dump result: $ver"
Write-Host "=== PORTABLE DEPLOYMENT COMPLETE ===" -ForegroundColor Green
