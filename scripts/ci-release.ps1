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
$InstallerDir = Join-Path $Root "artifacts\installer"
$Iss = Join-Path $Root "installer\Screeni.iss"
$CachedCoreDll = Join-Path $Root "artifacts\ci-cache\Screeni.Core.dll"

"#define MyAppVersion `"$Version`"" | Set-Content (Join-Path $Root "installer\ci-version.iss") -Encoding utf8NoBOM

$needInno = -not (
    (Test-Path "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe") -or
    (Test-Path "${env:ProgramFiles}\Inno Setup 6\ISCC.exe")
)
$chocoJob = $null
if ($needInno) {
    Write-Host "==> Installing Inno Setup in background"
    $chocoJob = Start-Job -ScriptBlock {
        & choco install innosetup -y --no-progress
        if ($LASTEXITCODE -ne 0) { throw "choco install innosetup failed: $LASTEXITCODE" }
    }
}

Write-Host "==> Restoring NuGet packages in background"
$restoreJob = Start-Job -ScriptBlock {
    param($Proj)
    & dotnet restore $Proj -r win-x64 --verbosity minimal
    if ($LASTEXITCODE -ne 0) { throw "dotnet restore failed: $LASTEXITCODE" }
} -ArgumentList $AppProj

$CoreDll = $null
if (Test-Path $CachedCoreDll) {
    Write-Host "==> Using cached Screeni.Core.dll"
    $CoreDll = $CachedCoreDll
    Write-Timing "core (cached dll)" $sw
} else {
    if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
        throw "ninja not found on PATH (expected on windows-latest, as in Latenci CI)"
    }

    Write-Host "==> Configure Screeni.Core (Ninja)"
    cmake -S $CoreSrc -B $CoreBuild -G Ninja -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
    Write-Timing "cmake configure" $sw

    $sw.Restart()
    Write-Host "==> Build Screeni.Core"
    cmake --build $CoreBuild --parallel
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }
    Write-Timing "core build" $sw

    $CoreDll = Join-Path $CoreBuild "bin\Screeni.Core.dll"
    if (-not (Test-Path $CoreDll)) {
        throw "Screeni.Core.dll not found after build."
    }

    New-Item -ItemType Directory -Force -Path (Split-Path $CachedCoreDll) | Out-Null
    Copy-Item $CoreDll $CachedCoreDll -Force
}

Write-Host "==> Waiting for NuGet restore"
Receive-Job $restoreJob -Wait -AutoRemoveJob

$sw.Restart()
Write-Host "==> Publish Screeni.App"
if (Test-Path $PublishDir) {
    Remove-Item $PublishDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $PublishDir | Out-Null

dotnet publish $AppProj `
    -c Release `
    -r win-x64 `
    --self-contained true `
    --no-restore `
    -p:Platform=x64 `
    -p:PublishSingleFile=false `
    -p:WindowsAppSDKSelfContained=false `
    -p:DebugType=None `
    -p:DebugSymbols=false `
    -o $PublishDir
if ($LASTEXITCODE -ne 0) { throw "dotnet publish failed" }
Write-Timing "dotnet publish" $sw

Copy-Item $CoreDll (Join-Path $PublishDir "Screeni.Core.dll") -Force
$AssetsDir = Join-Path $PublishDir "Assets"
New-Item -ItemType Directory -Force -Path $AssetsDir | Out-Null
Copy-Item (Join-Path $Root "src\Screeni.App\Assets\Screeni.ico") (Join-Path $AssetsDir "Screeni.ico") -Force
Get-ChildItem $PublishDir -Filter *.pdb -Recurse -ErrorAction SilentlyContinue | Remove-Item -Force

if ($chocoJob) {
    Write-Host "==> Waiting for Inno Setup install"
    Receive-Job $chocoJob -Wait -AutoRemoveJob
}

$Iscc = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup 6\ISCC.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $Iscc) {
    throw "Inno Setup 6 (ISCC.exe) not found."
}

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

Write-Timing "total build" $total
Write-Host "Installer ready: $Setup"
