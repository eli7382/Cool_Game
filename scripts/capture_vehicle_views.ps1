param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('car','car-cockpit','plane','plane-cockpit','heli','heli-cockpit')]
    [string]$Mode
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies ([System.Drawing.Bitmap].Assembly.Location) -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class BreakwaterWindowCapture {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll")] static extern bool ClientToScreen(IntPtr hwnd, ref POINT point);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
    public static bool Capture(IntPtr hwnd, string path) {
        RECT rect;
        if (!GetClientRect(hwnd, out rect)) return false;
        POINT origin = new POINT();
        if (!ClientToScreen(hwnd, ref origin)) return false;
        int width = rect.Right - rect.Left, height = rect.Bottom - rect.Top;
        if (width < 32 || height < 32) return false;
        using (Bitmap bitmap = new Bitmap(width, height, PixelFormat.Format32bppArgb)) {
            using (Graphics graphics = Graphics.FromImage(bitmap))
                graphics.CopyFromScreen(origin.X, origin.Y, 0, 0, new Size(width, height));
            bitmap.Save(path, ImageFormat.Png);
        }
        return true;
    }
}
'@

$workspace = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$exe = (Resolve-Path (Join-Path $workspace 'build\bin\Release\game.exe')).Path
$output = Join-Path $workspace ("screenshots\Breakwater_FinalVehicle_{0}.png" -f $Mode)
$process = Start-Process -FilePath $exe -ArgumentList ("--qa-capture={0} --qa-silent" -f $Mode) -WorkingDirectory $workspace -PassThru
for ($attempt = 0; $attempt -lt 80; $attempt++) {
    $process.Refresh()
    if ($process.HasExited) { throw "Game exited before its $Mode view was ready (exit $($process.ExitCode))." }
    if ($process.MainWindowHandle -ne [IntPtr]::Zero) { break }
    Start-Sleep -Milliseconds 100
}
if ($process.MainWindowHandle -eq [IntPtr]::Zero) { throw 'Game window did not appear.' }
[BreakwaterWindowCapture]::SetForegroundWindow($process.MainWindowHandle) | Out-Null
Start-Sleep -Milliseconds 2200
$captured = [BreakwaterWindowCapture]::Capture($process.MainWindowHandle, $output)
$exited = $process.WaitForExit(5000)
[pscustomobject]@{
    mode = $Mode
    process_id = $process.Id
    captured = $captured
    exited = $exited
    exit_code = if ($exited) { $process.ExitCode } else { $null }
    screenshot = $output
} | ConvertTo-Json -Compress
if (-not $captured -or -not $exited -or $process.ExitCode -ne 0) { throw "Capture QA did not complete for $Mode." }
