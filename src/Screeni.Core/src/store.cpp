#include "store.h"

#include "sqlite3.h"

#include <Windows.h>

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <sstream>
#include <unordered_map>

namespace {

std::string format_local_day(const std::chrono::system_clock::time_point& tp) {
    const auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm local{};
    localtime_s(&local, &time);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", local.tm_year + 1900, local.tm_mon + 1,
                  local.tm_mday);
    return std::string(buf);
}

int local_hour(const std::chrono::system_clock::time_point& tp) {
    const auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm local{};
    localtime_s(&local, &time);
    return local.tm_hour;
}

int64_t to_unix_ms(const std::chrono::system_clock::time_point& tp) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

std::wstring utf8_to_wide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    std::wstring result(static_cast<size_t>(size > 0 ? size : 0), L'\0');
    if (size > 1) {
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), size);
    }
    result.resize(size > 0 ? static_cast<size_t>(size) - 1 : 0);
    return result;
}

std::string wide_to_utf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(size > 0 ? size : 0), '\0');
    if (size > 1) {
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
    }
    result.resize(size > 0 ? static_cast<size_t>(size) - 1 : 0);
    return result;
}

bool digits_yyyy_mm_dd(const std::string& day) {
    if (day.size() != 10) {
        return false;
    }
    for (size_t i = 0; i < day.size(); ++i) {
        if (i == 4 || i == 7) {
            if (day[i] != '-') {
                return false;
            }
        } else if (!std::isdigit(static_cast<unsigned char>(day[i]))) {
            return false;
        }
    }
    return true;
}

std::optional<std::chrono::system_clock::time_point> parse_local_day_start(const std::string& day) {
    if (!digits_yyyy_mm_dd(day)) {
        return std::nullopt;
    }

    std::tm tm{};
    std::istringstream iss(day);
    iss >> std::get_time(&tm, "%Y-%m-%d");
    if (iss.fail()) {
        return std::nullopt;
    }

    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    const __time64_t time = _mktime64(&tm);
    if (time == -1) {
        return std::nullopt;
    }

    // Reject non-normalized dates (e.g. 2024-02-31 → March).
    char canonical[16];
    if (strftime(canonical, sizeof(canonical), "%Y-%m-%d", &tm) == 0 || day != canonical) {
        return std::nullopt;
    }

    return std::chrono::system_clock::from_time_t(static_cast<std::time_t>(time));
}

// Advance by civil calendar days (DST-safe), not fixed 24h spans.
std::optional<std::chrono::system_clock::time_point> add_local_days(
    const std::chrono::system_clock::time_point& day_start,
    int days) {
    const auto t = std::chrono::system_clock::to_time_t(day_start);
    std::tm tm{};
    if (localtime_s(&tm, &t) != 0) {
        return std::nullopt;
    }
    tm.tm_mday += days;
    tm.tm_hour = 0;
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    const __time64_t next = _mktime64(&tm);
    if (next == -1) {
        return std::nullopt;
    }
    return std::chrono::system_clock::from_time_t(static_cast<std::time_t>(next));
}

// Next local hour boundary via civil-time normalization (DST-safe).
std::optional<std::chrono::system_clock::time_point> next_local_hour_end(
    const std::chrono::system_clock::time_point& cursor) {
    const auto t = std::chrono::system_clock::to_time_t(cursor);
    std::tm tm{};
    if (localtime_s(&tm, &t) != 0) {
        return std::nullopt;
    }
    tm.tm_min = 0;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    tm.tm_hour += 1;
    const __time64_t next = _mktime64(&tm);
    if (next == -1) {
        return std::nullopt;
    }
    const auto hour_end = std::chrono::system_clock::from_time_t(static_cast<std::time_t>(next));
    if (hour_end <= cursor) {
        return std::nullopt;
    }
    return hour_end;
}

// Consecutive civil-day strings from start_day_local to end_day_local (inclusive).
std::vector<std::string> day_list(const std::string& start_day_local,
                                  const std::string& end_day_local,
                                  const std::optional<std::chrono::system_clock::time_point>& start,
                                  const std::optional<std::chrono::system_clock::time_point>& end) {
    std::vector<std::string> days;
    if (!start || !end || *end < *start) {
        return days;
    }
    days.push_back(start_day_local);
    for (auto cursor = *start; cursor < *end;) {
        const auto next = add_local_days(cursor, 1);
        if (!next) {
            return {};
        }
        days.push_back(format_local_day(*next));
        cursor = *next;
    }
    return days;
}

std::unordered_map<std::string, size_t> day_index(const std::vector<std::string>& days) {
    std::unordered_map<std::string, size_t> index;
    index.reserve(days.size());
    for (size_t i = 0; i < days.size(); ++i) {
        index.emplace(days[i], i);
    }
    return index;
}

}  // namespace

bool Store::is_valid_local_day(const std::string& day_yyyy_mm_dd) {
    return parse_local_day_start(day_yyyy_mm_dd).has_value();
}

Store::~Store() {
    close();
}

bool Store::open(const std::wstring& db_path) {
    std::lock_guard lock(mutex_);
    close_unlocked();

    const std::filesystem::path path(db_path);
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    const std::string utf8 = wide_to_utf8(db_path);
    if (sqlite3_open(utf8.c_str(), &db_) != SQLITE_OK) {
        close_unlocked();
        return false;
    }

    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
    if (!ensure_schema() || !prepare_statements()) {
        close_unlocked();
        return false;
    }
    return true;
}

void Store::close() {
    std::lock_guard lock(mutex_);
    close_unlocked();
}

void Store::close_unlocked() {
    finalize_statements();
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

void Store::finalize_statements() {
    auto fin = [](sqlite3_stmt*& stmt) {
        if (stmt) {
            sqlite3_finalize(stmt);
            stmt = nullptr;
        }
    };
    fin(stmt_upsert_app_);
    fin(stmt_insert_session_);
    fin(stmt_upsert_daily_);
    fin(stmt_upsert_hourly_);
    fin(stmt_today_total_);
    fin(stmt_hourly_totals_);
    fin(stmt_day_totals_);
    fin(stmt_hourly_totals_range_);
    fin(stmt_app_breakdown_);
}

bool Store::prepare_statements() {
    auto prep = [this](sqlite3_stmt*& stmt, const char* sql) -> bool {
        return sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK;
    };

    return prep(stmt_upsert_app_,
                "INSERT INTO apps(exe_path, display_name, first_seen, last_seen) VALUES(?,?,?,?) "
                "ON CONFLICT(exe_path) DO UPDATE SET "
                "display_name=excluded.display_name, last_seen=excluded.last_seen "
                "RETURNING id;") &&
           prep(stmt_insert_session_,
                "INSERT INTO sessions(app_id, start_utc, end_utc, duration_ms) VALUES(?,?,?,?);") &&
           prep(stmt_upsert_daily_,
                "INSERT INTO daily_totals(day, app_id, duration_ms) VALUES(?,?,?) "
                "ON CONFLICT(day, app_id) DO UPDATE SET "
                "duration_ms = duration_ms + excluded.duration_ms;") &&
           prep(stmt_upsert_hourly_,
                "INSERT INTO hourly_totals(day, hour, app_id, duration_ms) VALUES(?,?,?,?) "
                "ON CONFLICT(day, hour, app_id) DO UPDATE SET "
                "duration_ms = duration_ms + excluded.duration_ms;") &&
           prep(stmt_today_total_,
                "SELECT COALESCE(SUM(duration_ms), 0) FROM daily_totals WHERE day = ?;") &&
           prep(stmt_hourly_totals_,
                "SELECT hour, COALESCE(SUM(duration_ms), 0) FROM hourly_totals WHERE day = ? "
                "GROUP BY hour;") &&
           prep(stmt_day_totals_,
                "SELECT day, COALESCE(SUM(duration_ms), 0) FROM daily_totals "
                "WHERE day BETWEEN ? AND ? GROUP BY day;") &&
           prep(stmt_hourly_totals_range_,
                "SELECT day, hour, COALESCE(SUM(duration_ms), 0) FROM hourly_totals "
                "WHERE day BETWEEN ? AND ? GROUP BY day, hour;") &&
           prep(stmt_app_breakdown_,
                "SELECT a.exe_path, a.display_name, COALESCE(SUM(d.duration_ms), 0) AS total "
                "FROM daily_totals d "
                "JOIN apps a ON a.id = d.app_id "
                "WHERE d.day >= ? AND d.day <= ? "
                "GROUP BY a.id "
                "HAVING total > 0 "
                "ORDER BY total DESC;");
}

bool Store::exec(const char* sql) const {
    char* err = nullptr;
    const int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (err) {
        sqlite3_free(err);
    }
    return rc == SQLITE_OK;
}

bool Store::ensure_schema() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS apps ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  exe_path TEXT NOT NULL UNIQUE,"
        "  display_name TEXT NOT NULL,"
        "  first_seen INTEGER NOT NULL,"
        "  last_seen INTEGER NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS sessions ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  app_id INTEGER NOT NULL,"
        "  start_utc INTEGER NOT NULL,"
        "  end_utc INTEGER NOT NULL,"
        "  duration_ms INTEGER NOT NULL,"
        "  FOREIGN KEY(app_id) REFERENCES apps(id)"
        ");"
        "CREATE TABLE IF NOT EXISTS daily_totals ("
        "  day TEXT NOT NULL,"
        "  app_id INTEGER NOT NULL,"
        "  duration_ms INTEGER NOT NULL,"
        "  PRIMARY KEY(day, app_id),"
        "  FOREIGN KEY(app_id) REFERENCES apps(id)"
        ");"
        "CREATE TABLE IF NOT EXISTS hourly_totals ("
        "  day TEXT NOT NULL,"
        "  hour INTEGER NOT NULL,"
        "  app_id INTEGER NOT NULL,"
        "  duration_ms INTEGER NOT NULL,"
        "  PRIMARY KEY(day, hour, app_id),"
        "  FOREIGN KEY(app_id) REFERENCES apps(id)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_sessions_time ON sessions(start_utc, end_utc);"
        "CREATE INDEX IF NOT EXISTS idx_daily_day ON daily_totals(day);";
    return exec(sql);
}

int64_t Store::upsert_app(const std::wstring& exe_path, const std::wstring& display_name) {
    std::lock_guard lock(mutex_);
    if (!db_ || !stmt_upsert_app_) {
        return 0;
    }

    const int64_t now = to_unix_ms(std::chrono::system_clock::now());
    const std::string path_utf8 = wide_to_utf8(exe_path);
    const std::string name_utf8 = wide_to_utf8(display_name);

    sqlite3_reset(stmt_upsert_app_);
    sqlite3_clear_bindings(stmt_upsert_app_);
    sqlite3_bind_text(stmt_upsert_app_, 1, path_utf8.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_upsert_app_, 2, name_utf8.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt_upsert_app_, 3, now);
    sqlite3_bind_int64(stmt_upsert_app_, 4, now);

    int64_t id = 0;
    if (sqlite3_step(stmt_upsert_app_) == SQLITE_ROW) {
        id = sqlite3_column_int64(stmt_upsert_app_, 0);
    }
    sqlite3_reset(stmt_upsert_app_);
    return id;
}

void Store::add_usage(int64_t app_id,
                      const std::chrono::system_clock::time_point& start,
                      const std::chrono::system_clock::time_point& end) {
    if (app_id <= 0 || end <= start) {
        return;
    }

    std::lock_guard lock(mutex_);
    if (!db_ || !stmt_insert_session_ || !stmt_upsert_daily_ || !stmt_upsert_hourly_) {
        return;
    }

    exec("BEGIN IMMEDIATE;");

    const int64_t start_ms = to_unix_ms(start);
    const int64_t end_ms = to_unix_ms(end);
    const int64_t duration = end_ms - start_ms;

    sqlite3_reset(stmt_insert_session_);
    sqlite3_bind_int64(stmt_insert_session_, 1, app_id);
    sqlite3_bind_int64(stmt_insert_session_, 2, start_ms);
    sqlite3_bind_int64(stmt_insert_session_, 3, end_ms);
    sqlite3_bind_int64(stmt_insert_session_, 4, duration);
    sqlite3_step(stmt_insert_session_);
    sqlite3_reset(stmt_insert_session_);

    auto cursor = start;
    while (cursor < end) {
        const std::string day = format_local_day(cursor);
        const int hour = local_hour(cursor);

        const auto hour_end_opt = next_local_hour_end(cursor);
        if (!hour_end_opt) {
            break;
        }
        const auto hour_end = (*hour_end_opt > end) ? end : *hour_end_opt;

        const int64_t slice =
            std::chrono::duration_cast<std::chrono::milliseconds>(hour_end - cursor).count();
        if (slice > 0) {
            sqlite3_reset(stmt_upsert_daily_);
            sqlite3_bind_text(stmt_upsert_daily_, 1, day.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int64(stmt_upsert_daily_, 2, app_id);
            sqlite3_bind_int64(stmt_upsert_daily_, 3, slice);
            sqlite3_step(stmt_upsert_daily_);
            sqlite3_reset(stmt_upsert_daily_);

            sqlite3_reset(stmt_upsert_hourly_);
            sqlite3_bind_text(stmt_upsert_hourly_, 1, day.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt_upsert_hourly_, 2, hour);
            sqlite3_bind_int64(stmt_upsert_hourly_, 3, app_id);
            sqlite3_bind_int64(stmt_upsert_hourly_, 4, slice);
            sqlite3_step(stmt_upsert_hourly_);
            sqlite3_reset(stmt_upsert_hourly_);
        }

        cursor = hour_end;
    }

    exec("COMMIT;");
}

int64_t Store::today_total_ms() const {
    std::lock_guard lock(mutex_);
    if (!db_ || !stmt_today_total_) {
        return 0;
    }

    const std::string day = format_local_day(std::chrono::system_clock::now());
    sqlite3_reset(stmt_today_total_);
    sqlite3_clear_bindings(stmt_today_total_);
    sqlite3_bind_text(stmt_today_total_, 1, day.c_str(), -1, SQLITE_STATIC);
    int64_t total = 0;
    if (sqlite3_step(stmt_today_total_) == SQLITE_ROW) {
        total = sqlite3_column_int64(stmt_today_total_, 0);
    }
    sqlite3_reset(stmt_today_total_);
    return total;
}

std::vector<int64_t> Store::hourly_totals(const std::string& day_local) const {
    std::vector<int64_t> buckets(24, 0);
    if (!is_valid_local_day(day_local)) {
        return buckets;
    }

    std::lock_guard lock(mutex_);
    if (!db_ || !stmt_hourly_totals_) {
        return buckets;
    }

    sqlite3_reset(stmt_hourly_totals_);
    sqlite3_clear_bindings(stmt_hourly_totals_);
    sqlite3_bind_text(stmt_hourly_totals_, 1, day_local.c_str(), -1, SQLITE_STATIC);
    while (sqlite3_step(stmt_hourly_totals_) == SQLITE_ROW) {
        const int hour = sqlite3_column_int(stmt_hourly_totals_, 0);
        if (hour >= 0 && hour < 24) {
            buckets[static_cast<size_t>(hour)] = sqlite3_column_int64(stmt_hourly_totals_, 1);
        }
    }
    sqlite3_reset(stmt_hourly_totals_);
    return buckets;
}

std::vector<int64_t> Store::week_day_totals(const std::string& start_day_local) const {
    const auto day_start = parse_local_day_start(start_day_local);
    if (!day_start) {
        return std::vector<int64_t>(7, 0);
    }
    const auto end = add_local_days(*day_start, 6);
    if (!end) {
        return std::vector<int64_t>(7, 0);
    }
    auto totals = day_totals(start_day_local, format_local_day(*end));
    totals.resize(7, 0);
    return totals;
}

std::vector<int64_t> Store::day_totals(const std::string& start_day_local,
                                       const std::string& end_day_local) const {
    const auto start = parse_local_day_start(start_day_local);
    const auto end = parse_local_day_start(end_day_local);
    const auto days = day_list(start_day_local, end_day_local, start, end);
    std::vector<int64_t> out(days.size(), 0);
    if (days.empty()) {
        return out;
    }

    std::lock_guard lock(mutex_);
    if (!db_ || !stmt_day_totals_) {
        return out;
    }

    sqlite3_reset(stmt_day_totals_);
    sqlite3_clear_bindings(stmt_day_totals_);
    sqlite3_bind_text(stmt_day_totals_, 1, start_day_local.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_day_totals_, 2, end_day_local.c_str(), -1, SQLITE_STATIC);
    const auto index = day_index(days);
    while (sqlite3_step(stmt_day_totals_) == SQLITE_ROW) {
        const auto* const day =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt_day_totals_, 0));
        if (!day) {
            continue;
        }
        const auto it = index.find(day);
        if (it != index.end()) {
            out[it->second] = sqlite3_column_int64(stmt_day_totals_, 1);
        }
    }
    sqlite3_reset(stmt_day_totals_);
    return out;
}

std::vector<std::vector<int64_t>> Store::hourly_totals_range(const std::string& start_day_local,
                                                             const std::string& end_day_local) const {
    const auto start = parse_local_day_start(start_day_local);
    const auto end = parse_local_day_start(end_day_local);
    const auto days = day_list(start_day_local, end_day_local, start, end);
    std::vector<std::vector<int64_t>> out(days.size(), std::vector<int64_t>(24, 0));
    if (days.empty()) {
        return out;
    }

    std::lock_guard lock(mutex_);
    if (!db_ || !stmt_hourly_totals_range_) {
        return out;
    }

    sqlite3_reset(stmt_hourly_totals_range_);
    sqlite3_clear_bindings(stmt_hourly_totals_range_);
    sqlite3_bind_text(stmt_hourly_totals_range_, 1, start_day_local.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_hourly_totals_range_, 2, end_day_local.c_str(), -1, SQLITE_STATIC);
    const auto index = day_index(days);
    while (sqlite3_step(stmt_hourly_totals_range_) == SQLITE_ROW) {
        const auto* const day =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt_hourly_totals_range_, 0));
        if (!day) {
            continue;
        }
        const int hour = sqlite3_column_int(stmt_hourly_totals_range_, 1);
        if (hour < 0 || hour >= 24) {
            continue;
        }
        const auto it = index.find(day);
        if (it != index.end()) {
            out[it->second][static_cast<size_t>(hour)] =
                sqlite3_column_int64(stmt_hourly_totals_range_, 2);
        }
    }
    sqlite3_reset(stmt_hourly_totals_range_);
    return out;
}

std::vector<AppUsageRow> Store::app_breakdown(const std::string& start_day_local,
                                              const std::string& end_day_local) const {
    std::vector<AppUsageRow> rows;
    if (!is_valid_local_day(start_day_local) || !is_valid_local_day(end_day_local)) {
        return rows;
    }

    std::lock_guard lock(mutex_);
    if (!db_ || !stmt_app_breakdown_) {
        return rows;
    }

    sqlite3_reset(stmt_app_breakdown_);
    sqlite3_clear_bindings(stmt_app_breakdown_);
    sqlite3_bind_text(stmt_app_breakdown_, 1, start_day_local.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_app_breakdown_, 2, end_day_local.c_str(), -1, SQLITE_STATIC);
    while (sqlite3_step(stmt_app_breakdown_) == SQLITE_ROW) {
        AppUsageRow row;
        const auto* const path =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt_app_breakdown_, 0));
        const auto* const name =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt_app_breakdown_, 1));
        row.exe_path = utf8_to_wide(path ? path : "");
        row.display_name = utf8_to_wide(name ? name : "");
        row.duration_ms = sqlite3_column_int64(stmt_app_breakdown_, 2);
        rows.push_back(std::move(row));
    }
    sqlite3_reset(stmt_app_breakdown_);
    return rows;
}

bool Store::clear_all() {
    std::lock_guard lock(mutex_);
    if (!db_) {
        return false;
    }
    return exec("DELETE FROM hourly_totals; DELETE FROM daily_totals; DELETE FROM sessions; DELETE FROM apps;");
}
