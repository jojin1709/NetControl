#pragma once
#include "Models.h"
#include <string>
#include <unordered_map>
#include <cstdint>

class UsageStore {
public:
    bool load();
    bool save() const;
    uint64_t get(const std::wstring& path) const;
    void set(const std::wstring& path, uint64_t bytes);
    void add(const std::wstring& path, uint64_t bytes);
    AccessMode getMode(const std::wstring& path) const;
    void setMode(const std::wstring& path, AccessMode mode);
    void removeMode(const std::wstring& path);
    void clear();
    void clearUsage();
    const std::unordered_map<std::wstring, uint64_t>& allUsage() const { return values_; }
    const std::unordered_map<std::wstring, AccessMode>& allModes() const { return modes_; }
    void replaceModes(const std::unordered_map<std::wstring, AccessMode>& m) { modes_ = m; save(); }
    bool dirty() const { return dirty_; }
    void clearDirty() { dirty_ = false; }
    void markDirty() { dirty_ = true; }
private:
    std::wstring dirPath() const;
    std::wstring usagePath() const;
    std::wstring policyPath() const;
    std::unordered_map<std::wstring, uint64_t> values_;
    std::unordered_map<std::wstring, AccessMode> modes_;
    mutable bool dirty_{false};
};
