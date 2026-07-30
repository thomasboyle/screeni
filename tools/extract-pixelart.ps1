Add-Type -AssemblyName System.Drawing

# Extracts pixel-art sprites from the UI mockup and emits ASCII maps plus XAML path data.
# Usage: powershell -File tools/extract-pixelart.ps1 <mockup.png>

$src = [System.Drawing.Bitmap]::FromFile($args[0])

function Test-Art($x, $y) {
    $px = $src.GetPixel($x, $y)
    return ($px.G -gt $px.B + 18 -and $px.R -lt 210)
}

function Get-Bounds($x0, $y0, $x1, $y1) {
    $minX = $x1; $minY = $y1; $maxX = $x0; $maxY = $y0
    for ($y = $y0; $y -lt $y1; $y++) {
        for ($x = $x0; $x -lt $x1; $x++) {
            if (Test-Art $x $y) {
                if ($x -lt $minX) { $minX = $x }
                if ($x -gt $maxX) { $maxX = $x }
                if ($y -lt $minY) { $minY = $y }
                if ($y -gt $maxY) { $maxY = $y }
            }
        }
    }
    return @($minX, $minY, $maxX, $maxY)
}

function Get-Map($name, $x0, $y0, $x1, $y1, $cell) {
    $b = Get-Bounds $x0 $y0 $x1 $y1
    $cols = [int][Math]::Round((($b[2] - $b[0] + 1) / $cell))
    $rows = [int][Math]::Round((($b[3] - $b[1] + 1) / $cell))
    Write-Output "=== $name bbox=$($b[0]),$($b[1]) - $($b[2]),$($b[3]) grid=${cols}x${rows} ==="
    $lines = @()
    for ($r = 0; $r -lt $rows; $r++) {
        $line = ""
        for ($c = 0; $c -lt $cols; $c++) {
            $hits = 0
            for ($dy = 0; $dy -lt $cell; $dy++) {
                for ($dx = 0; $dx -lt $cell; $dx++) {
                    if (Test-Art ($b[0] + $c * $cell + $dx) ($b[1] + $r * $cell + $dy)) { $hits++ }
                }
            }
            if ($hits * 2 -ge $cell * $cell) { $line += "#" } else { $line += "." }
        }
        $lines += $line
        Write-Output $line
    }

    $path = ""
    for ($r = 0; $r -lt $lines.Count; $r++) {
        $line = $lines[$r]
        $c = 0
        while ($c -lt $line.Length) {
            if ($line[$c] -eq '#') {
                $runStart = $c
                while ($c -lt $line.Length -and $line[$c] -eq '#') { $c++ }
                $path += "M$runStart,${r}H${c}V$($r+1)H${runStart}Z"
            }
            else { $c++ }
        }
    }
    Write-Output "--- path data ($name) ---"
    Write-Output $path
    Write-Output ""
}

Get-Map "sprig" 924 256 990 336 2
Get-Map "appsleaf" 30 466 68 506 2
Get-Map "titleicon" 15 14 36 42 2
Get-Map "gear" 852 84 882 114 2

$src.Dispose()
