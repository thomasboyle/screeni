# Screeni — Test & Baseline Model

## Existing tests
None. CI builds + makes installer only. No test framework present.

## Characterization plan (implemented in this session)
Headless console bench exe `bench/ScreeniBench` linking `ScreeniCore` (no Qt) that:
1. Opens a temp DB, verifies schema (tables exist, WAL mode).
2. Seeds ~30 days x 20 apps of realistic usage via the public Store API only.
3. Verifies query semantics:
   - today_total_ms equals seeded value for today
   - hourly_totals: 24 buckets, per-hour sums match seeds, hours sum == daily total
   - week_day_totals: 7 buckets, Mon..Sun alignment
   - app_breakdown: descending totals, rows = apps with >0
   - add_usage cross-day slice (23:59:30 -> 00:00:30) splits into 2 days correctly
   - clear_all empties all tables
4. Benchmarks (median of N>=5):
   - today_total_ms
   - hourly_totals(day)
   - week_day_totals(week)
   - app_breakdown(week)
   - "insights refresh" composite (2x week + 1x hourly + 6x hourly + 14x hourly + breakdown)
   - add_usage single-segment flush (upsert_app + add_usage)
   - startup: Store::open on seeded DB
5. Reports per-op median ms + stmt-count estimate.

## Why headless
The Qt GUI cannot be automated here (no test framework, no offscreen platform guarantees;
Qt6Widgets offscreen plugin is not shipped in the local qtbase). GUI-side improvements
(visibility-gated refresh, icon cache) are measured where possible via static accounting and
labeled ESTIMATED; DB-side improvements are MEASURED.

## Reproduce
```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<qt>
cmake --build build --config Release
build\ScreeniBench.exe   (uses a temp db, prints table)
```