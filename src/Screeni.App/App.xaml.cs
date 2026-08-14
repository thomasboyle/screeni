using System.Diagnostics;
using System.Runtime.InteropServices;
using Microsoft.UI.Xaml;
using Screeni.Services;

namespace Screeni;

public partial class App : Application
{
    internal const string ShutdownRequestEventName = @"Local\Screeni.ShutdownRequest";
    internal const string ShutdownCompleteEventName = @"Local\Screeni.ShutdownComplete";
    private const ulong TickCountMask = 0xFFFFFFFFUL;
    private const ulong TickCountWrap = 0x100000000UL;
    private const ulong WorkingSetTrimIdleMs = 2000;

    private Window? _window;
    private TrayIconService? _tray;
    private readonly UsageService _usage = new();
    private readonly UpdateService _updates = new();
    private EventWaitHandle? _shutdownRequestEvent;
    private EventWaitHandle? _shutdownCompleteEvent;
    private RegisteredWaitHandle? _shutdownWait;
    private DispatcherTimer? _memoryTrimTimer;
    private int _updateInProgress;
    private bool _exitRequested;

    public UsageService Usage => _usage;
    public UpdateService Updates => _updates;
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
        RegisterShutdownSignal();
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        _usage.Start();

        var mainWindow = new MainWindow();
        _window = mainWindow;
        _window.Closed += OnWindowClosed;
        mainWindow.BindUpdateService(_updates);
        _window.Activate();

        _tray = new TrayIconService("Screeni");
        _tray.ShowRequested += ShowMainWindow;
        _tray.ExitRequested += OnTrayExitRequested;

        // Always revalidate on launch so a release published after the last session is noticed.
        _ = _updates.CheckForUpdatesAsync(force: true);

        TrimWorkingSet();
        _memoryTrimTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(5) };
        _memoryTrimTimer.Tick += (_, _) =>
        {
            if (HasBeenInputIdle(WorkingSetTrimIdleMs))
            {
                TrimWorkingSet();
            }
        };
        _memoryTrimTimer.Start();
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

    private void RegisterShutdownSignal()
    {
        try
        {
            _shutdownRequestEvent = new EventWaitHandle(
                false,
                EventResetMode.ManualReset,
                ShutdownRequestEventName);
            _shutdownCompleteEvent = new EventWaitHandle(
                false,
                EventResetMode.ManualReset,
                ShutdownCompleteEventName);
            _shutdownWait = ThreadPool.RegisterWaitForSingleObject(
                _shutdownRequestEvent,
                OnShutdownRequested,
                null,
                Timeout.Infinite,
                true);
        }
        catch
        {
            _shutdownWait?.Unregister(null);
            _shutdownWait = null;
            _shutdownRequestEvent?.Dispose();
            _shutdownRequestEvent = null;
            _shutdownCompleteEvent?.Dispose();
            _shutdownCompleteEvent = null;
        }
    }

    private void OnShutdownRequested(object? state, bool timedOut)
    {
        if (!timedOut)
        {
            _window?.DispatcherQueue.TryEnqueue(ExitApp);
        }
    }

    public async Task RequestUpdateInstallAsync()
    {
        if (Interlocked.Exchange(ref _updateInProgress, 1) != 0)
        {
            return;
        }

        try
        {
            if (_updates.AvailableUpdate is null
                && _updates.Status is not AppUpdateStatus.Failed)
            {
                return;
            }

            await _updates.DownloadAndInstallAsync().ConfigureAwait(true);
            if (!_exitRequested)
            {
                ExitApp();
            }
        }
        catch
        {
            // Status is already Failed via UpdateService; bubble UI refreshes via StateChanged.
        }
        finally
        {
            if (!_exitRequested)
            {
                Interlocked.Exchange(ref _updateInProgress, 0);
            }
        }
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

        _memoryTrimTimer?.Stop();
        _tray?.Dispose();
        _tray = null;
        _usage.Stop();
        _shutdownCompleteEvent?.Set();
        _window?.Close();
        Exit();
    }

    private static void TrimWorkingSet()
    {
        using var process = Process.GetCurrentProcess();
        EmptyWorkingSet(process.Handle);
    }

    private static bool HasBeenInputIdle(ulong thresholdMs)
    {
        var info = new LastInputInfo { Size = (uint)Marshal.SizeOf<LastInputInfo>() };
        if (!GetLastInputInfo(ref info))
        {
            return false;
        }

        var now = GetTickCount64();
        var last = (now & ~TickCountMask) | info.Time;
        if (last > now)
        {
            last -= TickCountWrap;
        }

        return now - last >= thresholdMs;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct LastInputInfo
    {
        public uint Size;
        public uint Time;
    }

    [DllImport("psapi.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool EmptyWorkingSet(IntPtr hProcess);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetLastInputInfo(ref LastInputInfo plii);

    [DllImport("kernel32.dll")]
    private static extern ulong GetTickCount64();
}
