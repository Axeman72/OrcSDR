# FT8 map memory recovery

COM17 lost the FT8 basemap after the V3c LoRa check. Read-only serial evidence in
`ft8-missing-map-readonly-device-20261025.log` reported 602,692 free PSRAM bytes,
largest block 524,288 bytes, and 15,840,000 bytes retained by LoRa IQ buffers.
FT8 was decoding. The user restarted and confirmed the map returned. This
supports memory pressure as the cause, without proving every possible failure.
An FT8 SD log-write error was also captured and remains a separate investigation.

The FT8 map now calls the existing reclaim registry before allocating missing
sprites. Holder callbacks retain ownership of their safety checks: live captures,
exports and active decoding must not be forcibly freed. Allocation or map-worker
out-of-memory results schedule a retry every two seconds even if no worker
revision changes. A completed composed map remains visible during recovery.
Healthy ordinary updates do not reclaim again. Exit resets retry state.

No new framebuffer or DMA reserve is added. Existing FT8 sprites remain
824×420×2 + 512×512×2 = 1,216,448 bytes of PSRAM, excluding allocator overhead
and the shared worker's separate canvas/scratch. Only bounded retry state is new.
UI and existing serial FT8 navigation use the same dashboard path.

## Checks executed

Run directory: `F:\AI\OrcSDR-TEMP\maps-integration-completion`. Raw logs stay there
outside Git.

- Focused dashboard regression compiled with WSL g++17 and the recording display.
  The initial command omitted the existing clock-status dependency and failed
  linking; the corrected command reproduced the missing automatic retry in
  `ft8-map-recovery-red2-20261025.log`.
- Optimized focused dashboard regression: PASS,
  `ft8-map-recovery-green-20261025.log`.
- Same dashboard regression with ASan/UBSan, including busy-holder retry,
  recovery without a revision, worker out-of-memory recovery, stable healthy
  updates and unchanged full-screen-clear count: PASS,
  `ft8-map-recovery-sanitized-20261025.log`.
- `python .local/maps/check_ft8_map_recovery_20261025.py`: real P4 compiler PASS;
  FT8 update stack 96 bytes. Compiler log
  `ft8-map-recovery-p4-20261025.log`.
- `git diff --check`: PASS.

Native build of source `9344fce7` and artifact inspection subsequently passed:

- `python .local/maps/build_release_ft8_map_20261025.py build` — exit 0,
  `release-ft8-map-native-build-20261025.log`.
- `python .local/maps/inspect_release_ft8_map_20261025.py` — PASS,
  `release-ft8-map-native-inspection-20261025.log`. Artifact and hash manifest:
  `.local/maps/release-ft8-map-9344fce7-20261025/manifest.json`.
- Existing 40 KiB internal reserve, production catalog, PSRAM placement, pinned
  engine and symbol checks passed inspection. This candidate also includes the
  previously prepared SD-capacity, LoRa label-redraw and start/pause fixes.

COM17 LoRa→FT8 transition/recovery remains unverified. No reset, tuning change,
reclaim command or flash was issued
during this correction. COM39 remains untouched. This is not a claim that maps
can allocate through arbitrary exhausted memory or that live buffers may be
discarded; the required hardware gate is the reported ordinary mode transition.

## First hardware candidate

App-only candidate `9344fce7` was installed on COM17 with all non-app partitions
verified unchanged and startup captured. Three LoRa→FT8 returns rendered the
world map, released retained LoRa buffers to zero, and left reception running.
The first released 15,840,000 bytes; free PSRAM afterward was 14,213,036 bytes.
Final shared frame: ready, 524,288 bytes, one tile, 478 features, zero missing or
failed tiles. The hash-verified final PNG visibly contains the world basemap,
receiver marker, attribution and controls. Logs:

- `ft8-map-recovery-transitions-device-20261025.log`
- `ft8-map-recovery-final-20261025.png`
- `ft8-map-recovery-snapshot-20261025.log`
- `release-ft8-map-boot-native-monitor-20261025.log`
- `release-ft8-map-preservation-20261025.log`

However, the second/third LoRa entries reported `RTL_IQ_ERROR no_psram_buffer`
and retained only 8,160,000 bytes, so these are not three successful full LoRa
reception cycles. Inspection found generic dashboard navigation queued LoRa's
capture allocations before `draw_sdr_screen` called FT8's sprite cleanup.
The follow-up moves that existing FT8 cleanup ahead of the LoRa queue call.
This follow-up requires native and round-trip hardware validation before the
overall memory handoff is closed. No protocol decode success is inferred from
these UI/memory checks.
