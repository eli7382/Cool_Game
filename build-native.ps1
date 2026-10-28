param(
    [string]$ToolchainRoot=(Join-Path $PSScriptRoot '.tools/w64devkit'),
    [string]$RaylibSource=(Join-Path $PSScriptRoot '.tools/raylib-5.5')
)
$ErrorActionPreference='Stop'
$taskBin=Join-Path $ToolchainRoot 'bin'
$taskCMake=Join-Path $taskBin 'cmake.exe'
if (!(Test-Path -LiteralPath $taskCMake)) { throw 'Portable compiler tools missing. See docs/PORSCHE_INTEGRATION.md.' }
if (!(Test-Path -LiteralPath (Join-Path $RaylibSource 'src/raylib.h'))) { throw 'Official raylib 5.5 sources missing.' }
$taskOldPath=$env:PATH
try {
    $env:PATH="$taskBin;$taskOldPath"
    & $taskCMake -S "$PSScriptRoot/cmake/standalone" -B "$PSScriptRoot/build-native/ninja" -G 'Ninja' "-DCMAKE_C_COMPILER=$taskBin/gcc.exe" "-DCMAKE_CXX_COMPILER=$taskBin/g++.exe" "-DCMAKE_MAKE_PROGRAM=$taskBin/ninja.exe" "-DRAYLIB_SOURCE_DIR=$RaylibSource" '-DCMAKE_BUILD_TYPE=Release' '-DCMAKE_POLICY_VERSION_MINIMUM=3.5'
    if ($LASTEXITCODE -ne 0) { throw 'Native configure failed' }
    & $taskCMake --build "$PSScriptRoot/build-native/ninja" --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Native build failed' }
} finally { $env:PATH=$taskOldPath }
