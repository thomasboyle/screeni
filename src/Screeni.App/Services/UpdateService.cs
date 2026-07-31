using System.Diagnostics;
using System.Net.Http.Headers;
using System.Text.Json;

namespace Screeni.Services;

public sealed class UpdateService
{
    private const string LatestReleaseUrl = "https://api.github.com/repos/thomasboyle/screeni/releases/latest";
    private static readonly HttpClient Http = CreateHttpClient();
    private static readonly Version CurrentVersion = GetCurrentVersion();

    public async Task<AvailableUpdate?> CheckAsync(CancellationToken cancellationToken = default)
    {
        using var response = await Http.GetAsync(LatestReleaseUrl, cancellationToken);
        response.EnsureSuccessStatusCode();
        await using var stream = await response.Content.ReadAsStreamAsync(cancellationToken);
        using var document = await JsonDocument.ParseAsync(stream, cancellationToken: cancellationToken);
        var root = document.RootElement;

        var tag = root.GetProperty("tag_name").GetString();
        if (!TryParseVersion(tag, out var version) || version <= CurrentVersion)
        {
            return null;
        }

        var installerName = $"ScreeniSetup-{version}.exe";
        foreach (var asset in root.GetProperty("assets").EnumerateArray())
        {
            if (!string.Equals(asset.GetProperty("name").GetString(), installerName, StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            var downloadUrl = asset.GetProperty("browser_download_url").GetString();
            if (!string.IsNullOrWhiteSpace(downloadUrl))
            {
                return new AvailableUpdate(version, version.ToString(3), downloadUrl);
            }
        }

        return null;
    }

    public async Task<string> DownloadInstallerAsync(
        AvailableUpdate update,
        CancellationToken cancellationToken = default)
    {
        var installerPath = Path.Combine(
            Path.GetTempPath(),
            $"ScreeniSetup-{update.VersionText}.exe");
        var temporaryPath = installerPath + ".download";

        try
        {
            using var response = await Http.GetAsync(
                update.DownloadUrl,
                HttpCompletionOption.ResponseHeadersRead,
                cancellationToken);
            response.EnsureSuccessStatusCode();
            await using var source = await response.Content.ReadAsStreamAsync(cancellationToken);
            await using var target = File.Create(temporaryPath);
            await source.CopyToAsync(target, cancellationToken);
            File.Move(temporaryPath, installerPath, true);
            return installerPath;
        }
        finally
        {
            if (File.Exists(temporaryPath))
            {
                File.Delete(temporaryPath);
            }
        }
    }

    public static void StartInstaller(string installerPath)
    {
        if (Process.Start(new ProcessStartInfo
        {
            FileName = installerPath,
            Arguments = "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP-",
            UseShellExecute = true,
            WorkingDirectory = Path.GetDirectoryName(installerPath) ?? AppContext.BaseDirectory
        }) is null)
        {
            throw new InvalidOperationException("Could not start the update installer.");
        }
    }

    private static HttpClient CreateHttpClient()
    {
        var client = new HttpClient();
        client.DefaultRequestHeaders.UserAgent.Add(new ProductInfoHeaderValue("Screeni", CurrentAssemblyVersionText()));
        client.DefaultRequestHeaders.Accept.Add(new MediaTypeWithQualityHeaderValue("application/vnd.github+json"));
        return client;
    }

    private static Version GetCurrentVersion()
    {
        var version = typeof(UpdateService).Assembly.GetName().Version;
        return version is null ? new Version(0, 0, 0) : new Version(version.Major, version.Minor, version.Build < 0 ? 0 : version.Build);
    }

    private static string CurrentAssemblyVersionText() => GetCurrentVersion().ToString(3);

    private static bool TryParseVersion(string? value, out Version version)
    {
        version = new Version();
        if (string.IsNullOrWhiteSpace(value))
        {
            return false;
        }

        var normalized = value.Trim();
        if (normalized.StartsWith('v'))
        {
            normalized = normalized[1..];
        }

        if (Version.TryParse(normalized, out var parsed) && parsed is not null)
        {
            version = parsed;
            return true;
        }

        return false;
    }
}

public sealed record AvailableUpdate(Version Version, string VersionText, string DownloadUrl);
