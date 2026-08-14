using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Input;
using Screeni.Services;
using Screeni.ViewModels;
using Windows.Graphics;

namespace Screeni;

public sealed partial class MainWindow : Window
{
    private UpdateService? _updateService;

    public DashboardViewModel ViewModel { get; }

    public MainWindow()
    {
        ViewModel = new DashboardViewModel(App.Current.Usage);
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

    public void RefreshDashboard() => ViewModel.Refresh();

    private void MainWindow_Activated(object sender, WindowActivatedEventArgs args)
    {
        if (args.WindowActivationState == WindowActivationState.Deactivated)
        {
            return;
        }

        RefreshDashboard();

        // Non-forced: UpdateService enforces a multi-hour recheck interval.
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

    private void ClearData_Click(object sender, RoutedEventArgs e) => ViewModel.ClearDataCommand.Execute(null);

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
