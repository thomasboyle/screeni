# Screeni — System Understanding (Phase 1)

## 1. Structural model
- `main.cpp`: QApplication, single-instance QLockFile, Monocraft font, theme from QSettings,
  global stylesheet, Tracker start, MainWindow show, exec.
- `Tracker` (core thread): WinEvent foreground hook + 1s idle poll + 5s flush, own message
  window; state under `state_mutex_`; publishes thread id + instance pointer.
- `Store`: SQLite wrapper, one mutex, 8 prepared statements, WAL + synchronous=NORMAL.
- UI: MainWindow (sidebar nav, stacked Overview/Insights, overlay SettingsPanel, tray, update
  bubble), OverviewPage (24h/7d chart + top-5 apps list), InsightsPage (week stats, dayparts,
  14-day trend), BarChart (QPainter), theme.h (header-only palettes + one global QSS).
- Services: TrayService, Autostart (registry Run key), UpdateService (GitHub check/download/install).
- Dead code (not built): `api.cpp` + `screeni.h` C API; `src/Screeni.Core/CMakeLists.txt` orphan.

## 2. Behavioral model
- Startup: lock -> font -> theme -> tracker thread -> window (sidebar + pages) -> 1.5s update check.
- Tracking: foreground change -> segment begin (resolve pid/exe/name, upsert app row) ->
  per-5s flush (BEGIN IMMEDIATE; insert session; per-hour daily+hourly upserts; COMMIT).
  Idle >= threshold -> end segment; idle exit -> rebind.
- UI refresh: 15s `refreshTimer_` -> `refreshAll()` (today total + tray tip + refresh BOTH
  pages unconditionally, even when window is hidden to tray).
- Settings: overlay panel; Apply -> threshold, autostart, theme restyle.
- Update: check(force on launch) -> ETag 304 short-circuit -> cache json -> bubble ->
  download to %TEMP%/ScreeniUpdate -> silent installer.
- Shutdown: tray Exit -> quit; window close -> hide.

## 3. Data model
- SQLite schema: apps(id, exe_path UNIQUE, display_name, first/last_seen), sessions(id, app_id,
  start/end_utc, duration_ms, FK), daily_totals(day,app_id PK, duration_ms), hourly_totals(day,hour,app_id PK).
  Indexes: idx_sessions_time (never queried), idx_daily_day (redundant w/ PK(day,app_id) prefix).
- QSettings: theme. update-cache.json: ETag/LastChecked/Latest/DownloadUrl/Dismissed.
- Conversions: wide<->utf8 via Win32; day strings via ostringstream+put_time (allocating).

## 4. Control-flow model
- Tracker: thread_main -> setup (msg window, hook, timer) -> initial foreground -> GetMessage
  loop; win_event_proc (out-of-context) -> on_foreground -> reconcile_focus(force);
  WM_TIMER 1s -> poll_idle_and_flush -> reconcile_focus(no force).
- reconcile_focus: idle edge or force -> flush+begin; else 5s periodic flush + fg hwnd catch-up.
- Error paths: setup failure -> ready_event + setup_ok_ -> start() joins thread; reply timeouts
  guarded by QPointer; DB statements null-checked; sqlite step errors ignored (best-effort).
- No feature flags, no retries (update check has 6h min-recheck).

## 5. Dependency model
- Qt6 Widgets+Network (UI), Win32 (tracker/autostart/overview icons), SQLite bundled.
- api.cpp/screeni.h: zero consumers. sessions table: written, never read.
- Two QNetworkAccessManager instances where one would serve both flows.

## 6. Resource model (hot spots)
- H1 (highest): `MainWindow::refreshAll()` every 15s regardless of visibility: rebuilds
  OverviewPage widget tree incl. per-app `SHGetFileInfo` icon extraction + process snapshot
  fallback, and full InsightsPage re-query (~35-40 SQLite statement executions) — all wasted
  when hidden to tray or page not visible.
- H2: InsightsPage.refresh: 2x week_day_totals(7 stmts each) + 7 hourly_totals(daypart loop)
  + 14 hourly_totals(trend) + breakdown = ~40 statement execs, each reset/bind/step.
- H3: OverviewPage.refresh: appsList clear+rebuild with new QWidget/QProgressBar per app and
  icon extraction per refresh (no cache).
- H4: Store::week_day_totals = 7 sequential per-day queries (range query possible).
- H5: add_usage uses sqlite3_exec("BEGIN IMMEDIATE;"/"COMMIT;") string parsing per flush; 
  per-slice ostringstream date formatting.
- H6: tray tip set every 15s even when unchanged.
- Energy: background app; periodic full UI rebuild is the dominant waste (GUI repaint + CPU).

## 7. Test model
- No tests. Characterization needed: Store behaviors (open/schema, upsert, add_usage slicing
  incl. DST/cross-day, queries, clear_all). A headless bench exe can both characterize and measure.

## 8. Risk & debt
- UpdateService install flow untestable locally (needs release artifact).
- Tracker concurrency is subtle but sound (atomics + event + mutex; QPointer-free).
- Duplicated formatDuration x3, localDayString x2, ChartBarData build loops.
- Dead C API + orphan CMakeLists.
- 15s hidden refresh keeps the machine busy for zero user value.