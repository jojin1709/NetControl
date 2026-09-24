#include "ScheduleManager.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <ctime>

static std::wstring schDir() {
    wchar_t dir[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, dir))) return L".";
    std::wstring p = std::wstring(dir) + L"\\NetControl";
    CreateDirectoryW(p.c_str(), nullptr);
    return p;
}

std::wstring ScheduleManager::path() const { return schDir() + L"\\schedules.tsv"; }

bool ScheduleManager::load() {
    rules_.clear();
    std::wifstream in(path());
    std::wstring line;
    while (std::getline(in, line)) {
        ScheduleRule r;
        size_t p = 0, s;
        auto next = [&]() {
            s = line.find(L'\t', p);
            std::wstring f = line.substr(p, s == std::wstring::npos ? std::wstring::npos : s - p);
            p = (s == std::wstring::npos) ? line.size() : s + 1;
            return f;
        };
        r.path = next();
        try { int m = std::stoi(next()); r.mode = m == 0 ? AccessMode::Allow : m == 1 ? AccessMode::Block : AccessMode::Ask; } catch (...) {}
        try { r.startHour = std::stoi(next()); } catch (...) {}
        try { r.endHour = std::stoi(next()); } catch (...) {}
        try { r.enabled = std::stoi(next()) != 0; } catch (...) {}
        if (!r.path.empty()) rules_.push_back(r);
    }
    return true;
}

bool ScheduleManager::save() const {
    std::wofstream out(path(), std::ios::trunc);
    if (!out) return false;
    for (auto& r : rules_) {
        out << r.path << L'\t'
            << (r.mode == AccessMode::Allow ? 0 : r.mode == AccessMode::Block ? 1 : 2) << L'\t'
            << r.startHour << L'\t' << r.endHour << L'\t' << (r.enabled ? 1 : 0) << L'\n';
    }
    return true;
}

void ScheduleManager::add(const ScheduleRule& r) { rules_.push_back(r); save(); }
void ScheduleManager::remove(size_t i) { if (i < rules_.size()) { rules_.erase(rules_.begin() + i); save(); } }

std::vector<std::pair<std::wstring, AccessMode>> ScheduleManager::activeNow() const {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    int hour = st.wHour;
    std::vector<std::pair<std::wstring, AccessMode>> out;
    for (auto& r : rules_) {
        if (!r.enabled) continue;
        bool active = (r.startHour <= r.endHour) ? (hour >= r.startHour && hour < r.endHour)
                                                 : (hour >= r.startHour || hour < r.endHour);
        if (active) out.emplace_back(r.path, r.mode);
    }
    return out;
}
