# Real functionality test plan

Run these on a Windows 10/11 x64 test machine or VM. Do not call the project production-ready until the cases below pass.

## Automated
- `ctest --test-dir build -C Release --output-on-failure` passes (core logic suite).

## Process discovery
- Start NetControl and verify real running processes appear.
- Start/close Chrome, Python, Node, etc. and verify the list changes after refresh.
- Verify executable paths are the real paths returned by Windows.

## Firewall enforcement
- Select a harmless test application and click Block.
- Verify a `NetControl Block - ...` outbound rule appears in Windows Defender Firewall.
- Verify outbound access is actually denied.
- Click Allow.
- Verify the rule behavior reverts and outbound access is restored.
- Reboot and verify a persisted Block policy is recreated.
- Verify NetControl never removes unrelated firewall rules.

## Rules page
- Enable/disable a rule with the row toggle; verify state changes in `wf.msc`.
- Delete a rule with the row X; verify it disappears from `wf.msc` and the app mode resets to Ask.
- Export/import rules CSV round-trip.
- Cleanup removes only stale NetControl rules.

## Scheduled rules
- Add a schedule for an app (e.g. `23-7`); toggle it on/off; edit hours via the value buttons; delete it.
- Set a window covering the current hour and verify the app mode flips to the scheduled mode on the next refresh tick.
- Verify `schedules.tsv` persists and reloads.

## Bandwidth
- Set a limit from the detail-panel "Set limit" button or the Rules page; verify it is shown in the UI and persisted.
- Clear the limit and verify it is removed.
- Verify the status line honestly reports that enforcement is pending the WFP module.

## Network observation
- Generate TCP traffic with a browser download; verify observed byte counters and live Mbps change.
- Verify IPv6 TCP connections are attributed (same EStats path).
- Verify UDP endpoint counts appear in the detail panel for a UDP-heavy app (e.g. DNS resolver).
- Verify values persist after restart in `%LOCALAPPDATA%\NetControl\usage.tsv`.
- Network Usage page: verify 30-day bars and per-app bars; mouse-wheel over the per-app card scrolls.

## Logs
- Verify connections appear in Connection Logs, clear works, CSV export/import round-trips.

## UI
- Tray: minimize/close hides to tray; tray click restores; tray menu works.
- Keyboard: arrows, Enter, Ctrl+C, Ctrl+A, Esc, F1.
- Theme and language switches apply immediately.
- All seven pages render without overlap at 100%–200% DPI.

## Limitations to validate
- UDP has no per-process byte API in user mode; UDP is counted as endpoints, not bytes.
- Ask mode is monitoring-only, not a true connection-interception prompt.
- Bandwidth limits are not enforced without the WFP callout module.

These limitations are intentional rather than simulated away.
