# Architecture

```text
NetControl.exe
  |
  +-- Win32/GDI UI (Ui.cpp, single main window, 7 pages)
  |     +-- sidebar nav, search, sort, list + detail panel
  |     +-- tray icon (TrayIcon), notifications (Notifier)
  |     +-- themes (Theme), localization (L10n: en/de/es)
  |
  +-- ProcessManager
  |     +-- Toolhelp32Snapshot
  |     +-- QueryFullProcessImageNameW
  |     +-- Authenticode verification
  |
  +-- FirewallManager
  |     +-- INetFwPolicy2 / INetFwRule
  |     +-- outbound application-specific rules
  |     +-- listRules / setRuleEnabled / removeRuleByName / import / export
  |
  +-- NetworkMonitor
  |     +-- GetExtendedTcpTable (PID ownership, IPv4 + IPv6)
  |     +-- Set/GetPerTcpConnectionEStats (per-connection byte counters)
  |     +-- UDP socket counting (per-PID endpoint counts)
  |     +-- sparkline history for Live Monitor
  |
  +-- ScheduleManager      -- hourly block/allow windows, %LOCALAPPDATA%\NetControl\schedules.tsv
  +-- BandwidthManager     -- per-app Mbps limits, pending WFP enforcement
  +-- NetworkProfileMonitor-- NLM: connection profile, cost, gate conditions
  +-- ConnectionLog        -- rolling log, CSV import/export
  +-- UsageHistory         -- daily totals, 30-day window
  +-- UsageStore
        +-- %LOCALAPPDATA%\NetControl\usage.tsv
        +-- %LOCALAPPDATA%\NetControl\policies.tsv
```

The design deliberately avoids a custom kernel driver in the initial build. The first implementation uses documented Windows firewall/IP Helper/NLM interfaces. Two limitations are surfaced in the UI rather than hidden:

1. Ordinary firewall rules can block/allow an executable but cannot provide a genuine per-connection interactive prompt. **Ask** is therefore a monitoring state in this build.
2. Bandwidth limits are stored and displayed but not enforced until the WFP callout module described in `WFP-CALLOUT.md` exists.
