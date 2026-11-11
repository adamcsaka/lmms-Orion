# LMMS Orion - Local Build Script for Windows (MSVC x64 + Qt6 + vcpkg)
$ErrorActionPreference = "Stop"

$repoDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $repoDir

Write-Host "=== Setting up LMMS Orion Local Build Environment ===" -ForegroundColor Cyan

# 1. Paths configuration
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$cmakeDir = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
$ninjaDir = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
$qtDir = "C:\Qt\6.8.0\msvc2022_64"
$vcpkgToolchain = "C:\vcpkg\scripts\buildsystems\vcpkg.cmake"

if (-not (Test-Path $vcvars)) {
    Write-Error "Visual Studio 2022 BuildTools vcvars64.bat not found at $vcvars"
}
if (-not (Test-Path (Join-Path $cmakeDir "cmake.exe"))) {
    Write-Error "CMake not found at $cmakeDir"
}
if (-not (Test-Path (Join-Path $ninjaDir "ninja.exe"))) {
    Write-Error "Ninja not found at $ninjaDir"
}
if (-not (Test-Path $vcpkgToolchain)) {
    Write-Error "vcpkg toolchain not found at $vcpkgToolchain"
}

# 2. Add build tools and Qt to PATH for this session
$env:PATH = "$ninjaDir;$cmakeDir;$qtDir\bin;$env:PATH"
$env:CMAKE_PREFIX_PATH = $qtDir
$env:VCPKG_ROOT = "C:\vcpkg"

Write-Host "Build tools and Qt configured successfully." -ForegroundColor Green

# 3. Create build directory if needed
if (-not (Test-Path "$repoDir\build")) {
    New-Item -ItemType Directory -Path "$repoDir\build" -Force | Out-Null
}

# 4. Configure with CMake
Write-Host "=== Configuring CMake with Ninja & vcpkg ===" -ForegroundColor Cyan
& cmd /c "`"$vcvars`" && cmake -S `"$repoDir`" -B `"$repoDir\build`" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTARGET_UARCH=official -DWANT_QT6=ON -DUSE_COMPILE_CACHE=OFF -DUSE_WERROR=OFF -DCMAKE_PREFIX_PATH=`"$qtDir`" --toolchain `"$vcpkgToolchain`" -DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_HOST_TRIPLET=x64-windows"

if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed with exit code $LASTEXITCODE"
}

# 5. Build LMMS
Write-Host "=== Compiling LMMS Orion ===" -ForegroundColor Cyan
& cmd /c "`"$vcvars`" && cmake --build `"$repoDir\build`" --parallel"

if ($LASTEXITCODE -eq 0) {
    Write-Host "=== BUILD SUCCESSFUL! ===" -ForegroundColor Green
    Write-Host "Output executable: $repoDir\build\lmms.exe" -ForegroundColor Green
} else {
    Write-Error "Build failed with exit code $LASTEXITCODE"
}
