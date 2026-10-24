# Native Windows simulator. All downloaded dependencies and caches stay in this checkout.
$ErrorActionPreference = 'Stop'
$simulatorArguments = @($args)
$projectRoot = Split-Path $PSScriptRoot -Parent
$buildDirectory = Join-Path $projectRoot 'build-simulator-windows'
$cacheDirectory = Join-Path $projectRoot '.cache'

function Find-Tool($Name, $Patterns) {
    $command = Get-Command $Name -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($command) { return $command.Source }
    foreach ($pattern in $Patterns) {
        $tool = Get-Item -Path $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($tool) { return $tool.FullName }
    }
    throw "Missing $Name. Install it or add it to PATH, then rerun scripts/simulate.ps1."
}

$cmake = Find-Tool 'cmake.exe' @('C:/Espressif/tools/cmake/*/bin/cmake.exe', 'C:/Espressif-5/tools/cmake/*/bin/cmake.exe')
$ninja = Find-Tool 'ninja.exe' @('C:/Espressif-5/tools/ninja/*/ninja.exe', 'C:/Espressif/tools/ninja/*/ninja.exe')
$zig = Find-Tool 'zig.exe' @((Join-Path $cacheDirectory 'simulator-tools/ziglang/zig.exe'), (Join-Path $cacheDirectory 'preview-tools/ziglang/zig.exe'))
$archiver = Find-Tool 'llvm-ar.exe' @('C:/Espressif-5/tools/esp-clang/*/esp-clang/bin/llvm-ar.exe', 'C:/Espressif/tools/esp-clang/*/esp-clang/bin/llvm-ar.exe')
$ranlib = Find-Tool 'llvm-ranlib.exe' @('C:/Espressif-5/tools/esp-clang/*/esp-clang/bin/llvm-ranlib.exe', 'C:/Espressif/tools/esp-clang/*/esp-clang/bin/llvm-ranlib.exe')

$sdlVersion = '2.32.10'
$dependencyDirectory = Join-Path $cacheDirectory 'simulator-deps'
$sdlDirectory = Join-Path $dependencyDirectory "SDL2-$sdlVersion"
if (-not (Test-Path (Join-Path $sdlDirectory 'cmake/sdl2-config.cmake'))) {
    New-Item -ItemType Directory -Force $dependencyDirectory | Out-Null
    $archive = Join-Path $dependencyDirectory "SDL2-devel-$sdlVersion-VC.zip"
    Write-Host "Downloading SDL2 $sdlVersion into the project cache..."
    Invoke-WebRequest -UseBasicParsing -Uri "https://github.com/libsdl-org/SDL/releases/download/release-$sdlVersion/SDL2-devel-$sdlVersion-VC.zip" -OutFile $archive
    $expectedHash = 'AF347939395A58B365846AAEA27391E69F9EC9D4DD650D6AC40802159B418A6E'
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expectedHash) {
        throw 'SDL2 archive checksum mismatch.'
    }
    Expand-Archive -LiteralPath $archive -DestinationPath $dependencyDirectory -Force
}

$layout = if ($env:TESLA_HEADUP_DASHBOARD_LAYOUT) { $env:TESLA_HEADUP_DASHBOARD_LAYOUT } else { 'true_hud' }
if ($layout -notin @('true_hud', 'road_scene')) { throw "Unknown layout: $layout" }
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $cacheDirectory 'zig-global'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $cacheDirectory 'zig-local'
$config = @('-S', $projectRoot, '-B', $buildDirectory, '-G', 'Ninja',
    "-DCMAKE_MAKE_PROGRAM=$ninja", "-DCMAKE_C_COMPILER=$zig", '-DCMAKE_C_COMPILER_ARG1=cc',
    "-DCMAKE_CXX_COMPILER=$zig", '-DCMAKE_CXX_COMPILER_ARG1=c++',
    "-DCMAKE_ASM_COMPILER=$zig", '-DCMAKE_ASM_COMPILER_ARG1=cc',
    "-DCMAKE_AR=$archiver", "-DCMAKE_RANLIB=$ranlib", '-DCMAKE_SIZEOF_VOID_P=8',
    '-DCMAKE_BUILD_TYPE=Release', '-DTESLA_HEADUP_BUILD_SIMULATOR=ON', '-DTESLA_HEADUP_BUILD_TESTS=OFF',
    "-DTESLA_HEADUP_DASHBOARD_LAYOUT=$layout", "-DSDL2_DIR=$sdlDirectory/cmake")
$localLvgl = Join-Path $projectRoot 'firmware/managed_components/lvgl__lvgl'
if (Test-Path (Join-Path $localLvgl 'CMakeLists.txt')) {
    $config += "-DFETCHCONTENT_SOURCE_DIR_LVGL=$localLvgl"
}
# Keep compiler-probe scratch files in the ignored build directory.
New-Item -ItemType Directory -Force $buildDirectory | Out-Null
Push-Location $buildDirectory
try {
    & $cmake @config
    $configureExitCode = $LASTEXITCODE
} finally {
    Pop-Location
}
if ($configureExitCode -ne 0) { exit $configureExitCode }
& $cmake --build $buildDirectory --target tesla_headup_simulator --parallel 6
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& (Join-Path $buildDirectory 'simulator/tesla_headup_simulator.exe') @simulatorArguments
exit $LASTEXITCODE
