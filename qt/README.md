# Screeni (C++ / Qt)

Pixel-accurate rewrite of Screeni as a **native C++20 + Qt 6 Widgets** application.

## Why this rewrite

| Goal | Approach |
|------|----------|
| **Speed** | Single process, no .NET runtime; tracker + UI in native code |
| **Efficiency** | WinEvent foreground hook + 1s idle poll, SQLite WAL |
| **Minimal install** | Qt Widgets only, optional static Qt, size-oriented Release flags |
| **Pixel fidelity** | Same matcha palette, 272px sidebar, 1024×764 window, Monocraft |

## Features (parity)

- Foreground-app usage tracking with idle threshold
- SQLite store compatible with `%LOCALAPPDATA%\\Screeni\\usage.db`
- Overview (Day/Week charts + top apps)
- Insights (week stats, peak hour, time-of-day, 14-day trend)
- System tray, start with Windows, settings, close-to-tray

## Build

```bat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\\Qt\\6.7.0\\msvc2022_64
cmake --build build --config Release
```

Requires Qt 6.5+ (Widgets, Network), CMake 3.21+, SQLite3 or `third_party/sqlite` amalgamation.

Copy existing `assets/Screeni.ico` and `assets/Fonts/Monocraft.ttf` for icon/font.

## Layout

```
src/core/      Tracker + Store (ported from Screeni.Core)
src/ui/        MainWindow, Overview, Insights, Settings, BarChart
src/services/  Tray + autostart
resources/     Qt resources
```

WinUI/C# sources under the previous layout remain for reference until this lands.
