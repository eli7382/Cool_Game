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
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$results = [System.Collections.Generic.List[object]]::new()
foreach ($mode in @('car','car-front','car-cockpit','plane','plane-front','plane-cockpit','heli','heli-front','heli-cockpit')) {
    $output = Join-Path $workspace ("screenshots\Breakwater_VehicleDetail_{0}_{1}.png" -f $stamp,$mode)
    $process = Start-Process -FilePath $exe -ArgumentList ("--qa-capture={0} --qa-silent" -f $mode) -WorkingDirectory $workspace -PassThru
    try {
        for ($attempt = 0; $attempt -lt 100; $attempt++) {
            $process.Refresh()
            if ($process.HasExited) { throw "Game exited before $mode rendered (exit $($process.ExitCode))." }
            if ($process.MainWindowHandle -ne [IntPtr]::Zero) { break }
            Start-Sleep -Milliseconds 100
        }
        if ($process.MainWindowHandle -eq [IntPtr]::Zero) { throw "Game window did not appear for $mode." }
        [BreakwaterWindowCapture]::SetForegroundWindow($process.MainWindowHandle) | Out-Null
        Start-Sleep -Milliseconds 5200
        $captured = [BreakwaterWindowCapture]::Capture($process.MainWindowHandle,$output)
        $exited = $process.WaitForExit(4000)
        if (-not $captured -or -not $exited -or $process.ExitCode -ne 0) { throw "Window capture failed for $mode." }
        $file = Get-Item -LiteralPath $output
        if ($file.Length -lt 50000) { throw "Window capture for $mode is unexpectedly small." }
        $results.Add([pscustomobject]@{mode=$mode;exit_code=$process.ExitCode;screenshot=$file.FullName;bytes=$file.Length})
    }
    finally {
        if ($process -and -not $process.HasExited) {
            $process.CloseMainWindow() | Out-Null
            if (-not $process.WaitForExit(2000)) { Stop-Process -Id $process.Id -Force }
        }
    }
}
$results | ConvertTo-Json -Depth 3
