$ErrorActionPreference = 'Stop'
$root = (Get-Location).Path
$exe = Join-Path $root 'build\bin\Release\game.exe'
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$qaLog = Join-Path $root 'logs\qa_plane_landing.log'
$sessionLog = Join-Path $root 'logs\breakwater-session.log'
$priorLog = if (Test-Path -LiteralPath $sessionLog) { [System.IO.File]::ReadAllText($sessionLog).Length } else { 0 }
$p = Start-Process -FilePath $exe -ArgumentList @('--qa-plane-landing','--qa-silent') -WorkingDirectory $root -PassThru
try {
  if (!$p.WaitForExit(15000)) { Stop-Process -Id $p.Id -Force; throw 'Landing fixture did not finish within 15 seconds.' }
  $allLog = [System.IO.File]::ReadAllText($sessionLog)
  $runText = $allLog.Substring($priorLog)
  if ($runText -match 'plane-hard-landing|plane-water-ditching') { throw 'The landing fixture reached a crash event; inspect its plane-state telemetry.' }
  $landingEvent = @($runText -split "`r?`n" | Where-Object { $_ -match ',plane-landing$' } | Select-Object -Last 1)
  if (!$landingEvent) { throw 'The final-approach physics did not retain a touchdown event.' }
  $muteEvent=@($runText -split "`r?`n" | Where-Object { $_ -match ',qa-audio-output-muted$' } | Select-Object -Last 1)
  if(!$muteEvent){throw 'The QA build did not confirm muted audio output.'}
  $result=@($runText -split "`r?`n" | Where-Object { $_ -match ',qa-plane-landing-result,' } | Select-Object -Last 1)
  if(!$result -or $result[0] -notmatch 'pass=1'){throw "The seeded aircraft did not complete an in-bounds runway touchdown: $($result[0])"}
  $contact = @($runText -split "`r?`n" | Where-Object { $_ -match 'qa-plane-landing-contact,' } | Select-Object -Last 1)
  if (!$contact) { throw 'No exact touchdown position was recorded.' }
  if ($contact[0] -notmatch 'runway=1') { throw "Touchdown fell outside the runway bounds: $($contact[0])" }
  $touchdown = @($runText -split "`r?`n" | Where-Object { $_ -match 'plane-landing-touchdown,' } | Select-Object -Last 1)
  if (!$touchdown) { throw 'Pre-contact sink-rate telemetry was not recorded.' }
  $sinkMatch=[regex]::Match($touchdown[0],'sink-mps=(-?[0-9.]+)')
  if(!$sinkMatch.Success){throw 'Touchdown sink-rate telemetry was malformed.'}
  $sinkRate=[double]::Parse($sinkMatch.Groups[1].Value,[System.Globalization.CultureInfo]::InvariantCulture)
  if($sinkRate -lt -5.2){throw ("Touchdown sink rate exceeded the game's hard-landing limit: {0:F2} m/s." -f $sinkRate)}
  $summary = @($runText -split "`r?`n" | Where-Object { $_ -match '^summary,' } | Select-Object -Last 1)
  [System.IO.File]::AppendAllText($qaLog,("timestamp=$stamp`r`nresult=runway-touchdown`r`naudio=muted-confirmed`r`nfixture=74mps-6m-agl-2mps-sink-runway-centerline`r`nlanding_event={0}`r`ntouchdown={1}`r`nposition={2}`r`nvalidation={3}`r`nsummary={4}`r`n" -f $landingEvent[0],$touchdown[0],$contact[0],$result[0],$summary[0]))
  Write-Output "Plane landing QA completed: $stamp"
} finally {
  if ($p -and !$p.HasExited) { Stop-Process -Id $p.Id -Force; $p.WaitForExit() }
}
