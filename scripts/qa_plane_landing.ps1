$ErrorActionPreference = 'Stop'
$root = (Get-Location).Path
$exe = Join-Path $root 'build\bin\Release\game.exe'
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$qaLog = Join-Path $root 'logs\qa_plane_landing.log'
$sessionLog = Join-Path $root 'logs\breakwater-session.log'
$priorLog = if (Test-Path -LiteralPath $sessionLog) { [System.IO.File]::ReadAllText($sessionLog).Length } else { 0 }
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Threading;
public static class BwLandingQa2 {
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
 [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint k,uint t);
 static IntPtr KeyData(int k,bool up){uint scan=MapVirtualKey((uint)k,0);return (IntPtr)(up?(0xC0000001u|(scan<<16)):(1u|(scan<<16)));}
 public static void Hold(IntPtr h,int k,int ms){PostMessage(h,0x0100,(IntPtr)k,KeyData(k,false));Thread.Sleep(ms);PostMessage(h,0x0101,(IntPtr)k,KeyData(k,true));}
}
"@
$p = Start-Process -FilePath $exe -ArgumentList '--qa-plane-landing' -WorkingDirectory $root -PassThru
try {
  $null = $p.WaitForInputIdle(10000)
  Start-Sleep -Milliseconds 500
  $p.Refresh()
  if (!$p.MainWindowHandle) { throw 'Landing fixture did not open a game window.' }
  $h = $p.MainWindowHandle
  $deadline = [DateTime]::UtcNow.AddSeconds(10)
  $runText = ''
  $landingEvent = ''
  while ([DateTime]::UtcNow -lt $deadline) {
    $p.Refresh()
    if ($p.HasExited) { break }
    $allLog = [System.IO.File]::ReadAllText($sessionLog)
    if ($allLog.Length -gt $priorLog) { $runText = $allLog.Substring($priorLog) }
    if ($runText -match 'qa-plane-landing-fixture') {
      $landingEvent = @($runText -split "`r?`n" | Where-Object { $_ -match ',plane-landing$' } | Select-Object -Last 1)
      if ($landingEvent) { break }
      if ($runText -match 'plane-hard-landing|plane-water-ditching') { throw 'The final-approach fixture crashed before touchdown.' }
    }
    Start-Sleep -Milliseconds 200
  }
  if ($runText -notmatch 'qa-plane-landing-fixture') { throw 'The final-approach setup did not run.' }
  if (!$landingEvent) { throw 'No touchdown was observed within 10 seconds; inspect plane-state telemetry.' }
  [BwLandingQa2]::Hold($h,0xA0,1250)
  [void][BwLandingQa2]::PostMessage($h,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)
  if (!$p.WaitForExit(5000)) { Stop-Process -Id $p.Id -Force }
  $allLog = [System.IO.File]::ReadAllText($sessionLog)
  $runText = $allLog.Substring($priorLog)
  if ($runText -match 'plane-hard-landing|plane-water-ditching') { throw 'The landing fixture reached a crash event; inspect its plane-state telemetry.' }
  $landingEvent = @($runText -split "`r?`n" | Where-Object { $_ -match ',plane-landing$' } | Select-Object -Last 1)
  if (!$landingEvent) { throw 'The final-approach physics did not retain a touchdown event.' }
  $summary = @($runText -split "`r?`n" | Where-Object { $_ -match '^summary,' } | Select-Object -Last 1)
  [System.IO.File]::AppendAllText($qaLog,("timestamp=$stamp`r`nresult=physics-touchdown`r`nfixture=8.2m-final-approach-on-runway-centerline`r`nlanding_event={0}`r`npilot_inputs=left-shift-brake-for-1250ms-after-touchdown`r`nsummary={1}`r`n" -f $landingEvent[0],$summary[0]))
  Write-Output "Plane landing QA completed: $stamp"
} finally {
  if ($p -and !$p.HasExited) {
    [void][BwLandingQa2]::PostMessage($p.MainWindowHandle,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)
    if (!$p.WaitForExit(5000)) { Stop-Process -Id $p.Id -Force }
  }
}
