param([string[]]$Modes = @('car','car-front','car-cockpit','plane','plane-front','plane-cockpit','heli','heli-front','heli-cockpit'))
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$workspace = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
foreach ($mode in $Modes) {
    $source = Get-ChildItem -LiteralPath (Join-Path $workspace 'screenshots') -Filter ("Breakwater_LiveQA_{0}_*.png" -f $mode) | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $source) { throw "Missing renderer capture for $mode" }
    $bitmap = [Drawing.Bitmap]::new($source.FullName)
    try {
        if ($bitmap.Width -ne 2880 -or $bitmap.Height -ne 1575) { throw "Unexpected dimensions for $($source.Name): $($bitmap.Width)x$($bitmap.Height)" }
        $rect = [Drawing.Rectangle]::new(0,525,1920,1050)
        $crop = $bitmap.Clone($rect,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $destination = Join-Path $workspace ("screenshots\Breakwater_VehicleDetail_{0}_{1}.png" -f $stamp,$mode)
            $crop.Save($destination,[Drawing.Imaging.ImageFormat]::Png)
        } finally { $crop.Dispose() }
    } finally { $bitmap.Dispose() }
}
Get-ChildItem -LiteralPath (Join-Path $workspace 'screenshots') -Filter ("Breakwater_VehicleDetail_{0}_*.png" -f $stamp) | Select-Object Name,Length
