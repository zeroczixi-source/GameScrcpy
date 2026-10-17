$ErrorActionPreference = 'Continue'
$env:PATH = "C:\Users\zero\qt\Tools\Tools\mingw1310_64\bin;" + $env:PATH
$cmakeRoot = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake"
$env:PATH = "$cmakeRoot\CMake\bin;$cmakeRoot\Ninja;" + $env:PATH
Set-Location "C:\Users\zero\Downloads\GameScrcpy-main"

& "$cmakeRoot\CMake\bin\cmake.exe" -S client -B client/build/mingw -DQSC_DEBUG_INFO=ON 2>&1 | Select-Object -Last 12

& "$cmakeRoot\Ninja\ninja.exe" -C client/build/mingw -j 8 *> client\build\build-p0.log
"NINJA_EXIT=$LASTEXITCODE"
Get-Content client\build\build-p0.log -Tail 20
