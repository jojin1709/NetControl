#pragma once
#include <windows.h>
#include <shellapi.h>
#include <string>

#define WM_TRAYICON (WM_USER + 40)
#define ID_TRAY_SHOW 3001
#define ID_TRAY_EXIT 3002
#define ID_TRAY_BLOCKED 3003

class TrayIcon {
public:
    bool add(HWND hwnd, HICON icon, const std::wstring& tip);
    void remove();
    void updateTip(const std::wstring& tip);
    void balloon(const std::wstring& title, const std::wstring& body, DWORD icon = NIIF_INFO);
    void showMenu(HWND hwnd);
private:
    NOTIFYICONDATAW nid_{};
    bool added_{false};
};
