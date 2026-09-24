#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct UsageDay {
    std::wstring date;
    uint64_t bytes{};
};

class UsageHistory {
public:
    bool load();
    bool save() const;
    void addSample(uint64_t totalBytesNow);
    const std::vector<UsageDay>& days() const { return days_; }
    void clear();
private:
    std::wstring path() const;
    std::vector<UsageDay> days_;
    uint64_t lastTotal_{};
    std::wstring lastDate_;
};
