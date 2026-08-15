param(
    [string]$InputIco,
    [string]$OutputIco,
    [int]$MaxSize = 64
)

# Repackages a PNG-compressed .ico keeping only frames at or below MaxSize.
# The 128/256px frames are the bulk of the file; tray/taskbar/installer use <=64.

$ErrorActionPreference = "Stop"

if (-not $InputIco) { $InputIco = Join-Path $PSScriptRoot "..\assets\Screeni.ico" }
if (-not $OutputIco) { $OutputIco = $InputIco }

$bytes = [System.IO.File]::ReadAllBytes($InputIco)
$count = [BitConverter]::ToUInt16($bytes, 4)

$entries = @()
for ($i = 0; $i -lt $count; $i++) {
    $off = 6 + ($i * 16)
    $w = $bytes[$off]
    $h = $bytes[$off + 1]
    $size = [BitConverter]::ToUInt32($bytes, $off + 8)
    $dataOff = [BitConverter]::ToUInt32($bytes, $off + 12)
    $dim = if ($w -eq 0) { 256 } else { $w }
    if ($dim -le $MaxSize) {
        $entries += [pscustomobject]@{ Width = $w; Height = $h; Size = [int]$size; DataOff = [int]$dataOff }
    }
}

if ($entries.Count -eq 0) { throw "No frames <= $MaxSize found in $InputIco" }

$headerSize = 6
$dirSize = $entries.Count * 16
$dataStart = $headerSize + $dirSize

$ms = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter($ms)

$bw.Write([uint16]0)          # reserved
$bw.Write([uint16]1)          # type: icon
$bw.Write([uint16]$entries.Count)

$cursor = $dataStart
foreach ($e in $entries) {
    $bw.Write([byte]$e.Width)
    $bw.Write([byte]$e.Height)
    $bw.Write([byte]0)        # color count
    $bw.Write([byte]0)        # reserved
    $bw.Write([uint16]1)      # planes
    $bw.Write([uint16]32)     # bit count
    $bw.Write([uint32]$e.Size)
    $bw.Write([uint32]$cursor)
    $cursor += $e.Size
}

foreach ($e in $entries) {
    $bw.Write($bytes, $e.DataOff, $e.Size)
}

$bw.Flush()
[System.IO.File]::WriteAllBytes($OutputIco, $ms.ToArray())
$bw.Dispose(); $ms.Dispose()

$newLen = (Get-Item $OutputIco).Length
Write-Host ("Rewrote {0}: {1} frames kept (<= {2}px), {3:n0} -> {4:n0} bytes" -f $OutputIco, $entries.Count, $MaxSize, $bytes.Length, $newLen)