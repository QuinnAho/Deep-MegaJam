<#
Converts PNG frames to JPEG so they can go into git (D-066: frames enter git as JPEG only).
Uses System.Drawing (no Python). Resizes to -Width, keeps aspect.
  tools\jpeg.ps1 -Src <folder of PNGs> -Dst <out folder> [-Width 1280] [-Quality 82] [-Prefix '']
  tools\jpeg.ps1 -Files a.png,b.png -Dst <out folder>
#>
param([string]$Src, [string]$Dst, [int]$Width = 1280, [long]$Quality = 82, [string]$Prefix = '', [string[]]$Files)
if (-not $Dst) { Write-Host 'FAIL jpeg: -Dst is required'; exit 1 }
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Force $Dst | Out-Null
$codec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
$ep = New-Object System.Drawing.Imaging.EncoderParameters 1
$ep.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality), $Quality
$list = @()
if ($Files) { foreach ($f in ($Files -split ',')) { if ($f.Trim()) { $list += Get-Item -LiteralPath $f.Trim() } } }
if ($Src) { $list += Get-ChildItem $Src -Filter *.png }
if (-not $list) { Write-Host 'FAIL jpeg: no PNGs given (-Src or -Files)'; exit 1 }
$list | ForEach-Object {
    $img = [System.Drawing.Image]::FromFile($_.FullName)
    $h = [int]([double]$img.Height * $Width / $img.Width)
    $bmp = New-Object System.Drawing.Bitmap $Width, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.DrawImage($img, 0, 0, $Width, $h)
    $out = Join-Path $Dst ($Prefix + $_.BaseName + '.jpg')
    $bmp.Save($out, $codec, $ep)
    $g.Dispose(); $bmp.Dispose(); $img.Dispose()
    "{0} {1}x{2} {3:N0} KB" -f (Split-Path $out -Leaf), $Width, $h, ((Get-Item $out).Length / 1KB)
}
