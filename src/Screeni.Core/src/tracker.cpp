#include "tracker.h"

#include <ShlObj.h>
#include <shellapi.h>

#include <cassert>
#include <cstddef>
#include <cwchar>
#include <cwctype>
#include <filesystem>
#include <vector>

std::atomic<Tracker*> Tracker::instance_ = nullptr;

namespace {

constexpr UINT kPollTimerId = 1;
constexpr UINT kPollIntervalMs = 1000;
constexpr UINT kFlushIntervalMs = 5000;
constexpr DWORD kVersionInfoMaxBytes = 1u << 20;

std::wstring db_path() {
    wchar_t* local = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) {
        return L"usage.db";
    }
    std::wstring path(local);
    CoTaskMemFree(local);
    path += L"\\Screeni\\usage.db";
    return path;
}

// LASTINPUTINFO::dwTime is a 32-bit GetTickCount value; reconstruct near GetTickCount64.
bool is_idle(int threshold_sec) {
    LASTINPUTINFO info{};
    info.cbSize = sizeof(info);
    if (!GetLastInputInfo(&info)) {
        return false;
    }
    const ULONGLONG now64 = GetTickCount64();
    ULONGLONG last64 = (now64 & ~0xFFFFFFFFULL) | static_cast<ULONGLONG>(info.dwTime);
    if (last64 > now64) {
        last64 -= 0x100000000ULL;
    }
    const ULONGLONG idle_ms = now64 - last64;
    return idle_ms >= static_cast<ULONGLONG>(threshold_sec) * 1000ull;
}

}  // namespace

Tracker::Tracker() = default;

Tracker::~Tracker() {
    stop();
}

bool Tracker::start() {
    if (running_.exchange(true)) {
        return true;
    }

    if (!store_.open(db_path())) {
        running_ = false;
        return false;
    }

    stop_requested_ = false;
    setup_ok_ = false;
    thread_id_ = 0;

    ready_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ready_event_) {
        store_.close();
        running_ = false;
        return false;
    }

    thread_ = std::thread([this] { thread_main(); });

    const DWORD wait = WaitForSingleObject(ready_event_, 15000);
    if (wait != WAIT_OBJECT_0 || !setup_ok_.load()) {
        stop_requested_ = true;
        const DWORD thread_id = thread_id_.load(std::memory_order_acquire);
        if (thread_id != 0) {
            PostThreadMessageW(thread_id, WM_QUIT, 0, 0);
        }
        if (thread_.joinable()) {
            thread_.join();
        }
        CloseHandle(ready_event_);
        ready_event_ = nullptr;
        store_.close();
        running_ = false;
        thread_id_ = 0;
        return false;
    }

    return true;
}

void Tracker::stop() {
    if (!running_.load()) {
        return;
    }

    stop_requested_ = true;
    const DWORD thread_id = thread_id_.load(std::memory_order_acquire);
    if (thread_id != 0) {
        PostThreadMessageW(thread_id, WM_QUIT, 0, 0);
    }
    if (thread_.joinable()) {
        thread_.join();
    }

    {
        std::lock_guard lock(state_mutex_);
        flush_current(true);
    }

    store_.close();

    if (ready_event_) {
        CloseHandle(ready_event_);
        ready_event_ = nullptr;
    }

    running_ = false;
    setup_ok_ = false;
    thread_id_ = 0;
}

void Tracker::set_idle_threshold_sec(int seconds) {
    int clamped = seconds;
    if (clamped < 15) {
        clamped = 15;
    }
    if (clamped > 3600) {
        clamped = 3600;
    }
    idle_threshold_sec_ = clamped;
}

bool Tracker::setup_message_loop() {
    assert(instance_.load(std::memory_order_acquire) == nullptr ||
           instance_.load(std::memory_order_acquire) == this);
    instance_.store(this, std::memory_order_release);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = message_wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"Screeni.Core.MessageWindow";
    if (!RegisterClassExW(&wc)) {
        const DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }

    message_hwnd_ = CreateWindowExW(0,
                                    wc.lpszClassName,
                                    L"",
                                    0,
                                    0,
                                    0,
                                    0,
                                    0,
                                    HWND_MESSAGE,
                                    nullptr,
                                    wc.hInstance,
                                    this);
    if (!message_hwnd_) {
        return false;
    }

    hook_ = SetWinEventHook(EVENT_SYSTEM_FOREGROUND,
                            EVENT_SYSTEM_FOREGROUND,
                            nullptr,
                            win_event_proc,
                            0,
                            0,
                            WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    if (!hook_) {
        return false;
    }

    if (SetTimer(message_hwnd_, kPollTimerId, kPollIntervalMs, nullptr) == 0) {
        return false;
    }

    return true;
}

void Tracker::teardown_message_loop() {
    if (message_hwnd_) {
        KillTimer(message_hwnd_, kPollTimerId);
    }
    if (hook_) {
        UnhookWinEvent(hook_);
        hook_ = nullptr;
    }
    if (message_hwnd_) {
        DestroyWindow(message_hwnd_);
        message_hwnd_ = nullptr;
    }
    instance_.store(nullptr, std::memory_order_release);
}

void Tracker::thread_main() {
    // Publish thread id + create the message queue before any fallible setup so
    // start()/stop() can always PostThreadMessageW(WM_QUIT) without racing.
    thread_id_.store(GetCurrentThreadId(), std::memory_order_release);
    MSG peek{};
    PeekMessageW(&peek, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

    const bool ok = setup_message_loop();
    setup_ok_ = ok;
    if (ready_event_) {
        SetEvent(ready_event_);
    }

    if (!ok) {
        teardown_message_loop();
        return;
    }

    on_foreground(GetForegroundWindow());

    MSG msg{};
    while (!stop_requested_.load() && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    teardown_message_loop();
}

void CALLBACK Tracker::win_event_proc(HWINEVENTHOOK,
                                      DWORD event,
                                      HWND hwnd,
                                      LONG id_object,
                                      LONG,
                                      DWORD,
                                      DWORD) {
    Tracker* const instance = instance_.load(std::memory_order_acquire);
    if (!instance || event != EVENT_SYSTEM_FOREGROUND || id_object != OBJID_WINDOW) {
        return;
    }
    instance->on_foreground(hwnd);
}

LRESULT CALLBACK Tracker::message_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return TRUE;
    }

    auto* self = reinterpret_cast<Tracker*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self && msg == WM_TIMER && wparam == kPollTimerId) {
        self->poll_idle_and_flush();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void Tracker::on_foreground(HWND hwnd) {
    std::lock_guard lock(state_mutex_);
    reconcile_focus(hwnd, true);
}

void Tracker::poll_idle_and_flush() {
    std::lock_guard lock(state_mutex_);
    reconcile_focus(nullptr, false);
}

// Step: sample idle, then either rebind focus (WinEvent / idle edge) or periodic flush + hwnd catch-up.
void Tracker::reconcile_focus(HWND hwnd, bool force_rescan) {
    const int threshold_sec = idle_threshold_sec_.load();
    const bool now_idle = is_idle(threshold_sec);

    if (force_rescan || now_idle != idle_) {
        flush_current(true);
        idle_ = now_idle;
        if (!now_idle) {
            const HWND target = hwnd != nullptr ? hwnd : GetForegroundWindow();
            begin_segment(target);
        } else {
            current_ = {};
        }
        return;
    }

    if (idle_ || !current_.active) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_flush_).count() >=
        static_cast<int64_t>(kFlushIntervalMs)) {
        flush_current(false);
    }

    const HWND fg = GetForegroundWindow();
    if (fg && fg != current_.hwnd) {
        flush_current(true);
        begin_segment(fg);
    }
}

void Tracker::flush_current(bool end_segment) {
    if (!current_.active || current_.app_id <= 0) {
        if (end_segment) {
            current_ = {};
        }
        return;
    }

    const auto now_steady = std::chrono::steady_clock::now();
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(now_steady - current_.segment_start);
    if (elapsed.count() > 0) {
        const auto end_wall = current_.segment_start_wall + elapsed;
        store_.add_usage(current_.app_id, current_.segment_start_wall, end_wall);
        current_.segment_start = now_steady;
        current_.segment_start_wall = end_wall;
    }
    last_flush_ = now_steady;

    if (end_segment) {
        current_.active = false;
    }
}

void Tracker::begin_segment(HWND hwnd) {
    current_ = {};
    if (!hwnd || !IsWindow(hwnd)) {
        return;
    }

    DWORD pid = 0;
    std::wstring exe;
    std::wstring name;
    if (!resolve_window(hwnd, pid, exe, name) || should_ignore(pid, exe)) {
        return;
    }

    const int64_t app_id = store_.upsert_app(exe, name);
    if (app_id <= 0) {
        return;
    }

    current_.hwnd = hwnd;
    current_.app_id = app_id;
    current_.segment_start = std::chrono::steady_clock::now();
    current_.segment_start_wall = std::chrono::system_clock::now();
    current_.active = true;
    last_flush_ = current_.segment_start;
}

bool Tracker::resolve_window(HWND hwnd, DWORD& pid, std::wstring& exe, std::wstring& name) const {
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) {
        return false;
    }
    exe = process_image_path(pid);
    if (exe.empty()) {
        return false;
    }
    name = file_description(exe);
    if (name.empty()) {
        name = std::filesystem::path(exe).stem().wstring();
    }
    return true;
}

bool Tracker::path_contains_ci(const std::wstring& haystack, const wchar_t* needle) {
    if (!needle || !*needle) {
        return false;
    }
    const size_t needle_len = wcslen(needle);
    if (needle_len > haystack.size()) {
        return false;
    }
    const size_t last = haystack.size() - needle_len;
    for (size_t i = 0; i <= last; ++i) {
        bool match = true;
        for (size_t j = 0; j < needle_len; ++j) {
            if (towlower(static_cast<wint_t>(haystack[i + j])) !=
                towlower(static_cast<wint_t>(needle[j]))) {
                match = false;
                break;
            }
        }
        if (match) {
            return true;
        }
    }
    return false;
}

bool Tracker::should_ignore(DWORD pid, const std::wstring& exe) {
    if (pid == GetCurrentProcessId()) {
        return true;
    }
    // Fragments are lowercase; path_contains_ci compares case-insensitively.
    static constexpr const wchar_t* kIgnored[] = {
        L"\\screeni.app.exe",
        L"\\lockapp.exe",
        L"\\logonui.exe",
        L"\\searchhost.exe",
        L"\\startmenuexperiencehost.exe",
        L"\\shellexperiencehost.exe",
    };
    for (const wchar_t* const fragment : kIgnored) {
        if (path_contains_ci(exe, fragment)) {
            return true;
        }
    }
    return false;
}

std::wstring Tracker::process_image_path(DWORD pid) {
    const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) {
        return {};
    }
    std::wstring path(MAX_PATH, L'\0');
    DWORD size = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(process, 0, path.data(), &size)) {
        path.resize(32768);
        size = static_cast<DWORD>(path.size());
        if (!QueryFullProcessImageNameW(process, 0, path.data(), &size)) {
            CloseHandle(process);
            return {};
        }
    }
    CloseHandle(process);
    path.resize(size);
    return path;
}

std::wstring Tracker::file_description(const std::wstring& path) {
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &handle);
    if (size == 0 || size > kVersionInfoMaxBytes) {
        return {};
    }
    std::vector<std::byte> buffer(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, buffer.data())) {
        return {};
    }

    struct LANGANDCODEPAGE {
        WORD language;
        WORD code_page;
    }* translate = nullptr;
    UINT translate_len = 0;
    if (!VerQueryValueW(buffer.data(),
                        L"\\VarFileInfo\\Translation",
                        reinterpret_cast<void**>(&translate),
                        &translate_len) ||
        translate_len < sizeof(LANGANDCODEPAGE)) {
        return {};
    }

    wchar_t sub_block[64];
    swprintf_s(sub_block,
               L"\\StringFileInfo\\%04x%04x\\FileDescription",
               translate[0].language,
               translate[0].code_page);

    wchar_t* description = nullptr;
    UINT description_len = 0;
    if (!VerQueryValueW(buffer.data(),
                        sub_block,
                        reinterpret_cast<void**>(&description),
                        &description_len) ||
        description_len == 0 || !description) {
        return {};
    }
    return std::wstring(description);
}
