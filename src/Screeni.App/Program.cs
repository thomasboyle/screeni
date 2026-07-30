using System.Runtime.InteropServices;
using Microsoft.UI.Xaml;
using WinRT;

namespace Screeni;

public static class Program
{
    [DllImport("Microsoft.ui.xaml.dll")]
    private static extern void XamlCheckProcessRequirements();

    [STAThread]
    public static void Main(string[] args)
    {
        XamlCheckProcessRequirements();
        ComWrappersSupport.InitializeComWrappers();
        Application.Start(_ => new App());
    }
}
