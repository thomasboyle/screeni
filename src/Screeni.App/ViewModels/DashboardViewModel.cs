using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Media;
using Screeni.Services;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Windows.Input;

namespace Screeni.ViewModels;

public enum DashboardRange
{
    Day,
    Week
}

public sealed class DashboardViewModel : INotifyPropertyChanged
{
    private const double ChartPlotHeight = 150;
    private const double DayBarWidth = 13;
    private const double WeekBarWidth = 52;

    private readonly UsageService _usage;
    private readonly DispatcherTimerProxy _timer;
    private string _todayTotalText = "0m";
    private DashboardRange _selectedRange = DashboardRange.Day;
    private bool _startWithWindows;
    private double _idleThresholdSec = 60;
    private string _rangeLabel = "Today";
    private int _barCount = 24;
    private List<AppUsageItem> _apps = [];
    private List<ChartBar> _bars = [];

    public event PropertyChangedEventHandler? PropertyChanged;

    public string TodayTotalText
    {
        get => _todayTotalText;
        private set => SetField(ref _todayTotalText, value);
    }

    public DashboardRange SelectedRange
    {
        get => _selectedRange;
        set
        {
            if (SetField(ref _selectedRange, value))
            {
                Refresh();
            }
        }
    }

    public bool StartWithWindows
    {
        get => _startWithWindows;
        set => SetField(ref _startWithWindows, value);
    }

    public double IdleThresholdSec
    {
        get => _idleThresholdSec;
        set => SetField(ref _idleThresholdSec, value);
    }

    public string RangeLabel
    {
        get => _rangeLabel;
        private set => SetField(ref _rangeLabel, value);
    }

    /// <summary>Column count for the chart layout, so bars never wrap onto a second row.</summary>
    public int BarCount
    {
        get => _barCount;
        private set => SetField(ref _barCount, value);
    }

    public List<AppUsageItem> Apps
    {
        get => _apps;
        private set => SetField(ref _apps, value);
    }

    public List<ChartBar> Bars
    {
        get => _bars;
        private set => SetField(ref _bars, value);
    }

    public ICommand SelectDayCommand { get; }
    public ICommand SelectWeekCommand { get; }
    public ICommand ApplySettingsCommand { get; }
    public ICommand ClearDataCommand { get; }

    public DashboardViewModel(UsageService usage)
    {
        _usage = usage;
        IdleThresholdSec = _usage.IdleThresholdSec;
        StartWithWindows = TrayIconService.GetStartWithWindows();
        SelectDayCommand = new RelayCommand(() => SelectedRange = DashboardRange.Day);
        SelectWeekCommand = new RelayCommand(() => SelectedRange = DashboardRange.Week);
        ApplySettingsCommand = new RelayCommand(ApplySettings);
        ClearDataCommand = new RelayCommand(ClearData);
        _timer = new DispatcherTimerProxy(TimeSpan.FromSeconds(15), Refresh);
        Refresh();
        _timer.Start();
    }

    private void ApplySettings()
    {
        _usage.IdleThresholdSec = (int)IdleThresholdSec;
        TrayIconService.SetStartWithWindows(StartWithWindows);
    }

    private void ClearData()
    {
        _usage.ClearData();
        Refresh();
    }

    public void Refresh()
    {
        var todayMs = _usage.GetTodayTotalMs();
        TodayTotalText = FormatDuration(todayMs);

        var today = DateOnly.FromDateTime(DateTime.Now);
        DateOnly start;
        DateOnly end;
        long[] buckets;
        string[] labels;

        if (SelectedRange == DashboardRange.Day)
        {
            start = today;
            end = today;
            RangeLabel = "Today";
            buckets = _usage.QueryHourly(today);
            labels = Enumerable.Range(0, 24).Select(h => h % 6 == 0 ? $"{h}" : string.Empty).ToArray();
        }
        else
        {
            var delta = ((int)DateTime.Now.DayOfWeek + 6) % 7;
            start = today.AddDays(-delta);
            end = start.AddDays(6);
            RangeLabel = $"{start:MMM d} – {end:MMM d}";
            buckets = _usage.QueryWeekDays(start);
            labels = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"];
        }

        var max = Math.Max(1, buckets.Max());
        var barWidth = SelectedRange == DashboardRange.Day ? DayBarWidth : WeekBarWidth;
        var bars = new List<ChartBar>(buckets.Length);
        for (var i = 0; i < buckets.Length; i++)
        {
            var share = buckets[i] / (double)max;
            bars.Add(new ChartBar(labels[i], Math.Max(3, share * ChartPlotHeight), barWidth, FormatDuration(buckets[i])));
        }
        Bars = bars;
        BarCount = buckets.Length;

        var apps = _usage.QueryAppBreakdown(start, end);
        var total = apps.Sum(a => a.DurationMs);
        var appItems = new List<AppUsageItem>(apps.Count);
        foreach (var app in apps)
        {
            appItems.Add(new AppUsageItem(
                app.DisplayName,
                app.ExePath,
                FormatDuration(app.DurationMs),
                total <= 0 ? 0 : (double)app.DurationMs / total,
                AppIconService.GetIcon(app.ExePath)));
        }
        Apps = appItems.Take(5).ToList();
    }

    public void DisposeTimer() => _timer.Stop();

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

public sealed record AppUsageItem(string Name, string ExePath, string DurationText, double Share, ImageSource? Icon)
{
    public Visibility IconVisibility => Icon is null ? Visibility.Collapsed : Visibility.Visible;

    public Visibility InitialVisibility => Icon is null ? Visibility.Visible : Visibility.Collapsed;

    /// <summary>Single-letter stand-in shown when the executable has no extractable icon.</summary>
    public string Initial => string.IsNullOrWhiteSpace(Name) ? "?" : Name.Trim()[..1].ToUpperInvariant();
}

public sealed record ChartBar(string Label, double BarHeight, double BarWidth, string Tooltip);

internal sealed class DispatcherTimerProxy
{
    private readonly Microsoft.UI.Xaml.DispatcherTimer _timer;

    public DispatcherTimerProxy(TimeSpan interval, Action tick)
    {
        _timer = new Microsoft.UI.Xaml.DispatcherTimer { Interval = interval };
        _timer.Tick += (_, _) => tick();
    }

    public void Start() => _timer.Start();
    public void Stop() => _timer.Stop();
}

internal sealed class RelayCommand(Action execute) : ICommand
{
    public event EventHandler? CanExecuteChanged;
    public bool CanExecute(object? parameter) => true;
    public void Execute(object? parameter) => execute();
    private void OnCanExecuteChanged() => CanExecuteChanged?.Invoke(this, EventArgs.Empty);
}
