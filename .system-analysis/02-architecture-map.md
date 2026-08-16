# Screeni — Architecture Map

```
main.cpp
 ├─ QApplication + QLockFile + font + theme + global QSS
 ├─ Tracker (own thread)                 ── Store (SQLite, WAL)
 │    ├─ WinEvent hook (OUTOFCONTEXT)         ├─ apps / sessions / daily_totals / hourly_totals
 │    ├─ 1s idle poll (WM_TIMER)              ├─ 8 prepared statements, 1 mutex
 │    └─ 5s flush of current segment          └─ used by BOTH tracker thread and UI thread
 └─ MainWindow
      ├─ sidebar: nav buttons, today total, update bubble
      ├─ stack: OverviewPage (BarChart + top-5 apps), InsightsPage (stats + BarChart)
      ├─ SettingsPanel overlay (autostart, idle threshold, theme)
      ├─ TrayService (menu: Open/Exit; double-click show)
      ├─ UpdateService (2x QNetworkAccessManager; GitHub API + download; %TEMP% installer)
      └─ refreshTimer_ 15s ─> refreshAll() [overview+insights+tray, even when hidden]
```

## Key invariants
- Store is the single persistence point; tracker thread and UI thread both call it; its mutex
  serializes (fine: sub-ms ops, but ~40 stmts per refresh add up on the UI thread).
- Tracker::stop() joins thread then flushes remainder under state_mutex_ then closes store.
- Window close = hide (tray app); only tray Exit quits.
- `quitting_` guard; `refreshTimer_` never paused while hidden (waste).

## Where the inefficiency concentrates
1. `MainWindow::refreshAll` (15s): unconditional, visibility-blind, rebuild-everything.
2. `InsightsPage::refresh`: ~40 SQLite statement executions (7+7 day queries, 7+14 hourly
   queries) on every refresh.
3. `OverviewPage::refresh`: full widget rebuild + filesystem icon extraction per app per refresh.
4. `Store::week_day_totals`: 7 queries where 1 range query suffices.
5. Dead paths: api.cpp/screeni.h (uncompiled), sessions never read, signals never connected.