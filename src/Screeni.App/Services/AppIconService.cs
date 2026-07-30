using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.WindowsRuntime;

namespace Screeni.Services;

/// <summary>
/// Extracts executable icons for the app list. Results are cached because the shell lookup and the
/// GDI round-trip are far more expensive than the 15 second dashboard refresh cadence.
/// </summary>
public static class AppIconService
{
    private const uint ShgfiIcon = 0x000000100;
    private const uint ShgfiLargeIcon = 0x000000000;
    private const uint ShgfiUseFileAttributes = 0x000000010;
    private const uint FileAttributeNormal = 0x00000080;
    private const int ObjBitmapSize = 32;
    private const int BiRgb = 0;
    private const uint DibRgbColors = 0;

    private static readonly Dictionary<string, ImageSource?> Cache = new(StringComparer.OrdinalIgnoreCase);

    public static ImageSource? GetIcon(string exePath)
    {
        if (string.IsNullOrWhiteSpace(exePath))
        {
            return null;
        }

        if (Cache.TryGetValue(exePath, out var cached))
        {
            return cached;
        }

        ImageSource? icon = null;
        try
        {
            icon = Extract(exePath);
        }
        catch
        {
            // A missing or unreadable executable falls back to the initial-letter tile.
        }

        Cache[exePath] = icon;
        return icon;
    }

    private static ImageSource? Extract(string exePath)
    {
        var info = new ShFileInfo();
        var flags = ShgfiIcon | ShgfiLargeIcon;
        if (!File.Exists(exePath))
        {
            flags |= ShgfiUseFileAttributes;
        }

        if (SHGetFileInfoW(exePath, FileAttributeNormal, ref info, (uint)Marshal.SizeOf<ShFileInfo>(), flags) == IntPtr.Zero
            || info.hIcon == IntPtr.Zero)
        {
            return null;
        }

        try
        {
            return RenderIcon(info.hIcon);
        }
        finally
        {
            DestroyIcon(info.hIcon);
        }
    }

    private static ImageSource? RenderIcon(IntPtr hIcon)
    {
        if (!GetIconInfo(hIcon, out var iconInfo))
        {
            return null;
        }

        try
        {
            if (iconInfo.hbmColor == IntPtr.Zero)
            {
                return null;
            }

            var bitmap = new Bitmap();
            if (GetObjectW(iconInfo.hbmColor, ObjBitmapSize, ref bitmap) == 0 || bitmap.bmWidth <= 0 || bitmap.bmHeight <= 0)
            {
                return null;
            }

            var width = bitmap.bmWidth;
            var height = bitmap.bmHeight;
            var pixels = ReadBitmapPixels(iconInfo.hbmColor, width, height);
            if (pixels is null)
            {
                return null;
            }

            // Monochrome-mask icons carry no alpha channel, so derive it from the mask bitmap.
            if (!HasTransparency(pixels) && iconInfo.hbmMask != IntPtr.Zero)
            {
                ApplyMaskAlpha(pixels, iconInfo.hbmMask, width, height);
            }

            Premultiply(pixels);

            var writeable = new WriteableBitmap(width, height);
            using (var stream = writeable.PixelBuffer.AsStream())
            {
                stream.Write(pixels, 0, pixels.Length);
            }

            return writeable;
        }
        finally
        {
            if (iconInfo.hbmColor != IntPtr.Zero)
            {
                DeleteObject(iconInfo.hbmColor);
            }

            if (iconInfo.hbmMask != IntPtr.Zero)
            {
                DeleteObject(iconInfo.hbmMask);
            }
        }
    }

    private static byte[]? ReadBitmapPixels(IntPtr hBitmap, int width, int height)
    {
        var hdc = GetDC(IntPtr.Zero);
        if (hdc == IntPtr.Zero)
        {
            return null;
        }

        try
        {
            var header = new BitmapInfoHeader
            {
                biSize = (uint)Marshal.SizeOf<BitmapInfoHeader>(),
                biWidth = width,
                biHeight = -height, // negative requests a top-down DIB
                biPlanes = 1,
                biBitCount = 32,
                biCompression = BiRgb
            };

            var pixels = new byte[width * height * 4];
            var copied = GetDIBits(hdc, hBitmap, 0, (uint)height, pixels, ref header, DibRgbColors);
            return copied == 0 ? null : pixels;
        }
        finally
        {
            ReleaseDC(IntPtr.Zero, hdc);
        }
    }

    private static bool HasTransparency(byte[] bgra)
    {
        for (var i = 3; i < bgra.Length; i += 4)
        {
            if (bgra[i] != 0)
            {
                return true;
            }
        }

        return false;
    }

    private static void ApplyMaskAlpha(byte[] bgra, IntPtr hbmMask, int width, int height)
    {
        var mask = ReadBitmapPixels(hbmMask, width, height);
        if (mask is null)
        {
            for (var i = 3; i < bgra.Length; i += 4)
            {
                bgra[i] = 0xFF;
            }

            return;
        }

        // In an icon mask, black means opaque and white means transparent.
        for (var i = 0; i < bgra.Length; i += 4)
        {
            bgra[i + 3] = mask[i] == 0 ? (byte)0xFF : (byte)0x00;
        }
    }

    private static void Premultiply(byte[] bgra)
    {
        for (var i = 0; i < bgra.Length; i += 4)
        {
            var alpha = bgra[i + 3];
            if (alpha == 0xFF)
            {
                continue;
            }

            bgra[i] = (byte)(bgra[i] * alpha / 255);
            bgra[i + 1] = (byte)(bgra[i + 1] * alpha / 255);
            bgra[i + 2] = (byte)(bgra[i + 2] * alpha / 255);
        }
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct ShFileInfo
    {
        public IntPtr hIcon;
        public int iIcon;
        public uint dwAttributes;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
        public string szDisplayName;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 80)]
        public string szTypeName;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct IconInfo
    {
        public int fIcon;
        public int xHotspot;
        public int yHotspot;
        public IntPtr hbmMask;
        public IntPtr hbmColor;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct Bitmap
    {
        public int bmType;
        public int bmWidth;
        public int bmHeight;
        public int bmWidthBytes;
        public ushort bmPlanes;
        public ushort bmBitsPixel;
        public IntPtr bmBits;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct BitmapInfoHeader
    {
        public uint biSize;
        public int biWidth;
        public int biHeight;
        public ushort biPlanes;
        public ushort biBitCount;
        public uint biCompression;
        public uint biSizeImage;
        public int biXPelsPerMeter;
        public int biYPelsPerMeter;
        public uint biClrUsed;
        public uint biClrImportant;
    }

    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr SHGetFileInfoW(string pszPath, uint dwFileAttributes, ref ShFileInfo psfi, uint cbFileInfo, uint uFlags);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetIconInfo(IntPtr hIcon, out IconInfo piconinfo);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool DestroyIcon(IntPtr hIcon);

    [DllImport("user32.dll")]
    private static extern IntPtr GetDC(IntPtr hWnd);

    [DllImport("user32.dll")]
    private static extern int ReleaseDC(IntPtr hWnd, IntPtr hDC);

    [DllImport("gdi32.dll", EntryPoint = "GetObjectW")]
    private static extern int GetObjectW(IntPtr hgdiobj, int cbBuffer, ref Bitmap lpvObject);

    [DllImport("gdi32.dll")]
    private static extern int GetDIBits(IntPtr hdc, IntPtr hbmp, uint uStartScan, uint cScanLines, byte[] lpvBits, ref BitmapInfoHeader lpbi, uint uUsage);

    [DllImport("gdi32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool DeleteObject(IntPtr hObject);
}
