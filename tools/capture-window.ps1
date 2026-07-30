# Captures the Screeni window to a PNG so UI changes can be reviewed without a live session.
# Usage: powershell -File tools/capture-window.ps1 <output.png> [processName]

param(
    [Parameter(Mandatory = $true)][string]$OutPath,
    [string]$ProcessName = "Screeni.App",
    # WinUI 3 composition surfaces often come back blank from PrintWindow, so screen is the default.
    [ValidateSet("screen", "printwindow")][string]$Mode = "screen"
)

Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class WindowCapture
{
    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdcBlt, uint nFlags);

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    public static extern bool IsIconic(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern int SetProcessDpiAwarenessContext(IntPtr value);

    public static readonly IntPtr DpiAwarenessContextPerMonitorAwareV2 = (IntPtr)(-4);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }
}
"@

[void][WindowCapture]::SetProcessDpiAwarenessContext([WindowCapture]::DpiAwarenessContextPerMonitorAwareV2)

$proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue |
    Where-Object { $_.MainWindowHandle -ne 0 } |
    Select-Object -First 1

if (-not $proc) {
    Write-Output "NO_WINDOW"
    exit 1
}

$hwnd = $proc.MainWindowHandle
if ([WindowCapture]::IsIconic($hwnd)) {
    [void][WindowCapture]::ShowWindow($hwnd, 9) # SW_RESTORE
}
[void][WindowCapture]::ShowWindow($hwnd, 5) # SW_SHOW
[void][WindowCapture]::SetForegroundWindow($hwnd)
Start-Sleep -Milliseconds 1000

$rect = New-Object WindowCapture+RECT
[void][WindowCapture]::GetWindowRect($hwnd, [ref]$rect)
$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top
Write-Output "window ${width}x${height} at $($rect.Left),$($rect.Top)"

$bmp = New-Object System.Drawing.Bitmap $width, $height
$gfx = [System.Drawing.Graphics]::FromImage($bmp)

if ($Mode -eq "printwindow") {
    $hdc = $gfx.GetHdc()
    [void][WindowCapture]::PrintWindow($hwnd, $hdc, 2)
    $gfx.ReleaseHdc($hdc)
}
else {
    $gfx.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bmp.Size)
}

$gfx.Dispose()

$bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Output "saved $OutPath"
