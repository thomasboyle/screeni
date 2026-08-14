using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Screeni.Services;
using Screeni.ViewModels;
using Windows.Graphics;
using Windows.UI;

namespace Screeni;

public sealed partial class MainWindow : Window
{
    private static readonly SolidColorBrush SelectedNavBrush = new(Color.FromArgb(0xFF, 0xA8, 0xAA, 0x8C));
    private static readonly SolidColorBrush TransparentBrush = new(Color.FromArgb(0x00, 0x00, 0x00, 0x00));

    private UpdateService? _updateService;
    private string _activePage = "overview";

    public DashboardViewModel ViewModel { get; }
    public InsightsViewModel Insights { get; }

    public MainWindow()
    {
        ViewModel = new DashboardViewModel(App.Current.Usage);
        Insights = new InsightsViewModel(App.Current.Usage);
        InitializeComponent();

        Title = "Screeni";
        AppWindow.Resize(new SizeInt32(1024, 764));

        var iconPath = Path.Combine(AppContext.BaseDirectory, "Assets", "Screeni.ico");
        if (File.Exists(iconPath))
        {
            AppWindow.SetIcon(iconPath);
        }

        if (AppWindow.Presenter is OverlappedPresenter presenter)
        {
            presenter.IsResizable = true;
        }

        ViewModel.PropertyChanged += (_, args) =>
        {
            if (args.PropertyName == nameof(DashboardViewModel.BarCount))
            {
                ChartLayout.MaximumRowsOrColumns = ViewModel.BarCount;
            }
        };

        ChartLayout.MaximumRowsOrColumns = ViewModel.BarCount;
        SyncRangeToggles();

        // Code-behind subscription: XAML Click can be dropped under Native AOT / XBF.
        OverviewNav.Click += OverviewNav_Click;
        InsightsNav.Click += InsightsNav_Click;

        ShowPage("overview");

        Activated += MainWindow_Activated;
    }

    public void BindUpdateService(UpdateService updates)
    {
        if (_updateService is not null)
        {
            _updateService.StateChanged -= OnUpdateStateChanged;
        }

        _updateService = updates;
        _updateService.StateChanged += OnUpdateStateChanged;
        RefreshUpdateBubble();
    }

    public void RefreshDashboard()
    {
        ViewModel.Refresh();
        if (_activePage == "insights")
        {
            Insights.Refresh();
        }
    }

    private void MainWindow_Activated(object sender, WindowActivatedEventArgs args)
    {
        if (args.WindowActivationState == WindowActivationState.Deactivated)
        {
            return;
        }

        RefreshDashboard();

        if (_updateService is not null)
        {
            _ = _updateService.CheckForUpdatesAsync();
        }
    }

    private void OnUpdateStateChanged(object? sender, EventArgs e)
    {
        DispatcherQueue.TryEnqueue(RefreshUpdateBubble);
    }

    private void RefreshUpdateBubble()
    {
        if (_updateService is null)
        {
            UpdateBubble.Visibility = Visibility.Collapsed;
            return;
        }

        var status = _updateService.Status;
        var update = _updateService.AvailableUpdate;
        var show = update is not null
            || status is AppUpdateStatus.Downloading or AppUpdateStatus.Installing or AppUpdateStatus.Failed;

        UpdateBubble.Visibility = show ? Visibility.Visible : Visibility.Collapsed;
        if (!show)
        {
            return;
        }

        if (update is not null)
        {
            UpdateBubbleVersionText.Text = $"v{update.Version.ToString(3)}";
        }
        else if (status is AppUpdateStatus.Failed && !string.IsNullOrEmpty(_updateService.LastError))
        {
            UpdateBubbleVersionText.Text = _updateService.LastError!;
        }
        else
        {
            UpdateBubbleVersionText.Text = string.Empty;
        }

        UpdateBubbleProgress.Visibility = status is AppUpdateStatus.Downloading or AppUpdateStatus.Installing
            ? Visibility.Visible
            : Visibility.Collapsed;
        UpdateBubbleProgress.Value = _updateService.DownloadProgress;
        UpdateBubbleDismissButton.IsEnabled = status is not AppUpdateStatus.Downloading and not AppUpdateStatus.Installing;
        UpdateBubbleActionButton.IsEnabled = status is AppUpdateStatus.Available or AppUpdateStatus.Failed;
        UpdateBubbleActionButton.Content = status switch
        {
            AppUpdateStatus.Downloading => $"Downloading {_updateService.DownloadProgress:0}%",
            AppUpdateStatus.Installing => "Installing...",
            AppUpdateStatus.Failed => "Retry",
            _ => "Install update",
        };
    }

    private void OverviewNav_Click(object sender, RoutedEventArgs e) => ShowPage("overview");

    private void InsightsNav_Click(object sender, RoutedEventArgs e) => ShowPage("insights");

    private void ShowPage(string page)
    {
        // Always log entry — proves Click reached the handler.
        try
        {
            var path = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "Screeni",
                "nav.log");
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            File.AppendAllText(path, $"{DateTime.Now:o} ShowPage enter: {page}\n");
        }
        catch
        {
        }

        _activePage = page;
        var showInsights = string.Equals(page, "insights", StringComparison.Ordinal);

        // Nav chrome first — if this runs, Click worked even when content layout fails.
        try
        {
            OverviewNav.Background = showInsights ? TransparentBrush : SelectedNavBrush;
            InsightsNav.Background = showInsights ? SelectedNavBrush : TransparentBrush;
            SetNavForeground(OverviewNav, !showInsights);
            SetNavForeground(InsightsNav, showInsights);
            Title = showInsights ? "Screeni — Insights" : "Screeni — Overview";
        }
        catch (Exception ex)
        {
            LogNav($"nav chrome ({page})", ex);
        }

        try
        {
            OverviewRow.Height = showInsights
                ? new GridLength(0)
                : new GridLength(1, GridUnitType.Star);
            InsightsRow.Height = showInsights
                ? new GridLength(1, GridUnitType.Star)
                : new GridLength(0);

            OverviewPanel.IsHitTestVisible = !showInsights;
            InsightsPanel.IsHitTestVisible = showInsights;
        }
        catch (Exception ex)
        {
            LogNav($"row layout ({page})", ex);
        }

        if (showInsights)
        {
            try
            {
                Insights.Refresh();
            }
            catch (Exception ex)
            {
                LogNav("Insights.Refresh", ex);
            }
        }
    }

    private static void LogNav(string where, Exception ex)
    {
        try
        {
            var path = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "Screeni",
                "nav.log");
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            File.AppendAllText(path, $"{DateTime.Now:o} {where}: {ex}\n");
        }
        catch
        {
        }
    }

    private static void SetNavForeground(Button button, bool selected)
    {
        if (button.Content is not StackPanel panel)
        {
            return;
        }

        foreach (var child in panel.Children)
        {
            switch (child)
            {
                case SymbolIcon icon:
                    icon.Foreground = selected
                        ? (Brush)Application.Current.Resources["NavSelectedForegroundBrush"]
                        : (Brush)Application.Current.Resources["InkMutedBrush"];
                    break;
                case TextBlock text:
                    text.Foreground = selected
                        ? (Brush)Application.Current.Resources["NavSelectedForegroundBrush"]
                        : (Brush)Application.Current.Resources["InkBrush"];
                    break;
            }
        }
    }

    private void DayButton_Click(object sender, RoutedEventArgs e)
    {
        ViewModel.SelectDayCommand.Execute(null);
        SyncRangeToggles();
    }

    private void WeekButton_Click(object sender, RoutedEventArgs e)
    {
        ViewModel.SelectWeekCommand.Execute(null);
        SyncRangeToggles();
    }

    private void SyncRangeToggles()
    {
        DayToggle.IsChecked = ViewModel.SelectedRange == DashboardRange.Day;
        WeekToggle.IsChecked = ViewModel.SelectedRange == DashboardRange.Week;
    }

    private void SettingsNavButton_Click(object sender, RoutedEventArgs e) =>
        SettingsOverlay.Visibility = Visibility.Visible;

    private void CloseSettings_Click(object sender, RoutedEventArgs e) =>
        SettingsOverlay.Visibility = Visibility.Collapsed;

    private void SettingsScrim_Tapped(object sender, TappedRoutedEventArgs e)
    {
        if (ReferenceEquals(e.OriginalSource, SettingsOverlay))
        {
            SettingsOverlay.Visibility = Visibility.Collapsed;
        }
    }

    private void ApplySettings_Click(object sender, RoutedEventArgs e)
    {
        ViewModel.ApplySettingsCommand.Execute(null);
        SettingsOverlay.Visibility = Visibility.Collapsed;
    }

    private void ClearData_Click(object sender, RoutedEventArgs e)
    {
        ViewModel.ClearDataCommand.Execute(null);
        Insights.Refresh();
    }

    private void UpdateBubbleDismissButton_Click(object sender, RoutedEventArgs e)
    {
        _updateService?.DismissAvailableUpdate();
    }

    private async void UpdateBubbleActionButton_Click(object sender, RoutedEventArgs e)
    {
        if (_updateService is null)
        {
            return;
        }

        if (_updateService.Status is AppUpdateStatus.Downloading or AppUpdateStatus.Installing)
        {
            return;
        }

        if (_updateService.AvailableUpdate is null)
        {
            await _updateService.CheckForUpdatesAsync(force: true);
            return;
        }

        await App.Current.RequestUpdateInstallAsync();
    }
}
