param([string]$BatchRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Drawing.Drawing2D;
using System.Runtime.InteropServices;
public static class MenuTextureExport {
 public static Rectangle Bounds(Bitmap src) {
  using(var b=new Bitmap(src.Width,src.Height,PixelFormat.Format32bppArgb)) {
   using(var g=Graphics.FromImage(b)) {g.CompositingMode=CompositingMode.SourceCopy;g.DrawImageUnscaled(src,0,0);}
   var data=b.LockBits(new Rectangle(0,0,b.Width,b.Height),ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
   int stride=Math.Abs(data.Stride);byte[] pixels=new byte[stride*b.Height];Marshal.Copy(data.Scan0,pixels,0,pixels.Length);
   b.UnlockBits(data);int x0=b.Width,y0=b.Height,x1=-1,y1=-1;
   for(int y=0;y<b.Height;y++)for(int x=0;x<b.Width;x++)if(pixels[y*stride+x*4+3]>8) {x0=Math.Min(x0,x);y0=Math.Min(y0,y);x1=Math.Max(x1,x);y1=Math.Max(y1,y);}
   if(x1<0)throw new Exception("Empty texture");return Rectangle.FromLTRB(x0,y0,x1+1,y1+1);
  }
 }
 public static Rectangle Export(string input,string output,bool opaque,bool logo) {
  using(var src=new Bitmap(input))using(var dst=new Bitmap(1920,1080,PixelFormat.Format32bppArgb))using(var g=Graphics.FromImage(dst)) {
   g.Clear(Color.Transparent);g.CompositingMode=CompositingMode.SourceCopy;
   g.InterpolationMode=InterpolationMode.NearestNeighbor;g.PixelOffsetMode=PixelOffsetMode.Half;g.SmoothingMode=SmoothingMode.None;
   var source=opaque?new Rectangle(0,0,src.Width,src.Height):Bounds(src);
   Rectangle target;
   if(opaque)target=new Rectangle(0,0,1920,1080);
   else if(logo) {double s=Math.Min(1040.0/source.Width,580.0/source.Height);int w=(int)Math.Round(source.Width*s),h=(int)Math.Round(source.Height*s);target=new Rectangle((1920-w)/2,(1080-h)/2,w,h);}
   else target=new Rectangle(680,490,560,100);
   g.DrawImage(src,target,source,GraphicsUnit.Pixel);dst.Save(output,ImageFormat.Png);return target;
  }
 }
}
'@ -ReferencedAssemblies System.Drawing
$spec = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'prompts.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$rows = @()
foreach ($item in $spec.images) {
 $inputPath=Join-Path $BatchRoot $item.source
 $outputPath=Join-Path $BatchRoot ('export/'+$item.file)
 $rect=[MenuTextureExport]::Export($inputPath,$outputPath,($item.key -eq 'background'),($item.key -eq 'logo'))
 $inputSize=[System.Drawing.Image]::FromFile($inputPath)
 $row=[ordered]@{id=('c2d.v2.menu.'+$item.key);file=('export/'+$item.file);source=$item.source;size=@(1920,1080);originalSize=@($inputSize.Width,$inputSize.Height);transparent=[bool]$item.alpha;region=@($rect.X,$rect.Y,$rect.Width,$rect.Height);sha256=(Get-FileHash -LiteralPath $outputPath -Algorithm SHA256).Hash.ToLower();sourceSha256=(Get-FileHash -LiteralPath $inputPath -Algorithm SHA256).Hash.ToLower();normalization='Nearest-neighbor dimensional export; trim near-invisible alpha margins at threshold 8; no repaint/color edits';runtimeIntegrated=$false}
 if ($item.key.StartsWith('button-')) {$row['textSafeRect']=@(760,512,400,56);$row['targetSize']=@(560,100);$row['stretchPolicy']='Use region as AtlasTexture at 560x100; no stretch or automatic nine-slice';$row['hitPolygon']=@(@(0,50),@(46,0),@(514,0),@(560,50),@(514,100),@(46,100))}
 $rows+=$row;$inputSize.Dispose()
}
$manifest=[ordered]@{version=1;designSize=@(1920,1080);style='C refined pixel fantasy';exportApproval='User explicitly approved dimension normalization';assets=$rows}
$manifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $BatchRoot 'manifest.json') -Encoding UTF8
Write-Output ('Exported '+$rows.Count+' textures at 1920x1080')

