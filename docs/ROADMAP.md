# Roadmap

## Phase 1 — current build (done)
- Native Win32/GDI UI with seven pages, tray, keyboard/multi-select, light/dark themes, en/de/es localization
- Dynamic process discovery with Authenticode publisher verification
- Real firewall block/allow rules plus per-rule enable/disable and delete
- Persistent local policy (`usage.tsv`, `policies.tsv`)
- IPv4 + IPv6 TCP per-process observed bytes via EStats, UDP socket counting
- Per-application usage history with 30-day daily bars
- Connection log with CSV import/export
- Scheduled block/allow rules by hour window (add/edit/toggle/delete in UI)
- Bandwidth limit registry with in-app controls (enforcement pending WFP)
- Network profile monitoring (NLM) and optional public-network-only gating
- Unit test target (`NetControlTests`) covering core logic

## Phase 2 — deeper network attribution
- Better connection/domain presentation
- More accurate upload/download attribution
- Service/process grouping
- Real-time UDP byte accounting (requires WFP or per-socket counters)

## Phase 3 — enforcement
- Implement the WFP callout design in `docs/WFP-CALLOUT.md` so that bandwidth limits become enforceable and Ask mode can classify connections
- True interactive Ask mode: signed WFP callout driver + service that classifies outbound connections and pauses/allow/blocks according to user policy. This is a materially larger Windows networking project and should not be approximated with a fake UI.
