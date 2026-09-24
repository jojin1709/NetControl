#pragma once
#include <string>
#include <unordered_map>

// Best-effort per-app bandwidth limit registry.
// True enforcement requires a WFP callout (see docs/WFP-CALLOUT.md).
// When WFP module is present, limits are applied; otherwise status reports pending.
class BandwidthManager {
public:
    void setLimit(const std::wstring& path, double mbps);
    double limit(const std::wstring& path) const;
    void remove(const std::wstring& path);
    const std::unordered_map<std::wstring, double>& all() const { return limits_; }
    bool load();
    bool save() const;
    bool enforcementAvailable() const { return wfpLoaded_; }
    void setWfpLoaded(bool v) { wfpLoaded_ = v; }
    std::wstring statusText() const;
private:
    std::wstring path() const;
    std::unordered_map<std::wstring, double> limits_;
    bool wfpLoaded_{false};
};
