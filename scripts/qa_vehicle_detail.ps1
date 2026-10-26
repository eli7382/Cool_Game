$ErrorActionPreference = 'Stop'
$root = (Get-Location).Path
$exe = Join-Path $root 'build\bin\Release\game.exe'
$screenshotDir = Join-Path $root 'screenshots'
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$cases = @(
    [pscustomobject]@{ Mode = 'car'; Name = 'CarChase' },
    [pscustomobject]@{ Mode = 'car-cockpit'; Name = 'CarCockpit' },
    [pscustomobject]@{ Mode = 'heli'; Name = 'HeliChase' },
    [pscustomobject]@{ Mode = 'heli-cockpit'; Name = 'HeliCockpit' },
    [pscustomobject]@{ Mode = 'plane'; Name = 'PlaneChase' },
    [pscustomobject]@{ Mode = 'plane-cockpit'; Name = 'PlaneCockpit' }
)
$captured = @()
foreach ($case in $cases) {
    $pattern = "Breakwater_LiveQA_$($case.Mode)_*.png"
    $before = [System.Collections.Generic.HashSet[string]]::new()
    Get-ChildItem -LiteralPath $screenshotDir -Filter $pattern -File | ForEach-Object { [void]$before.Add($_.FullName) }
    $process = Start-Process -FilePath $exe -ArgumentList @("--qa-capture=$($case.Mode)") -WorkingDirectory $root -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Capture process for $($case.Mode) exited $($process.ExitCode)." }
    $deadline = (Get-Date).AddSeconds(4)
    $candidate = $null
    while ((Get-Date) -lt $deadline -and !$candidate) {
        $candidate = Get-ChildItem -LiteralPath $screenshotDir -Filter $pattern -File |
            Where-Object { !$before.Contains($_.FullName) } |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if (!$candidate) { Start-Sleep -Milliseconds 80 }
    }
    if (!$candidate -or $candidate.Length -lt 1024) { throw "No valid internal screenshot was produced for $($case.Mode)." }
    $destination = Join-Path $screenshotDir "Breakwater_Detail_$($stamp)_$($case.Name).png"
    Move-Item -LiteralPath $candidate.FullName -Destination $destination
    $captured += "$($case.Name)=$destination ($((Get-Item -LiteralPath $destination).Length) bytes)"
}
$record = @(
    "timestamp=$stamp",
    "executable=build/bin/Release/game.exe",
    "capture_source=game_internal_renderer",
    "controls=static in-game QA camera presets; no vehicle-operation claim",
    "views=car-chase,car-cockpit,heli-chase,heli-cockpit,plane-chase,plane-cockpit"
) + $captured
Add-Content -LiteralPath (Join-Path $root 'logs\qa_vehicle_detail.log') -Value ($record -join [Environment]::NewLine)
$record
