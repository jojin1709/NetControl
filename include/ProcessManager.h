#pragma once
#include "Models.h"
#include <vector>

class ProcessManager {
public:
    std::vector<AppInfo> enumerate();
    static void freeIcons(std::vector<AppInfo>& apps);
    static std::wstring processPath(DWORD pid);
    static std::wstring processName(DWORD pid);
    static std::wstring publisherForPath(const std::wstring& path);
};
