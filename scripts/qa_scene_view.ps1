param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('car','car-front','car-cockpit','car-impact','plane','plane-front','plane-cockpit','plane-impact','heli','heli-front','heli-cockpit','heli-impact','house-exterior','house-interior','house-upstairs')]
    [string]$Mode,
    [ValidateSet('Low','High')]
    [string]$Preset = 'High'
)

$ErrorActionPreference = 'Stop'
$workspace = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$exe = (Resolve-Path (Join-Path $workspace 'build\bin\Release\game.exe')).Path
$started = Get-Date
$output = Join-Path $workspace ("screenshots\Breakwater_LiveQA_{0}.png" -f $Mode)
$arguments = @(("--qa-capture={0}" -f $Mode))
if ($Preset -eq 'Low') { $arguments += '--qa-capture-low' }
$process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $workspace -PassThru
$exited = $process.WaitForExit(20000)
$process.Refresh()
$renderShot = Get-ChildItem -LiteralPath (Join-Path $workspace 'screenshots') -Filter ("Breakwater_LiveQA_{0}_*.png" -f $Mode) |
    Where-Object { $_.LastWriteTime -ge $started } |
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
if (-not $captured -or -not $exited -or $process.ExitCode -ne 0) { throw "Capture QA did not complete for $Mode ($Preset)." }
