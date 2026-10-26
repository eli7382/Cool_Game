param([string]$Executable='game.exe')
$ErrorActionPreference='Stop';$root=(Get-Location).Path;$exe=Join-Path $root "build\bin\Release\$Executable";$stamp=Get-Date -Format 'yyyyMMdd_HHmmss'
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;using System.Runtime.InteropServices;using System.Drawing;using System.Drawing.Imaging;
public static class BwAudioSettingsQa {
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left,Top,Right,Bottom; }
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);[DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();[DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int n);[DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out RECT r);[DllImport("user32.dll")] public static extern uint MapVirtualKey(uint k,uint t);[DllImport("user32.dll")] public static extern void keybd_event(byte key,byte scan,uint flags,UIntPtr extra);[DllImport("kernel32.dll")] public static extern void Sleep(uint ms);
 public static void Tap(IntPtr h,int k,int ms){SetForegroundWindow(h);keybd_event((byte)k,(byte)MapVirtualKey((uint)k,0),0,UIntPtr.Zero);Sleep((uint)ms);keybd_event((byte)k,(byte)MapVirtualKey((uint)k,0),2,UIntPtr.Zero);Sleep(350);}
 public static void TapArrow(IntPtr h,int k,int ms){SetForegroundWindow(h);byte scan=(byte)MapVirtualKey((uint)k,0);keybd_event((byte)k,scan,1,UIntPtr.Zero);Sleep((uint)ms);keybd_event((byte)k,scan,3,UIntPtr.Zero);Sleep(350);}
 public static void Capture(IntPtr h,string f){RECT r;GetWindowRect(h,out r);using(Bitmap b=new Bitmap(r.Right-r.Left,r.Bottom-r.Top)){using(Graphics g=Graphics.FromImage(b)){g.CopyFromScreen(r.Left,r.Top,0,0,new Size(b.Width,b.Height));}b.Save(f,ImageFormat.Png);}}
}
"@
$p=Start-Process -FilePath $exe -WorkingDirectory $root -PassThru
try{$null=$p.WaitForInputIdle(10000);Start-Sleep -Milliseconds 2000;$p.Refresh();$h=$p.MainWindowHandle;if(!$h){throw 'Game window did not appear.'};[void][BwAudioSettingsQa]::ShowWindow($h,9);[void][BwAudioSettingsQa]::SetForegroundWindow($h);Start-Sleep -Milliseconds 500
 [BwAudioSettingsQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_AudioSettingsQA_$stamp`_Title.png"))
 [BwAudioSettingsQa]::Tap($h,0x53,140);[BwAudioSettingsQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_AudioSettingsQA_$stamp`_Menu1.png"))
 [BwAudioSettingsQa]::Tap($h,0x53,140);[BwAudioSettingsQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_AudioSettingsQA_$stamp`_Menu2.png"))
 [BwAudioSettingsQa]::Tap($h,0x0D,140);Start-Sleep -Milliseconds 500;[BwAudioSettingsQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_AudioSettingsQA_$stamp`_Settings.png"))
 [BwAudioSettingsQa]::Tap($h,0x53,140);[BwAudioSettingsQa]::TapArrow($h,0x27,140);[BwAudioSettingsQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_AudioSettingsQA_$stamp`_MasterAdjusted.png"))
 [BwAudioSettingsQa]::TapArrow($h,0x25,140);[BwAudioSettingsQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_AudioSettingsQA_$stamp`_MasterRestored.png"))
 for($i=0;$i -lt 6;$i++){[BwAudioSettingsQa]::Tap($h,0x53,80)}
 [BwAudioSettingsQa]::Tap($h,0x0D,140);[BwAudioSettingsQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_AudioSettingsQA_$stamp`_CaptionsOff.png"));[BwAudioSettingsQa]::Tap($h,0x0D,140)
 [BwAudioSettingsQa]::Tap($h,0x53,140);[BwAudioSettingsQa]::Tap($h,0x0D,140);[BwAudioSettingsQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_AudioSettingsQA_$stamp`_SpatialOff.png"));[BwAudioSettingsQa]::Tap($h,0x0D,140)
 [BwAudioSettingsQa]::Tap($h,0x53,140);[BwAudioSettingsQa]::Tap($h,0x0D,140);[BwAudioSettingsQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_AudioSettingsQA_$stamp`_DialogueFocus.png"));[BwAudioSettingsQa]::Tap($h,0x0D,140)
 Add-Content -LiteralPath (Join-Path $root 'logs\qa_audio.log') -Value "settings_capture=$stamp`nsettings_opened=manual-menu-navigation`ncontrols=master-adjust-and-restore,captions-toggle-and-restore,spatial-toggle-and-restore,mix-profile-toggle-and-restore`n"
 Write-Output "Audio settings captures completed: $stamp"
}finally{if($p -and !$p.HasExited){$null=[BwAudioSettingsQa]::SetForegroundWindow($p.MainWindowHandle);[BwAudioSettingsQa]::Tap($p.MainWindowHandle,0x1B,100);Start-Sleep -Milliseconds 300;[BwAudioSettingsQa]::Tap($p.MainWindowHandle,0x1B,100);if(!$p.WaitForExit(2500)){Stop-Process -Id $p.Id -Force}}}
