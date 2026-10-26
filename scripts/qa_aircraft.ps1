$ErrorActionPreference = 'Stop'
$root = (Get-Location).Path
$exe = Join-Path $root 'build\bin\Release\game.exe'
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$qaLog = Join-Path $root 'logs\qa_aircraft.log'
$sessionLog = Join-Path $root 'logs\breakwater-session.log'
$priorLog = if (Test-Path -LiteralPath $sessionLog) { (Get-Content -LiteralPath $sessionLog -Raw).Length } else { 0 }
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Drawing;
using System.Drawing.Imaging;
public static class BwAirQa {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd,int command);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code,uint mapType);
  [DllImport("user32.dll")] public static extern void keybd_event(byte key,byte scan,uint flags,UIntPtr extraInfo);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd,uint msg,IntPtr wParam,IntPtr lParam);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
  static IntPtr KeyData(int vk,bool release) { uint scan=MapVirtualKey((uint)vk,0), flags=(vk>=0x25&&vk<=0x28)?(1u<<24):0u; return (IntPtr)(release?(0xC0000001u|(scan<<16)|flags):(1u|(scan<<16)|flags)); }
  public static void Down(IntPtr h,int vk) { SetForegroundWindow(h); uint flags=(vk>=0x25&&vk<=0x28)?1u:0u; keybd_event((byte)vk,(byte)MapVirtualKey((uint)vk,0),flags,UIntPtr.Zero); PostMessage(h,0x0100,(IntPtr)vk,KeyData(vk,false)); }
  public static void Up(IntPtr h,int vk) { uint flags=2u|((vk>=0x25&&vk<=0x28)?1u:0u); keybd_event((byte)vk,(byte)MapVirtualKey((uint)vk,0),flags,UIntPtr.Zero); PostMessage(h,0x0101,(IntPtr)vk,KeyData(vk,true)); }
  public static void Tap(IntPtr h, int vk, int ms) { Down(h,vk); Thread.Sleep(ms); Up(h,vk); Thread.Sleep(120); }
  public static void Hold(IntPtr h, int vk, int ms) { Down(h,vk); Thread.Sleep(ms); Up(h,vk); Thread.Sleep(150); }
  public static void Chord(IntPtr h, int[] keys, int ms) { foreach(int k in keys) Down(h,k); Thread.Sleep(ms); for(int i=keys.Length-1;i>=0;--i) Up(h,keys[i]); Thread.Sleep(160); }
  public static void Capture(IntPtr h, string path) { RECT r; if(!GetWindowRect(h,out r)) throw new Exception("GetWindowRect failed"); int w=r.Right-r.Left, ht=r.Bottom-r.Top; using(Bitmap b=new Bitmap(w,ht)) { using(Graphics g=Graphics.FromImage(b)) { g.CopyFromScreen(r.Left,r.Top,0,0,new Size(w,ht)); } b.Save(path,ImageFormat.Png); } }
}
"@
$p = Start-Process -FilePath $exe -WorkingDirectory $root -PassThru
try {
  $null = $p.WaitForInputIdle(10000); Start-Sleep -Milliseconds 2400; $p.Refresh()
  if (!$p.MainWindowHandle) { throw 'Game window did not appear.' }
  $h = $p.MainWindowHandle; [void][BwAirQa]::ShowWindow($h,9); [void][BwAirQa]::SetForegroundWindow($h); Start-Sleep -Milliseconds 350
  $missionStarted = $false
  for ($attempt=0; $attempt -lt 8; $attempt++) {
    [BwAirQa]::Tap($h,0x0D,160); Start-Sleep -Milliseconds 350
    $currentLog = Get-Content -LiteralPath $sessionLog -Raw
    if ($currentLog.Length -gt $priorLog -and $currentLog.Substring($priorLog) -match 'mission-start') { $missionStarted = $true; break }
    [void][BwAirQa]::ShowWindow($h,9); [void][BwAirQa]::SetForegroundWindow($h); Start-Sleep -Milliseconds 250
  }
  if (-not $missionStarted) { throw 'Game did not receive the title-screen Enter input; no gameplay QA was performed.' }
  Start-Sleep -Milliseconds 350
  # Walk to the runway-head parking position with a measured on-foot route.
  [BwAirQa]::Hold($h,0x53,19400)
  [BwAirQa]::Hold($h,0x44,1100)
  [BwAirQa]::Tap($h,0x45,150); Start-Sleep -Milliseconds 300
  [BwAirQa]::Tap($h,0x43,120); Start-Sleep -Milliseconds 200
  [BwAirQa]::Tap($h,0x7B,120);Start-Sleep -Milliseconds 250
  # Build speed, rotate, then start a coordinated turn as soon as the wheels
  # leave the runway. The revised airfield start gives the aircraft room to arc.
  [BwAirQa]::Hold($h,0x57,8000)
  [BwAirQa]::Chord($h,@(0x57,0x26),2200)
  Start-Sleep -Milliseconds 1000
  [BwAirQa]::Tap($h,0x43,120); Start-Sleep -Milliseconds 200
  [BwAirQa]::Tap($h,0x7B,120)
  [BwAirQa]::Tap($h,0x7B,120)
  $landed = $false
  $approachStage = 0
  $lastTelemetryLine = $null
  $stalledTelemetrySamples = 0
  for ($step=0; $step -lt 240; $step++) {
    [BwAirQa]::Tap($h,0x79,80) # F10: read an in-game plane state without a screenshot.
    $p.Refresh(); if ($p.HasExited) { throw 'The game process exited during the aircraft flight route.' }
    Start-Sleep -Milliseconds 100
    $allLog = Get-Content -LiteralPath $sessionLog -Raw
    if ($allLog.Length -le $priorLog) { throw 'Runtime session log did not grow during this QA run.' }
    $runText = $allLog.Substring($priorLog)
    if ($runText -match 'plane-landing') { $landed = $true; break }
    if ($runText -match 'plane-hard-landing|plane-water-ditching') { throw 'The aircraft crashed before a controlled touchdown; inspect the latest plane-state telemetry.' }
    $stateLine = @($runText -split "`r?`n" | Where-Object { $_ -match ',plane-state,' } | Select-Object -Last 1)
    if (!$stateLine -or $stateLine[0] -notmatch ',plane-state,') {
      $recentEvents = @($runText -split "`r?`n" | Select-Object -Last 12) -join ' | '
      $failureShot = Join-Path $root ("screenshots\Breakwater_AircraftInputFailure_{0}.png" -f $stamp)
      try { [BwAirQa]::Capture($h,$failureShot) } catch { }
      [System.IO.File]::AppendAllText($qaLog,("input_failure_step={0} recent_events={1} screenshot={2}`r`n" -f $step,$recentEvents,$failureShot))
      throw 'F10 did not produce plane-state telemetry; the flight controller stopped.'
    }
    if ($lastTelemetryLine -and $lastTelemetryLine -eq $stateLine[0]) {
      $stalledTelemetrySamples++
      if ($stalledTelemetrySamples -ge 8) { throw 'Aircraft telemetry stopped changing before a landing event; the route is stalled.' }
      continue
    }
    $stalledTelemetrySamples=0
    $lastTelemetryLine=$stateLine[0]
    $telemetry = @{}
    foreach ($pair in $stateLine[0].Substring($stateLine[0].IndexOf('plane-state,')+12).Split(',')) {
      $parts = $pair.Split('=')
      $telemetry[$parts[0]] = [double]::Parse($parts[1],[System.Globalization.CultureInfo]::InvariantCulture)
    }
    [System.IO.File]::AppendAllText($qaLog,("flight_step={0} x={1:F1} z={2:F1} alt_m={3:F1} vs_mps={4:F1} speed_mps={5:F1} yaw_deg={6:F1} bank_deg={7:F1} normal_g={8:F2} longitudinal_g={9:F2} lateral_g={10:F2}`r`n" -f $step,$telemetry.x,$telemetry.z,$telemetry.alt,$telemetry.vs,$telemetry.mps,$telemetry.yaw,$telemetry.roll,$telemetry.ng,$telemetry.longg,$telemetry.latg))
    if ($approachStage -eq 2 -and $telemetry.z -lt 35.0 -and $telemetry.alt -gt 2.0) {
      throw 'The aircraft passed the runway end above touchdown height; the approach missed.'
    }
    if ($runText -notmatch 'plane-takeoff') {
      if ($telemetry.mps -lt 36.0) {
        if ($telemetry.pitch -lt 9.2) { [BwAirQa]::Chord($h,@(0x57,0x26),1250) }
        else { [BwAirQa]::Hold($h,0x57,1250) }
        continue
      }
      if ($telemetry.pitch -lt 9.2) { [BwAirQa]::Hold($h,0x26,220); continue }
      Start-Sleep -Milliseconds 250
      continue
    }
    if ($step -eq 0 -or $step % 20 -eq 0 -or $telemetry.alt -lt 8) { [BwAirQa]::Tap($h,0x7B,120) }
    # The south approach crosses a terrain ridge that rises above the runway.
    # Turn onto final from the north instead, where the runway approaches water.
    if ($approachStage -eq 0 -and $telemetry.z -gt 1600.0) {
      $approachStage = 1
      [System.IO.File]::AppendAllText($qaLog,"approach_stage=runway-alignment`r`n")
    }
    $targetSpeed = if ($approachStage -eq 2) { 40.0 } else { 45.0 }
    if ($telemetry.mps -gt ($targetSpeed + 1.2)) { [BwAirQa]::Hold($h,0x53,320); continue }
    if ($telemetry.mps -lt $targetSpeed-1.0) {
      $throttleMs=[Math]::Max(140,[Math]::Min(420,[int](140.0+($targetSpeed-$telemetry.mps)*52.0)))
      [BwAirQa]::Hold($h,0x57,$throttleMs)
    }
    # Fly north to the water approach, then aim for the runway center while
    # turning south. The look-ahead remains ahead of the aircraft on final.
    $turningHome = ($approachStage -ge 1)
    if ($turningHome) {
      # Steer directly from the aircraft's current cross-track position toward
      # the runway centerline; a projected intercept here under-corrected drift.
      $runwayDistance=[Math]::Max(90.0,$telemetry.z-190.0)
      $desiredYaw=[Math]::Atan2($telemetry.x,$runwayDistance)*180/[Math]::PI
    }
    else {
      $desiredYaw=180.0
    }
    $yawError=$desiredYaw-$telemetry.yaw
    while($yawError -gt 180){$yawError-=360};while($yawError -lt -180){$yawError+=360}
    if ($approachStage -eq 1 -and [Math]::Abs($yawError) -lt 25.0 -and $telemetry.z -gt 350.0) {
      $approachStage=2
      [System.IO.File]::AppendAllText($qaLog,"approach_stage=runway-final`r`n")
    }
    if ($approachStage -eq 2) {
      # Keep one runway-center aim point through the whole final turn so the
      # cross-track correction begins before the aircraft reaches the threshold.
      $headingAimZ=190.0
      $headingDistance=[Math]::Max(60.0,$telemetry.z-$headingAimZ)
      $desiredYaw=[Math]::Atan2($telemetry.x,$headingDistance)*180/[Math]::PI
      $yawError=$desiredYaw-$telemetry.yaw
      while($yawError -gt 180){$yawError-=360};while($yawError -lt -180){$yawError+=360}
    }
    $alignedApproach = ($approachStage -eq 2)
    $targetRoll=[Math]::Max(-.78,[Math]::Min(.78,$yawError*[Math]::PI/180*1.7))
    $currentRoll=$telemetry.roll*[Math]::PI/180
    $bankDeadband=if($approachStage -eq 2){.035}else{.10}
    $bankCorrectionMs=if($approachStage -eq 2){200}else{300}
    $bankKey=0
    if ($currentRoll -lt $targetRoll-$bankDeadband) { $bankKey=0x27 }
    elseif ($currentRoll -gt $targetRoll+$bankDeadband) { $bankKey=0x25 }
    if (!$alignedApproach) {
      # Keep the aircraft around a stable circuit height while climbing out and
      # while the turn is open, then descend during the long return leg so the
      # final glide does not begin above a physically reachable path.
      $targetAltitude=if($approachStage -eq 1){42.0}else{80.0}
      $minimumSink=if($approachStage -eq 1){-4.2}else{-2.5}
      $targetSink=[Math]::Max($minimumSink,[Math]::Min(4.2,($targetAltitude-$telemetry.alt)*.12))
    }
    else {
      # Begin descending as soon as the aircraft turns onto final. Track a
      # 3-degree path to the runway center with altitude-error feedback so a
      # late turn cannot leave the aircraft high across the threshold.
      $runwayDistance = [Math]::Max(35.0,$telemetry.z-190.0)
      $glideAltitude = 1.35 + $runwayDistance * 0.052
      $timeToRunway = [Math]::Max(3.0,$runwayDistance / [Math]::Max(30.0,$telemetry.mps))
      $altitudeError = $telemetry.alt - $glideAltitude
      $targetSink = -0.052 * $telemetry.mps - 0.72 * $altitudeError / $timeToRunway
      if ($runwayDistance -lt 120.0 -and $telemetry.alt -lt 8.0) { $targetSink = -0.8 }
      # The aircraft has no flap or trim control; hold a conservative descent
      # rate so pitch corrections do not drive it into a low-altitude dive.
      else { $targetSink = [Math]::Max(-4.4,[Math]::Min(2.0,$targetSink)) }
    }
    # Pitch is flight-path angle plus the speed-dependent angle of attack needed
    # to hold one-g lift. Apply the same trim from the first airborne sample so
    # low-speed climb corrections are not deferred until cruise speed.
    $speedForTrim=[Math]::Max(18.0,$telemetry.mps)
    # Match the game's q*S*CL=weight trim using standard-atmosphere density
    # and the current bank load; telemetry altitude is metres above terrain.
    $densityRatio=[Math]::Pow([Math]::Max(.01,1.0-.0065*[Math]::Max(0.0,$telemetry.alt)/288.15),4.25588)
    $dynamicPressure=.5*1.225*$densityRatio*$speedForTrim*$speedForTrim*[Math]::Max(.20,[Math]::Cos($currentRoll))
    $requiredLiftCoefficient=1200.0*9.80665/[Math]::Max(1.0,$dynamicPressure*15.0)
    $trimAoA=[Math]::Max(0.0,[Math]::Min(11.5,(($requiredLiftCoefficient-.32)/5.0)*180/[Math]::PI))
    $targetPitch=[Math]::Atan2($targetSink,[Math]::Max(26.0,$telemetry.mps))*180/[Math]::PI+$trimAoA
    $pitchError=$targetPitch-$telemetry.pitch
    $pitchKey=0
    $pitchDeadband=if($approachStage -eq 2){.35}else{1.0}
    if ($pitchError -gt $pitchDeadband) { $pitchKey=0x26 }
    elseif ($pitchError -lt -$pitchDeadband) { $pitchKey=0x28 }
    $pitchCorrectionMs=[Math]::Max(45,[Math]::Min(110,[int](48.0+[Math]::Abs($pitchError)*2.0)))
    [System.IO.File]::AppendAllText($qaLog,("controller_step={0} stage={1} alt={2:F1} sink={3:F2} pitch={4:F1} target_pitch={5:F1} pitch_key={6} yaw={7:F1} target_yaw={8:F1} roll={9:F1} target_roll={10:F1} bank_key={11}`r`n" -f $step,$approachStage,$telemetry.alt,$targetSink,$telemetry.pitch,$targetPitch,$pitchKey,$telemetry.yaw,$desiredYaw,$telemetry.roll,($targetRoll*180/[Math]::PI),$bankKey))
    if ($bankKey -and $pitchKey) {
      if($approachStage -eq 2){[BwAirQa]::Hold($h,$bankKey,$bankCorrectionMs);[BwAirQa]::Hold($h,$pitchKey,$pitchCorrectionMs);continue}
      [BwAirQa]::Chord($h,@($bankKey,$pitchKey),[Math]::Max(300,$pitchCorrectionMs));continue
    }
    if ($bankKey) { [BwAirQa]::Hold($h,$bankKey,$bankCorrectionMs);continue }
    if ($pitchKey) { [BwAirQa]::Hold($h,$pitchKey,$pitchCorrectionMs);continue }
    if ($telemetry.mps -gt 55) { [BwAirQa]::Hold($h,0x10,250); continue }
    Start-Sleep -Milliseconds 250
  }
  if (!$landed) { throw 'The aircraft did not reach a logged landing within the adaptive QA window.' }
  [BwAirQa]::PostMessage($p.MainWindowHandle,0x0010,[IntPtr]::Zero,[IntPtr]::Zero) | Out-Null
  if (!$p.WaitForExit(5000)) { Stop-Process -Id $p.Id -Force; $p.WaitForExit() }
  $allLog = Get-Content -LiteralPath $sessionLog -Raw;$runText = $allLog.Substring($priorLog)
  $summaryLine = @($runText -split "`r?`n" | Where-Object { $_ -match '^summary,' } | Select-Object -Last 1)
  $landingMatch = if ($summaryLine) { [regex]::Match($summaryLine[0],'plane_x=(-?[0-9.]+),plane_z=(-?[0-9.]+)') } else { $null }
  if (!$landingMatch -or !$landingMatch.Success) { throw 'Landing event lacked a final aircraft position in the session summary.' }
  $touchdownX = [double]::Parse($landingMatch.Groups[1].Value,[System.Globalization.CultureInfo]::InvariantCulture)
  $touchdownZ = [double]::Parse($landingMatch.Groups[2].Value,[System.Globalization.CultureInfo]::InvariantCulture)
  "touchdown_x=$('{0:F1}' -f $touchdownX) m`r`ntouchdown_z=$('{0:F1}' -f $touchdownZ) m`r`n" | Add-Content -LiteralPath $qaLog
  if ([Math]::Abs($touchdownX) -gt 14.5 -or $touchdownZ -lt 43.0 -or $touchdownZ -gt 337.0) {
    throw ("Aircraft touched down outside the runway at x={0:F1} m, z={1:F1} m." -f $touchdownX,$touchdownZ)
  }
  foreach ($event in @('mission-start','enter-plane','plane-takeoff','plane-landing')) {
    if ($runText -notmatch [regex]::Escape($event)) { throw "Current aircraft QA run did not log $event; screenshots are not evidence of that step." }
  }
  "timestamp=$stamp`r`nprocess_id=$($p.Id)`r`nplane=approach,enter,cockpit,power,rotation,coordinated-turn,feedback-descent,flare,runway-landing x=$touchdownX z=$touchdownZ`r`n" | Add-Content -LiteralPath $qaLog
  Write-Output "Plane QA sequence completed: $stamp"
} finally {
  if ($p -and !$p.HasExited) { [BwAirQa]::PostMessage($p.MainWindowHandle,0x0010,[IntPtr]::Zero,[IntPtr]::Zero) | Out-Null; if (!$p.WaitForExit(5000)) { Stop-Process -Id $p.Id -Force } }
}
