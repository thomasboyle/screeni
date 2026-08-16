# Screeni — Resource Model (with hotspot ranking)

| # | Path | Frequency | Cost driver | Impact |
|---|---|---|---|---|
| 1 | refreshAll() both pages | 15s, always | widget rebuild + ~40 sqlite stmts + icon extraction | CPU/disk/shell, even when hidden |
| 2 | OverviewPage app list | per refresh | SHGetFileInfo per app + optional snapshot walk | shell + disk per app |
| 3 | InsightsPage queries | per refresh | ~37 stmt execs (reset/bind/step) | CPU (UI thread) |
| 4 | week_day_totals | per refresh | 7 sequential queries | CPU, lock hold |
| 5 | add_usage | 5s flush | exec("BEGIN/COMMIT") parse, ostringstream per slice | CPU (tracker thread) |
| 6 | tray tip | 15s | native setToolTip even when unchanged | IPC/energy |
| 7 | today_total_ms | 15s + show | 1 stmt | negligible |
| 8 | font/theme/startup | once | Monocraft load + QSS build | fine |
| 9 | icon extraction | per refresh (no cache) | filesystem+shell | see #2 |

## Energy note
Screeni is a background/tray app on the user's machine. The largest avoidable energy cost is
the periodic full UI rebuild (CPU + GPU repaint + disk + shell) while hidden or on the
non-visible page.

## Memory
- Small: widget tree ~ a few MB; QVector<ChartBarData> tiny; icons are the only significant
  cacheable objects (QIcon per app path).
- No leaks spotted (Qt parented objects; QPointer used correctly).

## Disk
- usage.db: daily+hourly totals are the hot tables; sessions grows unbounded (never read).
- update-cache.json written once per check.

## Network
- 1 GitHub API call per launch (force) + recheck every >=6h; ETag 304 short-circuits; download
  only on user click.