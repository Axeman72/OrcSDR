# Release review and IQ continuity completion ledger

PR: #211, `codex/maps-integration-completion`. Starting local commit: `3e213113`.
User authorizes autonomous fixes, tests, builds, app-only hardware updates and
merge **after** the goal passes. Preserve both tablets' SD/NVS/world data and C6
firmware (COM17 3.0.6, COM39 1.4.1); leave unrelated worktrees alone.

## Required completion gates

- Resolve the still-valid findings in `2026-10-25-pr211-review-triage.md`,
  including the newer setup/documentation early-return finding.
- Restore CodeRabbit coverage for tests and shipping documentation; verify
  findings against current code rather than treating a green status as approval.
- Build and test the final reviewed source on hardware. The installed `9e3f0378`
  does not include the later `271dca27` LoRa/FT8/storage review fixes.
- Resolve map-related IQ loss. Measure driver and application counters by phase
  under active reception. Cached drag acceptance does not waive dropped IQ.
- Keep release-matrix FAIL/BLOCKED rows visible; no unsupported relabeling.
- Merge only after these gates pass; verify the merge on origin/main. Validate
  the tagged package before publication. Intended tag: `v0.5.0-beta.1`.

## Task ledger

| Task | State / evidence |
| --- | --- |
| Setup pause ownership on external navigation and documentation early return | Source correction; `python tests/setup_navigation_pause_test.py` passed after reproducing missing cleanup; device validation pending |
| Location lookup WithCaps task lifetime | Source correction; `python tests/location_task_lifetime_test.py` passed after reproducing missing reaper; P4/device checks pending |
| OrcDial initialization allocation-failure backoff | Focused production-block test passed for both queues and TX task; P4 syntax passed; device pending |
| ADS-B bounded zoom redraw and range-limit no-op | Focused production-handler test passed; map panel only, no-op at both limits; P4 syntax passed |
| FT8 row transfer and pending-frame redraw | Sanitized real-dashboard test passed with every output pixel compared against source projection; locked pending frames do not repaint static panels; P4 syntax passed |
| Shared download notice/header geometry | FT8 title and FT8/LoRa dividers moved beyond the shared notice; compile-time geometry guards; notice test passed after correcting stale visible-string assertions |
| Download/render scheduling and shared SD ownership | Pending |
| Current SD capacity and LoRa label behavior device checks | Pending |
| Restore review scope; review tests and release notes | Temporary source-only filters removed; external full review pending |
| Zero additional map-related IQ drops under recorded workloads | Pending; earlier cache chunking experiment failed this gate |
| Native build and COM17/COM39 final source validation | Pending |
| Final review, CI, merge/main verification and package validation | Pending |

Ruling: retain the user's quick map-file check and background-download Back
behavior. Do not reinstate full archive hashing or turn Back into cancellation
to satisfy superseded reviewer assumptions.

Ruling: use an externally reaped, suspended location lookup task. ESP-IDF 5.5.4
documents that plain `vTaskDelete` leaks WithCaps allocations, while self-delete
with `vTaskDeleteWithCaps` allocates another task and can abort on allocation
failure. The UI task can reclaim completed work without that allocation.

Raw logs/artifacts remain outside Git beside their invocation. Every subsequent
hardware result must identify the actual app hash, receiver and configuration.

## 2026-10-26 focused verification

- `bash tools/test-tab5-cadence-history.sh`: seven groups passed, including four
  new lifecycle/zoom tests; log `pr211-lifecycle-regressions-20261026b.log`.
- Sanitized production FT8 dashboard test: passed; log
  `pr211-ft8-followups-sanitized-20261026b.log`.
- `ctest --test-dir .local/maps/feature-replay-integration-20261023 -R
  "^map_download_notice$" --output-on-failure`: 1/1 passed; log
  `pr211-notice-regression-20261026.log`.
- Exact P4 compile-command syntax checks: main, map screen, location lookup,
  FT8, ADS-B, LoRa and download notice passed; log
  `pr211-release-followups-p4-syntax-final-20261026.log`.
- COM17 read-only baseline: USB identity verified, V3c driver 0.9.3 streaming
  2.4 MSPS, lifetime driver overruns/drops zero; baseline is not a map-load
  continuity result. Log `pr211-release-baseline-com17-20261026.log`.

The notice regression initially failed because the newer display stub removes
strings when their rectangle is cleared. Assertions incorrectly expected all
old terminal-state text to remain visible. The test now checks draw calls and
that the current state replaces earlier text, rather than weakening redraw
requirements. The FT8 row buffers add 2,672 bytes of external BSS; native ELF
placement and actual device behavior remain to be verified.
