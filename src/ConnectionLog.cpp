#include "ConnectionLog.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <ctime>

std::wstring ConnectionLog::protoName(Proto p) {
    switch (p) {
        case Proto::TCP4: return L"TCP/IPv4";
        case Proto::TCP6: return L"TCP/IPv6";
        case Proto::UDP4: return L"UDP/IPv4";
        case Proto::UDP6: return L"UDP/IPv6";
    }
    return L"?";
}

static std::wstring logDir() {
    wchar_t dir[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, dir))) return L".";
    std::wstring p = std::wstring(dir) + L"\\NetControl";
    CreateDirectoryW(p.c_str(), nullptr);
    return p;
}

std::wstring ConnectionLog::path() const { return logDir() + L"\\connection_log.csv"; }

bool ConnectionLog::load() {
    entries_.clear();
    std::wifstream in(path());
    if (!in) return true;
    std::wstring line;
    std::getline(in, line);
    while (std::getline(in, line)) {
        LogEntry e;
        size_t p = 0, s = 0;
        auto next = [&]() {
            s = line.find(L',', p);
            std::wstring f = line.substr(p, s == std::wstring::npos ? std::wstring::npos : s - p);
            p = (s == std::wstring::npos) ? line.size() : s + 1;
            return f;
        };
        try { e.timestamp = std::stoll(next()); } catch (...) { continue; }
        try { e.pid = static_cast<DWORD>(std::stoul(next())); } catch (...) { e.pid = 0; }
        e.app = next();
        e.remote = next();
        try { e.port = static_cast<uint16_t>(std::stoul(next())); } catch (...) { e.port = 0; }
        e.action = next();
        std::wstring pr = next();
        e.proto = pr == L"TCP/IPv6" ? Proto::TCP6 : pr == L"UDP/IPv4" ? Proto::UDP4 :
                  pr == L"UDP/IPv6" ? Proto::UDP6 : Proto::TCP4;
        entries_.push_back(std::move(e));
    }
    while (entries_.size() > kMax) entries_.pop_front();
    return true;
}

bool ConnectionLog::save() const {
    std::wofstream out(path(), std::ios::trunc);
    if (!out) return false;
    out << L"timestamp,pid,app,remote,port,action,proto\n";
    for (auto& e : entries_) {
        out << e.timestamp << L',' << e.pid << L',' << e.app << L',' << e.remote << L','
            << e.port << L',' << e.action << L',' << protoName(e.proto) << L'\n';
    }
    return true;
}

void ConnectionLog::append(const LogEntry& e) {
    entries_.push_back(e);
    while (entries_.size() > kMax) entries_.pop_front();
}

void ConnectionLog::clear() { entries_.clear(); save(); }

void ConnectionLog::importCsv(const std::wstring& path) {
    std::wifstream in(path);
    if (!in) return;
    std::wstring line;
    std::getline(in, line);
    while (std::getline(in, line)) {
        LogEntry e{};
        size_t p = 0, s = 0;
        auto next = [&]() {
            s = line.find(L',', p);
            std::wstring f = line.substr(p, s == std::wstring::npos ? std::wstring::npos : s - p);
            p = (s == std::wstring::npos) ? line.size() : s + 1;
            return f;
        };
        try { e.timestamp = std::stoll(next()); } catch (...) { continue; }
        try { e.pid = static_cast<DWORD>(std::stoul(next())); } catch (...) {}
        e.app = next(); e.remote = next();
        try { e.port = static_cast<uint16_t>(std::stoul(next())); } catch (...) {}
        e.action = next();
        e.proto = Proto::TCP4;
        entries_.push_back(std::move(e));
    }
    while (entries_.size() > kMax) entries_.pop_front();
    save();
}

bool ConnectionLog::exportCsv(const std::wstring& dst) const {
    std::wofstream out(dst, std::ios::trunc);
    if (!out) return false;
    out << L"timestamp,pid,app,remote,port,action,proto\n";
    for (auto& e : entries_)
        out << e.timestamp << L',' << e.pid << L',' << e.app << L',' << e.remote << L','
            << e.port << L',' << e.action << L',' << protoName(e.proto) << L'\n';
    return true;
}
