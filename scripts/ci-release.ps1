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

[System.IO.File]::WriteAllText(
    (Join-Path $Root "installer\ci-version.iss"),
    "#define MyAppVersion `"$Version`"`r`n")

# Qt install directory. install-qt-action exports QT_DIR (or Qt6_DIR / QT_ROOT_DIR
# depending on version); fall back to a local dev install inside the repo.
$QtDir = $env:QT_DIR
if (-not $QtDir) { $QtDir = $env:Qt6_DIR }
if (-not $QtDir) { $QtDir = $env:QT_ROOT_DIR }
if (-not $QtDir) {
    foreach ($probe in @(
        (Join-Path $Root "6.8.3\msvc2022_64"),
        "C:\Qt\6.8.3\msvc2022_64"
    )) {
        if (Test-Path $probe) { $QtDir = $probe; break }
    }
}
if (-not $QtDir -or -not (Test-Path $QtDir)) {
    throw "QT_DIR not set and no local Qt found. Set QT_DIR to a Qt 6 msvc2022_64 install."
}
Write-Host "==> Using Qt at: $QtDir"

# Normalise to forward slashes: cmake treats backslashes in -D strings as escapes.
$QtPrefix = $QtDir.Replace('\', '/')

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
# Configure fresh each run: a cached compiler/toolchain from a different
# environment (e.g. a local run under a different vcvars) would otherwise mix
# with the active INCLUDE/LIB paths and break the build. CI has no stale cache
# anyway (fresh workspace), so removing it costs nothing there.
Remove-Item (Join-Path $BuildDir "CMakeCache.txt") -ErrorAction SilentlyContinue
cmake -S $Root -B $BuildDir -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_PREFIX_PATH=$QtPrefix" `
    "-DSCREENI_VERSION=$Version"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
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
# --no-compiler-runtime: skip the ~24 MB vc_redist.x64.exe; the smaller CRT DLLs
# the binaries actually import are copied below instead.
& $Windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw --no-compiler-runtime $PublishDir
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

# Ship just the VC runtime DLLs the binaries import (~1.2 MB) instead of the
# vc_redist.x64.exe installer (~24 MB). Qt6*.dll import msvcp140/msvcp140_1/
# msvcp140_2/vcruntime140/vcruntime140_1; the api-ms-win-crt-* dlls are part of
# Windows 10+. The redist subfolder is Microsoft.VC143.CRT on older toolsets and
# Microsoft.VC145.CRT on the newest (VS 2026) ones, with identical DLL names.
$Crts = @("msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll", "vcruntime140.dll", "vcruntime140_1.dll")
$CrtDir = $null
if ($env:VCToolsRedistDir) {
    $CrtDir = (Get-ChildItem (Join-Path $env:VCToolsRedistDir "x64\Microsoft.VC14?.CRT") -Directory -ErrorAction SilentlyContinue | Select-Object -Last 1).FullName
}
if (-not $CrtDir) {
    $CrtDir = (Get-ChildItem "C:\Program Files\Microsoft Visual Studio\*\*\VC\Redist\MSVC\*\x64\Microsoft.VC14?.CRT" -Directory -ErrorAction SilentlyContinue | Select-Object -Last 1).FullName
}
if ($CrtDir) {
    foreach ($crt in $Crts) {
        $src = Join-Path $CrtDir $crt
        if (Test-Path $src) { Copy-Item $src $PublishDir -Force }
    }
} else {
    Write-Warning "VCToolsRedistDir not found - VC runtime DLLs not copied to publish dir"
}

# The installed app runs without the VC redistributable, so these DLLs are
# mandatory. Fail the release rather than ship an installer that won't start.
$missingCrts = $Crts | Where-Object { -not (Test-Path (Join-Path $PublishDir $_)) }
if ($missingCrts) {
    throw "Missing VC runtime DLL(s) in publish dir: $($missingCrts -join ', ')"
}

# Prune Qt runtime files this Widgets-only app never loads. dxcompiler/dxil are
# the D3D shader compiler used by the Qt Quick RHI; the SVG stack, touch and
# network-information plugins are all unused (verified against the app's
# runtime module list).
$Prune = @(
    "dxcompiler.dll", "dxil.dll", "Qt6Svg.dll",
    "imageformats\qgif.dll", "imageformats\qjpeg.dll", "imageformats\qsvg.dll",
    "iconengines", "generic", "networkinformation"
)
foreach ($rel in $Prune) {
    $p = Join-Path $PublishDir $rel
    if (Test-Path $p) { Remove-Item $p -Recurse -Force }
}

$AssetsDir = Join-Path $PublishDir "Assets"
New-Item -ItemType Directory -Force -Path $AssetsDir | Out-Null
Copy-Item (Join-Path $Root "src\Screeni.App\Assets\Screeni.ico") (Join-Path $AssetsDir "Screeni.ico") -Force

$publishSize = (Get-ChildItem $PublishDir -Recurse -File | Measure-Object Length -Sum).Sum
Write-Host ("Publish size: {0:n1} MB" -f ($publishSize / 1MB))
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