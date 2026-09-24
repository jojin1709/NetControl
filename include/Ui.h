#pragma once
#include "Models.h"
#include "ProcessManager.h"
#include "FirewallManager.h"
#include "NetworkMonitor.h"
#include "UsageStore.h"
#include "Theme.h"
#include "Settings.h"
#include "ConnectionLog.h"
#include "UsageHistory.h"
#include "ScheduleManager.h"
#include "BandwidthManager.h"
#include "Notifier.h"
#include "NetworkProfileMonitor.h"
#include "TrayIcon.h"
#include <windows.h>
#include <vector>
#include <string>
#include <deque>
#include <unordered_set>

#define IDC_SEARCH 1001

class Ui {
public:
    bool create(HINSTANCE instance, FirewallManager* fw, ProcessManager* pm, NetworkMonitor* nm, UsageStore* store);
    int run();
    void refresh();
    void applySettings();
private:
    enum class Page { Overview, Applications, LiveMonitor, NetworkUsage, ConnectionLogs, Rules, Settings };

    struct Layout {
        RECT sidebar{};
        RECT logo{};
        RECT nav[7]{};
        RECT protection{};
        int contentX{0};
        int contentR{0};
        RECT cards[4]{};
        bool showCards{true};
        RECT search{};
        RECT sort{};
        RECT listCard{};
        RECT listHeader{};
        int rowsTop{0};
        int rowH{0};
        int visibleRows{0};
        int colObs{0};
        int colStatus{0};
        int colSpeed{0};
        int colMore{0};
        RECT detail{};
        RECT btnAllow{};
        RECT btnBlock{};
        RECT btnAsk{};
        RECT btnOpen{};
        RECT btnConn{};
        RECT btnLimit{};
        RECT contentCard{};
        RECT statusBar{};
        RECT btnClear{};
        RECT btnExport{};
        RECT btnImport{};
        RECT btnAbout{};
    };

    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle(HWND, UINT, WPARAM, LPARAM);
    Layout computeLayout() const;
    std::vector<int> visibleIndices(const Layout& L) const;
    void paint(HDC hdc);
    void paintContent(HDC hdc, const Layout& L);
    void paintSidebar(HDC hdc, const Layout& L);
    void paintCards(HDC hdc, const Layout& L);
    void paintList(HDC hdc, const Layout& L);
    void paintDetail(HDC hdc, const Layout& L);
    void paintLiveMonitor(HDC hdc, const Layout& L);
    void paintUsage(HDC hdc, const Layout& L);
    void paintLogs(HDC hdc, const Layout& L);
    void paintRules(HDC hdc, const Layout& L);
    void paintSettings(HDC hdc, const Layout& L);
    void paintStatusBar(HDC hdc, const Layout& L);
    void drawText(HDC hdc, const std::wstring& text, int x, int y, int size, COLORREF color, bool bold = false);
    void drawTextRight(HDC hdc, const std::wstring& text, int right, int y, int size, COLORREF color, bool bold = false);
    void drawTextCentered(HDC hdc, const std::wstring& text, RECT r, int size, COLORREF color, bool bold = false);
    void drawRoundRect(HDC hdc, RECT r, COLORREF fill, COLORREF border, int radius = 12);
    void drawButton(HDC hdc, RECT r, const std::wstring& label, COLORREF fill, COLORREF textColor, bool hover = false);
    void drawToggle(HDC hdc, RECT r, bool on, bool hover);
    void drawNavIcon(HDC hdc, int icon, RECT r, COLORREF color);
    void drawMagnifier(HDC hdc, int cx, int cy, int rad, COLORREF color);
    void drawTriangleDown(HDC hdc, int cx, int cy, int w, COLORREF color);
    void drawMoreDots(HDC hdc, int cx, int cy, COLORREF color);
    void drawCheck(HDC hdc, int cx, int cy, int s, COLORREF color);
    void drawLogoMark(HDC hdc, int cx, int cy, int s, COLORREF color);
    void drawPlaceholderIcon(HDC hdc, int cx, int cy, COLORREF color);
    void drawSparkline(HDC hdc, RECT r, const std::deque<SparkPoint>& pts, COLORREF color);
    bool hit(const RECT& r, int x, int y) const;
    void loadApps();
    void refreshProcessList();
    void selectApp(int index);
    void setSelectedMode(AccessMode mode);
    void setModesForSelection(AccessMode mode);
    void openConnectionsDialog();
    void openAboutDialog();
    void copyPathToClipboard();
    void showContextMenu(int x, int y, int rowIndex);
    void setStatus(const std::wstring& msg, bool error = false);
    void syncFirewallModes();
    void cleanupStale();
    void exportRulesUi();
    void importRulesUi();
    void applySchedules();
    void logConnectionEvents();
    void addScheduleForSelected();
    void editScheduleHours(int index);
    void promptBandwidthLimit();
    struct SchedRow { RECT mode, sMinus, sVal, sPlus, eMinus, eVal, ePlus, tog, del; };
    SchedRow schedRowRects(const Layout& L, int row) const;
    RECT schedAddRect(const Layout& L) const;
    RECT schedCardRect(const Layout& L) const;
    int schedVisibleRows(const Layout& L) const;
    RECT rulesCardRect(const Layout& L) const;
    int rulesVisibleRows(const Layout& L) const;
    RECT ruleRowToggle(const Layout& L, int row) const;
    RECT ruleRowDelete(const Layout& L, int row) const;
    RECT bwCardRect(const Layout& L) const;
    RECT bwSetRect(const Layout& L) const;
    RECT bwClearRect(const Layout& L) const;
    int scale(int v) const;
    COLORREF themeBg() const { return th_.bg; }

    HWND hwnd_{};
    HWND searchHwnd_{};
    HINSTANCE instance_{};
    FirewallManager* fw_{};
    ProcessManager* pm_{};
    NetworkMonitor* nm_{};
    UsageStore* store_{};
    SettingsStore settings_{};
    ConnectionLog connLog_{};
    UsageHistory history_{};
    ScheduleManager schedules_{};
    BandwidthManager bandwidth_{};
    Notifier notifier_{};
    NetworkProfileMonitor profile_{};
    TrayIcon tray_{};
    Theme th_{};
    std::vector<AppInfo> apps_;
    std::unordered_set<int> multiSel_;
    std::wstring filter_;
    std::wstring statusMsg_;
    bool statusError_{false};
    Page page_{Page::Overview};
    int selected_{-1};
    int scroll_{0};
    int hoverRow_{-1};
    int hoverNav_{-1};
    int hoverBtn_{0};
    int logScroll_{0};
    int ruleScroll_{0};
    int usageScroll_{0};
    int scheduleScroll_{0};
    int sortMode_{0};
    bool searchFocus_{false};
    bool listInvalidated_{false};
    UINT_PTR timer_{0};
    UINT_PTR procTimer_{0};
    UINT_PTR saveTimer_{0};
    HFONT font_{}, fontBold_{};
    int dpi_{96};
    bool hiddenToTray_{false};
    DWORD lastLogPid_{0};
    std::wstring lastLogRemote_;
};
