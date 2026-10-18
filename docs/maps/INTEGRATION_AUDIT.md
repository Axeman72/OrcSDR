# Maps integration audit


## Latest checkpoint: enlarged selector tested on COM39

Normal firmware `6eb22466` builds, flashes and boots on COM39. The 1248x568 selector
now runs on the device; zoom/pan, new setup Skip/Confirm controls, invalid edit and
saved-location retention pass the bounded command/capture checks. Radio reports
zero driver drops/overruns and one live CRC-valid encrypted message during the run.
Overview redraws take 3.0-3.4 s with LoRa active; a zoomed view takes 6.0 s.

The user confirms both coordinate fields, Cancel, zoom and Use Location are visible
on the physical screen. Some saved captures still omit controls. Integration remains
incomplete: one held-cache SD read passes but the earlier intermittent failure is
not closed. Final DMA free/largest is 1327/1088 bytes, with very tight historical
minima. Regional-pack/verified-position dashboard and remaining recovery/release
acceptance gates remain. COM17/C6 and existing SD packs were untouched.

Source, exact commands, actual results, private-data boundaries, logs and next milestone:
[enlarged selector device result](../evidence/maps/2026-10-17-large-selector-device-result.md).
Earlier sections are chronological evidence, not the latest completion claim.


Status: Phase 0/1 discovery; implementation is not complete. Date: 2026-10-17.

## Reproducible baseline

- OrcSDR base: `fba861e36970357e0a005c03d666250b09096cb6`.
- OrcMaps base: `14cdd418fe9101d56ea904eaa8c4da695a7fed9d`; v0.2.1 release `882093ee6ec133cc66afcfce7de4884e91f5d15f`.
- Isolated branches: `codex/maps-integration-completion` in both repositories.
- OrcSDR worktree: `F:/AI/OrcSDR-TEMP/maps-integration-completion`.
- OrcMaps worktree: `F:/AI/OrcSDR-TEMP/orcmaps-integration-completion`.
- Primary checkouts have unrelated untracked files, preserved. No repository AGENTS.md found in either base tree; session maintainer instructions apply.
- ESP-IDF build script pins 5.5.4; driver pin remains v0.9.3 `31a159df4f006a837d5041029bc6d1bc63520234`; Hosted 3.0.6. No baseline/dependency bump authorized.
- Existing factory partition is 6 MiB; NVS 0x9000/0x6000, PHY 0xf000, application 0x10000, coredump follows application. Preserve these offsets. Release size guard reads the partition table (`tools/release/build-m5burner.ps1:59`).

## History verified rather than assumed

- PR #23 merged as `efa9925a`: shared ORCMAP1 maps and catalog/location plumbing.
- PR #26 merged as `fc30c103`: dashboard/ownership work.
- PR #84 is CLOSED, not merged: its embedded world-coastline asset is absent from current main.
- PR #173 Weather foundation is CLOSED, not merged: no Weather dashboard exists in this base tree.
- Historical `claude/orcmaps-integration`: main ahead 340, branch ahead 15. It includes unrelated CB and older driver/partition changes. Do not merge it wholesale.
- Separate `origin/codex/orcmaps-reconciliation-2026-10-17` already offers pagination and region-neutral labels from this same base. Individual diff reviewed; no commits imported wholesale.
- OrcMaps PR #4 polygon rings remains open at `70fea367909c74794bb69c7aed97e0f11f0a9387`; historical CI success is not current hardware acceptance.

## Device discovery, read only

USB instance identities differ: COM17 ends `8&33159DA7&0&0000`; COM39 ends `9&1D2B9888&0&0000` (VID 303A/PID 1001). These identify USB paths, not a physical-label acceptance by the user.

COM17: uptime 143725176 ms, Airband receiving; catalog reports FAA/aviation/legacy map installed. Legacy map version 2026-08-21. COM39: uptime 14898322 ms, LoRa receiving; all five built-in catalog entries uninstalled. Both catalog states ready=0 (unverified/offline). This confirms a listing is distinct from installed state; it does not hash/validate the packs.

No authentication, stop, reset, flash, download, format, migration or SD write was issued. UI_DOC_LIST requires authentication and produced no version in this unauthenticated pass. Firmware SHA/version and NVS schema therefore remain UNTESTED live. Full SD listing is blocked during reception by sd_transfer_radio_busy; no radio pause was authorized in this read-only stage. IP-estimate status is not canonical receiver location.

Reviewed command outputs: `docs/evidence/maps/2026-10-17-com17-readonly.log` and `...com39-readonly.log`. Original logs remain in the command working directory.

## Audit trace

The bounded text trace is `.local/maps/source-trace.txt`, generated with rg across firmware/docs/tools/tests/workflows, excluding image/archive binaries. Key source anchors are in the consumer and migration documents. Large fixtures were not read into the audit.

## Baseline checks

- OrcMaps: CMake Debug host target built; ctest reported 1/1 target passed (contains the host cases).
- OrcSDR: `wsl -d Ubuntu -- python3 -m unittest discover -s .../tests -p test_data_catalog.py`: 3 tests passed. Initial native Windows attempt failed because openssl was absent from PATH, not a firmware failure.
- Fresh ESP-IDF native build passed (4,617,600-byte application); size/headroom and device acceptance limits are recorded in P4_PERFORMANCE_EVIDENCE.md.

## Published catalog check

The 3,036-byte published `catalog-v1.json` and 97-byte base64 signature were read on the host only. `openssl dgst -sha256 -verify .../catalog_public_key.pem -signature signature.der catalog-v1.json` reported `Verified OK`. It contains FAA aircraft, FAA aviation (legacy alias faa_aviation), and the legacy lane_county_map offer; no PMTiles offer is present. No pack payload was downloaded or installed.

## Implementation work identified by the baseline audit

Full local-map inventory, canonical picker, safe wizard migration/rerun, shared OrcMaps rendering, consumer migrations, deterministic render tests and P4 measurements remain open. Every main.cpp change must be map/setup wiring only. No merges are authorized for this task.

## Current integration work

Local OrcSDR branch `codex/maps-integration-completion` remains based on fresh
`origin/main` fba861e36970357e0a005c03d666250b09096cb6. No OrcSDR PR or main merge has
been performed. Focused commits:

- bf1d82b8: catalog capacity/pagination and neutral legacy label.
- 4af52409: tested portable pipeline and local inventory foundations.
- 3d1bc222: pinned engine worker and first LoRa consumer migration.
- aeb6f4f7: setup/picker/local inventory, world provisioning gate, narrow main glue.
- cb79c1fc: ADS-B basemap and marker/hit-test projection.
- 511c2005: central whole-world compatibility renderer for FT8/JS8.

Source/build coverage is not hardware acceptance. Setup migration, manual E7 validation,
0/1/5/16 local pack cases, malformed/missing/mismatched manifests, bounds/antimeridian,
repeatable world renders and injected allocation-failure recovery have host coverage.
Current driver and ESP-IDF pins are unchanged. Main changes are map/setup initialization,
callbacks, Settings actions, the new screen's dispatch, and read-only map diagnostics.
No receiver lifecycle, DSP, USB, audio or Wi-Fi implementation was reorganized.

A low-memory blocker was identified before flashing: the baseline disables C++
exceptions, so library allocation failure aborts. The prepared recovery patch enables
the runtime with a 1 KiB emergency reserve; the user approved its full native rebuild.
This changes C++ compilation/runtime support across the firmware and must be checked
for size and real-device memory/radio behavior, not treated as a free map optimization.
The upstream cleanup fix is draft OrcMaps PR5, based on PR4; neither is merged by this
session. OrcSDR's final pin is c3cc09de3882149e03c7c00d7897c6882b324c6b for that rebuild.

## Unclosed gates

- Exception-enabled native build, pin agreement, size guard and symbol gate PASS; see P4_PERFORMANCE_EVIDENCE.md. Real-device memory/radio acceptance remains open.
- Correct-target confirmation and local flash/NVS backup before any COM39 flash.
- COM39 physical picker/setup/inventory/dashboard navigation and active-RF profiling.
- COM17 preserved-data validation; no flash or SD modification is authorized there.
- Live firmware/NVS identity, full SD inventory, hash verification and stored-location
  evidence not obtained by the earlier unauthenticated, receiving-only audit.
- No-SD/offline, interrupted setup and low-memory behavior on actual hardware.

No active-RF performance gain, radio continuity result, successful device migration,
physical visual acceptance or completed integration is claimed yet.

## Pre-flash focused review outcome

A focused review of the new worker, setup/picker, adapter, and main glue found one
important issue: separately committed latitude/longitude could leave a mixed pair.
The new versioned location_store replaces all three writers and the boot load seam,
keeping legacy keys intact and refusing unknown record versions. Its interruption
regression failed before the fix and passes afterward. The review recheck found no
remaining actual defect in this bounded scope. A full repository audit or physical
acceptance was not performed. Final hardware gates remain open.

## Latest hardware gate — 2026-10-17

COM39 candidates `4971ee5b` and `e87c6d3c` flashed with verified hashes and preserved
NVS, but both failed before OrcSDR initialization while reserving the 40 KiB internal/DMA pool. The
original firmware/table/core dump were restored with verified hashes; LoRa reception
and all six UI self-checks pass on the restored firmware. COM17 and C6 were untouched.
Map hardware acceptance remains blocked. The 592-byte placement change was
insufficient. The approved `fa520a34` diagnostic built, linked and captured 95508
free DMA bytes, but a largest reported block exactly equal to the 40960-byte request.
SDK inspection and a 32-bit allocator reproduction identify extra alignment space
as the missing margin. A four-payload map-state PSRAM placement change is prepared;
its P4 object check passes, but a full build and device tests await approval. See
`P4_PERFORMANCE_EVIDENCE.md` for commands, hashes, recovery paths and exact results.
