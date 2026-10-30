$ErrorActionPreference = 'Stop'
$workspace = $PSScriptRoot
$executable = @('dist\Breakwater_Windows_x64.exe','build-native\ninja\bin\game.exe') |
    ForEach-Object { Join-Path $workspace $_ } |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
    Select-Object -First 1
if (-not $executable) { throw 'Build the game with build-native.ps1 before launching.' }

Start-Process -FilePath $executable -WorkingDirectory $workspace
