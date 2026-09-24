#include "TrayIcon.h"
#include <cstring>

bool TrayIcon::add(HWND hwnd, HICON icon, const std::wstring& tip) {
    nid_ = {};
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = hwnd;
    nid_.uID = 1;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAYICON;
    nid_.hIcon = icon;
    wcsncpy_s(nid_.szTip, tip.c_str(), _TRUNCATE);
    added_ = Shell_NotifyIconW(NIM_ADD, &nid_) == TRUE;
    if (added_) {
        nid_.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &nid_);
    }
    return added_;
}

void TrayIcon::remove() {
    if (added_) { Shell_NotifyIconW(NIM_DELETE, &nid_); added_ = false; }
}

void TrayIcon::updateTip(const std::wstring& tip) {
    if (!added_) return;
    nid_.uFlags = NIF_TIP;
    wcsncpy_s(nid_.szTip, tip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayIcon::balloon(const std::wstring& title, const std::wstring& body, DWORD icon) {
    if (!added_) return;
    nid_.uFlags = NIF_INFO;
    wcsncpy_s(nid_.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(nid_.szInfo, body.c_str(), _TRUNCATE);
    nid_.dwInfoFlags = icon;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
}

void TrayIcon::showMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, ID_TRAY_SHOW, L"Open NetControl");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_TRAY_EXIT, L"Exit");
    SetForegroundWindow(hwnd);
    TrackPopupMenu(m, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(m);
}
