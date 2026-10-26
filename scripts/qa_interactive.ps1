$ErrorActionPreference = 'Stop'
$root = (Get-Location).Path
$exe = Join-Path $root 'build\bin\Release\game.exe'
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$qaLog = Join-Path $root 'logs\qa_interactive.log'
$sessionLog = Join-Path $root 'logs\breakwater-session.log'
$priorLog = if (Test-Path -LiteralPath $sessionLog) { (Get-Content -LiteralPath $sessionLog -Raw).Length } else { 0 }
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Drawing;
using System.Drawing.Imaging;
public static class BwQa {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X,Y; }
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd,int command);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd,uint msg,IntPtr wParam,IntPtr lParam);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code,uint mapType);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT point);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint flags,uint dx,uint dy,uint data,UIntPtr extra);
  static IntPtr KeyData(int vk,bool release) { uint scan=MapVirtualKey((uint)vk,0),flags=(vk>=0x25&&vk<=0x28)?(1u<<24):0u; return (IntPtr)(release?(0xC0000001u|(scan<<16)|flags):(1u|(scan<<16)|flags)); }
  public static void Down(IntPtr h,int vk) { SetForegroundWindow(h); PostMessage(h,0x0100,(IntPtr)vk,KeyData(vk,false)); }
  public static void Up(IntPtr h,int vk) { PostMessage(h,0x0101,(IntPtr)vk,KeyData(vk,true)); }
  public static void Tap(IntPtr h, int vk, int ms) { Down(h,vk); Thread.Sleep(ms); Up(h,vk); Thread.Sleep(100); }
  public static void Hold(IntPtr h, int vk, int ms) { Down(h,vk); Thread.Sleep(ms); Up(h,vk); Thread.Sleep(150); }
  public static void ClickClient(IntPtr h,int x,int y) { POINT p=new POINT();p.X=x;p.Y=y;if(!ClientToScreen(h,ref p))throw new Exception("ClientToScreen failed");if(!SetCursorPos(p.X,p.Y))throw new Exception("SetCursorPos failed");Thread.Sleep(80);mouse_event(2,0,0,0,UIntPtr.Zero);Thread.Sleep(100);mouse_event(4,0,0,0,UIntPtr.Zero);Thread.Sleep(180); }
  public static void Capture(IntPtr h, string path) { RECT r; if(!GetWindowRect(h,out r)) throw new Exception("GetWindowRect failed"); int w=r.Right-r.Left, ht=r.Bottom-r.Top; using(Bitmap b=new Bitmap(w,ht)) { using(Graphics g=Graphics.FromImage(b)) { g.CopyFromScreen(r.Left,r.Top,0,0,new Size(w,ht)); } b.Save(path,ImageFormat.Png); } }
}
"@
$p = Start-Process -FilePath $exe -WorkingDirectory $root -PassThru
try {
  $null = $p.WaitForInputIdle(10000)
  Start-Sleep -Milliseconds 1600
  $p.Refresh()
  if (!$p.MainWindowHandle) { throw 'Game window did not appear.' }
  $h = $p.MainWindowHandle
  [void][BwQa]::ShowWindow($h,9)
  [void][BwQa]::SetForegroundWindow($h)
  Start-Sleep -Milliseconds 350
  $missionStarted = $false
  for ($attempt=0; $attempt -lt 8; $attempt++) {
    [BwQa]::Tap($h,0x0D,160)
    Start-Sleep -Milliseconds 350
    $currentLog = [System.IO.File]::ReadAllText($sessionLog)
    if ($currentLog.Length -gt $priorLog -and $currentLog.Substring($priorLog) -match 'mission-start') { $missionStarted = $true; break }
    [void][BwQa]::ShowWindow($h,9)
    [void][BwQa]::SetForegroundWindow($h)
    Start-Sleep -Milliseconds 250
  }
  if (!$missionStarted) { throw 'Game did not receive the title-screen Enter input; no car QA was performed.' }
  Start-Sleep -Milliseconds 1200
  Start-Sleep -Milliseconds 1300
  [BwQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Live_$stamp`_Foot.png"))
  [BwQa]::Hold($h,0x44,800)
  [BwQa]::Tap($h,0x45,180)
  Start-Sleep -Milliseconds 500
  [BwQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Live_$stamp`_Car.png"))
  [BwQa]::Tap($h,0x43,150)
  Start-Sleep -Milliseconds 250
  [BwQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Live_$stamp`_CarCockpit.png"))
  [BwQa]::Hold($h,0x57,2200)
  [BwQa]::Hold($h,0x41,650)
  [BwQa]::Hold($h,0x57,900)
  [BwQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Live_$stamp`_CarDriving.png"))
  [BwQa]::Hold($h,0x20,1900)
  [BwQa]::Tap($h,0x45,180)
  Start-Sleep -Milliseconds 500
  [BwQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Live_$stamp`_AfterCar.png"))
  $allLog = Get-Content -LiteralPath $sessionLog -Raw
  if ($allLog.Length -le $priorLog) { throw 'Runtime session log did not grow during this QA run.' }
  $runText = $allLog.Substring($priorLog)
  if ($runText -notmatch 'mission-start') { throw 'This QA run did not log mission-start; its screenshots are not vehicle evidence.' }
  if ($runText -notmatch 'enter-car') { throw 'This QA run did not log enter-car; its screenshots are not vehicle evidence.' }
  "timestamp=$stamp`r`nprocess_id=$($p.Id)`r`ncar_controls=attempted`r`noutput_screenshots=Title,Foot,Car,CarCockpit,CarDriving,AfterCar`r`n" | Add-Content -LiteralPath $qaLog
  Write-Output "QA screenshots written with stamp $stamp"
} finally {
  if ($p -and !$p.HasExited) {
    [BwQa]::PostMessage($p.MainWindowHandle,0x0010,[IntPtr]::Zero,[IntPtr]::Zero) | Out-Null
    if (!$p.WaitForExit(5000)) { Stop-Process -Id $p.Id -Force }
  }
}
