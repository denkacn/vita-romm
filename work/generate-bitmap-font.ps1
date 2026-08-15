$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$Project = "D:\MyProjects\Software\vita-romm\work\romm-vita-skeleton"
$FontPath = "D:\MyProjects\Software\vita-romm\work\inter-download\extras\ttf\InterDisplay-SemiBold.ttf"
$AtlasPath = Join-Path $Project "assets\fonts\inter_atlas.png"
$HeaderPath = Join-Path $Project "src\font_atlas.h"
$Sizes = @(21, 23, 26, 30, 34, 46)
$First = 32
$Last = 126
$Padding = 3
$AtlasWidth = 1024

$private = New-Object System.Drawing.Text.PrivateFontCollection
$private.AddFontFile($FontPath)
$family = $private.Families[0]
$style = [System.Drawing.FontStyle]::Regular
$fmt = [System.Drawing.StringFormat]::GenericTypographic
$fmt.FormatFlags = $fmt.FormatFlags -bor [System.Drawing.StringFormatFlags]::MeasureTrailingSpaces

$probe = [System.Drawing.Bitmap]::new(8, 8, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$probeG = [System.Drawing.Graphics]::FromImage($probe)
$probeG.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

$glyphs = @()
$totalHeight = 0
foreach ($size in $Sizes) {
  $font = [System.Drawing.Font]::new($family, [float]$size, $style, [System.Drawing.GraphicsUnit]::Pixel)
  $em = $family.GetEmHeight($style)
  $ascent = [int][Math]::Ceiling($family.GetCellAscent($style) * $font.Size / $em)
  $lineHeight = [int][Math]::Ceiling($font.GetHeight($probeG))
  $x = 0
  $y = $totalHeight
  $rowH = 0
  $entries = @()
  for ($code = $First; $code -le $Last; $code++) {
    $ch = [char]$code
    $measure = $probeG.MeasureString([string]$ch, $font, 512, $fmt)
    $advance = [int][Math]::Ceiling($measure.Width)
    if ($code -eq 32 -and $advance -lt [int]($size * 0.32)) { $advance = [int][Math]::Ceiling($size * 0.34) }
    if ($advance -lt 1) { $advance = 1 }
    $cellW = $advance + $Padding * 2 + 2
    $cellH = $lineHeight + $Padding * 2 + 2
    if ($x + $cellW -gt $AtlasWidth) {
      $x = 0
      $y += $rowH
      $rowH = 0
    }
    $entries += [pscustomobject]@{
      Code=$code; X=$x; Y=$y; W=$cellW; H=$cellH; XOff=0; YOff=(-$ascent - $Padding); Advance=$advance
    }
    $x += $cellW
    if ($cellH -gt $rowH) { $rowH = $cellH }
  }
  $totalHeight = $y + $rowH
  $glyphs += [pscustomobject]@{ Size=$size; Ascent=$ascent; LineHeight=$lineHeight; Font=$font; Entries=$entries }
}
$probeG.Dispose()
$probe.Dispose()

$atlas = [System.Drawing.Bitmap]::new($AtlasWidth, $totalHeight, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($atlas)
$g.Clear([System.Drawing.Color]::Transparent)
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$brush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::White)
foreach ($set in $glyphs) {
  foreach ($entry in $set.Entries) {
    if ($entry.Code -eq 32) { continue }
    $g.DrawString([string][char]$entry.Code, $set.Font, $brush, [float]($entry.X + $Padding), [float]($entry.Y + $Padding), $fmt)
  }
}
$brush.Dispose()
$g.Dispose()
$atlas.Save($AtlasPath, [System.Drawing.Imaging.ImageFormat]::Png)
$atlas.Dispose()
foreach ($set in $glyphs) { $set.Font.Dispose() }
$private.Dispose()

$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add("#ifndef FONT_ATLAS_H")
$lines.Add("#define FONT_ATLAS_H")
$lines.Add("")
$lines.Add('#define FONT_ATLAS_PATH "app0:assets/fonts/inter_atlas.png"')
$lines.Add("#define FONT_ATLAS_FIRST 32")
$lines.Add("#define FONT_ATLAS_COUNT 95")
$lines.Add("#define FONT_ATLAS_SIZE_COUNT $($Sizes.Count)")
$lines.Add("")
$lines.Add("typedef struct {")
$lines.Add("    unsigned short x;")
$lines.Add("    unsigned short y;")
$lines.Add("    unsigned short w;")
$lines.Add("    unsigned short h;")
$lines.Add("    short xoff;")
$lines.Add("    short yoff;")
$lines.Add("    unsigned short advance;")
$lines.Add("} BitmapGlyph;")
$lines.Add("")
$lines.Add("typedef struct {")
$lines.Add("    unsigned short size;")
$lines.Add("    unsigned short ascent;")
$lines.Add("    unsigned short line_height;")
$lines.Add("} BitmapFontSize;")
$lines.Add("")
$lines.Add("static const BitmapFontSize FONT_ATLAS_SIZES[FONT_ATLAS_SIZE_COUNT] = {")
foreach ($set in $glyphs) { $lines.Add("    { $($set.Size), $($set.Ascent), $($set.LineHeight) },") }
$lines.Add("};")
$lines.Add("")
$lines.Add("static const BitmapGlyph FONT_ATLAS_GLYPHS[FONT_ATLAS_SIZE_COUNT][FONT_ATLAS_COUNT] = {")
foreach ($set in $glyphs) {
  $lines.Add("    {")
  foreach ($entry in $set.Entries) {
    $lines.Add("        { $($entry.X), $($entry.Y), $($entry.W), $($entry.H), $($entry.XOff), $($entry.YOff), $($entry.Advance) },")
  }
  $lines.Add("    },")
}
$lines.Add("};")
$lines.Add("")
$lines.Add("#endif")
Set-Content -Encoding ASCII -Path $HeaderPath -Value $lines

$img = [System.Drawing.Image]::FromFile($AtlasPath)
[pscustomobject]@{ Atlas=$AtlasPath; Width=$img.Width; Height=$img.Height; Length=(Get-Item $AtlasPath).Length; Header=$HeaderPath }
$img.Dispose()

