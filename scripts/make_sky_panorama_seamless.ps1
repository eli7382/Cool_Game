param(
    [string]$InputPath = 'assets\storm_sky_v3.png',
    [string]$OutputPath = 'assets\storm_sky_v4.png',
    [ValidateRange(8,512)][int]$BlendWidth = 128
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies ([System.Drawing.Bitmap].Assembly.Location) -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class BreakwaterSkyPanorama {
    private static int Row(int y,int height,int stride){return stride>=0?y*stride:(height-1-y)*-stride;}
    private static byte[] Read(Bitmap bitmap,out int stride){
        Rectangle rect=new Rectangle(0,0,bitmap.Width,bitmap.Height);
        BitmapData data=bitmap.LockBits(rect,ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
        stride=Math.Abs(data.Stride);byte[] pixels=new byte[stride*bitmap.Height];Marshal.Copy(data.Scan0,pixels,0,pixels.Length);bitmap.UnlockBits(data);return pixels;
    }
    private static void Write(Bitmap bitmap,byte[] pixels){
        Rectangle rect=new Rectangle(0,0,bitmap.Width,bitmap.Height);
        BitmapData data=bitmap.LockBits(rect,ImageLockMode.WriteOnly,PixelFormat.Format32bppArgb);
        Marshal.Copy(pixels,0,data.Scan0,pixels.Length);bitmap.UnlockBits(data);
    }
    private static byte Blend(byte a,byte b,double weight){return (byte)Math.Max(0,Math.Min(255,(int)Math.Round(a*(1.0-weight)+b*weight)));}
    public static double EdgeDelta(string path){
        using(Bitmap bitmap=new Bitmap(path)){
            int width=bitmap.Width,height=bitmap.Height,stride;byte[] pixels=Read(bitmap,out stride);double sum=0;
            for(int y=0;y<height;++y){int row=Row(y,height,stride),left=row,right=row+(width-1)*4;for(int c=0;c<3;++c)sum+=Math.Abs((int)pixels[left+c]-pixels[right+c]);}
            return sum/(height*3.0);
        }
    }
    public static void Make(string input,string output,int blendWidth){
        using(Bitmap sourceBitmap=new Bitmap(input)){
            int width=sourceBitmap.Width,height=sourceBitmap.Height,sourceStride;
            byte[] source=Read(sourceBitmap,out sourceStride),result=new byte[source.Length];
            int band=Math.Max(2,Math.Min(blendWidth,width/3));
            for(int y=0;y<height;++y)for(int x=0;x<width;++x){
                int row=Row(y,height,sourceStride),p=row+x*4,dist=Math.Min(x,width-1-x),partner=row+(width-1-x)*4;
                double alpha=dist>=band?0.0:0.25*(1.0+Math.Cos(Math.PI*dist/band));
                for(int c=0;c<3;++c)result[p+c]=Blend(source[p+c],source[partner+c],alpha);
                result[p+3]=source[p+3];
            }
            using(Bitmap finalBitmap=new Bitmap(width,height,PixelFormat.Format32bppArgb)){Write(finalBitmap,result);finalBitmap.Save(output,ImageFormat.Png);}
        }
    }
}
'@
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$input = Join-Path $root $InputPath
$output = Join-Path $root $OutputPath
if (!(Test-Path -LiteralPath $input)) { throw "Input panorama is missing: $input" }
$before = [BreakwaterSkyPanorama]::EdgeDelta($input)
[BreakwaterSkyPanorama]::Make($input,$output,$BlendWidth)
$after = [BreakwaterSkyPanorama]::EdgeDelta($output)
[pscustomobject]@{
    input = $input
    output = $output
    edge_delta_before = [math]::Round($before,2)
    edge_delta_after = [math]::Round($after,2)
    size_bytes = (Get-Item -LiteralPath $output).Length
} | ConvertTo-Json -Depth 3
