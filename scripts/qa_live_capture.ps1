param(
    [string]$GamePath = (Join-Path $PSScriptRoot '..\build\bin\Release\game.exe'),
    [string]$Workspace = (Split-Path $PSScriptRoot -Parent)
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$drawingAssembly = [System.Drawing.Bitmap].Assembly.Location
Add-Type -ReferencedAssemblies $drawingAssembly -TypeDefinition @'
using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Threading;

public static class BreakwaterLiveCapture {
    [StructLayout(LayoutKind.Sequential)] public struct RECT {
        public int Left, Top, Right, Bottom;
    }
    [StructLayout(LayoutKind.Sequential)] struct KEYBDINPUT {
        public ushort wVk, wScan;
        public uint dwFlags, time;
        public UIntPtr dwExtraInfo;
    }
    [StructLayout(LayoutKind.Explicit, Size = 32)] struct INPUTUNION {
        [FieldOffset(0)] public KEYBDINPUT ki;
    }
    [StructLayout(LayoutKind.Sequential)] struct INPUT {
        public uint type;
        public INPUTUNION data;
    }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll", SetLastError = true)] static extern uint SendInput(uint count, INPUT[] inputs, int size);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);

    static bool Key(byte key, uint flags) {
        INPUT i = new INPUT(); i.type = 1; i.data.ki.wVk = key; i.data.ki.dwFlags = flags;
        return SendInput(1, new INPUT[] { i }, Marshal.SizeOf(typeof(INPUT))) == 1;
    }
    public static bool Tap(byte key) {
        bool down = Key(key, 0);
        Thread.Sleep(100);
        bool up = Key(key, 2);
        Thread.Sleep(180);
        return down && up;
    }
    public static bool Capture(IntPtr hWnd, string path) {
        RECT r;
        if (!GetWindowRect(hWnd, out r)) return false;
        int w = r.Right - r.Left, h = r.Bottom - r.Top;
        if (w < 32 || h < 32) return false;
        using (Bitmap b = new Bitmap(w, h)) {
            using (Graphics g = Graphics.FromImage(b)) {
                g.CopyFromScreen(r.Left, r.Top, 0, 0, new Size(w, h));
            }
            b.Save(path, System.Drawing.Imaging.ImageFormat.Png);
        }
        return true;
    }
}
'@

$GamePath = (Resolve-Path -LiteralPath $GamePath).Path
$Workspace = (Resolve-Path -LiteralPath $Workspace).Path
$titleShot = Join-Path $Workspace 'screenshots\Breakwater_LiveQA_Title.png'
$playShot = Join-Path $Workspace 'screenshots\Breakwater_LiveQA_Gameplay.png'
$process = Start-Process -FilePath $GamePath -WorkingDirectory $Workspace -PassThru

for ($i = 0; $i -lt 50; $i++) {
    $process.Refresh()
    if ($process.MainWindowHandle -ne [IntPtr]::Zero) { break }
    Start-Sleep -Milliseconds 100
}
if ($process.MainWindowHandle -eq [IntPtr]::Zero) { throw 'Game window did not appear.' }

$window = $process.MainWindowHandle
$focused = [BreakwaterLiveCapture]::SetForegroundWindow($window)
Start-Sleep -Milliseconds 500
$titleCaptured = [BreakwaterLiveCapture]::Capture($window, $titleShot)
$enterAccepted = [BreakwaterLiveCapture]::Tap(0x0D) # Enter: start the operation from the title screen.
Start-Sleep -Milliseconds 900
$gameplayCaptured = [BreakwaterLiveCapture]::Capture($window, $playShot)
[BreakwaterLiveCapture]::Tap(0x7B) # F12: use the game's own screenshot path as well.
Start-Sleep -Milliseconds 800

$logPath = Join-Path $Workspace 'logs\breakwater-session.log'
$process.Refresh()
if (-not $process.HasExited) {
    [BreakwaterLiveCapture]::PostMessage($window, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    if (-not $process.WaitForExit(2000)) { $process.Kill(); $process.WaitForExit() }
}

[pscustomobject]@{
    process_id = $process.Id
    exit_code = $process.ExitCode
    foreground_request_succeeded = $focused
    foreground_window = [BreakwaterLiveCapture]::GetForegroundWindow().ToInt64()
    enter_input_accepted = $enterAccepted
    title_screenshot = $titleShot
    title_screenshot_captured = $titleCaptured
    gameplay_screenshot = $playShot
    gameplay_screenshot_captured = $gameplayCaptured
    log_tail = (Get-Content -LiteralPath $logPath | Select-Object -Last 20) -join "`n"
} | ConvertTo-Json -Depth 3
