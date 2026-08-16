# Screeni — Dependencies

| Dependency | Use | Cost | Verdict |
|---|---|---|---|
| Qt6 Widgets | entire UI | big, required | keep (sole UI toolkit) |
| Qt6 Network | update check/download | required | keep; use 1 NAM instead of 2 |
| Win32 API | tracker hook, autostart, icons, DWM | platform | keep |
| SQLite (bundled 3.46.1, SQLITE_OMIT-optimized) | persistence | static | keep |
| api.cpp/screeni.h C API | none (not compiled) | dead | remove or mark dead |
| src/Screeni.Core/CMakeLists.txt | orphan build file | dead | remove or keep for standalone core builds — keep, document |
| sessions table | written only | disk growth | keep (data format preservation), note |

## Dedup opportunities (code)
- formatDuration: 3 copies (MainWindow, OverviewPage, InsightsPage) -> 1 shared inline.
- localDayString: 2 copies (OverviewPage, InsightsPage) -> shared.
- OverviewPage signals dayRangeSelected/weekRangeSelected: connected to self, zero listeners -> dead.

## Third-party footprint
- No package manager; no runtime service; single exe + Qt DLLs + CRT DLLs (CI prunes).