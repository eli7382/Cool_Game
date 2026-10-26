$ErrorActionPreference = 'Stop'
$root = (Get-Location).Path
$exe = Join-Path $root 'build\bin\Release\game.exe'
$sessionLog = Join-Path $root 'logs\breakwater-session.log'
$qaLog = Join-Path $root 'logs\qa_task_narration.log'
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$marker = "qa-task-tour-start-$stamp-voices-16-streams-5"
$events = @('narrator-west-task','narrator-east-task','narrator-south-task','narrator-beacon-task')
$priorCaptures = @{}
Get-ChildItem -LiteralPath (Join-Path $root 'screenshots') -Filter 'Breakwater_*.png' -File | ForEach-Object { $priorCaptures[$_.FullName] = $true }
$process = Start-Process -FilePath $exe -ArgumentList @('--qa-task-tour',"--qa-task-token=$stamp") -WorkingDirectory $root -PassThru
try {
    $null = $process.WaitForInputIdle(10000)
    $deadline = (Get-Date).AddSeconds(75)
    while ((Get-Date) -lt $deadline) {
        $process.Refresh()
        if ($process.HasExited) { break }
        Start-Sleep -Milliseconds 250
    }
    $process.Refresh()
    if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force; throw 'Mission interaction/narration tour exceeded 75 seconds.' }
    if ($process.ExitCode -ne 0) { throw "Mission interaction/narration tour exited with code $($process.ExitCode)." }

    $text = Get-Content -LiteralPath $sessionLog -Raw
    $tourStart = $text.LastIndexOf($marker,[System.StringComparison]::Ordinal)
    if ($tourStart -lt 0) { throw 'Mission tour start event was not logged.' }
    $sessionStart = $text.LastIndexOf('session-start',$tourStart,[System.StringComparison]::Ordinal)
    if ($sessionStart -lt 0) { throw 'Mission session start event was not logged.' }
    $run = $text.Substring($sessionStart)
    $required = @($marker,'narrator-west-task','narrator-east-task','narrator-south-task','narrator-beacon-task','west-gauge-secured','east-gauge-secured','south-gauge-secured','mission-complete','qa-task-tour-complete')
    foreach ($needle in $required) { if ($run -notmatch [regex]::Escape($needle)) { throw "Mission tour evidence missing: $needle" } }
    $summary = [regex]::Match($run,'summary,elapsed=[^\r\n]+')
    if (-not $summary.Success -or $summary.Value -notmatch 'gates=3') { throw "Mission summary did not record all three gates: $($summary.Value)" }
    $captureCount = [regex]::Matches($run,'screenshot-taken').Count
    $newCaptures = @(Get-ChildItem -LiteralPath (Join-Path $root 'screenshots') -Filter 'Breakwater_*.png' -File | Where-Object { -not $priorCaptures.ContainsKey($_.FullName) } | Sort-Object LastWriteTime)
    if ($captureCount -lt 4 -or $newCaptures.Count -lt 4) { throw "Expected four renderer captures, found $captureCount capture events and $($newCaptures.Count) new files." }
    $captureNames = ($newCaptures | Select-Object -Last 4 | ForEach-Object { $_.Name }) -join ','
    "timestamp=$stamp`nexecutable=$exe`nmode=scripted_in_game_mission_fixture`nvoice_assets=16_loaded`nobjective_cues=west,east,south,beacon`nnormal_interact_path=used_for_three_gauges_and_beacon`ngates=3`nmission_complete=true`nrenderer_capture_events=$captureCount`nscreenshots=$captureNames`nprocess_exit=$($process.ExitCode)`n" | Add-Content -LiteralPath $qaLog
    Write-Output "Mission QA passed: 3 gauge interactions, beacon completion, 4 narrator cues, 16/16 voices, $captureCount game-renderer captures."
} finally {
    $process.Refresh()
    if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force }
}
