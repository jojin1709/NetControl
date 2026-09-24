#include "UsageStore.h"
#include "Settings.h"
#include "L10n.h"
#include "ScheduleManager.h"
#include "Theme.h"
#include "BandwidthManager.h"
#include "ConnectionLog.h"
#include "UsageHistory.h"
#include <cassert>
#include <iostream>
#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAIL: " #cond " line " << __LINE__ << "\n"; ++failures; } } while (0)

static void testUsageStore() {
    UsageStore s;
    CHECK(s.load());
    s.clear();
    s.add(L"C:\\test\\app.exe", 12345);
    CHECK(s.get(L"C:\\test\\app.exe") == 12345);
    s.setMode(L"C:\\test\\app.exe", AccessMode::Block);
    CHECK(s.getMode(L"C:\\test\\app.exe") == AccessMode::Block);
    CHECK(s.save());
    UsageStore s2;
    CHECK(s2.load());
    CHECK(s2.get(L"C:\\test\\app.exe") == 12345);
    CHECK(s2.getMode(L"C:\\test\\app.exe") == AccessMode::Block);
    s2.clearUsage();
    CHECK(s2.get(L"C:\\test\\app.exe") == 0);
    CHECK(s2.getMode(L"C:\\test\\app.exe") == AccessMode::Block);
    s2.clear();
}

static void testSettings() {
    SettingsStore st;
    st.load();
    auto& s = st.get();
    s.theme = ThemeMode::Dark;
    s.autoStart = true;
    s.language = L"de";
    CHECK(st.save());
    SettingsStore st2;
    CHECK(st2.load());
    CHECK(st2.get().theme == ThemeMode::Dark);
    CHECK(st2.get().autoStart);
    CHECK(st2.get().language == L"de");
}

static void testL10n() {
    auto& L = L10n::instance();
    L.setLanguage(L"en");
    CHECK(L.tr(L"Settings") == L"Settings");
    L.setLanguage(L"de");
    CHECK(L.tr(L"Settings") == L"Einstellungen");
    L.setLanguage(L"es");
    CHECK(L.tr(L"Settings") == L"Configuraci\u00f3n");
    L.setLanguage(L"en");
}

static void testTheme() {
    Theme light = makeTheme(ThemeMode::Light);
    Theme dark = makeTheme(ThemeMode::Dark);
    CHECK(light.bg != dark.bg);
    CHECK(light.mode == ThemeMode::Light);
    CHECK(dark.mode == ThemeMode::Dark);
}

static void testBandwidth() {
    BandwidthManager b;
    b.setLimit(L"C:\\x.exe", 5.5);
    CHECK(b.limit(L"C:\\x.exe") == 5.5);
    b.setLimit(L"C:\\x.exe", 0);
    CHECK(b.limit(L"C:\\x.exe") == 0.0);
    b.setLimit(L"C:\\y.exe", 2.0);
    CHECK(!b.enforcementAvailable());
    CHECK(b.statusText().find(L"WFP") != std::wstring::npos);
    b.remove(L"C:\\y.exe");
}

static void testSchedule() {
    ScheduleManager m;
    m.rules().clear();
    ScheduleRule r;
    r.path = L"C:\\game.exe";
    r.mode = AccessMode::Block;
    SYSTEMTIME st{};
    GetLocalTime(&st);
    r.startHour = st.wHour;
    r.endHour = st.wHour + 1 > 23 ? 24 : st.wHour + 1;
    r.enabled = true;
    m.add(r);
    auto active = m.activeNow();
    CHECK(!active.empty());
    CHECK(active[0].second == AccessMode::Block);
    m.remove(0);
    CHECK(m.rules().empty());
}

static void testConnectionLog() {
    ConnectionLog log;
    log.clear();
    LogEntry e;
    e.timestamp = 1000;
    e.pid = 42;
    e.app = L"app.exe";
    e.remote = L"1.2.3.4";
    e.port = 443;
    e.action = L"Block";
    e.proto = Proto::TCP6;
    log.append(e);
    CHECK(log.entries().size() == 1);
    CHECK(ConnectionLog::protoName(Proto::UDP4) == L"UDP/IPv4");
    CHECK(log.save());
    log.clear();
    CHECK(log.entries().empty());
}

static void testUsageHistory() {
    UsageHistory h;
    h.clear();
    h.addSample(1000);
    h.addSample(2000);
    CHECK(!h.days().empty());
    CHECK(h.days().back().bytes >= 1000);
    h.clear();
}

int main() {
    testUsageStore();
    testSettings();
    testL10n();
    testTheme();
    testBandwidth();
    testSchedule();
    testConnectionLog();
    testUsageHistory();
    if (failures == 0) {
        std::cout << "All tests passed\n";
        return 0;
    }
    std::cout << failures << " test(s) failed\n";
    return 1;
}
