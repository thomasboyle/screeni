# Screeni — Repository Inventory (Phase 0)

## Branch / state
- Branch: `main`
- Repo: git, origin = https://github.com/thomasboyle/screeni.git
- Uncommitted changes (preserved, user work): `src/theme.h` (+`outline: none` on nav buttons),
  `src/ui/MainWindow.cpp` (removed `Qt::NoFocus` from nav buttons — keyboard-accessibility fix)
- Untracked: `.ai/` (prior analysis), `opencode.json`
- Recent commits: focus-rect fix, SQLITE_STATIC bind perf, core header dedupe, sub-1MB exe
  build work, WinUI removal

## Project purpose
Windows screen-time tracker: WinEvent foreground hook -> SQLite -> Qt Widgets dashboard
(tray app, autostart, 2 themes, GitHub-Releases self-update via Inno installer).

## File inventory by category
| Category | Files |
|---|---|
| Runtime C++ (app) | src/main.cpp, src/pch.h, src/theme.h, src/AppIcon.h |
| UI (Qt Widgets) | src/ui/MainWindow.{h,cpp}, OverviewPage, InsightsPage, SettingsPanel, BarChart |
| Services | src/services/TrayService, Autostart, UpdateService |
| Core (plain C++, no Qt) | src/Screeni.Core/src/store.{h,cpp}, tracker.{h,cpp} |
| Core (dead: not compiled) | src/Screeni.Core/src/api.cpp, include/screeni.h |
| Bundled dep | src/Screeni.Core/third_party/sqlite/sqlite3.{h,c} (3.46.1 amalgamation) |
| Build | CMakeLists.txt, src/Screeni.Core/CMakeLists.txt (orphan), src/app.rc |
| CI/CD | .github/workflows/release.yml |
| Scripts | scripts/publish.ps1, scripts/ci-release.ps1, tools/*.ps1 |
| Installer | installer/Screeni.iss, installer/ci-version.iss |
| Docs | README.md (1 line), qt/README.md |
| Assets | assets/Screeni.ico, assets/Fonts/Monocraft*.ttf |
| Version | VERSION |
| Analysis | .ai/*.md (prior session artifacts) |

## Detected
- Language: C++20 (MSVC), C (bundled SQLite)
- Frameworks: Qt 6.8.3 (Widgets + Network, qtbase only), Win32 API, SQLite3
- Build: CMake 3.21+, Ninja generator, MSVC (local: VS18 Insiders 14.51; CI: VS2022 14.44)
- Tests: none (no framework, no test target)
- Benchmarks: none
- Entry point: src/main.cpp -> MainWindow
- Runtime deps: Qt6Widgets/Qt6Network DLLs (windeployqt), VC runtime DLLs, SQLite (static/bundled)
- External services: api.github.com (update check, ETag-cached), github.com release download
- Persistence: %LOCALAPPDATA%\Screeni\usage.db (SQLite WAL), QSettings (theme), AppLocalData/update-cache.json
- Environment/config: no .env; SCREENI_VERSION compile define; settings stored in registry (QSettings)

## Known unknowns
- No automated tests anywhere; CI = build + installer only
- No performance data; all perf claims must be measured by a new harness
- installer/ISCC only exercised in CI
- `usage.db` growth: sessions table written but never read (schema preserves history)
- C API (screeni.h/api.cpp) compiled nowhere in this repo's build

## Build/measure commands (local)
```
vcvars64.bat (VS18 Insiders)  ->  cmake --build build --config Release
```