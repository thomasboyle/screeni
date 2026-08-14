using System.ComponentModel;
using System.Runtime.CompilerServices;
using Screeni.Services;

namespace Screeni.ViewModels;

public sealed class InsightsViewModel : INotifyPropertyChanged
{
    private const double TrendPlotHeight = 96;
    private const long MinuteMs = 60_000;

    private static readonly string[] DayNames = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"];

    private readonly UsageService _usage;

    private string _weekTotalText = "0m";
    private string _dailyAverageText = "0m";
    private string _weekDeltaText = "—";
    private string _weekDeltaDetail = "Compared with the same days last week";
    private string _busiestDayText = "—";
    private string _quietestDayText = "—";
    private string _peakHourText = "—";
    private string _peakHourDetail = "No usage recorded today";
    private string _dominantAppText = "—";
    private string _dominantAppDetail = "No apps tracked this week";
    private string _activeAppsText = "0";
    private string _morningShareText = "0%";
    private string _afternoonShareText = "0%";
    private string _eveningShareText = "0%";
    private string _nightShareText = "0%";
    private double _morningShare;
    private double _afternoonShare;
    private double _eveningShare;
    private double _nightShare;
    private List<TrendBar> _trendBars = [];
    private bool _hasData;

    public event PropertyChangedEventHandler? PropertyChanged;

    public InsightsViewModel(UsageService usage)
    {
        _usage = usage;
        Refresh();
    }

    public string WeekTotalText
    {
        get => _weekTotalText;
        private set => SetField(ref _weekTotalText, value);
    }

    public string DailyAverageText
    {
        get => _dailyAverageText;
        private set => SetField(ref _dailyAverageText, value);
    }

    public string WeekDeltaText
    {
        get => _weekDeltaText;
        private set => SetField(ref _weekDeltaText, value);
    }

    public string WeekDeltaDetail
    {
        get => _weekDeltaDetail;
        private set => SetField(ref _weekDeltaDetail, value);
    }

    public string BusiestDayText
    {
        get => _busiestDayText;
        private set => SetField(ref _busiestDayText, value);
    }

    public string QuietestDayText
    {
        get => _quietestDayText;
        private set => SetField(ref _quietestDayText, value);
    }

    public string PeakHourText
    {
        get => _peakHourText;
        private set => SetField(ref _peakHourText, value);
    }

    public string PeakHourDetail
    {
        get => _peakHourDetail;
        private set => SetField(ref _peakHourDetail, value);
    }

    public string DominantAppText
    {
        get => _dominantAppText;
        private set => SetField(ref _dominantAppText, value);
    }

    public string DominantAppDetail
    {
        get => _dominantAppDetail;
        private set => SetField(ref _dominantAppDetail, value);
    }

    public string ActiveAppsText
    {
        get => _activeAppsText;
        private set => SetField(ref _activeAppsText, value);
    }

    public string MorningShareText
    {
        get => _morningShareText;
        private set => SetField(ref _morningShareText, value);
    }

    public string AfternoonShareText
    {
        get => _afternoonShareText;
        private set => SetField(ref _afternoonShareText, value);
    }

    public string EveningShareText
    {
        get => _eveningShareText;
        private set => SetField(ref _eveningShareText, value);
    }

    public string NightShareText
    {
        get => _nightShareText;
        private set => SetField(ref _nightShareText, value);
    }

    public double MorningShare
    {
        get => _morningShare;
        private set => SetField(ref _morningShare, value);
    }

    public double AfternoonShare
    {
        get => _afternoonShare;
        private set => SetField(ref _afternoonShare, value);
    }

    public double EveningShare
    {
        get => _eveningShare;
        private set => SetField(ref _eveningShare, value);
    }

    public double NightShare
    {
        get => _nightShare;
        private set => SetField(ref _nightShare, value);
    }

    public List<TrendBar> TrendBars
    {
        get => _trendBars;
        private set => SetField(ref _trendBars, value);
    }

    public bool HasData
    {
        get => _hasData;
        private set => SetField(ref _hasData, value);
    }

    public void Refresh()
    {
        var today = DateOnly.FromDateTime(DateTime.Now);
        var weekIndex = ((int)today.DayOfWeek + 6) % 7; // Mon=0 … Sun=6
        var thisMonday = today.AddDays(-weekIndex);
        var lastMonday = thisMonday.AddDays(-7);
        var daysElapsed = weekIndex + 1; // Mon through today inclusive

        var thisWeek = _usage.QueryWeekDays(thisMonday);
        var lastWeek = _usage.QueryWeekDays(lastMonday);

        long thisWeekToDate = 0;
        long lastWeekToDate = 0;
        for (var i = 0; i < daysElapsed; i++)
        {
            thisWeekToDate += thisWeek[i];
            lastWeekToDate += lastWeek[i];
        }

        WeekTotalText = FormatDuration(thisWeekToDate);
        DailyAverageText = FormatDuration(thisWeekToDate / daysElapsed);

        if (lastWeekToDate <= 0 && thisWeekToDate <= 0)
        {
            WeekDeltaText = "—";
            WeekDeltaDetail = "No data for this span last week";
        }
        else if (lastWeekToDate <= 0)
        {
            WeekDeltaText = "new";
            WeekDeltaDetail = "No comparable usage last week";
        }
        else
        {
            var delta = (thisWeekToDate - lastWeekToDate) / (double)lastWeekToDate;
            var pct = (int)Math.Round(delta * 100);
            WeekDeltaText = pct > 0 ? $"+{pct}%" : $"{pct}%";
            WeekDeltaDetail = "Same weekdays vs last week";
        }

        // Busiest / quietest among Mon–today only (future weekdays are still zero).
        var busiestIdx = 0;
        var quietestIdx = 0;
        for (var i = 1; i < daysElapsed; i++)
        {
            if (thisWeek[i] > thisWeek[busiestIdx])
            {
                busiestIdx = i;
            }

            if (thisWeek[i] < thisWeek[quietestIdx])
            {
                quietestIdx = i;
            }
        }

        if (thisWeekToDate <= 0)
        {
            BusiestDayText = "—";
            QuietestDayText = "—";
        }
        else
        {
            BusiestDayText = $"{DayNames[busiestIdx]} · {FormatDuration(thisWeek[busiestIdx])}";
            QuietestDayText = $"{DayNames[quietestIdx]} · {FormatDuration(thisWeek[quietestIdx])}";
        }

        // Peak hour today from hourly buckets.
        var todayHourly = _usage.QueryHourly(today);
        var peakHour = 0;
        for (var h = 1; h < 24; h++)
        {
            if (todayHourly[h] > todayHourly[peakHour])
            {
                peakHour = h;
            }
        }

        if (todayHourly[peakHour] <= 0)
        {
            PeakHourText = "—";
            PeakHourDetail = "No usage recorded today";
        }
        else
        {
            PeakHourText = FormatHourLabel(peakHour);
            PeakHourDetail = FormatDuration(todayHourly[peakHour]) + " in that hour";
        }

        // Time-of-day split: aggregate hourly across Mon–today.
        long morning = 0, afternoon = 0, evening = 0, night = 0;
        for (var d = 0; d < daysElapsed; d++)
        {
            var day = thisMonday.AddDays(d);
            var hourly = d == weekIndex ? todayHourly : _usage.QueryHourly(day);
            for (var h = 0; h < 24; h++)
            {
                var ms = hourly[h];
                if (h >= 5 && h < 12)
                {
                    morning += ms;
                }
                else if (h >= 12 && h < 17)
                {
                    afternoon += ms;
                }
                else if (h >= 17 && h < 22)
                {
                    evening += ms;
                }
                else
                {
                    night += ms;
                }
            }
        }

        var daypartTotal = morning + afternoon + evening + night;
        MorningShare = Share(morning, daypartTotal);
        AfternoonShare = Share(afternoon, daypartTotal);
        EveningShare = Share(evening, daypartTotal);
        NightShare = Share(night, daypartTotal);
        MorningShareText = FormatPercent(MorningShare);
        AfternoonShareText = FormatPercent(AfternoonShare);
        EveningShareText = FormatPercent(EveningShare);
        NightShareText = FormatPercent(NightShare);

        // App concentration this week (Mon–today).
        var apps = _usage.QueryAppBreakdown(thisMonday, today);
        var appTotal = apps.Sum(a => a.DurationMs);
        var active = apps.Count(a => a.DurationMs >= MinuteMs);
        ActiveAppsText = active.ToString();

        if (apps.Count == 0 || appTotal <= 0)
        {
            DominantAppText = "—";
            DominantAppDetail = "No apps tracked this week";
        }
        else
        {
            var top = apps[0];
            var share = (double)top.DurationMs / appTotal;
            DominantAppText = string.IsNullOrWhiteSpace(top.DisplayName) ? "Unknown" : top.DisplayName;
            DominantAppDetail = $"{FormatDuration(top.DurationMs)} · {FormatPercent(share)} of week";
        }

        // 14-day trend (oldest → newest).
        var trend = new List<TrendBar>(14);
        long trendMax = 1;
        var dayTotals = new long[14];
        for (var i = 0; i < 14; i++)
        {
            var day = today.AddDays(-(13 - i));
            var total = _usage.QueryHourly(day).Sum();
            dayTotals[i] = total;
            if (total > trendMax)
            {
                trendMax = total;
            }
        }

        for (var i = 0; i < 14; i++)
        {
            var day = today.AddDays(-(13 - i));
            var total = dayTotals[i];
            var height = Math.Max(2, (total / (double)trendMax) * TrendPlotHeight);
            var label = i == 13 || day.DayOfWeek is DayOfWeek.Monday
                ? day.ToString("d/M")
                : string.Empty;
            trend.Add(new TrendBar(label, height, FormatDuration(total), day.ToString("ddd d MMM")));
        }

        TrendBars = trend;
        HasData = thisWeekToDate > 0 || dayTotals.Any(v => v > 0);
    }

    private static double Share(long part, long total) =>
        total <= 0 ? 0 : Math.Clamp(part / (double)total, 0, 1);

    private static string FormatPercent(double share) =>
        $"{(int)Math.Round(share * 100)}%";

    private static string FormatHourLabel(int hour)
    {
        if (hour == 0)
        {
            return "12am";
        }

        if (hour == 12)
        {
            return "12pm";
        }

        return hour < 12 ? $"{hour}am" : $"{hour - 12}pm";
    }

    private static string FormatDuration(long ms)
    {
        if (ms < 0)
        {
            ms = 0;
        }

        var ts = TimeSpan.FromMilliseconds(ms);
        if (ts.TotalHours >= 1)
        {
            return $"{(int)ts.TotalHours}h {ts.Minutes}m";
        }

        if (ts.TotalMinutes >= 1)
        {
            return $"{(int)ts.TotalMinutes}m";
        }

        if (ms == 0)
        {
            return "0m";
        }

        return $"{Math.Max(1, (int)ts.TotalSeconds)}s";
    }

    private bool SetField<T>(ref T field, T value, [CallerMemberName] string? name = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value))
        {
            return false;
        }

        field = value;
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
        return true;
    }
}

public sealed record TrendBar(string Label, double BarHeight, string DurationText, string Tooltip);
