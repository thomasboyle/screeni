# Screeni (C++ / Qt)

Pixel-accurate rewrite of Screeni as a **native C++20 + Qt 6 Widgets** application.

## Why this rewrite

| Goal | Approach |
|------|----------|
| **Speed** | Single process, no .NET runtime; tracker + UI in native code |
| **Efficiency** | WinEvent foreground hook + 1s idle poll, SQLite WAL |
| **Minimal install** | Qt Widgets only, size-oriented Release flags, sub-1 MB exe |
| **Pixel fidelity** | Same matcha palette, 1024x764 window, Monocraft font |

## Features

- Foreground-app usage tracking with idle threshold (default 60s)
- SQLite store at `%LOCALAPPDATA%\Screeni\usage.db`
- Overview (Day/Week charts + top apps) and Insights (week stats, peak hour, time-of-day, 14-day trend)
- System tray, start with Windows, close-to-tray, Matcha/Lilac themes
- GitHub-Releases updater with ETag cache, download progress, silent install

## Build

```bat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
cmake --build build --config Release
```

Requires Qt 6.5+ (Widgets, Network), CMake 3.21+, Ninja, and an MSVC environment.
`scripts/publish.ps1` stages a runnable folder (`artifacts/publish`) and builds the
Inno Setup installer; `scripts/ci-release.ps1` is the CI equivalent.

Fonts and the app icon ship as loose files under `Assets/` next to the exe
(deployed by the publish scripts, installed by the installer). The exe falls back
to a system/placeholder icon and the default UI font if they are missing.

## Layout

```
src/Screeni.Core/  Tracker + Store + bundled SQLite (plain C/C++, no Qt)
src/ui/            MainWindow, Overview, Insights, Settings, BarChart
src/services/      Tray, autostart, updater
src/AppIcon.h      Loose-file icon loader with fallbacks
scripts/           publish.ps1 (local), ci-release.ps1 (CI)
installer/         Inno Setup script (Screeni.iss)
tools/             Icon/font/asset helpers (dev only)
```