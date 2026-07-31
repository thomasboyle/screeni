param(
    [string]$Configuration = "Release",
    [switch]$SkipInstaller
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$CoreSrc = Join-Path $Root "src\Screeni.Core"
$CoreBuild = Join-Path $Root "build\Screeni.Core"
$AppProj = Join-Path $Root "src\Screeni.App\Screeni.App.csproj"
$PublishDir = Join-Path $Root "artifacts\publish"
$InstallerDir = Join-Path $Root "artifacts\installer"
$Iss = Join-Path $Root "installer\Screeni.iss"

Write-Host "==> Building Screeni.Core ($Configuration)"
cmake -S $CoreSrc -B $CoreBuild -G "Visual Studio 17 2022" -A x64 | Out-Host
cmake --build $CoreBuild --config $Configuration | Out-Host

$CoreDll = Join-Path $CoreBuild "bin\$Configuration\Screeni.Core.dll"
if (-not (Test-Path $CoreDll)) {
    $CoreDll = Join-Path $CoreBuild "$Configuration\Screeni.Core.dll"
}
if (-not (Test-Path $CoreDll)) {
    throw "Screeni.Core.dll not found after build."
}

Write-Host "==> Publishing Screeni.App (trimmed self-contained .NET, framework WASDK)"
if (Test-Path $PublishDir) {
    Remove-Item $PublishDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $PublishDir | Out-Null

dotnet publish $AppProj `
    -c $Configuration `
    -r win-x64 `
    --self-contained true `
    -p:Platform=x64 `
    -p:PublishSingleFile=false `
    -p:PublishTrimmed=true `
    -p:TrimMode=partial `
    -p:DebugType=None `
    -p:DebugSymbols=false `
    -p:WindowsAppSDKSelfContained=false `
    -o $PublishDir | Out-Host

Copy-Item $CoreDll (Join-Path $PublishDir "Screeni.Core.dll") -Force

$AssetsDir = Join-Path $PublishDir "Assets"
New-Item -ItemType Directory -Force -Path $AssetsDir | Out-Null
Copy-Item (Join-Path $Root "src\Screeni.App\Assets\Screeni.ico") (Join-Path $AssetsDir "Screeni.ico") -Force

# Ensure XAML binaries and app PRI are present for unpackaged WinUI.
$xbfDir = Join-Path $Root "src\Screeni.App\obj\x64\$Configuration\net8.0-windows10.0.19041.0"
Copy-Item (Join-Path $xbfDir "App.xbf") $PublishDir -Force -ErrorAction SilentlyContinue
Copy-Item (Join-Path $xbfDir "MainWindow.xbf") $PublishDir -Force -ErrorAction SilentlyContinue
$builtPri = Join-Path $Root "src\Screeni.App\bin\x64\$Configuration\net8.0-windows10.0.19041.0\win-x64\resources.pri"
if (-not (Test-Path $builtPri)) {
    $builtPri = Join-Path $Root "src\Screeni.App\bin\x64\$Configuration\net8.0-windows10.0.19041.0\resources.pri"
}
if (Test-Path $builtPri) {
    Copy-Item $builtPri $PublishDir -Force
}

if ($SkipInstaller) {
    Write-Host "Publish complete: $PublishDir"
    exit 0
}

$Iscc = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup 6\ISCC.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $Iscc) {
    throw "Inno Setup 6 (ISCC.exe) not found. Install from https://jrsoftware.org/isinfo.php"
}

New-Item -ItemType Directory -Force -Path $InstallerDir | Out-Null
Write-Host "==> Compiling Inno Setup installer"
& $Iscc /DPublishDir="$PublishDir" $Iss | Out-Host

$Setup = Get-ChildItem $InstallerDir -Filter "ScreeniSetup-*.exe" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $Setup) {
    throw "Installer output not found in $InstallerDir"
}

$installerSize = (Get-Item $Setup).Length
$maxInstallerSize = 20MB
Write-Host ("Installer size: {0:n1} MB" -f ($installerSize / 1MB))
if ($installerSize -gt $maxInstallerSize) {
    throw "Installer exceeds the 20 MB limit: $([math]::Round($installerSize / 1MB, 1)) MB"
}

Write-Host "Installer ready: $($Setup.FullName)"
