param([ValidateSet('entry-stairs','stair-descent')][string]$Mode = 'entry-stairs')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$exe = Join-Path $root 'build\bin\Release\game.exe'
$sessionLog = Join-Path $root 'logs\breakwater-session.log'
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$priorLength = if (Test-Path -LiteralPath $sessionLog) { (Get-Content -LiteralPath $sessionLog -Raw).Length } else { 0 }
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Threading;
public static class BwSafehouseQa {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd,int command);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd,uint msg,IntPtr wParam,IntPtr lParam);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code,uint mapType);
  static IntPtr KeyData(int vk,bool release) { uint scan=MapVirtualKey((uint)vk,0);return (IntPtr)(release?(0xC0000001u|(scan<<16)):(1u|(scan<<16))); }
  public static void Down(IntPtr h,int vk) { PostMessage(h,0x0100,(IntPtr)vk,KeyData(vk,false)); }
  public static void Up(IntPtr h,int vk) { PostMessage(h,0x0101,(IntPtr)vk,KeyData(vk,true)); }
  public static void Tap(IntPtr h,int vk,int ms) { Down(h,vk);Thread.Sleep(ms);Up(h,vk);Thread.Sleep(100); }
  public static void Pulse(IntPtr h,int vk,int ms) { Down(h,vk);Thread.Sleep(ms);Up(h,vk); }
  public static void Chord(IntPtr h,int a,int b,int ms) { Down(h,a);Down(h,b);Thread.Sleep(ms);Up(h,b);Up(h,a); }
}
"@
$gameArgument = if ($Mode -eq 'stair-descent') { '--qa-safehouse-descent' } else { '--qa-safehouse' }
$process = Start-Process -FilePath $exe -ArgumentList @($gameArgument,'--qa-silent') -WorkingDirectory $root -PassThru
$runText = ''
try {
    $null = $process.WaitForInputIdle(10000)
    Start-Sleep -Milliseconds 700
    $process.Refresh()
    if (!$process.MainWindowHandle) { throw 'Game window did not appear.' }
    $h = $process.MainWindowHandle
    [void][BwSafehouseQa]::ShowWindow($h,9);[void][BwSafehouseQa]::SetForegroundWindow($h)
    $runText = ''
    for ($wait = 0; $wait -lt 60; $wait++) {
        Start-Sleep -Milliseconds 250
        $allText = [System.IO.File]::ReadAllText($sessionLog)
        $runText = if ($allText.Length -ge $priorLength) { $allText.Substring($priorLength) } else { $allText }
        if ($runText -match 'qa-safehouse-start') { break }
        $process.Refresh()
        if ($process.HasExited) { throw "Game exited before the safehouse fixture started (exit $($process.ExitCode))." }
    }
    if ($runText -notmatch 'qa-safehouse-start') { throw 'Seeded safehouse gameplay fixture did not start within 15 seconds.' }
    [void][BwSafehouseQa]::ShowWindow($h,9);[void][BwSafehouseQa]::SetForegroundWindow($h)
    Start-Sleep -Milliseconds 400
    if ($Mode -eq 'stair-descent') {
        [BwSafehouseQa]::Pulse($h,0x57,1700)
    } else {
        [BwSafehouseQa]::Tap($h,0x45,150)
        Start-Sleep -Milliseconds 750
        [BwSafehouseQa]::Chord($h,0x57,0x10,1050)
        Start-Sleep -Milliseconds 1200
        # Counter-steer on the flat interior to center on the stair lane, then
        # use short acceleration/braking pulses to approach and climb the flight.
        [BwSafehouseQa]::Pulse($h,0x41,780);[BwSafehouseQa]::Pulse($h,0x44,780)
        [BwSafehouseQa]::Pulse($h,0x57,640);[BwSafehouseQa]::Pulse($h,0x53,640)
        [BwSafehouseQa]::Pulse($h,0x57,600);[BwSafehouseQa]::Pulse($h,0x53,600)
        [BwSafehouseQa]::Pulse($h,0x53,825);[BwSafehouseQa]::Pulse($h,0x57,825)
        Start-Sleep -Milliseconds 400
        [BwSafehouseQa]::Pulse($h,0x57,825);[BwSafehouseQa]::Pulse($h,0x53,825)
        [BwSafehouseQa]::Pulse($h,0x57,250);[BwSafehouseQa]::Pulse($h,0x53,250)
    }
    Start-Sleep -Milliseconds 350
} finally {
    if ($process -and !$process.HasExited) {
        [BwSafehouseQa]::PostMessage($process.MainWindowHandle,0x0010,[IntPtr]::Zero,[IntPtr]::Zero) | Out-Null
        if (!$process.WaitForExit(5000)) { Stop-Process -Id $process.Id -Force }
    }
}
Start-Sleep -Milliseconds 250
$allText = [System.IO.File]::ReadAllText($sessionLog)
$runText = if ($allText.Length -ge $priorLength) { $allText.Substring($priorLength) } else { $allText }
$events = if ($Mode -eq 'stair-descent') { @('safehouse-ground-floor-reached') } else { @('safehouse-door-open','safehouse-entered','safehouse-upper-floor-reached') }
$missing = @($events | Where-Object { $runText -notmatch [regex]::Escape($_) })
$state = ($runText -split "`r?`n" | Where-Object { $_ -match ',qa-safehouse-end,' } | Select-Object -Last 1)
$report = [pscustomobject]@{
    stamp = $stamp
    process_id = $process.Id
    fixture = if ($Mode -eq 'stair-descent') { 'seeded upper stair landing; actual in-game input to descend to the ground floor' } else { 'seeded safehouse entry; actual in-game keyboard inputs for door entry and stair ascent' }
    expected_events = $events
    observed_events = @($events | Where-Object { $runText -match [regex]::Escape($_) })
    missing_events = $missing
    final_state = $state
    screenshots = @()
}
$reportPath = Join-Path $root ("logs\qa_safehouse_{0}.json" -f $stamp)
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $reportPath -Encoding UTF8
$report | ConvertTo-Json -Depth 5
if ($missing.Count) { throw "Safehouse gameplay QA missed events: $($missing -join ', ')" }
