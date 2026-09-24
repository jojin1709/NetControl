#include "Ui.h"
#include "L10n.h"
#include "AutoStart.h"
#include "Theme.h"

#include <windows.h>
#include <objidl.h>
#include <ole2.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <commdlg.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <ctime>

#ifndef IDI_NETCONTROL
#define IDI_NETCONTROL 101
#endif
#ifndef IDM_ABOUT
#define IDM_ABOUT 101
#define IDM_CLEAR 102
#define IDM_THEME 103
#define IDM_TRAY 104
#endif

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comdlg32.lib")

static COLORREF RGBc(BYTE r, BYTE g, BYTE b) { return RGB(r, g, b); }

static std::wstring fmtBytes(uint64_t b) {
    const double x = static_cast<double>(b);
    std::wostringstream o;
    o << std::fixed;
    if (x >= 1e9) { o << std::setprecision(2) << x / 1e9 << L" GB"; }
    else if (x >= 1e6) { o << std::setprecision(1) << x / 1e6 << L" MB"; }
    else if (x >= 1e3) { o << std::setprecision(1) << x / 1e3 << L" KB"; }
    else { o << b << L" B"; }
    return o.str();
}

static std::wstring fmtMbps(double m) {
    std::wostringstream o;
    o << std::fixed << std::setprecision(m >= 10 ? 0 : m >= 1 ? 1 : 2) << m << L" Mbps";
    return o.str();
}

static std::wstring modeName(AccessMode m) {
    return m == AccessMode::Allow ? TR(L"Allowed") : m == AccessMode::Block ? TR(L"Blocked") : TR(L"Ask");
}
static COLORREF modeColor(const Theme& th, AccessMode m) {
    return m == AccessMode::Allow ? th.success : m == AccessMode::Block ? th.danger : th.textDim;
}
static std::wstring ellipsize(const std::wstring& s, size_t maxLen) {
    if (s.size() <= maxLen) return s;
    if (maxLen <= 1) return L"\x2026";
    return L"\x2026" + s.substr(s.size() - (maxLen - 1));
}
static std::wstring fmtTime(int64_t ts) {
    time_t t = static_cast<time_t>(ts);
    struct tm tmv{};
    localtime_s(&tmv, &t);
    wchar_t buf[32]{};
    wcsftime(buf, 32, L"%Y-%m-%d %H:%M:%S", &tmv);
    return buf;
}
static std::wstring lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), towlower);
    return s;
}
static bool contains(const std::wstring& hay, const std::wstring& needle) {
    if (needle.empty()) return true;
    return lower(hay).find(lower(needle)) != std::wstring::npos;
}

namespace {
struct PromptCtx {
    std::wstring label;
    std::wstring initial;
    std::wstring value;
    bool ok{false};
    HWND edit{};
};
LRESULT CALLBACK promptWndProc(HWND hw, UINT msg, WPARAM w, LPARAM l) {
    auto* ctx = reinterpret_cast<PromptCtx*>(GetWindowLongPtrW(hw, GWLP_USERDATA));
    switch (msg) {
    case WM_NCCREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(l);
        ctx = static_cast<PromptCtx*>(cs->lpCreateParams);
        SetWindowLongPtrW(hw, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx));
        return TRUE;
    }
    case WM_CREATE: {
        CreateWindowExW(0, L"STATIC", ctx->label.c_str(), WS_CHILD | WS_VISIBLE,
                        12, 12, 400, 18, hw, nullptr, nullptr, nullptr);
        ctx->edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", ctx->initial.c_str(),
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                    12, 36, 400, 26, hw, reinterpret_cast<HMENU>(101), nullptr, nullptr);
        CreateWindowExW(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                        214, 76, 90, 30, hw, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        314, 76, 90, 30, hw, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
        SendMessageW(ctx->edit, WM_SETFONT, WPARAM(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SetFocus(ctx->edit);
        return 0;
    }
    case WM_COMMAND: {
        if (LOWORD(w) == IDOK && ctx) {
            const int len = GetWindowTextLengthW(ctx->edit);
            std::wstring buf(static_cast<size_t>(len) + 1, L'\0');
            GetWindowTextW(ctx->edit, buf.data(), len + 1);
            buf.resize(wcslen(buf.c_str()));
            ctx->value = buf;
            ctx->ok = true;
            DestroyWindow(hw);
            return 0;
        }
        if (LOWORD(w) == IDCANCEL && ctx) {
            ctx->ok = false;
            DestroyWindow(hw);
            return 0;
        }
        return 0;
    }
    case WM_CLOSE:
        if (ctx) ctx->ok = false;
        DestroyWindow(hw);
        return 0;
    default:
        return DefWindowProcW(hw, msg, w, l);
    }
}
}  // namespace

static std::wstring promptValue(HWND owner, const wchar_t* title, const wchar_t* label,
                                const std::wstring& initial) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = promptWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"NetControlPromptCls";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        RegisterClassW(&wc);
        registered = true;
    }
    PromptCtx ctx;
    ctx.label = label;
    ctx.initial = initial;
    HWND hw = CreateWindowExW(WS_EX_DLGMODALFRAME, L"NetControlPromptCls", title,
                              WS_CAPTION | WS_SYSMENU,
                              CW_USEDEFAULT, CW_USEDEFAULT, 448, 178,
                              owner, nullptr, GetModuleHandleW(nullptr), &ctx);
    if (!hw) return {};
    if (owner) {
        RECT orr{}, wr{};
        GetWindowRect(owner, &orr);
        GetWindowRect(hw, &wr);
        SetWindowPos(hw, nullptr,
                     orr.left + ((orr.right - orr.left) - (wr.right - wr.left)) / 2,
                     orr.top + ((orr.bottom - orr.top) - (wr.bottom - wr.top)) / 2,
                     0, 0, SWP_NOSIZE | SWP_NOZORDER);
        EnableWindow(owner, FALSE);
    }
    ShowWindow(hw, SW_SHOW);
    UpdateWindow(hw);
    SetForegroundWindow(hw);
    MSG msg;
    while (IsWindow(hw) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_QUIT) {
            PostQuitMessage(static_cast<int>(msg.wParam));
            break;
        }
        if (!IsDialogMessageW(hw, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (owner) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
    return ctx.ok ? ctx.value : std::wstring();
}

static bool parseHourRange(const std::wstring& s, int& start, int& end) {
    const size_t p = s.find(L'-');
    if (p == std::wstring::npos) return false;
    try {
        start = std::stoi(s.substr(0, p));
        end = std::stoi(s.substr(p + 1));
    } catch (...) {
        return false;
    }
    if (start < 0 || start > 23 || end < 1 || end > 24 || start >= end) return false;
    return true;
}

int Ui::scale(int v) const { return MulDiv(v, dpi_, 96); }
bool Ui::hit(const RECT& r, int x, int y) const {
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

bool Ui::create(HINSTANCE instance, FirewallManager* fw, ProcessManager* pm, NetworkMonitor* nm, UsageStore* store) {
    instance_ = instance; fw_ = fw; pm_ = pm; nm_ = nm; store_ = store;
    settings_.load();
    th_ = makeTheme(settings_.get().theme);
    L10n::instance().setLanguage(settings_.get().language);
    connLog_.load();
    history_.load();
    schedules_.load();
    bandwidth_.load();
    notifier_.setEnabled(settings_.get().notifications);

    WNDCLASSW wc{};
    wc.hInstance = instance;
    wc.lpfnWndProc = wndProc;
    wc.lpszClassName = L"NetControlWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassW(&wc);
    hwnd_ = CreateWindowExW(0, wc.lpszClassName, L"NetControl \u2014 Internet Control for Windows",
                            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                            CW_USEDEFAULT, CW_USEDEFAULT, 1280, 860,
                            nullptr, nullptr, instance, this);
    if (!hwnd_) return false;
    dpi_ = GetDpiForWindow(hwnd_);
    font_ = CreateFontW(-scale(16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    fontBold_ = CreateFontW(-scale(16), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    searchHwnd_ = CreateWindowExW(0, L"EDIT", L"",
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                  0, 0, 10, 10, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SEARCH)),
                                  instance, nullptr);
    if (searchHwnd_) {
        SendMessageW(searchHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
const std::wstring cue = TR(L"Search applications...");
        SendMessageW(searchHwnd_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(cue.c_str()));
        SendMessageW(searchHwnd_, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(scale(10), scale(10)));
        SetWindowLongPtrW(searchHwnd_, GWL_STYLE, GetWindowLongPtrW(searchHwnd_, GWL_STYLE) | ES_CENTER);
    }

    loadApps();
    notifier_.setCallback([this](const std::wstring& t, const std::wstring& b) {
        tray_.balloon(t, b, NIIF_WARNING);
    });
    HICON appIcon = LoadIconW(instance, MAKEINTRESOURCE(IDI_NETCONTROL));
    if (!appIcon) appIcon = LoadIconW(nullptr, IDI_SHIELD);
    tray_.add(hwnd_, appIcon, L"NetControl");
    HMENU men = LoadMenuW(instance, L"APP_MENU");
    if (men) SetMenu(hwnd_, men);

    timer_ = SetTimer(hwnd_, 1, 2000, nullptr);
    const int procSec = std::max(2, settings_.get().processRefreshSec);
    procTimer_ = SetTimer(hwnd_, 2, static_cast<UINT>(procSec * 1000), nullptr);
    saveTimer_ = SetTimer(hwnd_, 3, static_cast<UINT>(std::max(5, settings_.get().saveIntervalSec) * 1000), nullptr);

    setStatus(TR(L"Protection Active"));
    if (!settings_.get().startMinimized) {
        ShowWindow(hwnd_, SW_SHOW);
    } else {
        ShowWindow(hwnd_, SW_HIDE);
        hiddenToTray_ = true;
    }
    UpdateWindow(hwnd_);
    return true;
}

void Ui::applySettings() {
    th_ = makeTheme(settings_.get().theme);
    L10n::instance().setLanguage(settings_.get().language);
    notifier_.setEnabled(settings_.get().notifications);
    AutoStart::setEnabled(settings_.get().autoStart);
    settings_.save();
    const std::wstring cue = TR(L"Search applications...");
    if (searchHwnd_) SendMessageW(searchHwnd_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(cue.c_str()));
    if (procTimer_) KillTimer(hwnd_, procTimer_);
    procTimer_ = SetTimer(hwnd_, 2, static_cast<UINT>(std::max(2, settings_.get().processRefreshSec) * 1000), nullptr);
    if (saveTimer_) KillTimer(hwnd_, saveTimer_);
    saveTimer_ = SetTimer(hwnd_, 3, static_cast<UINT>(std::max(5, settings_.get().saveIntervalSec) * 1000), nullptr);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Ui::refreshProcessList() {
    ProcessManager::freeIcons(apps_);
    loadApps();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Ui::loadApps() {
    auto fresh = pm_->enumerate();
    for (auto& a : fresh) {
        a.historicalBytes = store_->get(a.path);
        a.mode = store_->getMode(a.path);
        std::wstring ruleErr;
        if (a.mode == AccessMode::Block) fw_->setMode(a.path, AccessMode::Block, ruleErr);
        else fw_->setMode(a.path, AccessMode::Allow, ruleErr);
    }
    apps_ = std::move(fresh);
    nm_->refresh(apps_);
    for (auto& a : apps_) store_->add(a.path, 0);
    if (selected_ >= static_cast<int>(apps_.size())) selected_ = -1;
    multiSel_.clear();
}

void Ui::syncFirewallModes() {
    for (auto& a : apps_) {
        std::wstring err;
        if (a.mode == AccessMode::Block) fw_->setMode(a.path, AccessMode::Block, err);
    }
}

void Ui::cleanupStale() {
    std::vector<std::wstring> live;
    for (auto& a : apps_) live.push_back(a.path);
    std::wstring err;
    int n = fw_->cleanupStaleRules(live, err);
    setStatus(L"Removed " + std::to_wstring(n) + L" stale firewall rule(s)", !err.empty());
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Ui::applySchedules() {
    auto active = schedules_.activeNow();
    for (auto& [path, mode] : active) {
        for (auto& a : apps_) {
            if (_wcsicmp(a.path.c_str(), path.c_str()) == 0 && a.mode != mode) {
                std::wstring err;
                if (fw_->setMode(a.path, mode, err)) {
                    a.mode = mode;
                    store_->setMode(a.path, mode);
                }
            }
        }
    }
}

void Ui::addScheduleForSelected() {
    if (!(selected_ >= 0 && selected_ < static_cast<int>(apps_.size()))) {
        setStatus(L"Select an application first", true);
        return;
    }
    const std::wstring& path = apps_[selected_].path;
    for (auto& r : schedules_.rules()) {
        if (_wcsicmp(r.path.c_str(), path.c_str()) == 0) {
            setStatus(L"A schedule for this app already exists", true);
            return;
        }
    }
    std::wstring in = promptValue(hwnd_, L"Add schedule",
                                  L"Active hours, start-end (e.g. 9-17):", L"9-17");
    if (in.empty()) return;
    int start = 0, end = 24;
    if (!parseHourRange(in, start, end)) {
        setStatus(L"Invalid range \u2014 use start-end with hours 0-24", true);
        return;
    }
    ScheduleRule r;
    r.path = path;
    r.mode = AccessMode::Block;
    r.startHour = start;
    r.endHour = end;
    r.enabled = true;
    schedules_.add(r);
    setStatus(L"Schedule added");
    applySchedules();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Ui::editScheduleHours(int index) {
    if (index < 0 || index >= static_cast<int>(schedules_.rules().size())) return;
    auto& r = schedules_.rules()[static_cast<size_t>(index)];
    const std::wstring initial = std::to_wstring(r.startHour) + L"-" + std::to_wstring(r.endHour);
    std::wstring in = promptValue(hwnd_, L"Edit schedule hours",
                                  L"Active hours, start-end (e.g. 9-17):", initial);
    if (in.empty()) return;
    int start = 0, end = 24;
    if (!parseHourRange(in, start, end)) {
        setStatus(L"Invalid range \u2014 use start-end with hours 0-24", true);
        return;
    }
    r.startHour = start;
    r.endHour = end;
    schedules_.save();
    setStatus(L"Schedule updated");
    applySchedules();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Ui::promptBandwidthLimit() {
    if (!(selected_ >= 0 && selected_ < static_cast<int>(apps_.size()))) {
        setStatus(L"Select an application first", true);
        return;
    }
    const std::wstring& path = apps_[selected_].path;
    const double cur = bandwidth_.limit(path);
    std::wstring initial = cur > 0 ? std::to_wstring(static_cast<int>(cur)) : std::wstring(L"10");
    std::wstring in = promptValue(hwnd_, L"Bandwidth limit",
                                  L"Limit in Mbps (0 removes the limit):", initial);
    if (in.empty()) return;
    double v = 0;
    try {
        v = std::stod(in);
    } catch (...) {
        setStatus(L"Invalid number", true);
        return;
    }
    if (v < 0 || v > 100000) {
        setStatus(L"Limit out of range (0-100000 Mbps)", true);
        return;
    }
    if (v == 0) {
        bandwidth_.remove(path);
        setStatus(L"Bandwidth limit cleared");
    } else {
        bandwidth_.setLimit(path, v);
        setStatus(L"Bandwidth limit set to " + fmtMbps(v));
    }
    bandwidth_.save();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Ui::logConnectionEvents() {
    auto conns = nm_->allConnections();
    for (auto& c : conns) {
        if (c.proto != Proto::TCP4 && c.proto != Proto::TCP6) continue;
        if (c.state != 5) continue;
        const std::wstring key = std::to_wstring(c.pid) + L"|" + c.remoteAddress;
        if (key == lastLogRemote_ && c.pid == lastLogPid_) continue;
        std::wstring app;
        AccessMode mode = AccessMode::Ask;
        for (auto& a : apps_) {
            if (a.pid == c.pid) { app = a.name; mode = a.mode; break; }
        }
        if (app.empty()) continue;
        LogEntry e;
        e.timestamp = static_cast<int64_t>(time(nullptr));
        e.pid = c.pid;
        e.app = app;
        e.remote = c.remoteAddress;
        e.port = c.remotePort;
        e.proto = c.proto;
        e.action = mode == AccessMode::Block ? L"Block" : mode == AccessMode::Allow ? L"Allow" : L"Ask";
        connLog_.append(e);
        if (mode == AccessMode::Block)
            notifier_.notifyBlocked(app, c.remoteAddress);
        lastLogPid_ = c.pid;
        lastLogRemote_ = key;
    }
}

void Ui::refresh() {
    applySchedules();
    nm_->refresh(apps_);
    for (auto& a : apps_) {
        const uint64_t stored = store_->get(a.path);
        if (a.historicalBytes > stored) store_->add(a.path, a.historicalBytes - stored);
    }
    history_.addSample(nm_->totalObservedBytes());
    logConnectionEvents();
    if (page_ == Page::ConnectionLogs) connLog_.save();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Ui::setStatus(const std::wstring& msg, bool error) {
    statusMsg_ = msg;
    statusError_ = error;
    tray_.updateTip(L"NetControl \u2014 " + msg);
}

Ui::Layout Ui::computeLayout() const {
    RECT cr{};
    GetClientRect(hwnd_, &cr);
    Layout L;
    const int W = cr.right, H = cr.bottom;
    const int side = scale(230);
    L.sidebar = {0, 0, side, H};
    L.logo = {scale(16), scale(18), side - scale(12), scale(72)};
    for (int i = 0; i < 7; ++i) {
        const int top = scale(88) + i * scale(50);
        L.nav[i] = {scale(14), top, side - scale(14), top + scale(44)};
    }
    L.protection = {scale(14), H - scale(110), side - scale(14), H - scale(58)};
    L.statusBar = {scale(14), H - scale(50), side - scale(14), H - scale(14)};

    L.contentX = side + scale(20);
    L.contentR = W - scale(20);
    const int contentW = L.contentR - L.contentX;
    L.showCards = (page_ == Page::Overview);

    const int cardGap = scale(10);
    const int cardW = (contentW - cardGap * 3) / 4;
    const int cardTop = scale(20), cardH = scale(92);
    for (int i = 0; i < 4; ++i) {
        const int left = L.contentX + i * (cardW + cardGap);
        L.cards[i] = {left, cardTop, left + cardW, cardTop + cardH};
    }

    const int headerTop = L.showCards ? scale(135) : scale(24);
    const int detailW = scale(330);
    const int detailX = L.contentR - detailW;
    const int listRight = page_ == Page::Overview || page_ == Page::Applications
                          ? detailX - scale(16) : L.contentR;
    const int titleH = scale(48);
    const int listBottom = H - scale(20);
    const bool showDetail = (page_ == Page::Overview || page_ == Page::Applications);
    L.detail = showDetail ? RECT{detailX, headerTop, L.contentR, listBottom} : RECT{};

    L.search = {listRight - scale(330), headerTop + scale(2), listRight - scale(130), headerTop + scale(36)};
    L.sort = {listRight - scale(120), headerTop + scale(2), listRight, headerTop + scale(36)};

    L.listHeader = {L.contentX, headerTop + titleH, listRight, headerTop + titleH + scale(32)};
    L.rowsTop = L.listHeader.bottom;
    L.rowH = scale(57);
    const int rowsArea = listBottom - L.rowsTop;
    L.visibleRows = L.rowH > 0 ? std::max(1, rowsArea / L.rowH) : 1;
    L.listCard = {L.contentX, headerTop + titleH, listRight, L.rowsTop + L.visibleRows * L.rowH};

    const int listW = listRight - L.contentX;
    L.colObs = L.contentX + scale(56) + ((listW - scale(56) - scale(140)) * 40) / 100;
    L.colStatus = L.contentX + scale(56) + ((listW - scale(56) - scale(140)) * 66) / 100;
    L.colSpeed = L.contentX + scale(56) + ((listW - scale(56) - scale(140)) * 88) / 100;
    L.colMore = listRight - scale(36);

    if (showDetail) {
        const int bx = L.detail.left + scale(16);
        const int contentRight = L.detail.right - scale(16);
        const int bw = (contentRight - bx - scale(20)) / 3;
        const int bh = scale(44);
        const int accessLabelY = L.detail.top + scale(248);
        const int btnY = accessLabelY + scale(28);
        L.btnAllow = {bx, btnY, bx + bw, btnY + bh};
        L.btnBlock = {bx + bw + scale(10), btnY, bx + bw * 2 + scale(10), btnY + bh};
        L.btnAsk = {bx + (bw + scale(10)) * 2, btnY, bx + bw * 3 + scale(20), btnY + bh};
        const int helpY = btnY + bh + scale(14);
        const int openY = helpY + scale(54) + scale(36) + scale(16);
        L.btnOpen = {bx, openY, bx + bw, openY + bh};
        L.btnConn = {bx + bw + scale(10), openY, contentRight, openY + bh};
        L.btnLimit = {bx, openY + bh + scale(10), contentRight, openY + bh * 2 + scale(10)};
    }

    L.contentCard = {L.contentX, headerTop + titleH - scale(6), listRight, listBottom};
    L.btnClear = {L.contentR - scale(120), headerTop + scale(4), L.contentR - scale(70), headerTop + scale(36)};
    L.btnExport = {L.contentR - scale(66), headerTop + scale(4), L.contentR - scale(36), headerTop + scale(36)};
    L.btnImport = {L.contentR - scale(32), headerTop + scale(4), L.contentR, headerTop + scale(36)};
    L.btnAbout = {L.contentR - scale(70), scale(4), L.contentR, scale(34)};
    return L;
}

RECT Ui::rulesCardRect(const Layout& L) const {
    return RECT{L.contentX, scale(70), L.contentR, scale(400)};
}
int Ui::rulesVisibleRows(const Layout& L) const {
    const RECT c = rulesCardRect(L);
    return (std::max)(1, static_cast<int>((c.bottom - c.top - scale(50)) / scale(34)));
}
RECT Ui::ruleRowToggle(const Layout& L, int row) const {
    const RECT c = rulesCardRect(L);
    const int y = c.top + scale(44) + row * scale(34);
    return RECT{c.right - scale(140), y - scale(2), c.right - scale(90), y + scale(18)};
}
RECT Ui::ruleRowDelete(const Layout& L, int row) const {
    const RECT c = rulesCardRect(L);
    const int y = c.top + scale(44) + row * scale(34);
    return RECT{c.right - scale(48), y - scale(4), c.right - scale(16), y + scale(22)};
}
RECT Ui::schedCardRect(const Layout& L) const {
    const RECT b = rulesCardRect(L);
    return RECT{L.contentX, b.bottom + scale(20), L.contentR,
                (std::min)(b.bottom + scale(240), L.contentCard.bottom)};
}
int Ui::schedVisibleRows(const Layout& L) const {
    const RECT c = schedCardRect(L);
    return (std::max)(1, static_cast<int>((c.bottom - c.top - scale(58)) / scale(30)));
}
RECT Ui::schedAddRect(const Layout& L) const {
    const RECT c = schedCardRect(L);
    return RECT{c.right - scale(130), c.top + scale(10), c.right - scale(16), c.top + scale(38)};
}
Ui::SchedRow Ui::schedRowRects(const Layout& L, int row) const {
    const RECT c = schedCardRect(L);
    const int y = c.top + scale(56) + row * scale(30);
    const int bh = scale(24);
    SchedRow r{};
    r.mode   = {c.left + scale(340), y - scale(2), c.left + scale(420), y + bh};
    r.sMinus = {c.left + scale(440), y - scale(2), c.left + scale(464), y + bh};
    r.sVal   = {c.left + scale(468), y - scale(2), c.left + scale(516), y + bh};
    r.sPlus  = {c.left + scale(520), y - scale(2), c.left + scale(544), y + bh};
    r.eMinus = {c.left + scale(560), y - scale(2), c.left + scale(584), y + bh};
    r.eVal   = {c.left + scale(588), y - scale(2), c.left + scale(636), y + bh};
    r.ePlus  = {c.left + scale(640), y - scale(2), c.left + scale(664), y + bh};
    r.tog    = {c.right - scale(130), y - scale(1), c.right - scale(80), y + scale(19)};
    r.del    = {c.right - scale(60), y - scale(2), c.right - scale(20), y + bh};
    return r;
}
RECT Ui::bwCardRect(const Layout& L) const {
    const RECT s = schedCardRect(L);
    return RECT{L.contentX, s.bottom + scale(16), L.contentR, L.contentCard.bottom};
}
RECT Ui::bwSetRect(const Layout& L) const {
    const RECT c = bwCardRect(L);
    return RECT{c.left + scale(300), c.bottom - scale(48), c.left + scale(440), c.bottom - scale(16)};
}
RECT Ui::bwClearRect(const Layout& L) const {
    const RECT c = bwCardRect(L);
    return RECT{c.left + scale(456), c.bottom - scale(48), c.left + scale(610), c.bottom - scale(16)};
}

std::vector<int> Ui::visibleIndices(const Layout& L) const {
    (void)L;
    std::vector<int> idx;
    idx.reserve(apps_.size());
    for (int i = 0; i < static_cast<int>(apps_.size()); ++i) {
        const auto& a = apps_[i];
        if (!filter_.empty()) {
            if (!contains(a.name, filter_) && !contains(a.path, filter_) && !contains(a.publisher, filter_) &&
                !contains(std::to_wstring(a.pid), filter_)) continue;
        }
        idx.push_back(i);
    }
    if (sortMode_ == 0) {
        std::sort(idx.begin(), idx.end(), [&](int a, int b) { return apps_[a].historicalBytes > apps_[b].historicalBytes; });
    } else if (sortMode_ == 1) {
        std::sort(idx.begin(), idx.end(), [&](int a, int b) { return _wcsicmp(apps_[a].name.c_str(), apps_[b].name.c_str()) < 0; });
    } else {
        std::sort(idx.begin(), idx.end(), [&](int a, int b) {
            if (apps_[a].mode != apps_[b].mode) return static_cast<int>(apps_[a].mode) < static_cast<int>(apps_[b].mode);
            return _wcsicmp(apps_[a].name.c_str(), apps_[b].name.c_str()) < 0;
        });
    }
    return idx;
}

void Ui::drawText(HDC hdc, const std::wstring& text, int x, int y, int size, COLORREF color, bool bold) {
    HFONT f = CreateFontW(-scale(size), 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                          DEFAULT_PITCH, L"Segoe UI");
    HGDIOBJ old = SelectObject(hdc, f);
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);
    TextOutW(hdc, x, y, text.c_str(), static_cast<int>(text.size()));
    SelectObject(hdc, old);
    DeleteObject(f);
}

void Ui::drawTextRight(HDC hdc, const std::wstring& text, int right, int y, int size, COLORREF color, bool bold) {
    HFONT f = CreateFontW(-scale(size), 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                          DEFAULT_PITCH, L"Segoe UI");
    HGDIOBJ old = SelectObject(hdc, f);
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);
    SIZE s{};
    GetTextExtentPoint32W(hdc, text.c_str(), static_cast<int>(text.size()), &s);
    TextOutW(hdc, right - s.cx, y, text.c_str(), static_cast<int>(text.size()));
    SelectObject(hdc, old);
    DeleteObject(f);
}

void Ui::drawTextCentered(HDC hdc, const std::wstring& text, RECT r, int size, COLORREF color, bool bold) {
    HFONT f = CreateFontW(-scale(size), 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                          DEFAULT_PITCH, L"Segoe UI");
    HGDIOBJ old = SelectObject(hdc, f);
    SetTextColor(hdc, color);
    SetBkMode(hdc, TRANSPARENT);
    RECT rc = r;
    DrawTextW(hdc, text.c_str(), static_cast<int>(text.size()), &rc,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, old);
    DeleteObject(f);
}

void Ui::drawRoundRect(HDC hdc, RECT r, COLORREF fill, COLORREF border, int radius) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(hdc, b);
    HGDIOBJ op = SelectObject(hdc, p);
    RoundRect(hdc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(b);
    DeleteObject(p);
}

void Ui::drawButton(HDC hdc, RECT r, const std::wstring& label, COLORREF fill, COLORREF textColor, bool hover) {
    COLORREF f = fill;
    if (hover) {
        int rr = GetRValue(fill), gg = GetGValue(fill), bb = GetBValue(fill);
        rr = std::clamp(rr - 14, 0, 255); gg = std::clamp(gg - 14, 0, 255); bb = std::clamp(bb - 14, 0, 255);
        f = RGBc(static_cast<BYTE>(rr), static_cast<BYTE>(gg), static_cast<BYTE>(bb));
    }
    drawRoundRect(hdc, r, f, f, scale(9));
    drawTextCentered(hdc, label, r, 14, textColor, true);
}

void Ui::drawToggle(HDC hdc, RECT r, bool on, bool hover) {
    (void)hover;
    COLORREF track = on ? th_.accent : RGBc(180, 188, 200);
    drawRoundRect(hdc, r, track, track, (r.bottom - r.top) / 2);
    const int cy = (r.top + r.bottom) / 2;
    const int kr = (r.bottom - r.top) / 2 - scale(3);
    const int kx = on ? r.right - (r.bottom - r.top) / 2 + scale(2) : r.left + (r.bottom - r.top) / 2 - scale(2);
    HBRUSH b = CreateSolidBrush(RGBc(255, 255, 255));
    HGDIOBJ old = SelectObject(hdc, b);
    HPEN p = CreatePen(PS_SOLID, 1, RGBc(255, 255, 255));
    HGDIOBJ oldPen = SelectObject(hdc, p);
    Ellipse(hdc, kx - kr, cy - kr, kx + kr, cy + kr);
    SelectObject(hdc, old);
    SelectObject(hdc, oldPen);
    DeleteObject(b);
    DeleteObject(p);
}

void Ui::drawNavIcon(HDC hdc, int icon, RECT r, COLORREF color) {
    const int cx = (r.left + r.right) / 2;
    const int cy = (r.top + r.bottom) / 2;
    const int s = scale(9);
    HPEN pen = CreatePen(PS_SOLID, scale(2), color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    switch (icon) {
    case 0:
        MoveToEx(hdc, cx - s, cy, nullptr); LineTo(hdc, cx, cy - s); LineTo(hdc, cx + s, cy); LineTo(hdc, cx - s, cy);
        Rectangle(hdc, cx - s + scale(3), cy, cx + s - scale(3), cy + s);
        break;
    case 1:
        Rectangle(hdc, cx - s, cy - s, cx - scale(2), cy - scale(2));
        Rectangle(hdc, cx + scale(2), cy - s, cx + s, cy - scale(2));
        Rectangle(hdc, cx - s, cy + scale(2), cx - scale(2), cy + s);
        Rectangle(hdc, cx + scale(2), cy + scale(2), cx + s, cy + s);
        break;
    case 2:
        MoveToEx(hdc, cx - s, cy, nullptr);
        LineTo(hdc, cx - scale(4), cy);
        LineTo(hdc, cx - scale(2), cy - s + scale(2));
        LineTo(hdc, cx + scale(2), cy + s - scale(2));
        LineTo(hdc, cx + scale(4), cy);
        LineTo(hdc, cx + s, cy);
        break;
    case 3:
        MoveToEx(hdc, cx, cy + s, nullptr); LineTo(hdc, cx, cy - s);
        MoveToEx(hdc, cx - scale(5), cy - s + scale(4), nullptr); LineTo(hdc, cx, cy - s); LineTo(hdc, cx + scale(5), cy - s + scale(4));
        break;
    case 4:
        for (int i = -1; i <= 1; ++i) {
            MoveToEx(hdc, cx - s, cy + i * scale(5), nullptr);
            LineTo(hdc, cx + s, cy + i * scale(5));
        }
        break;
    case 5:
        MoveToEx(hdc, cx, cy - s, nullptr);
        LineTo(hdc, cx + s, cy - s + scale(4));
        LineTo(hdc, cx + s, cy + scale(2));
        LineTo(hdc, cx, cy + s);
        LineTo(hdc, cx - s, cy + scale(2));
        LineTo(hdc, cx - s, cy - s + scale(4));
        LineTo(hdc, cx, cy - s);
        break;
    case 6:
        Ellipse(hdc, cx - s, cy - s, cx + s, cy + s);
        Ellipse(hdc, cx - scale(3), cy - scale(3), cx + scale(3), cy + scale(3));
        for (int i = 0; i < 4; ++i) {
            const double a = i * 3.14159265 / 2.0;
            const int x1 = cx + static_cast<int>(std::cos(a) * s);
            const int y1 = cy + static_cast<int>(std::sin(a) * s);
            const int x2 = cx + static_cast<int>(std::cos(a) * (s + scale(3)));
            const int y2 = cy + static_cast<int>(std::sin(a) * (s + scale(3)));
            MoveToEx(hdc, x1, y1, nullptr); LineTo(hdc, x2, y2);
        }
        break;
    }
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void Ui::drawMagnifier(HDC hdc, int cx, int cy, int rad, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, scale(2), color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Ellipse(hdc, cx - rad, cy - rad, cx + rad, cy + rad);
    MoveToEx(hdc, cx + rad - scale(2), cy + rad - scale(2), nullptr);
    LineTo(hdc, cx + rad + scale(4), cy + rad + scale(4));
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void Ui::drawTriangleDown(HDC hdc, int cx, int cy, int w, COLORREF color) {
    HBRUSH b = CreateSolidBrush(color);
    HGDIOBJ old = SelectObject(hdc, b);
    HPEN p = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(hdc, p);
    POINT pts[3] = {{cx - w, cy - w / 2}, {cx + w, cy - w / 2}, {cx, cy + w / 2}};
    Polygon(hdc, pts, 3);
    SelectObject(hdc, old);
    SelectObject(hdc, oldPen);
    DeleteObject(b);
    DeleteObject(p);
}

void Ui::drawMoreDots(HDC hdc, int cx, int cy, COLORREF color) {
    HBRUSH b = CreateSolidBrush(color);
    HGDIOBJ old = SelectObject(hdc, b);
    HPEN p = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ oldPen = SelectObject(hdc, p);
    const int r = scale(2);
    for (int i = -1; i <= 1; ++i) {
        Ellipse(hdc, cx + i * scale(7) - r, cy - r, cx + i * scale(7) + r, cy + r);
    }
    SelectObject(hdc, old);
    SelectObject(hdc, oldPen);
    DeleteObject(b);
    DeleteObject(p);
}

void Ui::drawCheck(HDC hdc, int cx, int cy, int s, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, scale(3), color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    MoveToEx(hdc, cx - s, cy, nullptr);
    LineTo(hdc, cx - s / 3, cy + s);
    LineTo(hdc, cx + s, cy - s);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void Ui::drawLogoMark(HDC hdc, int cx, int cy, int s, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, scale(3), color);
    HBRUSH b = CreateSolidBrush(RGBc(232, 240, 254));
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBr = SelectObject(hdc, b);
    POINT pts[4] = {{cx, cy - s}, {cx + s, cy}, {cx, cy + s}, {cx - s, cy}};
    Polygon(hdc, pts, 4);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);
    DeleteObject(b);
}

void Ui::drawPlaceholderIcon(HDC hdc, int cx, int cy, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, scale(2), color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    const int s = scale(22);
    Ellipse(hdc, cx - s, cy - s, cx + s, cy + s);
    MoveToEx(hdc, cx - s / 2, cy - s / 3, nullptr);
    LineTo(hdc, cx + s / 2, cy - s / 3);
    MoveToEx(hdc, cx - s / 2, cy + s / 4, nullptr);
    LineTo(hdc, cx + s / 4, cy + s / 4);
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void Ui::drawSparkline(HDC hdc, RECT r, const std::deque<SparkPoint>& pts, COLORREF color) {
    drawRoundRect(hdc, r, th_.card, th_.border, scale(10));
    if (pts.size() < 2) {
        drawTextCentered(hdc, L"Collecting samples...", r, 12, th_.textDim);
        return;
    }
    double mx = 0.05;
    for (auto& p : pts) mx = std::max(mx, p.mbps);
    const int w = r.right - r.left - scale(16);
    const int h = r.bottom - r.top - scale(16);
    const int x0 = r.left + scale(8);
    const int y0 = r.top + scale(8);
    HPEN pen = CreatePen(PS_SOLID, scale(2), color);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    HGDIOBJ oldBr = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    const int n = static_cast<int>(pts.size());
    int prevX = 0, prevY = 0;
    for (int i = 0; i < n; ++i) {
        const int x = x0 + (n == 1 ? 0 : i * w / (n - 1));
        const int y = y0 + h - static_cast<int>((pts[i].mbps / mx) * h);
        if (i > 0) {
            MoveToEx(hdc, prevX, prevY, nullptr);
            LineTo(hdc, x, y);
        }
        prevX = x; prevY = y;
    }
    SelectObject(hdc, oldBr);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
    drawText(hdc, L"Peak " + fmtMbps(mx), r.left + scale(12), r.top + scale(8), 11, th_.textDim);
    drawTextRight(hdc, L"Now " + fmtMbps(pts.back().mbps), r.right - scale(12), r.top + scale(8), 11, color, true);
}

void Ui::paintSidebar(HDC hdc, const Layout& L) {
    HBRUSH sb = CreateSolidBrush(th_.sidebar);
    FillRect(hdc, &L.sidebar, sb);
    DeleteObject(sb);
    HPEN edge = CreatePen(PS_SOLID, 1, th_.border);
    HGDIOBJ oldPen = SelectObject(hdc, edge);
    MoveToEx(hdc, L.sidebar.right - 1, 0, nullptr);
    LineTo(hdc, L.sidebar.right - 1, L.sidebar.bottom);
    SelectObject(hdc, oldPen);
    DeleteObject(edge);

    drawLogoMark(hdc, L.logo.left + scale(16), (L.logo.top + L.logo.bottom) / 2, scale(14), th_.accent);
    drawText(hdc, L"NetControl", L.logo.left + scale(40), L.logo.top + scale(6), 22, th_.text, true);
    drawText(hdc, L"Internet Control for Windows", L.logo.left + scale(40), L.logo.top + scale(34), 11, th_.textDim);

    static const wchar_t* navKeys[] = {L"Overview", L"Applications", L"Live Monitor", L"Network Usage",
                                       L"Connection Logs", L"Rules", L"Settings"};
    for (int i = 0; i < 7; ++i) {
        const bool active = (static_cast<int>(page_) == i);
        const bool hover = (hoverNav_ == i && !active);
        if (active) drawRoundRect(hdc, L.nav[i], th_.accent, th_.accent, scale(9));
        else if (hover) drawRoundRect(hdc, L.nav[i], th_.rowHover, th_.rowHover, scale(9));
        const COLORREF fg = active ? th_.accentText : hover ? th_.accent : th_.text;
        const COLORREF ic = active ? th_.accentText : hover ? th_.accent : th_.textDim;
        RECT ir = L.nav[i];
        ir.right = ir.left + scale(44);
        drawNavIcon(hdc, i, ir, ic);
        drawText(hdc, TR(navKeys[i]), L.nav[i].left + scale(46), L.nav[i].top + scale(12), 14, fg, active);
    }

    drawRoundRect(hdc, L.protection, th_.statusBg, th_.border, scale(10));
    drawCheck(hdc, L.protection.left + scale(20), (L.protection.top + L.protection.bottom) / 2 - scale(6), scale(7), th_.success);
    drawText(hdc, TR(L"Protection Active"), L.protection.left + scale(40), L.protection.top + scale(12), 13, th_.text, true);
    drawText(hdc, TR(L"Monitoring real processes"), L.protection.left + scale(40), L.protection.top + scale(32), 11, th_.textDim);

    auto prof = profile_.query();
    std::wstring profName = prof == NetworkProfile::Private ? L"Private" :
                            prof == NetworkProfile::Public ? L"Public" :
                            prof == NetworkProfile::Domain ? L"Domain" : L"Unknown";
    drawRoundRect(hdc, L.statusBar, th_.card, th_.border, scale(8));
    drawText(hdc, L"Profile: " + profName, L.statusBar.left + scale(10), L.statusBar.top + scale(8), 10, th_.textDim);
    drawText(hdc, statusError_ ? (L"! " + statusMsg_) : statusMsg_,
             L.statusBar.left + scale(10), L.statusBar.top + scale(24), 9,
             statusError_ ? th_.danger : th_.textDim);
}

void Ui::paintCards(HDC hdc, const Layout& L) {
    uint64_t total = 0;
    int blocked = 0;
    for (auto& a : apps_) {
        total += a.historicalBytes;
        if (a.mode == AccessMode::Block) ++blocked;
    }
    std::wstring vals[4] = {
        fmtBytes(total),
        fmtMbps(nm_->systemMbps()),
        std::to_wstring(static_cast<int>(apps_.size())),
        std::to_wstring(blocked)
    };
    const wchar_t* labs[] = {L"Observed Data", L"Current Speed", L"Detected Processes", L"Blocked Apps"};
    const wchar_t* subs[] = {L"TCP observed by NetControl", L"Live sample", L"Unique running PIDs", L"NetControl rules"};
    for (int i = 0; i < 4; ++i) {
        drawRoundRect(hdc, L.cards[i], th_.card, th_.border, scale(12));
        drawText(hdc, TR(labs[i]), L.cards[i].left + scale(18), L.cards[i].top + scale(16), 12, th_.textDim);
        drawText(hdc, vals[i], L.cards[i].left + scale(18), L.cards[i].top + scale(40), 22, th_.text, true);
        drawText(hdc, subs[i], L.cards[i].left + scale(18), L.cards[i].top + scale(70), 10, th_.textDim);
    }
}

void Ui::paintList(HDC hdc, const Layout& L) {
    const int titleY = (L.showCards ? scale(135) : scale(24));
    drawText(hdc, TR(L"Applications"), L.contentX, titleY, 24, th_.text, true);

    if (!multiSel_.empty()) {
        const std::wstring multi = std::to_wstring(multiSel_.size()) + L" selected";
        drawText(hdc, multi, L.contentX + scale(170), titleY + scale(8), 13, th_.accent, true);
    }

    drawRoundRect(hdc, L.listCard, th_.card, th_.border, scale(11));
    drawRoundRect(hdc, L.search, th_.inputBg, searchFocus_ ? th_.accent : th_.inputBorder, scale(9));
    drawMagnifier(hdc, L.search.left + scale(16), (L.search.top + L.search.bottom) / 2, scale(6), th_.textDim);

    static const wchar_t* sorts[] = {L"Sort: Data usage", L"Sort: Name", L"Sort: Status"};
    drawRoundRect(hdc, L.sort, th_.inputBg, th_.inputBorder, scale(9));
    drawText(hdc, TR(sorts[sortMode_]), L.sort.left + scale(10), L.sort.top + scale(9), 11, th_.textDim);
    drawTriangleDown(hdc, L.sort.right - scale(14), (L.sort.top + L.sort.bottom) / 2, scale(5), th_.textDim);

    const int hy = L.listHeader.top + scale(9);
    drawText(hdc, L"Application", L.contentX + scale(56), hy, 10, th_.textDim, true);
    drawTextRight(hdc, L"Observed", L.colObs, hy, 10, th_.textDim, true);
    drawText(hdc, L"Status", L.colStatus, hy, 10, th_.textDim, true);
    drawTextRight(hdc, L"Speed", L.colSpeed + scale(90), hy, 10, th_.textDim, true);

    auto idx = visibleIndices(L);
    if (scroll_ > std::max(0, static_cast<int>(idx.size()) - L.visibleRows)) {
        scroll_ = std::max(0, static_cast<int>(idx.size()) - L.visibleRows);
    }
    if (idx.empty()) {
        drawTextCentered(hdc, filter_.empty() ? L"No applications found" : L"No matches for your search",
                         {L.contentX, L.rowsTop, L.colMore, L.rowsTop + scale(60)}, 13, th_.textDim);
        return;
    }

    for (int n = 0; n < L.visibleRows; ++n) {
        const int k = scroll_ + n;
        if (k >= static_cast<int>(idx.size())) break;
        const int ai = idx[k];
        auto& a = apps_[ai];
        const int y = L.rowsTop + n * L.rowH;
        const RECT row{L.contentX + scale(4), y, L.colMore + scale(20), y + L.rowH - scale(2)};
        const bool isSel = (ai == selected_) || multiSel_.count(ai) > 0;
        if (isSel) drawRoundRect(hdc, row, th_.rowSel, th_.accent, scale(8));
        else if (hoverRow_ == n) drawRoundRect(hdc, row, th_.rowHover, th_.rowHover, scale(8));

        const int iy = y + (L.rowH - scale(30)) / 2;
        if (multiSel_.count(ai)) {
            RECT cb{L.contentX + scale(6), iy + scale(6), L.contentX + scale(24), iy + scale(24)};
            drawRoundRect(hdc, cb, th_.accent, th_.accent, scale(4));
            drawCheck(hdc, (cb.left + cb.right) / 2, (cb.top + cb.bottom) / 2, scale(5), RGBc(255, 255, 255));
        } else if (a.icon) {
            DrawIconEx(hdc, L.contentX + scale(14), iy, a.icon, scale(30), scale(30), 0, nullptr, DI_NORMAL);
        } else {
            RECT ir{L.contentX + scale(14), iy, L.contentX + scale(44), iy + scale(30)};
            drawRoundRect(hdc, ir, th_.border, th_.border, scale(7));
            std::wstring initial = a.name.empty() ? L"?" : std::wstring(1, a.name[0]);
            drawTextCentered(hdc, initial, ir, 13, th_.accent, true);
        }

        const int nameMaxPx = (L.colObs - scale(20)) - (L.contentX + scale(56));
        int maxChars = 40;
        if (nameMaxPx > 0) maxChars = std::max(12, nameMaxPx / scale(7));
        drawText(hdc, ellipsize(a.name, static_cast<size_t>(maxChars)), L.contentX + scale(56), y + scale(10), 13, th_.text, true);

        const int pathMaxPx = (L.colObs - scale(20)) - (L.contentX + scale(56));
        int pathChars = 52;
        if (pathMaxPx > 0) pathChars = std::max(16, pathMaxPx / scale(5));
        drawText(hdc, ellipsize(a.path, static_cast<size_t>(pathChars)), L.contentX + scale(56), y + scale(30), 9, th_.textDim);

        drawTextRight(hdc, fmtBytes(a.historicalBytes), L.colObs, y + scale(19), 12, th_.text, true);

        RECT mr{L.colStatus, y + (L.rowH - scale(26)) / 2, L.colStatus + scale(80), y + (L.rowH + scale(26)) / 2};
        drawRoundRect(hdc, mr, modeColor(th_, a.mode), modeColor(th_, a.mode), scale(13));
        drawTextCentered(hdc, modeName(a.mode), mr, 10, RGBc(255, 255, 255), true);

        drawTextRight(hdc, fmtMbps(a.currentMbps), L.colSpeed + scale(90), y + scale(19), 11, th_.textDim);
        drawMoreDots(hdc, L.colMore + scale(8), y + L.rowH / 2, th_.textDim);
    }

    const int total = static_cast<int>(idx.size());
    if (total > L.visibleRows) {
        const int trackTop = L.rowsTop + scale(4);
        const int trackBot = L.listCard.bottom - scale(4);
        const int trackH = trackBot - trackTop;
        RECT track{L.listCard.right - scale(8), trackTop, L.listCard.right - scale(4), trackBot};
        drawRoundRect(hdc, track, th_.border, th_.border, scale(2));
        const int thumbH = std::max(scale(30), trackH * L.visibleRows / total);
        const int maxScroll = total - L.visibleRows;
        const int thumbY = trackTop + (maxScroll > 0 ? (trackH - thumbH) * scroll_ / maxScroll : 0);
        RECT thumb{track.left, thumbY, track.right, thumbY + thumbH};
        drawRoundRect(hdc, thumb, th_.textDim, th_.textDim, scale(2));
    }

    const int shown = std::min(L.visibleRows, std::max(0, total - scroll_));
    const int from = total == 0 ? 0 : scroll_ + 1;
    const int to = total == 0 ? 0 : scroll_ + shown;
    std::wstring count = std::to_wstring(from) + L"-" + std::to_wstring(to) + L" of " + std::to_wstring(total);
    drawTextRight(hdc, count, L.listCard.right - scale(48), L.listCard.bottom - scale(24), 10, th_.textDim);
}

void Ui::paintDetail(HDC hdc, const Layout& L) {
    drawRoundRect(hdc, L.detail, th_.card, th_.border, scale(12));
    if (!(selected_ >= 0 && selected_ < static_cast<int>(apps_.size()))) {
        const int cx = (L.detail.left + L.detail.right) / 2;
        drawPlaceholderIcon(hdc, cx, L.detail.top + scale(80), th_.textDim);
        drawTextCentered(hdc, TR(L"Select an application"),
                         {L.detail.left, L.detail.top + scale(120), L.detail.right, L.detail.top + scale(150)},
                         16, th_.text, true);
        drawTextCentered(hdc, L"Choose a process from the list",
                         {L.detail.left + scale(10), L.detail.top + scale(155), L.detail.right - scale(10), L.detail.top + scale(180)},
                         11, th_.textDim);
        drawTextCentered(hdc, L"to inspect and manage access.",
                         {L.detail.left + scale(10), L.detail.top + scale(175), L.detail.right - scale(10), L.detail.top + scale(200)},
                         11, th_.textDim);
        return;
    }

    auto& a = apps_[selected_];
    const int px = L.detail.left + scale(16);
    const int py = L.detail.top + scale(16);
    if (a.icon) {
        DrawIconEx(hdc, px, py, a.icon, scale(40), scale(40), 0, nullptr, DI_NORMAL);
    } else {
        RECT ir{px, py, px + scale(40), py + scale(40)};
        drawRoundRect(hdc, ir, th_.border, th_.border, scale(8));
        std::wstring initial = a.name.empty() ? L"?" : std::wstring(1, a.name[0]);
        drawTextCentered(hdc, initial, ir, 16, th_.accent, true);
    }
    drawText(hdc, ellipsize(a.name, 28), px + scale(52), py + scale(2), 16, th_.text, true);
    drawText(hdc, ellipsize(a.publisher, 34), px + scale(52), py + scale(24), 10, th_.textDim);

    drawText(hdc, TR(L"Path"), px, py + scale(58), 10, th_.textDim, true);
    drawText(hdc, ellipsize(a.path, 48), px, py + scale(74), 9, th_.textDim);
    drawText(hdc, L"PID  " + std::to_wstring(a.pid), px, py + scale(94), 9, th_.textDim);
    if (a.udpPackets > 0) {
        drawText(hdc, L"UDP endpoints  " + std::to_wstring(a.udpPackets),
                 px + scale(120), py + scale(94), 9, th_.textDim);
    }

    const int sy = py + scale(120);
    drawText(hdc, L"Observed TCP usage", px, sy, 11, th_.text, true);
    drawText(hdc, fmtBytes(a.historicalBytes), px, sy + scale(22), 22, th_.text, true);
    drawText(hdc, L"Live speed", px, sy + scale(56), 11, th_.text, true);
    drawText(hdc, fmtMbps(a.currentMbps), px, sy + scale(74), 14, th_.accent, true);

    drawText(hdc, TR(L"Internet Access"), px, L.btnAllow.top - scale(30), 13, th_.text, true);
    const bool hAllow = hoverBtn_ == 1, hBlock = hoverBtn_ == 2, hAsk = hoverBtn_ == 3;
    drawButton(hdc, L.btnAllow, TR(L"Allow"),
               a.mode == AccessMode::Allow ? th_.success : th_.border,
               a.mode == AccessMode::Allow ? RGBc(255, 255, 255) : th_.text, hAllow && a.mode != AccessMode::Allow);
    drawButton(hdc, L.btnBlock, TR(L"Block"),
               a.mode == AccessMode::Block ? th_.danger : th_.border,
               a.mode == AccessMode::Block ? RGBc(255, 255, 255) : th_.text, hBlock && a.mode != AccessMode::Block);
    drawButton(hdc, L.btnAsk, TR(L"Ask"),
               a.mode == AccessMode::Ask ? th_.textDim : th_.border,
               a.mode == AccessMode::Ask ? RGBc(255, 255, 255) : th_.text, hAsk && a.mode != AccessMode::Ask);

    const int helpY = L.btnAllow.bottom + scale(14);
    drawText(hdc, L"Ask mode monitors activity only. It does", px, helpY, 9, th_.textDim);
    drawText(hdc, L"not intercept connections \u2014 firewall rules", px, helpY + scale(15), 9, th_.textDim);
    drawText(hdc, L"cannot prompt per connection.", px, helpY + scale(30), 9, th_.textDim);

    RECT ruleBadge{px, helpY + scale(54), L.detail.right - scale(16), helpY + scale(54) + scale(36)};
    const bool blocked = a.mode == AccessMode::Block;
    drawRoundRect(hdc, ruleBadge,
                  blocked ? RGBc(255, 240, 240) : RGBc(240, 248, 244),
                  blocked ? RGBc(245, 210, 210) : RGBc(210, 235, 220), scale(8));
    std::wstring ruleText = blocked ? L"Firewall rule: Blocking outbound" : L"Firewall rule: Outbound allowed";
    drawText(hdc, ruleText, px + scale(12), ruleBadge.top + scale(10), 10,
             blocked ? RGBc(170, 50, 55) : RGBc(25, 120, 75), true);

    const double bwLimit = bandwidth_.limit(a.path);

    drawButton(hdc, L.btnOpen, TR(L"Open file"), th_.border, th_.text, hoverBtn_ == 4);
    drawButton(hdc, L.btnConn, TR(L"Connections"), th_.border, th_.text, hoverBtn_ == 5);
    drawButton(hdc, L.btnLimit,
               bwLimit > 0 ? L"Limit: " + fmtMbps(bwLimit) : TR(L"Set limit"),
               bwLimit > 0 ? th_.accent : th_.border,
               bwLimit > 0 ? th_.accentText : th_.text, hoverBtn_ == 6);
}

void Ui::paintLiveMonitor(HDC hdc, const Layout& L) {
    const int titleY = scale(24);
    drawText(hdc, TR(L"Live Monitor"), L.contentX, titleY, 24, th_.text, true);

    RECT spark{L.contentX, scale(70), L.contentR, scale(300)};
    drawSparkline(hdc, spark, nm_->history(), th_.accent);

    RECT stats{L.contentX, scale(320), L.contentR, scale(430)};
    drawRoundRect(hdc, stats, th_.card, th_.border, scale(12));
    drawText(hdc, L"Current system throughput", stats.left + scale(20), stats.top + scale(16), 12, th_.textDim);
    drawText(hdc, fmtMbps(nm_->systemMbps()), stats.left + scale(20), stats.top + scale(44), 28, th_.accent, true);
    drawTextRight(hdc, L"Total observed: " + fmtBytes(nm_->totalObservedBytes()),
                  stats.right - scale(20), stats.top + scale(20), 13, th_.text, true);
    drawTextRight(hdc, L"Samples: " + std::to_wstring(nm_->history().size()),
                  stats.right - scale(20), stats.top + scale(48), 12, th_.textDim);

    RECT top = {L.contentX, scale(450), L.contentR, L.contentCard.bottom};
    drawRoundRect(hdc, top, th_.card, th_.border, scale(12));
    drawText(hdc, L"Top applications by live speed", top.left + scale(20), top.top + scale(14), 13, th_.text, true);
    std::vector<const AppInfo*> sorted;
    for (auto& a : apps_) sorted.push_back(&a);
    std::sort(sorted.begin(), sorted.end(), [](const AppInfo* a, const AppInfo* b) { return a->currentMbps > b->currentMbps; });
    int n = 0;
    for (auto* a : sorted) {
        if (n >= 8) break;
        if (a->currentMbps <= 0 && n > 2) continue;
        const int y = top.top + scale(44) + n * scale(32);
        drawText(hdc, ellipsize(a->name, 40), top.left + scale(20), y, 13, th_.text);
        const int barW = static_cast<int>((a->currentMbps / std::max(0.1, sorted.empty() ? 1.0 : sorted[0]->currentMbps)) * (top.right - top.left - scale(260)));
        RECT bar{top.left + scale(220), y + scale(4), top.left + scale(220) + std::max(scale(4), barW), y + scale(18)};
        drawRoundRect(hdc, bar, th_.accent, th_.accent, scale(4));
        drawTextRight(hdc, fmtMbps(a->currentMbps), top.right - scale(20), y, 12, th_.textDim);
        ++n;
    }
    if (n == 0) {
        drawTextCentered(hdc, L"No active throughput samples yet", top, 13, th_.textDim);
    }
}

void Ui::paintUsage(HDC hdc, const Layout& L) {
    const int titleY = scale(24);
    drawText(hdc, TR(L"Network Usage"), L.contentX, titleY, 24, th_.text, true);

    RECT card{L.contentX, scale(70), L.contentR, scale(340)};
    drawRoundRect(hdc, card, th_.card, th_.border, scale(12));
    drawText(hdc, L"Daily observed usage (last 30 days)", card.left + scale(20), card.top + scale(14), 13, th_.text, true);

    auto& days = history_.days();
    if (days.empty()) {
        drawTextCentered(hdc, L"No history yet \u2014 samples accumulate while NetControl runs", card, 13, th_.textDim);
    } else {
        uint64_t mx = 1;
        for (auto& d : days) mx = std::max(mx, d.bytes);
        const int n = static_cast<int>(days.size());
        const int areaL = card.left + scale(40);
        const int areaR = card.right - scale(40);
        const int areaT = card.top + scale(50);
        const int areaB = card.bottom - scale(40);
        const int barGap = scale(6);
        const int barW = std::max(scale(8), ((areaR - areaL) - barGap * (n - 1)) / std::max(1, n));
        for (int i = 0; i < n; ++i) {
            const int x = areaL + i * (barW + barGap);
            const int h = static_cast<int>(static_cast<double>(days[i].bytes) / mx * (areaB - areaT));
            RECT bar{x, areaB - h, x + barW, areaB};
            if (h > 2) drawRoundRect(hdc, bar, th_.accent, th_.accent, scale(3));
        }
        drawTextRight(hdc, fmtBytes(mx), areaR, areaT - scale(18), 10, th_.textDim);
        drawText(hdc, days.front().date, areaL, areaB + scale(6), 9, th_.textDim);
        drawTextRight(hdc, days.back().date, areaR, areaB + scale(6), 9, th_.textDim);
    }

    RECT appsCard{L.contentX, scale(360), L.contentR, L.contentCard.bottom};
    drawRoundRect(hdc, appsCard, th_.card, th_.border, scale(12));
    drawText(hdc, L"Usage by application", appsCard.left + scale(20), appsCard.top + scale(14), 13, th_.text, true);
    std::vector<const AppInfo*> sorted;
    for (auto& a : apps_) if (a.historicalBytes > 0) sorted.push_back(&a);
    std::sort(sorted.begin(), sorted.end(), [](const AppInfo* a, const AppInfo* b) { return a->historicalBytes > b->historicalBytes; });
    uint64_t mx = 1;
    if (!sorted.empty()) mx = sorted.front()->historicalBytes;
    const int total = static_cast<int>(sorted.size());
    const int maxRows = static_cast<int>((appsCard.bottom - appsCard.top - scale(50)) / scale(34));
    const int maxScroll = (std::max)(0, total - maxRows);
    if (usageScroll_ > maxScroll) usageScroll_ = maxScroll;
    if (usageScroll_ < 0) usageScroll_ = 0;
    int n = 0;
    for (int i = usageScroll_; i < total && n < maxRows; ++i, ++n) {
        const AppInfo* a = sorted[i];
        const int y = appsCard.top + scale(44) + n * scale(34);
        drawText(hdc, ellipsize(a->name, 44), appsCard.left + scale(20), y, 13, th_.text);
        drawTextRight(hdc, fmtBytes(a->historicalBytes), appsCard.right - scale(20), y, 12, th_.textDim, true);
        const int barW = static_cast<int>(static_cast<double>(a->historicalBytes) / mx * (appsCard.right - appsCard.left - scale(320)));
        RECT bar{appsCard.left + scale(260), y + scale(4), appsCard.left + scale(260) + std::max(scale(4), barW), y + scale(16)};
        drawRoundRect(hdc, bar, th_.accent, th_.accent, scale(3));
    }
    if (sorted.empty()) {
        drawTextCentered(hdc, L"No observed usage yet", appsCard, 13, th_.textDim);
    }
}

void Ui::paintLogs(HDC hdc, const Layout& L) {
    const int titleY = scale(24);
    drawText(hdc, TR(L"Connection Logs"), L.contentX, titleY, 24, th_.text, true);

    drawButton(hdc, L.btnClear, TR(L"Clear history"), th_.border, th_.text, hoverBtn_ == 20);
    drawButton(hdc, L.btnExport, L"\u2193", th_.accent, th_.accentText, hoverBtn_ == 21);
    drawButton(hdc, L.btnImport, L"\u2191", th_.border, th_.text, hoverBtn_ == 22);

    RECT card = {L.contentX, scale(70), L.contentR, L.contentCard.bottom};
    drawRoundRect(hdc, card, th_.card, th_.border, scale(12));

    auto& entries = connLog_.entries();
    const int hy = card.top + scale(10);
    drawText(hdc, L"Time", card.left + scale(16), hy, 10, th_.textDim, true);
    drawText(hdc, L"App", card.left + scale(180), hy, 10, th_.textDim, true);
    drawText(hdc, L"Remote", card.left + scale(360), hy, 10, th_.textDim, true);
    drawText(hdc, L"Proto", card.left + scale(560), hy, 10, th_.textDim, true);
    drawTextRight(hdc, L"Action", card.right - scale(20), hy, 10, th_.textDim, true);

    if (entries.empty()) {
        drawTextCentered(hdc, TR(L"No logs"), card, 13, th_.textDim);
        return;
    }
    const int rowH = scale(30);
    const int visible = (std::max)(1, static_cast<int>((card.bottom - card.top - scale(40)) / rowH));
    const int total = static_cast<int>(entries.size());
    if (logScroll_ > (std::max)(0, total - visible)) logScroll_ = (std::max)(0, total - visible);
    for (int n = 0; n < visible; ++n) {
        const int k = total - 1 - logScroll_ - n;
        if (k < 0) break;
        const auto& e = entries[k];
        const int y = card.top + scale(34) + n * rowH;
        if (n % 2 == 1) {
            RECT rr{card.left + scale(4), y - scale(4), card.right - scale(4), y + rowH - scale(8)};
            drawRoundRect(hdc, rr, th_.rowHover, th_.rowHover, scale(4));
        }
        drawText(hdc, fmtTime(e.timestamp), card.left + scale(16), y, 11, th_.textDim);
        drawText(hdc, ellipsize(e.app, 24), card.left + scale(180), y, 12, th_.text);
        drawText(hdc, e.remote + (e.port ? L":" + std::to_wstring(e.port) : L""), card.left + scale(360), y, 11, th_.textDim);
        drawText(hdc, ConnectionLog::protoName(e.proto), card.left + scale(560), y, 11, th_.textDim);
        COLORREF ac = e.action == L"Block" ? th_.danger : e.action == L"Allow" ? th_.success : th_.textDim;
        drawTextRight(hdc, e.action, card.right - scale(20), y, 11, ac, true);
    }
    drawTextRight(hdc, std::to_wstring(total) + L" entries", card.right - scale(20), card.bottom - scale(24), 10, th_.textDim);
}

void Ui::paintRules(HDC hdc, const Layout& L) {
    const int titleY = scale(24);
    drawText(hdc, TR(L"Rules"), L.contentX, titleY, 24, th_.text, true);
    drawButton(hdc, {L.contentR - scale(250), titleY + scale(4), L.contentR - scale(170), titleY + scale(36)},
               TR(L"Export rules"), th_.accent, th_.accentText, hoverBtn_ == 30);
    drawButton(hdc, {L.contentR - scale(164), titleY + scale(4), L.contentR - scale(84), titleY + scale(36)},
               TR(L"Import rules"), th_.border, th_.text, hoverBtn_ == 31);
    drawButton(hdc, {L.contentR - scale(78), titleY + scale(4), L.contentR, titleY + scale(36)},
               L"Cleanup", th_.border, th_.text, hoverBtn_ == 32);

    RECT card = rulesCardRect(L);
    drawRoundRect(hdc, card, th_.card, th_.border, scale(12));
    drawText(hdc, L"NetControl firewall rules", card.left + scale(20), card.top + scale(14), 13, th_.text, true);

    auto rules = fw_->listRules(true);
    if (rules.empty()) {
        drawTextCentered(hdc, TR(L"No rules"), card, 13, th_.textDim);
    } else {
        const int rowH = scale(34);
        const int visible = rulesVisibleRows(L);
        const int maxScroll = (std::max)(0, static_cast<int>(rules.size()) - visible);
        if (ruleScroll_ > maxScroll) ruleScroll_ = maxScroll;
        int n = 0;
        for (int i = ruleScroll_; i < static_cast<int>(rules.size()) && n < visible; ++i, ++n) {
            auto& r = rules[i];
            const int y = card.top + scale(44) + n * rowH;
            drawText(hdc, ellipsize(r.application.empty() ? r.name : r.application, 70),
                     card.left + scale(20), y, 12, th_.text);
            RECT tog = ruleRowToggle(L, n);
            drawToggle(hdc, tog, r.enabled, hoverRow_ == n);
            drawText(hdc, r.enabled ? TR(L"Enable") : TR(L"Disable"),
                     card.right - scale(84), y, 11, r.enabled ? th_.success : th_.textDim);
            drawButton(hdc, ruleRowDelete(L, n),
                       L"X", RGBc(255, 235, 235), th_.danger, false);
        }
    }

    RECT sch = schedCardRect(L);
    drawRoundRect(hdc, sch, th_.card, th_.border, scale(12));
    drawText(hdc, L"Scheduled access rules", sch.left + scale(20), sch.top + scale(14), 13, th_.text, true);
    drawText(hdc, L"Block/allow by time of day (hours 0-24)", sch.left + scale(20), sch.top + scale(36), 11, th_.textDim);
    drawButton(hdc, schedAddRect(L), TR(L"+ Add"), th_.accent, th_.accentText, hoverBtn_ == 33);
    auto& srules = schedules_.rules();
    if (srules.empty()) {
        drawTextCentered(hdc, L"No schedules \u2014 select an app and click + Add",
                         {sch.left, sch.top + scale(60), sch.right, sch.bottom}, 12, th_.textDim);
    } else {
        const int visible = schedVisibleRows(L);
        const int maxScroll = (std::max)(0, static_cast<int>(srules.size()) - visible);
        if (scheduleScroll_ > maxScroll) scheduleScroll_ = maxScroll;
        if (scheduleScroll_ < 0) scheduleScroll_ = 0;
        int n = 0;
        for (int i = scheduleScroll_; i < static_cast<int>(srules.size()) && n < visible; ++i, ++n) {
            const auto& r = srules[i];
            const int y = sch.top + scale(56) + n * scale(30);
            if (n % 2 == 1) {
                RECT rr{sch.left + scale(6), y - scale(4), sch.right - scale(6), y + scale(24)};
                drawRoundRect(hdc, rr, th_.rowHover, th_.rowHover, scale(4));
            }
            drawText(hdc, ellipsize(r.path, 40), sch.left + scale(20), y + scale(3), 11, th_.text);
            SchedRow sr = schedRowRects(L, n);
            drawButton(hdc, sr.mode, r.mode == AccessMode::Block ? L"Block" : L"Allow",
                       r.mode == AccessMode::Block ? th_.danger : th_.success,
                       RGBc(255, 255, 255), false);
            drawButton(hdc, sr.sMinus, L"-", th_.border, th_.text, false);
            drawButton(hdc, sr.sVal, std::to_wstring(r.startHour) + L":00", th_.border, th_.text, false);
            drawButton(hdc, sr.sPlus, L"+", th_.border, th_.text, false);
            drawButton(hdc, sr.eMinus, L"-", th_.border, th_.text, false);
            drawButton(hdc, sr.eVal, std::to_wstring(r.endHour) + L":00", th_.border, th_.text, false);
            drawButton(hdc, sr.ePlus, L"+", th_.border, th_.text, false);
            drawToggle(hdc, sr.tog, r.enabled, false);
            drawButton(hdc, sr.del, L"X", RGBc(255, 235, 235), th_.danger, false);
        }
        if (static_cast<int>(srules.size()) > visible) {
            drawTextRight(hdc, std::to_wstring(srules.size()) + L" schedules",
                          sch.right - scale(20), sch.bottom - scale(24), 10, th_.textDim);
        }
    }

    RECT bw = bwCardRect(L);
    if (bw.bottom > bw.top + scale(40)) {
        drawRoundRect(hdc, bw, th_.card, th_.border, scale(12));
        drawText(hdc, L"Bandwidth shaping", bw.left + scale(20), bw.top + scale(12), 13, th_.text, true);
        drawText(hdc, bandwidth_.statusText(), bw.left + scale(20), bw.top + scale(36), 11, th_.warn);
        std::wstring selLine = L"No app selected";
        if (selected_ >= 0 && selected_ < static_cast<int>(apps_.size())) {
            const double lim = bandwidth_.limit(apps_[selected_].path);
            selLine = L"Limit for " + ellipsize(apps_[selected_].name, 32) + L": " +
                      (lim > 0 ? fmtMbps(lim) : std::wstring(L"none"));
        }
        drawText(hdc, selLine, bw.left + scale(20), bw.top + scale(60), 11, th_.text);
        if (bw.bottom - bw.top >= scale(70)) {
            drawButton(hdc, bwSetRect(L), TR(L"Set limit"), th_.accent, th_.accentText, hoverBtn_ == 34);
            drawButton(hdc, bwClearRect(L), TR(L"Clear limit"), th_.border, th_.text, hoverBtn_ == 35);
        }
    }
}

void Ui::paintSettings(HDC hdc, const Layout& L) {
    const int titleY = scale(24);
    drawText(hdc, TR(L"Settings"), L.contentX, titleY, 24, th_.text, true);
    drawButton(hdc, L.btnAbout, TR(L"About"), th_.border, th_.text, hoverBtn_ == 40);

    auto& s = settings_.get();
    RECT card{L.contentX, scale(70), L.contentR, scale(360)};
    drawRoundRect(hdc, card, th_.card, th_.border, scale(12));
    drawText(hdc, L"General", card.left + scale(20), card.top + scale(14), 14, th_.text, true);

    auto row = [&](int i, const std::wstring& label, const std::wstring& sub) {
        const int y = card.top + scale(48) + i * scale(52);
        drawText(hdc, label, card.left + scale(20), y, 13, th_.text, true);
        if (!sub.empty()) drawText(hdc, sub, card.left + scale(20), y + scale(20), 10, th_.textDim);
    };
    auto togRect = [&](int i) {
        const int y = card.top + scale(48) + i * scale(52);
        return RECT{card.right - scale(80), y, card.right - scale(20), y + scale(26)};
    };

    row(0, TR(L"Dark mode"), s.theme == ThemeMode::Dark ? L"Dark" : L"Light");
    drawToggle(hdc, togRect(0), s.theme == ThemeMode::Dark, hoverBtn_ == 41);
    row(1, TR(L"Auto-start"), L"HKCU Run key");
    drawToggle(hdc, togRect(1), s.autoStart, hoverBtn_ == 42);
    row(2, TR(L"Notifications"), L"Tray balloons on blocks");
    drawToggle(hdc, togRect(2), s.notifications, hoverBtn_ == 43);
    row(3, L"Auto-refresh processes", L"Interval " + std::to_wstring(s.processRefreshSec) + L"s");
    drawToggle(hdc, togRect(3), s.autoRefreshProcesses, hoverBtn_ == 44);
    row(4, L"Network profile gate", L"Only apply schedules on selected profiles");
    drawToggle(hdc, togRect(4), s.networkProfileGate, hoverBtn_ == 45);
    row(5, L"Block only on public networks", L"Safety: never block on private/domain");
    drawToggle(hdc, togRect(5), s.blockOnlyPublic, hoverBtn_ == 46);

    RECT langCard{L.contentX, scale(380), L.contentR, scale(520)};
    drawRoundRect(hdc, langCard, th_.card, th_.border, scale(12));
    drawText(hdc, TR(L"Language"), langCard.left + scale(20), langCard.top + scale(14), 14, th_.text, true);
    const wchar_t* langs[] = {L"en", L"de", L"es"};
    const wchar_t* langNames[] = {L"English", L"Deutsch", L"Espa\u00f1ol"};
    for (int i = 0; i < 3; ++i) {
        RECT r{langCard.left + scale(20) + i * scale(140), langCard.top + scale(50),
               langCard.left + scale(20) + i * scale(140) + scale(130), langCard.top + scale(90)};
        const bool on = s.language == langs[i];
        drawButton(hdc, r, langNames[i], on ? th_.accent : th_.border, on ? th_.accentText : th_.text, false);
    }

    RECT dataCard{L.contentX, scale(540), L.contentR, scale(680)};
    drawRoundRect(hdc, dataCard, th_.card, th_.border, scale(12));
    drawText(hdc, L"Data", dataCard.left + scale(20), dataCard.top + scale(14), 14, th_.text, true);
    drawText(hdc, L"Storage: %LOCALAPPDATA%\\NetControl  |  Save every " +
             std::to_wstring(s.saveIntervalSec) + L"s", dataCard.left + scale(20), dataCard.top + scale(44), 11, th_.textDim);
    drawText(hdc, bandwidth_.statusText(), dataCard.left + scale(20), dataCard.top + scale(64), 11, th_.textDim);
    drawButton(hdc, {dataCard.left + scale(20), dataCard.top + scale(90), dataCard.left + scale(160), dataCard.top + scale(124)},
               TR(L"Clear history"), th_.border, th_.text, hoverBtn_ == 47);

    RECT aboutCard{L.contentX, scale(700), L.contentR, L.contentCard.bottom};
    if (aboutCard.bottom > aboutCard.top + scale(40)) {
        drawRoundRect(hdc, aboutCard, th_.card, th_.border, scale(12));
        drawText(hdc, L"NetControl", aboutCard.left + scale(20), aboutCard.top + scale(14), 16, th_.text, true);
        drawText(hdc, L"Local-only internet control. No cloud, no account.", aboutCard.left + scale(20), aboutCard.top + scale(42), 12, th_.textDim);
        drawText(hdc, L"Ask mode is monitor-only; Block uses Windows Firewall.", aboutCard.left + scale(20), aboutCard.top + scale(62), 11, th_.textDim);
        drawText(hdc, L"Bandwidth shaping requires the WFP callout (docs/WFP-CALLOUT.md).", aboutCard.left + scale(20), aboutCard.top + scale(80), 11, th_.textDim);
    }
}

void Ui::paintContent(HDC hdc, const Layout& L) {
    HBRUSH bg = CreateSolidBrush(th_.bg);
    RECT cr{};
    GetClientRect(hwnd_, &cr);
    FillRect(hdc, &cr, bg);
    DeleteObject(bg);
    paintSidebar(hdc, L);
    switch (page_) {
    case Page::Overview:
        paintCards(hdc, L);
        paintList(hdc, L);
        paintDetail(hdc, L);
        break;
    case Page::Applications:
        paintList(hdc, L);
        paintDetail(hdc, L);
        break;
    case Page::LiveMonitor:
        paintLiveMonitor(hdc, L);
        break;
    case Page::NetworkUsage:
        paintUsage(hdc, L);
        break;
    case Page::ConnectionLogs:
        paintLogs(hdc, L);
        break;
    case Page::Rules:
        paintRules(hdc, L);
        break;
    case Page::Settings:
        paintSettings(hdc, L);
        break;
    }
}

void Ui::paint(HDC hdc) {
    RECT cr{};
    GetClientRect(hwnd_, &cr);
    if (cr.right <= 0 || cr.bottom <= 0) return;
    const Layout L = computeLayout();
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, cr.right, cr.bottom);
    HGDIOBJ old = SelectObject(mem, bmp);
    paintContent(mem, L);
    BitBlt(hdc, 0, 0, cr.right, cr.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    if (searchHwnd_) {
        const bool showSearch = (page_ == Page::Overview || page_ == Page::Applications);
        const int show = showSearch ? SW_SHOW : SW_HIDE;
        if (IsWindowVisible(searchHwnd_) != (show == SW_SHOW)) ShowWindow(searchHwnd_, show);
        if (showSearch) {
            RECT wr = L.search;
            MapWindowPoints(hwnd_, nullptr, reinterpret_cast<POINT*>(&wr), 2);
            const int inset = scale(34);
            SetWindowPos(searchHwnd_, HWND_TOP,
                         wr.left + inset, wr.top + scale(3),
                         (wr.right - wr.left) - inset - scale(10),
                         (wr.bottom - wr.top) - scale(6),
                         SWP_NOACTIVATE);
        }
    }
}

void Ui::selectApp(int index) {
    selected_ = index;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Ui::setSelectedMode(AccessMode mode) {
    if (selected_ < 0 || selected_ >= static_cast<int>(apps_.size())) return;
    if (!multiSel_.empty()) { setModesForSelection(mode); return; }
    auto& a = apps_[selected_];
    if (settings_.get().blockOnlyPublic && settings_.get().networkProfileGate &&
        mode == AccessMode::Block && !profile_.isBlockAllowed(true, true)) {
        setStatus(L"Blocked only allowed on public networks (gate enabled)", true);
        return;
    }
    std::wstring err;
    if (!fw_->setMode(a.path, mode, err)) {
        MessageBoxW(hwnd_, err.c_str(), L"NetControl", MB_ICONERROR);
        return;
    }
    a.mode = mode;
    store_->setMode(a.path, mode);
    store_->save();
    if (mode == AccessMode::Block) notifier_.notifyBlocked(a.name, L"rule updated");
    setStatus(a.name + L": " + modeName(mode));
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Ui::setModesForSelection(AccessMode mode) {
    int ok = 0;
    for (int i : multiSel_) {
        if (i < 0 || i >= static_cast<int>(apps_.size())) continue;
        auto& a = apps_[i];
        std::wstring err;
        if (fw_->setMode(a.path, mode, err)) {
            a.mode = mode;
            store_->setMode(a.path, mode);
            ++ok;
        }
    }
    store_->save();
    setStatus(std::to_wstring(ok) + L" app(s): " + modeName(mode));
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Ui::openConnectionsDialog() {
    if (selected_ < 0 || selected_ >= static_cast<int>(apps_.size())) return;
    auto& a = apps_[selected_];
    auto conns = nm_->connectionsForPid(a.pid);
    std::wostringstream os;
    os << a.name;
    if (conns.empty()) {
        os << L"\n\nNo active TCP connections observed.";
    } else {
        os << L"\n\nActive connections (" << conns.size() << L"):\n\n";
        int n = 0;
        for (auto& c : conns) {
            if (n++ >= 40) { os << L"\n\u2026 and " << (conns.size() - 40) << L" more\n"; break; }
            os << ConnectionLog::protoName(c.proto) << L"  " << c.remoteAddress << L":" << c.remotePort << L"\n";
        }
    }
    MessageBoxW(hwnd_, os.str().c_str(), L"Connections", MB_OK | MB_ICONINFORMATION);
}

void Ui::openAboutDialog() {
    std::wstring msg =
        L"NetControl\nInternet Control for Windows\n\n"
        L"Local-only storage. No cloud or account.\n"
        L"Allow/Block use Windows Firewall rules.\n"
        L"Ask mode is monitor-only (no WFP driver in user-mode).\n"
        L"Per-connection bytes via TCP EStats; UDP counted as connections only.\n\n"
        L"Developed by JOJIN JOHN\n\n"
        L"Data: %LOCALAPPDATA%\\NetControl\n"
        L"Docs: docs\\WFP-CALLOUT.md, BUILD_STATUS.md";
    MessageBoxW(hwnd_, msg.c_str(), L"About NetControl", MB_OK | MB_ICONINFORMATION);
}

void Ui::copyPathToClipboard() {
    if (selected_ < 0 || selected_ >= static_cast<int>(apps_.size())) return;
    const auto& p = apps_[selected_].path;
    if (!OpenClipboard(hwnd_)) return;
    EmptyClipboard();
    const SIZE_T bytes = (p.size() + 1) * sizeof(wchar_t);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (h) {
        void* dst = GlobalLock(h);
        if (dst) {
            memcpy(dst, p.c_str(), bytes);
            GlobalUnlock(h);
            SetClipboardData(CF_UNICODETEXT, h);
        } else GlobalFree(h);
    }
    CloseClipboard();
    setStatus(L"Path copied");
}

void Ui::showContextMenu(int x, int y, int rowIndex) {
    if (rowIndex < 0 || rowIndex >= static_cast<int>(apps_.size())) return;
    selectApp(rowIndex);
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, 4001, TR(L"Allow").c_str());
    AppendMenuW(m, MF_STRING, 4002, TR(L"Block").c_str());
    AppendMenuW(m, MF_STRING, 4003, TR(L"Ask").c_str());
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, 4004, TR(L"Copy path").c_str());
    AppendMenuW(m, MF_STRING, 4005, TR(L"Open file").c_str());
    AppendMenuW(m, MF_STRING, 4006, TR(L"Connections").c_str());
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, 4007, TR(L"Select all").c_str());
    POINT pt{x, y};
    ClientToScreen(hwnd_, &pt);
    TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(m);
}

void Ui::exportRulesUi() {
    wchar_t file[MAX_PATH] = L"netcontrol_rules.csv";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = L"CSV (*.csv)\0*.csv\0All\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"csv";
    if (GetSaveFileNameW(&ofn)) {
        std::wstring err;
        if (fw_->exportRules(file, err)) setStatus(L"Rules exported");
        else setStatus(err, true);
    }
}

void Ui::importRulesUi() {
    wchar_t file[MAX_PATH]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFilter = L"CSV (*.csv)\0*.csv\0All\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        std::wstring err;
        if (fw_->importRules(file, err)) setStatus(err);
        else setStatus(err, true);
        refreshProcessList();
    }
}

LRESULT CALLBACK Ui::wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    Ui* self = reinterpret_cast<Ui*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (m == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(l);
        self = reinterpret_cast<Ui*>(cs->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = h;
    }
    return self ? self->handle(h, m, w, l) : DefWindowProcW(h, m, w, l);
}

LRESULT Ui::handle(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_ERASEBKGND:
        return 1;
    case WM_DPICHANGED: {
        dpi_ = HIWORD(w);
        if (font_) DeleteObject(font_);
        if (fontBold_) DeleteObject(fontBold_);
        font_ = CreateFontW(-scale(16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        fontBold_ = CreateFontW(-scale(16), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        if (searchHwnd_) SendMessageW(searchHwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        auto r = reinterpret_cast<RECT*>(l);
        SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_GETMINMAXINFO: {
        auto mmi = reinterpret_cast<MINMAXINFO*>(l);
        mmi->ptMinTrackSize.x = scale(1024);
        mmi->ptMinTrackSize.y = scale(680);
        return 0;
    }
    case WM_COMMAND: {
        const int id = LOWORD(w);
        if (id == IDC_SEARCH) {
            if (HIWORD(w) == EN_CHANGE) {
                wchar_t buf[256]{};
                if (searchHwnd_) GetWindowTextW(searchHwnd_, buf, 256);
                filter_ = buf;
                scroll_ = 0;
                InvalidateRect(h, nullptr, FALSE);
            } else if (HIWORD(w) == EN_SETFOCUS) {
                searchFocus_ = true;
                if (searchHwnd_) SendMessageW(searchHwnd_, EM_SETSEL, 0, -1);
                InvalidateRect(h, nullptr, FALSE);
            } else if (HIWORD(w) == EN_KILLFOCUS) {
                searchFocus_ = false;
                InvalidateRect(h, nullptr, FALSE);
            }
            return 0;
        }
        if (id == ID_TRAY_SHOW) {
            hiddenToTray_ = false;
            ShowWindow(h, SW_SHOW);
            SetForegroundWindow(h);
            return 0;
        }
        if (id == ID_TRAY_EXIT) {
            DestroyWindow(h);
            return 0;
        }
        if (id == 4001) { setSelectedMode(AccessMode::Allow); return 0; }
        if (id == 4002) { setSelectedMode(AccessMode::Block); return 0; }
        if (id == 4003) { setSelectedMode(AccessMode::Ask); return 0; }
        if (id == 4004) { copyPathToClipboard(); return 0; }
        if (id == 4005) {
            if (selected_ >= 0 && selected_ < static_cast<int>(apps_.size()))
                ShellExecuteW(h, L"open", apps_[selected_].path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        }
        if (id == 4006) { openConnectionsDialog(); return 0; }
        if (id == 4007) {
            multiSel_.clear();
            for (int i = 0; i < static_cast<int>(apps_.size()); ++i) multiSel_.insert(i);
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        if (id == 101) { openAboutDialog(); return 0; }
        if (id == 102) {
            store_->clear();
            history_.clear();
            setStatus(L"Usage history cleared");
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        if (id == 103) {
            settings_.get().theme = settings_.get().theme == ThemeMode::Dark ? ThemeMode::Light : ThemeMode::Dark;
            applySettings();
            return 0;
        }
        if (id == 104) {
            if (hiddenToTray_ || !IsWindowVisible(h)) {
                hiddenToTray_ = false;
                ShowWindow(h, SW_SHOW);
                SetForegroundWindow(h);
            } else {
                hiddenToTray_ = true;
                ShowWindow(h, SW_HIDE);
            }
            return 0;
        }
        return 0;
    }
    case WM_TRAYICON: {
        const UINT msg = static_cast<UINT>(l);
        if (msg == WM_LBUTTONUP || msg == WM_LBUTTONDBLCLK) {
            hiddenToTray_ = false;
            ShowWindow(h, SW_SHOW);
            SetForegroundWindow(h);
        } else if (msg == WM_RBUTTONUP) {
            tray_.showMenu(h);
        }
        return 0;
    }
    case WM_HOTKEY:
        return 0;
    case WM_KEYDOWN: {
        if (w == VK_F1) { openAboutDialog(); return 0; }
        if (w == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
            multiSel_.clear();
            for (int i = 0; i < static_cast<int>(apps_.size()); ++i) multiSel_.insert(i);
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        if (w == VK_ESCAPE) {
            multiSel_.clear();
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        if (w == VK_UP || w == VK_DOWN) {
            auto idx = visibleIndices(computeLayout());
            if (idx.empty()) return 0;
            int pos = 0;
            for (int i = 0; i < static_cast<int>(idx.size()); ++i)
                if (idx[i] == selected_) { pos = i; break; }
            pos = std::clamp(pos + (w == VK_DOWN ? 1 : -1), 0, static_cast<int>(idx.size()) - 1);
            selected_ = idx[pos];
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        if (w == VK_RETURN) { openConnectionsDialog(); return 0; }
        if (w == 'C' && (GetKeyState(VK_CONTROL) & 0x8000)) { copyPathToClipboard(); return 0; }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        paint(dc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_TIMER:
        if (w == 1) refresh();
        else if (w == 2 && settings_.get().autoRefreshProcesses) refreshProcessList();
        else if (w == 3) {
            store_->save();
            settings_.save();
            connLog_.save();
            history_.save();
        }
        return 0;
    case WM_MOUSEWHEEL: {
        const int delta = GET_WHEEL_DELTA_WPARAM(w);
        POINT pt{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        ScreenToClient(h, &pt);
        const Layout L = computeLayout();
        if (page_ == Page::ConnectionLogs && hit({L.contentX, scale(70), L.contentR, L.contentCard.bottom}, pt.x, pt.y)) {
            const auto& entries = connLog_.entries();
            const int maxScroll = std::max(0, static_cast<int>(entries.size()) - 1);
            logScroll_ = std::clamp(logScroll_ - (delta / WHEEL_DELTA) * 3, 0, maxScroll);
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        if (page_ == Page::NetworkUsage && hit({L.contentX, scale(360), L.contentR, L.contentCard.bottom}, pt.x, pt.y)) {
            int total = 0;
            for (auto& a : apps_) if (a.historicalBytes > 0) ++total;
            const int maxRows = static_cast<int>((L.contentCard.bottom - scale(360) - scale(50)) / scale(34));
            const int maxScroll = (std::max)(0, total - maxRows);
            usageScroll_ = std::clamp(usageScroll_ - (delta / WHEEL_DELTA) * 3, 0, maxScroll);
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        if (page_ == Page::Rules && hit(rulesCardRect(L), pt.x, pt.y)) {
            const int maxScroll = (std::max)(0, static_cast<int>(fw_->listRules(true).size()) - rulesVisibleRows(L));
            ruleScroll_ = std::clamp(ruleScroll_ - (delta / WHEEL_DELTA), 0, maxScroll);
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        if (page_ == Page::Rules && hit(schedCardRect(L), pt.x, pt.y)) {
            const int maxScroll = (std::max)(0, static_cast<int>(schedules_.rules().size()) - schedVisibleRows(L));
            scheduleScroll_ = std::clamp(scheduleScroll_ - (delta / WHEEL_DELTA), 0, maxScroll);
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        if (page_ == Page::Overview || page_ == Page::Applications) {
            if (hit(L.listCard, pt.x, pt.y) || (page_ == Page::Overview && pt.x >= L.contentX && pt.x < L.detail.left && pt.y >= L.rowsTop)) {
                auto idx = visibleIndices(L);
                const int maxScroll = std::max(0, static_cast<int>(idx.size()) - L.visibleRows);
                scroll_ = std::clamp(scroll_ - (delta / WHEEL_DELTA) * 3, 0, maxScroll);
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
        }
        return DefWindowProcW(h, m, w, l);
    }
    case WM_MOUSEMOVE: {
        const int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
        const Layout L = computeLayout();
        int newNav = -1, newRow = -1, newBtn = 0;
        bool interactive = false;
        for (int i = 0; i < 7; ++i) {
            if (hit(L.nav[i], x, y)) { newNav = i; interactive = true; break; }
        }
        if (page_ == Page::Overview || page_ == Page::Applications) {
            auto idx = visibleIndices(L);
            if (hit(L.listCard, x, y) && y >= L.rowsTop) {
                const int n = (y - L.rowsTop) / L.rowH;
                if (n >= 0 && n < L.visibleRows && scroll_ + n < static_cast<int>(idx.size())) {
                    newRow = n;
                    interactive = true;
                }
            }
            if (hit(L.search, x, y) || hit(L.sort, x, y)) interactive = true;
            if (selected_ >= 0 && selected_ < static_cast<int>(apps_.size())) {
                if (hit(L.btnAllow, x, y)) { newBtn = 1; interactive = true; }
                else if (hit(L.btnBlock, x, y)) { newBtn = 2; interactive = true; }
                else if (hit(L.btnAsk, x, y)) { newBtn = 3; interactive = true; }
                else if (hit(L.btnOpen, x, y)) { newBtn = 4; interactive = true; }
                else if (hit(L.btnConn, x, y)) { newBtn = 5; interactive = true; }
                else if (hit(L.btnLimit, x, y)) { newBtn = 6; interactive = true; }
            }
        } else if (page_ == Page::ConnectionLogs) {
            if (hit(L.btnClear, x, y)) { newBtn = 20; interactive = true; }
            else if (hit(L.btnExport, x, y)) { newBtn = 21; interactive = true; }
            else if (hit(L.btnImport, x, y)) { newBtn = 22; interactive = true; }
        } else if (page_ == Page::Rules) {
            RECT b1{L.contentR - scale(250), scale(28), L.contentR - scale(170), scale(60)};
            RECT b2{L.contentR - scale(164), scale(28), L.contentR - scale(84), scale(60)};
            RECT b3{L.contentR - scale(78), scale(28), L.contentR, scale(60)};
            if (hit(b1, x, y)) { newBtn = 30; interactive = true; }
            else if (hit(b2, x, y)) { newBtn = 31; interactive = true; }
            else if (hit(b3, x, y)) { newBtn = 32; interactive = true; }
            else if (hit(schedAddRect(L), x, y)) { newBtn = 33; interactive = true; }
            else if (hit(bwSetRect(L), x, y)) { newBtn = 34; interactive = true; }
            else if (hit(bwClearRect(L), x, y)) { newBtn = 35; interactive = true; }
            else if (hit(rulesCardRect(L), x, y)) {
                const int n = (y - (rulesCardRect(L).top + scale(44))) / scale(34);
                if (y >= rulesCardRect(L).top + scale(44) && n >= 0 && n < rulesVisibleRows(L)) {
                    newRow = n; interactive = true;
                }
            } else if (hit(schedCardRect(L), x, y)) {
                interactive = true;
            }
        } else if (page_ == Page::Settings) {
            if (hit(L.btnAbout, x, y)) { newBtn = 40; interactive = true; }
            else {
                RECT card{L.contentX, scale(70), L.contentR, scale(360)};
                for (int i = 0; i < 6; ++i) {
                    RECT t{card.right - scale(80), card.top + scale(48) + i * scale(52),
                           card.right - scale(20), card.top + scale(48) + i * scale(52) + scale(26)};
                    if (hit(t, x, y)) { newBtn = 41 + i; interactive = true; break; }
                }
                if (!newBtn) {
                    RECT dataCard{L.contentX, scale(540), L.contentR, scale(680)};
                    RECT clr{dataCard.left + scale(20), dataCard.top + scale(90), dataCard.left + scale(160), dataCard.top + scale(124)};
                    if (hit(clr, x, y)) { newBtn = 47; interactive = true; }
                }
            }
        }
        if (newNav != hoverNav_ || newRow != hoverRow_ || newBtn != hoverBtn_) {
            hoverNav_ = newNav;
            hoverRow_ = newRow;
            hoverBtn_ = newBtn;
            InvalidateRect(h, nullptr, FALSE);
        }
        SetCursor(LoadCursor(nullptr, interactive ? IDC_HAND : IDC_ARROW));
        return 0;
    }
    case WM_RBUTTONUP: {
        const int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
        const Layout L = computeLayout();
        if ((page_ == Page::Overview || page_ == Page::Applications) && hit(L.listCard, x, y) && y >= L.rowsTop) {
            auto idx = visibleIndices(L);
            const int n = (y - L.rowsTop) / L.rowH;
            const int k = scroll_ + n;
            if (n >= 0 && n < L.visibleRows && k >= 0 && k < static_cast<int>(idx.size()))
                showContextMenu(x, y, idx[k]);
            return 0;
        }
        return 0;
    }
    case WM_SETCURSOR: {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(h, &pt);
        const Layout L = computeLayout();
        bool hand = false;
        for (int i = 0; i < 7; ++i) if (hit(L.nav[i], pt.x, pt.y)) hand = true;
        if (page_ == Page::Overview || page_ == Page::Applications) {
            if (hit(L.search, pt.x, pt.y) || hit(L.sort, pt.x, pt.y) || hit(L.listCard, pt.x, pt.y)) hand = true;
        }
        SetCursor(LoadCursor(nullptr, hand ? IDC_HAND : IDC_ARROW));
        return TRUE;
    }
    case WM_LBUTTONDOWN: {
        const int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
        const Layout L = computeLayout();
        SetFocus(h);
        if (page_ == Page::Overview || page_ == Page::Applications) {
            if (hit(L.listCard, x, y) && y >= L.rowsTop) {
                auto idx = visibleIndices(L);
                const int n = (y - L.rowsTop) / L.rowH;
                const int k = scroll_ + n;
                if (n >= 0 && n < L.visibleRows && k >= 0 && k < static_cast<int>(idx.size())) {
                    const int ai = idx[k];
                    if (GetKeyState(VK_CONTROL) & 0x8000) {
                        if (multiSel_.count(ai)) multiSel_.erase(ai);
                        else multiSel_.insert(ai);
                        selected_ = ai;
                        InvalidateRect(h, nullptr, FALSE);
                        return 0;
                    }
                    multiSel_.clear();
                    selectApp(ai);
                    return 0;
                }
            }
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        const int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
        const Layout L = computeLayout();
        for (int i = 0; i < 7; ++i) {
            if (hit(L.nav[i], x, y)) {
                page_ = static_cast<Page>(i);
                scroll_ = 0;
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
        }
        if (page_ == Page::Overview || page_ == Page::Applications) {
            if (hit(L.sort, x, y)) {
                sortMode_ = (sortMode_ + 1) % 3;
                scroll_ = 0;
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
            if (selected_ >= 0 && selected_ < static_cast<int>(apps_.size())) {
                if (hit(L.btnAllow, x, y)) { setSelectedMode(AccessMode::Allow); return 0; }
                if (hit(L.btnBlock, x, y)) { setSelectedMode(AccessMode::Block); return 0; }
                if (hit(L.btnAsk, x, y)) { setSelectedMode(AccessMode::Ask); return 0; }
                if (hit(L.btnOpen, x, y)) {
                    ShellExecuteW(h, L"open", apps_[selected_].path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    return 0;
                }
                if (hit(L.btnConn, x, y)) { openConnectionsDialog(); return 0; }
                if (hit(L.btnLimit, x, y)) { promptBandwidthLimit(); return 0; }
            }
        } else if (page_ == Page::ConnectionLogs) {
            if (hit(L.btnClear, x, y)) { connLog_.clear(); setStatus(L"Logs cleared"); InvalidateRect(h, nullptr, FALSE); return 0; }
            if (hit(L.btnExport, x, y)) {
                wchar_t file[MAX_PATH] = L"netcontrol_logs.csv";
                OPENFILENAMEW ofn{};
                ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = h;
                ofn.lpstrFilter = L"CSV (*.csv)\0*.csv\0"; ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_OVERWRITEPROMPT; ofn.lpstrDefExt = L"csv";
                if (GetSaveFileNameW(&ofn)) {
                    const bool ok = connLog_.exportCsv(file);
                    setStatus(ok ? L"Logs exported" : L"Export failed", !ok);
                }
                return 0;
            }
            if (hit(L.btnImport, x, y)) {
                wchar_t file[MAX_PATH]{};
                OPENFILENAMEW ofn{};
                ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = h;
                ofn.lpstrFilter = L"CSV (*.csv)\0*.csv\0"; ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_FILEMUSTEXIST;
                if (GetOpenFileNameW(&ofn)) {
                    connLog_.importCsv(file);
                    setStatus(L"Logs imported");
                    InvalidateRect(h, nullptr, FALSE);
                }
                return 0;
            }
        } else if (page_ == Page::Rules) {
            RECT b1{L.contentR - scale(250), scale(28), L.contentR - scale(170), scale(60)};
            RECT b2{L.contentR - scale(164), scale(28), L.contentR - scale(84), scale(60)};
            RECT b3{L.contentR - scale(78), scale(28), L.contentR, scale(60)};
            if (hit(b1, x, y)) { exportRulesUi(); return 0; }
            if (hit(b2, x, y)) { importRulesUi(); return 0; }
            if (hit(b3, x, y)) { cleanupStale(); return 0; }
            {
                RECT rc = rulesCardRect(L);
                if (hit(rc, x, y)) {
                    auto rl = fw_->listRules(true);
                    const int n = (y - (rc.top + scale(44))) / scale(34);
                    const int idx = ruleScroll_ + n;
                    if (y >= rc.top + scale(44) && n >= 0 && n < rulesVisibleRows(L) &&
                        idx >= 0 && idx < static_cast<int>(rl.size())) {
                        if (hit(ruleRowToggle(L, n), x, y)) {
                            std::wstring err;
                            if (fw_->setRuleEnabled(rl[idx].name, !rl[idx].enabled, err)) {
                                setStatus(rl[idx].enabled ? L"Rule disabled" : L"Rule enabled");
                            } else {
                                setStatus(err.empty() ? L"Failed to change rule" : err, true);
                            }
                            InvalidateRect(h, nullptr, FALSE);
                            return 0;
                        }
                        if (hit(ruleRowDelete(L, n), x, y)) {
                            std::wstring err;
                            const std::wstring appName = rl[idx].application;
                            if (fw_->removeRuleByName(rl[idx].name, err)) {
                                if (!appName.empty()) {
                                    store_->setMode(appName, AccessMode::Ask);
                                    store_->save();
                                    for (auto& ap : apps_)
                                        if (_wcsicmp(ap.path.c_str(), appName.c_str()) == 0) ap.mode = AccessMode::Ask;
                                }
                                setStatus(L"Rule removed");
                            } else {
                                setStatus(err.empty() ? L"Failed to remove rule" : err, true);
                            }
                            InvalidateRect(h, nullptr, FALSE);
                            return 0;
                        }
                    }
                }
            }
            if (hit(schedAddRect(L), x, y)) { addScheduleForSelected(); return 0; }
            {
                RECT sc = schedCardRect(L);
                if (hit(sc, x, y)) {
                    auto& sr = schedules_.rules();
                    const int n = (y - (sc.top + scale(56))) / scale(30);
                    const int idx = scheduleScroll_ + n;
                    if (y >= sc.top + scale(56) && n >= 0 && n < schedVisibleRows(L) &&
                        idx >= 0 && idx < static_cast<int>(sr.size())) {
                        SchedRow r = schedRowRects(L, n);
                        auto& rule = sr[idx];
                        if (hit(r.mode, x, y)) {
                            rule.mode = rule.mode == AccessMode::Block ? AccessMode::Allow : AccessMode::Block;
                            schedules_.save();
                        } else if (hit(r.sMinus, x, y)) {
                            rule.startHour = (rule.startHour + 23) % 24;
                            schedules_.save();
                        } else if (hit(r.sPlus, x, y)) {
                            rule.startHour = (rule.startHour + 1) % 24;
                            schedules_.save();
                        } else if (hit(r.eMinus, x, y)) {
                            rule.endHour = rule.endHour <= 1 ? 24 : rule.endHour - 1;
                            schedules_.save();
                        } else if (hit(r.ePlus, x, y)) {
                            rule.endHour = rule.endHour >= 24 ? 1 : rule.endHour + 1;
                            schedules_.save();
                        } else if (hit(r.sVal, x, y) || hit(r.eVal, x, y)) {
                            editScheduleHours(idx);
                        } else if (hit(r.tog, x, y)) {
                            rule.enabled = !rule.enabled;
                            schedules_.save();
                        } else if (hit(r.del, x, y)) {
                            schedules_.remove(static_cast<size_t>(idx));
                            setStatus(L"Schedule removed");
                        }
                        applySchedules();
                        InvalidateRect(h, nullptr, FALSE);
                        return 0;
                    }
                }
            }
            if (hit(bwSetRect(L), x, y)) { promptBandwidthLimit(); return 0; }
            if (hit(bwClearRect(L), x, y)) {
                if (selected_ >= 0 && selected_ < static_cast<int>(apps_.size())) {
                    bandwidth_.remove(apps_[selected_].path);
                    bandwidth_.save();
                    setStatus(L"Bandwidth limit cleared");
                    InvalidateRect(h, nullptr, FALSE);
                }
                return 0;
            }
        } else if (page_ == Page::Settings) {
            if (hit(L.btnAbout, x, y)) { openAboutDialog(); return 0; }
            RECT card{L.contentX, scale(70), L.contentR, scale(360)};
            auto tog = [&](int i) {
                return RECT{card.right - scale(80), card.top + scale(48) + i * scale(52),
                            card.right - scale(20), card.top + scale(48) + i * scale(52) + scale(26)};
            };
            if (hit(tog(0), x, y)) {
                settings_.get().theme = settings_.get().theme == ThemeMode::Dark ? ThemeMode::Light : ThemeMode::Dark;
                applySettings(); return 0;
            }
            if (hit(tog(1), x, y)) {
                settings_.get().autoStart = !settings_.get().autoStart;
                applySettings();
                setStatus(settings_.get().autoStart ? L"Auto-start enabled" : L"Auto-start disabled");
                return 0;
            }
            if (hit(tog(2), x, y)) { settings_.get().notifications = !settings_.get().notifications; applySettings(); return 0; }
            if (hit(tog(3), x, y)) { settings_.get().autoRefreshProcesses = !settings_.get().autoRefreshProcesses; applySettings(); return 0; }
            if (hit(tog(4), x, y)) { settings_.get().networkProfileGate = !settings_.get().networkProfileGate; applySettings(); return 0; }
            if (hit(tog(5), x, y)) { settings_.get().blockOnlyPublic = !settings_.get().blockOnlyPublic; applySettings(); return 0; }
            RECT langCard{L.contentX, scale(380), L.contentR, scale(520)};
            const wchar_t* langs[] = {L"en", L"de", L"es"};
            for (int i = 0; i < 3; ++i) {
                RECT r{langCard.left + scale(20) + i * scale(140), langCard.top + scale(50),
                       langCard.left + scale(20) + i * scale(140) + scale(130), langCard.top + scale(90)};
                if (hit(r, x, y)) { settings_.get().language = langs[i]; applySettings(); return 0; }
            }
            RECT dataCard{L.contentX, scale(540), L.contentR, scale(680)};
            RECT clr{dataCard.left + scale(20), dataCard.top + scale(90), dataCard.left + scale(160), dataCard.top + scale(124)};
            if (hit(clr, x, y)) {
                store_->clear();
                history_.clear();
                setStatus(L"Usage history cleared");
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
        }
        return 0;
    }
    case WM_SIZE:
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_SYSCOMMAND:
        if ((w & 0xFFF0) == SC_MINIMIZE) {
            hiddenToTray_ = true;
            ShowWindow(h, SW_HIDE);
            return 0;
        }
        return DefWindowProcW(h, m, w, l);
    case WM_CLOSE:
        hiddenToTray_ = true;
        ShowWindow(h, SW_HIDE);
        return 0;
    case WM_DESTROY:
        ProcessManager::freeIcons(apps_);
        store_->save();
        settings_.save();
        connLog_.save();
        history_.save();
        schedules_.save();
        tray_.remove();
        if (timer_) KillTimer(h, timer_);
        if (procTimer_) KillTimer(h, procTimer_);
        if (saveTimer_) KillTimer(h, saveTimer_);
        if (font_) DeleteObject(font_);
        if (fontBold_) DeleteObject(fontBold_);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int Ui::run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (searchHwnd_ && IsDialogMessageW(searchHwnd_, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
