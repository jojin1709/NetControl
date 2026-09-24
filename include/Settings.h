#pragma once
#include "Theme.h"
#include "Models.h"
#include <string>

struct AppSettings {
    ThemeMode theme{ThemeMode::Light};
    bool autoStart{false};
    bool startMinimized{false};
    bool notifications{true};
    bool autoRefreshProcesses{true};
    bool networkProfileGate{false};
    bool blockOnlyPublic{false};
    std::wstring language{L"en"};
    int saveIntervalSec{30};
    int processRefreshSec{5};
    double defaultBandwidthLimit{0.0};
};

class SettingsStore {
public:
    bool load();
    bool save() const;
    AppSettings& get() { return s_; }
    const AppSettings& get() const { return s_; }
    void set(const AppSettings& v) { s_ = v; }
private:
    std::wstring path() const;
    AppSettings s_{};
};
