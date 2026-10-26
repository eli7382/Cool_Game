param([ValidateSet('v4','v5','v6','v7','v8','v9','v10','v11','v12','v13','current')][string]$Version = 'v4')
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies ([System.Drawing.Bitmap].Assembly.Location) -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class BreakwaterNormalMap {
    public static void Generate(string input, string output, float strength) {
        using (Bitmap source = new Bitmap(input))
        using (Bitmap color = new Bitmap(source.Width, source.Height, PixelFormat.Format32bppArgb)) {
            using (Graphics graphics = Graphics.FromImage(color))
                graphics.DrawImage(source, 0, 0, source.Width, source.Height);
            int width = color.Width, height = color.Height, count = width*height;
            Rectangle rect = new Rectangle(0, 0, width, height);
            BitmapData read = color.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            byte[] rgba = new byte[count*4];
            Marshal.Copy(read.Scan0, rgba, 0, rgba.Length);
            color.UnlockBits(read);
            float[] luminance = new float[count];
            for (int i=0; i<count; ++i) {
                int p=i*4;
                luminance[i]=(rgba[p+2]*0.2126f + rgba[p+1]*0.7152f + rgba[p]*0.0722f)/255.0f;
            }
            using (Bitmap normal = new Bitmap(width, height, PixelFormat.Format32bppArgb)) {
                BitmapData write = normal.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
                byte[] pixels = new byte[count*4];
                for (int y=0; y<height; ++y) for (int x=0; x<width; ++x) {
                    int left=(x+width-1)%width, right=(x+1)%width, up=(y+height-1)%height, down=(y+1)%height;
                    float dx=(luminance[y*width+right]-luminance[y*width+left])*strength;
                    float dy=(luminance[down*width+x]-luminance[up*width+x])*strength;
                    float nx=-dx, ny=-dy, nz=1.0f;
                    float inv=1.0f/(float)Math.Sqrt(nx*nx+ny*ny+nz*nz);
                    int p=(y*width+x)*4;
                    pixels[p]=(byte)Math.Max(0,Math.Min(255,(nz*inv*0.5f+0.5f)*255.0f));
                    pixels[p+1]=(byte)Math.Max(0,Math.Min(255,(ny*inv*0.5f+0.5f)*255.0f));
                    pixels[p+2]=(byte)Math.Max(0,Math.Min(255,(nx*inv*0.5f+0.5f)*255.0f));
                    pixels[p+3]=255;
                }
                Marshal.Copy(pixels, 0, write.Scan0, pixels.Length);
                normal.UnlockBits(write);
                normal.Save(output, ImageFormat.Png);
            }
        }
    }
    public static void GenerateRoughness(string input, string output, float baseRoughness) {
        using (Bitmap source = new Bitmap(input))
        using (Bitmap color = new Bitmap(source.Width, source.Height, PixelFormat.Format32bppArgb)) {
            using (Graphics graphics = Graphics.FromImage(color))
                graphics.DrawImage(source, 0, 0, source.Width, source.Height);
            int width = color.Width, height = color.Height, count = width*height;
            Rectangle rect = new Rectangle(0, 0, width, height);
            BitmapData read = color.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            byte[] rgba = new byte[count*4];
            Marshal.Copy(read.Scan0, rgba, 0, rgba.Length);
            color.UnlockBits(read);
            float[] luminance = new float[count];
            for (int i=0; i<count; ++i) {
                int p=i*4;
                luminance[i]=(rgba[p+2]*0.2126f + rgba[p+1]*0.7152f + rgba[p]*0.0722f)/255.0f;
            }
            using (Bitmap roughness = new Bitmap(width, height, PixelFormat.Format32bppArgb)) {
                BitmapData write = roughness.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
                byte[] pixels = new byte[count*4];
                for (int y=0; y<height; ++y) for (int x=0; x<width; ++x) {
                    int left=(x+width-1)%width, right=(x+1)%width, up=(y+height-1)%height, down=(y+1)%height;
                    float gx=Math.Abs(luminance[y*width+right]-luminance[y*width+left]);
                    float gy=Math.Abs(luminance[down*width+x]-luminance[up*width+x]);
                    float variation=Math.Min(0.17f,(gx+gy)*1.75f+Math.Abs(luminance[y*width+x]-0.5f)*0.035f);
                    byte value=(byte)Math.Max(0,Math.Min(255,(baseRoughness+variation)*255.0f));
                    int p=(y*width+x)*4;
                    pixels[p]=pixels[p+1]=pixels[p+2]=value;pixels[p+3]=255;
                }
                Marshal.Copy(pixels, 0, write.Scan0, pixels.Length);
                roughness.UnlockBits(write);
                roughness.Save(output, ImageFormat.Png);
            }
        }
    }
}
'@

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if ($Version -eq 'current') {
    # Derive each current surface set from the exact albedo tile bound to its
    # high-preset vehicle shell: car v10, aircraft and helicopter v9.
    $mapSets = @(
        @{ name='car'; version='v10'; strength=0.46; roughness=0.22 },
        @{ name='plane'; version='v9'; strength=0.80; roughness=0.38 },
        @{ name='helicopter'; version='v9'; strength=0.80; roughness=0.48 }
    )
    foreach ($set in $mapSets) {
        $source = Join-Path $root ("assets\vehicle_paint_{0}_{1}.png" -f $set.name,$set.version)
        $target = Join-Path $root ("assets\vehicle_normal_{0}_{1}.png" -f $set.name,$set.version)
        $roughness = Join-Path $root ("assets\vehicle_roughness_{0}_{1}.png" -f $set.name,$set.version)
        [BreakwaterNormalMap]::Generate($source,$target,$set.strength)
        [BreakwaterNormalMap]::GenerateRoughness($source,$roughness,$set.roughness)
        [pscustomobject]@{source=$source;normal_map=$target;roughness_map=$roughness;normal_bytes=(Get-Item -LiteralPath $target).Length;roughness_bytes=(Get-Item -LiteralPath $roughness).Length}
    }
    return
}
$names = if ($Version -in @('v6','v7','v9')) { @('car') } else { @('car','plane','helicopter') }
foreach ($name in $names) {
    $source = Join-Path $root ("assets\vehicle_paint_{0}_{1}.png" -f $name,$Version)
    $target = Join-Path $root ("assets\vehicle_normal_{0}_{1}.png" -f $name,$Version)
    $roughness = Join-Path $root ("assets\vehicle_roughness_{0}_{1}.png" -f $name,$Version)
    $normalStrength = if ($Version -eq 'v7') { 0.58 } elseif ($Version -eq 'v12') { 0.64 } elseif ($Version -eq 'v13') { 0.24 } else { 1.8 }
    if ($Version -eq 'v8') { $normalStrength = 0.80 }
    if ($Version -eq 'v9') { $normalStrength = 0.46 }
    if ($Version -eq 'v11') { $normalStrength = 0.72 }
    [BreakwaterNormalMap]::Generate($source,$target,$normalStrength)
    if ($Version -eq 'v13') {
        $base = if ($name -eq 'car') { 0.22 } elseif ($name -eq 'plane') { 0.32 } else { 0.47 }
    } elseif ($Version -eq 'v12') {
        $base = if ($name -eq 'car') { 0.26 } elseif ($name -eq 'plane') { 0.37 } else { 0.50 }
    } elseif ($Version -eq 'v7') {
        $base = 0.28
    } elseif ($Version -eq 'v8') {
        $base = if ($name -eq 'car') { 0.27 } elseif ($name -eq 'plane') { 0.38 } else { 0.48 }
    } elseif ($Version -eq 'v9') {
        $base = 0.22
    } elseif ($Version -eq 'v11') {
        $base = if ($name -eq 'car') { 0.23 } elseif ($name -eq 'plane') { 0.34 } else { 0.55 }
    } elseif ($Version -in @('v5','v6')) {
        $base = if ($name -eq 'car') { 0.36 } elseif ($name -eq 'plane') { 0.34 } else { 0.50 }
    } else {
        $base = if ($name -eq 'car') { 0.30 } elseif ($name -eq 'plane') { 0.31 } else { 0.48 }
    }
    [BreakwaterNormalMap]::GenerateRoughness($source,$roughness,$base)
    [pscustomobject]@{source=$source;normal_map=$target;roughness_map=$roughness;normal_bytes=(Get-Item -LiteralPath $target).Length;roughness_bytes=(Get-Item -LiteralPath $roughness).Length}
}
