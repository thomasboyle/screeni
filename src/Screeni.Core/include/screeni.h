#pragma once

#include <stdint.h>

#ifdef SCREENI_CORE_EXPORTS
#define SCREENI_API __declspec(dllexport)
#else
#define SCREENI_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*screeni_bucket_callback)(int bucket_index, int64_t duration_ms, void* user);
typedef void (*screeni_app_callback)(const wchar_t* exe_path,
                                     const wchar_t* display_name,
                                     int64_t duration_ms,
                                     void* user);

SCREENI_API int screeni_start(void);
SCREENI_API void screeni_stop(void);
SCREENI_API int screeni_is_running(void);

SCREENI_API int64_t screeni_get_today_total_ms(void);

/* Day is local calendar date as YYYY-MM-DD. Hourly buckets are 0..23. */
SCREENI_API int screeni_query_hourly(const char* day_yyyy_mm_dd,
                                     screeni_bucket_callback cb,
                                     void* user);

/* start_day is Monday (or any start); returns 7 consecutive local days as buckets 0..6. */
SCREENI_API int screeni_query_week_days(const char* start_day_yyyy_mm_dd,
                                        screeni_bucket_callback cb,
                                        void* user);

SCREENI_API int screeni_query_app_breakdown(const char* start_day_yyyy_mm_dd,
                                            const char* end_day_yyyy_mm_dd,
                                            screeni_app_callback cb,
                                            void* user);

SCREENI_API int screeni_set_idle_threshold_sec(int seconds);
SCREENI_API int screeni_get_idle_threshold_sec(void);
SCREENI_API int screeni_clear_data(void);

#ifdef __cplusplus
}
#endif
