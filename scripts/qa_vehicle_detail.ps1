$ErrorActionPreference='Stop';$root=(Get-Location).Path;$exe=Join-Path $root 'build\bin\Release\game.exe';$stamp=Get-Date -Format 'yyyyMMdd_HHmmss'
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;using System.Runtime.InteropServices;using System.Threading;using System.Drawing;using System.Drawing.Imaging;
public static class BwDetailQa {
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left,Top,Right,Bottom; }
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);[DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);[DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out RECT r);[DllImport("user32.dll")] public static extern uint MapVirtualKey(uint k,uint t);
 public static void Down(IntPtr h,int k){uint sc=MapVirtualKey((uint)k,0),ex=(k>=0x25&&k<=0x28)?(1u<<24):0u;PostMessage(h,0x0100,(IntPtr)k,(IntPtr)(1u|(sc<<16)|ex));}
 public static void Up(IntPtr h,int k){uint sc=MapVirtualKey((uint)k,0),ex=(k>=0x25&&k<=0x28)?(1u<<24):0u;PostMessage(h,0x0101,(IntPtr)k,(IntPtr)(0xC0000001u|(sc<<16)|ex));}
 public static void Tap(IntPtr h,int k,int ms){Down(h,k);Thread.Sleep(ms);Up(h,k);Thread.Sleep(180);}
 public static void Hold(IntPtr h,int k,int ms){Down(h,k);Thread.Sleep(ms);Up(h,k);Thread.Sleep(180);}
 public static void Chord(IntPtr h,int a,int b,int ms){Down(h,a);Down(h,b);Thread.Sleep(ms);Up(h,b);Up(h,a);Thread.Sleep(180);}
 public static void Capture(IntPtr h,string f){RECT r;GetWindowRect(h,out r);using(Bitmap b=new Bitmap(r.Right-r.Left,r.Bottom-r.Top)){using(Graphics g=Graphics.FromImage(b)){g.CopyFromScreen(r.Left,r.Top,0,0,new Size(b.Width,b.Height));}b.Save(f,ImageFormat.Png);}}
}
"@
$p=Start-Process -FilePath $exe -WorkingDirectory $root -PassThru
try{$null=$p.WaitForInputIdle(10000);Start-Sleep -Milliseconds 900;$p.Refresh();$h=$p.MainWindowHandle;if(!$h){throw 'No game window'};[void][BwDetailQa]::SetForegroundWindow($h);[BwDetailQa]::Tap($h,0x0D,150);Start-Sleep -Milliseconds 700
 [BwDetailQa]::Hold($h,0x44,450);[BwDetailQa]::Tap($h,0x45,150);Start-Sleep -Milliseconds 400;[BwDetailQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Detail_$stamp`_CarChase.png"))
 [BwDetailQa]::Tap($h,0x43,120);Start-Sleep -Milliseconds 250;[BwDetailQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Detail_$stamp`_CarCockpit.png"))
 [BwDetailQa]::Hold($h,0x57,900);[BwDetailQa]::Hold($h,0x20,1300);[BwDetailQa]::Tap($h,0x45,150);Start-Sleep -Milliseconds 350
 [BwDetailQa]::Hold($h,0x44,2600);[BwDetailQa]::Hold($h,0x57,2300);[BwDetailQa]::Tap($h,0x45,150);Start-Sleep -Milliseconds 300
 [BwDetailQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Detail_$stamp`_HeliChase.png"));[BwDetailQa]::Tap($h,0x43,120);Start-Sleep -Milliseconds 250;[BwDetailQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Detail_$stamp`_HeliCockpit.png"));[BwDetailQa]::Tap($h,0x43,120)
 [BwDetailQa]::Tap($h,0x45,150);Start-Sleep -Milliseconds 250;[BwDetailQa]::Chord($h,0x41,0x53,7600);[BwDetailQa]::Tap($h,0x45,150);Start-Sleep -Milliseconds 350
 [BwDetailQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Detail_$stamp`_PlaneChase.png"));[BwDetailQa]::Tap($h,0x43,120);Start-Sleep -Milliseconds 250;[BwDetailQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Detail_$stamp`_PlaneCockpit.png"))
 Add-Content -LiteralPath (Join-Path $root 'logs\qa_vehicle_detail.log') -Value "timestamp=$stamp`nexecutable=game.exe`nviews=car-chase,cockpit,heli-chase,cockpit,plane-chase,cockpit`n"
 Write-Output "Vehicle detail screenshots captured: $stamp"
}finally{if($p -and !$p.HasExited){[BwDetailQa]::PostMessage($p.MainWindowHandle,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null;if(!$p.WaitForExit(5000)){Stop-Process -Id $p.Id -Force}}}
