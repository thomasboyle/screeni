# Screeni — Baseline & Measurements (Phase 2)

## Environment
- Windows 11, MSVC 14.51 (VS18 Insiders) x64, Ninja, Release (/O1, /OPT:REF /OPT:ICF)
- Qt 6.8.3 (qtbase, msvc2022_64), bundled SQLite 3.46.1 with repo's SQLITE_OMIT set
- Machine: same dev box for all runs; median of 7 (5 for store open), 1 warmup

## Baseline (HEAD a45d579, before any change)
| Workload | Baseline median |
|---|---:|
| today_total_ms | 0.012 ms |
| hourly_totals(today) | 0.043 ms |
| week_day_totals(monday) | 0.087 ms |
| app_breakdown(week) | 0.092 ms |
| insights refresh (current pattern: 35 stmts) | 1.382 ms |
| tracker flush (upsert_app + add_usage 5s) | 0.189 ms |
| store open (seeded db) | 3.31 ms |
| Screeni.exe size | 914,432 B |
| src LOC (excl. third_party) | 3,161 |

## After Pass 1 (range queries, refresh gating, icon cache)
| Workload | Median |
|---|---:|
| week_day_totals (1 stmt now) | 0.040 ms |
| insights refresh (new pattern: 2 stmts) | 0.848 ms |
| insights refresh (old pattern, unchanged) | 1.258 ms |

## After Pass 2 (split daily/hourly granularity, hash-map row mapping)
| Workload | Median |
|---|---:|
| insights refresh (new pattern: 3 stmts) | 0.452 ms |
| day_totals(week) | 0.043 ms |
| tracker flush | 0.19 ms |
| store open | 3.5 ms |

## Limitations
- GUI path (widget rebuild, icon extraction, tray) not automatable headless: DB-side numbers
  are MEASURED; UI-side gains are ESTIMATED from call-count and operation elimination.
- Seeded DB = 30 days × 20 apps × 4-8 sessions/day (realistic upper bound; most users' DBs
  are smaller).
- Store::open max 235ms outlier = first open after fresh seed (file creation/WAL).

## Reproduce
```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<qt-msvc2022_64>
cmake --build build --config Release
build\ScreeniBench.exe
```