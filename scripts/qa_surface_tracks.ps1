$ErrorActionPreference = 'Stop'
$root = (Get-Location).Path
$exe = Join-Path $root 'build\bin\Release\game.exe'
$sessionLog = Join-Path $root 'logs\breakwater-session.log'
$priorLog = if (Test-Path -LiteralPath $sessionLog) { (Get-Content -LiteralPath $sessionLog -Raw).Length } else { 0 }
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Threading;
public static class BreakwaterTrackQa {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr wp, IntPtr lp);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint mapType);
  static IntPtr Data(int vk, bool up) { uint scan=MapVirtualKey((uint)vk,0),flags=(vk>=0x25&&vk<=0x28)?(1u<<24):0u;return (IntPtr)(up?(0xC0000001u|(scan<<16)|flags):(1u|(scan<<16)|flags)); }
  public static void Down(IntPtr h,int vk) { SetForegroundWindow(h);BringWindowToTop(h);PostMessage(h,0x0100,(IntPtr)vk,Data(vk,false)); }
  public static void Up(IntPtr h,int vk) { PostMessage(h,0x0101,(IntPtr)vk,Data(vk,true)); }
  public static void Tap(IntPtr h,int vk,int ms) { Down(h,vk);Thread.Sleep(ms);Up(h,vk);Thread.Sleep(100); }
  public static void Hold(IntPtr h,int vk,int ms) { Down(h,vk);Thread.Sleep(ms);Up(h,vk);Thread.Sleep(150); }
}
"@

$started = Get-Date
$before = @(Get-ChildItem -LiteralPath (Join-Path $root 'screenshots') -Filter 'Breakwater_*.png' | ForEach-Object FullName)
$p = Start-Process -FilePath $exe -WorkingDirectory $root -PassThru
try {
  $null = $p.WaitForInputIdle(10000)
  Start-Sleep -Milliseconds 1600
  for ($i=0; $i -lt 60; $i++) {
    $p.Refresh()
    if ($p.MainWindowHandle) { break }
    Start-Sleep -Milliseconds 100
  }
  if (!$p.MainWindowHandle) { throw 'Game window did not appear.' }
  $h = $p.MainWindowHandle
  [void][BreakwaterTrackQa]::ShowWindow($h,9)
  [void][BreakwaterTrackQa]::SetForegroundWindow($h)
  Start-Sleep -Milliseconds 350
  [BreakwaterTrackQa]::Tap($h,0x0D,160)
  for ($i=0; $i -lt 8; $i++) {
    Start-Sleep -Milliseconds 350
    $text = [System.IO.File]::ReadAllText($sessionLog)
    if ($text.Length -gt $priorLog -and $text.Substring($priorLog) -match 'mission-start') { break }
    [void][BreakwaterTrackQa]::ShowWindow($h,9)
    [void][BreakwaterTrackQa]::SetForegroundWindow($h)
    [BreakwaterTrackQa]::Tap($h,0x0D,140)
  }
  $text = [System.IO.File]::ReadAllText($sessionLog)
  if ($text.Length -le $priorLog -or $text.Substring($priorLog) -notmatch 'mission-start') { throw 'Mission start input was not recorded.' }
  [BreakwaterTrackQa]::Hold($h,0x44,350)
  [BreakwaterTrackQa]::Tap($h,0x45,160)
  Start-Sleep -Milliseconds 600
  [BreakwaterTrackQa]::Hold($h,0x57,2200)
  [BreakwaterTrackQa]::Hold($h,0x41,650)
  [BreakwaterTrackQa]::Hold($h,0x57,900)
  [BreakwaterTrackQa]::Tap($h,0x7B,120)
  Start-Sleep -Milliseconds 700
  [BreakwaterTrackQa]::Hold($h,0x20,1900)
  [BreakwaterTrackQa]::Tap($h,0x45,160)
  Start-Sleep -Milliseconds 350
  [BreakwaterTrackQa]::Hold($h,0x57,2800)
  [BreakwaterTrackQa]::Tap($h,0x7B,120)
  Start-Sleep -Milliseconds 700
  [BreakwaterTrackQa]::PostMessage($h,0x0010,[IntPtr]::Zero,[IntPtr]::Zero) | Out-Null
  if (!$p.WaitForExit(5000)) { Stop-Process -Id $p.Id -Force }
  $p.Refresh()
  $text = [System.IO.File]::ReadAllText($sessionLog)
  $run = if ($text.Length -gt $priorLog) { $text.Substring($priorLog) } else { '' }
  if ($run -notmatch 'enter-car') { throw 'The game did not record car entry; tire-mark interaction was not exercised.' }
  if ($run -notmatch 'exit-car') { throw 'The game did not record car exit; on-foot mark interaction was not exercised.' }
  $summary = [regex]::Match($run,'summary,[^\r\n]+')
  if (!$summary.Success -or $summary.Value -notmatch 'footprint_marks=([1-9][0-9]*)' -or $summary.Value -notmatch 'tire_track_segments=([1-9][0-9]*)') { throw 'The live run did not report both generated footprint and tire-strip marks.' }
  $after = @(Get-ChildItem -LiteralPath (Join-Path $root 'screenshots') -Filter 'Breakwater_*.png' | Where-Object { $_.LastWriteTime -ge $started -and $before -notcontains $_.FullName } | Sort-Object LastWriteTime | Select-Object -ExpandProperty FullName)
  if ($after.Count -lt 2) { throw "Expected at least two game-renderer screenshots, found $($after.Count)." }
  [pscustomobject]@{ stamp=$stamp; process_id=$p.Id; exit_code=$p.ExitCode; mission_started=$true; entered_car=($run -match 'enter-car'); exited_car=($run -match 'exit-car'); surface_mark_summary=$summary.Value; screenshots=$after; note='Live window inputs generated wheel movement and on-foot movement; the final session telemetry counts the two surface-mark classes; screenshots are from the in-game F12 renderer path.' } | ConvertTo-Json -Depth 4
} finally {
  if ($p -and !$p.HasExited) {
    [BreakwaterTrackQa]::PostMessage($p.MainWindowHandle,0x0010,[IntPtr]::Zero,[IntPtr]::Zero) | Out-Null
    if (!$p.WaitForExit(5000)) { Stop-Process -Id $p.Id -Force }
  }
}
