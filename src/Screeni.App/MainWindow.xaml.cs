using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Input;
using Screeni.ViewModels;
using Windows.Graphics;

namespace Screeni;

public sealed partial class MainWindow : Window
{
    public DashboardViewModel ViewModel { get; }
    public event Action? UpdateRequested;

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

        Activated += (_, args) =>
        {
            if (args.WindowActivationState != WindowActivationState.Deactivated)
            {
                RefreshDashboard();
            }
        };
    }

    public void RefreshDashboard() => ViewModel.Refresh();

    public void ShowUpdateNotification(string version)
    {
        UpdateVersionText.Text = $"Screeni {version} is ready";
        UpdateMessageText.Text = "Click to download and install";
        UpdateBubble.IsEnabled = true;
        UpdateBubble.Visibility = Visibility.Visible;
    }

    public void ShowUpdateStatus(string message)
    {
        UpdateVersionText.Text = "Updating Screeni...";
        UpdateMessageText.Text = message;
        UpdateBubble.IsEnabled = false;
        UpdateBubble.Visibility = Visibility.Visible;
    }

    public void ShowUpdateError()
    {
        UpdateVersionText.Text = "Update failed";
        UpdateMessageText.Text = "Try again later";
        UpdateBubble.IsEnabled = true;
        UpdateBubble.Visibility = Visibility.Visible;
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

    private void SettingsButton_Click(object sender, RoutedEventArgs e) =>
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

    private void UpdateBubble_Click(object sender, RoutedEventArgs e) => UpdateRequested?.Invoke();
}
