param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('forest','forest-close','tree-detail','coastal-water','coastal-water-wake')]
    [string]$Mode,
    [ValidateSet('Low','High')]
    [string]$Preset = 'High',
    [switch]$MidTide
)

$ErrorActionPreference = 'Stop'
# The game writes its own framebuffer capture; no desktop-window screenshot is needed here.

$workspace = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$exe = (Resolve-Path (Join-Path $workspace 'build\bin\Release\game.exe')).Path
$output = Join-Path $workspace ("screenshots\Breakwater_LiveQA_{0}.png" -f $Mode)
$startedUtcMs = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$arguments = @(("--qa-capture={0}" -f $Mode))
if ($Preset -eq 'Low') { $arguments += '--qa-capture-low' }
if ($MidTide) {
    if ($Mode -ne 'coastal-water') { throw '-MidTide is valid only with -Mode coastal-water.' }
    $arguments += '--qa-mid-tide'
}
$arguments += '--qa-silent'
$process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $workspace -PassThru
$exited = $process.WaitForExit(20000)
$process.Refresh()
$renderShot = Get-ChildItem -LiteralPath (Join-Path $workspace 'screenshots') -Filter ("Breakwater_LiveQA_{0}_*.png" -f $Mode) |
    Where-Object { $_.BaseName -match '_(\d+)$' -and [long]$matches[1] -ge $startedUtcMs } |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
$captured = $false
if ($renderShot) {
    Copy-Item -LiteralPath $renderShot.FullName -Destination $output -Force
    $captured = (Get-Item -LiteralPath $output).Length -gt 100000
}
[pscustomobject]@{
    mode = $Mode
    preset = $Preset
    process_id = $process.Id
    captured = $captured
    exited = $exited
    exit_code = if ($exited) { $process.ExitCode } else { $null }
    screenshot = $output
    renderer_screenshot = if ($renderShot) { $renderShot.FullName } else { $null }
} | ConvertTo-Json -Compress
if (-not $captured -or -not $exited -or $process.ExitCode -ne 0) { throw "Capture QA did not complete for $Mode." }
