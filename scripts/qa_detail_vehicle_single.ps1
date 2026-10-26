param([ValidateSet('Car','Plane','Helicopter')][string]$Vehicle,[string]$Executable='game.exe')
$ErrorActionPreference='Stop';$root=(Get-Location).Path;$exe=Join-Path $root "build\bin\Release\$Executable";$stamp=Get-Date -Format 'yyyyMMdd_HHmmss';$kind=$Vehicle.ToLowerInvariant()
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;using System.Runtime.InteropServices;using System.Threading;using System.Drawing;using System.Drawing.Imaging;
public static class BwSingleQa {
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left,Top,Right,Bottom; }
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);[DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();[DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int n);[DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);[DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out RECT r);[DllImport("user32.dll")] public static extern uint MapVirtualKey(uint k,uint t);[DllImport("user32.dll")] public static extern void keybd_event(byte key,byte scan,uint flags,UIntPtr extra);
 public static void Down(IntPtr h,int k){SetForegroundWindow(h);keybd_event((byte)k,(byte)MapVirtualKey((uint)k,0),0,UIntPtr.Zero);}
 public static void Up(IntPtr h,int k){keybd_event((byte)k,(byte)MapVirtualKey((uint)k,0),2,UIntPtr.Zero);}
 public static void Tap(IntPtr h,int k,int ms){Down(h,k);Thread.Sleep(ms);Up(h,k);Thread.Sleep(180);}
 public static void Hold(IntPtr h,int k,int ms){Down(h,k);Thread.Sleep(ms);Up(h,k);Thread.Sleep(180);}
 public static void Chord(IntPtr h,int a,int b,int ms){Down(h,a);Down(h,b);Thread.Sleep(ms);Up(h,b);Up(h,a);Thread.Sleep(180);}
 public static void Chord3(IntPtr h,int a,int b,int c,int ms){Down(h,a);Down(h,b);Down(h,c);Thread.Sleep(ms);Up(h,c);Up(h,b);Up(h,a);Thread.Sleep(180);}
 public static void Capture(IntPtr h,string f){RECT r;GetWindowRect(h,out r);using(Bitmap b=new Bitmap(r.Right-r.Left,r.Bottom-r.Top)){using(Graphics g=Graphics.FromImage(b)){g.CopyFromScreen(r.Left,r.Top,0,0,new Size(b.Width,b.Height));}b.Save(f,ImageFormat.Png);}}
}
"@
$p=Start-Process -FilePath $exe -WorkingDirectory $root -PassThru
try{$null=$p.WaitForInputIdle(10000);Start-Sleep -Milliseconds 1700;$p.Refresh();$h=$p.MainWindowHandle;if(!$h){throw 'No game window'};[void][BwSingleQa]::ShowWindow($h,9);$foregroundAccepted=[BwSingleQa]::SetForegroundWindow($h);Start-Sleep -Milliseconds 300;Write-Output "window=$h foreground=$([BwSingleQa]::GetForegroundWindow()) activation=$foregroundAccepted";[BwSingleQa]::Tap($h,0x0D,180);Start-Sleep -Milliseconds 1400
 if($Vehicle -eq 'Car'){[BwSingleQa]::Hold($h,0x44,450)}
 if($Vehicle -eq 'Helicopter'){[BwSingleQa]::Hold($h,0x44,5400);[BwSingleQa]::Hold($h,0x57,1800)}
 if($Vehicle -eq 'Plane'){[BwSingleQa]::Hold($h,0x53,5900);[BwSingleQa]::Hold($h,0x41,1450)}
 Start-Sleep -Milliseconds 500;[BwSingleQa]::Tap($h,0x45,150);Start-Sleep -Milliseconds 500
 [BwSingleQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_ModelQA_$stamp`_${kind}_chase.png"))
 [BwSingleQa]::Tap($h,0x43,120);Start-Sleep -Milliseconds 350;[BwSingleQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_ModelQA_$stamp`_${kind}_cockpit.png"))
 Add-Content -LiteralPath (Join-Path $root 'logs\qa_vehicle_model_views.log') -Value "timestamp=$stamp`nvehicle=$Vehicle`nexecutable=$Executable`nviews=chase,cockpit`n"
 Write-Output "Captured $Vehicle model views: $stamp"
}finally{if($p -and !$p.HasExited){[BwSingleQa]::PostMessage($p.MainWindowHandle,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null;if(!$p.WaitForExit(5000)){Stop-Process -Id $p.Id -Force}}}

