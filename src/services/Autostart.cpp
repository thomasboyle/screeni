#include "Autostart.h"

#ifdef _WIN32
#include <Windows.h>
#include <string>

namespace {
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"Screeni";

std::wstring modulePath()
{
    wchar_t buf[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return {};
    return std::wstring(buf, n);
}
}  // namespace

bool Autostart::isEnabled()
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return false;
    DWORD type = 0;
    DWORD size = 0;
    const LONG rc = RegQueryValueExW(key, kValueName, nullptr, &type, nullptr, &size);
    RegCloseKey(key);
    return rc == ERROR_SUCCESS && type == REG_SZ;
}

void Autostart::setEnabled(bool enabled)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) !=
        ERROR_SUCCESS)
        return;
    if (enabled) {
        const std::wstring path = modulePath();
        if (!path.empty()) {
            std::wstring value = L"\"" + path + L"\"";
            RegSetValueExW(key, kValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                           static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
        }
    } else {
        RegDeleteValueW(key, kValueName);
    }
    RegCloseKey(key);
}
#else
bool Autostart::isEnabled() { return false; }
void Autostart::setEnabled(bool) {}
#endif
