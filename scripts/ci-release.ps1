param(
    [Parameter(Mandatory = $true)]
    [string]$Version
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

function Write-Timing([string]$Label, [Diagnostics.Stopwatch]$Sw) {
    Write-Host ("==> {0}: {1:n1}s" -f $Label, $Sw.Elapsed.TotalSeconds)
}

$total = [Diagnostics.Stopwatch]::StartNew()
$sw = [Diagnostics.Stopwatch]::StartNew()

$BuildDir = Join-Path $Root "build"
$PublishDir = Join-Path $Root "artifacts\publish"
$InstallerDir = Join-Path $Root "artifacts\installer"
$Iss = Join-Path $Root "installer\Screeni.iss"
$Exe = Join-Path $BuildDir "Screeni.exe"

"#define MyAppVersion `"$Version`"" | Set-Content (Join-Path $Root "installer\ci-version.iss") -Encoding utf8NoBOM

# Qt install directory (provided by the workflow via jurplel/install-qt-action).
$QtDir = $env:QT_DIR
if (-not $QtDir) {
    # Fallback: probe for a locally installed Qt like the dev environment.
    $probe = Join-Path $Root "6.8.3\msvc2022_64"
    if (Test-Path $probe) { $QtDir = $probe }
}
if (-not $QtDir -or -not (Test-Path $QtDir)) {
    throw "QT_DIR not set and no local Qt found. Set QT_DIR to a Qt 6 msvc2022_64 install."
}
Write-Host "==> Using Qt at: $QtDir"

# Locate Inno Setup.
$Iscc = @()
$innoPaths = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles(x86)}\Inno Setup\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup\ISCC.exe"
)
foreach ($path in $innoPaths) {
    if (Test-Path $path) { $Iscc += $path }
}
if (-not $Iscc) {
    Write-Host "==> Installing Inno Setup (not pre-installed)"
    & choco install innosetup -y --no-progress
    if ($LASTEXITCODE -ne 0) { throw "choco install innosetup failed: $LASTEXITCODE" }
    $Iscc = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "${env:ProgramFiles}\Inno Setup 6\ISCC.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
} else {
    $Iscc = $Iscc | Select-Object -First 1
    Write-Host "==> Found Inno Setup at: $Iscc"
}

# Configure + build with CMake. Requires an MSVC environment (ilammy/msvc-dev-cmd
# in CI); the vcvars64 env is already active at this point.
$sw.Restart()
Write-Host "==> Configuring CMake"
if (-not (Test-Path (Join-Path $BuildDir "CMakeCache.txt"))) {
    cmake -S $Root -B $BuildDir -G Ninja `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_PREFIX_PATH=$QtDir `
        -DSCREENI_VERSION=$Version
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
} else {
    # Re-run to pick up a version change without a full reconfigure.
    cmake -S $Root -B $BuildDir -DSCREENI_VERSION=$Version
    if ($LASTEXITCODE -ne 0) { throw "CMake reconfigure failed" }
}
Write-Timing "cmake configure" $sw

$sw.Restart()
Write-Host "==> Building Screeni"
cmake --build $BuildDir --config Release
if ($LASTEXITCODE -ne 0) { throw "CMake build failed" }
if (-not (Test-Path $Exe)) { throw "Build output not found: $Exe" }
Write-Timing "cmake build" $sw

# Stage a clean publish directory: exe + windeployqt output + app icon.
$sw.Restart()
Write-Host "==> Staging publish directory"
if (Test-Path $PublishDir) { Remove-Item $PublishDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $PublishDir | Out-Null
Copy-Item $Exe $PublishDir -Force

$Windeployqt = Join-Path $QtDir "bin\windeployqt.exe"
if (-not (Test-Path $Windeployqt)) { throw "windeployqt.exe not found at $Windeployqt" }
& $Windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw $PublishDir
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

$AssetsDir = Join-Path $PublishDir "Assets"
New-Item -ItemType Directory -Force -Path $AssetsDir | Out-Null
Copy-Item (Join-Path $Root "src\Screeni.App\Assets\Screeni.ico") (Join-Path $AssetsDir "Screeni.ico") -Force
Write-Timing "staging + windeployqt" $sw

# Compile installer.
New-Item -ItemType Directory -Force -Path $InstallerDir | Out-Null
Get-ChildItem $InstallerDir -Filter "ScreeniSetup-*.exe" -ErrorAction SilentlyContinue | Remove-Item -Force

$sw.Restart()
Write-Host "==> Compile installer"
& $Iscc /DCI /DPublishDir="$PublishDir" $Iss
if ($LASTEXITCODE -ne 0) { throw "ISCC failed" }
Write-Timing "inno setup" $sw

$Setup = Join-Path $InstallerDir "ScreeniSetup-$Version.exe"
if (-not (Test-Path $Setup)) {
    throw "Installer output not found: $Setup"
}

$installerSize = (Get-Item $Setup).Length
Write-Host ("Installer size: {0:n1} MB" -f ($installerSize / 1MB))

Write-Timing "total build" $total
Write-Host "Installer ready: $Setup"