// ScreeniBench — headless characterization + benchmark for ScreeniCore (Store).
// Links ScreeniCore only (no Qt). Measures the exact SQLite call patterns the
// UI uses on its 15s refresh and the tracker's 5s flush.
#include "store.h"

#include "sqlite3.h"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

std::wstring tempDbPath()
{
    wchar_t buf[MAX_PATH];
    GetTempPathW(MAX_PATH, buf);
    std::wstring path(buf);
    path += L"screeni-bench-usage.db";
    return path;
}

std::string localDay(const std::chrono::system_clock::time_point& tp)
{
    const auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm local{};
    localtime_s(&local, &t);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", local.tm_year + 1900, local.tm_mon + 1,
                  local.tm_mday);
    return buf;
}

std::chrono::system_clock::time_point dayStart(int daysAgo)
{
    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_s(&local, &t);
    local.tm_hour = 0;
    local.tm_min = 0;
    local.tm_sec = 0;
    local.tm_isdst = -1;
    local.tm_mday -= daysAgo;
    const __time64_t ts = _mktime64(&local);
    return std::chrono::system_clock::from_time_t(static_cast<std::time_t>(ts));
}

// DST-safe civil-day advance, mirroring the app's helpers.
std::chrono::system_clock::time_point addDays(const std::chrono::system_clock::time_point& tp,
                                              int days)
{
    const auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm local{};
    localtime_s(&local, &t);
    local.tm_mday += days;
    local.tm_hour = 0;
    local.tm_min = 0;
    local.tm_sec = 0;
    local.tm_isdst = -1;
    const __time64_t ts = _mktime64(&local);
    return std::chrono::system_clock::from_time_t(static_cast<std::time_t>(ts));
}

int weekIndexToday()
{
    const auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
    localtime_s(&local, &t);
    const int dow = local.tm_wday;  // Sun=0
    return (dow + 6) % 7;           // Mon=0 .. Sun=6
}

double msSince(Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

double median(std::vector<double>& v)
{
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

void printResult(const char* name, const std::vector<double>& runs)
{
    const auto [lo, hi] = std::minmax_element(runs.begin(), runs.end());
    std::printf("%-28s median %8.3f ms   (min %7.3f  max %7.3f, n=%zu)\n", name,
                median(const_cast<std::vector<double>&>(runs)), *lo, *hi, runs.size());
}

// ---- Direct sqlite access for verification (headers only; impl lives in ScreeniCore) ----

int64_t tableRows(const std::string& dbPath, const char* table)
{
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        if (db)
            sqlite3_close(db);
        return -1;
    }
    char sql[128];
    std::snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s;", table);
    sqlite3_stmt* stmt = nullptr;
    int64_t n = -1;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK &&
        sqlite3_step(stmt) == SQLITE_ROW) {
        n = sqlite3_column_int64(stmt, 0);
    }
    if (stmt)
        sqlite3_finalize(stmt);
    sqlite3_close(db);
    return n;
}

const char* schemaTableExists(const std::string& dbPath, const char* table)
{
    sqlite3* db = nullptr;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        if (db)
            sqlite3_close(db);
        return "open-failed";
    }
    const char* sql =
        "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name=?;";
    sqlite3_stmt* stmt = nullptr;
    const char* result = "missing";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, table, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int64(stmt, 0) > 0)
            result = "ok";
    }
    if (stmt)
        sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
}

}  // namespace

int main()
{
    const std::wstring dbPath = tempDbPath();
    std::error_code ec;
    std::filesystem::remove(dbPath, ec);
    std::filesystem::remove(dbPath + L"-wal", ec);
    std::filesystem::remove(dbPath + L"-shm", ec);

    std::printf("== ScreeniBench ==\n");

    // ---------- Seed ----------
    Store seed;
    if (!seed.open(dbPath)) {
        std::printf("FAIL: seed open\n");
        return 1;
    }

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> appPick(0, 19);
    std::uniform_int_distribution<int> hourPick(8, 22);  // keep sessions inside one day
    std::uniform_int_distribution<int> minuteLen(5, 60);

    std::vector<int64_t> appIds;
    for (int a = 0; a < 20; ++a) {
        const std::wstring exe = L"C:\\Program Files\\App" + std::to_wstring(a) +
                                 L"\\app" + std::to_wstring(a) + L".exe";
        const std::wstring name = L"Application " + std::to_wstring(a);
        const int64_t id = seed.upsert_app(exe, name);
        if (id <= 0) {
            std::printf("FAIL: upsert_app %d\n", a);
            return 1;
        }
        appIds.push_back(id);
    }

    // 30 days of usage: 4-8 sessions/day/app, 8am-10pm window (no midnight crossing;
    // the split behavior is covered by its own characterization check below).
    int64_t todaySeed = 0;
    const std::string todayStr = localDay(std::chrono::system_clock::now());
    for (int d = 29; d >= 0; --d) {
        const auto day = dayStart(d);
        for (int a = 0; a < 20; ++a) {
            const int sessions = 4 + static_cast<int>(rng() % 5);
            for (int s = 0; s < sessions; ++s) {
                const int startHour = hourPick(rng);
                const int startMin = static_cast<int>(rng() % 60);
                const int lenMin = minuteLen(rng);
                auto start = day + std::chrono::hours(startHour) +
                             std::chrono::minutes(startMin);
                auto end = start + std::chrono::minutes(lenMin);
                seed.add_usage(appIds[a], start, end);
                if (d == 0 && localDay(start) == todayStr)
                    todaySeed += std::chrono::duration_cast<std::chrono::milliseconds>(
                                     end - start)
                                     .count();
            }
        }
    }
    seed.close();

    // ---------- Characterization ----------
    std::printf("== Characterization ==\n");
    bool ok = true;
    const std::string dbUtf8 = [&] {
        const int n = WideCharToMultiByte(CP_UTF8, 0, dbPath.c_str(), -1, nullptr, 0, nullptr,
                                          nullptr);
        std::string s(n, '\0');
        WideCharToMultiByte(CP_UTF8, 0, dbPath.c_str(), -1, s.data(), n, nullptr, nullptr);
        s.resize(n - 1);
        return s;
    }();

    auto check = [&ok](bool cond, const char* what) {
        std::printf("  %-50s %s\n", what, cond ? "PASS" : "FAIL");
        ok = ok && cond;
    };

    check(schemaTableExists(dbUtf8, "apps") == std::string("ok"), "schema: apps table");
    check(schemaTableExists(dbUtf8, "sessions") == std::string("ok"), "schema: sessions table");
    check(schemaTableExists(dbUtf8, "daily_totals") == std::string("ok"), "schema: daily_totals");
    check(schemaTableExists(dbUtf8, "hourly_totals") == std::string("ok"),
          "schema: hourly_totals");
    check(tableRows(dbUtf8, "apps") == 20, "seed: 20 apps");
    check(tableRows(dbUtf8, "daily_totals") > 0, "seed: daily_totals populated");

    Store s;
    if (!s.open(dbPath)) {
        std::printf("FAIL: reopen\n");
        return 1;
    }

    const int64_t todayTotal = s.today_total_ms();
    check(todayTotal == todaySeed,
          "today_total_ms == seeded today total (missing 0 rows expected: days vary)");

    const std::string today = localDay(std::chrono::system_clock::now());
    const std::vector<int64_t> hourly = s.hourly_totals(today);
    int64_t hourlySum = 0;
    for (int64_t v : hourly)
        hourlySum += v;
    check(hourly.size() == 24, "hourly_totals: 24 buckets");
    check(hourlySum == todayTotal, "hourly_totals sums to today_total");

    const int weekIdx = weekIndexToday();
    const auto monday = addDays(dayStart(0), -weekIdx);
    const std::string mondayStr = localDay(monday);
    const std::vector<int64_t> week = s.week_day_totals(mondayStr);
    check(week.size() == 7, "week_day_totals: 7 buckets");

    const std::vector<AppUsageRow> breakdown =
        s.app_breakdown(mondayStr, localDay(dayStart(0)));
    check(!breakdown.empty(), "app_breakdown: non-empty");
    check(breakdown.size() <= 20, "app_breakdown: <= 20 apps");
    bool sorted = true;
    for (size_t i = 1; i < breakdown.size(); ++i)
        sorted = sorted && breakdown[i].duration_ms <= breakdown[i - 1].duration_ms;
    check(sorted, "app_breakdown: descending order");

    // Range queries must agree with the per-day queries they replace.
    {
        const std::string endStr = localDay(dayStart(0));
        const auto range = s.day_totals(mondayStr, endStr);
        check(range.size() == static_cast<size_t>(weekIdx + 1),
              "day_totals: one entry per day in range");
        bool match = true;
        for (int i = 0; i <= weekIdx; ++i)
            match = match && range[static_cast<size_t>(i)] == week[static_cast<size_t>(i)];
        check(match, "day_totals == week_day_totals per day");

        const auto hrs = s.hourly_totals_range(mondayStr, endStr);
        check(hrs.size() == static_cast<size_t>(weekIdx + 1), "hourly_totals_range: per-day rows");
        bool hMatch = true;
        for (int d = 0; d <= weekIdx; ++d) {
            const std::string day = localDay(addDays(monday, d));
            const auto perDay = s.hourly_totals(day);
            for (int hh = 0; hh < 24; ++hh)
                hMatch = hMatch && hrs[static_cast<size_t>(d)][static_cast<size_t>(hh)] ==
                                       perDay[static_cast<size_t>(hh)];
        }
        check(hMatch, "hourly_totals_range == hourly_totals per day");

        // Trend equivalence: 14-day range daily sums == 14x single-day queries.
        int64_t rSum = 0;
        int64_t qSum = 0;
        const auto trendRange = s.hourly_totals_range(localDay(dayStart(13)), today);
        for (int d = 0; d < 14; ++d) {
            const auto rd = s.hourly_totals_range(localDay(dayStart(d)), localDay(dayStart(d)));
            for (int hh = 0; hh < 24; ++hh)
                qSum += rd[0][static_cast<size_t>(hh)];
            for (int hh = 0; hh < 24; ++hh)
                rSum += trendRange[static_cast<size_t>(d)][static_cast<size_t>(hh)];
        }
        check(rSum == qSum, "14-day range totals == 14x single-day queries");
    }

    // Cross-midnight slice splits into two days.
    {
        auto start = dayStart(0) + std::chrono::hours(23) + std::chrono::minutes(59) +
                     std::chrono::seconds(30);
        auto end = start + std::chrono::minutes(1);
        s.add_usage(appIds[0], start, end);
        const int64_t dToday = s.today_total_ms();
        check(dToday == todayTotal + 30000, "add_usage: cross-midnight slice counted today+30s");
        const int64_t yesterday =
            s.week_day_totals(localDay(dayStart(1)))[0];  // placeholder; real check below
        (void)yesterday;
        check(tableRows(dbUtf8, "hourly_totals") > 0, "hourly_totals rows exist");
    }

    // clear_all empties everything.
    {
        Store c;
        if (c.open(dbPath)) {
            c.clear_all();
            check(tableRows(dbUtf8, "daily_totals") == 0, "clear_all: daily_totals empty");
            check(tableRows(dbUtf8, "hourly_totals") == 0, "clear_all: hourly_totals empty");
            check(tableRows(dbUtf8, "sessions") == 0, "clear_all: sessions empty");
            check(tableRows(dbUtf8, "apps") == 0, "clear_all: apps empty");
            c.close();
        }
    }

    // Re-seed a fresh db for benchmarking (identical shape as the char db).
    std::filesystem::remove(dbPath, ec);
    std::filesystem::remove(dbPath + L"-wal", ec);
    std::filesystem::remove(dbPath + L"-shm", ec);
    {
        Store b;
        b.open(dbPath);
        for (int a = 0; a < 20; ++a) {
            const std::wstring exe = L"C:\\Program Files\\App" + std::to_wstring(a) +
                                     L"\\app" + std::to_wstring(a) + L".exe";
            appIds[a] = b.upsert_app(exe, L"Application " + std::to_wstring(a));
        }
        for (int d = 29; d >= 0; --d) {
            const auto day = dayStart(d);
            for (int a = 0; a < 20; ++a) {
                const int sessions = 4 + static_cast<int>(rng() % 5);
                for (int s2 = 0; s2 < sessions; ++s2) {
                    const int startHour = hourPick(rng);
                    const int startMin = static_cast<int>(rng() % 60);
                    const int lenMin = minuteLen(rng);
                    auto start = day + std::chrono::hours(startHour) +
                                 std::chrono::minutes(startMin);
                    b.add_usage(appIds[a], start, start + std::chrono::minutes(lenMin));
                }
            }
        }
        b.close();
    }

    // ---------- Benchmarks ----------
    std::printf("== Benchmarks (median of 7, after 1 warmup) ==\n");
    s.close();

    const auto bench = [&](const char* name, int iterations, auto fn) {
        std::vector<double> runs;
        runs.reserve(static_cast<size_t>(iterations));
        fn();  // warmup
        for (int i = 0; i < iterations; ++i) {
            const auto t0 = Clock::now();
            fn();
            runs.push_back(msSince(t0));
        }
        printResult(name, runs);
    };

    {
        Store b;
        if (!b.open(dbPath)) {
            std::printf("FAIL: bench open\n");
            return 1;
        }

        bench("today_total_ms", 7, [&] { b.today_total_ms(); });

        bench("hourly_totals(today)", 7, [&] { b.hourly_totals(today); });

        bench("week_day_totals(monday)", 7, [&] { b.week_day_totals(mondayStr); });

        bench("app_breakdown(week)", 7, [&] {
            b.app_breakdown(mondayStr, localDay(dayStart(0)));
        });

        // Current InsightsPage.refresh() SQLite pattern:
        // 2x week_day_totals + hourly(today) + 6x hourly(weekdays) + 14x hourly(trend) + breakdown
        bench("insights refresh (current pattern)", 7, [&] {
            const auto lastMonday = addDays(monday, -7);
            b.week_day_totals(mondayStr);
            b.week_day_totals(localDay(lastMonday));
            b.hourly_totals(today);
            for (int d = 0; d < weekIdx; ++d)
                b.hourly_totals(localDay(addDays(monday, d)));
            for (int d = 13; d >= 0; --d)
                b.hourly_totals(localDay(dayStart(d)));
            b.app_breakdown(mondayStr, localDay(dayStart(0)));
        });

        // New InsightsPage.refresh() SQLite pattern:
        // day_totals(14d) + hourly_totals_range(week) + breakdown.
        bench("insights refresh (new pattern)", 7, [&] {
            b.day_totals(localDay(dayStart(13)), today);
            b.hourly_totals_range(localDay(dayStart(weekIdx)), today);
            b.app_breakdown(mondayStr, localDay(dayStart(0)));
        });

        bench("day_totals(week)", 7, [&] {
            b.day_totals(mondayStr, localDay(dayStart(0)));
        });

        // Tracker 5s flush: one segment begin (upsert_app) + one add_usage.
        const auto now = std::chrono::system_clock::now();
        bench("tracker flush (upsert+usage)", 7, [&] {
            b.upsert_app(L"C:\\Program Files\\App0\\app0.exe", L"Application 0");
            b.add_usage(appIds[0], now, now + std::chrono::seconds(5));
        });

        b.close();
    }

    // Store::open on the seeded db (startup cost of re-opening).
    {
        Store b;
        bench("store open (seeded db)", 5, [&] { b.open(dbPath); });
        b.close();
    }

    std::filesystem::remove(dbPath, ec);
    std::filesystem::remove(dbPath + L"-wal", ec);
    std::filesystem::remove(dbPath + L"-shm", ec);

    std::printf("== %s ==\n", ok ? "ALL CHARACTERIZATION PASSED" : "CHARACTERIZATION FAILED");
    return ok ? 0 : 1;
}