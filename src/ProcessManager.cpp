#include "ProcessManager.h"
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <sddl.h>
#include <wintrust.h>
#include <softpub.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <algorithm>
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "wintrust.lib")

std::wstring ProcessManager::processPath(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return L"";
    wchar_t buf[32768]; DWORD n = static_cast<DWORD>(std::size(buf));
    std::wstring out;
    if (QueryFullProcessImageNameW(h, 0, buf, &n)) out.assign(buf, n);
    CloseHandle(h);
    return out;
}

std::wstring ProcessManager::processName(DWORD pid) {
    std::wstring p = processPath(pid);
    if (p.empty()) return L"";
    const size_t pos = p.find_last_of(L"\\/");
    return pos == std::wstring::npos ? p : p.substr(pos + 1);
}

std::wstring ProcessManager::publisherForPath(const std::wstring& path) {
    // Only report a publisher/trust result when Authenticode verification succeeds.
    // We deliberately do not infer trust from an executable filename.
    if (path.empty()) return L"Unknown publisher";
    WINTRUST_FILE_INFO fileInfo{};
    fileInfo.cbStruct = sizeof(fileInfo);
    fileInfo.pcwszFilePath = path.c_str();
    WINTRUST_DATA data{};
    data.cbStruct = sizeof(data);
    data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_NONE;
    data.dwUnionChoice = WTD_CHOICE_FILE;
    data.pFile = &fileInfo;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    GUID policy = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG status = WinVerifyTrust(nullptr, &policy, &data);
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &policy, &data);
    return status == ERROR_SUCCESS ? L"Verified signature" : L"Unknown publisher";
}

std::vector<AppInfo> ProcessManager::enumerate() {
    std::vector<AppInfo> result;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return result;
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            std::wstring path = processPath(pe.th32ProcessID);
            if (path.empty()) continue;
            AppInfo a;
            a.pid = pe.th32ProcessID;
            a.name = pe.szExeFile;
            a.path = path;
            a.publisher = publisherForPath(path);
            a.running = true;
            a.icon = ExtractIconW(GetModuleHandleW(nullptr), path.c_str(), 0);
            a.iconOwned = (a.icon && a.icon != INVALID_HANDLE_VALUE && a.icon != reinterpret_cast<HICON>(1));
            if (!a.iconOwned) a.icon = nullptr;
            result.push_back(std::move(a));
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    std::sort(result.begin(), result.end(), [](const AppInfo& a, const AppInfo& b) {
        return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
    });
    return result;
}

void ProcessManager::freeIcons(std::vector<AppInfo>& apps) {
    for (auto& a : apps) {
        if (a.iconOwned && a.icon) {
            DestroyIcon(a.icon);
            a.icon = nullptr;
            a.iconOwned = false;
        }
    }
}
