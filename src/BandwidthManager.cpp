#include "BandwidthManager.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>

static std::wstring bwDir() {
    wchar_t dir[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, dir))) return L".";
    std::wstring p = std::wstring(dir) + L"\\NetControl";
    CreateDirectoryW(p.c_str(), nullptr);
    return p;
}

std::wstring BandwidthManager::path() const { return bwDir() + L"\\bandwidth.tsv"; }

void BandwidthManager::setLimit(const std::wstring& p, double mbps) {
    if (mbps <= 0) limits_.erase(p);
    else limits_[p] = mbps;
    save();
}

double BandwidthManager::limit(const std::wstring& p) const {
    auto it = limits_.find(p);
    return it == limits_.end() ? 0.0 : it->second;
}

void BandwidthManager::remove(const std::wstring& p) { limits_.erase(p); save(); }

bool BandwidthManager::load() {
    limits_.clear();
    std::wifstream in(path());
    std::wstring line;
    while (std::getline(in, line)) {
        auto t = line.find(L'\t');
        if (t == std::wstring::npos) continue;
        try { limits_[line.substr(0, t)] = std::stod(line.substr(t + 1)); } catch (...) {}
    }
    return true;
}

bool BandwidthManager::save() const {
    std::wofstream out(path(), std::ios::trunc);
    if (!out) return false;
    for (auto& [p, v] : limits_) out << p << L'\t' << v << L'\n';
    return true;
}

std::wstring BandwidthManager::statusText() const {
    if (limits_.empty()) return L"No bandwidth limits configured";
    if (!wfpLoaded_) return std::to_wstring(limits_.size()) + L" limit(s) saved \u2014 WFP callout not loaded (see docs)";
    return std::to_wstring(limits_.size()) + L" limit(s) enforced via WFP";
}
