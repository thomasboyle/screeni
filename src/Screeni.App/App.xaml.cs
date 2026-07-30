using Microsoft.UI.Xaml;
using Screeni.Services;

namespace Screeni;

public partial class App : Application
{
    private Window? _window;
    private TrayIconService? _tray;
    private readonly UsageService _usage = new();
    private bool _exitRequested;

    public UsageService Usage => _usage;
    public static new App Current => (App)Application.Current;

    public App()
    {
        UnhandledException += (_, e) =>
        {
            try
            {
                var path = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                    "Screeni",
                    "crash.log");
                Directory.CreateDirectory(Path.GetDirectoryName(path)!);
                File.AppendAllText(path, $"{DateTime.Now:o}\n{e.Exception}\n\n");
            }
            catch
            {
                // ignore logging failures
            }

            e.Handled = true;
        };

        InitializeComponent();
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        _usage.Start();

        _window = new MainWindow();
        _window.Closed += OnWindowClosed;
        _window.Activate();

        _tray = new TrayIconService("Screeni");
        _tray.ShowRequested += ShowMainWindow;
        _tray.ExitRequested += OnTrayExitRequested;
    }

    private void OnWindowClosed(object sender, WindowEventArgs args)
    {
        if (_exitRequested)
        {
            return;
        }

        args.Handled = true;
        _window?.AppWindow.Hide();
    }

    public void ShowMainWindow()
    {
        if (_window is null)
        {
            return;
        }

        _window.AppWindow.Show();
        _window.Activate();
        if (_window is MainWindow main)
        {
            main.RefreshDashboard();
        }
    }

    private void OnTrayExitRequested()
    {
        // Do not tear down the tray HWND from inside its WindowProc / popup stack.
        var queue = _window?.DispatcherQueue;
        if (queue is not null && queue.TryEnqueue(ExitApp))
        {
            return;
        }

        ExitApp();
    }

    public void ExitApp()
    {
        if (_exitRequested)
        {
            return;
        }

        _exitRequested = true;

        if (_window is MainWindow main)
        {
            main.ViewModel.DisposeTimer();
        }

        _tray?.Dispose();
        _tray = null;
        _usage.Stop();
        _window?.Close();
        Exit();
    }
}
