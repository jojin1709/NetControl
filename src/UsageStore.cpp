#include "UsageStore.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>

std::wstring UsageStore::dirPath() const {
    wchar_t dir[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, dir))) return L".";
    std::wstring p = std::wstring(dir) + L"\\NetControl";
    CreateDirectoryW(p.c_str(), nullptr);
    return p;
}
std::wstring UsageStore::usagePath() const { return dirPath() + L"\\usage.tsv"; }
std::wstring UsageStore::policyPath() const { return dirPath() + L"\\policies.tsv"; }

bool UsageStore::load() {
    values_.clear(); modes_.clear(); dirty_ = false;
    {
        std::wifstream in(usagePath());
        std::wstring line;
        while (std::getline(in, line)) {
            auto t = line.find(L'\t');
            if (t == std::wstring::npos) continue;
            try { values_[line.substr(0, t)] = std::stoull(line.substr(t + 1)); } catch (...) {}
        }
    }
    {
        std::wifstream in(policyPath());
        std::wstring line;
        while (std::getline(in, line)) {
            auto t = line.find(L'\t');
            if (t == std::wstring::npos) continue;
            try {
                int v = std::stoi(line.substr(t + 1));
                modes_[line.substr(0, t)] = v == 0 ? AccessMode::Allow : v == 1 ? AccessMode::Block : AccessMode::Ask;
            } catch (...) {}
        }
    }
    return true;
}

bool UsageStore::save() const {
    std::wofstream u(usagePath(), std::ios::trunc);
    if (!u) return false;
    for (auto& [p, v] : values_) u << p << L'\t' << v << L'\n';
    std::wofstream p(policyPath(), std::ios::trunc);
    if (!p) return false;
    for (auto& [path, m] : modes_)
        p << path << L'\t' << (m == AccessMode::Allow ? 0 : m == AccessMode::Block ? 1 : 2) << L'\n';
    dirty_ = false;
    return true;
}

uint64_t UsageStore::get(const std::wstring& path) const {
    auto it = values_.find(path);
    return it == values_.end() ? 0 : it->second;
}
void UsageStore::set(const std::wstring& path, uint64_t bytes) {
    if (!path.empty()) { values_[path] = bytes; dirty_ = true; }
}
void UsageStore::add(const std::wstring& path, uint64_t bytes) {
    if (!path.empty() && bytes) { values_[path] += bytes; dirty_ = true; }
}
AccessMode UsageStore::getMode(const std::wstring& path) const {
    auto it = modes_.find(path);
    return it == modes_.end() ? AccessMode::Ask : it->second;
}
void UsageStore::setMode(const std::wstring& path, AccessMode mode) {
    if (!path.empty()) { modes_[path] = mode; dirty_ = true; }
}
void UsageStore::removeMode(const std::wstring& path) { modes_.erase(path); dirty_ = true; }
void UsageStore::clear() { values_.clear(); modes_.clear(); dirty_ = true; save(); }
void UsageStore::clearUsage() { values_.clear(); dirty_ = true; save(); }
