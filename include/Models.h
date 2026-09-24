#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

enum class AccessMode { Allow, Block, Ask };

enum class NetworkProfile { Unknown, Private, Public, Domain, Metered };

struct AppInfo {
    DWORD pid{};
    std::wstring name;
    std::wstring path;
    std::wstring publisher;
    AccessMode mode{AccessMode::Ask};
    uint64_t bytesIn{};
    uint64_t bytesOut{};
    uint64_t historicalBytes{};
    uint64_t udpPackets{};
    double currentMbps{};
    bool running{};
    HICON icon{};
    bool iconOwned{};
    bool selected{};
};

enum class Proto { TCP4, TCP6, UDP4, UDP6 };

struct ConnectionInfo {
    DWORD pid{};
    DWORD state{};
    std::wstring remoteAddress;
    uint16_t remotePort{};
    uint16_t localPort{};
    Proto proto{Proto::TCP4};
    std::wstring localAddress;
};

struct LogEntry {
    int64_t timestamp{};
    DWORD pid{};
    std::wstring app;
    std::wstring remote;
    uint16_t port{};
    std::wstring action;
    Proto proto{Proto::TCP4};
};

struct SparkPoint {
    double mbps{};
    int64_t timestamp{};
};

struct ScheduleRule {
    std::wstring path;
    AccessMode mode{AccessMode::Block};
    int startHour{0};
    int endHour{24};
    bool enabled{true};
};
