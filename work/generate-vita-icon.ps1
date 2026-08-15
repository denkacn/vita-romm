$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

$outPath = "C:\Users\Denkacn\Documents\Codex\2026-08-14\xn\work\romm-vita-skeleton\assets\icon0.png"
$width = 128
$height = 128

$bitmap = New-Object System.Drawing.Bitmap($width, $height, [System.Drawing.Imaging.PixelFormat]::Format8bppIndexed)
$palette = $bitmap.Palette

$colors = @(
  [System.Drawing.Color]::FromArgb(18, 22, 30),
  [System.Drawing.Color]::FromArgb(30, 42, 56),
  [System.Drawing.Color]::FromArgb(48, 76, 102),
  [System.Drawing.Color]::FromArgb(91, 192, 190),
  [System.Drawing.Color]::FromArgb(238, 242, 246),
  [System.Drawing.Color]::FromArgb(110, 224, 149),
  [System.Drawing.Color]::FromArgb(245, 174, 79),
  [System.Drawing.Color]::FromArgb(12, 16, 22)
)

for ($i = 0; $i -lt 256; $i++) {
  $palette.Entries[$i] = $colors[$i % $colors.Count]
}
$bitmap.Palette = $palette

$rect = New-Object System.Drawing.Rectangle(0, 0, $width, $height)
$data = $bitmap.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::WriteOnly, $bitmap.PixelFormat)
try {
  $stride = [Math]::Abs($data.Stride)
  $bytes = New-Object byte[] ($stride * $height)

  for ($y = 0; $y -lt $height; $y++) {
    for ($x = 0; $x -lt $width; $x++) {
      $dx = $x - 64
      $dy = $y - 64
      $dist2 = $dx * $dx + $dy * $dy
      $idx = 0

      if ($x -lt 8 -or $x -ge 120 -or $y -lt 8 -or $y -ge 120) {
        $idx = 7
      } elseif ($dist2 -lt 2800) {
        $idx = 1
      }
      if ($dist2 -lt 1900) {
        $idx = 2
      }
      if ($x -ge 28 -and $x -le 100 -and $y -ge 36 -and $y -le 88) {
        $idx = 3
      }
      if ($x -ge 38 -and $x -le 52 -and $y -ge 48 -and $y -le 76) {
        $idx = 4
      }
      if ($x -ge 58 -and $x -le 72 -and $y -ge 48 -and $y -le 76) {
        $idx = 4
      }
      if ($x -ge 78 -and $x -le 92 -and $y -ge 48 -and $y -le 76) {
        $idx = 4
      }
      if ($x -ge 34 -and $x -le 94 -and $y -ge 94 -and $y -le 100) {
        $idx = 5
      }

      $bytes[$y * $stride + $x] = [byte]$idx
    }
  }

  [Runtime.InteropServices.Marshal]::Copy($bytes, 0, $data.Scan0, $bytes.Length)
} finally {
  $bitmap.UnlockBits($data)
}

$bitmap.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
$bitmap.Dispose()

Write-Output "Generated 8bpp indexed PNG: $outPath"
