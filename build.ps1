$ErrorActionPreference = 'Stop'
$runName = Split-Path $PSScriptRoot -Leaf
& (Join-Path $PSScriptRoot '..\..\tools\Build-Run.ps1') -RunName $runName
