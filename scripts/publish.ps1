param(
    [string]$Configuration = "Release",
    [switch]$SkipInstaller
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$AppVersion = (Get-Content (Join-Path $Root "VERSION") -Raw).Trim()
$BuildDir = Join-Path $Root "build"
$PublishDir = Join-Path $Root "artifacts\publish"
$InstallerDir = Join-Path $Root "artifacts\installer"
$Iss = Join-Path $Root "installer\Screeni.iss"

# Qt install directory: env override, else the local dev install inside the repo.
$QtDir = $env:QT_DIR
if (-not $QtDir) {
    $probe = Join-Path $Root "6.8.3\msvc2022_64"
    if (Test-Path $probe) { $QtDir = $probe }
}
if (-not $QtDir -or -not (Test-Path $QtDir)) {
    throw "QT_DIR not set and no local Qt found at $Root\6.8.3\msvc2022_64"
}

Write-Host "==> Configuring Screeni ($Configuration) with Qt at $QtDir"
$QtPrefix = $QtDir.Replace('\', '/')
Remove-Item (Join-Path $BuildDir "CMakeCache.txt") -ErrorAction SilentlyContinue
cmake -S $Root -B $BuildDir -G Ninja `
    -DCMAKE_BUILD_TYPE=$Configuration `
    -DCMAKE_PREFIX_PATH=$QtPrefix `
    "-DSCREENI_VERSION=$AppVersion"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

Write-Host "==> Building Screeni ($Configuration)"
cmake --build $BuildDir --config $Configuration
if ($LASTEXITCODE -ne 0) { throw "CMake build failed" }

Write-Host "==> Staging publish directory"
if (Test-Path $PublishDir) { Remove-Item $PublishDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $PublishDir | Out-Null
Copy-Item (Join-Path $BuildDir "Screeni.exe") $PublishDir -Force

$Windeployqt = Join-Path $QtDir "bin\windeployqt.exe"
if (-not (Test-Path $Windeployqt)) { throw "windeployqt.exe not found at $Windeployqt" }
& $Windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw $PublishDir
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

$AssetsDir = Join-Path $PublishDir "Assets"
New-Item -ItemType Directory -Force -Path $AssetsDir | Out-Null
Copy-Item (Join-Path $Root "src\Screeni.App\Assets\Screeni.ico") (Join-Path $AssetsDir "Screeni.ico") -Force

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

Write-Host ("Installer size: {0:n1} MB" -f ((Get-Item $Setup).Length / 1MB))
Write-Host "Installer ready: $($Setup.FullName)"