# First-boot catalog clock handling

The COM39 clean-install walkthrough exposed a required manual step: Check
Catalog failed immediately with "Wait for network time or set the clock".
Fresh NVS intentionally leaves `time_service` unestablished; NTP was started
only by clock controls or the FT8 auto-sync path. Wi-Fi connection alone did
not establish clock trust. This prevented first-time users from obtaining maps
inside setup.

## Scoped design and execution plan

The user requires setup to handle this automatically, without a manual clock
screen. Reuse the existing SNTP/RTC service. The catalog worker requests time
through an atomic one-shot; the UI loop starts/polls SNTP and owns RTC access.
Check Catalog shows "Getting network time automatically..." and waits up to
25 seconds, accepting cancellation or network loss. On success it continues the
same catalog operation; on failure Retry requests time again. A valid clock
skips this wait. Signed catalog validation, freshness policy, HTTPS certificate
validation and the installed-map fallback are unchanged. No fabricated time or
trust bypass is introduced. This applies equally to UI and serial checks.

Plan: add a focused deterministic clock regression, implement the worker/UI
handoff, compile both P4 units, build/inspect one native candidate, update only
COM39's app preserving its fresh setup state, then validate real catalog access
without setting its clock manually. Preserve COM39 C6 1.4.1 and SD contents.

## Source checks

- `python tests/map_catalog_clock_test.py`: initial red missing-helper result in
  `map-catalog-clock-red-20261025.log`, then PASS in
  `map-catalog-clock-green-20261025.log`. Extracts the production wait routine;
  exercises already-valid time, delayed success through a UI request, bounded
  timeout, cancellation, Wi-Fi loss and a successful retry.
- `python .local/maps/check_catalog_clock_syntax_20261025.py map_downloads.cpp main.cpp`:
  both exit 0 using the real P4 compiler, log
  `map-catalog-clock-p4-syntax-20261025.log`.
- `git diff --check`: PASS.

Logs are in the invocation directory
`F:\AI\OrcSDR-TEMP\maps-integration-completion`, outside Git. Real automatic time
sync, catalog access and physical progress remain unverified here.
The bounded wait occupies the existing map worker; UI cancellation/status stay
responsive. Background transfer/render worker separation remains another gate.

## Native candidate and hardware scheduling

Source `018b54ef` native build and artifact inspection passed. The first build
was rejected before compilation because the diagnostic build ID exceeded the
39-character limit; shortening only that build input corrected it. Commands:

- `python .local/maps/build_catalog_clock_20261025.py build build2` — exit 0,
  `catalog-clock-native-build2-20261025.log`.
- `python .local/maps/inspect_catalog_clock_20261025.py` — PASS,
  `catalog-clock-native-inspection-20261025.log`. Frozen artifact/manifest:
  `.local/maps/catalog-clock-018b54ef-20261025/`.
- App size 5,301,728 bytes, SHA-256
  `3d4d7502b131279f421191c284027005d1ff98c4e771c29bfc834714a8f210d5`.

During implementation, the user set COM39's clock and began downloading the map.
No app update, reset, erase, cancellation or serial control was performed after
that report. Its active download must finish or be deliberately paused before
installing this candidate. That download is not evidence of automatic first-boot
time acquisition, because the user set the clock manually. A controlled unset-
clock test is still required; do not erase its setup state merely to obtain it.
The candidate is not installed. COM17 remains untouched during this clock fix.
