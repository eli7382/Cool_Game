param([ValidatePattern('^v[0-9]+$')][string]$Version = 'v8')
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies ([System.Drawing.Bitmap].Assembly.Location) -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class BreakwaterSeamlessTexture {
    private static int ClampInt(int value,int min,int max){return Math.Max(min,Math.Min(max,value));}
    private static int Row(int y, int height, int stride){return stride >= 0 ? y*stride : (height-1-y)*-stride;}
    public static double EdgeDelta(string path, bool horizontal) {
        using (Bitmap bitmap = new Bitmap(path)) {
            int width=bitmap.Width,height=bitmap.Height;
            Rectangle rect=new Rectangle(0,0,width,height);
            BitmapData data=bitmap.LockBits(rect,ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
            int stride=Math.Abs(data.Stride);byte[] pixels=new byte[stride*height];Marshal.Copy(data.Scan0,pixels,0,pixels.Length);bitmap.UnlockBits(data);
            double sum=0;int count=horizontal?height:width;
            for(int i=0;i<count;++i){
                int a=horizontal?Row(i,height,data.Stride):Row(0,height,data.Stride)+i*4;
                int b=horizontal?Row(i,height,data.Stride)+(width-1)*4:Row(height-1,height,data.Stride)+i*4;
                for(int c=0;c<3;++c)sum+=Math.Abs((int)pixels[a+c]-(int)pixels[b+c]);
            }
            return sum/(count*3.0);
        }
    }
    private static byte[] Read(Bitmap bitmap,out int stride) {
        Rectangle rect=new Rectangle(0,0,bitmap.Width,bitmap.Height);
        BitmapData data=bitmap.LockBits(rect,ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
        stride=Math.Abs(data.Stride);byte[] pixels=new byte[stride*bitmap.Height];Marshal.Copy(data.Scan0,pixels,0,pixels.Length);bitmap.UnlockBits(data);return pixels;
    }
    private static void Write(Bitmap bitmap,byte[] pixels) {
        Rectangle rect=new Rectangle(0,0,bitmap.Width,bitmap.Height);
        BitmapData data=bitmap.LockBits(rect,ImageLockMode.WriteOnly,PixelFormat.Format32bppArgb);
        Marshal.Copy(pixels,0,data.Scan0,pixels.Length);bitmap.UnlockBits(data);
    }
    private static byte Mix(byte a,byte b,double weight){return (byte)ClampInt((int)Math.Round(a*(1.0-weight)+b*weight),0,255);}
    public static void Make(string input,string output,int blendWidth) {
        using(Bitmap original=new Bitmap(input)) {
            int width=original.Width,height=original.Height,sourceStride;
            byte[] source=Read(original,out sourceStride),horizontal=new byte[source.Length];
            int band=ClampInt(blendWidth,2,Math.Min(width,height)/3);
            for(int y=0;y<height;++y)for(int x=0;x<width;++x){
                int row=Row(y,height,sourceStride),p=row+x*4,dist=Math.Min(x,width-1-x),partner=row+(width-1-x)*4;
                double alpha=dist>=band?0.0:0.5*(1.0+Math.Cos(Math.PI*dist/band))*0.5;
                for(int c=0;c<3;++c)horizontal[p+c]=Mix(source[p+c],source[partner+c],alpha);
                horizontal[p+3]=source[p+3];
            }
            using(Bitmap xPass=new Bitmap(width,height,PixelFormat.Format32bppArgb)) {
                Write(xPass,horizontal);int xStride;byte[] xPixels=Read(xPass,out xStride),vertical=new byte[xPixels.Length];
                for(int y=0;y<height;++y)for(int x=0;x<width;++x){
                    int row=Row(y,height,xStride),p=row+x*4,dist=Math.Min(y,height-1-y),partner=Row(height-1-y,height,xStride)+x*4;
                    double alpha=dist>=band?0.0:0.5*(1.0+Math.Cos(Math.PI*dist/band))*0.5;
                    for(int c=0;c<3;++c)vertical[p+c]=Mix(xPixels[p+c],xPixels[partner+c],alpha);
                    vertical[p+3]=xPixels[p+3];
                }
                using(Bitmap final=new Bitmap(width,height,PixelFormat.Format32bppArgb)){Write(final,vertical);final.Save(output,ImageFormat.Png);}
            }
        }
    }
}
'@

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$rawRoot = Join-Path $root 'logs\asset_inspect'
$files = @('car','plane','helicopter')
foreach ($name in $files) {
    $source = Join-Path $rawRoot ("vehicle_paint_{0}_{1}_raw.png" -f $name,$Version)
    $target = Join-Path $root ("assets\vehicle_paint_{0}_{1}.png" -f $name,$Version)
    if (!(Test-Path -LiteralPath $source)) { throw "Preserved raw texture is missing: $source" }
    $beforeX = [BreakwaterSeamlessTexture]::EdgeDelta($source,$true)
    $beforeY = [BreakwaterSeamlessTexture]::EdgeDelta($source,$false)
    [BreakwaterSeamlessTexture]::Make($source,$target,80)
    $afterX = [BreakwaterSeamlessTexture]::EdgeDelta($target,$true)
    $afterY = [BreakwaterSeamlessTexture]::EdgeDelta($target,$false)
    [pscustomobject]@{name=$name;before_x=[math]::Round($beforeX,2);before_y=[math]::Round($beforeY,2);after_x=[math]::Round($afterX,2);after_y=[math]::Round($afterY,2)}
}
