using System.Runtime.InteropServices;
using Microsoft.UI.Xaml;
using WinRT;

namespace Screeni;

public static class Program
{
    private const string SingleInstanceMutexName = @"Local\Screeni.SingleInstance";

    [DllImport("Microsoft.ui.xaml.dll")]
    private static extern void XamlCheckProcessRequirements();

    [STAThread]
    public static void Main(string[] args)
    {
        using var singleInstanceMutex = new Mutex(
            initiallyOwned: true,
            name: SingleInstanceMutexName,
            createdNew: out var createdNew);
        if (!createdNew)
        {
            return;
        }

        XamlCheckProcessRequirements();
        ComWrappersSupport.InitializeComWrappers();
        Application.Start(_ => new App());
    }
}
