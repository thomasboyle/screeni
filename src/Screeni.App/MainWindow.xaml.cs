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

        UpdateBubbleVersionText.Text = update is null ? string.Empty : $"v{update.Version.ToString(3)}";
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
            AppUpdateStatus.Failed => "Retry install",
            _ => "Install update",
        };
    }

    private void OverviewNav_Click(object sender, RoutedEventArgs e) => ShowPage("overview");

    private void InsightsNav_Click(object sender, RoutedEventArgs e) => ShowPage("insights");

    private void ShowPage(string page)
    {
        _activePage = page;

        OverviewPanel.Visibility = page == "overview" ? Visibility.Visible : Visibility.Collapsed;
        InsightsPanel.Visibility = page == "insights" ? Visibility.Visible : Visibility.Collapsed;

        OverviewNav.Background = page == "overview" ? SelectedNavBrush : TransparentBrush;
        InsightsNav.Background = page == "insights" ? SelectedNavBrush : TransparentBrush;

        SetNavForeground(OverviewNav, page == "overview");
        SetNavForeground(InsightsNav, page == "insights");

        if (page == "insights")
        {
            Insights.Refresh();
        }
    }

    private static void SetNavForeground(Button button, bool selected)
    {
        if (button.Content is not StackPanel panel)
        {
            return;
        }

        var brush = selected
            ? (Brush)Application.Current.Resources["NavSelectedForegroundBrush"]
            : (Brush)Application.Current.Resources["InkMutedBrush"];

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

        await App.Current.RequestUpdateInstallAsync();
    }
}
