#pragma once
#include <string>

class AutoStart {
public:
    static bool isEnabled();
    static bool setEnabled(bool enable);
    static std::wstring exePath();
};
