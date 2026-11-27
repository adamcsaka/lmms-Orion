# LMMS Orion - incremental ninja build only (assumes cmake already configured)
$repoDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
$ninja = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
$buildThreads = [Math]::Max(1, [System.Environment]::ProcessorCount - 4)
& cmd /c "`"$vcvars`" && `"$ninja`" -C `"$repoDir\build`" -j $buildThreads"
exit $LASTEXITCODE
