$ErrorActionPreference = 'Stop'
$workspace = $PSScriptRoot
$executable = Join-Path $workspace 'dist\Breakwater_Windows_x64.exe'

if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "Packaged game executable not found: $executable"
}

Start-Process -FilePath $executable -WorkingDirectory $workspace
