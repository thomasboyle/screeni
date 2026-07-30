# Generates crisp pixel-art path geometry for the retro UI glyphs.
# Each sprite is authored as an ASCII map; output is XAML path data on a 1-unit-per-pixel grid.

function Convert-MapToPath($lines) {
    $path = ""
    for ($r = 0; $r -lt $lines.Count; $r++) {
        $line = $lines[$r]
        $c = 0
        while ($c -lt $line.Length) {
            if ($line[$c] -eq '#') {
                $start = $c
                while ($c -lt $line.Length -and $line[$c] -eq '#') { $c++ }
                $path += "M$start,${r}H${c}V$($r + 1)H${start}Z"
            }
            else { $c++ }
        }
    }
    return $path
}

function Show-Sprite($name, $lines) {
    Write-Output "=== $name ($($lines[0].Length)x$($lines.Count)) ==="
    $lines | ForEach-Object { Write-Output $_ }
    Write-Output "--- path ---"
    Write-Output (Convert-MapToPath $lines)
    Write-Output ""
}

# Cog: solid hub with eight teeth and a punched centre, sampled from polar maths so it stays symmetric.
$size = 17
$center = ($size - 1) / 2
$gear = @()
for ($y = 0; $y -lt $size; $y++) {
    $line = ""
    for ($x = 0; $x -lt $size; $x++) {
        $dx = $x - $center
        $dy = $y - $center
        $r = [Math]::Sqrt($dx * $dx + $dy * $dy)
        $theta = [Math]::Atan2($dy, $dx)
        $tooth = [Math]::Cos(8 * $theta)
        $solid = ($r -le 5.9) -or ($r -le 8.2 -and $tooth -gt -0.05)
        if ($solid -and $r -gt 2.9) { $line += "#" } else { $line += "." }
    }
    $gear += $line
}
Show-Sprite "gear" $gear

Show-Sprite "caption-minimize" @(
    "........",
    "........",
    "........",
    "........",
    "........",
    "########",
    "########",
    "........")

Show-Sprite "caption-maximize" @(
    "########",
    "########",
    "##....##",
    "##....##",
    "##....##",
    "##....##",
    "########",
    "########")

Show-Sprite "caption-close" @(
    "##....##",
    "###..###",
    ".######.",
    "..####..",
    "..####..",
    ".######.",
    "###..###",
    "##....##")
