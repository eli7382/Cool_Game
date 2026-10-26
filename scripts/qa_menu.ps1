$ErrorActionPreference='Stop';$root=(Get-Location).Path;$exe=Join-Path $root 'build\bin\Release\game.exe';$stamp=Get-Date -Format 'yyyyMMdd_HHmmss'
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;using System.Runtime.InteropServices;using System.Threading;using System.Drawing;using System.Drawing.Imaging;
public static class BwMenuQa {
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left,Top,Right,Bottom; }
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);[DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd,uint msg,IntPtr wParam,IntPtr lParam);[DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd,out RECT r);[DllImport("user32.dll")] public static extern uint MapVirtualKey(uint c,uint t);
 public static void Down(IntPtr h,int k){uint sc=MapVirtualKey((uint)k,0),ex=(k>=0x25&&k<=0x28)?(1u<<24):0u;PostMessage(h,0x0100,(IntPtr)k,(IntPtr)(1u|(sc<<16)|ex));}
 public static void Up(IntPtr h,int k){uint sc=MapVirtualKey((uint)k,0),ex=(k>=0x25&&k<=0x28)?(1u<<24):0u;PostMessage(h,0x0101,(IntPtr)k,(IntPtr)(0xC0000001u|(sc<<16)|ex));}
 public static void Tap(IntPtr h,int k,int ms){Down(h,k);Thread.Sleep(ms);Up(h,k);Thread.Sleep(180);}public static void Capture(IntPtr h,string f){RECT r;GetWindowRect(h,out r);using(Bitmap b=new Bitmap(r.Right-r.Left,r.Bottom-r.Top)){using(Graphics g=Graphics.FromImage(b)){g.CopyFromScreen(r.Left,r.Top,0,0,new Size(b.Width,b.Height));}b.Save(f,ImageFormat.Png);}}
}
"@
$p=Start-Process -FilePath $exe -WorkingDirectory $root -PassThru
try{$null=$p.WaitForInputIdle(10000);Start-Sleep -Milliseconds 900;$p.Refresh();$h=$p.MainWindowHandle;if(!$h){throw 'No game window'};[void][BwMenuQa]::SetForegroundWindow($h);Start-Sleep -Milliseconds 250
 [BwMenuQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Menu_$stamp`_Title.png"))
 [BwMenuQa]::Tap($h,0x28,100);[BwMenuQa]::Tap($h,0x0D,100);Start-Sleep -Milliseconds 250;[BwMenuQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Menu_$stamp`_Controls.png"))
 [BwMenuQa]::Tap($h,0x1B,100);[BwMenuQa]::Tap($h,0x28,100);[BwMenuQa]::Tap($h,0x0D,100);Start-Sleep -Milliseconds 250;[BwMenuQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Menu_$stamp`_SettingsLow.png"))
 [BwMenuQa]::Tap($h,0x0D,100);Start-Sleep -Milliseconds 200;[BwMenuQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Menu_$stamp`_SettingsHigh.png"))
 [BwMenuQa]::Tap($h,0x28,100);[BwMenuQa]::Tap($h,0x0D,100);Start-Sleep -Milliseconds 150;[BwMenuQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Menu_$stamp`_SettingsMuted.png"))
 [BwMenuQa]::Tap($h,0x28,100);[BwMenuQa]::Tap($h,0x0D,100);[BwMenuQa]::Tap($h,0x26,100);[BwMenuQa]::Tap($h,0x26,100);[BwMenuQa]::Tap($h,0x0D,120);Start-Sleep -Milliseconds 800
 [BwMenuQa]::Tap($h,0x50,100);Start-Sleep -Milliseconds 150;[BwMenuQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Menu_$stamp`_Pause.png"))
 [BwMenuQa]::Tap($h,0x28,100);[BwMenuQa]::Tap($h,0x0D,120);Start-Sleep -Milliseconds 500;[BwMenuQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_Menu_$stamp`_Restarted.png"))
 "timestamp=$stamp`r`nmenus=title,controls,settings-low,settings-high,audio-muted,pause,restart`r`n" | Add-Content -LiteralPath (Join-Path $root 'logs\qa_menu.log');Write-Output "Menu QA completed: $stamp"
}finally{if($p -and !$p.HasExited){[BwMenuQa]::PostMessage($p.MainWindowHandle,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null;if(!$p.WaitForExit(5000)){Stop-Process -Id $p.Id -Force}}}
