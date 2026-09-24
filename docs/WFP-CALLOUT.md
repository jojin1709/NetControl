# WFP Callout / Driver Design (Item 29)

True per-connection **Ask** prompts and **bandwidth shaping** (Item 30) require a Windows Filtering Platform callout driver.

## Why user-mode cannot fully do this

- Windows Firewall rule APIs (INetFwPolicy2) only support allow/block — no interactive callback.
- Per-packet rate limiting is not exposed to ordinary applications.

## Architecture (future module)

```
NetControl.exe  (user-mode UI)
    |
    | DeviceIoControl / shared section
    v
netcontrolwfp.sys  (kernel callout)
    |
    +-- FWPM_SUBLAYER_NETCONTROL
    +-- ClassifyFn: BLOCK | PERMIT | pend for UI
    +-- Byte counting for bandwidth shaping
    +-- Named event: user prompts
```

## Implementation checklist

1. Install WDK; create a KMDF/WDM driver project.
2. Register callout at `FWPM_LAYER_ALE_AUTH_CONNECT_V4/V6`.
3. On classify: if app has Ask mode → `FWP_ACTION_PEND` + signal user-mode.
4. User-mode shows prompt; completes with permit/block.
5. For shaping: inspect `FWP_CALLOUT_IO_PACKET` / use `ALE_RESOURCE_ASSIGN` + token bucket in classify.
6. Sign with attestation cert; ship via INF.

## User-mode fallback (current)

- **Ask** = monitor-only (no fake prompt) — honest behavior documented in README.
- **Bandwidth limits** are stored in `bandwidth.tsv` and reported as *pending* until WFP loads.

Set environment variable `NETCONTROL_WFP=1` when the signed driver service is present to flip `BandwidthManager::enforcementAvailable()`.
