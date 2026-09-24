#pragma once
#include "Models.h"
#include <string>
#include <vector>

struct FirewallRuleInfo {
    std::wstring name;
    std::wstring application;
    bool enabled{true};
    bool netControl{false};
    void* rule{};
};

class FirewallManager {
public:
    FirewallManager();
    ~FirewallManager();
    bool initialize(std::wstring& error);
    bool isElevated() const;
    bool setMode(const std::wstring& exePath, AccessMode mode, std::wstring& error);
    AccessMode getMode(const std::wstring& exePath) const;
    bool removeRule(const std::wstring& exePath, std::wstring& error);
    std::vector<FirewallRuleInfo> listRules(bool netControlOnly = false) const;
    bool setRuleEnabled(const std::wstring& ruleName, bool enabled, std::wstring& error);
    bool removeRuleByName(const std::wstring& ruleName, std::wstring& error);
    int cleanupStaleRules(const std::vector<std::wstring>& livePaths, std::wstring& error);
    bool importRules(const std::wstring& csvPath, std::wstring& error);
    bool exportRules(const std::wstring& csvPath, std::wstring& error) const;
private:
    void* policy_{};
    std::wstring ruleName(const std::wstring& path) const;
    bool setBlockRule(const std::wstring& path, bool block, std::wstring& error);
};
