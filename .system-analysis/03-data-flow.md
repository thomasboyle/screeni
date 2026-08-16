# Screeni — Data Flow

## Write path (tracker thread, every focus change + 5s)
WinEvent/GetForegroundWindow -> hwnd -> GetWindowThreadProcessId -> QueryFullProcessImageName
-> GetFileVersionInfo(FileDescription) -> should_ignore? -> Store::upsert_app (1 stmt, text via
SQLITE_STATIC) -> segment(steady+wall start)
-> every 5s: Store::add_usage -> BEGIN IMMEDIATE (exec) -> insert session -> per hour slice:
format_local_day (ostringstream) + local_hour (2x localtime_s) -> upsert daily + upsert hourly
-> COMMIT (exec)

## Read path (UI thread)
refreshAll (15s) -> today_total_ms (1 stmt)
  -> OverviewPage.refresh: hourly_totals(today) [1 stmt] OR week_day_totals [7 stmts] +
     app_breakdown [1 stmt] -> per-app: SHGetFileInfo (icon, disk+shell) [+ CreateToolhelp32
     snapshot + QueryFullProcessImageName when stored path stale]
  -> InsightsPage.refresh: week_day_totals x2 [14 stmts] + hourly_totals(today) [1] +
     hourly_totals x6 (dayparts) + hourly_totals x14 (trend) + app_breakdown [1]
     = ~37 stmts + widget updates
  -> tray tip update

## Observations
- Read volume is trivial in DB rows but statement-heavy; all reads run on the UI thread under
  the Store mutex; hidden-window refreshes produce zero user value.
- Icon extraction is the most expensive UI-side operation (shell + process enumeration) and is
  repeated every 15s with no cache.
- Sessions table grows unbounded; no read path uses it (kept for future/history).