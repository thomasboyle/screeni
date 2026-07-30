#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

struct AppUsageRow {
    std::wstring exe_path;
    std::wstring display_name;
    int64_t duration_ms = 0;
};

class Store {
public:
    Store() = default;
    ~Store();

    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    static bool is_valid_local_day(const std::string& day_yyyy_mm_dd);

    bool open(const std::wstring& db_path);
    void close();

    int64_t upsert_app(const std::wstring& exe_path, const std::wstring& display_name);
    void add_usage(int64_t app_id,
                   const std::chrono::system_clock::time_point& start,
                   const std::chrono::system_clock::time_point& end);

    int64_t today_total_ms() const;
    std::vector<int64_t> hourly_totals(const std::string& day_local) const;
    std::vector<int64_t> week_day_totals(const std::string& start_day_local) const;
    std::vector<AppUsageRow> app_breakdown(const std::string& start_day_local,
                                           const std::string& end_day_local) const;
    bool clear_all();

private:
    sqlite3* db_ = nullptr;
    mutable std::mutex mutex_;

    sqlite3_stmt* stmt_upsert_app_ = nullptr;
    sqlite3_stmt* stmt_insert_session_ = nullptr;
    sqlite3_stmt* stmt_upsert_daily_ = nullptr;
    sqlite3_stmt* stmt_upsert_hourly_ = nullptr;
    sqlite3_stmt* stmt_today_total_ = nullptr;
    sqlite3_stmt* stmt_hourly_totals_ = nullptr;
    sqlite3_stmt* stmt_day_total_ = nullptr;
    sqlite3_stmt* stmt_app_breakdown_ = nullptr;

    bool exec(const char* sql) const;
    bool ensure_schema();
    bool prepare_statements();
    void finalize_statements();
    void close_unlocked();
};
