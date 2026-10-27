$ErrorActionPreference = 'Stop'
$workspace = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$exe = (Resolve-Path (Join-Path $workspace 'build\bin\Release\game.exe')).Path
$modes = @('car','car-front','car-cockpit','plane','plane-front','plane-cockpit','heli','heli-front','heli-cockpit')
$captures = [System.Collections.Generic.List[object]]::new()

foreach ($mode in $modes) {
    $started = Get-Date
    $process = Start-Process -FilePath $exe -ArgumentList ("--qa-capture={0} --qa-silent" -f $mode) -WorkingDirectory $workspace -PassThru
    $exited = $process.WaitForExit(20000)
    $process.Refresh()
    $shot = Get-ChildItem -LiteralPath (Join-Path $workspace 'screenshots') -Filter ("Breakwater_LiveQA_{0}_*.png" -f $mode) |
        Where-Object { $_.LastWriteTime -ge $started } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (-not $exited -or $process.ExitCode -ne 0 -or -not $shot -or $shot.Length -lt 100000) {
        throw "Internal renderer capture failed for $mode (exited=$exited exit=$($process.ExitCode))."
    }
    $captures.Add([pscustomobject]@{mode=$mode;exit_code=$process.ExitCode;screenshot=$shot.FullName;bytes=$shot.Length})
}

$captures | ConvertTo-Json -Depth 3
