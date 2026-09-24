#include "UsageHistory.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <ctime>
#include <algorithm>

static std::wstring histDir() {
    wchar_t dir[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, dir))) return L".";
    std::wstring p = std::wstring(dir) + L"\\NetControl";
    CreateDirectoryW(p.c_str(), nullptr);
    return p;
}

std::wstring UsageHistory::path() const { return histDir() + L"\\usage_history.tsv"; }

static std::wstring today() {
    wchar_t b[32]{};
    SYSTEMTIME st{};
    GetLocalTime(&st);
    swprintf(b, 32, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
    return b;
}

bool UsageHistory::load() {
    days_.clear();
    std::wifstream in(path());
    std::wstring line;
    while (std::getline(in, line)) {
        auto t = line.find(L'\t');
        if (t == std::wstring::npos) continue;
        UsageDay d;
        d.date = line.substr(0, t);
        try { d.bytes = std::stoull(line.substr(t + 1)); } catch (...) { continue; }
        days_.push_back(d);
    }
    if (!days_.empty()) { lastDate_ = days_.back().date; lastTotal_ = days_.back().bytes; }
    return true;
}

bool UsageHistory::save() const {
    std::wofstream out(path(), std::ios::trunc);
    if (!out) return false;
    for (auto& d : days_) out << d.date << L'\t' << d.bytes << L'\n';
    return true;
}

void UsageHistory::addSample(uint64_t totalBytesNow) {
    std::wstring t = today();
    if (t != lastDate_) {
        UsageDay d{t, 0};
        days_.push_back(d);
        lastDate_ = t;
        lastTotal_ = totalBytesNow;
        if (days_.size() > 90) days_.erase(days_.begin());
    }
    if (totalBytesNow >= lastTotal_) {
        days_.back().bytes += (totalBytesNow - lastTotal_);
        lastTotal_ = totalBytesNow;
    } else {
        lastTotal_ = totalBytesNow;
    }
    save();
}

void UsageHistory::clear() { days_.clear(); lastTotal_ = 0; lastDate_.clear(); save(); }
