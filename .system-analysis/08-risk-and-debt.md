# Screeni — Risks & Technical Debt

## Known debt
1. refreshAll() is visibility-blind and page-blind; rebuilds everything every 15s (the single
   biggest inefficiency). FIX in Pass 1.
2. InsightsPage refresh = ~37-40 sqlite statement executions per refresh. FIX in Pass 1 (range queries).
3. OverviewPage icon extraction repeated per refresh; no cache. FIX in Pass 1.
4. week_day_totals = 7 sequential queries. FIX in Pass 1 (single range query).
5. Dead code: api.cpp/screeni.h (uncompiled), 2 connected-but-unlistened signals, duplicated
   formatDuration x3 / localDayString x2. FIX in Pass 1 (remove/consolidate).
6. Two QNetworkAccessManagers for one host. Consolidate.
7. add_usage: exec-string transactions + ostringstream formatting. Micro-optimize in Pass 2.

## Risks to manage while changing
- Data format must stay compatible with existing user DBs (WAL, PKs, AUTOINCREMENT, UPSERT).
  Only additive queries allowed; no schema change.
- Tracker/store concurrency contract: UI never holds store mutex across state_mutex_.
- The uncommitted focus-rect fix (theme.h/MainWindow.cpp) must remain intact.
- Update flow untouched (cannot be validated locally).
- Release/CI pipeline must still build: new bench target must not disturb Screeni.exe packaging.
- Measurement noise: use medians, >=5 runs, same machine/toolchain.

## Open questions
- None blocking; all changes are reversible within the working tree.
- Do not discard the user's uncommitted changes (tracked as part of working tree).