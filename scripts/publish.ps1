param(
    [string]$Configuration = "Release",
    [switch]$SkipInstaller,
    [switch]$ForceReconfigure
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
if ($ForceReconfigure) {
    Remove-Item (Join-Path $BuildDir "CMakeCache.txt") -ErrorAction SilentlyContinue
} else {
    # Reuse the configure cache when the recorded compiler still exists. Dropping
    # the cache forces a slow full reconfigure on every run; cmake regenerates it
    # automatically when the toolchain, Qt path or SCREENI_VERSION changes.
    $cache = Join-Path $BuildDir "CMakeCache.txt"
    $keep = $false
    if (Test-Path $cache) {
        $match = Select-String -Path $cache -Pattern '^CMAKE_CXX_COMPILER:FILEPATH=(.*)$' | Select-Object -First 1
        if ($match) { $keep = Test-Path $match.Matches[0].Groups[1].Value }
    }
    if (-not $keep) { Remove-Item $cache -ErrorAction SilentlyContinue }
}
cmake -S $Root -B $BuildDir -G Ninja `
    "-DCMAKE_BUILD_TYPE=$Configuration" `
    "-DCMAKE_PREFIX_PATH=$QtPrefix" `
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
& $Windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw --no-compiler-runtime $PublishDir
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

# Ship just the VC runtime DLLs the binaries import (~1.2 MB) instead of the
# vc_redist.x64.exe installer (~24 MB) that --no-compiler-runtime skips. Without
# these the installed app won't start on machines lacking the VC redistributable.
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

# Fail rather than ship an installer that won't start.
$missingCrts = $Crts | Where-Object { -not (Test-Path (Join-Path $PublishDir $_)) }
if ($missingCrts) {
    throw "Missing VC runtime DLL(s) in publish dir: $($missingCrts -join ', ')"
}

# Prune Qt runtime files this Widgets-only app never loads. Keeps the publish
# dir (and thus the installer + its compile time) small.
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
Copy-Item (Join-Path $Root "assets\Screeni.ico") (Join-Path $AssetsDir "Screeni.ico") -Force
$FontsDir = Join-Path $AssetsDir "Fonts"
New-Item -ItemType Directory -Force -Path $FontsDir | Out-Null
Copy-Item (Join-Path $Root "assets\Fonts\*.ttf") $FontsDir -Force

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

Write-Host ("Installer size: {0:n1} MB" -f ($Setup.Length / 1MB))
Write-Host "Installer ready: $($Setup.FullName)"