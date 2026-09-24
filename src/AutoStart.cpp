#include "AutoStart.h"
#include <windows.h>
#include <string>

static const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* kValue = L"NetControl";

std::wstring AutoStart::exePath() {
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return buf;
}

bool AutoStart::isEnabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &key) != ERROR_SUCCESS) return false;
    wchar_t buf[MAX_PATH]{};
    DWORD sz = sizeof(buf);
    DWORD type = 0;
    bool ok = RegQueryValueExW(key, kValue, nullptr, &type, reinterpret_cast<LPBYTE>(buf), &sz) == ERROR_SUCCESS
              && type == REG_SZ;
    RegCloseKey(key);
    return ok;
}

bool AutoStart::setEnabled(bool enable) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) return false;
    LONG r;
    if (enable) {
        std::wstring cmd = L"\"" + exePath() + L"\"";
        r = RegSetValueExW(key, kValue, 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(cmd.c_str()),
                           static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
    } else {
        r = RegDeleteValueW(key, kValue);
        if (r == ERROR_FILE_NOT_FOUND) r = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return r == ERROR_SUCCESS;
}
