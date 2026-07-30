#include "screeni.h"

#include "tracker.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {

std::mutex g_mutex;
std::unique_ptr<Tracker> g_tracker;

Tracker& ensure_tracker() {
    if (!g_tracker) {
        g_tracker = std::make_unique<Tracker>();
    }
    return *g_tracker;
}

}  // namespace

extern "C" {

int screeni_start(void) {
    std::lock_guard lock(g_mutex);
    return ensure_tracker().start() ? 1 : 0;
}

void screeni_stop(void) {
    std::lock_guard lock(g_mutex);
    if (g_tracker) {
        g_tracker->stop();
    }
}

int screeni_is_running(void) {
    std::lock_guard lock(g_mutex);
    return (g_tracker && g_tracker->is_running()) ? 1 : 0;
}

int64_t screeni_get_today_total_ms(void) {
    std::lock_guard lock(g_mutex);
    if (!g_tracker) {
        return 0;
    }
    return g_tracker->store().today_total_ms();
}

int screeni_query_hourly(const char* day_yyyy_mm_dd, screeni_bucket_callback cb, void* user) {
    if (!day_yyyy_mm_dd || !cb || !Store::is_valid_local_day(day_yyyy_mm_dd)) {
        return 0;
    }

    std::vector<int64_t> buckets;
    {
        std::lock_guard lock(g_mutex);
        if (!g_tracker) {
            return 0;
        }
        buckets = g_tracker->store().hourly_totals(day_yyyy_mm_dd);
    }
    for (int i = 0; i < static_cast<int>(buckets.size()); ++i) {
        cb(i, buckets[static_cast<size_t>(i)], user);
    }
    return 1;
}

int screeni_query_week_days(const char* start_day_yyyy_mm_dd, screeni_bucket_callback cb, void* user) {
    if (!start_day_yyyy_mm_dd || !cb || !Store::is_valid_local_day(start_day_yyyy_mm_dd)) {
        return 0;
    }

    std::vector<int64_t> buckets;
    {
        std::lock_guard lock(g_mutex);
        if (!g_tracker) {
            return 0;
        }
        buckets = g_tracker->store().week_day_totals(start_day_yyyy_mm_dd);
    }
    for (int i = 0; i < static_cast<int>(buckets.size()); ++i) {
        cb(i, buckets[static_cast<size_t>(i)], user);
    }
    return 1;
}

int screeni_query_app_breakdown(const char* start_day_yyyy_mm_dd,
                                const char* end_day_yyyy_mm_dd,
                                screeni_app_callback cb,
                                void* user) {
    if (!start_day_yyyy_mm_dd || !end_day_yyyy_mm_dd || !cb ||
        !Store::is_valid_local_day(start_day_yyyy_mm_dd) ||
        !Store::is_valid_local_day(end_day_yyyy_mm_dd)) {
        return 0;
    }

    std::vector<AppUsageRow> rows;
    {
        std::lock_guard lock(g_mutex);
        if (!g_tracker) {
            return 0;
        }
        rows = g_tracker->store().app_breakdown(start_day_yyyy_mm_dd, end_day_yyyy_mm_dd);
    }
    for (const auto& row : rows) {
        cb(row.exe_path.c_str(), row.display_name.c_str(), row.duration_ms, user);
    }
    return 1;
}

int screeni_set_idle_threshold_sec(int seconds) {
    std::lock_guard lock(g_mutex);
    ensure_tracker().set_idle_threshold_sec(seconds);
    return 1;
}

int screeni_get_idle_threshold_sec(void) {
    std::lock_guard lock(g_mutex);
    return ensure_tracker().idle_threshold_sec();
}

int screeni_clear_data(void) {
    std::lock_guard lock(g_mutex);
    if (!g_tracker) {
        return 0;
    }
    return g_tracker->store().clear_all() ? 1 : 0;
}

}  // extern "C"
