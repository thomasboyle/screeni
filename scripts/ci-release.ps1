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

$CoreSrc = Join-Path $Root "src\Screeni.Core"
$CoreBuild = Join-Path $Root "build\Screeni.Core"
$AppProj = Join-Path $Root "src\Screeni.App\Screeni.App.csproj"
$PublishDir = Join-Path $Root "artifacts\publish"
$PublishCache = Join-Path $Root "artifacts\ci-cache\publish"
$InstallerDir = Join-Path $Root "artifacts\installer"
$Iss = Join-Path $Root "installer\Screeni.iss"
$CachedCoreDll = Join-Path $Root "artifacts\ci-cache\Screeni.Core.dll"

"#define MyAppVersion `"$Version`"" | Set-Content (Join-Path $Root "installer\ci-version.iss") -Encoding utf8NoBOM

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
Write-Timing "inno discovery" $sw

# Overlap Screeni.Core build with AOT publish when both miss cache.
$buildProc = $null
$CoreDll = $null
if (Test-Path $CachedCoreDll) {
    Write-Host "==> Using cached Screeni.Core.dll"
    $CoreDll = $CachedCoreDll
    Write-Timing "core (cached dll)" $sw
} else {
    if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
        throw "ninja not found on PATH (expected on windows-latest, as in Latenci CI)"
    }

    Write-Host "==> Starting background Screeni.Core build"
    $coreScript = @"
`$ErrorActionPreference = 'Stop'
cmake -S '$CoreSrc' -B '$CoreBuild' -G Ninja -DCMAKE_BUILD_TYPE=Release
if (`$LASTEXITCODE -ne 0) { exit 1 }
cmake --build '$CoreBuild' --parallel
if (`$LASTEXITCODE -ne 0) { exit 2 }
`$CoreDllFinal = Join-Path '$CoreBuild' 'bin\Screeni.Core.dll'
if (-not (Test-Path `$CoreDllFinal)) { exit 3 }
New-Item -ItemType Directory -Force -Path (Split-Path '$CachedCoreDll') | Out-Null
Copy-Item `$CoreDllFinal '$CachedCoreDll' -Force
exit 0
"@
    $tempScript = Join-Path $env:TEMP "screeni-build-core.ps1"
    Set-Content -Path $tempScript -Value $coreScript -Encoding utf8
    # Start-Process inherits MSVC env from ilammy/msvc-dev-cmd; Start-Job does not.
    $buildProc = Start-Process pwsh -ArgumentList @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $tempScript) -PassThru -NoNewWindow
}

$sw.Restart()
try {
    if (Test-Path (Join-Path $PublishCache "Screeni.App.exe")) {
        Write-Host "==> Using cached publish output"
        if (Test-Path $PublishDir) { Remove-Item $PublishDir -Recurse -Force }
        New-Item -ItemType Directory -Force -Path (Split-Path $PublishDir) | Out-Null
        Copy-Item $PublishCache $PublishDir -Recurse -Force
        Write-Timing "dotnet publish (cached)" $sw
    } else {
        Write-Host "==> Publish Screeni.App"
        if (Test-Path $PublishDir) { Remove-Item $PublishDir -Recurse -Force }
        New-Item -ItemType Directory -Force -Path $PublishDir | Out-Null

        # IlcOptimizationPreference=Size comes from the csproj (faster AOT compile than Speed).
        dotnet publish $AppProj `
            -c Release `
            -r win-x64 `
            --self-contained true `
            -p:Platform=x64 `
            -p:Version=$Version `
            -p:PublishAot=true `
            -p:PublishSingleFile=false `
            -p:PublishTrimmed=true `
            -p:TrimMode=partial `
            -p:WindowsAppSDKSelfContained=false `
            -p:DebugType=None `
            -p:DebugSymbols=false `
            -o $PublishDir
        if ($LASTEXITCODE -ne 0) { throw "dotnet publish failed" }
        Write-Timing "dotnet publish" $sw

        if (Test-Path $PublishCache) { Remove-Item $PublishCache -Recurse -Force }
        New-Item -ItemType Directory -Force -Path (Split-Path $PublishCache) | Out-Null
        Copy-Item $PublishDir $PublishCache -Recurse -Force
    }
} finally {
    if ($buildProc -and -not $buildProc.HasExited) {
        Write-Host "==> Waiting for Screeni.Core build to finish..."
        $buildProc.WaitForExit()
    }
}

if ($buildProc) {
    if ($buildProc.ExitCode -ne 0) {
        throw "Screeni.Core background build failed with exit code $($buildProc.ExitCode)"
    }
    Write-Timing "core build (overlapped)" $sw
}

if (-not $CoreDll) {
    $CoreDll = $CachedCoreDll
}
if (-not (Test-Path $CoreDll)) {
    throw "Screeni.Core.dll not found after build."
}

Copy-Item $CoreDll (Join-Path $PublishDir "Screeni.Core.dll") -Force
$AssetsDir = Join-Path $PublishDir "Assets"
New-Item -ItemType Directory -Force -Path $AssetsDir | Out-Null
Copy-Item (Join-Path $Root "src\Screeni.App\Assets\Screeni.ico") (Join-Path $AssetsDir "Screeni.ico") -Force
Get-ChildItem $PublishDir -Filter *.pdb -Recurse -ErrorAction SilentlyContinue | Remove-Item -Force

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
$maxInstallerSize = 20MB
Write-Host ("Installer size: {0:n1} MB" -f ($installerSize / 1MB))
if ($installerSize -gt $maxInstallerSize) {
    throw "Installer exceeds the 20 MB limit: $([math]::Round($installerSize / 1MB, 1)) MB"
}

Write-Timing "total build" $total
Write-Host "Installer ready: $Setup"
