$ErrorActionPreference = 'Stop'
$root=(Get-Location).Path;$exe=Join-Path $root 'build\bin\Release\game.exe';$stamp=Get-Date -Format 'yyyyMMdd_HHmmss';$sessionLog=Join-Path $root 'logs\breakwater-session.log';$priorLog=if(Test-Path -LiteralPath $sessionLog){(Get-Content -LiteralPath $sessionLog -Raw).Length}else{0}
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;using System.Runtime.InteropServices;using System.Threading;using System.Drawing;using System.Drawing.Imaging;
public static class BwHeliQa {
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left,Top,Right,Bottom; }
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd,int command);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd,uint msg,IntPtr wParam,IntPtr lParam);[DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code,uint mapType);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd,out RECT rect);
 static IntPtr KeyData(int k,bool up){uint sc=MapVirtualKey((uint)k,0),ex=(k>=0x25&&k<=0x28)?(1u<<24):0u;return (IntPtr)(up?(0xC0000001u|(sc<<16)|ex):(1u|(sc<<16)|ex));}
 public static void Down(IntPtr h,int k){SetForegroundWindow(h);PostMessage(h,0x0100,(IntPtr)k,KeyData(k,false));} public static void Up(IntPtr h,int k){PostMessage(h,0x0101,(IntPtr)k,KeyData(k,true));}
 public static void Tap(IntPtr h,int k,int ms){Down(h,k);Thread.Sleep(ms);Up(h,k);Thread.Sleep(120);} public static void Hold(IntPtr h,int k,int ms){Down(h,k);Thread.Sleep(ms);Up(h,k);Thread.Sleep(150);}
 public static void Chord(IntPtr h,int[] ks,int ms){foreach(int k in ks)Down(h,k);Thread.Sleep(ms);for(int i=ks.Length-1;i>=0;--i)Up(h,ks[i]);Thread.Sleep(160);}
 public static void Capture(IntPtr h,string path){RECT r;if(!GetWindowRect(h,out r))throw new Exception("GetWindowRect failed");using(Bitmap b=new Bitmap(r.Right-r.Left,r.Bottom-r.Top)){using(Graphics g=Graphics.FromImage(b)){g.CopyFromScreen(r.Left,r.Top,0,0,new Size(b.Width,b.Height));}b.Save(path,ImageFormat.Png);}}
}
"@
$p=Start-Process -FilePath $exe -WorkingDirectory $root -PassThru
try{
 $null=$p.WaitForInputIdle(10000);Start-Sleep -Milliseconds 1600;$p.Refresh();if(!$p.MainWindowHandle){throw 'Game window did not appear.'};$h=$p.MainWindowHandle;[void][BwHeliQa]::ShowWindow($h,9);$foregroundAccepted=[BwHeliQa]::SetForegroundWindow($h);Start-Sleep -Milliseconds 350;Write-Output "window=$h foreground=$([BwHeliQa]::GetForegroundWindow()) activation=$foregroundAccepted"
 $missionStarted=$false
 for($attempt=0;$attempt -lt 8;$attempt++){[BwHeliQa]::Tap($h,0x0D,160);Start-Sleep -Milliseconds 450;$p.Refresh();if($p.HasExited){break};$allLog=Get-Content -LiteralPath $sessionLog -Raw;if($allLog.Length -gt $priorLog -and $allLog.Substring($priorLog) -match 'mission-start'){$missionStarted=$true;break};[void][BwHeliQa]::ShowWindow($h,9);[void][BwHeliQa]::SetForegroundWindow($h);Start-Sleep -Milliseconds 250}
 if(!$missionStarted){throw 'Game did not receive the title-screen Enter input; no helicopter QA was performed.'}
 [BwHeliQa]::Chord($h,@(0x44,0x57),5000);[BwHeliQa]::Hold($h,0x44,1500)
 [BwHeliQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Heli_$stamp`_Approach.png"));[BwHeliQa]::Tap($h,0x45,150);Start-Sleep -Milliseconds 300
 [BwHeliQa]::Tap($h,0x43,120);Start-Sleep -Milliseconds 200;[BwHeliQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Heli_$stamp`_Cockpit.png"))
 # Let the engine governor and main rotor spool at idle. Raise collective to a
 # hover climb, then trim back near hover instead of making a full-power jump.
 Start-Sleep -Milliseconds 3000;[BwHeliQa]::Hold($h,0x20,1300);[BwHeliQa]::Hold($h,0x11,200);Start-Sleep -Milliseconds 1800
 [BwHeliQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Heli_$stamp`_Hover.png"))
 [BwHeliQa]::Chord($h,@(0x57,0x44),1800);[BwHeliQa]::Hold($h,0x51,700)
 [BwHeliQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Heli_$stamp`_Translation.png"))
 # Ease collective down slightly and allow several seconds for a controlled
 # descent at a shallow sink rate.
 [BwHeliQa]::Hold($h,0x11,200);Start-Sleep -Milliseconds 12500
 [BwHeliQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Heli_$stamp`_Landing.png"))
 $allLog=Get-Content -LiteralPath $sessionLog -Raw;if($allLog.Length -le $priorLog){throw 'Runtime session log did not grow during this QA run.'};$runText=$allLog.Substring($priorLog)
 foreach($event in @('mission-start','enter-helicopter','helicopter-takeoff','helicopter-landing')){if($runText -notmatch [regex]::Escape($event)){throw "Current helicopter QA run did not log $event; screenshots are not evidence of that step."}}
 "timestamp=$stamp`r`nprocess_id=$($p.Id)`r`nhelicopter=approach,enter,cockpit,collective,hover,translation,yaw,landing-attempt`r`n" | Add-Content -LiteralPath (Join-Path $root 'logs\qa_helicopter.log')
 Write-Output "Helicopter QA sequence completed: $stamp"
}finally{if($p -and !$p.HasExited){[BwHeliQa]::PostMessage($p.MainWindowHandle,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null;if(!$p.WaitForExit(5000)){Stop-Process -Id $p.Id -Force}}}
