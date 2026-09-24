# NetControl

Developed by **JOJIN JOHN**

A local-first Windows desktop utility that presents a simple application-level Internet permission UI while using real Windows APIs underneath.

## Important implementation scope

This build is intentionally honest about what Windows user-mode APIs can enforce:

- **Allow**: removes NetControl's outbound block rule for the executable.
- **Block**: creates a real Windows Firewall outbound block rule for that exact executable path.
- **Ask**: removes NetControl's block rule and monitors the process. It does **not** pretend to intercept every connection and show a prompt. A true per-connection interactive prompt requires a deeper Windows Filtering Platform callout/driver design (see `docs/WFP-CALLOUT.md`).
- **Process list**: enumerated from the current Windows process table; no hard-coded app list.
- **Publisher**: only reports a verified Authenticode signature as verified. It does not infer trust from a filename.
- **Live network data**: Windows TCP extended statistics (IPv4 and IPv6) for per-PID connections, plus UDP socket counting. Labeled as observed data, not fake total-device data.
- **History**: locally persisted under `%LOCALAPPDATA%\\NetControl` and accumulates observed bytes while NetControl is running.
- **Bandwidth limits**: per-app limits are stored and shown in the UI; enforcement is pending the WFP callout module. The UI reports this status instead of pretending limits are active.
- **No cloud, login, or server is required for the core functions.**

## Features

Seven pages, all functional:

- **Overview** — protection cards, searchable/sortable app list, detail panel with Allow/Block/Ask, Open file, Connections, and bandwidth limit controls.
- **Applications** — the app list without overview cards, same controls.
- **Live Monitor** — system throughput sparkline, total observed bytes, top applications by live speed.
- **Network Usage** — 30-day daily usage bars, per-application usage bars with mouse-wheel scrolling.
- **Connection Logs** — rolling connection log with CSV import/export and clear history.
- **Rules** — NetControl firewall rules with per-row enable/disable toggle and delete; scheduled access rules with add/edit/toggle/delete; bandwidth shaping controls (set/clear per-app limit).
- **Settings** — theme, auto-start, notifications, auto-refresh, network-profile gating, block-only-public, language (en/de/es), data clearing, About.

Additional behavior:

- System tray icon with menu, tooltips, and balloons; minimize/close hides to tray.
- Keyboard: arrow-key navigation, Enter for connections, Ctrl+C copy path, Ctrl+A select all, F1 About, Esc clear selection.
- Right-click context menu on app rows; multi-select with Ctrl+click.
- Scheduled block/apply runs on the refresh timer (independent of the network-profile gate setting).
- Rule, log, and schedule changes persist immediately.

## Build requirements

- Windows 10/11 x64
- Visual Studio with **Desktop development with C++** (or Build Tools) and a Windows 10/11 SDK
- CMake 3.24+
- Administrator rights when running the application (required for firewall administration in this implementation)

## Build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Executables:

```text
build\Release\NetControl.exe
build\Release\NetControlTests.exe
```

Run tests:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

## Test the real blocking function

1. Start NetControl as administrator.
2. Select a harmless test executable.
3. Click **Block**.
4. Verify the application loses outbound connectivity.
5. Click **Allow**.
6. Verify connectivity returns.
7. Open `wf.msc` or Windows Defender Firewall with Advanced Security and inspect the `NetControl Block - ...` outbound rule.

Do not test by blocking critical Windows components unless you understand the consequences.

## Why there is no fake "Ask" implementation

Windows Firewall's ordinary rule API does not provide an application callback that pauses an arbitrary outbound connection and asks the user what to do. NetControl therefore refuses to fake this behavior. A future WFP callout/driver module can implement true interactive interception if that feature is required (see `docs/WFP-CALLOUT.md`).
