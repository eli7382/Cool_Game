param([string]$Executable='game.exe')
$ErrorActionPreference='Stop'
$root=(Get-Location).Path
$exe=Join-Path $root "build\bin\Release\$Executable"
$voiceCount=(Get-ChildItem -LiteralPath (Join-Path $root 'assets\audio\narrator') -Filter '*.wav').Count
if($voiceCount -ne 16){throw "Expected 16 bundled narrator WAVs, found $voiceCount."}
$stamp=Get-Date -Format 'yyyyMMdd_HHmmss'
$sessionLog=Join-Path $root 'logs\breakwater-session.log'
$priorLog=if(Test-Path -LiteralPath $sessionLog){(Get-Content -LiteralPath $sessionLog -Raw).Length}else{0}
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;using System.Runtime.InteropServices;using System.Drawing;using System.Drawing.Imaging;
public static class BwAudioQa {
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left,Top,Right,Bottom; }
 [StructLayout(LayoutKind.Sequential)] struct KEYBDINPUT { public ushort wVk,wScan; public uint dwFlags,time; public IntPtr dwExtraInfo; }
 [StructLayout(LayoutKind.Explicit,Size=32)] struct INPUTUNION { [FieldOffset(0)] public KEYBDINPUT ki; }
 [StructLayout(LayoutKind.Sequential)] struct INPUT { public uint type; public INPUTUNION u; }
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);[DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();[DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int n);[DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);[DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h,uint msg,IntPtr wp,IntPtr lp);[DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out RECT r);[DllImport("user32.dll",SetLastError=true)] static extern uint SendInput(uint n,INPUT[] inputs,int size);[DllImport("kernel32.dll")] public static extern void Sleep(uint ms);
 public static bool Focus(IntPtr h){ShowWindow(h,9);SetForegroundWindow(h);BringWindowToTop(h);for(int i=0;i<30;i++){if(GetForegroundWindow()==h)return true;Sleep(50);}return GetForegroundWindow()==h;}
 static void Key(int k,bool up){INPUT i=new INPUT();i.type=1;i.u.ki.wVk=(ushort)k;i.u.ki.dwFlags=up?2u:0u;if(SendInput(1,new INPUT[]{i},Marshal.SizeOf(typeof(INPUT)))!=1)throw new Exception("SendInput failed: "+Marshal.GetLastWin32Error());}
 public static void Down(IntPtr h,int k){if(!Focus(h))throw new Exception("Game window did not become foreground.");Key(k,false);}
 public static void Up(IntPtr h,int k){Key(k,true);}
 public static void Tap(IntPtr h,int k,int ms){Down(h,k);Sleep((uint)ms);Up(h,k);Sleep(180);}
 public static void Hold(IntPtr h,int k,int ms){Down(h,k);Sleep((uint)ms);Up(h,k);Sleep(180);}
 public static void Capture(IntPtr h,string f){RECT r;GetWindowRect(h,out r);using(Bitmap b=new Bitmap(r.Right-r.Left,r.Bottom-r.Top)){using(Graphics g=Graphics.FromImage(b)){g.CopyFromScreen(r.Left,r.Top,0,0,new Size(b.Width,b.Height));}b.Save(f,ImageFormat.Png);}}
}
"@
$p=Start-Process -FilePath $exe -WorkingDirectory $root -PassThru
try {
    $null=$p.WaitForInputIdle(10000);Start-Sleep -Milliseconds 1600;$p.Refresh();$h=$p.MainWindowHandle
    if(!$h){throw 'Game window did not appear.'}
    $activated=[BwAudioQa]::Focus($h)
    "window=$h foreground=$activated" | Add-Content -LiteralPath (Join-Path $root 'logs\qa_audio.log')

    [BwAudioQa]::Tap($h,0x0D,120);Start-Sleep -Milliseconds 1300
    $text=Get-Content -LiteralPath $sessionLog -Raw
    if($text.Length -le $priorLog -or $text.Substring($priorLog) -notmatch 'mission-start'){throw 'The game window did not start the mission from this QA run; audio screenshots are not valid.'}
    [BwAudioQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_AudioQA_$stamp`_Briefing.png"))
    [BwAudioQa]::Hold($h,0x44,420);[BwAudioQa]::Tap($h,0x45,120);Start-Sleep -Milliseconds 250
    $text=Get-Content -LiteralPath $sessionLog -Raw
    if($text.Substring($priorLog) -notmatch 'enter-car'){throw 'The game did not record car entry during this QA run; pickup audio was not verified.'}
    [BwAudioQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_AudioQA_$stamp`_Pickup.png"))
    Start-Sleep -Milliseconds 11000
    [BwAudioQa]::Capture($h,(Join-Path $root "screenshots\Breakwater_AudioQA_$stamp`_PickupGuidance.png"))
    Start-Sleep -Milliseconds 400
    $text=Get-Content -LiteralPath $sessionLog -Raw
    if($text.Length -le $priorLog){throw 'Runtime session log did not grow during this QA run.'}
    $text=$text.Substring($priorLog)
    $required=@('audio-ready-voices-16-of-16-streams-5-of-5','narrator-briefing','enter-car','narrator-pickup-controls')
    foreach($needle in $required){if($text -notmatch [regex]::Escape($needle)){throw "Runtime audio evidence missing: $needle"}}
    "timestamp=$stamp`nexecutable=$Executable`naudio_load=16_voices_5_streams`nvoice_events=briefing,pickup-controls`nvehicle_enter=car`ncaptures=briefing,pickup,pickup-guidance`n" | Add-Content -LiteralPath (Join-Path $root 'logs\qa_audio.log')
    Write-Output "Audio runtime QA completed: $stamp; voice events and audio resources logged."
} catch {
    "timestamp=$stamp`nstatus=failed`nreason=$($_.Exception.Message)`n" | Add-Content -LiteralPath (Join-Path $root 'logs\qa_audio.log')
    throw
} finally {
    if($p -and !$p.HasExited){[void][BwAudioQa]::PostMessage($p.MainWindowHandle,0x0010,[IntPtr]::Zero,[IntPtr]::Zero);if(!$p.WaitForExit(2500)){Stop-Process -Id $p.Id -Force}}
}
