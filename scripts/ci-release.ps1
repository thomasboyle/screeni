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
$Redist = Join-Path $Root "installer\redist\WindowsAppRuntimeInstall-x64.exe"
$RedistUrl = "https://aka.ms/windowsappsdk/1.6/1.6.250602001/windowsappruntimeinstall-x64.exe"

"#define MyAppVersion `"$Version`"" | Set-Content (Join-Path $Root "installer\ci-version.iss") -Encoding utf8NoBOM

# Kick off tool install in parallel with the native build when needed.
$chocoJob = $null
$needNinja = -not (Get-Command ninja -ErrorAction SilentlyContinue)
$needInno = -not (
    (Test-Path "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe") -or
    (Test-Path "${env:ProgramFiles}\Inno Setup 6\ISCC.exe")
)
if ($needNinja -or $needInno) {
    $pkgs = @()
    if ($needNinja) { $pkgs += "ninja" }
    if ($needInno) { $pkgs += "innosetup" }
    Write-Host "==> Installing tools in background: $($pkgs -join ', ')"
    $chocoJob = Start-Job -ScriptBlock {
        param($Packages)
        choco install @Packages -y --no-progress
        if ($LASTEXITCODE -ne 0) { throw "choco install failed: $LASTEXITCODE" }
    } -ArgumentList (, $pkgs)
}

# Prefetch WASDK redist in parallel when missing.
$redistJob = $null
if (-not (Test-Path $Redist)) {
    New-Item -ItemType Directory -Force -Path (Split-Path $Redist) | Out-Null
    Write-Host "==> Downloading Windows App Runtime redist in background"
    $redistJob = Start-Job -ScriptBlock {
        param($Url, $OutFile)
        Invoke-WebRequest -Uri $Url -OutFile $OutFile
    } -ArgumentList $RedistUrl, $Redist
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

if ($redistJob) {
    Write-Host "==> Waiting for redist download"
    Receive-Job $redistJob -Wait -AutoRemoveJob
    if (-not (Test-Path $Redist)) { throw "Redist download failed" }
}
if ($chocoJob) {
    Write-Host "==> Waiting for tool install"
    Receive-Job $chocoJob -Wait -AutoRemoveJob
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" +
                [System.Environment]::GetEnvironmentVariable("Path", "User")
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
