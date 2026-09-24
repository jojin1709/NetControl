#include "Settings.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>

static std::wstring cfgDir() {
    wchar_t dir[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, dir))) return L".";
    std::wstring p = std::wstring(dir) + L"\\NetControl";
    CreateDirectoryW(p.c_str(), nullptr);
    return p;
}

std::wstring SettingsStore::path() const { return cfgDir() + L"\\settings.ini"; }

bool SettingsStore::load() {
    std::wifstream in(path());
    if (!in) return false;
    std::wstring line;
    while (std::getline(in, line)) {
        auto eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == L"theme") s_.theme = (v == L"dark") ? ThemeMode::Dark : ThemeMode::Light;
        else if (k == L"autoStart") s_.autoStart = (v == L"1");
        else if (k == L"startMinimized") s_.startMinimized = (v == L"1");
        else if (k == L"notifications") s_.notifications = (v == L"1");
        else if (k == L"autoRefreshProcesses") s_.autoRefreshProcesses = (v != L"0");
        else if (k == L"networkProfileGate") s_.networkProfileGate = (v == L"1");
        else if (k == L"blockOnlyPublic") s_.blockOnlyPublic = (v == L"1");
        else if (k == L"language") s_.language = v;
        else if (k == L"saveIntervalSec") { try { s_.saveIntervalSec = std::stoi(v); } catch (...) {} }
        else if (k == L"processRefreshSec") { try { s_.processRefreshSec = std::stoi(v); } catch (...) {} }
        else if (k == L"defaultBandwidthLimit") { try { s_.defaultBandwidthLimit = std::stod(v); } catch (...) {} }
    }
    return true;
}

bool SettingsStore::save() const {
    std::wofstream out(path(), std::ios::trunc);
    if (!out) return false;
    out << L"theme=" << (s_.theme == ThemeMode::Dark ? L"dark" : L"light") << L"\n";
    out << L"autoStart=" << (s_.autoStart ? 1 : 0) << L"\n";
    out << L"startMinimized=" << (s_.startMinimized ? 1 : 0) << L"\n";
    out << L"notifications=" << (s_.notifications ? 1 : 0) << L"\n";
    out << L"autoRefreshProcesses=" << (s_.autoRefreshProcesses ? 1 : 0) << L"\n";
    out << L"networkProfileGate=" << (s_.networkProfileGate ? 1 : 0) << L"\n";
    out << L"blockOnlyPublic=" << (s_.blockOnlyPublic ? 1 : 0) << L"\n";
    out << L"language=" << s_.language << L"\n";
    out << L"saveIntervalSec=" << s_.saveIntervalSec << L"\n";
    out << L"processRefreshSec=" << s_.processRefreshSec << L"\n";
    out << L"defaultBandwidthLimit=" << s_.defaultBandwidthLimit << L"\n";
    return true;
}
