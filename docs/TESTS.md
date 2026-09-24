# Unit / integration tests (Item 28)

NetControl uses a lightweight assert-based test executable (`NetControlTests`) built with CMake/CTest.

## Run

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Covered

| Area | Tests |
|------|-------|
| UsageStore | round-trip usage/policies, clear, dirty flag |
| SettingsStore | defaults, save/load |
| L10n | en/de/es lookups |
| ScheduleManager | activeNow hour windows |
| Theme | light/dark non-null colors |
| BandwidthManager | set/get/clear, status text |
| ConnectionLog | append, cap, proto names |
| Ellipsize / fmt | shared helpers via public headers where possible |

Network/firewall integration tests require elevation and are manual — see `docs/TEST_PLAN.md`.
