#include "Theme.h"

Theme makeTheme(ThemeMode mode) {
    Theme t{};
    t.mode = mode;
    if (mode == ThemeMode::Dark) {
        t.bg = RGB(18, 22, 28);
        t.sidebar = RGB(24, 28, 36);
        t.card = RGB(30, 36, 46);
        t.border = RGB(48, 56, 70);
        t.text = RGB(230, 235, 242);
        t.textDim = RGB(140, 150, 165);
        t.accent = RGB(56, 132, 255);
        t.accentText = RGB(255, 255, 255);
        t.success = RGB(40, 170, 110);
        t.danger = RGB(220, 80, 90);
        t.warn = RGB(200, 150, 50);
        t.rowHover = RGB(36, 42, 54);
        t.rowSel = RGB(30, 48, 78);
        t.inputBg = RGB(26, 30, 38);
        t.inputBorder = RGB(56, 64, 80);
        t.statusBg = RGB(26, 40, 34);
    } else {
        t.bg = RGB(246, 248, 251);
        t.sidebar = RGB(250, 251, 253);
        t.card = RGB(255, 255, 255);
        t.border = RGB(228, 232, 238);
        t.text = RGB(15, 23, 42);
        t.textDim = RGB(100, 113, 131);
        t.accent = RGB(37, 118, 230);
        t.accentText = RGB(255, 255, 255);
        t.success = RGB(30, 150, 105);
        t.danger = RGB(210, 75, 80);
        t.warn = RGB(180, 130, 40);
        t.rowHover = RGB(248, 250, 253);
        t.rowSel = RGB(240, 246, 255);
        t.inputBg = RGB(255, 255, 255);
        t.inputBorder = RGB(210, 216, 226);
        t.statusBg = RGB(237, 248, 240);
    }
    return t;
}

const wchar_t* themeName(ThemeMode m) { return m == ThemeMode::Dark ? L"Dark" : L"Light"; }
