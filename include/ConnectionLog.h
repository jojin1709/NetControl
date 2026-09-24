#pragma once
#include "Models.h"
#include <string>
#include <vector>
#include <deque>

class ConnectionLog {
public:
    bool load();
    bool save() const;
    void append(const LogEntry& e);
    const std::deque<LogEntry>& entries() const { return entries_; }
    void clear();
    void importCsv(const std::wstring& path);
    bool exportCsv(const std::wstring& path) const;
    static std::wstring protoName(Proto p);
private:
    std::wstring path() const;
    std::deque<LogEntry> entries_;
    static constexpr size_t kMax = 2000;
};
