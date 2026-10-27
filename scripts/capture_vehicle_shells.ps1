param([string[]]$Modes = @('car','car-front','car-cockpit','plane','plane-front','plane-cockpit','heli','heli-front','heli-cockpit'))
$ErrorActionPreference = 'Stop'
$workspace = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$exe = (Resolve-Path (Join-Path $workspace 'build\bin\Release\game.exe')).Path
$results = [System.Collections.Generic.List[object]]::new()
foreach ($mode in $Modes) {
    $started = Get-Date
    $process = Start-Process -FilePath $exe -ArgumentList ("--qa-capture={0} --qa-silent" -f $mode) -WorkingDirectory $workspace -PassThru
    try {
        $exited = $process.WaitForExit(30000)
        if (-not $exited) { throw "Game did not complete the in-game capture for $mode." }
        if ($process.ExitCode -ne 0) { throw "Game exited with $($process.ExitCode) while capturing $mode." }
        $shot = Get-ChildItem (Join-Path $workspace 'screenshots') -Filter ("Breakwater_LiveQA_{0}_*.png" -f $mode) |
            Where-Object { $_.LastWriteTime -ge $started } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if (-not $shot -or $shot.Length -lt 50000) { throw "No valid in-game framebuffer image was written for $mode." }
        $results.Add([pscustomobject]@{mode=$mode;exit_code=$process.ExitCode;screenshot=$shot.FullName;bytes=$shot.Length})
    }
    finally {
        if ($process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id -Force
        }
    }
}
$results | ConvertTo-Json -Depth 3
