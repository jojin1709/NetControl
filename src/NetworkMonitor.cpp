#include "NetworkMonitor.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <tcpmib.h>
#include <tcpestats.h>
#include <cstring>
#include <algorithm>
#include <vector>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

static uint64_t tcpBytesForRow(const MIB_TCPROW& row) {
    TCP_ESTATS_DATA_ROD_v0 rod{};
    TCP_ESTATS_DATA_RW_v0 rw{};
    rw.EnableCollection = TRUE;
    SetPerTcpConnectionEStats(const_cast<PMIB_TCPROW>(&row), TcpConnectionEstatsData,
                              reinterpret_cast<PUCHAR>(&rw), 0, sizeof(rw), 0);
    if (GetPerTcpConnectionEStats(const_cast<PMIB_TCPROW>(&row), TcpConnectionEstatsData,
                                  nullptr, 0, 0, nullptr, 0, 0,
                                  reinterpret_cast<PUCHAR>(&rod), 0, sizeof(rod)) != NO_ERROR) return 0;
    return static_cast<uint64_t>(rod.DataBytesIn) + static_cast<uint64_t>(rod.DataBytesOut);
}

static uint64_t tcp6BytesForRow(const MIB_TCP6ROW& row) {
    TCP_ESTATS_DATA_ROD_v0 rod{};
    TCP_ESTATS_DATA_RW_v0 rw{};
    rw.EnableCollection = TRUE;
    SetPerTcp6ConnectionEStats(const_cast<PMIB_TCP6ROW>(&row), TcpConnectionEstatsData,
                               reinterpret_cast<PUCHAR>(&rw), 0, sizeof(rw), 0);
    if (GetPerTcp6ConnectionEStats(const_cast<PMIB_TCP6ROW>(&row), TcpConnectionEstatsData,
                                   nullptr, 0, 0, nullptr, 0, 0,
                                   reinterpret_cast<PUCHAR>(&rod), 0, sizeof(rod)) != NO_ERROR) return 0;
    return static_cast<uint64_t>(rod.DataBytesIn) + static_cast<uint64_t>(rod.DataBytesOut);
}

void NetworkMonitor::sampleTcp(std::unordered_map<DWORD, uint64_t>& current, int family,
                               int tableClass, bool v6) {
    DWORD size = 0;
    GetExtendedTcpTable(nullptr, &size, TRUE, family, static_cast<TCP_TABLE_CLASS>(tableClass), 0);
    if (!size) return;
    std::vector<unsigned char> buf(size);
    if (GetExtendedTcpTable(buf.data(), &size, TRUE, family, static_cast<TCP_TABLE_CLASS>(tableClass), 0) != NO_ERROR) return;
    if (!v6) {
        auto* table = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buf.data());
        for (DWORD i = 0; i < table->dwNumEntries; ++i) {
            auto& r = table->table[i];
            if (r.dwState != MIB_TCP_STATE_ESTAB) continue;
            MIB_TCPROW row{};
            row.dwState = r.dwState;
            row.dwLocalAddr = r.dwLocalAddr;
            row.dwLocalPort = r.dwLocalPort;
            row.dwRemoteAddr = r.dwRemoteAddr;
            row.dwRemotePort = r.dwRemotePort;
            current[r.dwOwningPid] += tcpBytesForRow(row);
        }
    } else {
        auto* table = reinterpret_cast<PMIB_TCP6TABLE_OWNER_PID>(buf.data());
        for (DWORD i = 0; i < table->dwNumEntries; ++i) {
            auto& r = table->table[i];
            if (r.dwState != MIB_TCP_STATE_ESTAB) continue;
            MIB_TCP6ROW row{};
            row.State = static_cast<MIB_TCP_STATE>(r.dwState);
            row.dwLocalScopeId = r.dwLocalScopeId;
            row.dwLocalPort = r.dwLocalPort;
            row.dwRemoteScopeId = r.dwRemoteScopeId;
            row.dwRemotePort = r.dwRemotePort;
            memcpy(row.LocalAddr.u.Byte, r.ucLocalAddr, 16);
            memcpy(row.RemoteAddr.u.Byte, r.ucRemoteAddr, 16);
            current[r.dwOwningPid] += tcp6BytesForRow(row);
        }
    }
}

void NetworkMonitor::sampleUdp(std::unordered_map<DWORD, uint64_t>& packets, int family, int tableClass) {
    DWORD size = 0;
    GetExtendedUdpTable(nullptr, &size, TRUE, family, static_cast<UDP_TABLE_CLASS>(tableClass), 0);
    if (!size) return;
    std::vector<unsigned char> buf(size);
    if (GetExtendedUdpTable(buf.data(), &size, TRUE, family, static_cast<UDP_TABLE_CLASS>(tableClass), 0) != NO_ERROR) return;
    if (family == AF_INET) {
        auto* table = reinterpret_cast<PMIB_UDPTABLE_OWNER_PID>(buf.data());
        for (DWORD i = 0; i < table->dwNumEntries; ++i)
            packets[table->table[i].dwOwningPid] += 1;
    } else {
        auto* table = reinterpret_cast<PMIB_UDP6TABLE_OWNER_PID>(buf.data());
        for (DWORD i = 0; i < table->dwNumEntries; ++i)
            packets[table->table[i].dwOwningPid] += 1;
    }
}

void NetworkMonitor::refresh(std::vector<AppInfo>& apps) {
    const auto now = std::chrono::steady_clock::now();
    double dt = 1.0;
    if (lastRefresh_.time_since_epoch().count() != 0)
        dt = std::max(0.25, std::chrono::duration<double>(now - lastRefresh_).count());
    lastRefresh_ = now;

    std::unordered_map<DWORD, uint64_t> current;
    std::unordered_map<DWORD, uint64_t> udpCounts;
    sampleTcp(current, AF_INET, TCP_TABLE_OWNER_PID_ALL, false);
    sampleTcp(current, AF_INET6, TCP_TABLE_OWNER_PID_ALL, true);
    sampleUdp(udpCounts, AF_INET, UDP_TABLE_OWNER_PID);
    sampleUdp(udpCounts, AF_INET6, UDP_TABLE_OWNER_PID);

    uint64_t deltaTotal = 0;
    for (auto& a : apps) {
        uint64_t cur = current[a.pid];
        auto it = previous_.find(a.pid);
        uint64_t delta = 0;
        if (it != previous_.end() && cur >= it->second.bytes) delta = cur - it->second.bytes;
        else if (it != previous_.end() && cur < it->second.bytes) delta = cur;
        a.bytesIn = cur;
        a.bytesOut = 0;
        a.udpPackets = udpCounts[a.pid];
        a.currentMbps = (delta * 8.0 / dt) / 1'000'000.0;
        if (delta) a.historicalBytes += delta;
        deltaTotal += delta;
    }
    for (auto& kv : current) previous_[kv.first] = Sample{kv.second, now};
    systemMbps_ = (deltaTotal * 8.0 / dt) / 1'000'000.0;
    totalObservedBytes_ += deltaTotal;
    history_.push_back({systemMbps_, std::chrono::duration_cast<std::chrono::seconds>(
                          std::chrono::system_clock::now().time_since_epoch()).count()});
    while (history_.size() > 120) history_.pop_front();
    sampled_ = true;
    if (cb_) cb_(cbCtx_);
}

std::vector<ConnectionInfo> NetworkMonitor::connectionsForPid(DWORD pid) {
    std::vector<ConnectionInfo> out;
    for (auto& c : allConnections()) if (c.pid == pid) out.push_back(c);
    return out;
}

std::vector<ConnectionInfo> NetworkMonitor::allConnections() {
    std::vector<ConnectionInfo> out;
    auto pushTcp = [&](bool v6) {
        const int family = v6 ? AF_INET6 : AF_INET;
        DWORD size = 0;
        GetExtendedTcpTable(nullptr, &size, TRUE, family, TCP_TABLE_OWNER_PID_ALL, 0);
        if (!size) return;
        std::vector<unsigned char> buf(size);
        if (GetExtendedTcpTable(buf.data(), &size, TRUE, family, TCP_TABLE_OWNER_PID_ALL, 0) != NO_ERROR) return;
        char ip[64]{};
        if (!v6) {
            auto* t = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buf.data());
            for (DWORD i = 0; i < t->dwNumEntries; ++i) {
                auto& r = t->table[i];
                IN_ADDR a{}; a.S_un.S_addr = r.dwRemoteAddr;
                InetNtopA(AF_INET, &a, ip, sizeof(ip));
                IN_ADDR la{}; la.S_un.S_addr = r.dwLocalAddr;
                char lip[64]{}; InetNtopA(AF_INET, &la, lip, sizeof(lip));
                ConnectionInfo c;
                c.pid = r.dwOwningPid; c.state = r.dwState;
                c.remoteAddress = std::wstring(ip, ip + strlen(ip));
                c.localAddress = std::wstring(lip, lip + strlen(lip));
                c.remotePort = ntohs(static_cast<u_short>(r.dwRemotePort));
                c.localPort = ntohs(static_cast<u_short>(r.dwLocalPort));
                c.proto = Proto::TCP4;
                out.push_back(std::move(c));
            }
        } else {
            auto* t = reinterpret_cast<PMIB_TCP6TABLE_OWNER_PID>(buf.data());
            for (DWORD i = 0; i < t->dwNumEntries; ++i) {
                auto& r = t->table[i];
                InetNtopA(AF_INET6, r.ucRemoteAddr, ip, sizeof(ip));
                char lip[64]{}; InetNtopA(AF_INET6, r.ucLocalAddr, lip, sizeof(lip));
                ConnectionInfo c;
                c.pid = r.dwOwningPid; c.state = r.dwState;
                c.remoteAddress = std::wstring(ip, ip + strlen(ip));
                c.localAddress = std::wstring(lip, lip + strlen(lip));
                c.remotePort = ntohs(static_cast<u_short>(r.dwRemotePort));
                c.localPort = ntohs(static_cast<u_short>(r.dwLocalPort));
                c.proto = Proto::TCP6;
                out.push_back(std::move(c));
            }
        }
    };
    auto pushUdp = [&](bool v6) {
        const int family = v6 ? AF_INET6 : AF_INET;
        DWORD size = 0;
        GetExtendedUdpTable(nullptr, &size, TRUE, family, UDP_TABLE_OWNER_PID, 0);
        if (!size) return;
        std::vector<unsigned char> buf(size);
        if (GetExtendedUdpTable(buf.data(), &size, TRUE, family, UDP_TABLE_OWNER_PID, 0) != NO_ERROR) return;
        char ip[64]{};
        if (!v6) {
            auto* t = reinterpret_cast<PMIB_UDPTABLE_OWNER_PID>(buf.data());
            for (DWORD i = 0; i < t->dwNumEntries; ++i) {
                auto& r = t->table[i];
                IN_ADDR la{}; la.S_un.S_addr = r.dwLocalAddr;
                InetNtopA(AF_INET, &la, ip, sizeof(ip));
                ConnectionInfo c;
                c.pid = r.dwOwningPid; c.state = 0;
                c.remoteAddress = L"*";
                c.localAddress = std::wstring(ip, ip + strlen(ip));
                c.localPort = ntohs(static_cast<u_short>(r.dwLocalPort));
                c.proto = Proto::UDP4;
                out.push_back(std::move(c));
            }
        } else {
            auto* t = reinterpret_cast<PMIB_UDP6TABLE_OWNER_PID>(buf.data());
            for (DWORD i = 0; i < t->dwNumEntries; ++i) {
                auto& r = t->table[i];
                InetNtopA(AF_INET6, r.ucLocalAddr, ip, sizeof(ip));
                ConnectionInfo c;
                c.pid = r.dwOwningPid; c.state = 0;
                c.remoteAddress = L"*";
                c.localAddress = std::wstring(ip, ip + strlen(ip));
                c.localPort = ntohs(static_cast<u_short>(r.dwLocalPort));
                c.proto = Proto::UDP6;
                out.push_back(std::move(c));
            }
        }
    };
    pushTcp(false);
    pushTcp(true);
    pushUdp(false);
    pushUdp(true);
    return out;
}
