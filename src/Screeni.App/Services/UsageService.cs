using System.Collections.Concurrent;
using System.Runtime.InteropServices;
using Screeni.Native;

namespace Screeni.Services;

public sealed class UsageService
{
    private readonly object _gate = new();
    private bool _started;

    public bool Start()
    {
        lock (_gate)
        {
            if (_started)
            {
                return true;
            }

            var ok = ScreeniNative.screeni_start() != 0;
            _started = ok;
            return ok;
        }
    }

    public void Stop()
    {
        lock (_gate)
        {
            if (!_started)
            {
                return;
            }

            ScreeniNative.screeni_stop();
            _started = false;
        }
    }

    public long GetTodayTotalMs() => ScreeniNative.screeni_get_today_total_ms();

    public long[] QueryHourly(DateOnly day)
    {
        var buckets = new long[24];
        ScreeniNative.BucketCallback cb = (index, duration, _) =>
        {
            if (index >= 0 && index < buckets.Length)
            {
                buckets[index] = duration;
            }
        };
        var handle = GCHandle.Alloc(cb);
        try
        {
            ScreeniNative.screeni_query_hourly(day.ToString("yyyy-MM-dd"), cb, IntPtr.Zero);
        }
        finally
        {
            handle.Free();
        }
        return buckets;
    }

    public long[] QueryWeekDays(DateOnly startDay)
    {
        var buckets = new long[7];
        ScreeniNative.BucketCallback cb = (index, duration, _) =>
        {
            if (index >= 0 && index < buckets.Length)
            {
                buckets[index] = duration;
            }
        };
        var handle = GCHandle.Alloc(cb);
        try
        {
            ScreeniNative.screeni_query_week_days(startDay.ToString("yyyy-MM-dd"), cb, IntPtr.Zero);
        }
        finally
        {
            handle.Free();
        }
        return buckets;
    }

    public IReadOnlyList<AppUsage> QueryAppBreakdown(DateOnly startDay, DateOnly endDay)
    {
        var rows = new ConcurrentBag<AppUsage>();
        ScreeniNative.AppCallback cb = (exePtr, namePtr, duration, _) =>
        {
            var exe = Marshal.PtrToStringUni(exePtr) ?? string.Empty;
            var name = Marshal.PtrToStringUni(namePtr) ?? string.Empty;
            rows.Add(new AppUsage(exe, name, duration));
        };
        var handle = GCHandle.Alloc(cb);
        try
        {
            ScreeniNative.screeni_query_app_breakdown(
                startDay.ToString("yyyy-MM-dd"),
                endDay.ToString("yyyy-MM-dd"),
                cb,
                IntPtr.Zero);
        }
        finally
        {
            handle.Free();
        }
        return rows.OrderByDescending(r => r.DurationMs).ToList();
    }

    public int IdleThresholdSec
    {
        get => ScreeniNative.screeni_get_idle_threshold_sec();
        set => ScreeniNative.screeni_set_idle_threshold_sec(value);
    }

    public void ClearData() => ScreeniNative.screeni_clear_data();
}

public sealed record AppUsage(string ExePath, string DisplayName, long DurationMs);
