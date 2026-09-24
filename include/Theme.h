#pragma once
#include <windows.h>
#include <string>

enum class ThemeMode { Light, Dark };

struct Theme {
    COLORREF bg{}, sidebar{}, card{}, border{}, text{}, textDim{}, accent{};
    COLORREF accentText{}, success{}, danger{}, warn{}, rowHover{}, rowSel{};
    COLORREF inputBg{}, inputBorder{}, statusBg{};
    ThemeMode mode{ThemeMode::Light};
};

Theme makeTheme(ThemeMode mode);
const wchar_t* themeName(ThemeMode m);
