param(
    [string]$InputPath = 'logs\asset_inspect\marsh_ground_v8_raw.png',
    [string]$OutputPath = 'assets\marsh_ground_v8.png',
    [ValidateRange(8,512)][int]$BlendWidth = 96
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies ([System.Drawing.Bitmap].Assembly.Location) -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class BreakwaterGroundTexture {
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
    public static double EdgeDelta(string path,bool horizontal){
        using(Bitmap bitmap=new Bitmap(path)){
            int width=bitmap.Width,height=bitmap.Height,stride;byte[] pixels=Read(bitmap,out stride);double sum=0;
            int count=horizontal?height:width;
            for(int i=0;i<count;++i){
                int a=horizontal?Row(i,height,stride):Row(0,height,stride)+i*4;
                int b=horizontal?Row(i,height,stride)+(width-1)*4:Row(height-1,height,stride)+i*4;
                for(int c=0;c<3;++c)sum+=Math.Abs((int)pixels[a+c]-(int)pixels[b+c]);
            }
            return sum/(count*3.0);
        }
    }
    public static void Make(string input,string output,int blendWidth){
        using(Bitmap sourceBitmap=new Bitmap(input)){
            int width=sourceBitmap.Width,height=sourceBitmap.Height,sourceStride;
            byte[] source=Read(sourceBitmap,out sourceStride),horizontal=new byte[source.Length];
            int band=Math.Max(2,Math.Min(blendWidth,Math.Min(width,height)/3));
            for(int y=0;y<height;++y)for(int x=0;x<width;++x){
                int row=Row(y,height,sourceStride),p=row+x*4,dist=Math.Min(x,width-1-x),partner=row+(width-1-x)*4;
                double alpha=dist>=band?0.0:0.25*(1.0+Math.Cos(Math.PI*dist/band));
                for(int c=0;c<3;++c)horizontal[p+c]=Blend(source[p+c],source[partner+c],alpha);
                horizontal[p+3]=source[p+3];
            }
            using(Bitmap xPass=new Bitmap(width,height,PixelFormat.Format32bppArgb)){
                Write(xPass,horizontal);int xStride;byte[] xPixels=Read(xPass,out xStride),vertical=new byte[xPixels.Length];
                for(int y=0;y<height;++y)for(int x=0;x<width;++x){
                    int row=Row(y,height,xStride),p=row+x*4,dist=Math.Min(y,height-1-y),partner=Row(height-1-y,height,xStride)+x*4;
                    double alpha=dist>=band?0.0:0.25*(1.0+Math.Cos(Math.PI*dist/band));
                    for(int c=0;c<3;++c)vertical[p+c]=Blend(xPixels[p+c],xPixels[partner+c],alpha);
                    vertical[p+3]=xPixels[p+3];
                }
                using(Bitmap finalBitmap=new Bitmap(width,height,PixelFormat.Format32bppArgb)){Write(finalBitmap,vertical);finalBitmap.Save(output,ImageFormat.Png);}
            }
        }
    }
}
'@
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$input = Join-Path $root $InputPath
$output = Join-Path $root $OutputPath
if (!(Test-Path -LiteralPath $input)) { throw "Input texture is missing: $input" }
$beforeX = [BreakwaterGroundTexture]::EdgeDelta($input,$true)
$beforeY = [BreakwaterGroundTexture]::EdgeDelta($input,$false)
[BreakwaterGroundTexture]::Make($input,$output,$BlendWidth)
$afterX = [BreakwaterGroundTexture]::EdgeDelta($output,$true)
$afterY = [BreakwaterGroundTexture]::EdgeDelta($output,$false)
$dimensions = [System.Drawing.Image]::FromFile($output)
$width = $dimensions.Width
$height = $dimensions.Height
$dimensions.Dispose()
[pscustomobject]@{
    input = $input
    output = $output
    size_bytes = (Get-Item -LiteralPath $output).Length
    dimensions = "${width}x${height}"
    edge_delta_before = @{horizontal=[math]::Round($beforeX,2);vertical=[math]::Round($beforeY,2)}
    edge_delta_after = @{horizontal=[math]::Round($afterX,2);vertical=[math]::Round($afterY,2)}
} | ConvertTo-Json -Depth 3
