#pragma once

#include "store.h"

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

class Tracker {
public:
    Tracker();
    ~Tracker();

    Tracker(const Tracker&) = delete;
    Tracker& operator=(const Tracker&) = delete;

    bool start();
    void stop();
    bool is_running() const { return running_.load(); }

    void set_idle_threshold_sec(int seconds);
    int idle_threshold_sec() const { return idle_threshold_sec_.load(); }

    Store& store() { return store_; }
    const Store& store() const { return store_; }

private:
    struct FocusState {
        HWND hwnd = nullptr;
        DWORD pid = 0;
        int64_t app_id = 0;
        std::wstring exe_path;
        std::wstring display_name;
        std::chrono::steady_clock::time_point segment_start{};
        std::chrono::system_clock::time_point segment_start_wall{};
        bool active = false;
    };

    void thread_main();
    bool setup_message_loop();
    void teardown_message_loop();
    void on_foreground(HWND hwnd);
    void poll_idle_and_flush();
    // Caller must hold state_mutex_. force_rescan: always end segment and rebind (WinEvent).
    void reconcile_focus(HWND hwnd, bool force_rescan);
    void flush_current(bool end_segment);
    void begin_segment(HWND hwnd);
    bool resolve_window(HWND hwnd, DWORD& pid, std::wstring& exe, std::wstring& name) const;
    static bool should_ignore(DWORD pid, const std::wstring& exe);
    static bool path_contains_ci(const std::wstring& haystack, const wchar_t* needle);
    static std::wstring process_image_path(DWORD pid);
    static std::wstring file_description(const std::wstring& path);

    Store store_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> setup_ok_{false};
    std::atomic<int> idle_threshold_sec_{60};
    HANDLE ready_event_ = nullptr;
    DWORD thread_id_ = 0;
    HWINEVENTHOOK hook_ = nullptr;
    HWND message_hwnd_ = nullptr;

    mutable std::mutex state_mutex_;
    FocusState current_;
    bool idle_ = false;
    std::chrono::steady_clock::time_point last_flush_{};

    static Tracker* instance_;
    static void CALLBACK win_event_proc(HWINEVENTHOOK hook,
                                        DWORD event,
                                        HWND hwnd,
                                        LONG id_object,
                                        LONG id_child,
                                        DWORD id_event_thread,
                                        DWORD event_time);
    static LRESULT CALLBACK message_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
};
