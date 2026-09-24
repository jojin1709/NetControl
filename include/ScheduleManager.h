#pragma once
#include "Models.h"
#include <string>
#include <vector>
#include <utility>

class ScheduleManager {
public:
    bool load();
    bool save() const;
    std::vector<ScheduleRule>& rules() { return rules_; }
    const std::vector<ScheduleRule>& rules() const { return rules_; }
    void add(const ScheduleRule& r);
    void remove(size_t index);
    // Returns path -> mode for rules active right now
    std::vector<std::pair<std::wstring, AccessMode>> activeNow() const;
private:
    std::wstring path() const;
    std::vector<ScheduleRule> rules_;
};
