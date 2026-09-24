#pragma once
#include "Models.h"
#include <vector>
#include <unordered_map>
#include <chrono>
#include <deque>
#include <string>

class NetworkMonitor {
public:
    void refresh(std::vector<AppInfo>& apps);
    std::vector<ConnectionInfo> connectionsForPid(DWORD pid);
    std::vector<ConnectionInfo> allConnections();
    double systemMbps() const { return systemMbps_; }
    uint64_t totalObservedBytes() const { return totalObservedBytes_; }
    const std::deque<SparkPoint>& history() const { return history_; }
    bool sampleReady() const { return sampled_; }
    void setOnSample(void (*cb)(void*), void* ctx) { cb_ = cb; cbCtx_ = ctx; }
private:
    struct Sample { uint64_t bytes{}; std::chrono::steady_clock::time_point time; };
    std::unordered_map<DWORD, Sample> previous_;
    std::chrono::steady_clock::time_point lastRefresh_{};
    double systemMbps_{};
    uint64_t totalObservedBytes_{};
    bool sampled_{false};
    std::deque<SparkPoint> history_;
    void (*cb_)(void*){nullptr};
    void* cbCtx_{nullptr};
    void sampleTcp(std::unordered_map<DWORD, uint64_t>& current, int family, int tableClass, bool v6);
    void sampleUdp(std::unordered_map<DWORD, uint64_t>& packets, int family, int tableClass);
};
