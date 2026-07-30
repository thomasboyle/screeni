using System.Runtime.InteropServices;

namespace Screeni.Native;

internal static class ScreeniNative
{
    private const string DllName = "Screeni.Core.dll";

    public delegate void BucketCallback(int bucketIndex, long durationMs, IntPtr user);
    public delegate void AppCallback(IntPtr exePath, IntPtr displayName, long durationMs, IntPtr user);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int screeni_start();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern void screeni_stop();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int screeni_is_running();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern long screeni_get_today_total_ms();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern int screeni_query_hourly(string dayYyyyMmDd, BucketCallback cb, IntPtr user);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern int screeni_query_week_days(string startDayYyyyMmDd, BucketCallback cb, IntPtr user);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern int screeni_query_app_breakdown(string startDayYyyyMmDd, string endDayYyyyMmDd, AppCallback cb, IntPtr user);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int screeni_set_idle_threshold_sec(int seconds);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int screeni_get_idle_threshold_sec();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    public static extern int screeni_clear_data();
}
